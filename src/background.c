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
  return smooth01((phase - start) / (end - start));
}

static float bridgeBell(float t) { return 4.0f * t * (1.0f - t); }

// Parallax depth for decor element `i`: three quantized layers (near layers
// drift and scroll more). Clouds and blocks share it so their motion lines up
// during a bridge.
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

// Sky haze for the dark backdrops: one TEX_NEBULA quad wrapped
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
// beveled cells so the Falling Blocks backdrop rains shapes, not lone squares.
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
STATIC_ASSERT(sizeof(RIVER) / sizeof(RIVER[0]) == RIVER_POINTS,
              river_points_full);

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

// Sunny Hills far layer: day atmosphere, cloud depth, and valley silhouettes.
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

// Sunny Hills mid layer: patterned fields, river, trees, cottage, and pollen.
// It can sink as one plane during the cloudline bridge without moving the sky.
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

// Birdsong Skies far layer: clear air and the shared daylight disc.
static void drawSkiesFar(float alpha, int omitDisc) {
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.35f,
          SCREEN_WIDTH * 1.3f, SCREEN_HEIGHT * 0.8f, 0.0f,
          Fade(SUN_CORE, 0.1f * alpha));
  if (!omitDisc) {
    drawSun(0.9f * alpha);
  }
}

// Birdsong Skies mid layer: high clouds, wind ribbons, and migrating birds.
// There is no ground plane, so the climb feels clear of Sunny Hills.
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

// Night Blocks far layer: night haze, stars, moon, and shooting stars.
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

// Night Blocks mid layer: floating square islands beneath the star field.
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

