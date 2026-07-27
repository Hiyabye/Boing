#include "background.h"

#include "core.h"
#include "raylib.h"
#include "theme.h"

#include <math.h>

#define CLOUD_COUNT 14
#define BLOCK_COUNT 20
#define HILL_COUNT 10
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

// One beveled cell in an arbitrary color: dark border + body + a top sheen, so
// stacked cells read as chunky 3D blocks (used by the tetromino backdrop).
static void drawBlockC(float x, float y, float s, Color c, float a) {
  DrawRectangle(x, y, s, s, Fade(ColorBrightness(c, -0.5f), a));
  DrawRectangle(x + s * 0.12f, y + s * 0.12f, s * 0.76f, s * 0.76f, Fade(c, a));
  DrawRectangle(x + s * 0.12f, y + s * 0.12f, s * 0.76f, s * 0.18f,
                Fade(ColorBrightness(c, 0.4f), a * 0.85f)); // sheen
}

// The seven tetromino footprints as (dx,dy) cells; drawTetromino stamps four
// beveled cells so the L4 backdrop rains real shapes, not lone squares.
#define TETRO_COUNT 7
static const signed char TETRO[TETRO_COUNT][4][2] = {
    {{-1, 0}, {0, 0}, {1, 0}, {2, 0}}, // I
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},  // O
    {{0, 0}, {1, 0}, {2, 0}, {1, 1}},  // T
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}},  // S
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}},  // Z
    {{0, 0}, {1, 0}, {2, 0}, {2, 1}},  // L
    {{0, 0}, {1, 0}, {2, 0}, {0, 1}},  // J
};
static void drawTetromino(float x, float y, float cell, int shape, Color c,
                          float a) {
  for (int k = 0; k < 4; k++) {
    drawBlockC(x + TETRO[shape][k][0] * cell, y + TETRO[shape][k][1] * cell,
               cell, c, a);
  }
}

// A rounded grassy mound: shaded skirt, rounded crown, and a brighter sunlit
// grass cap on top for depth (vs. the old flat single-color ellipse).
static void drawHill(float x, float baseY, float w, Color base, float a) {
  Color body = Fade(base, a);
  DrawRectangle(x - w, baseY, 2.0f * w, w, body); // skirt to the baseline
  DrawEllipse(x, baseY, w, w * 0.55f, body);      // rounded crown
  DrawEllipse(x, baseY - w * 0.08f, w * 0.82f, w * 0.4f,
              Fade(ColorBrightness(base, 0.28f), a)); // sunlit grass cap
}

// A little round-canopy tree: brown trunk + layered green foliage with a
// highlight, for the crests of the L1 hills.
static void drawTree(float x, float groundY, float s, float a) {
  DrawRectangle(x - s * 0.12f, groundY - s * 1.1f, s * 0.24f, s * 1.1f,
                Fade((Color){0x7A, 0x4A, 0x2A, 0xFF}, a)); // trunk
  Color g = GRASS, gd = GRASS_DK;
  DrawCircle(x, groundY - s * 1.5f, s * 0.75f, Fade(gd, a));
  DrawCircle(x - s * 0.4f, groundY - s * 1.3f, s * 0.6f, Fade(gd, a));
  DrawCircle(x + s * 0.4f, groundY - s * 1.3f, s * 0.6f, Fade(gd, a));
  DrawCircle(x, groundY - s * 1.6f, s * 0.62f, Fade(g, a));
  DrawCircle(x - s * 0.3f, groundY - s * 1.75f, s * 0.32f,
             Fade(ColorBrightness(g, 0.3f), a)); // sun-catch highlight
}

// A rounded shrub: three overlapping leaves with a top highlight.
static void drawBush(float x, float y, float s, float a) {
  Color g = GRASS, gd = GRASS_DK;
  DrawCircle(x - s * 0.6f, y, s * 0.6f, Fade(gd, a));
  DrawCircle(x + s * 0.6f, y, s * 0.6f, Fade(gd, a));
  DrawCircle(x, y - s * 0.2f, s * 0.8f, Fade(g, a));
  DrawCircle(x - s * 0.25f, y - s * 0.35f, s * 0.32f,
             Fade(ColorBrightness(g, 0.3f), a));
}

// A distant gull: a four-segment "m" silhouette drifting across the sky.
static void drawBird(float x, float y, float s, float a) {
  Color c = Fade((Color){0x44, 0x52, 0x6E, 0xFF}, a);
  Vector2 p0 = {x - s, y}, p1 = {x - s * 0.4f, y - s * 0.4f}, p2 = {x, y};
  Vector2 p3 = {x + s * 0.4f, y - s * 0.4f}, p4 = {x + s, y};
  DrawLineEx(p0, p1, 2.0f, c);
  DrawLineEx(p1, p2, 2.0f, c);
  DrawLineEx(p2, p3, 2.0f, c);
  DrawLineEx(p3, p4, 2.0f, c);
}

