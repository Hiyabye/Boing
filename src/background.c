#include "background.h"

#include "core.h"
#include "raylib.h"
#include "rlgl.h"
#include "tex.h"
#include "theme.h"

#include <math.h>

#define BLOCK_COUNT 20
#define STAR_COUNT 44
#define MAZE_CELL 110.0f
#define SUN_X (SCREEN_WIDTH * 0.78f)
#define SUN_Y (SCREEN_HEIGHT * 0.20f)
#define SUN_R 46.0f

#define FAR_START 0.76f
#define FAR_END 0.94f
#define MID_START 0.82f
#define MID_END 0.98f
#define NEAR_START 0.90f

static float wrapf(float value, float range) {
  float r = fmodf(value, range);
  return r < 0.0f ? r + range : r;
}

static float easedWindow(float phase, float start, float end) {
  float t = clampf((phase - start) / (end - start), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

static float bridgeBell(float t) { return 4.0f * t * (1.0f - t); }

// Parallax depth for decor element `i`: three quantized layers (near layers
// drift and scroll more). Clouds and blocks share it so their crossfade lines
// up during a theme transition.
static float parallaxDepth(int i) { return 0.25f + (i % 3) * 0.2f; }

// Fluffy cloud: one shaded underpaint plus three TEX_CLOUD body stamps. One
// quad would read as a circle however much noise is inside it, so the lumpy
// silhouette still comes from the arrangement. The texture supplies the
// chewed edge and internal structure that flat circles cannot.
static void drawCloud(float x, float y, float s, Color c, float a) {
  drawTex(TEX_CLOUD, x, y + 0.4f * s, 5.3f * s, 2.6f * s, 0.0f,
          Fade(CLOUD_SHADE, 0.38f * a));
  Color col = Fade(c, a);
  drawTex(TEX_CLOUD, x - 1.3f * s, y + 0.15f * s, 2.9f * s, 2.1f * s, 0.0f,
          col);
  drawTex(TEX_CLOUD, x + 1.3f * s, y + 0.15f * s, 2.7f * s, 1.9f * s, 0.0f,
          col);
  drawTex(TEX_CLOUD, x, y - 0.3f * s, 3.9f * s, 2.9f * s, 0.0f, col);
}

// Sun/moon. The two hard-edged halo discs became one additive TEX_PUFF: a real
// falloff instead of two visible steps, and additive so it reads as light cast
// on the sky rather than paint laid over it. `detail` grows the moon craters.
static void drawCelestial(float radius, float detail, float alpha) {
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, SUN_X, SUN_Y, radius * 8.0f, radius * 8.0f, 0.0f,
          Fade(SUN, 0.32f * alpha));
  EndBlendMode();
  DrawCircle(SUN_X, SUN_Y, radius, Fade(SUN, alpha));
  DrawCircle(SUN_X, SUN_Y, radius * 0.6f, Fade(SUN_CORE, alpha));
  if (detail > 0.01f) {
    Color cr = Fade(ColorBrightness(SUN_CORE, -0.18f), alpha * detail);
    DrawCircle(SUN_X - radius * 0.3f, SUN_Y - radius * 0.2f, radius * 0.18f,
               cr);
    DrawCircle(SUN_X + radius * 0.25f, SUN_Y + radius * 0.15f, radius * 0.13f,
               cr);
    DrawCircle(SUN_X + radius * 0.05f, SUN_Y - radius * 0.35f, radius * 0.1f,
               cr);
  }
}

static void drawSun(float alpha) { drawCelestial(SUN_R, 0.0f, alpha); }
static void drawMoon(float alpha) { drawCelestial(SUN_R, 1.0f, alpha); }

// Sky haze for the three dark backdrops: one TEX_NEBULA quad wrapped
// vertically as two stacked copies. Deliberately *not* tiled — perlin noise has
// a hard seam at its own edges, and one scrolling pair shows that seam once per
// 1200 units instead of at every tile line. Faint by design; it exists to stop
// the sky gradient reading as a flat wash.
#define NEBULA_H (SCREEN_HEIGHT * 1.5f)
static void drawNebula(float cameraY, float alpha) {
  float y = wrapf(-cameraY * 0.06f, NEBULA_H);
  Color c = Fade(CLOUD, 0.13f * alpha);
  for (int k = -1; k <= 1; k += 2) {
    drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, y + k * NEBULA_H * 0.5f,
            SCREEN_WIDTH * 1.4f, NEBULA_H, 0.0f, c);
  }
}

// One beveled cell in an arbitrary color: dark border + body + a top sheen, so
// stacked cells read as chunky 3D blocks (used by the tetromino backdrop).
static void drawBlockC(float x, float y, float s, Color c, float a) {
  DrawRectangle(x, y, s, s, Fade(ColorBrightness(c, -0.5f), a));
  DrawRectangle(x + s * 0.12f, y + s * 0.12f, s * 0.76f, s * 0.76f, Fade(c, a));
  DrawRectangle(x + s * 0.12f, y + s * 0.12f, s * 0.76f, s * 0.18f,
                Fade(ColorBrightness(c, 0.4f), a * 0.85f)); // sheen
}

// The seven tetromino footprints as (dx,dy) cells; drawTetromino stamps four
// beveled cells so the L5 backdrop rains real shapes, not lone squares.
// Kept non-negative so world.c can use the same offsets as collision boxes.
const signed char TETRO[][4][2] = {
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // I
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, // O
    {{0, 0}, {1, 0}, {2, 0}, {1, 1}}, // T
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}}, // S
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // Z
    {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // L
    {{0, 0}, {1, 0}, {2, 0}, {0, 1}}, // J
};
// A missing shape would be a zero-filled row: four cells stacked on one square,
// which is a legal-looking platform you cannot land on the visible half of.
STATIC_ASSERT(sizeof(TETRO) / sizeof(TETRO[0]) == TETRO_COUNT, tetro_full);

void drawTetromino(float x, float y, float cell, int shape, Color c, float a) {
  for (int k = 0; k < 4; k++) {
    drawBlockC(x + TETRO[shape][k][0] * cell, y + TETRO[shape][k][1] * cell,
               cell, c, a);
  }
}

// The landscape is built from broad illustrated planes instead of stacked
// ellipses. Each ridge is an x-monotonic polygon, so it has a clean silhouette
// without a rectangular skirt showing through the layer above it.
#define RIDGE_POINTS 19
static float ridgeY(float x, float baseY, float amp, float phase, float peaks) {
  float u = (x + phase) / SCREEN_WIDTH;
  float rolling =
      sinf(u * PI * 2.0f) + sinf(u * PI * 5.0f + phase * 0.01f) * 0.28f;
  float mountain = -0.45f - fabsf(sinf(u * PI * 3.0f + phase * 0.008f)) -
                   fabsf(sinf(u * PI * 7.0f)) * 0.18f;
  return baseY + amp * (rolling * (1.0f - peaks) + mountain * peaks);
}

static void drawLandscapeBand(float baseY, float amp, float phase, Color c,
                              float alpha, float rim, float peaks) {
  Vector2 ridge[RIDGE_POINTS];
  Vector2 fan[RIDGE_POINTS + 3];
  fan[0] = (Vector2){SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT + 120.0f};
  fan[1] = (Vector2){SCREEN_WIDTH + 80.0f, SCREEN_HEIGHT + 120.0f};
  for (int i = 0; i < RIDGE_POINTS; i++) {
    float x = -80.0f + i * (SCREEN_WIDTH + 160.0f) / (RIDGE_POINTS - 1);
    ridge[i] = (Vector2){x, ridgeY(x, baseY, amp, phase, peaks)};
  }
  // raylib culls clockwise triangles in its y-down shape coordinates, so the
  // fan boundary runs from bottom-right across the ridge right-to-left.
  for (int i = 0; i < RIDGE_POINTS; i++) {
    fan[i + 2] = ridge[RIDGE_POINTS - 1 - i];
  }
  fan[RIDGE_POINTS + 2] = (Vector2){-80.0f, SCREEN_HEIGHT + 120.0f};
  DrawTriangleFan(fan, RIDGE_POINTS + 3, Fade(c, alpha));
  DrawSplineCatmullRom(ridge, RIDGE_POINTS, rim,
                       Fade(ColorBrightness(c, -0.08f), alpha * 0.22f));
}

