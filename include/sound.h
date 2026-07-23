#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Music track ids (indices into TRACKS[] in sound.c). 0/1 must stay
// Nature/Arcade — theme.c's THEMES[].track uses those bare literals. Menu and
// game over are FSM-driven (main.c), not themes.
typedef enum {
  TRACK_NATURE,   // 0
  TRACK_ARCADE,   // 1
  TRACK_MENU,     // 2 — title + credits screens
  TRACK_GAMEOVER, // 3 — game-over screen
} MusicTrack;

// Select the music/SFX track; starts a crossfade from the current one. Themes
// drive this via themeTrack() (see theme.h); no-op if already on `track`.
void setTrack(int track);

// Independent music / SFX levels (0..1), applied in the mix (settings.c drives
// these instead of a single SetMasterVolume so the two channels split).
void setVolumes(float music, float sfx);

// Procedural sound effects (synthesized in C, mixed into the music stream).
// Adding one = a new SfxId + a length in sfxLen[] + a case in renderVoice().
typedef enum {
  SFX_COIN,
  SFX_BOUNCE,
  SFX_POWERUP,
  SFX_HIT,
  SFX_SELECT,
  SFX_LEVELUP,  // arrival at a new level/theme band
  SFX_GAMEOVER, // sad tone on a normal death
  SFX_SPLAT,    // monster splattered (invincible plow-through)
  SFX_PICKUP,   // Balloon/Shield pickup variant
  SFX_COUNT
} SfxId;

void sfx(SfxId id); // trigger a one-shot effect

#endif // SOUND_H
