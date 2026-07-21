#include "theme.h"

#include "core.h" // clampf

#include <stddef.h> // offsetof

// Ascent (world-units of climb) that one theme spans before it has fully
// morphed into the next. ~16000 matches the old single-theme dusk scale.
#define BAND_HEIGHT 16000.0f

// The theme table. A new level is one more row here (and, for a distinct
// backdrop shape, a case in background.c). Row order = climb order.
const Theme THEMES[] = {
    // 0 — Nature: sunny day, clouds, candy blob. (the original palette)
    {
        .skyTop = {0x6F, 0xC5, 0xFF, 0xFF},
        .skyBot = {0xCF, 0xF3, 0xFF, 0xFF},
        .skyTopHigh = {0x5A, 0x54, 0xC8, 0xFF},
        .skyBotHigh = {0xA8, 0x9C, 0xEA, 0xFF},
        .sun = {0xFF, 0xE7, 0x8A, 0xFF},
        .sunCore = {0xFF, 0xF6, 0xD0, 0xFF},
        .cloud = {0xFF, 0xFF, 0xFF, 0xFF},
        .cloudShade = {0xC7, 0xDE, 0xF2, 0xFF},
        .candyPink = {0xFF, 0x7F, 0xB0, 0xFF},
        .candyPinkDk = {0xE4, 0x46, 0x8B, 0xFF},
        .mouth = {0x7A, 0x2A, 0x4A, 0xFF},
        .coinHi = {0xFF, 0xE3, 0x6B, 0xFF},
        .coinLo = {0xFF, 0xA6, 0x1E, 0xFF},
        .grass = {0x8B, 0xD4, 0x50, 0xFF},
        .grassDk = {0x57, 0xA6, 0x30, 0xFF},
        .grape = {0xAC, 0x6E, 0xF2, 0xFF},
        .grapeDk = {0x71, 0x3B, 0xC2, 0xFF},
        .magnetRed = {0xE8, 0x3A, 0x3A, 0xFF},
        .rocketBody = {0xF2, 0xF2, 0xF2, 0xFF},
        .rocketFin = {0xFF, 0x6B, 0x3D, 0xFF},
        .flame = {0xFF, 0xC2, 0x3D, 0xFF},
        .confetti =
            {
                {0xFF, 0x7F, 0xB0, 0xFF},
                {0x5A, 0xD1, 0xE6, 0xFF},
                {0x8B, 0xD4, 0x50, 0xFF},
                {0xFF, 0xE3, 0x6B, 0xFF},
                {0xAC, 0x6E, 0xF2, 0xFF},
                {0xFF, 0x9E, 0x3D, 0xFF},
            },
        .backdrop = BACKDROP_CLOUDS,
        .track = 0,
    },
    // 1 — Arcade: dark neon "tetris" sky, falling-block backdrop, neon scene.
    {
        .skyTop = {0x14, 0x12, 0x30, 0xFF},
        .skyBot = {0x05, 0x05, 0x14, 0xFF},
        .skyTopHigh = {0x24, 0x1C, 0x4A, 0xFF},
        .skyBotHigh = {0x02, 0x02, 0x0A, 0xFF},
        .sun = {0x39, 0xE6, 0xD8, 0xFF}, // unused (no sun in blocks backdrop)
        .sunCore = {0xC8, 0xFF, 0xF6, 0xFF},
        .cloud = {0x2D, 0xE2, 0xE6, 0xFF},      // neon block body
        .cloudShade = {0x17, 0x7A, 0x9E, 0xFF}, // block shade
        .candyPink = {0xFF, 0x7F, 0xB0, 0xFF},
        .candyPinkDk = {0xE4, 0x46, 0x8B, 0xFF},
        .mouth = {0x1A, 0x0C, 0x28, 0xFF},
        .coinHi = {0xF9, 0xF0, 0x2E, 0xFF},
        .coinLo = {0xE0, 0x8A, 0x00, 0xFF},
        .grass = {0xE6, 0x2D, 0x9B, 0xFF}, // neon platform
        .grassDk = {0x8A, 0x17, 0x5E, 0xFF},
        .grape = {0x39, 0xE6, 0x4E, 0xFF}, // neon-green monster
        .grapeDk = {0x1E, 0x8A, 0x2E, 0xFF},
        .magnetRed = {0xE6, 0x2D, 0x2D, 0xFF},
        .rocketBody = {0xE0, 0xE0, 0xF0, 0xFF},
        .rocketFin = {0x2D, 0xE2, 0xE6, 0xFF},
        .flame = {0xF9, 0xF0, 0x2E, 0xFF},
        .confetti = // classic tetromino colors
        {
            {0x2D, 0xE2, 0xE6, 0xFF},
            {0xF9, 0xF0, 0x2E, 0xFF},
            {0xA0, 0x2D, 0xE6, 0xFF},
            {0x39, 0xE6, 0x4E, 0xFF},
            {0xE6, 0x2D, 0x2D, 0xFF},
            {0xE6, 0x8A, 0x2D, 0xFF},
        },
        .backdrop = BACKDROP_BLOCKS,
        .track = 1,
    },
};
const int THEME_COUNT = (int)(sizeof(THEMES) / sizeof(THEMES[0]));

Theme gTheme;

// Neighbor indices + blend factor from the last themeUpdate.
static int gLo = 0, gHi = 1;
static float gF = 0.0f;

// Blend every Color field (the whole leading run of the struct) from a->b by f.
static Theme lerpTheme(const Theme *a, const Theme *b, float f) {
  Theme r = *a;
  const Color *ca = (const Color *)a;
  const Color *cb = (const Color *)b;
  Color *cr = (Color *)&r;
  int n = (int)(offsetof(Theme, backdrop) / sizeof(Color));
  for (int i = 0; i < n; i++) {
    cr[i] = ColorLerp(ca[i], cb[i], f);
  }
  // Discrete fields switch at the midpoint; the background reads themeBlend()
  // directly for a crossfade, so this only feeds themeTrack().
  r.backdrop = f < 0.5f ? a->backdrop : b->backdrop;
  r.track = f < 0.5f ? a->track : b->track;
  return r;
}

void themeUpdate(float ascent) {
  if (ascent < 0.0f) {
    ascent = 0.0f;
  }
  float t = ascent / BAND_HEIGHT;
  int level = (int)t; // 0-based band, unbounded
  gF = clampf(t - (float)level, 0.0f, 1.0f);
  // Levels are infinite; themes loop, so once the last theme's band fills it
  // wraps back to the first (e.g. with 2 themes: nature→arcade→nature→…).
  gLo = level % THEME_COUNT;
  gHi = (level + 1) % THEME_COUNT;
  gTheme = lerpTheme(&THEMES[gLo], &THEMES[gHi], gF);
}

void themeBlend(int *lo, int *hi, float *f) {
  *lo = gLo;
  *hi = gHi;
  *f = gF;
}

int themeTrack(void) { return gTheme.track; }

// 0-based level (theme band) fully entered at this ascent — increments once
// per whole BAND_HEIGHT climbed, unbounded (levels are infinite; themes loop).
// Pure (no global state) so gameplay can poll it.
int themeLevelAt(float ascent) {
  if (ascent < 0.0f) {
    ascent = 0.0f;
  }
  return (int)(ascent / BAND_HEIGHT);
}