// Soft textured crowns keep the trees in the same painted language as the
// clouds. The sparse trunks and branches remain crisp enough to read at depth.
static void drawStoryTree(float x, float groundY, float s, float alpha) {
  Color wood = Fade((Color){0x72, 0x4A, 0x32, 0xFF}, alpha);
  DrawLineEx((Vector2){x, groundY},
             (Vector2){x + s * 0.08f, groundY - s * 1.55f}, s * 0.2f, wood);
  DrawLineEx((Vector2){x, groundY - s * 0.9f},
             (Vector2){x - s * 0.5f, groundY - s * 1.35f}, s * 0.1f, wood);
  drawTex(TEX_CLOUD, x, groundY - s * 1.55f, s * 2.5f, s * 2.0f, 0.0f,
          Fade(GRASS_DK, alpha));
  drawTex(TEX_CLOUD, x - s * 0.45f, groundY - s * 1.75f, s * 1.5f, s * 1.35f,
          -8.0f, Fade(GRASS, alpha));
  drawTex(TEX_CLOUD, x + s * 0.48f, groundY - s * 1.65f, s * 1.45f, s * 1.25f,
          9.0f, Fade(ColorBrightness(GRASS, 0.2f), alpha * 0.8f));
}

static void drawCottage(float x, float y, float s, float alpha) {
  Color wall = Fade((Color){0xFF, 0xED, 0xC8, 0xFF}, alpha);
  Color roof = Fade((Color){0xC8, 0x5A, 0x48, 0xFF}, alpha);
  DrawRectangleRounded(
      (Rectangle){x - 28.0f * s, y - 28.0f * s, 56.0f * s, 28.0f * s}, 0.12f, 4,
      wall);
  DrawTriangle((Vector2){x - 36.0f * s, y - 26.0f * s},
               (Vector2){x + 36.0f * s, y - 26.0f * s},
               (Vector2){x, y - 55.0f * s}, roof);
  DrawRectangle(x + 14.0f * s, y - 18.0f * s, 10.0f * s, 18.0f * s,
                Fade((Color){0x76, 0x55, 0x48, 0xFF}, alpha));
  DrawRectangle(x - 19.0f * s, y - 17.0f * s, 11.0f * s, 9.0f * s,
                Fade(SUN_CORE, alpha));
  drawTex(TEX_PUFF, x - 13.5f * s, y - 12.5f * s, 28.0f * s, 22.0f * s, 0.0f,
          Fade(SUN, alpha * 0.28f));
}

static void drawPatchwork(float alpha) {
  drawLandscapeBand(570.0f, 34.0f, 170.0f, ColorBrightness(GRASS, 0.16f),
                    0.82f * alpha, 3.0f, 0.0f);
  drawLandscapeBand(635.0f, 30.0f, 350.0f, ColorLerp(GRASS, COIN_HI, 0.16f),
                    0.78f * alpha, 3.0f, 0.0f);
  drawLandscapeBand(700.0f, 26.0f, 80.0f, ColorBrightness(GRASS_DK, 0.12f),
                    0.72f * alpha, 3.0f, 0.0f);
}

#define RIVER_POINTS 7
static const Vector2 RIVER[] = {{450, 474}, {425, 520}, {475, 575}, {430, 640},
                                {505, 710}, {450, 770}, {520, 850}};
static const Vector2 RIVER_GRID[] = {{440, 430}, {440, 500}, {440, 570},
                                     {440, 640}, {440, 710}, {440, 780},
                                     {440, 850}};
STATIC_ASSERT(sizeof(RIVER) / sizeof(RIVER[0]) == RIVER_POINTS,
              river_points_full);
STATIC_ASSERT(sizeof(RIVER_GRID) / sizeof(RIVER_GRID[0]) == RIVER_POINTS,
              river_grid_full);

static void drawRiver(float alpha) {
  Color water = ColorLerp(SKY_TOP, SKY_BOT, 0.5f);
  DrawSplineCatmullRom(RIVER, RIVER_POINTS, 24.0f, Fade(water, 0.62f * alpha));
  DrawSplineCatmullRom(RIVER, RIVER_POINTS, 4.0f,
                       Fade(SUN_CORE, 0.58f * alpha));
}

static void drawMeadowSprig(float x, float y, float side, float s,
                            float alpha) {
  Vector2 stem[] = {{x, y},
                    {x + side * 14.0f * s, y - 58.0f * s},
                    {x + side * 8.0f * s, y - 126.0f * s},
                    {x + side * 34.0f * s, y - 192.0f * s}};
  Color dark = Fade(ColorBrightness(GRASS_DK, -0.28f), 0.68f * alpha);
  DrawSplineCatmullRom(stem, 4, 5.0f * s, dark);
  for (int i = 1; i < 4; i++) {
    float leafSide = (i % 2) ? -side : side;
    drawTex(TEX_CLOUD, stem[i].x + leafSide * 20.0f * s, stem[i].y + 10.0f * s,
            58.0f * s, 24.0f * s, leafSide * 28.0f,
            Fade(GRASS_DK, 0.7f * alpha));
  }
}

static void drawForegroundFoliage(float cameraY, float alpha) {
  float sway = 0.04f * sinf(-cameraY * 0.001f);
  drawMeadowSprig(-8.0f, 830.0f, 1.0f, 1.1f + sway, alpha);
  drawMeadowSprig(24.0f, 840.0f, 1.0f, 0.82f - sway, 0.8f * alpha);
  drawMeadowSprig(808.0f, 830.0f, -1.0f, 1.18f - sway, alpha);
  drawMeadowSprig(776.0f, 845.0f, -1.0f, 0.78f + sway, 0.8f * alpha);
}

static void drawPollen(float cameraY, float alpha) {
  for (int i = 0; i < 18; i++) {
    float x = wrapf(i * 97.0f + (float)GetTime() * (4.0f + i % 3),
                    SCREEN_WIDTH + 40.0f) -
              20.0f;
    float y = wrapf(i * 83.0f - cameraY * 0.22f, SCREEN_HEIGHT + 80.0f) - 40.0f;
    float r = 1.0f + (i % 3) * 0.55f;
    DrawCircle(x, y + sinf((float)GetTime() + i) * 5.0f, r,
               Fade(SUN_CORE, (0.18f + (i % 4) * 0.06f) * alpha));
  }
}

// L1 far layer: day atmosphere, cloud depth, and the valley silhouette.
static void drawHillsFar(float cameraY, float alpha, float sink, int omitDisc) {
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.32f,
          SCREEN_WIDTH * 1.35f, SCREEN_HEIGHT * 0.75f, 0.0f,
          Fade(SUN_CORE, 0.09f * alpha));
  if (!omitDisc) {
    drawSun(alpha);
  }

  for (int i = 0; i < 7; i++) {
    float depth = 0.1f + (i % 3) * 0.08f;
    float s = 22.0f + (i % 3) * 8.0f;
    float x = wrapf(i * 197.0f + (float)GetTime() * (5.0f * depth),
                    SCREEN_WIDTH + 380.0f) -
              190.0f;
    float y =
        wrapf(80.0f + i * 91.0f - cameraY * depth, SCREEN_HEIGHT + 340.0f) -
        170.0f;
    drawCloud(x, y, s, CLOUD, (0.58f - depth * 0.35f) * alpha);
  }

  Color farMountain =
      ColorBrightness(ColorLerp(SKY_TOP, GRASS_DK, 0.18f), -0.18f);
  Color nearMountain =
      ColorBrightness(ColorLerp(CLOUD_SHADE, GRASS_DK, 0.58f), -0.1f);
  rlPushMatrix();
  rlTranslatef(0.0f, sink, 0.0f);
  drawLandscapeBand(455.0f, 110.0f, 90.0f, farMountain, 0.92f * alpha, 3.0f,
                    0.88f);
  drawLandscapeBand(510.0f, 105.0f, 310.0f, nearMountain, 0.9f * alpha, 4.0f,
                    0.78f);
  drawLandscapeBand(525.0f, 48.0f, 40.0f, ColorBrightness(GRASS, 0.04f),
                    0.98f * alpha, 5.0f, 0.0f);
  rlPopMatrix();
}

