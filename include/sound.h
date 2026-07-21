#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Procedural sound effects (synthesized in C, mixed into the music stream).
void sfxCoin(void);
void sfxBounce(void);
void sfxPowerup(void);
void sfxHit(void);
void sfxSelect(void);

#endif // SOUND_H
