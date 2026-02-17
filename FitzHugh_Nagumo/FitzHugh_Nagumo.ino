// FitzHugh-Nagumo reaction diffusion //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH*HEIGHT) 

float randomf(float minf, float maxf) {return minf + (esp_random()%(1UL << 31)) * (maxf - minf) / (1UL << 31);}

uint16_t* frameBuffer;
float reactionRate = 0.2f;
float diffusionRate = 0.01f;
float kRate = 0.4f;
float fRate = 0.09f;  
float *gridU = NULL;  
float *gridV = NULL;
float *gridNext = NULL;
float *diffRateUYarr = NULL;
float *diffRateUXarr = NULL;
float *farr = NULL;
float *karr = NULL;
float *temp = NULL;

void rndseed(){

  memset(gridU, 0, 4*SCR);
  memset(gridV, 0, 4*SCR);
  memset(gridNext, 0, 4*SCR);
  
  diffusionRate = randomf(0.01f, 0.02f); 
  kRate = randomf(0.1f, 0.3f);
  fRate = randomf(0.02f, 0.05f);
  
  for(int i=0; i<SCR; ++i){
    gridU[i] = 1.0f;
    gridV[i] = randomf(0.0f, 0.8f);
    diffRateUYarr[i] = 0.06f + randomf(-0.01f, 0.01f);
    diffRateUXarr[i] = 0.02f + randomf(-0.01f, 0.01f);
  }

  setupF();
  setupK();

}

void diffusionV() {

    float *pNext = gridNext;
    float *pC = gridV;
    float *pU = gridV;
    float *pD = gridV + 2 * WIDTH;
    float dRate4 = diffusionRate * 4.0f;

    memcpy(gridNext, gridV, WIDTH * sizeof(float));

    for (int j = 1; j < HEIGHT - 1; ++j) {
        int offset = j * WIDTH;
        gridNext[offset] = gridV[offset]; 
        gridNext[offset + WIDTH - 1] = gridV[offset + WIDTH - 1];

        float *pNextPtr = gridNext + offset + 1;
        float *pCPtr = gridV + offset + 1;
        float *pUPtr = gridV + offset - WIDTH + 1;
        float *pDPtr = gridV + offset + WIDTH + 1;

        for (int i = 1; i < WIDTH - 1; ++i) {
            float laplacian = *(pCPtr-1) + *(pCPtr+1) + *pUPtr + *pDPtr - 4.0f * (*pCPtr);
            *pNextPtr = *pCPtr + dRate4 * laplacian;
            pNextPtr++; pCPtr++; pUPtr++; pDPtr++;
        }
    }

    memcpy(gridNext + (HEIGHT - 1) * WIDTH, gridV + (HEIGHT - 1) * WIDTH, WIDTH * sizeof(float));

    float* t = gridV; gridV = gridNext; gridNext = t;

}

void diffusionU() {

    float *pNext = gridNext + WIDTH;
    float *pC = gridU + WIDTH;
    float *pU = gridU;
    float *pD = gridU + 2 * WIDTH;
    float *pDX = diffRateUXarr + WIDTH;
    float *pDY = diffRateUYarr + WIDTH;

    for (int j = 1; j < HEIGHT - 1; ++j) {
        pNext[0] = pC[0];
        pNext[WIDTH-1] = pC[WIDTH-1];

        pC++; pU++; pD++; pNext++; pDX++; pDY++;
        for (int i = 1; i < WIDTH - 1; ++i) {
            float lapX = *(pC-1) + *(pC+1) - 2.0f * (*pC);
            float lapY = *pU + *pD - 2.0f * (*pC);
            *pNext = *pC + 4.0f * ((*pDX) * lapX + (*pDY) * lapY);
            pC++; pU++; pD++; pNext++; pDX++; pDY++;
        }
        pC++; pU++; pD++; pNext++; pDX++; pDY++;
    }
    float* t = gridU; gridU = gridNext; gridNext = t;

}

void reaction() {

    float *pU = gridU;
    float *pV = gridV;
    float *pK = karr;
    float *pF = farr;
    const float rRate4 = reactionRate * 4.0f;

    for (int i = 0; i < SCR; ++i) {
        float u = *pU;
        float v = *pV;
        
        float nextU = u + rRate4 * (u - (u * u * u) - v + *pK);
        float nextV = v + rRate4 * (*pF) * (u - v);

        if (nextU < 0.0f) nextU = 0.001f; else if (nextU > 1.0f) nextU = 1.0f;
        if (nextV < 0.0f) nextV = 0.001f; else if (nextV > 1.0f) nextV = 1.0f;

        *pU = nextU;
        *pV = nextV;

        pU++; pV++; pK++; pF++;
    }

}

void setupF() {

    float *pF = farr;
    float fStep = fRate / (float)WIDTH;
    
    for (int j = 0; j < HEIGHT; ++j) {
        float currentF = 0.01f; 
        for (int i = 0; i < WIDTH; ++i) {
            *pF++ = currentF;
            currentF += fStep;
        }
    }

}

void setupK() {

    float *pK = karr;
    float kStep = kRate / (float)HEIGHT;
    float currentK = 0.06f;

    for (int j = 0; j < HEIGHT; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            *pK++ = currentK;
        }
        currentK += kStep;
    }

}

void render() {

    float *pU = gridU;
    uint16_t *pOut = frameBuffer;

    for (int i = 0; i < SCR; ++i) {
        float val = *pU++;
        if (val > 1.0f) val = 1.0f;
        if (val < 0.0f) val = 0.0f;
        
        uint8_t coll = (uint8_t)(255.0f * val);
        
        uint16_t g5 = coll >> 3;
        uint16_t g6 = coll >> 2;
        *pOut++ = (g5 << 11) | (g6 << 5) | g5;
    }

}

void setup(){

    srand(time(NULL));
  
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    gridU = (float*)ps_malloc(4*SCR);
    gridV = (float*)ps_malloc(4*SCR);
    gridNext = (float*)ps_malloc(4*SCR);
    diffRateUYarr = (float*)ps_malloc(4*SCR);
    diffRateUXarr = (float*)ps_malloc(4*SCR);
    farr = (float*)ps_malloc(4*SCR);
    karr = (float*)ps_malloc(4*SCR);
    temp = (float*)ps_malloc(4*SCR);  
    frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    diffusionU();
    diffusionV();
    reaction();

    render();

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}