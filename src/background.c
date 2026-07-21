#include "background.h"

#include "core.h"
#include "raylib.h"
#include "theme.h"

#include <math.h>

#define CLOUD_COUNT 14
#define BLOCK_COUNT 20
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

static void drawSun(float alpha) {
  DrawCircle(SUN_X, SUN_Y, SUN_R * 1.9f, Fade(SUN, 0.18f * alpha));
  DrawCircle(SUN_X, SUN_Y, SUN_R * 1.4f, Fade(SUN, 0.28f * alpha));
  DrawCircle(SUN_X, SUN_Y, SUN_R, Fade(SUN, alpha));
  DrawCircle(SUN_X, SUN_Y, SUN_R * 0.6f, Fade(SUN_CORE, alpha));
}

// One neon "tetromino" cell: filled body with a lighter inset, arcade look.
static void drawBlock(float x, float y, float s, float a) {
  DrawRectangle(x, y, s, s, Fade(CLOUD_SHADE, a));
  DrawRectangle(x + s * 0.14f, y + s * 0.14f, s * 0.72f, s * 0.72f,
                Fade(CLOUD, a));
}

// Nature backdrop: sun + three parallax layers of clouds, faded by `alpha`.
static void drawCloudsBackdrop(float cameraY, float alpha) {
  drawSun(alpha);
  for (int i = 0; i < CLOUD_COUNT; i++) {
    float depth = 0.25f + (i % 3) * 0.2f; // near layers drift/scroll more
    float s = 14.0f + (i % 3) * 9.0f;
    float x = wrapf(i * 151.0f + (float)GetTime() * (6.0f * depth),
                    SCREEN_WIDTH + 300.0f) -
              150.0f;
    float y =
        wrapf(i * 97.0f - cameraY * depth, SCREEN_HEIGHT + 200.0f) - 100.0f;
    drawCloud(x, y, s, CLOUD, (0.85f - depth * 0.3f) * alpha);
  }
}

// Arcade backdrop: three parallax layers of falling neon blocks, faded by
// `alpha`. Same parallax idiom as the clouds so the crossfade lines up.
static void drawBlocksBackdrop(float cameraY, float alpha) {
  for (int i = 0; i < BLOCK_COUNT; i++) {
    float depth = 0.25f + (i % 3) * 0.2f;
    float s = 26.0f + (i % 3) * 16.0f;
    float x =
        wrapf(i * 137.0f + (i % 2 ? 40.0f : 0.0f), SCREEN_WIDTH + 200.0f) -
        100.0f;
    // Blocks fall: scroll down over time and with the camera (parallax).
    float y =
        wrapf(i * 113.0f + (float)GetTime() * (20.0f * depth) - cameraY * depth,
              SCREEN_HEIGHT + 200.0f) -
        100.0f;
    drawBlock(x, y, s, (0.75f - depth * 0.25f) * alpha);
  }
}

static void drawBackdrop(BackdropStyle style, float cameraY, float alpha) {
  if (alpha <= 0.01f) {
    return;
  }
  if (style == BACKDROP_BLOCKS) {
    drawBlocksBackdrop(cameraY, alpha);
  } else {
    drawCloudsBackdrop(cameraY, alpha);
  }
}

void drawMenuBackground(void) {
  themeUpdate(0.0f); // menu is always the first (nature) theme
  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SKY_TOP, SKY_BOT);
  drawSun(1.0f);
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

// Sky gradient morphs with altitude (blended palette), and the two neighbor
// themes' backdrops crossfade over the transition — clouds out, blocks in.
void drawGameBackground(float cameraY) {
  float ascent = -cameraY; // camera starts at 0 and decreases going up
  themeUpdate(ascent);

  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SKY_TOP, SKY_BOT);

  int lo, hi;
  float f;
  themeBlend(&lo, &hi, &f);
  drawBackdrop(THEMES[lo].backdrop, cameraY, 1.0f - f);
  drawBackdrop(THEMES[hi].backdrop, cameraY, f);
}