// Slowly rotating sun rays behind the disc (L1 only — never on the L2 moon).
static void drawSunRays(float alpha) {
  float t = (float)GetTime() * 0.2f;
  for (int i = 0; i < 12; i++) {
    float ang = t + i * (PI / 6.0f);
    Vector2 a = {SUN_X + cosf(ang) * SUN_R * 1.3f,
                 SUN_Y + sinf(ang) * SUN_R * 1.3f};
    Vector2 b = {SUN_X + cosf(ang) * SUN_R * 2.1f,
                 SUN_Y + sinf(ang) * SUN_R * 2.1f};
    DrawLineEx(a, b, 4.0f, Fade(SUN, 0.22f * alpha));
  }
}

// L1 backdrop: rayed sun + drifting gulls + rolling hills dressed with trees &
// bushes + parallax clouds.
static void drawHillsBackdrop(float cameraY, float alpha) {
  drawSunRays(alpha);
  drawSun(alpha);
  for (int i = 0; i < 5; i++) {
    float x =
        wrapf((float)GetTime() * 18.0f + i * 190.0f, SCREEN_WIDTH + 120.0f) -
        60.0f;
    float y = 70.0f + i * 34.0f + 8.0f * sinf((float)GetTime() * 0.8f + i);
    drawBird(x, y, 9.0f, 0.5f * alpha);
  }
  for (int i = 0; i < HILL_COUNT; i++) {
    float depth = 0.15f + (i % 2) * 0.15f;
    float w = 120.0f + (i % 3) * 70.0f;
    float x = wrapf(i * 233.0f + (float)GetTime() * (3.0f * depth),
                    SCREEN_WIDTH + 500.0f) -
              250.0f;
    float y =
        wrapf(i * 180.0f - cameraY * depth, SCREEN_HEIGHT + 400.0f) - 100.0f;
    float a = (0.6f - depth * 0.3f) * alpha;
    drawHill(x, y, w, (i % 2) ? GRASS_DK : GRASS, a);
    if (i % 3 == 0) {
      drawTree(x + w * 0.3f, y, 26.0f, a);
    } else if (i % 3 == 1) {
      drawBush(x - w * 0.2f, y - 6.0f, 18.0f, a);
    }
  }
  for (int i = 0; i < CLOUD_COUNT; i++) {
    float depth = parallaxDepth(i);
    float s = 14.0f + (i % 3) * 9.0f;
    float x = wrapf(i * 151.0f + (float)GetTime() * (6.0f * depth),
                    SCREEN_WIDTH + 300.0f) -
              150.0f;
    float y =
        wrapf(i * 97.0f - cameraY * depth, SCREEN_HEIGHT + 200.0f) - 100.0f;
    drawCloud(x, y, s, CLOUD, (0.85f - depth * 0.3f) * alpha);
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

// L2 backdrop: moon + craters, twinkling stars (some sparkling), a periodic
// shooting star, and floating voxel islands.
static void drawStarsBackdrop(float cameraY, float alpha) {
  drawMoon(alpha * 0.95f);
  for (int i = 0; i < STAR_COUNT; i++) {
    float depth = 0.2f + (i % 3) * 0.12f;
    float x = wrapf(i * 71.0f, SCREEN_WIDTH);
    float y = wrapf(i * 137.0f - cameraY * depth, SCREEN_HEIGHT + 100.0f);
    float tw = (0.4f + 0.6f * fabsf(sinf((float)GetTime() * 1.6f + i))) * alpha;
    float s = (i % 5 == 0) ? 3.5f : 1.5f + (i % 2);
    drawStar(x, y, s, tw, SUN_CORE);
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

// L3 backdrop: a scrolling neon maze (broken corridors, not a full grid) with
// pellets + pulsing power-pellets, drifting ghosts, and a chomping hero.
static void drawMazeBackdrop(float cameraY, float alpha) {
  float sc = -cameraY * 0.4f;
  float off = wrapf(sc, MAZE_CELL);
  int scroll = (int)floorf(sc / MAZE_CELL);
  int cols = (int)(SCREEN_WIDTH / MAZE_CELL) + 2;
  int rows = (int)(SCREEN_HEIGHT / MAZE_CELL) + 3;
  float t = 8.0f; // wall thickness
  Color glow = Fade(CLOUD, 0.16f * alpha);
  Color wall = Fade(ColorBrightness(CLOUD, 0.25f), 0.65f * alpha);
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
        DrawCircle(px, py, pr, Fade(COIN_HI, 0.8f * alpha));
      } else {
        DrawCircle(px, py, 3.0f, Fade(COIN_HI, 0.55f * alpha));
      }
    }
  }
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

// L4 backdrop: a faint playfield "well" grid with real tetrominoes raining in
// the theme's tetromino colors (parallax fall, same idiom as the clouds).
static void drawBlocksBackdrop(float cameraY, float alpha) {
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
