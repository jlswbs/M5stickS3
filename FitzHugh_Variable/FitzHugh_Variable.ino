// 2D Variable FitzHugh-Nagumo reaction diffusion //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH*HEIGHT)

#define FOCUS_COLS 3
#define FOCUS_ROWS 4
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

struct Focus {
    float x, y;
    float f, k, diffX, diffY;
};

Focus focuses[NUM_FOCUSES];

float *gridU, *gridV, *gridNext;
float *farr, *karr, *diffRateUXarr, *diffRateUYarr;
uint16_t* frameBuffer;

float reactionRate = 0.2f;
float diffusionRateV = 0.015f;

float randomf(float minf, float maxf) { return minf + (esp_random() / 4294967295.0f) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t grayLUT[256];

void rndseed() {

    for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);

    float cellW = (float)WIDTH / FOCUS_COLS;
    float cellH = (float)HEIGHT / FOCUS_ROWS;

    for (int r = 0; r < FOCUS_ROWS; r++) {
        for (int c = 0; c < FOCUS_COLS; c++) {
            int i = r * FOCUS_COLS + c;
            focuses[i].x = (c * cellW) + (cellW / 2.0f);
            focuses[i].y = (r * cellH) + (cellH / 2.0f);

            focuses[i].f = randomf(0.02f, 0.05f);
            focuses[i].k = randomf(0.1f, 0.3f);
            focuses[i].diffX = randomf(0.01f, 0.06f);
            focuses[i].diffY = randomf(0.01f, 0.06f);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = y * WIDTH + x;
            float totalW = 0, sf = 0, sk = 0, sdx = 0, sdy = 0;

            for (int f = 0; f < NUM_FOCUSES; f++) {
                float d2 = powf(x - focuses[f].x, 2) + powf(y - focuses[f].y, 2);
                float w = 1.0f / (d2 + 25.0f);
                sf += focuses[f].f * w;
                sk += focuses[f].k * w;
                sdx += focuses[f].diffX * w;
                sdy += focuses[f].diffY * w;
                totalW += w;
            }
            farr[idx] = sf / totalW;
            karr[idx] = sk / totalW;
            diffRateUXarr[idx] = sdx / totalW;
            diffRateUYarr[idx] = sdy / totalW;

            gridU[idx] = 1.0f;
            gridV[idx] = randomf(0.0f, 0.5f);
        }
    }

}

void diffusionU() {

    for (int j = 1; j < HEIGHT - 1; ++j) {
        int off = j * WIDTH;
        for (int i = 1; i < WIDTH - 1; ++i) {
            int idx = off + i;
            float lapX = gridU[idx-1] + gridU[idx+1] - 2.0f * gridU[idx];
            float lapY = gridU[idx-WIDTH] + gridU[idx+WIDTH] - 2.0f * gridU[idx];
            gridNext[idx] = gridU[idx] + 4.0f * (diffRateUXarr[idx] * lapX + diffRateUYarr[idx] * lapY);
        }
    }
    float* t = gridU; gridU = gridNext; gridNext = t;

}

void diffusionV() {

    float dRate4 = diffusionRateV * 4.0f;
    for (int j = 1; j < HEIGHT - 1; ++j) {
        int off = j * WIDTH;
        for (int i = 1; i < WIDTH - 1; ++i) {
            int idx = off + i;
            float lap = gridV[idx-1] + gridV[idx+1] + gridV[idx-WIDTH] + gridV[idx+WIDTH] - 4.0f * gridV[idx];
            gridNext[idx] = gridV[idx] + dRate4 * lap;
        }
    }
    float* t = gridV; gridV = gridNext; gridNext = t;

}

void reaction() {

    const float rRate4 = reactionRate * 4.0f;
    for (int i = 0; i < SCR; ++i) {
        float u = gridU[i];
        float v = gridV[i];

        float nextU = u + rRate4 * (u - (u * u * u) - v + karr[i]);
        float nextV = v + rRate4 * (farr[i]) * (u - v);

        gridU[i] = fmaxf(0.0f, fminf(1.0f, nextU));
        gridV[i] = fmaxf(0.0f, fminf(1.0f, nextV));
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    gridU = (float*)ps_malloc(4*SCR);
    gridV = (float*)ps_malloc(4*SCR);
    gridNext = (float*)ps_malloc(4*SCR);
    diffRateUXarr = (float*)ps_malloc(4*SCR);
    diffRateUYarr = (float*)ps_malloc(4*SCR);
    farr = (float*)ps_malloc(4*SCR);
    karr = (float*)ps_malloc(4*SCR);

    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    diffusionU();
    diffusionV();
    reaction();

    for (int i = 0; i < SCR; ++i) {
        float val = 255.0f * gridU[i];
        frameBuffer[i] = grayLUT[(uint8_t)val];
    }

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}