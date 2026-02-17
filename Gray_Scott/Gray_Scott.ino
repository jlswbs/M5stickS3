// Gray-Scott reaction diffusion - SIMD ESP32-S3 //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

float randomf(float minf, float maxf) { return minf + (esp_random() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }

uint16_t* frameBuffer;
float diffU = 0.16f, diffV = 0.08f, paramF = 0.035f, paramK = 0.06f;
float *gridU, *gridV;

void rndseed() {

    diffU = randomf(0.0999f, 0.1999f);
    diffV = randomf(0.0749f, 0.0849f);
    paramF = randomf(0.0299f, 0.0399f);
    paramK = randomf(0.0549f, 0.0649f);

    for (int i = 0; i < SCR; i++) {
        gridU[i] = 1.0f;
        gridV[i] = 0.0f;
    }

    int numClusters = 4 + (esp_random() % 6);
    for (int c = 0; c < numClusters; c++) {
        int cx = 20 + (esp_random() % (WIDTH - 40));
        int cy = 20 + (esp_random() % (HEIGHT - 40));
        int r = 3 + (esp_random() % 6);

        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                int px = cx + i;
                int py = cy + j;
                if (px > 0 && px < WIDTH && py > 0 && py < HEIGHT) {
                    int idx = py * WIDTH + px;
                    gridU[idx] = 0.5f * randomf(0.0f, 2.0f);
                    gridV[idx] = 0.25f * randomf(0.5f, 2.5f);
                }
            }
        }
    }

}

typedef float v4f __attribute__ ((vector_size (16)));

void timestep(float F, float K, float diffU, float diffV) {

    v4f vF = {F, F, F, F};
    v4f vKF = {K + F, K + F, K + F, K + F};
    v4f vDiffU = {diffU, diffU, diffU, diffU};
    v4f vDiffV = {diffV, diffV, diffV, diffV};
    v4f vOne = {1.0f, 1.0f, 1.0f, 1.0f};
    v4f vM4 = {-4.0f, -4.0f, -4.0f, -4.0f};

    for (int j = 1; j < HEIGHT - 1; j++) {
        int row = j * WIDTH;

        for (int i = 1; i < WIDTH - 4; i += 4) {
            int idx = row + i;

            v4f u = *(v4f*)&gridU[idx];
            v4f v = *(v4f*)&gridV[idx];

            v4f uL = *(v4f*)&gridU[idx - 1];
            v4f uR = *(v4f*)&gridU[idx + 1];
            v4f uU = *(v4f*)&gridU[idx - WIDTH];
            v4f uD = *(v4f*)&gridU[idx + WIDTH];

            v4f vL = *(v4f*)&gridV[idx - 1];
            v4f vR = *(v4f*)&gridV[idx + 1];
            v4f vU_p = *(v4f*)&gridV[idx - WIDTH];
            v4f vD = *(v4f*)&gridV[idx + WIDTH];

            v4f uvv = u * v * v;

            v4f lapU = (uL + uR + uU + uD + (u * vM4));
            v4f lapV = (vL + vR + vU_p + vD + (v * vM4));

            v4f dU_vec = vDiffU * lapU - uvv + vF * (vOne - u);
            v4f dV_vec = vDiffV * lapV + uvv - vKF * v;

            *(v4f*)&gridU[idx] = u + dU_vec;
            *(v4f*)&gridV[idx] = v + dV_vec;
        }
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    gridU = (float*)heap_caps_aligned_alloc(16, SCR * sizeof(float), MALLOC_CAP_SPIRAM);
    gridV = (float*)heap_caps_aligned_alloc(16, SCR * sizeof(float), MALLOC_CAP_SPIRAM);

    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    for (int k = 0; k < 20; k++) timestep(paramF, paramK, diffU, diffV);

    float* pU = gridU;
    uint16_t* pFB = frameBuffer;
    for (int i = 0; i < SCR; i++) {
        uint8_t coll = (uint8_t)(255.0f * (*pU++));
        *pFB++ = ((coll & 0xF8) << 8) | ((coll & 0xFC) << 3) | (coll >> 3);
    }

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}