#include "theme.h"

#include "core.h"  // clampf
#include "sound.h" // MusicTrack ids for Theme.track

#include <stddef.h> // offsetof

// Ascent (world-units of climb) that one theme spans before it has fully
// morphed into the next — a full level band.
#define BAND_HEIGHT 20000.0f

// The theme table. A new level is one more row here (and, for a distinct
// backdrop shape, a case in background.c). Row order = climb order — the scene
// morphs meadow->open sky->night->neon->digital over the climb, each an homage
// to a classic-gaming *era* (never a specific product; no proprietary names
// ship).
const Theme THEMES[] = {
    // 0 — Sunny Hills: bright day, clouds + rolling hills, candy blob. A
    // classic sidescroller morning. (the original Nature palette)
    {
        .skyTop = {0x6F, 0xC5, 0xFF, 0xFF},
        .skyBot = {0xCF, 0xF3, 0xFF, 0xFF},
        .skyTopHigh = {0x5A, 0x54, 0xC8, 0xFF},
        .skyBotHigh = {0xA8, 0x9C, 0xEA, 0xFF},
        .sun = {0xFF, 0xE7, 0x8A, 0xFF},
        .sunCore = {0xFF, 0xF6, 0xD0, 0xFF},
        .cloud = {0xFF, 0xFF, 0xFF, 0xFF},
        .cloudShade = {0xC7, 0xDE, 0xF2, 0xFF},
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
        .backdrop = BACKDROP_HILLS,
        .track = TRACK_L1,
    },
    // 1 - Birdsong Skies: clear azure air, sunlit cloud banks and migrating
    // birds. Platforms drift sideways on the wind.
    {
        .skyTop = {0x28, 0x94, 0xE8, 0xFF},
        .skyBot = {0xB8, 0xEB, 0xFF, 0xFF},
        .skyTopHigh = {0x17, 0x69, 0xB8, 0xFF},
        .skyBotHigh = {0x72, 0xC9, 0xED, 0xFF},
        .sun = {0xFF, 0xE7, 0x92, 0xFF},
        .sunCore = {0xFF, 0xFB, 0xDF, 0xFF},
        .cloud = {0xFF, 0xFF, 0xFF, 0xFF},
        .cloudShade = {0xA9, 0xD7, 0xEA, 0xFF},
        .mouth = {0x45, 0x36, 0x58, 0xFF},
        .coinHi = {0xFF, 0xE7, 0x73, 0xFF},
        .coinLo = {0xF4, 0xA8, 0x25, 0xFF},
        .grass = {0x70, 0xC9, 0xE8, 0xFF},
        .grassDk = {0x2F, 0x86, 0xB8, 0xFF},
        .grape = {0xF0, 0x72, 0xA8, 0xFF},
        .grapeDk = {0xB8, 0x3F, 0x78, 0xFF},
        .magnetRed = {0xE8, 0x45, 0x45, 0xFF},
        .rocketBody = {0xF8, 0xFC, 0xFF, 0xFF},
        .rocketFin = {0x36, 0x9E, 0xD8, 0xFF},
        .flame = {0xFF, 0xC9, 0x42, 0xFF},
        .confetti =
            {
                {0x28, 0x94, 0xE8, 0xFF},
                {0xFF, 0xFF, 0xFF, 0xFF},
                {0xFF, 0xE7, 0x73, 0xFF},
                {0x70, 0xC9, 0xE8, 0xFF},
                {0xF0, 0x72, 0xA8, 0xFF},
                {0x9D, 0xDE, 0xF5, 0xFF},
            },
        .backdrop = BACKDROP_SKIES,
        .track = TRACK_L2,
    },
    // 2 - Night Blocks: moonlit deep-blue sky, pixel stars, blocky terrain. A
    // voxel-survival dusk (homage to blocky night-survival games).
    {
        .skyTop = {0x14, 0x1E, 0x44, 0xFF},
        .skyBot = {0x30, 0x42, 0x70, 0xFF},
        .skyTopHigh = {0x0A, 0x12, 0x2E, 0xFF},
        .skyBotHigh = {0x1C, 0x28, 0x4E, 0xFF},
        .sun = {0xD6, 0xE2, 0xFF, 0xFF}, // moon glow
        .sunCore = {0xFF, 0xFF, 0xFF, 0xFF},
        .cloud = {0x5A, 0x6C, 0x9E, 0xFF}, // dusky night cloud/terrain body
        .cloudShade = {0x30, 0x3E, 0x66, 0xFF}, // terrain shade
        .mouth = {0x2A, 0x1A, 0x38, 0xFF},
        .coinHi = {0x8E, 0xF0, 0xFF, 0xFF}, // diamond-cyan pickups
        .coinLo = {0x3A, 0xA8, 0xE0, 0xFF},
        .grass = {0x6C, 0xA8, 0x3E, 0xFF}, // grass-block green over dirt
        .grassDk = {0x4A, 0x38, 0x28, 0xFF},
        .grape = {0x6E, 0xC8, 0x7A, 0xFF}, // spooky night creature
        .grapeDk = {0x3E, 0x82, 0x4E, 0xFF},
        .magnetRed = {0xE8, 0x4A, 0x4A, 0xFF},
        .rocketBody = {0xE0, 0xE0, 0xF0, 0xFF},
        .rocketFin = {0x6E, 0xA8, 0xFF, 0xFF},
        .flame = {0xFF, 0xC2, 0x3D, 0xFF},
        .confetti =
            {
                {0x8E, 0xF0, 0xFF, 0xFF},
                {0x6C, 0xA8, 0x3E, 0xFF},
                {0xD6, 0xE2, 0xFF, 0xFF},
                {0x6E, 0xC8, 0x7A, 0xFF},
                {0x3A, 0xA8, 0xE0, 0xFF},
                {0xFF, 0xE3, 0x6B, 0xFF},
            },
        .backdrop = BACKDROP_STARS,
        .track = TRACK_L3,
    },
    // 3 - Neon Maze: purple-black void, neon maze walls + pellet dots, ghost
    // creatures (homage to the golden-age dot-muncher maze arcades).
    {
        .skyTop = {0x2A, 0x10, 0x48, 0xFF},
        .skyBot = {0x10, 0x06, 0x24, 0xFF},
        .skyTopHigh = {0x3A, 0x18, 0x60, 0xFF},
        .skyBotHigh = {0x08, 0x03, 0x16, 0xFF},
        .sun = {0x9A, 0x6A, 0xFF, 0xFF}, // faint neon glow
        .sunCore = {0xE0, 0xC0, 0xFF, 0xFF},
        .cloud = {0x2A, 0x5A, 0xF0, 0xFF},      // neon maze wall body
        .cloudShade = {0x16, 0x2E, 0x9A, 0xFF}, // wall shade
        .mouth = {0x1A, 0x08, 0x2A, 0xFF},
        .coinHi = {0xFF, 0xE8, 0x6B, 0xFF}, // pellet yellow
        .coinLo = {0xFF, 0xC0, 0x1E, 0xFF},
        .grass = {0xE0, 0x4A, 0xF0, 0xFF}, // neon-magenta platform
        .grassDk = {0x8A, 0x24, 0x9A, 0xFF},
        .grape = {0xFF, 0x6E, 0xC8, 0xFF}, // pink ghost creature
        .grapeDk = {0xC2, 0x3B, 0x8A, 0xFF},
        .magnetRed = {0xFF, 0x3A, 0x5A, 0xFF},
        .rocketBody = {0xE8, 0xE8, 0xFF, 0xFF},
        .rocketFin = {0x2A, 0x5A, 0xF0, 0xFF},
        .flame = {0xFF, 0xC2, 0x3D, 0xFF},
        .confetti =
            {
                {0xFF, 0xE8, 0x6B, 0xFF},
                {0x2A, 0x5A, 0xF0, 0xFF},
                {0xFF, 0x6E, 0xC8, 0xFF},
                {0xE0, 0x4A, 0xF0, 0xFF},
                {0x39, 0xE6, 0xD8, 0xFF},
                {0xFF, 0x9E, 0x3D, 0xFF},
            },
        .backdrop = BACKDROP_MAZE,
        .track = TRACK_L4,
    },
    // 4 - Falling Blocks: green digital sky, falling neon tetromino backdrop.
    // The endgame (homage to the falling-block puzzle era).
    {
        .skyTop = {0x0A, 0x22, 0x14, 0xFF},
        .skyBot = {0x03, 0x0E, 0x06, 0xFF},
        .skyTopHigh = {0x14, 0x30, 0x1E, 0xFF},
        .skyBotHigh = {0x02, 0x06, 0x03, 0xFF},
        .sun = {0x39, 0xE6, 0xD8, 0xFF}, // unused (no sun in blocks backdrop)
        .sunCore = {0xC8, 0xFF, 0xF6, 0xFF},
        .cloud = {0x2D, 0xE2, 0xE6, 0xFF},      // neon block body
        .cloudShade = {0x17, 0x7A, 0x9E, 0xFF}, // block shade
        .mouth = {0x0C, 0x1A, 0x10, 0xFF},
        .coinHi = {0xF9, 0xF0, 0x2E, 0xFF},
        .coinLo = {0xE0, 0x8A, 0x00, 0xFF},
        .grass = {0xE6, 0x2D, 0x9B, 0xFF}, // neon platform
        .grassDk = {0x8A, 0x17, 0x5E, 0xFF},
        .grape = {0xFF, 0x62, 0x58, 0xFF}, // warm hazard against green field
        .grapeDk = {0xA8, 0x22, 0x3A, 0xFF},
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
        .track = TRACK_L5,
    },
};
// THEME_COUNT is a macro (theme.h) so tables keyed off the theme list can be
// checked at compile time; keep it honest about the table it names.
STATIC_ASSERT(sizeof(THEMES) / sizeof(THEMES[0]) == THEME_COUNT, themes_full);
// Level tracks are the first contiguous MusicTrack ids, one per theme row.
STATIC_ASSERT(THEME_COUNT == TRACK_MENU, theme_level_tracks_full);