// An original signal kite for the Maze distance layer: nested circuit shapes,
// a bright core, and split antenna tails. It echoes the lattice without
// borrowing a recognizable arcade character silhouette.
static void drawSignalKite(float x, float y, float s, Color c, float a) {
  Color dark = Fade(ColorBrightness(c, -0.5f), a);
  DrawPoly((Vector2){x, y}, 6, s, 30.0f, dark);
  DrawPoly((Vector2){x, y}, 4, s * 0.68f, 45.0f, Fade(c, a));
  DrawRing((Vector2){x, y}, s * 0.18f, s * 0.32f, 0.0f, 360.0f, 18,
           Fade(RAYWHITE, 0.82f * a));
  for (int side = -1; side <= 1; side += 2) {
    Vector2 tip = {x + side * s * 0.62f, y + s * 1.45f};
    DrawLineEx((Vector2){x + side * s * 0.2f, y + s * 0.58f}, tip, 2.0f, dark);
    DrawCircleV(tip, s * 0.12f, Fade(COIN_HI, a));
  }
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

// Neon Maze far layer: scrolling broken corridors and signal pellets.
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

// Neon Maze mid layer: signal fauna moving between the corridors.
static void drawMazeMid(float cameraY, float alpha) {
  for (int i = 0; i < 4; i++) { // drifting signal kites
    float x = wrapf((float)GetTime() * (14.0f + i * 4) + i * 220.0f,
                    SCREEN_WIDTH + 160.0f) -
              80.0f;
    float depth = 0.3f + 0.1f * i;
    float y =
        wrapf(i * 260.0f - cameraY * depth, SCREEN_HEIGHT + 240.0f) - 60.0f;
    drawSignalKite(x, y, 18.0f, gTheme.confetti[i % CONFETTI_COUNT],
                   0.7f * alpha);
  }
  { // a bright pulse courier crossing one parallax band
    float x = wrapf((float)GetTime() * 40.0f, SCREEN_WIDTH + 120.0f) - 60.0f;
    float y = wrapf(80.0f - cameraY * 0.35f, SCREEN_HEIGHT + 200.0f) - 60.0f;
    float pulse = 1.0f + 0.12f * sinf((float)GetTime() * 6.0f);
    DrawLineEx((Vector2){x - 42.0f, y}, (Vector2){x - 15.0f, y}, 3.0f,
               Fade(COIN_HI, 0.28f * alpha));
    for (int i = 1; i <= 3; i++) {
      DrawCircle(x - 12.0f * i, y, 4.5f - i,
                 Fade(COIN_HI, (0.58f - i * 0.1f) * alpha));
    }
    DrawPoly((Vector2){x, y}, 4, 18.0f * pulse, 45.0f,
             Fade(COIN_HI, 0.85f * alpha));
    DrawCircle(x, y, 5.0f * pulse, Fade(RAYWHITE, 0.9f * alpha));
  }
}

// Falling Blocks far layer: faint vertical lines of a playfield well.
static void drawBlocksFar(float cameraY, float alpha) {
  drawNebula(cameraY, alpha);
  for (int i = 0; i * 44 <= SCREEN_WIDTH; i++) {
    DrawRectangle(i * 44, 0, 1, SCREEN_HEIGHT, Fade(CLOUD, 0.04f * alpha));
  }
}

// Falling Blocks mid layer: muted shapes behind brighter gameplay pieces.
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

// Magma Forge far layer: a smoky caldera lit from below. Broad volcanic
// silhouettes retain Blocks' angular language while the molten crater begins
// the warm, mineral-rich path toward the Garden.
static void drawMagmaFar(float cameraY, float alpha) {
  drawNebula(cameraY, 0.9f * alpha);
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, SCREEN_WIDTH * 0.5f, 485.0f, 520.0f, 250.0f, 0.0f,
          Fade(SUN, 0.24f * alpha));
  drawTex(TEX_PUFF, SCREEN_WIDTH * 0.5f, 505.0f, 260.0f, 105.0f, 0.0f,
          Fade(SUN_CORE, 0.22f * alpha));
  EndBlendMode();

  Color far =
      ColorBrightness(ColorLerp(SKY_TOP_HIGH, CLOUD_SHADE, 0.62f), -0.18f);
  Color near = ColorBrightness(ColorLerp(CLOUD, CLOUD_SHADE, 0.72f), -0.12f);
  drawLandscapeBand(500.0f, 118.0f, 74.0f, far, 0.96f * alpha, 3.5f, 0.95f);
  drawLandscapeBand(560.0f, 92.0f, 305.0f, near, 0.94f * alpha, 4.0f, 0.9f);
  DrawEllipse(SCREEN_WIDTH * 0.5f, 518.0f, 92.0f, 17.0f,
              Fade(ColorBrightness(CLOUD_SHADE, -0.4f), alpha));
  DrawEllipse(SCREEN_WIDTH * 0.5f, 514.0f, 74.0f, 9.0f,
              Fade(ColorLerp(COIN_LO, SUN_CORE, 0.42f), 0.82f * alpha));

  for (int i = 0; i < 6; i++) {
    float depth = 0.08f + (i % 3) * 0.05f;
    float x = wrapf(i * 181.0f + (float)GetTime() * depth * 7.0f,
                    SCREEN_WIDTH + 300.0f) -
              150.0f;
    float y =
        wrapf(80.0f + i * 113.0f - cameraY * depth, SCREEN_HEIGHT + 360.0f) -
        180.0f;
    drawCloud(x, y, 20.0f + (i % 3) * 7.0f, CLOUD,
              (0.2f + (i % 2) * 0.05f) * alpha);
  }
}

// Midground lava forks through black rock while embers rise against gravity.
// The center remains sparse enough that charged gameplay vents read first.
static void drawMagmaMid(float cameraY, float alpha) {
  static const Vector2 FLOW[][5] = {
      {{100, 850}, {145, 735}, {120, 650}, {185, 575}, {160, 505}},
      {{410, 850}, {360, 748}, {430, 660}, {385, 585}, {420, 512}},
      {{735, 850}, {675, 750}, {710, 665}, {645, 585}, {675, 520}},
  };
  for (int f = 0; f < 3; f++) {
    Vector2 river[5];
    float sway = sinf((float)GetTime() * 0.45f + f * 1.7f) * 6.0f;
    for (int i = 0; i < 5; i++) {
      river[i] = FLOW[f][i];
      river[i].x += sway * (float)i / 4.0f;
    }
    DrawSplineCatmullRom(river, 5, 18.0f,
                         Fade(ColorBrightness(COIN_LO, -0.28f), 0.7f * alpha));
    DrawSplineCatmullRom(river, 5, 5.0f, Fade(SUN_CORE, 0.7f * alpha));
  }

  for (int i = 0; i < 22; i++) {
    float depth = 0.18f + (i % 3) * 0.04f;
    float x = wrapf(31.0f + i * 83.0f + sinf(i * 2.1f) * 23.0f,
                    SCREEN_WIDTH + 40.0f) -
              20.0f;
    float y = wrapf(i * 67.0f - (float)GetTime() * (18.0f + i % 4 * 4.0f) -
                        cameraY * depth,
                    SCREEN_HEIGHT + 100.0f) -
              50.0f;
    float r = 1.2f + (i % 3) * 0.65f;
    DrawLineEx((Vector2){x, y + 7.0f}, (Vector2){x, y}, 1.2f,
               Fade(COIN_LO, 0.24f * alpha));
    DrawCircle(x, y, r, Fade(i % 4 ? FLAME : SUN_CORE, 0.58f * alpha));
  }
}

