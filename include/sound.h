#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"

void initAudio(AudioStream *stream);
void updateAudio(AudioStream *stream);
void deinitAudio(AudioStream *stream);

// Start/poll the soundtrack bake worker. Splash calls this until all cues are
// ready, keeping expensive synthesis off both the frame and audio stream paths.
bool audioPreloadStep(void);

// Gameplay tracks have semantic ids; each Theme selects one explicitly.
typedef enum {
  TRACK_HILLS,
  TRACK_SKIES,
  TRACK_NIGHT,
  TRACK_MAZE,
  TRACK_BLOCKS,
  TRACK_GAMEPLAY_COUNT,
  TRACK_MENU = TRACK_GAMEPLAY_COUNT, // title/help/missions/settings
  TRACK_SHOP,    // beat-synchronized variation of the menu theme
  TRACK_CREDITS, // dedicated reflective finale
  TRACK_RESULTS, // quiet game-over results bed
  TRACK_COUNT
} MusicTrack;
#define TRACK_NONE (-1)

// Select a cue. Gameplay cues enter on their composed downbeat; MENU and SHOP
// preserve their shared timeline so navigation between them stays seamless.
void setTrack(int track);
// PAUSED and pause-opened settings keep the current era cue running, but strip
// it to a quieter filtered bed instead of starting another song.
void setMusicPaused(bool paused);

// Live gameplay layers. Combo adds era-specific energy, danger adds a low
// pulse, and finale raises the Falling Blocks countermelody. Inputs are
// clamped.
void setMusicDynamics(int combo, float danger, float finale);
void musicMonsterHit(void);
// Play the level-arrival stinger in the destination gameplay track's palette.
void musicEraTransition(MusicTrack track);
void musicLogoSting(void);

// Independent buses, both 0..1.
void setVolumes(float music, float sfxVolume);

// Every gameplay/UI event has one semantic id. The renderer captures the active
// era when triggered, then gives that event the era's own instrument palette.
typedef enum {
  SFX_COIN,
  SFX_BOUNCE,
  SFX_CRUMBLE,
  SFX_MAGNET,
  SFX_ROCKET,
  SFX_BALLOON,
  SFX_SHIELD,
  SFX_PELLET,
  SFX_DEBUFF,
  SFX_HIT,
  SFX_SPLAT,
  SFX_SELECT,
  SFX_PURCHASE,
  SFX_DENIED,
  SFX_LEVELUP,
  SFX_GAMEOVER,
  SFX_NEWBEST,
  SFX_STATIC,
  SFX_LOGO,
  SFX_COUNT
} SfxId;

void sfx(SfxId id);
void sfxPitched(SfxId id, float pitch);
// `pan` is -1 left .. 1 right. World events use their screen x; UI stays
// centered through sfx()/sfxPitched().
void sfxAt(SfxId id, float pitch, float pan);

#endif // SOUND_H
