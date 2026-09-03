#include "theme.h"

#include "core.h" // clampf

#include <stddef.h> // offsetof

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
// open sky, night, neon, digital blocks, a rooted predawn garden, then meadow
// again. Each remains genre homage, never a specific product.
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
        .bridge = BRIDGE_REBOOT,
        .track = TRACK_BLOCKS,
        .name = "FALLING BLOCKS",
        .glow = 0.7f,
    },
    // 5 - Reboot Garden: predawn terraces built from rooted cells. Landing on
    // a descending piece stops it and grows a safe garden in the same shape.
    {
        .skyTop = {0x48, 0x5A, 0x96, 0xFF},
        .skyBot = {0xF1, 0xA5, 0x8F, 0xFF},
        .skyTopHigh = {0x27, 0x35, 0x63, 0xFF},
        .skyBotHigh = {0x9D, 0x66, 0x78, 0xFF},
        .sun = {0xFF, 0xC5, 0x6E, 0xFF},
        .sunCore = {0xFF, 0xF0, 0xBE, 0xFF},
        .cloud = {0xBD, 0xE5, 0xC7, 0xFF},
        .cloudShade = {0x67, 0x9A, 0x83, 0xFF},
        .mouth = {0x4A, 0x29, 0x40, 0xFF},
        .coinHi = {0xFF, 0xE3, 0x6B, 0xFF},
        .coinLo = {0xE9, 0x9A, 0x32, 0xFF},
        .grass = {0x79, 0xCE, 0x77, 0xFF},
        .grassDk = {0x37, 0x6B, 0x50, 0xFF},
        .grape = {0xE6, 0x6F, 0x9E, 0xFF},
        .grapeDk = {0x99, 0x43, 0x6D, 0xFF},
        .magnetRed = {0xDF, 0x48, 0x4E, 0xFF},
        .rocketBody = {0xF4, 0xF1, 0xE8, 0xFF},
        .rocketFin = {0x45, 0xA9, 0x9A, 0xFF},
        .flame = {0xFF, 0xAE, 0x52, 0xFF},
        .confetti =
            {
                {0x79, 0xCE, 0x77, 0xFF},
                {0xF1, 0x88, 0x88, 0xFF},
                {0xFF, 0xE3, 0x6B, 0xFF},
                {0x63, 0xC9, 0xC0, 0xFF},
                {0xAD, 0x78, 0xD0, 0xFF},
                {0xF0, 0xA2, 0x55, 0xFF},
            },
        .backdrop = BACKDROP_GARDEN,
        .bridge = BRIDGE_BLOOM,
        .track = TRACK_HILLS,
        .name = "REBOOT GARDEN",
        .glow = 0.24f,
    },
};
STATIC_ASSERT(sizeof(THEMES) / sizeof(THEMES[0]) == ERA_COUNT, themes_full);
STATIC_ASSERT((int)BRIDGE_COUNT == (int)ERA_COUNT, era_bridges_full);

Theme gTheme;
Color gUiAccent = {255, 127, 176, 255};
Color gUiAccentDark = {228, 70, 139, 255};

// lerpTheme walks the leading Color run as a raw Color[] up to `backdrop`, so
// that offset must land on a whole number of Colors - no non-Color field may
// sneak in before `backdrop`. A reordered struct trips this at compile time.
STATIC_ASSERT(offsetof(Theme, backdrop) % sizeof(Color) == 0, theme_color_run);

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
  const Color *ca = (const Color *)a;
  const Color *cb = (const Color *)b;
  Color *cr = (Color *)&r;
  int n = (int)(offsetof(Theme, backdrop) / sizeof(Color));
  for (int i = 0; i < n; i++) {
    cr[i] = ColorLerp(ca[i], cb[i], f);
  }
  return r;
}

