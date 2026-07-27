#ifndef WORLD_H
#define WORLD_H

#include "player.h"
#include "raylib.h"

#include <stdbool.h>

#define MAX_COINS 64
#define MAX_PLATFORMS 16
#define MAX_MONSTERS 8
#define MAX_POWERUPS 4
#define MAX_PARTICLES 64

typedef struct Coin {
  Vector2 position;
  bool active;
  Vector2 velocity; // usually {0}; set when knocked off by a monster hit
} Coin;

typedef struct Platform {
  Vector2 position; // top-left corner
  bool active;
} Platform;

typedef struct Monster {
  Vector2 position;
  float vx; // horizontal drift
  bool active;
} Monster;

typedef enum {
  POWERUP_MAGNET,
  POWERUP_ROCKET,
  POWERUP_BALLOON,
  POWERUP_SHIELD,
  // Debuffs (bad pickups to dodge): flagged `debuff` in POWERUP_DEFS.
  POWERUP_ANCHOR, // heavier gravity, falls faster
  POWERUP_DIZZY,  // reversed steering
  POWERUP_FOG,    // screen dims, cuts visibility
  POWERUP_BUTTER, // butterfingers: coins slip through, can't collect
  POWERUP_COUNT
} PowerUpType;

// Per-type behaviour, in one place so adding a power-up (Balloon/Shield/…) is a
// new enum value + one table row — no hunting across spawn/collect/HUD sites.
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

extern const PowerUpDef POWERUP_DEFS[POWERUP_COUNT];

typedef struct PowerUp {
  Vector2 position;
  PowerUpType type;
  bool active;
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
  float cameraY; // top of view in world coords; only decreases (moves up)
  float spawnY;  // next height at which to place a coin
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
  int level; // 0-based theme band entered; a level-up fanfare fires on increase
  int combo; // coins chained without falling to the floor / taking a hit
  float comboTimer; // frames left before an idle chain auto-resets
  int bestCombo;    // longest chain this run (game-over summary)
  int comboBonus;   // accumulated combo bonus points, folded into getScore
  int hitstop;      // frames of physics freeze remaining (impact juice)
} World;

World createWorld(void);
void updateWorld(World *world, Player *player);
void drawGround(void);
void drawWorld(const World *world);
void drawPowerAura(const World *world, const Player *player);
// Glyph-only power-up icon (no halo/bob) — reused by the SHOP upgrade cards.
void drawPowerupGlyph(PowerUpType type, Vector2 c, float R, float t);
bool isPlayerFallen(const World *world, const Player *player);
int getScore(const World *world);

// Accessors so callers (HUD/camera/loop) read state without poking World
// fields. (C can't fully hide a stack struct without a heap/opaque pointer —
// overkill here — so the fields stay public; these just keep main.c from
// reaching in.)
float worldCameraY(const World *world);
float worldShake(const World *world);
int worldLevel(const World *world); // 0-based theme band the climb has entered
float worldPowerTimer(const World *world, PowerUpType type);
bool worldInvincible(const World *world); // any active power-up grants immunity
float worldInputDir(
    const World *world); // -1 while Dizzy reverses steering, else +1
float worldFog(
    const World *world); // Fog timer (0 = clear), drives the post-pass murk

// Purchased per-powerup upgrade levels (indexed by PuTrack), set once per run
// from saved progress before createWorld. Drives the spawn/effect/duration
// scaling and the debuff-suppress roll inside world.c.
void worldSetUpgrades(const int *puLevel);

#endif // WORLD_H
