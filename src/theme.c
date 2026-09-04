#include "theme.h"

#include "core.h" // clampf

#include <stddef.h> // offsetof
#include <string.h> // memcpy

#ifdef BG_SELFTEST
#include <assert.h>
#include <math.h>
#endif

// Ascent (world units of climb) occupied by one era. The measured 19-32
// second era keeps its palette stable until the final roughly 4-7 seconds.
#define BAND_HEIGHT 20000.0f
#define TRANSITION_START 0.78f

// The theme table. A new level is one more row here (and, for a distinct
// backdrop shape, a case in background.c). Row order is climb order: meadow,
// open sky, night, neon, digital blocks, a volcanic forge, an underground root
// garden, then meadow again. Each remains genre homage, never a specific
// product.
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
        .bridge = BRIDGE_CLOUDLINE,
        .track = TRACK_HILLS,
        .name = "SUNNY HILLS",
        .glow = 0.12f,
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
        .bridge = BRIDGE_TWILIGHT,
        .track = TRACK_SKIES,
        .name = "BIRDSONG SKIES",
        .glow = 0.16f,
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
        .bridge = BRIDGE_SIGNAL,
        .track = TRACK_NIGHT,
        .name = "NIGHT BLOCKS",
        .glow = 0.35f,
    },
    // 3 - Neon Maze: purple-black void, signal corridors, pellet dots, and
    // roaming grape creatures in an original dot-chase homage.
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
        .bridge = BRIDGE_FRAGMENTATION,
        .track = TRACK_MAZE,
        .name = "NEON MAZE",
        .glow = 1.0f,
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
        .bridge = BRIDGE_MELTDOWN,
        .track = TRACK_BLOCKS,
        .name = "FALLING BLOCKS",
        .glow = 0.7f,
    },
    // 5 - Magma Forge: a volcanic foundry where charged basalt vents erupt on
    // landing, launching the blob before cooling into safe obsidian.
    {
        .skyTop = {0x2A, 0x0F, 0x25, 0xFF},
        .skyBot = {0x7A, 0x20, 0x18, 0xFF},
        .skyTopHigh = {0x12, 0x08, 0x18, 0xFF},
        .skyBotHigh = {0x3B, 0x0F, 0x12, 0xFF},
        .sun = {0xFF, 0x6A, 0x24, 0xFF},
        .sunCore = {0xFF, 0xD9, 0x79, 0xFF},
        .cloud = {0x5A, 0x3B, 0x3B, 0xFF},
        .cloudShade = {0x21, 0x1A, 0x22, 0xFF},
        .mouth = {0x2B, 0x10, 0x15, 0xFF},
        .coinHi = {0xFF, 0xE4, 0x7A, 0xFF},
        .coinLo = {0xFF, 0x7A, 0x1F, 0xFF},
        .grass = {0x65, 0x45, 0x45, 0xFF},
        .grassDk = {0x24, 0x1A, 0x22, 0xFF},
        .grape = {0x55, 0xD6, 0xC7, 0xFF},
        .grapeDk = {0x24, 0x7F, 0x78, 0xFF},
        .magnetRed = {0xF2, 0x3D, 0x32, 0xFF},
        .rocketBody = {0xE8, 0xDF, 0xD8, 0xFF},
        .rocketFin = {0xFF, 0x62, 0x24, 0xFF},
        .flame = {0xFF, 0xC1, 0x34, 0xFF},
        .confetti =
            {
                {0xFF, 0x5A, 0x1F, 0xFF},
                {0xFF, 0xB5, 0x2E, 0xFF},
                {0xFF, 0xE4, 0x7A, 0xFF},
                {0xB8, 0x3A, 0x2D, 0xFF},
                {0x6F, 0x39, 0x44, 0xFF},
                {0x5A, 0xD8, 0xC9, 0xFF},
            },
        .backdrop = BACKDROP_MAGMA,
        .bridge = BRIDGE_BURROW,
        .track = TRACK_MAGMA,
        .name = "MAGMA FORGE",
        .glow = 0.9f,
    },
    // 6 - Reboot Garden: a living layer of loam and old roots beneath the
    // surface. Landing anchors a falling seedbed, wakes its roots, and sprouts
    // a safe platform while long rootworms patrol the soil.
    {
        .skyTop = {0x21, 0x14, 0x0F, 0xFF},
        .skyBot = {0x6B, 0x43, 0x2C, 0xFF},
        .skyTopHigh = {0x13, 0x0B, 0x08, 0xFF},
        .skyBotHigh = {0x3A, 0x24, 0x19, 0xFF},
        .sun = {0xD6, 0x9A, 0x56, 0xFF},
        .sunCore = {0xFF, 0xE0, 0xA1, 0xFF},
        .cloud = {0xA8, 0x79, 0x4F, 0xFF},
        .cloudShade = {0x4A, 0x2D, 0x1D, 0xFF},
        .mouth = {0x2B, 0x17, 0x11, 0xFF},
        .coinHi = {0xFF, 0xD5, 0x6A, 0xFF},
        .coinLo = {0xC8, 0x79, 0x2C, 0xFF},
        .grass = {0x88, 0xB8, 0x4F, 0xFF},
        .grassDk = {0x5B, 0x3B, 0x24, 0xFF},
        .grape = {0xD9, 0x82, 0x62, 0xFF},
        .grapeDk = {0x7A, 0x3E, 0x35, 0xFF},
        .magnetRed = {0xD6, 0x45, 0x38, 0xFF},
        .rocketBody = {0xEE, 0xE1, 0xCB, 0xFF},
        .rocketFin = {0x73, 0x95, 0x45, 0xFF},
        .flame = {0xF2, 0xA4, 0x45, 0xFF},
        .confetti =
            {
                {0x88, 0xB8, 0x4F, 0xFF},
                {0xD9, 0x82, 0x62, 0xFF},
                {0xFF, 0xD5, 0x6A, 0xFF},
                {0xB9, 0x8A, 0x5A, 0xFF},
                {0x80, 0x60, 0x4A, 0xFF},
                {0xE9, 0xC5, 0x8D, 0xFF},
            },
        .backdrop = BACKDROP_GARDEN,
        .bridge = BRIDGE_EMERGENCE,
        .track = TRACK_GARDEN,
        .name = "REBOOT GARDEN",
        .glow = 0.2f,
    },
};
STATIC_ASSERT(sizeof(THEMES) / sizeof(THEMES[0]) == ERA_COUNT, themes_full);
STATIC_ASSERT((int)BRIDGE_COUNT == (int)ERA_COUNT, era_bridges_full);

