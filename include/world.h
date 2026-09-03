#ifndef WORLD_H
#define WORLD_H

#include "player.h"
#include "raylib.h"
#include "theme.h"

#include <stdbool.h>

#define MAX_COINS 64
#define MAX_PLATFORMS 16
#define MAX_MONSTERS 8
#define MAX_POWERUPS 4
#define MAX_PARTICLES 64

typedef struct Coin {
  Vector2 position;
  bool active;
  unsigned char era; // immutable EraId sampled from spawn world Y
  Vector2 velocity;  // usually {0}; set when knocked off by a monster hit
} Coin;

typedef struct Platform {
  Vector2 position; // top-left cell's corner
  bool active;
  // Per-era behavior is stamped once from spawn world Y. The compact era tag
  // identifies the source chapter; these concrete fields remain authoritative.
  bool fragile;      // Night Blocks: crumbles after it is landed on
  unsigned char era; // immutable EraId
  float fuse; // frames left before a landed-on fragile pad breaks (0 = intact)
  float vx;   // Birdsong Skies: horizontal cloud drift, reflected at the edges
  float vy;   // Falling Blocks: downward drift; > 0 also makes the pad crush
  int shape; // Moving Blocks index into TETRO[] for art + collision. Ignored by
             // collision at vy == 0, including a reboot-locked one-row board.
  float pressure; // immutable 0..1 arrival/relief tuning; also organic growth
} Platform;

typedef struct Monster {
  Vector2 position;
  float vx; // horizontal drift
  bool active;
  unsigned char era; // immutable EraId sampled from spawn world Y
  float vy;      // vertical drift — 0 except while ghost-chasing on Neon Maze
  unsigned seed; // spawn-time look (horns/eyes/mouth/legs). A field, not a hash
                 // of the live position like entSeed: a ghost *moves*, and a
                 // seed derived from y re-rolls its whole face every frame.
  float pressure; // immutable 0..1 steering pressure sampled at spawn
} Monster;

typedef enum {
  POWERUP_MAGNET,
  POWERUP_ROCKET,
  POWERUP_BALLOON,
  POWERUP_SHIELD,
  POWERUP_PELLET, // Neon Maze only: frightens the ghosts so you can eat them
  // Debuffs (bad pickups to dodge): flagged `debuff` in POWERUP_DEFS.
  POWERUP_ANCHOR, // heavier gravity, falls faster
  POWERUP_DIZZY,  // reversed steering
  POWERUP_FOG,    // blurs and hazes the screen, cutting visibility
  POWERUP_BUTTER, // butterfingers: coins slip through, can't collect
  POWERUP_COUNT
} PowerUpType;

// Per-type behaviour, in one place so adding a power-up is a new enum value +
// one definition row + one POWERUP_SFX row, not a hunt across call sites.
// The moment-to-moment *effect* (magnet pull, rocket thrust) is still coded
// per-type in world.c; the table owns everything else.
typedef struct PowerUpDef {
  float duration;    // frames @60fps the effect lasts
  Color color;       // HUD timer-bar color
  const char *label; // HUD timer-bar label
  int spawnWeight;   // relative spawn frequency vs other types
  float pickupShake; // screen shake on pickup
  bool invincible;   // grants monster immunity while active (worldInvincible)
  bool debuff;       // bad pickup: sad reaction, no boost (see collectPowerups)
} PowerUpDef;

// Declared unsized deliberately. Spelling the length here would give the
// definition in world.c a composite type of exactly that length, so its
// completeness STATIC_ASSERT would measure the declaration rather than the
// positional table and could never fail. Callers index it by PowerUpType.
extern const PowerUpDef POWERUP_DEFS[];

typedef struct PowerUp {
  Vector2 position;
  PowerUpType type;
  bool active;
  unsigned char era; // immutable EraId sampled from spawn world Y
} PowerUp;

typedef struct Particle {
  Vector2 position;
  Vector2 velocity;
  float life; // 1 at birth, decays to 0
  Color color;
} Particle;

typedef struct World {
  Coin coins[MAX_COINS];
  Platform platforms[MAX_PLATFORMS];
  Monster monsters[MAX_MONSTERS];
  PowerUp powerups[MAX_POWERUPS];
  Particle particles[MAX_PARTICLES];
  // Top of view in world coords. Tracks the climb upward, but can chase a fall
  // back down as far as DEATH_DROP past the peak, where the run ends.
  float cameraY;
  float spawnY;         // next height at which to place a coin
  float platformSpawnY; // next height at which to place a platform
  float monsterSpawnY;  // next height at which to consider a monster
  float powerupSpawnY;  // next height at which to consider a power-up
  float lastSpawnX;     // previous coin x, so coins form a followable trail
  float bestY;          // highest point the player has reached (smallest y)
  float powerTimer[POWERUP_COUNT]; // frames of each power-up remaining
  int shieldHits;   // monster hits the active Shield still absorbs
  float shake;      // screen-shake magnitude, decays each frame
  Vector2 flashPos; // where the last collect "pop" ring is centered
  float flash;      // pop-ring life, 1 at collect -> 0
  int coinsCollected;
  int level; // highest peak-sample level reached; used by end-of-run tasks
  int combo; // coins chained without falling to the floor / taking a hit
  float comboTimer; // frames left before an idle chain auto-resets
  int bestCombo;    // longest chain this run (game-over summary)
  int comboBonus;   // accumulated combo bonus points, folded into getScore
  int hitstop;      // frames of physics freeze remaining (impact juice)
  int ghostChain;   // monsters eaten on the current pellet (doubling bonus)
} World;

World createWorld(void);
void updateWorld(World *world, Player *player);
void drawGround(void);
void drawWorld(const World *world);
// Additive living glow for active effects. Draw before the player so the aura
// lights its silhouette without washing out the face.
void drawPowerAura(const World *world, const Player *player);
// Glyph-only power-up icon (no halo/bob) — reused by the SHOP upgrade cards.
void drawPowerupGlyph(PowerUpType type, Vector2 c, float R, float t);
bool isPlayerFallen(const World *world, const Player *player);
int getScore(const World *world);

// Named accessors for common/derived reads at module boundaries. The stack
// struct stays public because making it opaque would require heap ownership;
// callers still read a few plain fields when composing HUD or audio state.
float worldCameraY(const World *world);
// Monotonic highest ascent reached, and the camera-relative ascent currently
// in view. Both use the same center-world-Y convention as themeSampleAtWorldY.
float worldPeakAscent(const World *world);
float worldViewAscent(const World *world);
float worldShake(const World *world);
int worldLevel(const World *world); // 0-based monotonic peak-sample level
float worldPowerTimer(const World *world, PowerUpType type);
bool worldInvincible(const World *world); // active immunity-granting power-up
float worldInputDir(
    const World *world); // -1 while Dizzy reverses steering, else +1
float worldFog(
    const World *world); // Fog timer (0 = clear), drives the post-pass murk

// Purchased per-powerup upgrade levels (indexed by PuTrack), set once per run
// from saved progress before createWorld. Drives the spawn/effect/duration
// scaling and the debuff-suppress roll inside world.c.
void worldSetUpgrades(const int *puLevel);

#endif // WORLD_H
