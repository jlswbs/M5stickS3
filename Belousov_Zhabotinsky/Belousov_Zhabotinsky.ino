// Belousov-Zhabotinsky reaction diffusion //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

float randomf(float minf, float maxf) { return minf + (esp_random() % (1UL << 31)) * (maxf - minf) / (1UL << 31); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

uint16_t* frameBuffer;
float *a, *b, *c;

float adjust = 1.2f;
int offset_p = 0;
int offset_q = SCR;

void rndseed() {

    adjust = randomf(0.75f, 1.35f);
    offset_p = 0;
    offset_q = SCR;

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
    
    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    rndseed();

}

void loop() {

    M5.update();
    if (M5.BtnA.isPressed()) rndseed();

    float *ap = a + offset_p;
    float *bp = b + offset_p;
    float *cp = c + offset_p;
    float *aq_ptr = a + offset_q;
    float *bq_ptr = b + offset_q;
    float *cq_ptr = c + offset_q;
    uint16_t *fb_ptr = frameBuffer;

    for (int y = 0; y < HEIGHT; y++) {

        int y_up  = ((y - 1 + HEIGHT) % HEIGHT) * WIDTH;
        int y_mid = y * WIDTH;
        int y_dn  = ((y + 1) % HEIGHT) * WIDTH;

        for (int x = 0; x < WIDTH; x++) {

            int x_l = (x - 1 + WIDTH) % WIDTH;
            int x_r = (x + 1) % WIDTH;

            float sa = ap[x_l + y_mid] + ap[x + y_mid] + ap[x_r + y_mid] +
                       ap[x_l + y_up]  + ap[x + y_up]  + ap[x_r + y_up]  +
                       ap[x_l + y_dn]  + ap[x + y_dn]  + ap[x_r + y_dn];

            float sb = bp[x_l + y_mid] + bp[x + y_mid] + bp[x_r + y_mid] +
                       bp[x_l + y_up]  + bp[x + y_up]  + bp[x_r + y_up]  +
                       bp[x_l + y_dn]  + bp[x + y_dn]  + bp[x_r + y_dn];

            float sc = cp[x_l + y_mid] + cp[x + y_mid] + cp[x_r + y_mid] +
                       cp[x_l + y_up]  + cp[x + y_up]  + cp[x_r + y_up]  +
                       cp[x_l + y_dn]  + cp[x + y_dn]  + cp[x_r + y_dn];

            float ca = sa * 0.11111111f;
            float cb = sb * 0.11111111f;
            float cc = sc * 0.11111111f;
            
            float na = constrain(ca + ca * (adjust * cb - cc), 0.0f, 1.0f);
            float nb = constrain(cb + cb * (cc - adjust * ca), 0.0f, 1.0f);
            float nc = constrain(cc + cc * (ca - cb), 0.0f, 1.0f);

            *aq_ptr++ = na;
            *bq_ptr++ = nb;
            *cq_ptr++ = nc;
            *fb_ptr++ = color565((uint8_t)(na * 255), (uint8_t)(nb * 255), (uint8_t)(nc * 255));

        }
        
    }

    offset_p = SCR - offset_p;
    offset_q = SCR - offset_q;

    M5.Display.waitDMA();
    M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}