static void drawGardenSprout(float x, float y, float h, float alpha) {
  float sway = sinf((float)GetTime() * 1.8f + x * 0.03f) * h * 0.08f;
  Color stem = Fade(GRASS_DK, alpha);
  DrawLineEx((Vector2){x, y}, (Vector2){x + sway, y - h}, 2.2f, stem);
  DrawCircle(x + sway - h * 0.13f, y - h * 0.62f, h * 0.12f,
             Fade(GRASS, alpha));
  DrawCircle(x + sway + h * 0.13f, y - h * 0.78f, h * 0.11f,
             Fade(ColorBrightness(GRASS, 0.18f), alpha));
  DrawCircle(x + sway, y - h, h * 0.1f, Fade(COIN_HI, 0.8f * alpha));
}

// One outlined root shared by the underground backdrop and its two bridges.
// The pale center makes overlapping branches readable against dark loam.
static void drawRootPath(const Vector2 *path, int count, float width,
                         float alpha) {
  DrawSplineCatmullRom(
      path, count, width + 4.0f,
      Fade(ColorBrightness(CLOUD_SHADE, -0.3f), 0.68f * alpha));
  DrawSplineCatmullRom(path, count, width, Fade(CLOUD, 0.8f * alpha));
  DrawSplineCatmullRom(path, count, fmaxf(1.0f, width * 0.16f),
                       Fade(SUN_CORE, 0.18f * alpha));
}

static void drawRootStrand(float x, float y, float length, float bend,
                           float width, float alpha) {
  Vector2 path[6];
  for (int i = 0; i < 6; i++) {
    float f = i / 5.0f;
    float curl = sinf(x * 0.017f + i * 1.37f) * length * 0.045f;
    path[i] = (Vector2){x + bend * f + curl * sinf(f * PI), y + length * f};
  }
  drawRootPath(path, 6, width, alpha);
  for (int i = 2; i <= 4; i += 2) {
    float side = ((int)x + i) & 1 ? -1.0f : 1.0f;
    Vector2 fork = path[i], tip = {fork.x + side * (20.0f + i * 4.0f),
                                   fork.y + 24.0f + i * 3.0f};
    DrawLineEx(fork, tip, fmaxf(1.0f, width * 0.38f),
               Fade(CLOUD, 0.62f * alpha));
    DrawCircleV(tip, fmaxf(1.4f, width * 0.26f), Fade(SUN_CORE, 0.28f * alpha));
  }
}

// Reboot Garden is below the surface. Slow soil strata and distant taproots
// replace the old sky and hills while keeping the center low-contrast.
static void drawGardenFar(float cameraY, float alpha) {
  drawTex(TEX_ROCK, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, SCREEN_WIDTH,
          SCREEN_HEIGHT, 0.0f,
          Fade(ColorBrightness(CLOUD_SHADE, -0.12f), 0.14f * alpha));
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.42f,
          SCREEN_WIDTH * 1.25f, SCREEN_HEIGHT, 0.0f, Fade(CLOUD, 0.1f * alpha));

  for (int i = 0; i < 6; i++) {
    float depth = 0.07f + i * 0.018f;
    float y =
        wrapf(i * 173.0f - cameraY * depth, SCREEN_HEIGHT + 210.0f) - 105.0f;
    Vector2 seam[7];
    for (int k = 0; k < 7; k++) {
      float x = -70.0f + k * 157.0f;
      seam[k] = (Vector2){x, y + sinf(k * 1.31f + i * 0.8f) * 17.0f};
    }
    DrawSplineCatmullRom(seam, 7, 12.0f + (i % 2) * 5.0f,
                         Fade(SKY_TOP_HIGH, 0.16f * alpha));
    DrawSplineCatmullRom(seam, 7, 2.0f, Fade(CLOUD, 0.14f * alpha));
  }

  for (int i = 0; i < 7; i++) {
    float y = wrapf(i * 227.0f - cameraY * (0.1f + (i % 3) * 0.02f),
                    SCREEN_HEIGHT + 620.0f) -
              510.0f;
    drawRootStrand(42.0f + i * 121.0f, y, 440.0f + (i % 3) * 55.0f,
                   (i & 1 ? -1.0f : 1.0f) * (38.0f + i * 3.0f), 3.0f + (i % 3),
                   0.34f * alpha);
  }

  for (int i = 0; i < 18; i++) {
    float x = wrapf(31.0f + i * 89.0f, SCREEN_WIDTH + 60.0f) - 30.0f;
    float y =
        wrapf(i * 67.0f - cameraY * 0.14f, SCREEN_HEIGHT + 100.0f) - 50.0f;
    DrawCircle(x, y, 2.0f + (i % 3),
               Fade(i % 4 ? CLOUD_SHADE : SUN, 0.2f * alpha));
  }
}