// L1 mid layer: patterned fields, river, trees, cottage, and pollen. It can
// sink as one plane during the cloudline bridge without moving the sky.
static void drawHillsMid(float cameraY, float alpha, float sink) {
  rlPushMatrix();
  rlTranslatef(0.0f, sink, 0.0f);
  drawPatchwork(alpha);
  drawRiver(alpha);
  drawTex(TEX_CLOUD, 180.0f, 615.0f, 260.0f, 105.0f, -8.0f,
          Fade(ColorBrightness(GRASS_DK, -0.2f), 0.08f * alpha));
  drawTex(TEX_CLOUD, 650.0f, 690.0f, 310.0f, 120.0f, 6.0f,
          Fade(ColorBrightness(GRASS_DK, -0.2f), 0.07f * alpha));

  for (int i = 0; i < 7; i++) {
    float x = 24.0f + i * 126.0f;
    float ground = ridgeY(x, 525.0f, 48.0f, 40.0f, 0.0f) + 8.0f;
    drawStoryTree(x, ground, 18.0f + (i % 3) * 4.0f,
                  (0.72f + (i % 2) * 0.1f) * alpha);
  }
  drawCottage(610.0f, ridgeY(610.0f, 525.0f, 48.0f, 40.0f, 0.0f) + 8.0f, 0.88f,
              0.9f * alpha);
  drawPollen(cameraY, alpha);
  rlPopMatrix();
}

// One bright sky bird, built from simple animated shapes so its wingbeat stays
// readable against the cloud banks. Sizes vary by parallax layer below.
static void drawSkyBird(float x, float y, float s, float phase, float alpha) {
  float flap = sinf((float)GetTime() * 5.0f + phase);
  Color body = Fade((Color){0x4D, 0x59, 0x6D, 0xFF}, alpha);
  Color wing = Fade((Color){0x71, 0x7D, 0x90, 0xFF}, alpha);
  DrawTriangle((Vector2){x - s * 0.65f, y},
               (Vector2){x - s * 1.15f, y - s * 0.32f},
               (Vector2){x - s * 0.95f, y + s * 0.28f}, body);
  DrawEllipse(x, y, s * 0.78f, s * 0.32f, body);
  DrawTriangle((Vector2){x - s * 0.15f, y},
               (Vector2){x - s * 0.65f, y - s * (0.55f + flap * 0.3f)},
               (Vector2){x + s * 0.4f, y - s * 0.08f}, wing);
  DrawCircle(x + s * 0.68f, y - s * 0.12f, s * 0.28f, body);
  DrawTriangle((Vector2){x + s * 0.9f, y - s * 0.12f},
               (Vector2){x + s * 1.25f, y - s * 0.02f},
               (Vector2){x + s * 0.9f, y + s * 0.08f}, Fade(COIN_LO, alpha));
  DrawCircle(x + s * 0.76f, y - s * 0.18f, fmaxf(1.0f, s * 0.045f),
             Fade(BLACK, alpha));
}

// L2 far layer: clear air and the shared daylight disc.
static void drawSkiesFar(float alpha, int omitDisc) {
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.35f,
          SCREEN_WIDTH * 1.3f, SCREEN_HEIGHT * 0.8f, 0.0f,
          Fade(SUN_CORE, 0.1f * alpha));
  if (!omitDisc) {
    drawSun(0.9f * alpha);
  }
}

// L2 mid layer: high cloud banks, wind ribbons, and migrating birds. There is
// deliberately no ground plane, so the climb feels clear of Sunny Hills.
static void drawSkiesMid(float cameraY, float alpha) {
  for (int i = 0; i < 13; i++) {
    float depth = 0.1f + (i % 4) * 0.08f;
    float s = 22.0f + (i % 4) * 12.0f;
    float x = wrapf(i * 181.0f + (float)GetTime() * (9.0f + depth * 16.0f),
                    SCREEN_WIDTH + 440.0f) -
              220.0f;
    float y =
        wrapf(45.0f + i * 127.0f - cameraY * depth, SCREEN_HEIGHT + 360.0f) -
        180.0f;
    drawCloud(x, y, s, CLOUD, (0.76f - depth * 0.45f) * alpha);
  }

  for (int i = 0; i < 3; i++) {
    float y = wrapf(170.0f + i * 260.0f - cameraY * (0.16f + i * 0.05f),
                    SCREEN_HEIGHT + 180.0f) -
              90.0f;
    Vector2 gust[] = {{-70.0f, y + 14.0f},
                      {180.0f, y - 18.0f},
                      {510.0f, y + 20.0f},
                      {870.0f, y - 8.0f}};
    DrawSplineCatmullRom(gust, 4, 3.0f,
                         Fade(SUN_CORE, (0.16f + i * 0.04f) * alpha));
  }

  for (int i = 0; i < 9; i++) {
    float depth = 0.18f + (i % 3) * 0.09f;
    float s = 10.0f + (i % 3) * 5.0f;
    float x = wrapf((float)GetTime() * (28.0f + i * 2.0f) + i * 137.0f,
                    SCREEN_WIDTH + 180.0f) -
              90.0f;
    float y =
        wrapf(70.0f + i * 89.0f - cameraY * depth, SCREEN_HEIGHT + 160.0f) -
        80.0f;
    drawSkyBird(x, y, s, i * 0.8f, (0.48f + depth) * alpha);
  }
}

// A twinkling star: tiny ones are pixels, bright ones a four-point sparkle.
static void drawStar(float x, float y, float s, float tw, Color c) {
  Color col = Fade(c, tw);
  if (s >= 3.0f) {
    DrawRectangle(x - s, y - 1, 2.0f * s, 2.0f, col);
    DrawRectangle(x - 1, y - s, 2.0f, 2.0f * s, col);
    DrawCircle(x, y, 1.6f, col);
  } else {
    DrawRectangle(x, y, s, s, col);
  }
}

// A floating voxel island: grass-topped dirt block tapering to a point, with
// cube seams — reads as a chunk of blocky terrain hanging in the night sky.
static void drawVoxelIsland(float x, float y, float s, float a) {
  Color grassC = GRASS, dirt = GRASS_DK;
  DrawRectangle(x - s, y, 2.0f * s, s * 0.8f, Fade(dirt, a));
  // Stone grain, over the main rectangular body only. Cellular noise is
  // distance-to-seed, so it draws a web of veins — but it fills its own square
  // box, so putting it on anything but a rectangle overhangs the silhouette.
  drawTex(TEX_ROCK, x, y + s * 0.4f, 2.0f * s, s * 0.8f, 0.0f,
          Fade(ColorBrightness(dirt, -0.45f), a * 0.5f));
  DrawRectangle(x - s * 0.6f, y + s * 0.8f, s * 1.2f, s * 0.5f,
                Fade(ColorBrightness(dirt, -0.2f), a));
  DrawRectangle(x - s * 0.25f, y + s * 1.3f, s * 0.5f, s * 0.35f,
                Fade(ColorBrightness(dirt, -0.35f), a));
  DrawRectangle(x - s, y - s * 0.3f, 2.0f * s, s * 0.35f, Fade(grassC, a));
  for (int k = -1; k <= 1; k++) {
    DrawRectangle(x + k * s * 0.66f, y - s * 0.3f, 2.0f, s * 1.1f,
                  Fade(ColorBrightness(dirt, -0.4f), a * 0.7f)); // cube seams
  }
}

