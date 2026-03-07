// 2D Variable Belousov-Zhabotinsky reaction diffusion //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

#define FOCUS_COLS 3
#define FOCUS_ROWS 4
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

struct Focus {
    float x, y;
    float adjust;
};

Focus focuses[NUM_FOCUSES];
float *pixelAdjust;

float *a, *b, *c;
uint16_t* frameBuffer;

int offset_p = 0;
int offset_q = SCR;

float randomf(float minf, float maxf) { return minf + (esp_random() / 4294967295.0f) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

void rndseed() {

    float cellW = (float)WIDTH / FOCUS_COLS;
    float cellH = (float)HEIGHT / FOCUS_ROWS;

    for (int r = 0; r < FOCUS_ROWS; r++) {
        for (int c = 0; c < FOCUS_COLS; c++) {
            int i = r * FOCUS_COLS + c;
            focuses[i].x = (c * cellW) + (cellW / 2.0f);
            focuses[i].y = (r * cellH) + (cellH / 2.0f);
            focuses[i].adjust = randomf(0.5f, 1.8f);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float totalWeight = 0;
            float sumAdjust = 0;

            for (int f = 0; f < NUM_FOCUSES; f++) {
                float dx = x - focuses[f].x;
                float dy = y - focuses[f].y;
                float d2 = dx*dx + dy*dy;
                float weight = 1.0f / (d2 + 20.0f);
                
                sumAdjust += focuses[f].adjust * weight;
                totalWeight += weight;
            }
            pixelAdjust[y * WIDTH + x] = sumAdjust / totalWeight;
        }
    }

    for(int i = 0; i < SCR * 2; i++){
        a[i] = randomf(0.0f, 1.0f);
        b[i] = randomf(0.0f, 1.0f);
        c[i] = randomf(0.0f, 1.0f);
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    a = (float*)ps_malloc(SCR * 2 * sizeof(float));
    b = (float*)ps_malloc(SCR * 2 * sizeof(float));
    c = (float*)ps_malloc(SCR * 2 * sizeof(float));
    pixelAdjust = (float*)ps_malloc(SCR * sizeof(float));
    
    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    int p = offset_p;
    int q = offset_q;

    for (int y = 0; y < HEIGHT; y++) {

        int y_up  = ((y - 1 + HEIGHT) % HEIGHT) * WIDTH;
        int y_mid = y * WIDTH;
        int y_dn  = ((y + 1) % HEIGHT) * WIDTH;

        for (int x = 0; x < WIDTH; x++) {
            int x_l = (x - 1 + WIDTH) % WIDTH;
            int x_r = (x + 1) % WIDTH;
            int idx = x + y_mid;

            float sa = a[p + x_l + y_mid] + a[p + idx] + a[p + x_r + y_mid] +
                       a[p + x_l + y_up]  + a[p + x + y_up]  + a[p + x_r + y_up]  +
                       a[p + x_l + y_dn]  + a[p + x + y_dn]  + a[p + x_r + y_dn];

            float sb = b[p + x_l + y_mid] + b[p + idx] + b[p + x_r + y_mid] +
                       b[p + x_l + y_up]  + b[p + x + y_up]  + b[p + x_r + y_up]  +
                       b[p + x_l + y_dn]  + b[p + x + y_dn]  + b[p + x_r + y_dn];

            float sc = c[p + x_l + y_mid] + c[p + idx] + c[p + x_r + y_mid] +
                       c[p + x_l + y_up]  + c[p + x + y_up]  + c[p + x_r + y_up]  +
                       c[p + x_l + y_dn]  + c[p + x + y_dn]  + c[p + x_r + y_dn];

            float ca = sa * 0.11111111f;
            float cb = sb * 0.11111111f;
            float cc = sc * 0.11111111f;
            
            float localAdj = pixelAdjust[idx];
            
            float na = constrain(ca + ca * (localAdj * cb - cc), 0.0f, 1.0f);
            float nb = constrain(cb + cb * (cc - localAdj * ca), 0.0f, 1.0f);
            float nc = constrain(cc + cc * (ca - cb), 0.0f, 1.0f);

            a[q + idx] = na;
            b[q + idx] = nb;
            c[q + idx] = nc;
            
            frameBuffer[idx] = color565((uint8_t)(na * 255), (uint8_t)(nb * 255), (uint8_t)(nc * 255));
        }

    }

    offset_p = SCR - offset_p;
    offset_q = SCR - offset_q;

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}