// Midground roots form a living transport network. Amber nodules and the
// preserved sprouts are sparse landmarks, not another layer of hazards.
static void drawGardenMid(float cameraY, float alpha) {
  for (int i = 0; i < 5; i++) {
    float y = wrapf(i * 251.0f - cameraY * (0.22f + i * 0.025f),
                    SCREEN_HEIGHT + 520.0f) -
              390.0f;
    float x = 70.0f + i * 165.0f;
    float length = 300.0f + (i % 2) * 70.0f;
    float bend = (i & 1 ? -1.0f : 1.0f) * (48.0f + i * 4.0f);
    drawRootStrand(x, y, length, bend, 7.0f + (i % 3) * 1.8f, 0.62f * alpha);
    Vector2 tip = {x + bend, y + length};
    DrawCircleV(tip, 8.0f + (i % 2) * 3.0f,
                Fade(ColorBrightness(CLOUD, 0.12f), 0.52f * alpha));
    DrawCircleV(tip, 3.0f, Fade(SUN_CORE, 0.46f * alpha));
    if ((i & 1) == 0) {
      DrawEllipse(tip.x, tip.y + 3.0f, 24.0f, 8.0f,
                  Fade(CLOUD_SHADE, 0.56f * alpha));
      drawGardenSprout(tip.x, tip.y, 18.0f + i * 2.0f, 0.72f * alpha);
    }
  }

  for (int i = 0; i < 4; i++) {
    float y =
        wrapf(130.0f + i * 239.0f - cameraY * 0.34f, SCREEN_HEIGHT + 220.0f) -
        110.0f;
    Vector2 lateral[] = {{-45.0f, y + 18.0f},
                         {165.0f, y - 22.0f},
                         {390.0f, y + 14.0f},
                         {635.0f, y - 18.0f},
                         {845.0f, y + 10.0f}};
    drawRootPath(lateral, 5, 4.0f + (i % 2) * 2.0f, 0.28f * alpha);
  }
}

// Blocks -> Magma: rigid grid lines sag into glowing drips while the falling
// cells soften, redden, and collect along the forge's molten horizon.
static void drawMeltdownFar(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float sag = easedWindow(t, 0.08f, 0.88f);
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, SCREEN_WIDTH * 0.5f, 505.0f, 580.0f, 190.0f, 0.0f,
          Fade(SUN, 0.16f * alpha));
  EndBlendMode();
  for (int i = 0; i < 10; i++) {
    float x = 38.0f + i * 81.0f;
    float depth = 0.12f + (i % 3) * 0.04f;
    float drift = sinf(i * 1.9f - cameraY * depth * 0.003f) * 18.0f * sag;
    Vector2 drip[] = {{x, -20.0f},
                      {x, 145.0f + i % 2 * 35.0f},
                      {x + drift * 0.45f, 315.0f + i % 3 * 24.0f},
                      {x + drift, 475.0f + i % 2 * 31.0f}};
    Color c = ColorLerp(CLOUD, COIN_LO, sag);
    DrawSplineCatmullRom(drip, 4, 1.0f + 3.0f * sag,
                         Fade(c, (0.16f + 0.3f * sag) * alpha));
    DrawCircleV(drip[3], 2.0f + 5.0f * sag,
                Fade(ColorLerp(c, SUN_CORE, 0.35f), 0.62f * alpha));
  }
}

