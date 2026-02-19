// Complex Ginzburg-Landau reaction diffusion //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

float param_b = 1.0f;
float param_c = -1.5f;
float dt = 0.1f;

uint16_t* frameBuffer;
float *re, *im;

int offset_p = 0;
int offset_q = SCR;

float randomf(float minf, float maxf) { return minf + (esp_random() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

void rndseed() {

    offset_p = 0;
    offset_q = SCR;

    param_b = randomf(0.1f, 1.5f);
    param_c = randomf(-2.5f, -0.1f);

    for(int i = 0; i < SCR * 2; i++){
        re[i] = randomf(-0.5f, 0.5f);
        im[i] = randomf(-0.5f, 0.5f);
    }

}

void setup() {

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.end();
    M5.Display.setRotation(0);
    M5.Display.setSwapBytes(true);
    M5.Display.initDMA();

    re = (float*)ps_malloc(SCR * 2 * sizeof(float));
    im = (float*)ps_malloc(SCR * 2 * sizeof(float));
    
    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    float *rep = re + offset_p;
    float *imp = im + offset_p;
    float *req_ptr = re + offset_q;
    float *imq_ptr = im + offset_q;
    uint16_t *fb_ptr = frameBuffer;

    for (int y = 0; y < HEIGHT; y++) {

        int y_up  = ((y - 1 + HEIGHT) % HEIGHT) * WIDTH;
        int y_mid = y * WIDTH;
        int y_dn  = ((y + 1) % HEIGHT) * WIDTH;

        for (int x = 0; x < WIDTH; x++) {

            int x_l = (x - 1 + WIDTH) % WIDTH;
            int x_r = (x + 1) % WIDTH;

            float lapRe = (rep[x_l + y_mid] + rep[x_r + y_mid] + rep[x + y_up] + rep[x + y_dn] - 4.0f * rep[x + y_mid]);
            float lapIm = (imp[x_l + y_mid] + imp[x_r + y_mid] + imp[x + y_up] + imp[x + y_dn] - 4.0f * imp[x + y_mid]);

            float R = rep[x + y_mid];
            float I = imp[x + y_mid];
            float sqMag = R*R + I*I;

            float dR = R - (R - param_c * I) * sqMag + (lapRe - param_b * lapIm);
            float dI = I - (I + param_c * R) * sqMag + (lapIm + param_b * lapRe);

            float nRe = R + dR * dt;
            float nIm = I + dI * dt;

            nRe = constrain(nRe, -1.5f, 1.5f);
            nIm = constrain(nIm, -1.5f, 1.5f);

            *req_ptr++ = nRe;
            *imq_ptr++ = nIm;

            uint8_t r_col = (uint8_t)((nRe + 1.5f) * 85.0f);
            uint8_t g_col = (uint8_t)(sqMag * 120.0f);
            uint8_t b_col = (uint8_t)((nIm + 1.5f) * 85.0f);
            
            *fb_ptr++ = color565(r_col, g_col, b_col);

        }

    }

    offset_p = SCR - offset_p;
    offset_q = SCR - offset_q;

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}