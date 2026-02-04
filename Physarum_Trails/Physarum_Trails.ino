// 2D Physarum chemotax trails //

#include "M5Unified.h"

#define WIDTH   135
#define HEIGHT  240
#define SCR     (WIDTH*HEIGHT)

#define NUM_SPECIES  2
#define AGENTS_PER_SPECIES 1500
#define TOTAL_AGENTS (NUM_SPECIES * AGENTS_PER_SPECIES)
#define MOVE_SPEED 1.0f
#define SENSOR_DISTANCE 7.0f
#define SENSOR_ANGLE  0.8f
#define DEPOSIT_AMOUNT 150.0f
#define DECAY_RATE 0.985f

uint16_t* frameBuffer;

struct Agent {
    float x;
    float y;
    float angle;
    int species;
};

float *trail = NULL;
float *trailTemp = NULL;
uint16_t *coll = NULL;
Agent *agents = NULL;

float randf() { return (float)(esp_random() % 1000000) / 1000000.0f; }

uint16_t blendColors(uint16_t c1, uint16_t c2, float ratio) {

  uint8_t r1 = (c1 >> 11) & 0x1F;
  uint8_t g1 = (c1 >> 5) & 0x3F;
  uint8_t b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F;
  uint8_t g2 = (c2 >> 5) & 0x3F;
  uint8_t b2 = c2 & 0x1F;
  uint8_t r = r1 + (uint8_t)((r2 - r1) * ratio);
  uint8_t g = g1 + (uint8_t)((g2 - g1) * ratio);
  uint8_t b = b1 + (uint8_t)((b2 - b1) * ratio);
  return (r << 11) | (g << 5) | b;

}

float senseTrail(int species, float x, float y) {

  return trail[species * SCR + get_idx((int)x, (int)y)];

}

float senseEnemyTrail(int species, float x, float y) {

  int idx = get_idx((int)x, (int)y);
  float enemySum = 0.0f;
  for (int s = 0; s < NUM_SPECIES; s++) {
    if (s != species) {
      enemySum += trail[s * SCR + idx];
    }
  }

  return enemySum;

}

void rndseed() {

  memset(trail, 0, NUM_SPECIES * SCR * sizeof(float));
  memset(trailTemp, 0, SCR * sizeof(float));
  memset(frameBuffer, 0, SCR * sizeof(uint16_t));

  for (int i = 0; i < NUM_SPECIES; i++) {
    float hue = (float)i / (float)NUM_SPECIES;
    uint8_t r, g, b;
    if (hue < 0.166f) { r = 31; g = hue * 378; b = 0; }
    else if (hue < 0.333f) { r = (0.333f - hue) * 186; g = 63; b = 0; }
    else if (hue < 0.5f) { r = 0; g = 63; b = (hue - 0.333f) * 186; }
    else if (hue < 0.666f) { r = 0; g = (0.666f - hue) * 378; b = 31; }
    else if (hue < 0.833f) { r = (hue - 0.666f) * 186; g = 0; b = 31; }
    else { r = 31; g = 0; b = (1.0f - hue) * 186; }
    coll[i] = ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
  }

  for (int i = 0; i < NUM_SPECIES * SCR; i++) trail[i] = 0.0f;
    
  for (int i = 0; i < TOTAL_AGENTS; i++) {
    agents[i].x = randf() * WIDTH;
    agents[i].y = randf() * HEIGHT;
    agents[i].angle = randf() * 2.0f * M_PI;
    agents[i].species = i / AGENTS_PER_SPECIES;
  }

}

int get_idx(int x, int y) {

  if (x < 0) x = 0; 
  if (x >= WIDTH) x = WIDTH - 1;
  if (y < 0) y = 0; 
  if (y >= HEIGHT) y = HEIGHT - 1;
  return y * WIDTH + x;

}

