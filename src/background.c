#include "background.h"

#include "core.h"
#include "raylib.h"
#include "theme.h"

#include <math.h>

#define CLOUD_COUNT 14
#define HIGH_ALTITUDE 16000.0f // ascent at which the sky reaches its dusk tint
#define SUN_X (SCREEN_WIDTH * 0.78f)
#define SUN_Y (SCREEN_HEIGHT * 0.20f)
#define SUN_R 46.0f

static float wrapf(float value, float range) {
  float r = fmodf(value, range);
  return r < 0.0f ? r + range : r;
}

// Fluffy cloud: a clump of overlapping circles on a flat base.
static void drawCloud(float x, float y, float s, Color c, float a) {
  Color col = Fade(c, a);
  DrawRectangle(x - 1.7f * s, y - 0.2f * s, 3.4f * s, 1.0f * s, col);
  DrawCircle(x - 1.6f * s, y, 1.0f * s, col);
  DrawCircle(x - 0.6f * s, y - 0.7f * s, 1.3f * s, col);
  DrawCircle(x + 0.6f * s, y - 0.5f * s, 1.1f * s, col);
  DrawCircle(x + 1.6f * s, y, 1.0f * s, col);
  DrawCircle(x, y, 1.5f * s, col);
}

static void drawSun(void) {
  DrawCircle(SUN_X, SUN_Y, SUN_R * 1.9f, Fade(SUN, 0.18f));
  DrawCircle(SUN_X, SUN_Y, SUN_R * 1.4f, Fade(SUN, 0.28f));
  DrawCircle(SUN_X, SUN_Y, SUN_R, SUN);
  DrawCircle(SUN_X, SUN_Y, SUN_R * 0.6f, SUN_CORE);
}

void drawMenuBackground(void) {
  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SKY_TOP, SKY_BOT);
  drawSun();
  // Big lazy drifting clouds.
  for (int i = 0; i < 7; i++) {
    float s = 26.0f + (i % 3) * 10.0f;
    float x = wrapf(i * 173.0f + (float)GetTime() * (8.0f + i),
                    SCREEN_WIDTH + 400.0f) -
              200.0f;
    float y = 110.0f + i * 92.0f;
    drawCloud(x, y, s, CLOUD, 0.9f);
  }
}

// Bright day sky that eases toward a soft dusk violet the higher you climb,
// with three parallax layers of fluffy clouds and a sun.
void drawGameBackground(float cameraY) {
  float ascent = -cameraY; // camera starts at 0 and decreases going up
  float high = clampf(ascent / HIGH_ALTITUDE, 0.0f, 1.0f);

  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                         ColorLerp(SKY_TOP, SKY_TOP_HIGH, high),
                         ColorLerp(SKY_BOT, SKY_BOT_HIGH, high));
  drawSun();

  for (int i = 0; i < CLOUD_COUNT; i++) {
    float depth = 0.25f + (i % 3) * 0.2f; // near layers drift/scroll more
    float s = 14.0f + (i % 3) * 9.0f;
    float x = wrapf(i * 151.0f + (float)GetTime() * (6.0f * depth),
                    SCREEN_WIDTH + 300.0f) -
              150.0f;
    float y =
        wrapf(i * 97.0f - cameraY * depth, SCREEN_HEIGHT + 200.0f) - 100.0f;
    drawCloud(x, y, s, CLOUD, 0.85f - depth * 0.3f);
  }
}
