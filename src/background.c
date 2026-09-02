#include "background.h"

#include "core.h"
#include "raylib.h"
#include "tex.h"
#include "theme.h"

#include <math.h>

#define BLOCK_COUNT 20
#define STAR_COUNT 44
#define MAZE_CELL 110.0f
#define SUN_X (SCREEN_WIDTH * 0.78f)
#define SUN_Y (SCREEN_HEIGHT * 0.20f)
#define SUN_R 46.0f

static float wrapf(float value, float range) {
  float r = fmodf(value, range);
  return r < 0.0f ? r + range : r;
}

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
// on the sky rather than paint laid over it.
static void drawSun(float alpha) {
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, SUN_X, SUN_Y, SUN_R * 8.0f, SUN_R * 8.0f, 0.0f,
          Fade(SUN, 0.32f * alpha));
  EndBlendMode();
  DrawCircle(SUN_X, SUN_Y, SUN_R, Fade(SUN, alpha));
  DrawCircle(SUN_X, SUN_Y, SUN_R * 0.6f, Fade(SUN_CORE, alpha));
}

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

static void drawRiver(float alpha) {
  Vector2 river[] = {{450, 474}, {425, 520}, {475, 575}, {430, 640},
                     {505, 710}, {450, 770}, {520, 850}};
  Color water = ColorLerp(SKY_TOP, SKY_BOT, 0.5f);
  DrawSplineCatmullRom(river, 7, 24.0f, Fade(water, 0.62f * alpha));
  DrawSplineCatmullRom(river, 7, 4.0f, Fade(SUN_CORE, 0.58f * alpha));
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
  drawMeadowSprig(50.0f, 840.0f, 1.0f, 0.82f - sway, 0.8f * alpha);
  drawMeadowSprig(808.0f, 830.0f, -1.0f, 1.18f - sway, alpha);
  drawMeadowSprig(755.0f, 845.0f, -1.0f, 0.78f + sway, 0.8f * alpha);
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

// L1 backdrop: a storybook valley with atmospheric mountain planes, patterned
// meadow fields, a winding river, textured trees, and close foliage parallax.
static void drawHillsBackdrop(float cameraY, float alpha) {
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.32f,
          SCREEN_WIDTH * 1.35f, SCREEN_HEIGHT * 0.75f, 0.0f,
          Fade(SUN_CORE, 0.09f * alpha));
  drawSun(alpha);

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
  drawLandscapeBand(455.0f, 110.0f, 90.0f, farMountain, 0.92f * alpha, 3.0f,
                    0.88f);
  drawLandscapeBand(510.0f, 105.0f, 310.0f, nearMountain, 0.9f * alpha, 4.0f,
                    0.78f);
  drawLandscapeBand(525.0f, 48.0f, 40.0f, ColorBrightness(GRASS, 0.04f),
                    0.98f * alpha, 5.0f, 0.0f);

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
  drawForegroundFoliage(cameraY, alpha);
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

// L2 backdrop: open azure air, layered high cloud banks, wind ribbons and
// migrating birds. There is deliberately no ground plane, so the climb feels
// clear of Sunny Hills before Night Blocks closes in.
static void drawSkiesBackdrop(float cameraY, float alpha) {
  drawTex(TEX_NEBULA, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.35f,
          SCREEN_WIDTH * 1.3f, SCREEN_HEIGHT * 0.8f, 0.0f,
          Fade(SUN_CORE, 0.1f * alpha));
  drawSun(0.9f * alpha);

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

// The moon: the theme's glowing disc plus a few darker craters.
static void drawMoon(float alpha) {
  drawSun(alpha); // theme's `sun`/`sunCore` are a pale moon at night
  Color cr = Fade(ColorBrightness(SUN_CORE, -0.18f), alpha);
  DrawCircle(SUN_X - SUN_R * 0.3f, SUN_Y - SUN_R * 0.2f, SUN_R * 0.18f, cr);
  DrawCircle(SUN_X + SUN_R * 0.25f, SUN_Y + SUN_R * 0.15f, SUN_R * 0.13f, cr);
  DrawCircle(SUN_X + SUN_R * 0.05f, SUN_Y - SUN_R * 0.35f, SUN_R * 0.1f, cr);
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

// L3 backdrop: moon + craters, twinkling stars (some sparkling), a periodic
// shooting star, and floating voxel islands.
static void drawStarsBackdrop(float cameraY, float alpha) {
  drawNebula(cameraY, alpha);
  drawStarfield(cameraY, alpha);
  drawMoon(alpha * 0.95f);
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
  int h = i * 92837111 + j * 689287499;
  h ^= h >> 13;
  h *= 1274126177;
  return h & 0x7fffffff;
}

// L4 backdrop: a scrolling neon maze (broken corridors, not a full grid) with
// pellets + pulsing power-pellets, drifting ghosts, and a chomping hero.
static void drawMazeBackdrop(float cameraY, float alpha) {
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

// L5 backdrop: a faint playfield "well" grid with real tetrominoes raining in
// the theme's tetromino colors (parallax fall, same idiom as the clouds).
static void drawBlocksBackdrop(float cameraY, float alpha) {
  drawNebula(cameraY, alpha);
  for (int i = 0; i * 44 <= SCREEN_WIDTH; i++) {
    DrawRectangle(i * 44, 0, 1, SCREEN_HEIGHT, Fade(CLOUD, 0.05f * alpha));
  }
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
    drawTetromino(x, y, cell, i % TETRO_COUNT,
                  gTheme.confetti[i % CONFETTI_COUNT],
                  (0.8f - depth * 0.25f) * alpha);
  }
}

static void drawBackdrop(BackdropStyle style, float cameraY, float alpha) {
  if (alpha <= 0.01f) {
    return;
  }
  switch (style) {
  case BACKDROP_SKIES:
    drawSkiesBackdrop(cameraY, alpha);
    break;
  case BACKDROP_STARS:
    drawStarsBackdrop(cameraY, alpha);
    break;
  case BACKDROP_MAZE:
    drawMazeBackdrop(cameraY, alpha);
    break;
  case BACKDROP_BLOCKS:
    drawBlocksBackdrop(cameraY, alpha);
    break;
  case BACKDROP_HILLS:
  default:
    drawHillsBackdrop(cameraY, alpha);
    break;
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

// Fraction of the band over which the incoming backdrop's shapes crossfade in
// — held out until this late so the next level's decor only appears near the
// very end of the current level, not from its start. The sky gradient still
// morphs continuously across the whole band (below); only the shapes wait.
#define BACKDROP_XFADE_START 0.85f

// Sky gradient morphs continuously with altitude (blended palette); the two
// neighbor themes' backdrop *shapes* crossfade only in the last ~15% of the
// band (BACKDROP_XFADE_START) so the next level's decor appears near the end.
void drawGameBackground(float cameraY) {
  float ascent = -cameraY; // camera starts at 0 and decreases going up
  themeUpdate(ascent);

  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SKY_TOP, SKY_BOT);

  int lo, hi;
  float f;
  themeBlend(&lo, &hi, &f);
  float fx = f <= BACKDROP_XFADE_START
                 ? 0.0f
                 : (f - BACKDROP_XFADE_START) / (1.0f - BACKDROP_XFADE_START);
  drawBackdrop(THEMES[lo].backdrop, cameraY, 1.0f - fx);
  drawBackdrop(THEMES[hi].backdrop, cameraY, fx);
}

#ifdef BG_SELFTEST
// Contact strip of all five backdrops, so the later eras can be eyeballed
// without climbing 80,000 units. Same reason the `post` selftest exists. Needs
// a real GL context (unlike the svg/tex ones) because it renders the actual
// draw path.
//   cc -DBG_SELFTEST src/background.c src/theme.c src/tex.c \
//      -Iinclude -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
//      \ -o /tmp/bgtest && /tmp/bgtest /tmp/bgstrip.png
#include <assert.h>
#include <stdio.h>

// Only has to land *inside* each band, so a change to BAND_HEIGHT in theme.c
// degrades the strip rather than breaking it. Sampled just past each band's
// *start*, not its midpoint: the palette lerps continuously across the whole
// band, so a midpoint sample shows every theme half-morphed into the next one
// (grey clouds on the day level) and tells you nothing about the art.
#define BAND 20000.0f
#define CELL 300

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "bgtest");
  while (!texPreloadStep()) {
  }
  RenderTexture rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
  Image sheet = GenImageColor(CELL * THEME_COUNT, CELL, BLACK);

  for (int i = 0; i < THEME_COUNT; i++) {
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    drawGameBackground(-(i * BAND + BAND * 0.05f)); // just inside the band
    EndTextureMode();

    Image im = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&im); // framebuffer textures are bottom-up
    // Every translucent draw blends the target's *alpha* channel down as well
    // as its color, so the readback is a correct picture carrying junk alpha.
    // Left alone, ImageDrawImage would then blend the cell onto the black
    // sheet and every cloud would read as a dark smudge — an artifact of this
    // strip, not of the game.
    Color *q = im.data;
    for (int k = 0, n = im.width * im.height; k < n; k++) {
      q[k].a = 255;
    }

    // Two failure modes this pass can actually produce, neither of which
    // crashes: a texture that never baked leaves the backdrop blank, and an
    // additive pass left un-ended blows the whole frame to white.
    const Color *p = im.data;
    long sum = 0;
    int lo = 255, hi = 0;
    for (int k = 0, n = im.width * im.height; k < n; k++) {
      int l = (p[k].r + p[k].g + p[k].b) / 3;
      sum += l;
      lo = (l < lo) ? l : lo;
      hi = (l > hi) ? l : hi;
    }
    int mean = (int)(sum / (im.width * im.height));
    printf("theme %d: luma %3d..%3d  mean %3d\n", i, lo, hi, mean);
    assert(hi - lo > 32 && "backdrop came out flat");
    assert(mean > 8 && mean < 247 && "backdrop blown to black or white");

    ImageResize(&im, CELL, CELL);
    ImageDrawImage(&sheet, im, i * CELL, 0, WHITE);
    UnloadImage(im);
  }
  ExportImage(sheet, argc > 1 ? argv[1] : "bgstrip.png");
  printf("ok — %d backdrops\n", THEME_COUNT);
  CloseWindow();
  return 0;
}
#endif