Theme gTheme;
Color gUiAccent = {255, 127, 176, 255};
Color gUiAccentDark = {228, 70, 139, 255};

// Neighbor indices + blend factor from the last themeUpdate.
static int bandLo = 0, bandHi = 1;
static float bandF = 0.0f;

// lerpTheme walks the leading Color run as a raw Color[] up to `backdrop`, so
// that offset must land on a whole number of Colors — i.e. no non-Color field
// may sneak in before `backdrop`. A reordered struct trips this at compile
// time.
STATIC_ASSERT(offsetof(Theme, backdrop) % sizeof(Color) == 0, theme_color_run);

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
  // Keep a discrete fallback backdrop value, though background.c reads
  // themeBlend() directly to crossfade the two shapes.
  r.backdrop = f < 0.5f ? a->backdrop : b->backdrop;
  return r;
}

void themeUpdate(float ascent) {
  if (ascent < 0.0f) {
    ascent = 0.0f;
  }
  float t = ascent / BAND_HEIGHT;
  int level = (int)t; // 0-based band, unbounded
  bandF = clampf(t - (float)level, 0.0f, 1.0f);
  // Levels are infinite; themes loop, so once the last theme's band fills it
  // wraps back to the first (Sunny Hills -> Birdsong Skies -> Night Blocks ->
  // Neon Maze -> Falling Blocks -> Sunny Hills -> ...).
  bandLo = level % THEME_COUNT;
  bandHi = (level + 1) % THEME_COUNT;
  gTheme = lerpTheme(&THEMES[bandLo], &THEMES[bandHi], bandF);
}

