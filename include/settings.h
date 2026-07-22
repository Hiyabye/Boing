#ifndef SETTINGS_H
#define SETTINGS_H

#include "core.h"

// Player-adjustable options: target framerate, window resolution, master
// volume. Persisted to a plain text file beside the exe (like the highscore),
// so it's runtime-generated and doesn't count against the 1.44MB budget.
typedef struct {
  int fpsIndex; // index into the framerate options (see settings.c)
  int resIndex; // index into the resolution options (square window sizes)
  float volume; // 0..1, master volume
} Settings;

Settings loadSettings(void);
void saveSettings(const Settings *s);
void applySettings(const Settings *s); // push to raylib (FPS/window/volume)

// Keyboard-driven settings screen. Up/Down pick a row, Left/Right change it,
// Esc/Space exits. Sets *state to returnTo on exit (menu or pause).
void updateSettingsMenu(Settings *s, GameState *state, GameState returnTo);
void drawSettingsMenu(const Settings *s);

#endif // SETTINGS_H
