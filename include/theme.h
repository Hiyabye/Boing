#ifndef THEME_H
#define THEME_H

#include "raylib.h"

// The palette is runtime state now: a game can hold several themes and morph
// between them as the player climbs (see theme.c / THEMES[]). `gTheme` is the
// blended palette for the *current frame*; themeUpdate() recomputes it from the
// ascent. Scene-color macros read `gTheme`; the UI accent macros read the
// selected outfit colors instead.

typedef enum {
  BACKDROP_HILLS, // L1: day sky, clouds + rolling green hills/bushes
  BACKDROP_SKIES, // L2: open blue sky, high clouds + flying birds
  BACKDROP_STARS, // L3: night sky, moon + pixel stars + blocky terrain
  BACKDROP_MAZE,  // L4: neon maze lattice + pellet dots on a dark ground
  BACKDROP_BLOCKS // L5: falling neon tetromino cells
} BackdropStyle;

// Number of rows in THEMES[] (theme.c static-asserts the table matches) and the
// per-theme particle-color count. Macros, not `const int`, so tables keyed off
// the theme list can be checked at compile time — see ERA_NAMES in
// screen_play.c, which must stay one name per theme.
#define THEME_COUNT 5
#define CONFETTI_COUNT 6

// One theme. Every Color must come before `backdrop` — themeUpdate() blends the
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
  int track; // music/SFX track id for this theme
} Theme;

// The blended palette for the current frame, plus the selected outfit colors
// used by UI_ACCENT. setPlayerCharacter() keeps the latter pair in sync.
extern Theme gTheme;
extern Color gUiAccent, gUiAccentDark;

// The theme table (defined in theme.c; THEME_COUNT rows).
extern const Theme THEMES[];

// Recompute `gTheme` for the given ascent (-cameraY; 0 at the ground).
void themeUpdate(float ascent);
// The two neighbor theme indices and blend factor f (0..1) from the last
// themeUpdate — used by the background to crossfade backdrop shapes.
void themeBlend(int *lo, int *hi, float *f);
// The music/SFX track for the current level (switches at the band boundary).
int themeTrack(void);
// Post-processing bloom weight (0..1) for the current blend - subtle in
// daylight, full in the neon level, blended across the band like the palette.
float themeGlow(void);
// 0-based level fully entered at `ascent` (see theme.c); unbounded - themes
// loop once past the last band (nature->arcade->nature->...).
int themeLevelAt(float ascent);
// Progress through that level's band (0..1). Kept here so gameplay/audio does
// not duplicate the private band-height tuning constant.
float themeBandProgressAt(float ascent);

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
