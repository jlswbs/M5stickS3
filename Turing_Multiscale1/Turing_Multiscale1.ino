// 2D Multi-scale Turing patterns // 

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH*HEIGHT)

float *gridA, *gridB;
float *currentGrid, *nextGrid;
float *blurBuffer, *bestVariation, *inhibitor;
uint16_t* frameBuffer;
int *bestLevel;
bool *direction;

int radii[WIDTH];
float stepSizes[WIDTH];
int levels, blurlevels;
float base, stepScale, stepOffset, blurFactor;

float randomf(float minf, float maxf) { return minf + ((float)esp_random() / (float)RAND_MAX) * (maxf - minf); }

void rndrule() {

    base = randomf(1.4f, 2.5f);
    stepScale = randomf(0.01f, 0.06f);
    stepOffset = randomf(0.01f, 0.03f);
    blurFactor = randomf(0.7f, 0.9f);

    levels = 6;
    blurlevels = (int)((levels + 1) * blurFactor);

    for (int i = 0; i < levels; i++) {
        int maxRadius = fminf(WIDTH, HEIGHT) / 3;
        radii[i] = fminf((int)powf(base, i), maxRadius);
        stepSizes[i] = logf(radii[i]) * stepScale + stepOffset;
    }

    for (int i = 0; i < SCR; i++) {
        currentGrid[i] = randomf(-1.0f, 1.0f);
        bestVariation[i] = 1e38; 
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    gridA           = (float*)ps_malloc(SCR * sizeof(float));
    gridB           = (float*)ps_malloc(SCR * sizeof(float));
    blurBuffer      = (float*)ps_malloc(SCR * sizeof(float));
    bestVariation   = (float*)ps_malloc(SCR * sizeof(float));
    inhibitor       = (float*)ps_malloc(SCR * sizeof(float));
    bestLevel       = (int*)ps_malloc(SCR * sizeof(int));
    direction       = (bool*)ps_malloc(SCR * sizeof(bool));
    frameBuffer     = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    currentGrid = gridA;
    nextGrid = gridB;

    rndrule();

}

void loop() {

    M5.update();
    if(M5.BtnA.isPressed()) rndrule();

    M5.Display.waitDMA();

    float *currentActivator = currentGrid;
    float *currentInhibitor = inhibitor;

    for (int lvl = 0; lvl < levels - 1; lvl++) {
        int radius = radii[lvl];

        if (lvl <= blurlevels) {
            float *p_blur = blurBuffer;
            float *p_act = currentActivator;
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    if (y == 0 && x == 0) *p_blur = *p_act;
                    else if (y == 0) *p_blur = *(p_blur - 1) + *p_act;
                    else if (x == 0) *p_blur = *(p_blur - WIDTH) + *p_act;
                    else *p_blur = *(p_blur - 1) + *(p_blur - WIDTH) - *(p_blur - WIDTH - 1) + *p_act;
                    p_blur++; p_act++;
                }
            }
        }

        for (int y = 0; y < HEIGHT; y++) {
            int miny = (y - radius < 0) ? 0 : y - radius;
            int maxy = (y + radius >= HEIGHT) ? HEIGHT - 1 : y + radius;
            for (int x = 0; x < WIDTH; x++) {
                int minx = (x - radius < 0) ? 0 : x - radius;
                int maxx = (x + radius >= WIDTH) ? WIDTH - 1 : x + radius;
                int area = (maxx - minx) * (maxy - miny);
                int idx = y * WIDTH + x;

                float inh = (blurBuffer[maxy * WIDTH + maxx] - blurBuffer[maxy * WIDTH + minx]
                           - blurBuffer[miny * WIDTH + maxx] + blurBuffer[miny * WIDTH + minx]) / area;
                
                float v = fabsf(currentActivator[idx] - inh);
                if (lvl == 0 || v < bestVariation[idx]) {
                    bestVariation[idx] = v;
                    bestLevel[idx] = lvl;
                    direction[idx] = currentActivator[idx] > inh;
                }
                currentInhibitor[idx] = inh;
            }
        }

        currentActivator = currentInhibitor;
        
    }

    float smallest = 1e38;
    float largest = -1e38;

    for (int i = 0; i < SCR; i++) {
        float move = stepSizes[bestLevel[i]];
        if (direction[i]) nextGrid[i] = currentGrid[i] + move;
        else nextGrid[i] = currentGrid[i] - move;

        if (nextGrid[i] < smallest) smallest = nextGrid[i];
        if (nextGrid[i] > largest) largest = nextGrid[i];
    }

    float range = (largest - smallest) * 0.5f;

    for (int i = 0; i < SCR; i++) {      
        nextGrid[i] = ((nextGrid[i] - smallest) / range) - 1.0f;
        uint8_t val = 128 + (127.0f * nextGrid[i]);
        frameBuffer[i] = M5.Display.color565(val, val, val);
    }

    currentGrid = nextGrid;

    M5.Display.startWrite();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);
    M5.Display.endWrite();

}