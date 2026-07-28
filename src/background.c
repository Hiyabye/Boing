#include "background.h"

#include "core.h"
#include "raylib.h"
#include "tex.h"
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

// Fluffy cloud: a clump of three TEX_CLOUD stamps. One quad would read as a
// circle however much noise is inside it — the mask is radial — so the lumpy
// silhouette still has to come from the arrangement, exactly as the old
// five-circle stack made it. What the texture buys is the chewed edge and the
// internal structure the flat circles never had.
static void drawCloud(float x, float y, float s, Color c, float a) {
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
  // {on-screen tile size, parallax depth, alpha, source x offset}. The far
  // layer's tile is the texture's own pixel size so it maps 1:1 and its 1px
  // stars stay sharp; the near one is magnified 1.5x into bigger, closer stars.
  // The x offsets keep the two layers from being the same picture twice.
  static const float LAYER[2][4] = {{512.0f, 0.10f, 0.55f, 0.0f},
                                    {768.0f, 0.26f, 0.9f, 213.0f}};
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

// L2 backdrop: moon + craters, twinkling stars (some sparkling), a periodic
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

// L3 backdrop: a scrolling neon maze (broken corridors, not a full grid) with
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

// L4 backdrop: a faint playfield "well" grid with real tetrominoes raining in
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
// Contact strip of all four backdrops, so L2/L3/L4 can be eyeballed without
// climbing 60,000 units. Same reason the `post` selftest exists. Needs a real
// GL context (unlike the svg/tex ones) because it renders the actual draw path.
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
