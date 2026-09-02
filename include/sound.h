#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Prerender one not-yet-baked music track per call (each is millions of
// samples), with menu music first. main.c drives this every frame so remaining
// tracks bake behind the splash-to-menu dip instead of delaying the menu. A
// track not baked yet plays as silence (updateAudio guards it).
bool audioPreloadStep(void);

// Music track ids (indices into TRACKS[] in sound.c). 0..4 are the five level
// tracks (theme.c's THEMES[].track names those ids, one per climb band); menu
// and game over are FSM-driven (main.c), not themes.
typedef enum {
  TRACK_L1,       // 0 - Sunny Hills (day)
  TRACK_L2,       // 1 - Birdsong Skies
  TRACK_L3,       // 2 - Night Blocks
  TRACK_L4,       // 3 - Neon Maze
  TRACK_L5,       // 4 - Falling Blocks
  TRACK_MENU,     // 5 - title + credits screens
  TRACK_GAMEOVER, // 6 - game-over screen
  TRACK_COUNT
} MusicTrack;

// Select the music track and its SFX variant. Level switches crossfade; menu
// and game-over switches cut immediately. No-op if already set.
void setTrack(int track);

// Live gameplay layers mixed over the baked theme track. `danger` and `finale`
// are 0..1; combo progressively adds percussion. Non-playing states pass zero.
void setMusicDynamics(int combo, float danger, float finale);
// Event accents kept separate from generic SFX so only a real monster hit
// strips the arrangement, while each arriving era gets its own stinger pitch.
void musicMonsterHit(void);
void musicEraTransition(int era);

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
