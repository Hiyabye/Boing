#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Prerender one not-yet-baked music track per call (each is heavy — millions of
// samples). Returns true once every track is baked. The startup splash drives
// this so the load streams in behind the logo instead of freezing the window;
// until a track is baked it plays as silence (updateAudio guards it).
bool audioPreloadStep(void);

// Music track ids (indices into TRACKS[] in sound.c). 0..3 are the four level
// tracks (theme.c's THEMES[].track uses those bare literals, one per climb
// band); menu and game over are FSM-driven (main.c), not themes.
typedef enum {
  TRACK_L1,       // 0 — Sunny Hills (day)
  TRACK_L2,       // 1 — Night Blocks
  TRACK_L3,       // 2 — Neon Maze
  TRACK_L4,       // 3 — Falling Blocks
  TRACK_MENU,     // 4 — title + credits screens
  TRACK_GAMEOVER, // 5 — game-over screen
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
  SFX_NEWBEST,  // new high score jingle (game-over screen)
  SFX_COUNT
} SfxId;

void sfx(SfxId id); // trigger a one-shot effect
// Same, pitched: `pitch` scales every frequency (1.0 = normal). Drives the
// rising coin-combo blip.
void sfxPitched(SfxId id, float pitch);

#endif // SOUND_H