void themeBlend(int *lo, int *hi, float *f) {
  *lo = bandLo;
  *hi = bandHi;
  *f = bandF;
}

// How much post-processing bloom this altitude wants. A separate table rather
// than a Theme field because Theme's leading run must stay all-Color for the
// generic lerp above; blended over the band by hand so the glow rises into the
// neon level and falls out of it with the palette.
//
// It also weights every additive stamp in world.c (see glowStamp), so this is
// the one knob for how hard the arcade eras blaze. Sunny Hills is no longer a
// flat 0: real daylight blooms too, and the value that used to mean "no bloom"
// now also meant "no light on the coins".
static const float GLOW[] = {0.12f, 0.16f, 0.35f, 1.0f, 0.7f};
STATIC_ASSERT(sizeof(GLOW) / sizeof(GLOW[0]) == THEME_COUNT,
              theme_glow_table_complete);

float themeGlow(void) {
  return GLOW[bandLo] + (GLOW[bandHi] - GLOW[bandLo]) * bandF;
}

// 0-based level (theme band) fully entered at this ascent — increments once
// per whole BAND_HEIGHT climbed, unbounded (levels are infinite; themes loop).
// Pure (no global state) so gameplay can poll it.
int themeLevelAt(float ascent) {
  if (ascent < 0.0f) {
    ascent = 0.0f;
  }
  return (int)(ascent / BAND_HEIGHT);
}

float themeBandProgressAt(float ascent) {
  int level = themeLevelAt(ascent);
  return clampf(ascent / BAND_HEIGHT - (float)level, 0.0f, 1.0f);
}