static void drawMeltdownMid(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float melt = easedWindow(t, 0.12f, 0.9f);
  for (int i = 0; i < 12; i++) {
    float depth = 0.25f + (i % 3) * 0.08f;
    float x =
        wrapf(i * 137.0f + (i % 2) * 41.0f, SCREEN_WIDTH + 100.0f) - 50.0f;
    float falling =
        wrapf(i * 109.0f + (float)GetTime() * 18.0f - cameraY * depth,
              SCREEN_HEIGHT + 180.0f) -
        90.0f;
    float pool = 540.0f + (i % 4) * 67.0f;
    float y = falling + (pool - falling) * melt;
    float size = 16.0f - 7.0f * melt;
    Color c = ColorLerp(gTheme.confetti[i % CONFETTI_COUNT], FLAME, melt);
    DrawRectangleRounded((Rectangle){x - size * 0.5f, y - size * 0.5f, size,
                                     size + 10.0f * melt},
                         0.08f + 0.92f * melt, 6, Fade(c, 0.68f * alpha));
    if (melt > 0.28f) {
      DrawCircle(x + sinf(i * 2.3f) * 8.0f * melt, y + size + 8.0f * melt,
                 2.0f + 3.0f * melt, Fade(SUN_CORE, alpha * melt));
    }
  }
}

// Magma -> Garden: the forge does not become another sunrise. Its lava veins
// cool into pale taproots while dark soil closes around the old caldera.
static void drawBurrowFar(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float root = easedWindow(t, 0.1f, 0.9f);
  float ceiling = 24.0f + 120.0f * root;
  DrawRectangle(0, 0, SCREEN_WIDTH, ceiling,
                Fade(ColorLerp(SKY_TOP_HIGH, CLOUD_SHADE, root), 0.3f * alpha));
  for (int i = 0; i < 13; i++) {
    float x = -20.0f + i * 72.0f;
    float y = ceiling + sinf(i * 1.53f - cameraY * 0.001f) * 18.0f;
    DrawCircle(
        x, y, 24.0f + (i % 3) * 8.0f,
        Fade(ColorLerp(CLOUD_SHADE, SKY_TOP_HIGH, 0.35f), 0.38f * alpha));
  }

  for (int i = 0; i < 9; i++) {
    float x = 35.0f + i * 93.0f;
    float drift = sinf(i * 1.9f - cameraY * 0.002f) * 28.0f;
    Vector2 vein[] = {{x, 495.0f},
                      {x + drift * 0.25f, 390.0f},
                      {x - drift * 0.35f, 275.0f},
                      {x + drift, ceiling - 8.0f}};
    Color c = ColorLerp(COIN_LO, CLOUD, root);
    DrawSplineCatmullRom(vein, 4, 3.0f + 4.0f * root,
                         Fade(ColorBrightness(c, -0.38f), 0.56f * alpha));
    DrawSplineCatmullRom(vein, 4, 1.2f + 2.3f * root, Fade(c, 0.75f * alpha));
    DrawCircleV(vein[3], 2.0f + 4.0f * root,
                Fade(ColorLerp(SUN_CORE, CLOUD, root), 0.62f * alpha));
  }
}

