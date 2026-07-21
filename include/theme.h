#ifndef THEME_H
#define THEME_H

#include "raylib.h"

// The palette is runtime state now: a game can hold several themes and morph
// between them as the player climbs (see theme.c / THEMES[]). `gTheme` is the
// blended palette for the *current frame*; themeUpdate() recomputes it from the
// ascent. The color macros below just read `gTheme`, so every existing inline
// draw site (world.c / player.c / background.c) keeps working unchanged.

typedef enum { BACKDROP_CLOUDS, BACKDROP_BLOCKS } BackdropStyle;

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
  // Player (copied into Character at spawn, so the protagonist stays constant).
  Color candyPink, candyPinkDk, mouth;
  // Coins, ground, monsters, power-ups.
  Color coinHi, coinLo, grass, grassDk, grape, grapeDk;
  Color magnetRed, rocketBody, rocketFin, flame;
  // Particle-burst colors.
  Color confetti[6];
  // --- non-Color fields must stay after every Color above ---
  BackdropStyle backdrop;
  int track; // music/SFX track id for this theme
} Theme;

// The blended palette for the current frame; the macros below read it.
extern Theme gTheme;

// The theme table and its size (defined in theme.c).
extern const Theme THEMES[];
extern const int THEME_COUNT;

// Recompute `gTheme` for the given ascent (-cameraY; 0 at the ground).
void themeUpdate(float ascent);
// The two neighbor theme indices and blend factor f (0..1) from the last
// themeUpdate — used by the background to crossfade backdrop shapes.
void themeBlend(int *lo, int *hi, float *f);
// The music/SFX track for the current blend (switches at the band midpoint).
int themeTrack(void);

// Color macros read the runtime blended palette — call sites are unchanged.
#define SKY_TOP (gTheme.skyTop)
#define SKY_BOT (gTheme.skyBot)
#define SKY_TOP_HIGH (gTheme.skyTopHigh)
#define SKY_BOT_HIGH (gTheme.skyBotHigh)
#define SUN (gTheme.sun)
#define SUN_CORE (gTheme.sunCore)
#define CLOUD (gTheme.cloud)
#define CLOUD_SHADE (gTheme.cloudShade)
#define CANDY_PINK (gTheme.candyPink)
#define CANDY_PINK_DK (gTheme.candyPinkDk)
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