Theme gTheme;
Color gUiAccent = {255, 127, 176, 255};
Color gUiAccentDark = {228, 70, 139, 255};

// The declaration keeps 19 named colors and the confetti palette contiguous.
// Exact byte-count checks reject padding and force a guard update if either
// generic blend range changes.
#define ATMOSPHERE_COLOR_COUNT 8
#define THEME_COLOR_COUNT (19 + CONFETTI_COUNT)
STATIC_ASSERT(offsetof(Theme, mouth) == ATMOSPHERE_COLOR_COUNT * sizeof(Color),
              theme_atmosphere_run);
STATIC_ASSERT(offsetof(Theme, backdrop) == THEME_COLOR_COUNT * sizeof(Color),
              theme_color_run);

// Character-byte addressing plus memcpy is defined for adjacent struct fields;
// treating those fields as one Color array would violate C's aliasing rules.
static Color themeColorAt(const Theme *theme, int i) {
  Color color;
  memcpy(&color, (const unsigned char *)theme + (size_t)i * sizeof(Color),
         sizeof(color));
  return color;
}

static void setThemeColor(Theme *theme, int i, Color color) {
  memcpy((unsigned char *)theme + (size_t)i * sizeof(Color), &color,
         sizeof(color));
}

static float transitionBlend(float phase) {
  float t = clampf((phase - TRANSITION_START) / (1.0f - TRANSITION_START), 0.0f,
                   1.0f);
  return t * t * (3.0f - 2.0f * t);
}

ThemeSample themeSampleAt(float ascent) {
  if (ascent < 0.0f) {
    ascent = 0.0f;
  }
  float t = ascent / BAND_HEIGHT;
  int level = (int)t;
  EraId era = (EraId)(level % ERA_COUNT);
  float phase = clampf(t - (float)level, 0.0f, 1.0f);
  return (ThemeSample){
      .level = level,
      .cycle = level / ERA_COUNT,
      .era = era,
      .next = (EraId)((era + 1) % ERA_COUNT),
      .phase = phase,
      .blend = transitionBlend(phase),
  };
}

