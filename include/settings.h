#ifndef SETTINGS_H
#define SETTINGS_H

#include "core.h"

// Player-adjustable options: target framerate, window resolution, and separate
// music / SFX volumes. Persisted as keys in the shared "save.json" beside the
// exe (via persist), so it's runtime-generated and doesn't count against the budget.
typedef struct {
  int fpsIndex;      // index into the framerate options (see settings.c)
  int resIndex;      // index into the resolution options (square window sizes)
  float musicVolume; // 0..1, background music level
  float sfxVolume;   // 0..1, sound-effects level
} Settings;

Settings loadSettings(void);
void saveSettings(const Settings *s);
void applySettings(const Settings *s); // push to raylib (FPS/window/volume)

// Settings screen. Mouse drags each row's slider knob; keyboard Up/Down pick a
// row and Left/Right nudge it one stop; Esc/Space or BACK exits. Sets *state to
// returnTo on exit (menu or pause).
void updateSettingsMenu(Settings *s, GameState *state, GameState returnTo);
void drawSettingsMenu(const Settings *s);

#endif // SETTINGS_H