void nextstep() {

  for (int i = 0; i < TOTAL_AGENTS; i++) {

    Agent *ag = &agents[i];

    float sA = SENSOR_ANGLE;
    float sD = SENSOR_DISTANCE;
        
    float fwdX = ag->x + cosf(ag->angle) * sD;
    float fwdY = ag->y + sinf(ag->angle) * sD;
    float lftX = ag->x + cosf(ag->angle + sA) * sD;
    float lftY = ag->y + sinf(ag->angle + sA) * sD;
    float rgtX = ag->x + cosf(ag->angle - sA) * sD;
    float rgtY = ag->y + sinf(ag->angle - sA) * sD;

    float wF = senseTrail(ag->species, fwdX, fwdY) - senseEnemyTrail(ag->species, fwdX, fwdY) * 0.5f;
    float wL = senseTrail(ag->species, lftX, lftY) - senseEnemyTrail(ag->species, lftX, lftY) * 0.5f;
    float wR = senseTrail(ag->species, rgtX, rgtY) - senseEnemyTrail(ag->species, rgtX, rgtY) * 0.5f;

    float rndSteer = (randf() - 0.5f) * 0.3f;
    if (wF > wL && wF > wR) ag->angle += rndSteer;
    else if (wL > wR) ag->angle += sA + rndSteer;
    else if (wR > wL) ag->angle -= sA + rndSteer;
    else ag->angle += (randf() - 0.5f) * 1.0f;

    ag->x += cosf(ag->angle) * MOVE_SPEED;
    ag->y += sinf(ag->angle) * MOVE_SPEED;

    if (ag->x < 0) { 
      ag->x = 0; 
      ag->angle = M_PI - ag->angle; 
    } else if (ag->x >= WIDTH) { 
      ag->x = WIDTH - 1; 
      ag->angle = M_PI - ag->angle; 
    }

    if (ag->y < 0) { 
      ag->y = 0; 
      ag->angle = -ag->angle; 
    } else if (ag->y >= HEIGHT) { 
      ag->y = HEIGHT - 1; 
      ag->angle = -ag->angle; 
    }

    int tidx = ag->species * SCR + get_idx((int)ag->x, (int)ag->y);
    trail[tidx] += DEPOSIT_AMOUNT;
    if (trail[tidx] > 100.0f) trail[tidx] = 100.0f;
  }

  for (int i = 0; i < NUM_SPECIES * SCR; i++) {
    trail[i] *= DECAY_RATE;
  }

}


void setup() {

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Speaker.end();
  M5.Display.setRotation(0);
  M5.Display.setSwapBytes(true);
  M5.Display.initDMA();

  trail = (float *)ps_malloc(NUM_SPECIES * SCR * sizeof(float));
  trailTemp = (float *)ps_malloc(SCR * sizeof(float));
  coll = (uint16_t *)ps_malloc(NUM_SPECIES * sizeof(uint16_t));
  agents = (Agent *)ps_malloc(TOTAL_AGENTS * sizeof(Agent));
  frameBuffer = (uint16_t*)malloc(SCR * sizeof(uint16_t));

  rndseed();

}

void loop() {

  M5.update();
  if(M5.BtnA.isPressed()) rndseed();

  M5.Display.waitDMA();

  nextstep();

  for (int y = 0; y < HEIGHT; y++) {
    int yOff = y * WIDTH;
    for (int x = 0; x < WIDTH; x++) {
      int idx = yOff + x;
      float maxTrail = 0.0f;
      int dominant = -1;

      for (int s = 0; s < NUM_SPECIES; s++) {
        float val = trail[s * SCR + idx];
        if (val > maxTrail) {
          maxTrail = val;
          dominant = s;
        }
      }

      if (dominant >= 0 && maxTrail > 0.5f) {
        float intensity = (maxTrail > 100.0f) ? 1.0f : maxTrail / 100.0f;
        frameBuffer[x+y*WIDTH] = blendColors(0x0000, coll[dominant], intensity);
      }
            
    }
  }

  M5.Display.startWrite();
  M5.Display.pushImage(0, 0, WIDTH, HEIGHT, frameBuffer);
  M5.Display.endWrite();

}