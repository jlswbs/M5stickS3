// 2D Variable Gray-Scott reaction diffusion - SIMD ESP32-S3 //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

#define FOCUS_COLS 3
#define FOCUS_ROWS 4
#define NUM_FOCUSES (FOCUS_COLS * FOCUS_ROWS)

struct Focus {
    float x, y, F, K;
};

Focus focuses[NUM_FOCUSES];
float *mapF, *mapK;
float *gridU, *gridV;
uint16_t* frameBuffer;

float diffU = 0.16f, diffV = 0.08f;

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
            focuses[i].F = randomf(0.025f, 0.045f); 
            focuses[i].K = randomf(0.055f, 0.065f);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = y * WIDTH + x;
            float totalW = 0, sf = 0, sk = 0;
            for (int f = 0; f < NUM_FOCUSES; f++) {
                float d2 = powf(x - focuses[f].x, 2) + powf(y - focuses[f].y, 2);
                float w = 1.0f / (d2 + 35.0f);
                sf += focuses[f].F * w;
                sk += focuses[f].K * w;
                totalW += w;
            }
            mapF[idx] = sf / totalW;
            mapK[idx] = sk / totalW;
            
            gridU[idx] = 1.0f;
            gridV[idx] = 0.0f;
        }
    }

    for (int c = 0; c < 8; c++) {
        int cx = 20 + (esp_random() % (WIDTH - 40));
        int cy = 20 + (esp_random() % (HEIGHT - 40));
        int r = 5;
        for (int i = -r; i <= r; i++) {
            for (int j = -r; j <= r; j++) {
                int px = cx + i, py = cy + j;
                if (px > 0 && px < WIDTH && py > 0 && py < HEIGHT) {
                    gridU[py * WIDTH + px] = 0.5f;
                    gridV[py * WIDTH + px] = 0.25f;
                }
            }
        }
    }

}

typedef float v4f __attribute__ ((vector_size (16)));

void timestep() {

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
            
            v4f curF = *(v4f*)&mapF[idx];
            v4f curK = *(v4f*)&mapK[idx];

            v4f uL = *(v4f*)&gridU[idx - 1]; 
            v4f uR = *(v4f*)&gridU[idx + 1];
            v4f uU = *(v4f*)&gridU[idx - WIDTH]; 
            v4f uD = *(v4f*)&gridU[idx + WIDTH];

            v4f vL = *(v4f*)&gridV[idx - 1]; 
            v4f vR = *(v4f*)&gridV[idx + 1];
            v4f vU = *(v4f*)&gridV[idx - WIDTH]; 
            v4f vD = *(v4f*)&gridV[idx + WIDTH];

            v4f uvv = u * v * v;
            v4f lapU = uL + uR + uU + uD + (u * vM4);
            v4f lapV = vL + vR + vU + vD + (v * vM4);

            v4f dU = vDiffU * lapU - uvv + curF * (vOne - u);
            v4f dV = vDiffV * lapV + uvv - (curK + curF) * v;

            *(v4f*)&gridU[idx] = u + dU;
            *(v4f*)&gridV[idx] = v + dV;
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
    mapF  = (float*)heap_caps_aligned_alloc(16, SCR * sizeof(float), MALLOC_CAP_SPIRAM);
    mapK  = (float*)heap_caps_aligned_alloc(16, SCR * sizeof(float), MALLOC_CAP_SPIRAM);

    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    for (int k = 0; k < 15; k++) timestep();

    for (int i = 0; i < SCR; i++) { frameBuffer[i] = grayLUT[(uint8_t)(gridU[i] * 255.0f)]; }

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}