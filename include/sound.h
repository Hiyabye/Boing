#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Select the music/SFX track; starts a crossfade from the current one. Themes
// drive this via themeTrack() (see theme.h); no-op if already on `track`.
void setTrack(int track);

// Procedural sound effects (synthesized in C, mixed into the music stream).
void sfxCoin(void);
void sfxBounce(void);
void sfxPowerup(void);
void sfxHit(void);
void sfxSelect(void);
void sfxLevelUp(void);  // arrival at a new level/theme band
void sfxGameOver(void); // sad tone on a normal death
void sfxSplat(void);    // monster splattered (invincible plow-through)
void sfxPowerup2(void); // Balloon/Shield pickup variant

#endif // SOUND_H