// Two parallax layers of the tiled starfield, replacing the 35 dim single-pixel
// stars the loop below used to draw (the 9 bright ones stay — they twinkle, and
// a texture can't).
//
// Two sign traps live here. Climbing makes `cameraY` negative, and a *source
// rect* scrolls the opposite way a position does — a growing srcY pulls content
// up — so the offset is negated twice over to keep stars sliding down past the
// player. The wrap is seamless because the offset is modulo one tile and the
// texture is GL_REPEAT, which also swallows the negative coordinate.
static void drawStarfield(float cameraY, float alpha) {
  Texture2D t = texGet(TEX_STARFIELD);
  if (t.id == 0) {
    return;
  }
  // {on-screen tile size, parallax depth, alpha, source x offset}. The x
  // offsets keep the two layers from being the same picture twice.
  //
  // The far layer's tile is the texture's own pixel size so it maps 1:1 and its
  // 1px stars stay sharp; the near one is magnified 1.5x into bigger, closer
  // stars. Enlarging these would thin the field — a screenful is one tile's
  // worth however big the tile is — but magnifying a 1px star just blurs it
  // into bokeh. Thin the field in tex.c's STAR_THIN instead, which drops whole
  // stars and leaves the survivors sharp.
  static const float LAYER[2][4] = {{512.0f, 0.10f, 0.7f, 0.0f},
                                    {768.0f, 0.26f, 1.0f, 213.0f}};
  for (int i = 0; i < 2; i++) {
    float tile = LAYER[i][0];
    float scroll = wrapf(-cameraY * LAYER[i][1], tile);
    Rectangle src = {LAYER[i][3], -scroll / tile * t.height,
                     SCREEN_WIDTH / tile * t.width,
                     SCREEN_HEIGHT / tile * t.height};
    DrawTexturePro(t, src, (Rectangle){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
                   (Vector2){0, 0}, 0.0f, Fade(SUN_CORE, LAYER[i][2] * alpha));
  }
}

// L3 far layer: night haze, stars, moon, and the occasional shooting star.
static void drawStarsFar(float cameraY, float alpha, int omitDisc) {
  drawNebula(cameraY, alpha);
  drawStarfield(cameraY, alpha);
  if (!omitDisc) {
    drawMoon(alpha * 0.95f);
  }
  // Only the bright sparkles remain as primitives: the starfield texture is a
  // still image, so the twinkle has to be drawn. Same stride the old `i % 5`
  // size test used, so these land exactly where they always did.
  for (int i = 0; i < STAR_COUNT; i += 5) {
    float depth = 0.2f + (i % 3) * 0.12f;
    float x = wrapf(i * 71.0f, SCREEN_WIDTH);
    float y = wrapf(i * 137.0f - cameraY * depth, SCREEN_HEIGHT + 100.0f);
    float tw = (0.4f + 0.6f * fabsf(sinf((float)GetTime() * 1.6f + i))) * alpha;
    drawStar(x, y, 3.5f, tw, SUN_CORE);
  }
  float st = fmodf((float)GetTime(), 7.0f); // shooting star every ~7s
  if (st < 0.8f) {
    float p = st / 0.8f;
    float sx = SCREEN_WIDTH * (0.1f + p * 0.7f);
    float sy = 90.0f + p * 120.0f;
    DrawLineEx((Vector2){sx, sy}, (Vector2){sx - 40.0f, sy - 22.0f}, 2.5f,
               Fade(SUN_CORE, (1.0f - p) * alpha));
  }
}

// L3 mid layer: floating square islands beneath the star field.
static void drawStarsMid(float cameraY, float alpha) {
  for (int i = 0; i < BLOCK_COUNT / 3; i++) {
    float depth = 0.3f + (i % 2) * 0.15f;
    float s = 24.0f + (i % 3) * 12.0f;
    float x = wrapf(i * 251.0f, SCREEN_WIDTH + 200.0f) - 100.0f;
    float y =
        wrapf(i * 211.0f - cameraY * depth, SCREEN_HEIGHT + 300.0f) - 80.0f;
    drawVoxelIsland(x, y, s, (0.55f - depth * 0.2f) * alpha);
  }
}

// A drifting arcade ghost: rounded dome, skirt, scalloped hem, and eyes.
static void drawGhost(float x, float y, float s, Color c, float a) {
  Color body = Fade(c, a);
  DrawCircle(x, y, s, body);                  // dome
  DrawRectangle(x - s, y, 2.0f * s, s, body); // skirt
  DrawCircle(x - s * 0.6f, y + s, s * 0.4f, body);
  DrawCircle(x, y + s, s * 0.4f, body);
  DrawCircle(x + s * 0.6f, y + s, s * 0.4f, body); // scalloped hem
  Color w = Fade(WHITE, a);
  Color p = Fade((Color){0x2A, 0x2A, 0xE0, 0xFF}, a);
  DrawCircle(x - s * 0.35f, y - s * 0.1f, s * 0.28f, w);
  DrawCircle(x + s * 0.35f, y - s * 0.1f, s * 0.28f, w);
  DrawCircle(x - s * 0.28f, y - s * 0.1f, s * 0.14f, p);
  DrawCircle(x + s * 0.42f, y - s * 0.1f, s * 0.14f, p);
}

// Small stable hash for the maze wall/pellet pattern (per world cell).
static int hash2(int i, int j) {
  unsigned h = (unsigned)i * 92837111u + (unsigned)j * 689287499u;
  unsigned shifted = h >> 13;
  if (h & 0x80000000u) {
    shifted |= 0xfff80000u; // preserve the old arithmetic right shift
  }
  h ^= shifted;
  h *= 1274126177u;
  return (int)(h & 0x7fffffffu);
}

// L4 far layer: scrolling broken corridors and their signal pellets.
static void drawMazeFar(float cameraY, float alpha) {
  drawNebula(cameraY, alpha);
  float sc = -cameraY * 0.4f;
  float off = wrapf(sc, MAZE_CELL);
  int scroll = (int)floorf(sc / MAZE_CELL);
  int cols = (int)(SCREEN_WIDTH / MAZE_CELL) + 2;
  int rows = (int)(SCREEN_HEIGHT / MAZE_CELL) + 3;
  float t = 8.0f; // wall thickness
  Color glow = Fade(CLOUD, 0.16f * alpha);
  Color wall = Fade(ColorBrightness(CLOUD, 0.25f), 0.65f * alpha);
  // The whole lattice goes down additively — neon *is* light, and one blend
  // switch around the grid beats 180 of them inside it (BeginBlendMode flushes
  // the batch). Ghosts and the hero below stay on normal blend: they are
  // characters lit by the maze, not part of it.
  BeginBlendMode(BLEND_ADDITIVE);
  for (int gy = 0; gy < rows; gy++) {
    for (int gx = 0; gx < cols; gx++) {
      float cx = gx * MAZE_CELL - MAZE_CELL;
      float cy = gy * MAZE_CELL - MAZE_CELL + off;
      int h = hash2(gx, gy + scroll);
      if (h & 1) { // right wall
        Rectangle r = {cx + MAZE_CELL - t / 2, cy, t, MAZE_CELL};
        DrawRectangleRounded(
            (Rectangle){r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 1, 4,
            glow);
        DrawRectangleRounded(r, 1, 4, wall);
      }
      if (h & 2) { // bottom wall
        Rectangle r = {cx, cy + MAZE_CELL - t / 2, MAZE_CELL, t};
        DrawRectangleRounded(
            (Rectangle){r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 1, 4,
            glow);
        DrawRectangleRounded(r, 1, 4, wall);
      }
      float px = cx + MAZE_CELL / 2, py = cy + MAZE_CELL / 2;
      if (h & 4) { // pulsing power-pellet
        float pr = 6.0f + 2.0f * sinf((float)GetTime() * 4.0f + h);
        drawTex(TEX_PUFF, px, py, pr * 6.0f, pr * 6.0f, 0.0f,
                Fade(COIN_HI, 0.5f * alpha));
        DrawCircle(px, py, pr, Fade(COIN_HI, 0.8f * alpha));
      } else {
        drawTex(TEX_PUFF, px, py, 22.0f, 22.0f, 0.0f,
                Fade(COIN_HI, 0.3f * alpha));
        DrawCircle(px, py, 3.0f, Fade(COIN_HI, 0.55f * alpha));
      }
    }
  }
  EndBlendMode();
}

// L4 mid layer: distant maze inhabitants moving between the corridors.
static void drawMazeMid(float cameraY, float alpha) {
  for (int i = 0; i < 4; i++) { // drifting ghosts
    float x = wrapf((float)GetTime() * (14.0f + i * 4) + i * 220.0f,
                    SCREEN_WIDTH + 160.0f) -
              80.0f;
    float depth = 0.3f + 0.1f * i;
    float y =
        wrapf(i * 260.0f - cameraY * depth, SCREEN_HEIGHT + 240.0f) - 60.0f;
    drawGhost(x, y, 22.0f, gTheme.confetti[i % CONFETTI_COUNT], 0.7f * alpha);
  }
  { // chomping hero crossing a parallax band
    float x = wrapf((float)GetTime() * 40.0f, SCREEN_WIDTH + 120.0f) - 60.0f;
    float y = wrapf(80.0f - cameraY * 0.35f, SCREEN_HEIGHT + 200.0f) - 60.0f;
    float mouth = 22.0f * fabsf(sinf((float)GetTime() * 6.0f));
    DrawCircleSector((Vector2){x, y}, 20.0f, mouth, 360.0f - mouth, 24,
                     Fade(COIN_HI, 0.85f * alpha));
  }
}

// L5 far layer: the faint vertical lines of a playfield well.
static void drawBlocksFar(float cameraY, float alpha) {
  drawNebula(cameraY, alpha);
  for (int i = 0; i * 44 <= SCREEN_WIDTH; i++) {
    DrawRectangle(i * 44, 0, 1, SCREEN_HEIGHT, Fade(CLOUD, 0.04f * alpha));
  }
}

// L5 mid layer: muted tetrominoes behind the brighter gameplay pieces.
static void drawBlocksMid(float cameraY, float alpha) {
  for (int i = 0; i < BLOCK_COUNT; i++) {
    float depth = parallaxDepth(i);
    float cell = 14.0f + (i % 3) * 8.0f;
    float x =
        wrapf(i * 137.0f + (i % 2 ? 40.0f : 0.0f), SCREEN_WIDTH + 200.0f) -
        100.0f;
    float y =
        wrapf(i * 113.0f + (float)GetTime() * (20.0f * depth) - cameraY * depth,
              SCREEN_HEIGHT + 200.0f) -
        100.0f;
    Color ghost =
        ColorLerp(SKY_TOP_HIGH, gTheme.confetti[i % CONFETTI_COUNT], 0.28f);
    drawTetromino(x, y, cell, i % TETRO_COUNT, ghost,
                  (0.42f - depth * 0.12f) * alpha);
  }
}

// Blocks -> Hills: falling cells first lock into a flat horizon, then soften
// onto the incoming mountain ridge. One bright grid cell rounds and grows into
// the new sun, while a faint predawn ribbon preserves light across the change.
static void drawRebootFar(float cameraY, float t) {
  if (t <= 0.0f) {
    return;
  }
  float round = easedWindow(t, 0.18f, 0.92f);
  float gridX = floorf(SUN_X / 44.0f) * 44.0f + 22.0f;
  float gridY = floorf(SUN_Y / 44.0f) * 44.0f + 22.0f;
  float cellX = gridX + (SUN_X - gridX) * round;
  float cellY = gridY + (SUN_Y - gridY) * round;
  float cell = 22.0f - 6.0f * round;
  float cellIn = easedWindow(t, 0.0f, 0.12f);
  if (round < 0.999f) {
    drawBlockC(cellX - cell * 0.5f, cellY - cell * 0.5f, cell,
               ColorLerp(CLOUD, SUN_CORE, round), cellIn * (1.0f - round));
  }
  drawCelestial(11.0f + (SUN_R - 11.0f) * round, 0.0f, round);

  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float lock = easedWindow(t, 0.08f, 0.56f);
  float soften = easedWindow(t, 0.42f, 0.94f);
  Vector2 ridge[11];
  for (int i = 0; i < 11; i++) {
    float x = -20.0f + i * 84.0f;
    float depth = 0.18f + (i % 3) * 0.04f;
    float falling =
        wrapf(70.0f + i * 107.0f + (float)GetTime() * 18.0f - cameraY * depth,
              SCREEN_HEIGHT + 180.0f) -
        90.0f;
    float horizon = 440.0f + (i % 2) * 3.0f;
    float mountain = ridgeY(x, 465.0f, 92.0f, 90.0f, 0.88f);
    float target = horizon + (mountain - horizon) * soften;
    float y = falling + (target - falling) * lock;
    Color c = ColorLerp(gTheme.confetti[i % CONFETTI_COUNT], GRASS_DK, soften);
    drawBlockC(x - 8.0f, y - 8.0f, 16.0f, c, alpha * (0.72f - 0.34f * soften));
    ridge[i] = (Vector2){x, target};
  }
  DrawSplineCatmullRom(ridge, 11, 4.0f - 1.5f * soften,
                       Fade(ColorLerp(CLOUD, GRASS_DK, soften), 0.5f * alpha));

  Vector2 aurora[] = {{-40.0f, 270.0f},
                      {190.0f, 210.0f},
                      {430.0f, 255.0f},
                      {670.0f, 190.0f},
                      {840.0f, 235.0f}};
  DrawSplineCatmullRom(aurora, 5, 18.0f - 10.0f * t,
                       Fade(ColorLerp(CLOUD, SUN, 0.45f), 0.12f * alpha));
  DrawSplineCatmullRom(aurora, 5, 3.0f, Fade(SUN_CORE, 0.2f * alpha));
}

// Settled cell rows flatten and widen into patchwork fields. A straight well
// line bends into the river while loose pixels round into rising seeds and
// tiny sprouts, changing motion only after the blocks have come to rest.
static void drawRebootMid(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float settle = easedWindow(t, 0.05f, 0.64f);
  float organic = easedWindow(t, 0.40f, 0.94f);
  static const float BAND_Y[] = {560.0f, 635.0f, 705.0f};
  for (int i = 0; i < 12; i++) {
    float depth = 0.28f + (i % 3) * 0.05f;
    float falling =
        wrapf(i * 127.0f + (float)GetTime() * (18.0f * depth) - cameraY * depth,
              SCREEN_HEIGHT + 180.0f) -
        90.0f;
    float targetX = 45.0f + (i % 4) * 235.0f + (i / 4) * 24.0f;
    float targetY = BAND_Y[i / 4] + sinf(i * 1.7f) * 12.0f;
    float startX = wrapf(i * 151.0f, SCREEN_WIDTH + 80.0f) - 40.0f;
    float x = startX + (targetX - startX) * settle;
    float y = falling + (targetY - falling) * settle;
    float w = 18.0f + (62.0f + (i % 3) * 14.0f) * organic;
    float h = 18.0f + 14.0f * organic;
    Color field = (i / 4 == 1) ? ColorLerp(GRASS, COIN_HI, 0.16f)
                               : (i / 4 == 0 ? GRASS : GRASS_DK);
    Color c = ColorLerp(gTheme.confetti[i % CONFETTI_COUNT], field, organic);
    DrawRectangleRounded((Rectangle){x - w * 0.5f, y - h * 0.5f, w, h},
                         0.08f + 0.72f * organic, 6, Fade(c, 0.5f * alpha));
    DrawRectangle(x - w * 0.42f, y - h * 0.34f, w * 0.84f,
                  fmaxf(1.0f, h * 0.12f),
                  Fade(ColorBrightness(c, 0.28f), 0.28f * alpha));
  }

  float bend = easedWindow(t, 0.22f, 0.9f);
  Vector2 water[RIVER_POINTS];
  for (int i = 0; i < RIVER_POINTS; i++) {
    water[i] =
        (Vector2){RIVER_GRID[i].x + (RIVER[i].x - RIVER_GRID[i].x) * bend,
                  RIVER_GRID[i].y + (RIVER[i].y - RIVER_GRID[i].y) * bend};
  }
  DrawSplineCatmullRom(water, RIVER_POINTS, 9.0f + 8.0f * bend,
                       Fade(ColorLerp(CLOUD, SKY_BOT, bend), 0.38f * alpha));
  DrawSplineCatmullRom(water, RIVER_POINTS, 2.5f,
                       Fade(SUN_CORE, 0.48f * alpha));

  float sprout = easedWindow(t, 0.54f, 0.96f);
  for (int i = 0; i < 14; i++) {
    float startX = wrapf(34.0f + i * 91.0f, SCREEN_WIDTH + 50.0f) - 25.0f;
    float startY = wrapf(i * 73.0f + (float)GetTime() * 14.0f -
                             cameraY * (0.2f + (i % 2) * 0.04f),
                         SCREEN_HEIGHT + 100.0f) -
                   50.0f;
    float seedX = wrapf(i * 97.0f + (float)GetTime() * (3.0f + i % 3),
                        SCREEN_WIDTH + 40.0f) -
                  20.0f;
    float seedY =
        wrapf(700.0f - i * 61.0f - (float)GetTime() * (28.0f + (i % 3) * 3.0f) -
                  cameraY * 0.02f,
              SCREEN_HEIGHT + 100.0f) -
        50.0f;
    float x = startX + (seedX - startX) * sprout;
    float y = startY + (seedY - startY) * sprout;
    Color c = ColorLerp(gTheme.confetti[i % CONFETTI_COUNT], SUN_CORE, sprout);
    DrawRectangle(x - 2.5f, y - 2.5f, 5.0f * (1.0f - sprout),
                  5.0f * (1.0f - sprout), Fade(c, 0.5f * alpha));
    DrawCircle(x, y, 1.2f + 1.0f * sprout, Fade(c, 0.55f * alpha * sprout));
    if (sprout > 0.55f) {
      float grow = (sprout - 0.55f) / 0.45f;
      DrawLineEx((Vector2){x, y + 8.0f * grow}, (Vector2){x, y}, 1.2f,
                 Fade(GRASS_DK, 0.42f * alpha * grow));
    }
  }
}

// Hills -> Skies: the valley drops away while one river stroke straightens
// into a wind ribbon. Pollen gains span and motion until it reads as tiny
// birds.
static void drawCloudlineBridge(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }

  float windY = wrapf(430.0f - cameraY * 0.18f, SCREEN_HEIGHT + 180.0f) - 90.0f;
  const Vector2 river[] = {{450, 474}, {425, 520}, {475, 575}, {430, 640}};
  const Vector2 wind[] = {{-70, 14}, {180, -18}, {510, 20}, {870, -8}};
  Vector2 ribbon[4];
  for (int i = 0; i < 4; i++) {
    ribbon[i] = (Vector2){river[i].x + (wind[i].x - river[i].x) * t,
                          river[i].y + (windY + wind[i].y - river[i].y) * t};
  }
  DrawSplineCatmullRom(ribbon, 4, 9.0f - 6.0f * t,
                       Fade(ColorLerp(SKY_BOT, SUN_CORE, t), 0.38f * alpha));
  DrawSplineCatmullRom(ribbon, 4, 2.5f, Fade(SUN_CORE, 0.65f * alpha));

  for (int i = 0; i < 10; i++) {
    float x = wrapf(i * 103.0f + (float)GetTime() * (5.0f + 20.0f * t),
                    SCREEN_WIDTH + 60.0f) -
              30.0f;
    float y = wrapf(i * 79.0f - cameraY * (0.2f + i % 2 * 0.04f),
                    SCREEN_HEIGHT + 100.0f) -
              50.0f;
    float span = 1.0f + 7.0f * t;
    DrawCircle(x, y, 1.8f - 1.2f * t,
               Fade(SUN_CORE, 0.45f * alpha * (1.0f - t)));
    DrawLineEx((Vector2){x - span, y}, (Vector2){x, y - 2.5f * t}, 1.2f + t,
               Fade(SUN_CORE, 0.48f * alpha * t));
    DrawLineEx((Vector2){x, y - 2.5f * t}, (Vector2){x + span, y}, 1.2f + t,
               Fade(SUN_CORE, 0.48f * alpha * t));
  }
}

// Skies -> Night: the shared disc contracts through blue hour as moon detail
// appears. Rounded cloud slivers square off and grow block undersides.
static void drawTwilightDisc(float t) {
  float pinch = bridgeBell(t);
  drawCelestial(SUN_R * (1.0f - 0.22f * pinch), easedWindow(t, 0.28f, 0.88f),
                0.9f + 0.05f * t);
}

static void drawTwilightIslands(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  for (int i = 0; i < 5; i++) {
    float x = 60.0f + i * 180.0f;
    float y = wrapf(i * 181.0f - cameraY * (0.24f + 0.03f * (i % 2)),
                    SCREEN_HEIGHT + 240.0f) -
              80.0f;
    float w = 100.0f - 34.0f * t;
    float h = 20.0f + 20.0f * t;
    Color body = ColorLerp(CLOUD, GRASS_DK, t);
    DrawRectangleRounded((Rectangle){x - w * 0.5f, y, w, h}, 1.0f - 0.9f * t, 8,
                         Fade(body, 0.42f * alpha));
    DrawRectangle(x - w * 0.5f, y - 4.0f * t, w, 5.0f * t,
                  Fade(GRASS, 0.48f * alpha * t));
    DrawRectangle(x - w * 0.27f, y + h, w * 0.54f, h * 0.55f * t,
                  Fade(ColorBrightness(GRASS_DK, -0.18f), 0.38f * alpha * t));
  }
}

// Night -> Maze, the Constellation Lock keyframe: free stars first settle
// onto regular signal rows, then each straight constellation bends into an
// orthogonal corridor. The moon contracts into one pulsing node while island
// seams extend into the same line language.
static void drawSignalFar(float cameraY, float t) {
  drawCelestial(SUN_R * (1.0f - 0.72f * t), 1.0f - t, 0.95f * (1.0f - t));

  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float lock = easedWindow(t, 0.08f, 0.72f);
  float route = easedWindow(t, 0.38f, 0.92f);
  Vector2 node[12];
  float off = wrapf(-cameraY * 0.4f, MAZE_CELL);
  for (int i = 0; i < 12; i++) {
    float depth = 0.2f + (i % 3) * 0.12f;
    Vector2 star = {
        wrapf(i * 71.0f, SCREEN_WIDTH),
        wrapf(i * 137.0f - cameraY * depth, SCREEN_HEIGHT + 100.0f)};
    Vector2 grid = {70.0f + (i % 4) * 220.0f,
                    80.0f + (i / 4) * 260.0f + off * 0.35f};
    node[i] = (Vector2){star.x + (grid.x - star.x) * lock,
                        star.y + (grid.y - star.y) * lock};
  }

  BeginBlendMode(BLEND_ADDITIVE);
  Color line = Fade(CLOUD, 0.28f * alpha);
  for (int i = 0; i < 12; i++) {
    drawTex(TEX_PUFF, node[i].x, node[i].y, 24.0f, 24.0f, 0.0f,
            Fade(SUN_CORE, 0.22f * alpha));
    DrawCircleV(node[i], 2.0f + 2.0f * lock, Fade(SUN_CORE, 0.6f * alpha));
    if (i % 4 != 3) {
      Vector2 q = node[i + 1];
      Vector2 mid = {(node[i].x + q.x) * 0.5f, (node[i].y + q.y) * 0.5f};
      Vector2 elbow = {mid.x + (q.x - mid.x) * route,
                       mid.y + (node[i].y - mid.y) * route};
      DrawLineEx(node[i], elbow, 2.0f, line);
      DrawLineEx(elbow, q, 2.0f, line);
    }
  }
  float pulse = 0.65f + 0.35f * sinf((float)GetTime() * 4.0f);
  drawTex(TEX_PUFF, SUN_X, SUN_Y, 150.0f, 150.0f, 0.0f,
          Fade(CLOUD, 0.2f * alpha * pulse));
  DrawRectangleLinesEx(
      (Rectangle){SUN_X - 12.0f - 8.0f * route, SUN_Y - 12.0f - 8.0f * route,
                  24.0f + 16.0f * route, 24.0f + 16.0f * route},
      2.0f, Fade(CLOUD, 0.52f * alpha));
  EndBlendMode();
}

static void drawSignalMid(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float lock = easedWindow(t, 0.16f, 0.86f);
  for (int i = 0; i < 4; i++) {
    float x = 90.0f + i * 215.0f;
    float y = wrapf(i * 211.0f - cameraY * (0.3f + 0.05f * (i % 2)),
                    SCREEN_HEIGHT + 260.0f) -
              80.0f;
    float reach = 30.0f + 90.0f * lock;
    Color seam = Fade(CLOUD, 0.34f * alpha);
    DrawLineEx((Vector2){x - reach, y}, (Vector2){x + reach, y}, 3.0f, seam);
    DrawLineEx((Vector2){x + reach, y},
               (Vector2){x + reach, y + 30.0f + 55.0f * lock}, 3.0f, seam);
  }
}

// Maze -> Blocks: junction lights spread into four cells, close into real
// tetromino groups, and trade corridor scrolling for downward motion.
static void drawFragmentBridge(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  static const signed char SPREAD[4][2] = {{-2, 0}, {0, -2}, {2, 0}, {0, 2}};
  float group = easedWindow(t, 0.08f, 0.78f);
  for (int i = 0; i < 6; i++) {
    int shape = (i * 2 + 1) % TETRO_COUNT;
    float depth = 0.25f + (i % 3) * 0.1f;
    float motionDepth = 0.4f + (depth - 0.4f) * t;
    float speed = 8.0f + 18.0f * t;
    float cx = 85.0f + (i % 3) * 315.0f;
    float cy =
        wrapf(i * 173.0f + (float)GetTime() * speed - cameraY * motionDepth,
              SCREEN_HEIGHT + 220.0f) -
        110.0f;
    float cell = 18.0f;
    float size = 4.0f + 18.0f * group;
    Color c = ColorLerp(COIN_HI, gTheme.confetti[i % CONFETTI_COUNT], group);
    for (int k = 0; k < 4; k++) {
      float sx = SPREAD[k][0] * cell, sy = SPREAD[k][1] * cell;
      float tx = (TETRO[shape][k][0] - 1) * cell;
      float ty = (TETRO[shape][k][1] - 0.5f) * cell;
      float x = cx + sx + (tx - sx) * group;
      float y = cy + sy + (ty - sy) * group;
      drawTex(TEX_PUFF, x, y, 24.0f + 18.0f * t, 24.0f + 18.0f * t, 0.0f,
              Fade(c, 0.18f * alpha));
      DrawRectangleRounded(
          (Rectangle){x - size * 0.5f, y - size * 0.5f, size, size},
          1.0f - 0.9f * group, 5, Fade(c, 0.72f * alpha));
      DrawRectangle(x - size * 0.34f, y - size * 0.34f, size * 0.68f,
                    fmaxf(1.0f, size * 0.12f),
                    Fade(ColorBrightness(c, 0.35f), 0.42f * alpha * t));
    }
  }
}

// Near-camera layers. Unlike the backdrop above, these draw after the world so
// their clipped edge silhouettes can pass in front of it. Keep all opaque ink
// inside the outer ~60px: depth is useful only while gameplay stays readable.
static void drawSkiesForeground(float cameraY, float alpha) {
  for (int i = 0; i < 4; i++) {
    float depth = 0.56f + (i % 2) * 0.1f;
    float s = 48.0f + (i % 2) * 8.0f;
    float x = (i % 2) ? SCREEN_WIDTH + 94.0f : -94.0f;
    float y =
        wrapf(i * 267.0f + (float)GetTime() * (8.0f + i) - cameraY * depth,
              SCREEN_HEIGHT + 360.0f) -
        180.0f;
    drawCloud(x, y, s, CLOUD, 0.3f * alpha);
  }
}

static void drawStarsForeground(float cameraY, float alpha) {
  for (int i = 0; i < 5; i++) {
    float depth = 0.58f + (i % 3) * 0.06f;
    float s = 54.0f + (i % 2) * 10.0f;
    float x = (i % 2) ? SCREEN_WIDTH + s * 0.35f : -s * 0.35f;
    float y =
        wrapf(i * 241.0f - cameraY * depth, SCREEN_HEIGHT + 300.0f) - 150.0f;
    drawVoxelIsland(x, y, s, 0.42f * alpha);
  }
}

static void drawMazeForeground(float cameraY, float alpha) {
  float off = wrapf(-cameraY * 0.72f + (float)GetTime() * 9.0f, 180.0f);
  Color glow = Fade(CLOUD, 0.13f * alpha);
  Color wall = Fade(ColorBrightness(CLOUD, 0.35f), 0.38f * alpha);
  BeginBlendMode(BLEND_ADDITIVE);
  DrawRectangle(0, 0, 9, SCREEN_HEIGHT, glow);
  DrawRectangle(SCREEN_WIDTH - 9, 0, 9, SCREEN_HEIGHT, glow);
  for (int side = 0; side < 2; side++) {
    for (int i = -1; i < 6; i++) {
      float y = i * 180.0f + off;
      Rectangle r = {side ? SCREEN_WIDTH - 52.0f : 0.0f, y, 52.0f, 8.0f};
      DrawRectangleRounded(
          (Rectangle){r.x - 3.0f, r.y - 3.0f, r.width + 6.0f, 14.0f}, 1.0f, 4,
          glow);
      DrawRectangleRounded(r, 1.0f, 4, wall);
      drawTex(TEX_PUFF, side ? SCREEN_WIDTH - 8.0f : 8.0f, y + 4.0f, 42.0f,
              42.0f, 0.0f, glow);
    }
  }
  EndBlendMode();
}

static float tetrominoWidth(int shape, float cell) {
  int maxX = 0;
  for (int i = 0; i < 4; i++) {
    if (TETRO[shape][i][0] > maxX) {
      maxX = TETRO[shape][i][0];
    }
  }
  return (maxX + 1) * cell;
}

static void drawBlocksForeground(float cameraY, float alpha) {
  for (int i = 0; i < 4; i++) {
    int shape = (i * 2 + 1) % TETRO_COUNT;
    float depth = 0.58f + (i % 2) * 0.12f;
    float cell = 30.0f + (i % 2) * 7.0f;
    float width = tetrominoWidth(shape, cell);
    float x = (i % 2) ? SCREEN_WIDTH - 44.0f : 44.0f - width;
    float y = wrapf(i * 257.0f + (float)GetTime() * (22.0f + i * 3.0f) -
                        cameraY * depth,
                    SCREEN_HEIGHT + 280.0f) -
              140.0f;
    Color c = ColorLerp(SKY_TOP_HIGH, gTheme.confetti[(i + 2) % CONFETTI_COUNT],
                        0.55f);
    drawTetromino(x, y, cell, shape, c, 0.38f * alpha);
  }
}

static void drawBackdropFar(BackdropStyle style, float cameraY, float alpha,
                            float sink, int omitDisc) {
  if (alpha <= 0.01f) {
    return;
  }
  switch (style) {
  case BACKDROP_HILLS:
    drawHillsFar(cameraY, alpha, sink, omitDisc);
    break;
  case BACKDROP_SKIES:
    drawSkiesFar(alpha, omitDisc);
    break;
  case BACKDROP_STARS:
    drawStarsFar(cameraY, alpha, omitDisc);
    break;
  case BACKDROP_MAZE:
    drawMazeFar(cameraY, alpha);
    break;
  case BACKDROP_BLOCKS:
    drawBlocksFar(cameraY, alpha);
    break;
  }
}

static void drawBackdropMid(BackdropStyle style, float cameraY, float alpha,
                            float sink) {
  if (alpha <= 0.01f) {
    return;
  }
  switch (style) {
  case BACKDROP_HILLS:
    drawHillsMid(cameraY, alpha, sink);
    break;
  case BACKDROP_SKIES:
    drawSkiesMid(cameraY, alpha);
    break;
  case BACKDROP_STARS:
    drawStarsMid(cameraY, alpha);
    break;
  case BACKDROP_MAZE:
    drawMazeMid(cameraY, alpha);
    break;
  case BACKDROP_BLOCKS:
    drawBlocksMid(cameraY, alpha);
    break;
  }
}

static void drawForeground(BackdropStyle style, float cameraY, float alpha,
                           float sink) {
  if (alpha <= 0.01f) {
    return;
  }
  switch (style) {
  case BACKDROP_HILLS:
    rlPushMatrix();
    rlTranslatef(0.0f, sink, 0.0f);
    drawForegroundFoliage(cameraY, alpha);
    rlPopMatrix();
    break;
  case BACKDROP_SKIES:
    drawSkiesForeground(cameraY, alpha);
    break;
  case BACKDROP_STARS:
    drawStarsForeground(cameraY, alpha);
    break;
  case BACKDROP_MAZE:
    drawMazeForeground(cameraY, alpha);
    break;
  case BACKDROP_BLOCKS:
    drawBlocksForeground(cameraY, alpha);
    break;
  }
}

static void drawBridgeFar(BridgeStyle bridge, float cameraY, float t) {
  if (bridge == BRIDGE_TWILIGHT) {
    drawTwilightDisc(t);
  } else if (bridge == BRIDGE_SIGNAL) {
    drawSignalFar(cameraY, t);
  } else if (bridge == BRIDGE_REBOOT) {
    drawRebootFar(cameraY, t);
  }
}

static void drawBridgeMid(BridgeStyle bridge, float cameraY, float t) {
  if (bridge == BRIDGE_CLOUDLINE) {
    drawCloudlineBridge(cameraY, t);
  } else if (bridge == BRIDGE_TWILIGHT) {
    drawTwilightIslands(cameraY, t);
  } else if (bridge == BRIDGE_SIGNAL) {
    drawSignalMid(cameraY, t);
  } else if (bridge == BRIDGE_FRAGMENTATION) {
    drawFragmentBridge(cameraY, t);
  } else if (bridge == BRIDGE_REBOOT) {
    drawRebootMid(cameraY, t);
  }
}

void drawMenuBackground(void) {
  themeUpdate(themeSampleAt(0.0f)); // menu is always the first theme
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

// Far silhouettes hand off first, then midground decor, then the close edge
// layer. Every pair is complementary, so additive scenes never stack at full
// strength. The cloudline sink applies only to outgoing Hills geometry.
void drawGameBackground(float cameraY) {
  ThemeSample sample = themeSampleAtWorldY(cameraY + SCREEN_HEIGHT / 2.0f);
  ThemeSample top = themeSampleAtWorldY(cameraY);
  ThemeSample bottom = themeSampleAtWorldY(cameraY + SCREEN_HEIGHT);
  themeUpdate(top);
  Color skyTop = SKY_TOP;
  themeUpdate(bottom);
  Color skyBottom = SKY_BOT;
  themeUpdate(sample);

  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, skyTop, skyBottom);

  float far = easedWindow(sample.phase, FAR_START, FAR_END);
  float mid = easedWindow(sample.phase, MID_START, MID_END);
  BridgeStyle bridge = THEMES[sample.era].bridge;
  int omitDisc = bridge == BRIDGE_TWILIGHT || bridge == BRIDGE_SIGNAL ||
                 bridge == BRIDGE_REBOOT;
  float farSink = bridge == BRIDGE_CLOUDLINE ? far * 170.0f : 0.0f;
  float midSink = bridge == BRIDGE_CLOUDLINE ? mid * 230.0f : 0.0f;

  drawBackdropFar(THEMES[sample.era].backdrop, cameraY, 1.0f - far, farSink,
                  omitDisc);
  drawBackdropFar(THEMES[sample.next].backdrop, cameraY, far, 0.0f, omitDisc);
  drawBridgeFar(bridge, cameraY, far);

  drawBackdropMid(THEMES[sample.era].backdrop, cameraY, 1.0f - mid, midSink);
  drawBackdropMid(THEMES[sample.next].backdrop, cameraY, mid, 0.0f);
  drawBridgeMid(bridge, cameraY, mid);
}

void drawGameForeground(float cameraY) {
  ThemeSample sample = themeSampleAtWorldY(cameraY + SCREEN_HEIGHT / 2.0f);
  themeUpdate(sample);
  float near = easedWindow(sample.phase, NEAR_START, 1.0f);
  BridgeStyle bridge = THEMES[sample.era].bridge;
  float sink = bridge == BRIDGE_CLOUDLINE ? near * 170.0f : 0.0f;
  drawForeground(THEMES[sample.era].backdrop, cameraY, 1.0f - near, sink);
  drawForeground(THEMES[sample.next].backdrop, cameraY, near, 0.0f);
}

#ifdef BG_SELFTEST
// Transition contact matrix of every outgoing edge, including Blocks -> Hills.
// Rows follow THEMES[] order. Columns are the phase-zero boundary, then phases
// .70, .78, .84, .90, .96, .995, and finally the next era at phase .005.
// Every cell includes the foreground depth pass. This needs a real GL context
// because it renders the actual draw path.
//   cc -DBG_SELFTEST src/background.c src/theme.c src/tex.c \
//      -Iinclude -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo \
//      -framework OpenGL -o /tmp/bgtest && /tmp/bgtest /tmp/bgstrip.png
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// This mirrors theme.c's private band height only for sampling. If that tuning
// changes, the matrix degrades visibly instead of affecting shipped behavior.
#define BAND 20000.0f
#define CELL 300

static const float SAMPLE_PHASES[] = {0.0f,  0.70f, 0.78f,  0.84f,
                                      0.90f, 0.96f, 0.995f, 1.005f};
#define SAMPLE_COUNT ((int)(sizeof(SAMPLE_PHASES) / sizeof(SAMPLE_PHASES[0])))

static void transitionSelfTest(void) {
  assert(bridgeBell(0.0f) == 0.0f && bridgeBell(0.5f) == 1.0f &&
         bridgeBell(1.0f) == 0.0f);
  float far = easedWindow(0.84f, FAR_START, FAR_END);
  float mid = easedWindow(0.84f, MID_START, MID_END);
  float near = easedWindow(0.84f, NEAR_START, 1.0f);
  assert(far > mid && mid > near);
  far = easedWindow(0.96f, FAR_START, FAR_END);
  mid = easedWindow(0.96f, MID_START, MID_END);
  near = easedWindow(0.96f, NEAR_START, 1.0f);
  assert(far > mid && mid > near);
  assert(easedWindow(1.0f, FAR_START, FAR_END) == 1.0f &&
         easedWindow(1.0f, MID_START, MID_END) == 1.0f &&
         easedWindow(1.0f, NEAR_START, 1.0f) == 1.0f);

  // At a center-view boundary, the screen top has entered the new era while
  // the bottom still carries the old sky. Both remain camera-relative.
  ThemeSample top = themeSampleAtWorldY(-BAND);
  ThemeSample bottom = themeSampleAtWorldY(-BAND + SCREEN_HEIGHT);
  assert(top.level == 1 && bottom.level == 0 && top.era == ERA_SKIES &&
         bottom.era == ERA_HILLS);
}

static Image renderSample(RenderTexture rt, float cameraY) {
  BeginTextureMode(rt);
  ClearBackground(BLACK);
  drawGameBackground(cameraY);
  EndTextureMode();
  Image base = LoadImageFromTexture(rt.texture);

  // Add the near layer to the same target so each cell proves both that the
  // foreground draws and that it remains outside the central playfield.
  BeginTextureMode(rt);
  drawGameForeground(cameraY);
  EndTextureMode();
  Image im = LoadImageFromTexture(rt.texture);
  ImageFlipVertical(&base);
  ImageFlipVertical(&im); // framebuffer textures are bottom-up

  const Color *bp = base.data, *fp = im.data;
  int edgeChanged = 0, centerChanged = 0;
  for (int y = 0; y < im.height; y++) {
    for (int x = 0; x < im.width; x++) {
      int k = y * im.width + x;
      int d = abs(fp[k].r - bp[k].r) + abs(fp[k].g - bp[k].g) +
              abs(fp[k].b - bp[k].b);
      if (d > 6) {
        if (x < 100 || x >= im.width - 100) {
          edgeChanged++;
        } else {
          centerChanged++;
        }
      }
    }
  }
  assert(edgeChanged > 100 && "theme foreground drew no visible edge ink");
  assert(centerChanged == 0 && "theme foreground obscures the playfield");
  UnloadImage(base);

  // Translucent draws leave useful RGB but junk framebuffer alpha. Stomp it
  // before sheet composition or ImageDrawImage darkens the rendered cell.
  Color *p = im.data;
  for (int k = 0, n = im.width * im.height; k < n; k++) {
    p[k].a = 255;
  }
  return im;
}

int main(int argc, char **argv) {
  themeSampleSelfTest();
  transitionSelfTest();
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "bgtest");
  while (!texPreloadStep()) {
  }
  RenderTexture rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
  Image sheet = GenImageColor(CELL * SAMPLE_COUNT, CELL * THEME_COUNT, BLACK);

  printf("columns: edge .000 | stable .700 | .780 | .840 | .900 | .960 | "
         ".995 | next .005\n");
  for (int edge = 0; edge < THEME_COUNT; edge++) {
    int next = themeSampleAt(edge * BAND).next;
    for (int col = 0; col < SAMPLE_COUNT; col++) {
      float phase = SAMPLE_PHASES[col];
      float cameraY = -(edge + phase) * BAND;
      Image im = renderSample(rt, cameraY);

      // Weighted luma plus its range gives a compact grayscale readability
      // report. Keep the existing flat-frame and blown-frame failure gates.
      const Color *p = im.data;
      long sum = 0;
      int lo = 255, hi = 0;
      for (int k = 0, n = im.width * im.height; k < n; k++) {
        int l = (54 * p[k].r + 183 * p[k].g + 19 * p[k].b) / 256;
        sum += l;
        lo = (l < lo) ? l : lo;
        hi = (l > hi) ? l : hi;
      }
      int mean = (int)(sum / (im.width * im.height));
      printf("edge %d->%d phase %5.3f: luma %3d..%3d mean %3d contrast %3d\n",
             edge, next, phase > 1.0f ? phase - 1.0f : phase, lo, hi, mean,
             hi - lo);
      assert(hi - lo > 32 && "backdrop came out flat");
      assert(mean > 8 && mean < 247 && "backdrop blown to black or white");

      ImageResize(&im, CELL, CELL);
      ImageDrawImage(&sheet, im, col * CELL, edge * CELL, WHITE);
      UnloadImage(im);
    }
  }
  ExportImage(sheet, argc > 1 ? argv[1] : "bgstrip.png");
  printf("ok - %d edges x %d samples\n", THEME_COUNT, SAMPLE_COUNT);
  CloseWindow();
  return 0;
}
#endif