// Molten rivers split into a branching root lattice. Embers lose their flame,
// settle into amber nodules, and wake the first underground sprouts.
static void drawBurrowMid(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float root = easedWindow(t, 0.18f, 0.9f);
  for (int i = 0; i < 3; i++) {
    float x = 105.0f + i * 295.0f;
    float sway = sinf(i * 2.1f - cameraY * 0.001f) * 35.0f;
    Vector2 path[] = {{x, 850.0f},
                      {x + sway, 690.0f},
                      {x - 55.0f + sway * 0.3f, 510.0f},
                      {x + 35.0f, 335.0f},
                      {x - 18.0f, 165.0f}};
    Color c = ColorLerp(FLAME, CLOUD, root);
    DrawSplineCatmullRom(path, 5, 17.0f - 9.0f * root,
                         Fade(ColorBrightness(c, -0.35f), 0.52f * alpha));
    DrawSplineCatmullRom(path, 5, 5.0f + 2.0f * root, Fade(c, 0.76f * alpha));
    if (root > 0.25f) {
      for (int k = 1; k < 4; k++) {
        float side = ((i + k) & 1) ? -1.0f : 1.0f;
        Vector2 tip = {path[k].x + side * (35.0f + 15.0f * root),
                       path[k].y + 28.0f};
        DrawLineEx(path[k], tip, 1.0f + 3.0f * root,
                   Fade(CLOUD, 0.58f * alpha * root));
      }
    }
  }

  for (int i = 0; i < 15; i++) {
    float x = wrapf(28.0f + i * 91.0f, SCREEN_WIDTH + 50.0f) - 25.0f;
    float emberY = wrapf(i * 71.0f - (float)GetTime() * (18.0f + i % 3 * 4.0f) -
                             cameraY * 0.18f,
                         SCREEN_HEIGHT + 100.0f) -
                   50.0f;
    float bedY = 170.0f + (i % 5) * 145.0f;
    float y = emberY + (bedY - emberY) * root;
    Color c = ColorLerp(i % 3 ? FLAME : SUN_CORE, COIN_HI, root);
    DrawCircle(x, y, 1.5f + 2.5f * root, Fade(c, 0.62f * alpha));
    if (root > 0.62f && (i % 4) == 0) {
      drawGardenSprout(x, y, 6.0f + 12.0f * root,
                       alpha * (root - 0.62f) / 0.38f);
    }
  }
}

// Garden -> Hills: the root vault parts around a growing shaft of daylight.
// At the exact wrap the custom disc matches the ordinary Hills sun.
static void drawEmergenceFar(float t) {
  float open = easedWindow(t, 0.08f, 0.96f);
  if (open > 0.01f) {
    drawCelestial(8.0f + (SUN_R - 8.0f) * open, 0.0f, open);
  }
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  for (int side = -1; side <= 1; side += 2) {
    float pull = side * 175.0f * open;
    Vector2 vault[] = {
        {SUN_X + side * 18.0f + pull, -35.0f},
        {SUN_X + side * 58.0f + pull, 92.0f},
        {SUN_X + side * 112.0f + pull, 210.0f},
        {SUN_X + side * 205.0f + pull, 315.0f},
    };
    drawRootPath(vault, 4, 13.0f - 6.0f * open, 0.68f * alpha);
  }
}

