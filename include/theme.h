#ifndef THEME_H
#define THEME_H

#include "raylib.h"
#include "sound.h"

// The palette is runtime state: a game can hold several themes and morph
// between them as the player climbs (see theme.c / THEMES[]). `gTheme` is the
// blended palette for the current draw sample; scene-color macros read it while
// the UI accent macros read the selected outfit colors instead.

// Stable gameplay eras in climb order. Level is unbounded, while EraId wraps
// through this closed loop.
typedef enum {
  ERA_HILLS,
  ERA_SKIES,
  ERA_NIGHT,
  ERA_MAZE,
  ERA_BLOCKS,
  ERA_GARDEN,
  ERA_COUNT
} EraId;

// One pure reading of the closed-loop journey at an ascent. `level` is
// monotonic and unbounded; only `era` and `next` wrap. `phase` is raw progress
// through the current era. `blend` is a smooth visual weight held at zero
// through the stable core, then eased to one across the exit transition.
typedef struct {
  int level;
  int cycle;
  EraId era;
  EraId next;
  float phase;
  float blend;
} ThemeSample;

// Authored outgoing relationship for each era. Bridge geometry and optional
// atmosphere-only palette keys render between eras without becoming eras.
typedef enum {
  BRIDGE_CLOUDLINE,
  BRIDGE_TWILIGHT,
  BRIDGE_SIGNAL,
  BRIDGE_FRAGMENTATION,
  BRIDGE_REBOOT,
  BRIDGE_BLOOM,
  BRIDGE_COUNT
} BridgeStyle;

typedef enum {
  BACKDROP_HILLS,  // day sky, clouds, and rolling green hills
  BACKDROP_SKIES,  // open blue sky, high clouds, and flying birds
  BACKDROP_STARS,  // night sky, moon, pixel stars, and blocky terrain
  BACKDROP_MAZE,   // neon maze lattice and pellet dots on dark ground
  BACKDROP_BLOCKS, // falling neon tetromino cells
  BACKDROP_GARDEN  // rooted cells, terraced fields, and predawn growth
} BackdropStyle;

#define CONFETTI_COUNT 6

// One theme. Every Color must come before `backdrop` - themeUpdate() blends the
// whole leading run of Colors generically (offsetof(Theme, backdrop)), so a new
// color field needs no change to the lerp.
typedef struct {
  // Sky (top/bottom gradient) + a deeper pair kept for shaded accents.
  Color skyTop, skyBot, skyTopHigh, skyBotHigh;
  // Sun / celestial glow.
  Color sun, sunCore;
  // Backdrop decor body + shade (clouds for nature, blocks for arcade).
  Color cloud, cloudShade;
  // Shared procedural face-line color. The UI accent follows the selected
  // outfit instead, so it lives separately from the altitude-based palette.
  Color mouth;
  // Coins, ground, monsters, power-ups.
  Color coinHi, coinLo, grass, grassDk, grape, grapeDk;
  Color magnetRed, rocketBody, rocketFin, flame;
  // Particle-burst colors.
  Color confetti[CONFETTI_COUNT];
  // --- non-Color fields must stay after every Color above ---
  BackdropStyle backdrop;
  BridgeStyle bridge;
  MusicTrack track; // active cue also selects the SFX palette
  const char *name;
  float glow;
} Theme;

// The blended palette for the current frame, plus the selected outfit colors
// used by UI_ACCENT. setPlayerCharacter() keeps the latter pair in sync.
extern Theme gTheme;
extern Color gUiAccent, gUiAccentDark;

// The theme table (defined in theme.c; ERA_COUNT rows).
extern const Theme THEMES[];

// Pure journey samplers. World Y follows the shared convention:
// ascent = max(0, SCREEN_HEIGHT/2 - worldY).
ThemeSample themeSampleAt(float ascent);
ThemeSample themeSampleAtWorldY(float worldY);

// Recompute the temporary rendered palette from an explicit sample.
void themeUpdate(ThemeSample sample);
// Post-processing/additive glow weight for the sample's era pair and blend.
float themeGlow(ThemeSample sample);

#ifdef BG_SELFTEST
// Boundary checks run as part of the existing background self-test.
void themeSampleSelfTest(void);
#endif

// Scene colors read the blended palette; UI accents follow the selected outfit.
#define SKY_TOP (gTheme.skyTop)
#define SKY_BOT (gTheme.skyBot)
#define SKY_TOP_HIGH (gTheme.skyTopHigh)
#define SKY_BOT_HIGH (gTheme.skyBotHigh)
#define SUN (gTheme.sun)
#define SUN_CORE (gTheme.sunCore)
#define CLOUD (gTheme.cloud)
#define CLOUD_SHADE (gTheme.cloudShade)
#define UI_ACCENT (gUiAccent)
#define UI_ACCENT_DK (gUiAccentDark)
#define MOUTH (gTheme.mouth)
#define COIN_HI (gTheme.coinHi)
#define COIN_LO (gTheme.coinLo)
#define GRASS (gTheme.grass)
#define GRASS_DK (gTheme.grassDk)
#define GRAPE (gTheme.grape)
#define GRAPE_DK (gTheme.grapeDk)
#define MAGNET_RED (gTheme.magnetRed)
#define ROCKET_BODY (gTheme.rocketBody)
#define ROCKET_FIN (gTheme.rocketFin)
#define FLAME (gTheme.flame)

#endif // THEME_H
