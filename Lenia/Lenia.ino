// Lenia continuous cellular automaton //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

float mu = 0.15f;
float sigma = 0.015f;
float dt = 0.25f;
int R = 12;

float *field, *nextField, *sat;
uint16_t* frameBuffer;

float randomf(float minf, float maxf) { return minf + (esp_random() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

float growth(float x) {

    float diff = (x - mu) / sigma;
    return expf(-0.5f * diff * diff) * 2.0f - 1.0f;

}

void rndseed() {

    for(int i = 0; i < SCR; i++) {
        field[i] = (randomf(0.0f, 1.0f) > 0.75f) ? randomf(0.2f, 0.8f) : 0.0f;
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    field = (float*)ps_malloc(SCR * sizeof(float));
    nextField = (float*)ps_malloc(SCR * sizeof(float));
    sat = (float*)ps_malloc(SCR * sizeof(float));
    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    for (int y = 0; y < HEIGHT; y++) {
        float rowSum = 0;
        for (int x = 0; x < WIDTH; x++) {
            rowSum += field[x + y * WIDTH];
            sat[x + y * WIDTH] = (y > 0 ? sat[x + (y-1) * WIDTH] : 0) + rowSum;
        }
    }

    int r_inner = R;
    int r_side = R * 0.7f; 

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            
            int x1a = max(0, x - r_inner), x1b = min(WIDTH - 1, x + r_inner);
            int y1a = max(0, y - r_side),  y1b = min(HEIGHT - 1, y + r_side);
            float s1 = sat[x1b + y1b*WIDTH] - sat[x1a + y1b*WIDTH] - sat[x1b + y1a*WIDTH] + sat[x1a + y1a*WIDTH];
            float a1 = (x1b - x1a) * (y1b - y1a);

            int x2a = max(0, x - r_side),  x2b = min(WIDTH - 1, x + r_side);
            int y2a = max(0, y - r_inner), y2b = min(HEIGHT - 1, y + r_inner);
            float s2 = sat[x2b + y2b*WIDTH] - sat[x2a + y2b*WIDTH] - sat[x2b + y2a*WIDTH] + sat[x2a + y2a*WIDTH];
            float a2 = (x2b - x2a) * (y2b - y2a);

            int x3a = max(0, x - r_side),  x3b = min(WIDTH - 1, x + r_side);
            int y3a = max(0, y - r_side),  y3b = min(HEIGHT - 1, y + r_side);
            float s3 = sat[x3b + y3b*WIDTH] - sat[x3a + y3b*WIDTH] - sat[x3b + y3a*WIDTH] + sat[x3a + y3a*WIDTH];
            float a3 = (x3b - x3a) * (y3b - y3a);

            float avg = (s1 + s2 - s3) / (a1 + a2 - a3);
            
            float val = field[x + y * WIDTH] + dt * growth(avg);
            val = constrain(val, 0.0f, 1.0f);
            
            nextField[x + y * WIDTH] = val;

            float enhanced = powf(val, 0.5f);
            uint8_t c = (uint8_t)(enhanced * 255);
            frameBuffer[x + y * WIDTH] = color565(c, c, c);
        }
    }

    float* temp = field; 
    field = nextField; 
    nextField = temp;

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}