// Thick roots become trunks, packed soil drops below the new horizon, and the
// preserved sprouts open into the meadow's saplings and pollen.
static void drawEmergenceMid(float cameraY, float t) {
  float alpha = bridgeBell(t);
  if (alpha <= 0.01f) {
    return;
  }
  float grow = easedWindow(t, 0.08f, 0.88f);
  for (int i = 0; i < 7; i++) {
    float x = 24.0f + i * 126.0f;
    float ground = ridgeY(x, 525.0f, 48.0f, 40.0f, 0.0f) + 8.0f;
    Vector2 root[] = {{x - 13.0f, 820.0f + 80.0f * grow},
                      {x + 18.0f, 710.0f + 54.0f * grow},
                      {x - 8.0f, 620.0f + 30.0f * grow},
                      {x, ground}};
    drawRootPath(root, 4, 8.0f - 3.5f * grow, 0.54f * alpha);
    float clod = 30.0f * (1.0f - grow);
    DrawEllipse(x, ground + clod * 0.35f, clod, clod * 0.42f,
                Fade(CLOUD_SHADE, 0.52f * alpha));
    drawGardenSprout(x, ground, 16.0f + 44.0f * grow, 0.78f * alpha);
  }
  drawPollen(cameraY, alpha * grow);
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

static void drawMagmaForeground(float cameraY, float alpha) {
  for (int side = 0; side < 2; side++) {
    float edge = side ? SCREEN_WIDTH + 16.0f : -16.0f;
    float inward = side ? -1.0f : 1.0f;
    for (int i = 0; i < 4; i++) {
      float y = wrapf(i * 229.0f - cameraY * (0.6f + i * 0.035f),
                      SCREEN_HEIGHT + 240.0f) -
                120.0f;
      float r = 42.0f + (i % 2) * 12.0f;
      Color rock = ColorLerp(CLOUD_SHADE, SKY_TOP_HIGH, 0.35f);
      DrawPoly((Vector2){edge, y}, 7, r, side ? 18.0f : -18.0f,
               Fade(rock, 0.68f * alpha));
      Vector2 a = {edge + inward * r * 0.18f, y - r * 0.55f};
      Vector2 b = {edge + inward * r * 0.58f, y - r * 0.08f};
      Vector2 c = {edge + inward * r * 0.28f, y + r * 0.5f};
      DrawLineEx(a, b, 3.0f, Fade(COIN_LO, 0.42f * alpha));
      DrawLineEx(b, c, 2.2f, Fade(SUN_CORE, 0.3f * alpha));
      DrawCircle(b.x, b.y, 3.0f, Fade(FLAME, 0.46f * alpha));
    }
  }
}

static void drawGardenForeground(float cameraY, float alpha) {
  for (int side = 0; side < 2; side++) {
    float x = side ? SCREEN_WIDTH + 18.0f : -18.0f;
    float inward = side ? -1.0f : 1.0f;
    for (int i = 0; i < 3; i++) {
      float y = wrapf(i * 307.0f - cameraY * (0.58f + i * 0.04f),
                      SCREEN_HEIGHT + 390.0f) -
                250.0f;
      drawRootStrand(x, y, 310.0f + i * 34.0f, inward * (34.0f + i * 5.0f),
                     13.0f + i * 2.0f, 0.68f * alpha);
      float pocketY = y + 195.0f + i * 18.0f;
      DrawCircle(x + inward * 9.0f, pocketY, 37.0f + i * 5.0f,
                 Fade(CLOUD_SHADE, 0.46f * alpha));
      drawGardenSprout(x + inward * 34.0f, pocketY - 22.0f, 35.0f + i * 8.0f,
                       0.7f * alpha);
    }
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
  case BACKDROP_MAGMA:
    drawMagmaFar(cameraY, alpha);
    break;
  case BACKDROP_GARDEN:
    drawGardenFar(cameraY, alpha);
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
  case BACKDROP_MAGMA:
    drawMagmaMid(cameraY, alpha);
    break;
  case BACKDROP_GARDEN:
    drawGardenMid(cameraY, alpha);
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
  case BACKDROP_MAGMA:
    drawMagmaForeground(cameraY, alpha);
    break;
  case BACKDROP_GARDEN:
    drawGardenForeground(cameraY, alpha);
    break;
  }
}

static void drawBridgeFar(BridgeStyle bridge, float cameraY, float t) {
  if (bridge == BRIDGE_TWILIGHT) {
    drawTwilightDisc(t);
  } else if (bridge == BRIDGE_SIGNAL) {
    drawSignalFar(cameraY, t);
  } else if (bridge == BRIDGE_MELTDOWN) {
    drawMeltdownFar(cameraY, t);
  } else if (bridge == BRIDGE_BURROW) {
    drawBurrowFar(cameraY, t);
  } else if (bridge == BRIDGE_EMERGENCE) {
    drawEmergenceFar(t);
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
  } else if (bridge == BRIDGE_MELTDOWN) {
    drawMeltdownMid(cameraY, t);
  } else if (bridge == BRIDGE_BURROW) {
    drawBurrowMid(cameraY, t);
  } else if (bridge == BRIDGE_EMERGENCE) {
    drawEmergenceMid(cameraY, t);
  }
}

void drawMenuBackground(void) {
  themeUpdate(themeSampleAt(0.0f)); // the menu always uses Sunny Hills
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
                 bridge == BRIDGE_MELTDOWN || bridge == BRIDGE_BURROW ||
                 bridge == BRIDGE_EMERGENCE;
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
// Transition contact matrix of every outgoing edge, including Blocks -> Magma,
// Magma -> Garden, and the Garden -> Hills wrap. Rows follow THEMES[] order.
// Columns sample .000, .700, .780, .840, .900, .960, .995, and finally
// the next era at phase .005.
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
  Image sheet = GenImageColor(CELL * SAMPLE_COUNT, CELL * ERA_COUNT, BLACK);

  printf("columns: edge .000 | stable .700 | .780 | .840 | .900 | .960 | "
         ".995 | next .005\n");
  for (int edge = 0; edge < ERA_COUNT; edge++) {
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
  printf("ok - %d edges x %d samples\n", ERA_COUNT, SAMPLE_COUNT);
  CloseWindow();
  return 0;
}
#endif