ThemeSample themeSampleAtWorldY(float worldY) {
  return themeSampleAt(SCREEN_HEIGHT / 2.0f - worldY);
}

// Blend every Color field (the whole leading run of the struct) from a->b by f.
static Theme lerpTheme(const Theme *a, const Theme *b, float f) {
  Theme r = f < 0.5f ? *a : *b;
  for (int i = 0; i < THEME_COLOR_COUNT; i++) {
    setThemeColor(&r, i, ColorLerp(themeColorAt(a, i), themeColorAt(b, i), f));
  }
  return r;
}

// Edge-only palette keys give selected transitions authored color anchors.
// They cover atmosphere through cloud shade, never gameplay colors, and remain
// bridge rendering rather than gameplay eras.
static const Color BLUE_HOUR[] = {
    {0x37, 0x57, 0x90, 0xFF}, {0xC7, 0x79, 0xA0, 0xFF},
    {0x20, 0x2F, 0x68, 0xFF}, {0x70, 0x4D, 0x85, 0xFF},
    {0xFF, 0xD1, 0x9A, 0xFF}, {0xFF, 0xF1, 0xD4, 0xFF},
    {0xC7, 0xB5, 0xD2, 0xFF}, {0x78, 0x6F, 0x9D, 0xFF},
};
static const Color MELTDOWN_HEAT[] = {
    {0x24, 0x12, 0x3F, 0xFF}, {0xA5, 0x32, 0x21, 0xFF},
    {0x0F, 0x15, 0x2D, 0xFF}, {0x50, 0x18, 0x1B, 0xFF},
    {0xFF, 0x74, 0x2E, 0xFF}, {0xFF, 0xE1, 0x88, 0xFF},
    {0x80, 0x35, 0x38, 0xFF}, {0x2B, 0x20, 0x2B, 0xFF},
};
static const Color DEEP_LOAM[] = {
    {0x20, 0x12, 0x10, 0xFF}, {0x84, 0x42, 0x25, 0xFF},
    {0x10, 0x08, 0x09, 0xFF}, {0x45, 0x23, 0x19, 0xFF},
    {0xE6, 0x7A, 0x32, 0xFF}, {0xFF, 0xD0, 0x80, 0xFF},
    {0x6D, 0x49, 0x32, 0xFF}, {0x2B, 0x1A, 0x15, 0xFF},
};
static const Color SURFACE_LIGHT[] = {
    {0x5A, 0x78, 0x7D, 0xFF}, {0xD8, 0xB7, 0x78, 0xFF},
    {0x34, 0x4F, 0x58, 0xFF}, {0x8D, 0x72, 0x4D, 0xFF},
    {0xFF, 0xE0, 0x8A, 0xFF}, {0xFF, 0xF4, 0xC9, 0xFF},
    {0xD8, 0xC7, 0x9D, 0xFF}, {0x8D, 0x76, 0x52, 0xFF},
};
STATIC_ASSERT(sizeof(BLUE_HOUR) / sizeof(BLUE_HOUR[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              blue_hour_full);
STATIC_ASSERT(sizeof(MELTDOWN_HEAT) / sizeof(MELTDOWN_HEAT[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              meltdown_heat_full);
STATIC_ASSERT(sizeof(DEEP_LOAM) / sizeof(DEEP_LOAM[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              deep_loam_full);
STATIC_ASSERT(sizeof(SURFACE_LIGHT) / sizeof(SURFACE_LIGHT[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              surface_light_full);

static Theme renderedTheme(ThemeSample sample) {
  const Theme *a = &THEMES[sample.era], *b = &THEMES[sample.next];
  Theme r = lerpTheme(a, b, sample.blend);
  const Color *key = NULL;
  if (a->bridge == BRIDGE_TWILIGHT) {
    key = BLUE_HOUR;
  } else if (a->bridge == BRIDGE_MELTDOWN) {
    key = MELTDOWN_HEAT;
  } else if (a->bridge == BRIDGE_BURROW) {
    key = DEEP_LOAM;
  } else if (a->bridge == BRIDGE_EMERGENCE) {
    key = SURFACE_LIGHT;
  }
  if (key != NULL) {
    float f = sample.blend * 2.0f;
    f = f < 1.0f ? f : f - 1.0f;
    for (int i = 0; i < ATMOSPHERE_COLOR_COUNT; i++) {
      Color from = sample.blend < 0.5f ? themeColorAt(a, i) : key[i];
      Color to = sample.blend < 0.5f ? key[i] : themeColorAt(b, i);
      setThemeColor(&r, i, ColorLerp(from, to, f));
    }
  }
  return r;
}

void themeUpdate(ThemeSample sample) { gTheme = renderedTheme(sample); }

// How much post-processing bloom this altitude wants. It also weights every
// additive stamp in world.c, so each authoritative theme row owns one glow
// knob. Sunny Hills keeps a small base because daylight coins still need light.
float themeGlow(ThemeSample sample) {
  float lo = THEMES[sample.era].glow, hi = THEMES[sample.next].glow;
  return lo + (hi - lo) * sample.blend;
}

#ifdef BG_SELFTEST
void themeSampleSelfTest(void) {
  ThemeSample s = themeSampleAt(-1.0f);
  assert(s.level == 0 && s.cycle == 0 && s.era == ERA_HILLS &&
         s.next == ERA_SKIES && s.phase == 0.0f && s.blend == 0.0f);

  s = themeSampleAt(BAND_HEIGHT * 0.70f);
  assert(fabsf(s.phase - 0.70f) < 0.0001f && s.blend == 0.0f);
  s = themeSampleAt(BAND_HEIGHT * TRANSITION_START);
  assert(fabsf(s.phase - TRANSITION_START) < 0.0001f && s.blend < 0.0001f);
  s = themeSampleAt(BAND_HEIGHT * (TRANSITION_START + 1.0f) * 0.5f);
  assert(fabsf(s.blend - 0.5f) < 0.0001f);

  // Post bloom follows each eased visual sample: the forge grows hotter as
  // Blocks melt, then cools smoothly into the underground garden.
  s = themeSampleAt(BAND_HEIGHT * (ERA_BLOCKS + 0.70f));
  assert(s.blend == 0.0f &&
         fabsf(themeGlow(s) - THEMES[ERA_BLOCKS].glow) < 0.0001f);
  s = themeSampleAt(BAND_HEIGHT *
                    (ERA_BLOCKS + (TRANSITION_START + 1.0f) * 0.5f));
  float meltGlow = themeGlow(s);
  assert(fabsf(s.blend - 0.5f) < 0.0001f &&
         meltGlow > THEMES[ERA_BLOCKS].glow &&
         meltGlow < THEMES[ERA_MAGMA].glow);
  s = themeSampleAt(BAND_HEIGHT *
                    (ERA_MAGMA + (TRANSITION_START + 1.0f) * 0.5f));
  float burrowGlow = themeGlow(s);
  assert(burrowGlow < THEMES[ERA_MAGMA].glow &&
         burrowGlow > THEMES[ERA_GARDEN].glow);

  int meltdownEdges = 0, burrowEdges = 0, emergenceEdges = 0;
  for (int era = 0; era < ERA_COUNT; era++) {
    meltdownEdges += THEMES[era].bridge == BRIDGE_MELTDOWN;
    burrowEdges += THEMES[era].bridge == BRIDGE_BURROW;
    emergenceEdges += THEMES[era].bridge == BRIDGE_EMERGENCE;
  }
  assert(meltdownEdges == 1 && THEMES[ERA_BLOCKS].bridge == BRIDGE_MELTDOWN);
  assert(burrowEdges == 1 && THEMES[ERA_MAGMA].bridge == BRIDGE_BURROW);
  assert(emergenceEdges == 1 && THEMES[ERA_GARDEN].bridge == BRIDGE_EMERGENCE);
  assert(THEMES[ERA_GARDEN].track == TRACK_GARDEN);

  for (int level = 0; level < ERA_COUNT; level++) {
    float edge = BAND_HEIGHT * (level + 1);
    EraId next = (EraId)((level + 1) % ERA_COUNT);
    ThemeSample before = themeSampleAt(nextafterf(edge, 0.0f));
    ThemeSample at = themeSampleAt(edge);
    assert(before.level == level && before.era == (EraId)level &&
           before.next == next && before.phase < 1.0f &&
           before.blend > 0.9999f);
    assert(at.level == level + 1 && at.era == next && at.phase == 0.0f &&
           at.blend == 0.0f);
  }

  float edge = BAND_HEIGHT;
  s = themeSampleAt(nextafterf(edge, INFINITY));
  assert(s.level == 1 && s.era == ERA_SKIES && s.phase > 0.0f &&
         s.blend == 0.0f);

  float wrap = BAND_HEIGHT * ERA_COUNT;
  s = themeSampleAt(wrap);
  assert(s.level == ERA_COUNT && s.cycle == 1 && s.era == ERA_HILLS &&
         s.next == ERA_SKIES && s.phase == 0.0f && s.blend == 0.0f &&
         fabsf(themeGlow(s) - THEMES[ERA_HILLS].glow) < 0.0001f);
  s = themeSampleAt(nextafterf(wrap, INFINITY));
  assert(s.level == ERA_COUNT && s.cycle == 1 && s.era == ERA_HILLS &&
         s.phase > 0.0f && s.blend == 0.0f);

  s = themeSampleAtWorldY(SCREEN_HEIGHT / 2.0f - edge);
  assert(s.level == 1 && s.era == ERA_SKIES && s.phase == 0.0f);
  float midpoint = (TRANSITION_START + 1.0f) * 0.5f;
  s = themeSampleAt(BAND_HEIGHT * (ERA_SKIES + midpoint));
  assert(s.level == ERA_SKIES && s.cycle == 0 && s.era == ERA_SKIES &&
         s.next == ERA_NIGHT && fabsf(s.blend - 0.5f) < 0.0001f);
  s.blend = 0.5f;
  themeUpdate(s);
  assert(
      gTheme.skyTop.r == BLUE_HOUR[0].r && gTheme.skyTop.g == BLUE_HOUR[0].g &&
      gTheme.skyTop.b == BLUE_HOUR[0].b && gTheme.skyBot.r == BLUE_HOUR[1].r &&
      gTheme.skyBot.g == BLUE_HOUR[1].g && gTheme.skyBot.b == BLUE_HOUR[1].b);

  s = themeSampleAt(BAND_HEIGHT * (ERA_BLOCKS + midpoint));
  assert(s.level == ERA_BLOCKS && s.cycle == 0 && s.era == ERA_BLOCKS &&
         s.next == ERA_MAGMA && fabsf(s.blend - 0.5f) < 0.0001f);
  s.blend = 0.5f;
  themeUpdate(s);
  assert(gTheme.skyTop.r == MELTDOWN_HEAT[0].r &&
         gTheme.skyTop.g == MELTDOWN_HEAT[0].g &&
         gTheme.skyTop.b == MELTDOWN_HEAT[0].b &&
         gTheme.skyBot.r == MELTDOWN_HEAT[1].r &&
         gTheme.skyBot.g == MELTDOWN_HEAT[1].g &&
         gTheme.skyBot.b == MELTDOWN_HEAT[1].b);

  s = themeSampleAt(BAND_HEIGHT * (ERA_MAGMA + midpoint));
  assert(s.level == ERA_MAGMA && s.era == ERA_MAGMA && s.next == ERA_GARDEN &&
         fabsf(s.blend - 0.5f) < 0.0001f);
  s.blend = 0.5f;
  themeUpdate(s);
  assert(
      gTheme.skyTop.r == DEEP_LOAM[0].r && gTheme.skyTop.g == DEEP_LOAM[0].g &&
      gTheme.skyTop.b == DEEP_LOAM[0].b && gTheme.skyBot.r == DEEP_LOAM[1].r &&
      gTheme.skyBot.g == DEEP_LOAM[1].g && gTheme.skyBot.b == DEEP_LOAM[1].b);

  s = themeSampleAt(BAND_HEIGHT * (ERA_GARDEN + midpoint));
  assert(s.level == ERA_GARDEN && s.era == ERA_GARDEN && s.next == ERA_HILLS &&
         fabsf(s.blend - 0.5f) < 0.0001f);
  s.blend = 0.5f;
  themeUpdate(s);
  assert(gTheme.skyTop.r == SURFACE_LIGHT[0].r &&
         gTheme.skyTop.g == SURFACE_LIGHT[0].g &&
         gTheme.skyTop.b == SURFACE_LIGHT[0].b &&
         gTheme.skyBot.r == SURFACE_LIGHT[1].r &&
         gTheme.skyBot.g == SURFACE_LIGHT[1].g &&
         gTheme.skyBot.b == SURFACE_LIGHT[1].b);
}
#endif
