// 2D Variable Physarum chemotax trails //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

#define FOCUS_COLS 3
#define FOCUS_ROWS 4
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

#define NUM_SPECIES  2
#define AGENTS_PER_SPECIES 2500
#define TOTAL_AGENTS (NUM_SPECIES * AGENTS_PER_SPECIES)
#define MOVE_SPEED 1.0f
#define DEPOSIT_AMOUNT 50.0f
#define DECAY_RATE 0.965f

struct Focus {
    float x, y;
    float sA, sD, rS;
};

struct Agent {
    float x, y, angle;
    int species;
};

Focus focuses[NUM_FOCUSES];
float *pixelSA, *pixelSD, *pixelRS;
float *trail;
uint16_t *coll;
Agent *agents;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (esp_random() / 4294967295.0f) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

int get_idx(int x, int y) {

    if (x < 0) x = 0; if (x >= WIDTH) x = WIDTH - 1;
    if (y < 0) y = 0; if (y >= HEIGHT) y = HEIGHT - 1;
    return y * WIDTH + x;

}

uint16_t blendColors(uint16_t c1, uint16_t c2, float ratio) {

    uint8_t r = ((c1 >> 11) & 0x1F) + (uint8_t)((((c2 >> 11) & 0x1F) - ((c1 >> 11) & 0x1F)) * ratio);
    uint8_t g = ((c1 >> 5) & 0x3F) + (uint8_t)((((c2 >> 5) & 0x3F) - ((c1 >> 5) & 0x3F)) * ratio);
    uint8_t b = (c1 & 0x1F) + (uint8_t)(((c2 & 0x1F) - (c1 & 0x1F)) * ratio);
    return (r << 11) | (g << 5) | b;

}

void rndseed() {

    float cellW = (float)WIDTH / FOCUS_COLS;
    float cellH = (float)HEIGHT / FOCUS_ROWS;

    for (int r = 0; r < FOCUS_ROWS; r++) {
        for (int c = 0; c < FOCUS_COLS; c++) {
            int i = r * FOCUS_COLS + c;
            focuses[i].x = (c * cellW) + (cellW / 2.0f);
            focuses[i].y = (r * cellH) + (cellH / 2.0f);
            focuses[i].sA = randomf(0.2f, 1.3f);
            focuses[i].sD = randomf(5.0f, 20.0f);
            focuses[i].rS = randomf(0.05f, 0.4f);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = x + y * WIDTH;
            float totalW = 0, sa = 0, sd = 0, rs = 0;
            for (int f = 0; f < NUM_FOCUSES; f++) {
                float d2 = powf(x - focuses[f].x, 2) + powf(y - focuses[f].y, 2);
                float w = 1.0f / (d2 + 40.0f);
                sa += focuses[f].sA * w;
                sd += focuses[f].sD * w;
                rs += focuses[f].rS * w;
                totalW += w;
            }
            pixelSA[idx] = sa / totalW;
            pixelSD[idx] = sd / totalW;
            pixelRS[idx] = rs / totalW;
        }
    }

    memset(trail, 0, NUM_SPECIES * SCR * sizeof(float));
    coll[0] = color565(255, 50, 50);
    coll[1] = color565(50, 255, 50);

    for (int i = 0; i < TOTAL_AGENTS; i++) {
        agents[i].x = randomf(0, WIDTH);
        agents[i].y = randomf(0, HEIGHT);
        agents[i].angle = randomf(0, 2.0f * M_PI);
        agents[i].species = i / AGENTS_PER_SPECIES;
    }

}

void nextstep() {

    for (int i = 0; i < TOTAL_AGENTS; i++) {
        Agent *ag = &agents[i];
        int idx = get_idx((int)ag->x, (int)ag->y);
        
        float sA = pixelSA[idx];
        float sD = pixelSD[idx];
        float rS = pixelRS[idx];

        auto sense = [&](float angleOffset) {
            float angle = ag->angle + angleOffset;
            int sx = (int)(ag->x + cosf(angle) * sD);
            int sy = (int)(ag->y + sinf(angle) * sD);
            int sIdx = get_idx(sx, sy);
            float myTrail = trail[ag->species * SCR + sIdx];
            float enemyTrail = trail[((ag->species + 1) % NUM_SPECIES) * SCR + sIdx];
            return myTrail - enemyTrail * 0.5f;
        };

        float wF = sense(0);
        float wL = sense(sA);
        float wR = sense(-sA);

        float rndSteer = (randomf(-0.5f, 0.5f)) * rS;
        if (wF > wL && wF > wR) ag->angle += rndSteer;
        else if (wL > wR) ag->angle += sA + rndSteer;
        else if (wR > wL) ag->angle -= sA + rndSteer;
        else ag->angle += (randomf(-0.5f, 0.5f)) * 1.0f;

        ag->x += cosf(ag->angle) * MOVE_SPEED;
        ag->y += sinf(ag->angle) * MOVE_SPEED;

        if (ag->x < 0 || ag->x >= WIDTH) { ag->angle = M_PI - ag->angle; ag->x = fmaxf(0, fminf(WIDTH-1, ag->x)); }
        if (ag->y < 0 || ag->y >= HEIGHT) { ag->angle = -ag->angle; ag->y = fmaxf(0, fminf(HEIGHT-1, ag->y)); }

        int tidx = ag->species * SCR + get_idx((int)ag->x, (int)ag->y);
        trail[tidx] += DEPOSIT_AMOUNT;
        if (trail[tidx] > 100.0f) trail[tidx] = 100.0f;
    }

    for (int i = 0; i < NUM_SPECIES * SCR; i++) trail[i] *= DECAY_RATE;

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    trail = (float *)ps_malloc(NUM_SPECIES * SCR * sizeof(float));
    pixelSA = (float *)ps_malloc(SCR * sizeof(float));
    pixelSD = (float *)ps_malloc(SCR * sizeof(float));
    pixelRS = (float *)ps_malloc(SCR * sizeof(float));
    coll = (uint16_t *)ps_malloc(NUM_SPECIES * sizeof(uint16_t));
    agents = (Agent *)ps_malloc(TOTAL_AGENTS * sizeof(Agent));
    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if(M5.BtnA.isPressed()) rndseed();

    nextstep();

    for (int i = 0; i < SCR; i++) {

        float t1 = trail[0 * SCR + i];
        float t2 = trail[1 * SCR + i];
        
        if (t1 > t2) {
            frameBuffer[i] = blendColors(0x0000, coll[0], fminf(1.0f, t1 / 80.0f));
        } else if (t2 > t1) {
            frameBuffer[i] = blendColors(0x0000, coll[1], fminf(1.0f, t2 / 80.0f));
        } else {
            frameBuffer[i] = 0x0000;
        }

    }

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}