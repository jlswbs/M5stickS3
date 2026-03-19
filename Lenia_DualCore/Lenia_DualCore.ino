// 2D Lenia continuous cellular automaton - dual core and circle kernel //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH * HEIGHT)

float mu = 0.15f;
float sigma = 0.015f;
float dt = 0.3f;
int R = 13;

float *field, *nextField;
uint16_t* frameBuffer;
uint16_t grayLUT[256];

SemaphoreHandle_t sync_core0;
SemaphoreHandle_t sync_core1;

struct TaskParams {
    int startY;
    int endY;
};

float randomf(float minf, float maxf) { return minf + (esp_random() / 4294967295.0f) * (maxf - minf); }
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

#define LUT_SIZE 1024
float growthLUT[LUT_SIZE];

void precache_growth() {

  for (int i = 0; i < LUT_SIZE; i++) {
    float x = (float)i / (LUT_SIZE - 1);
    float diff = (x - mu) / sigma;
    growthLUT[i] = expf(-0.5f * diff * diff) * 2.0f - 1.0f;
  }

}

void rndseed() {

    precache_growth();

  mu = randomf(0.13f, 0.20f);
  sigma = randomf(0.012f, 0.022f);

  for (int i = 0; i < 256; i++) grayLUT[i] = color565(i, i, i);
  for(int i = 0; i < SCR; i++) field[i] = (randomf(0.0f, 1.0f) > 0.75f) ? randomf(0.0f, 1.0f) : 0.0f;

}

void compute_lenia_section(int startY, int endY) {

  float r_sq = (float)R * R;
  for (int y = startY; y < endY; y++) {

    int rowOff_y = y * WIDTH;
    for (int x = 0; x < WIDTH; x++) {

      float total_sum = 0.0f;
      int count = 0;

      int y_min = max(0, y - R);
      int y_max = min(HEIGHT - 1, y + R);
      int x_min = max(0, x - R);
      int x_max = min(WIDTH - 1, x + R);

      for (int ky = y_min; ky <= y_max; ky++) {
        float dy_sq = (float)(ky - y) * (float)(ky - y);
        int rowOff_ky = ky * WIDTH;
        for (int kx = x_min; kx <= x_max; kx++) {
          float dx = (float)(kx - x);
          if (dx * dx + dy_sq <= r_sq) {
            total_sum += field[kx + rowOff_ky];
            count++;
          }
        }
      }

      float avg = (count > 0) ? (total_sum / count) : 0;
      int lut_idx = (int)(avg * (LUT_SIZE - 1));
      if (lut_idx < 0) lut_idx = 0;
      if (lut_idx >= LUT_SIZE) lut_idx = LUT_SIZE - 1;

      float g_val = growthLUT[lut_idx];
      float val = field[x + rowOff_y] + dt * g_val;
      float res = constrain(val, 0.0f, 1.0f);
            
      nextField[x + rowOff_y] = res;
      frameBuffer[x + rowOff_y] = grayLUT[(uint8_t)(res * 255.0f)];

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

    field = (float*)malloc(SCR * sizeof(float));
    nextField = (float*)malloc(SCR * sizeof(float));

    frameBuffer = (uint16_t*)heap_caps_malloc(SCR * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    sync_core0 = xSemaphoreCreateBinary();
    sync_core1 = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(core1_task, "LeniaCore1", 4096, NULL, 1, NULL, 1);

    rndseed();

}

void loop() {

  M5.update();
  if (M5.BtnA.wasPressed()) rndseed();

  xSemaphoreGive(sync_core1);

  compute_lenia_section(0, HEIGHT / 2);

  xSemaphoreTake(sync_core0, portMAX_DELAY);

  float* temp = field;
  field = nextField;
  nextField = temp;

  M5.Display.waitDMA();
  M5.Display.pushImageDMA(0, 0, WIDTH, HEIGHT, frameBuffer);

}

void core1_task(void * pvParameters) {
  
  while(true) {

    xSemaphoreTake(sync_core1, portMAX_DELAY);
        
    compute_lenia_section(HEIGHT / 2, HEIGHT);
        
    xSemaphoreGive(sync_core0);

  }

}