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

// Power-up durations (frames @60fps). Public so the HUD timer bar can compute
// its fill fraction without duplicating them.
#define MAGNET_DURATION 300.0f // ~5s
#define ROCKET_DURATION 90.0f  // ~1.5s

typedef struct Coin {
  Vector2 position;
  bool active;
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

typedef enum { POWERUP_MAGNET, POWERUP_ROCKET } PowerUpType;

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
  float magnetTimer;    // frames of coin-magnet remaining
  float rocketTimer;    // frames of rocket thrust remaining
  float shake;          // screen-shake magnitude, decays each frame
  Vector2 flashPos;     // where the last collect "pop" ring is centered
  float flash;          // pop-ring life, 1 at collect -> 0
  int coinsCollected;
  bool dead; // hit a monster
} World;

World createWorld(void);
void updateWorld(World *world, Player *player);
void drawGround(void);
void drawWorld(const World *world);
bool isPlayerFallen(const World *world, const Player *player);
int getScore(const World *world);

#endif // WORLD_H