// Edge-only palette keys give two transitions authored color anchors. They
// cover atmosphere through cloud shade, never gameplay colors, and remain
// bridge rendering rather than gameplay eras.
static const Color BLUE_HOUR[] = {
    {0x37, 0x57, 0x90, 0xFF}, {0xC7, 0x79, 0xA0, 0xFF},
    {0x20, 0x2F, 0x68, 0xFF}, {0x70, 0x4D, 0x85, 0xFF},
    {0xFF, 0xD1, 0x9A, 0xFF}, {0xFF, 0xF1, 0xD4, 0xFF},
    {0xC7, 0xB5, 0xD2, 0xFF}, {0x78, 0x6F, 0x9D, 0xFF},
};
static const Color REBOOT_DAWN[] = {
    {0x35, 0x34, 0x78, 0xFF}, {0xD4, 0x72, 0x82, 0xFF},
    {0x1C, 0x26, 0x58, 0xFF}, {0x69, 0x42, 0x76, 0xFF},
    {0xFF, 0xB3, 0x72, 0xFF}, {0xFF, 0xE6, 0xB0, 0xFF},
    {0x4D, 0xC6, 0xB6, 0xFF}, {0x36, 0x62, 0x76, 0xFF},
};
#define ATMOSPHERE_COLOR_COUNT (offsetof(Theme, mouth) / sizeof(Color))
STATIC_ASSERT(sizeof(BLUE_HOUR) / sizeof(BLUE_HOUR[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              blue_hour_full);
STATIC_ASSERT(sizeof(REBOOT_DAWN) / sizeof(REBOOT_DAWN[0]) ==
                  ATMOSPHERE_COLOR_COUNT,
              reboot_dawn_full);

static Theme renderedTheme(ThemeSample sample) {
  const Theme *a = &THEMES[sample.era], *b = &THEMES[sample.next];
  Theme r = lerpTheme(a, b, sample.blend);
  const Color *key = NULL;
  if (a->bridge == BRIDGE_TWILIGHT) {
    key = BLUE_HOUR;
  } else if (a->bridge == BRIDGE_REBOOT) {
    key = REBOOT_DAWN;
  }
  if (key != NULL) {
    float f = sample.blend * 2.0f;
    const Color *from = f < 1.0f ? (const Color *)a : key;
    const Color *to = f < 1.0f ? key : (const Color *)b;
    f = f < 1.0f ? f : f - 1.0f;
    Color *colors = (Color *)&r;
    for (int i = 0; i < (int)ATMOSPHERE_COLOR_COUNT; i++) {
      colors[i] = ColorLerp(from[i], to[i], f);
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

  // Post bloom follows the eased visual sample: stable through the core, then
  // descending smoothly across the reboot into the predawn garden.
  s = themeSampleAt(BAND_HEIGHT * (ERA_BLOCKS + 0.70f));
  assert(s.blend == 0.0f &&
         fabsf(themeGlow(s) - THEMES[ERA_BLOCKS].glow) < 0.0001f);
  s = themeSampleAt(BAND_HEIGHT *
                    (ERA_BLOCKS + (TRANSITION_START + 1.0f) * 0.5f));
  float rebootGlow = themeGlow(s);
  assert(fabsf(s.blend - 0.5f) < 0.0001f &&
         rebootGlow < THEMES[ERA_BLOCKS].glow &&
         rebootGlow > THEMES[ERA_GARDEN].glow);

  int rebootEdges = 0, bloomEdges = 0;
  for (int era = 0; era < ERA_COUNT; era++) {
    rebootEdges += THEMES[era].bridge == BRIDGE_REBOOT;
    bloomEdges += THEMES[era].bridge == BRIDGE_BLOOM;
  }
  assert(rebootEdges == 1 && THEMES[ERA_BLOCKS].bridge == BRIDGE_REBOOT);
  assert(bloomEdges == 1 && THEMES[ERA_GARDEN].bridge == BRIDGE_BLOOM);
  assert(THEMES[ERA_GARDEN].track == TRACK_HILLS);

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
         s.next == ERA_GARDEN && fabsf(s.blend - 0.5f) < 0.0001f);
  s.blend = 0.5f;
  themeUpdate(s);
  assert(gTheme.skyTop.r == REBOOT_DAWN[0].r &&
         gTheme.skyTop.g == REBOOT_DAWN[0].g &&
         gTheme.skyTop.b == REBOOT_DAWN[0].b &&
         gTheme.skyBot.r == REBOOT_DAWN[1].r &&
         gTheme.skyBot.g == REBOOT_DAWN[1].g &&
         gTheme.skyBot.b == REBOOT_DAWN[1].b);
}
#endif
