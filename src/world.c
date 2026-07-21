#include "world.h"

#include "core.h"
#include "sound.h"
#include "theme.h"

#include <math.h>

// Tuning knobs — hand-tune after playtesting.
#define BOUNCE_VELOCITY -37.0f // floor bounce at the start of a run
#define COIN_BOOST -20.0f      // upward kick per coin collected
#define COIN_RADIUS 12.0f
#define COIN_SPACING 55.0f // vertical distance between spawned coins
#define COIN_WANDER 150.0f // max horizontal step between consecutive coins
#define SPAWN_MARGIN 40.0f // keep spawns away from screen edges
#define PLATFORM_BOUNCE -28.0f
#define PLATFORM_SPACING 300.0f // vertical distance between platforms
#define PLATFORM_WIDTH 100.0f
#define PLATFORM_HEIGHT 16.0f
#define HEIGHT_PER_POINT 10.0f
#define POINTS_PER_COIN 5

#define MONSTER_RADIUS 24.0f
#define MONSTER_SPACING 900.0f // vertical distance between monster bands
#define MONSTER_DRIFT 1.5f     // horizontal speed
#define MONSTER_COIN_LOSS 5    // coins scattered/lost on a monster hit
#define POWERUP_RADIUS 18.0f
#define POWERUP_SPACING 1500.0f // rare
#define MAGNET_RADIUS 260.0f    // coins within this get pulled
#define MAGNET_PULL 6.0f        // px/frame toward the player
#define ROCKET_VELOCITY -34.0f  // upward thrust while active
#define PARTICLE_GRAVITY 0.3f
#define PARTICLE_DECAY 0.03f
#define SHAKE_HIT 14.0f   // screen shake on a monster hit
#define SHAKE_LAUNCH 6.0f // shake on floor bounce / rocket pickup
#define SHAKE_DECAY 0.85f // per-frame shake falloff
#define FLASH_DECAY 0.08f // pop-ring fade per frame
#define DEATH_DROP                                                             \
  450.0f // how far below the peak view the camera chases a fall

// Power-up table (indexed by PowerUpType). Ordered exactly as the enum.
//              duration color       label     weight shake        invincible
const PowerUpDef POWERUP_DEFS[POWERUP_COUNT] = {
    // HUD timer-bar colors are fixed (not themed): the table is static config.
    [POWERUP_MAGNET] = {600.0f, SKYBLUE, "MAGNET", 1, 0.0f, false}, // ~10s
    [POWERUP_ROCKET] = {120.0f, ORANGE, "ROCKET", 1, SHAKE_LAUNCH, true}, // ~2s
};

float worldCameraY(const World *world) { return world->cameraY; }
float worldShake(const World *world) { return world->shake; }
bool worldDead(const World *world) { return world->dead; }
int worldLevel(const World *world) { return world->level; } // 0-based band

float worldPowerTimer(const World *world, PowerUpType type) {
  return world->powerTimer[type];
}

bool worldInvincible(const World *world) {
  for (int t = 0; t < POWERUP_COUNT; t++) {
    if (world->powerTimer[t] > 0.0f && POWERUP_DEFS[t].invincible) {
      return true;
    }
  }
  return false;
}

World createWorld(void) {
  World world = {0};
  world.spawnY = FLOOR_Y - 150.0f; // first coins reachable from the floor
  world.platformSpawnY = FLOOR_Y - PLATFORM_SPACING;
  world.monsterSpawnY = FLOOR_Y - MONSTER_SPACING;
  world.powerupSpawnY = FLOOR_Y - POWERUP_SPACING;
  world.lastSpawnX = SCREEN_WIDTH / 2.0f;
  world.bestY = FLOOR_Y;
  return world;
}

static void spawnCoins(World *world) {
  // Keep one screen of coins pre-generated above the view.
  while (world->spawnY > world->cameraY - SCREEN_HEIGHT) {
    for (int i = 0; i < MAX_COINS; i++) {
      if (world->coins[i].active) {
        continue;
      }
      float x = world->lastSpawnX + GetRandomValue(-COIN_WANDER, COIN_WANDER);
      if (x < SPAWN_MARGIN) {
        x = SPAWN_MARGIN;
      } else if (x > SCREEN_WIDTH - SPAWN_MARGIN) {
        x = SCREEN_WIDTH - SPAWN_MARGIN;
      }
      world->coins[i] = (Coin){{x, world->spawnY}, true};
      world->lastSpawnX = x;
      break;
    }
    world->spawnY -= COIN_SPACING;
  }
}

static void spawnPlatforms(World *world) {
  while (world->platformSpawnY > world->cameraY - SCREEN_HEIGHT) {
    for (int i = 0; i < MAX_PLATFORMS; i++) {
      if (world->platforms[i].active) {
        continue;
      }
      float x = GetRandomValue(SPAWN_MARGIN,
                               SCREEN_WIDTH - SPAWN_MARGIN - PLATFORM_WIDTH);
      world->platforms[i] = (Platform){{x, world->platformSpawnY}, true};
      break;
    }
    world->platformSpawnY -= PLATFORM_SPACING;
  }
}

static void spawnMonsters(World *world) {
  while (world->monsterSpawnY > world->cameraY - SCREEN_HEIGHT) {
    if (GetRandomValue(0, 4) == 0) { // ~1 in 5 bands actually gets a monster
      for (int i = 0; i < MAX_MONSTERS; i++) {
        if (world->monsters[i].active) {
          continue;
        }
        float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
        float vx = GetRandomValue(0, 1) ? MONSTER_DRIFT : -MONSTER_DRIFT;
        world->monsters[i] = (Monster){{x, world->monsterSpawnY}, vx, true};
        break;
      }
    }
    world->monsterSpawnY -= MONSTER_SPACING;
  }
}

static void spawnPowerups(World *world) {
  while (world->powerupSpawnY > world->cameraY - SCREEN_HEIGHT) {
    for (int i = 0; i < MAX_POWERUPS; i++) {
      if (world->powerups[i].active) {
        continue;
      }
      float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
      // Weighted pick over POWERUP_DEFS[].spawnWeight.
      int total = 0;
      for (int k = 0; k < POWERUP_COUNT; k++) {
        total += POWERUP_DEFS[k].spawnWeight;
      }
      int roll = GetRandomValue(0, total - 1);
      PowerUpType t = 0;
      while (t < POWERUP_COUNT - 1 && roll >= POWERUP_DEFS[t].spawnWeight) {
        roll -= POWERUP_DEFS[t].spawnWeight;
        t++;
      }
      world->powerups[i] = (PowerUp){{x, world->powerupSpawnY}, t, true};
      break;
    }
    world->powerupSpawnY -= POWERUP_SPACING;
  }
}

static void spawnParticles(World *world, Vector2 at, Color color, int count,
                           bool confetti) {
  int made = 0;
  for (int i = 0; i < MAX_PARTICLES && made < count; i++) {
    if (world->particles[i].life > 0.0f) {
      continue;
    }
    float a = GetRandomValue(0, 628) / 100.0f; // 0..2π
    float sp = GetRandomValue(2, 7);
    Color c =
        confetti ? gTheme.confetti[GetRandomValue(0, 5)] : color; // 6 colors
    world->particles[i] = (Particle){
        at, {cosf(a) * sp, sinf(a) * sp - 2.0f}, 1.0f, c}; // slight pop upward
    made++;
  }
}

// Deactivate any element of world->arr[] that has dropped `margin` past the
// bottom cull line. Assumes locals `world` and `cullY`.
#define CULL(arr, count, margin)                                               \
  for (int i = 0; i < (count); i++) {                                          \
    if (world->arr[i].active && world->arr[i].position.y > cullY + (margin)) { \
      world->arr[i].active = false;                                            \
    }                                                                          \
  }

static void cullBelowView(World *world) {
  float cullY = world->cameraY + SCREEN_HEIGHT + COIN_RADIUS;
  CULL(coins, MAX_COINS, 0.0f);
  CULL(platforms, MAX_PLATFORMS, PLATFORM_HEIGHT);
  CULL(monsters, MAX_MONSTERS, MONSTER_RADIUS);
  CULL(powerups, MAX_POWERUPS, POWERUP_RADIUS);
}
#undef CULL

static void collectCoins(World *world, Player *player) {
  for (int i = 0; i < MAX_COINS; i++) {
    if (!world->coins[i].active) {
      continue;
    }
    if (CheckCollisionCircles(player->position, player->radius,
                              world->coins[i].position, COIN_RADIUS)) {
      spawnParticles(world, world->coins[i].position, WHITE, 14, true);
      world->flashPos = world->coins[i].position;
      world->flash = 1.0f;
      world->coins[i].active = false;
      world->coinsCollected++;
      player->velocity.y = COIN_BOOST;
      player->expression = EXPR_HAPPY;
      sfxCoin();
    }
  }
}

// Coin magnet: while active, drag nearby coins toward the player so the
// existing overlap check sweeps them up next frame.
static void applyMagnet(World *world, const Player *player) {
  for (int i = 0; i < MAX_COINS; i++) {
    if (!world->coins[i].active) {
      continue;
    }
    Vector2 c = world->coins[i].position;
    float dx = player->position.x - c.x;
    float dy = player->position.y - c.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 0.0f && d < MAGNET_RADIUS) {
      world->coins[i].position.x += dx / d * MAGNET_PULL;
      world->coins[i].position.y += dy / d * MAGNET_PULL;
    }
  }
}

static void collectPowerups(World *world, Player *player) {
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (!world->powerups[i].active) {
      continue;
    }
    if (CheckCollisionCircles(player->position, player->radius,
                              world->powerups[i].position, POWERUP_RADIUS)) {
      PowerUpType t = world->powerups[i].type;
      // Same collect juice as a coin: pop particles, flash, and an upward kick.
      spawnParticles(world, world->powerups[i].position, WHITE, 14, true);
      world->flashPos = world->powerups[i].position;
      world->flash = 1.0f;
      world->powerups[i].active = false;
      world->powerTimer[t] = POWERUP_DEFS[t].duration;
      world->shake = fmaxf(world->shake, POWERUP_DEFS[t].pickupShake);
      player->velocity.y = COIN_BOOST;
      player->expression = EXPR_HAPPY;
      sfxPowerup();
    }
  }
}

static void updateMonsters(World *world, Player *player) {
  for (int i = 0; i < MAX_MONSTERS; i++) {
    if (!world->monsters[i].active) {
      continue;
    }
    Monster *m = &world->monsters[i];
    m->position.x += m->vx;
    if (m->position.x < MONSTER_RADIUS ||
        m->position.x > SCREEN_WIDTH - MONSTER_RADIUS) {
      m->vx = -m->vx;
    }
    if (CheckCollisionCircles(player->position, player->radius, m->position,
                              MONSTER_RADIUS)) {
      if (worldInvincible(world)) {
        // Invincible (rocket etc.): plow through, splatter the monster.
        spawnParticles(world, m->position, GRAPE, 12, false);
        m->active = false;
        sfxBounce();
      } else {
        // Mega-Jump-style: a hit costs coins (they scatter out) and knocks you
        // down + pops the monster, but does NOT kill — only a fall ends the
        // run.
        int lost = world->coinsCollected < MONSTER_COIN_LOSS
                       ? world->coinsCollected
                       : MONSTER_COIN_LOSS;
        world->coinsCollected -= lost;
        spawnParticles(world, player->position, COIN_HI, lost > 0 ? lost : 4,
                       false);
        m->active = false;
        world->shake = SHAKE_HIT;
        player->velocity.y = 12.0f;    // knocked downward
        player->expression = EXPR_SAD; // shakes off on the next coin/power-up
        sfxHit();
      }
    }
  }
}

static void updateParticles(World *world) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (world->particles[i].life <= 0.0f) {
      continue;
    }
    Particle *p = &world->particles[i];
    p->position.x += p->velocity.x;
    p->position.y += p->velocity.y;
    p->velocity.y += PARTICLE_GRAVITY;
    p->life -= PARTICLE_DECAY;
  }
}

// One-way platforms: only catch the player while falling, crossing the top
// edge this frame with their center over the platform.
static void bounceOnPlatforms(World *world, Player *player) {
  if (player->velocity.y <= 0.0f) {
    return;
  }
  float feet = player->position.y + player->radius;
  float prevFeet = feet - player->velocity.y;
  for (int i = 0; i < MAX_PLATFORMS; i++) {
    if (!world->platforms[i].active) {
      continue;
    }
    Vector2 p = world->platforms[i].position;
    if (prevFeet <= p.y && feet >= p.y && player->position.x >= p.x &&
        player->position.x <= p.x + PLATFORM_WIDTH) {
      player->position.y = p.y - player->radius;
      player->velocity.y = PLATFORM_BOUNCE;
      spawnParticles(world, (Vector2){player->position.x, p.y}, CLOUD, 8,
                     false);
      sfxBounce();
      return;
    }
  }
}

void updateWorld(World *world, Player *player) {
  // Bounce off the floor; stops mattering once the camera has risen past it.
  if (player->position.y + player->radius >= FLOOR_Y) {
    player->position.y = FLOOR_Y - player->radius;
    player->velocity.y = BOUNCE_VELOCITY;
    spawnParticles(world, (Vector2){player->position.x, FLOOR_Y}, GRASS, 10,
                   false);
    world->shake = SHAKE_LAUNCH;
    sfxBounce();
  }

  bounceOnPlatforms(world, player);
  collectPowerups(world, player);

  // Per-type active effects (the physics is inherently type-specific)...
  if (world->powerTimer[POWERUP_MAGNET] > 0.0f) {
    applyMagnet(world, player);
  }
  collectCoins(world, player);
  if (world->powerTimer[POWERUP_ROCKET] > 0.0f) {
    player->velocity.y = ROCKET_VELOCITY;
  }
  // ...then tick every power-up timer down uniformly.
  for (int t = 0; t < POWERUP_COUNT; t++) {
    if (world->powerTimer[t] > 0.0f) {
      world->powerTimer[t] -= 1.0f;
    }
  }

  updateMonsters(world, player);

  if (player->position.y < world->bestY) {
    world->bestY = player->position.y;
  }
  // Camera locks to the highest point reached; when the player falls below the
  // view it chases their feet downward, but only DEATH_DROP past that peak — a
  // fall beyond it leaves the player off-screen and isPlayerFallen ends the
  // run.
  float peakY = fminf(0.0f, world->bestY - SCREEN_HEIGHT / 2.0f);
  float need = player->position.y + player->radius - SCREEN_HEIGHT;
  float floorY = peakY + DEATH_DROP;
  world->cameraY = clampf(need, peakY, floorY);

  // Level-up fanfare on first arrival in a new theme band. Keyed off the peak
  // (monotonic), so a fall-and-reclimb never re-triggers it.
  int lvl = themeLevelAt(-peakY);
  if (lvl > world->level) {
    world->level = lvl;
    sfxLevelUp();
  }

  updateParticles(world);
  world->shake *= SHAKE_DECAY;
  if (world->shake < 0.1f) {
    world->shake = 0.0f;
  }
  if (world->flash > 0.0f) {
    world->flash -= FLASH_DECAY;
  }
  spawnCoins(world);
  spawnPlatforms(world);
  spawnMonsters(world);
  spawnPowerups(world);
  cullBelowView(world);
}

static void drawCoin(Vector2 c, float t) {
  float spin = fabsf(cosf(t * 4.0f + c.y * 0.05f)); // 0 (edge-on) .. 1 (face)
  float w = COIN_RADIUS * (0.22f + 0.78f * spin);
  DrawCircle(c.x, c.y, COIN_RADIUS * 1.5f, Fade(COIN_HI, 0.22f)); // glow
  DrawEllipse(c.x, c.y, w, COIN_RADIUS, COIN_LO);
  DrawEllipse(c.x, c.y, w * 0.68f, COIN_RADIUS * 0.68f, COIN_HI);
  if (w > COIN_RADIUS * 0.4f) {
    DrawCircle(c.x - w * 0.32f, c.y - COIN_RADIUS * 0.34f, 2.5f, RAYWHITE);
  }
}

static void drawCloudPad(Vector2 p) {
  float cx = p.x + PLATFORM_WIDTH / 2.0f;
  DrawEllipse(cx, p.y + PLATFORM_HEIGHT + 6.0f, PLATFORM_WIDTH * 0.48f, 9.0f,
              Fade(SKY_BOT_HIGH, 0.35f)); // soft shadow
  DrawCircle(p.x + 24.0f, p.y, 17.0f, CLOUD);
  DrawCircle(p.x + PLATFORM_WIDTH - 24.0f, p.y, 17.0f, CLOUD);
  DrawCircle(cx, p.y - 5.0f, 21.0f, CLOUD);
  DrawRectangleRounded((Rectangle){p.x, p.y, PLATFORM_WIDTH, PLATFORM_HEIGHT},
                       1.0f, 8, CLOUD);
  DrawRectangleRounded((Rectangle){p.x, p.y + PLATFORM_HEIGHT * 0.55f,
                                   PLATFORM_WIDTH, PLATFORM_HEIGHT * 0.45f},
                       1.0f, 8, Fade(CLOUD_SHADE, 0.7f));
}

static void drawMonster(Vector2 m, float t) {
  float R = MONSTER_RADIUS;
  float wob = 1.0f + 0.08f * sinf(t * 6.0f + m.x * 0.1f);
  DrawCircle(m.x, m.y, R * 1.35f, Fade(GRAPE, 0.22f));             // glow
  DrawCircle(m.x - R * 0.5f, m.y + R * 0.8f, R * 0.26f, GRAPE_DK); // feet
  DrawCircle(m.x + R * 0.5f, m.y + R * 0.8f, R * 0.26f, GRAPE_DK);
  DrawEllipse(m.x, m.y, R * wob, R / wob, GRAPE_DK);
  DrawEllipse(m.x, m.y - R * 0.1f, R * 0.82f * wob, R * 0.82f / wob, GRAPE);
  DrawCircle(m.x - 8, m.y - 5, 6, RAYWHITE); // eyes
  DrawCircle(m.x + 8, m.y - 5, 6, RAYWHITE);
  DrawCircle(m.x - 8, m.y - 4, 3, BLACK);
  DrawCircle(m.x + 8, m.y - 4, 3, BLACK);
  for (int k = 0; k < 4; k++) { // zig-zag mouth
    float x0 = m.x - 9 + k * 5;
    float y0 = m.y + (k % 2 ? 12 : 9);
    float y1 = m.y + (k % 2 ? 9 : 12);
    DrawLineEx((Vector2){x0, y0}, (Vector2){x0 + 5, y1}, 2.0f, BLACK);
  }
}

static void drawPowerUp(const PowerUp *pu, float t) {
  Vector2 c = {pu->position.x,
               pu->position.y + sinf(t * 3.0f + pu->position.x * 0.1f) * 4.0f};
  float R = POWERUP_RADIUS;
  DrawCircle(c.x, c.y, R * 1.6f, Fade(RAYWHITE, 0.3f)); // glow
  if (pu->type == POWERUP_MAGNET) {
    DrawRing(c, R * 0.5f, R, 0, 360, 32, MAGNET_RED);
    DrawCircle(c.x - R * 0.72f, c.y + R * 0.55f, 4, LIGHTGRAY); // pole tips
    DrawCircle(c.x + R * 0.72f, c.y + R * 0.55f, 4, LIGHTGRAY);
  } else {
    float fl = R * (0.6f + 0.35f * sinf(t * 22.0f)); // flame flicker
    DrawPoly((Vector2){c.x, c.y + R * 0.9f + fl * 0.4f}, 3, fl, 90, FLAME);
    DrawEllipse(c.x, c.y, R * 0.5f, R * 0.9f, ROCKET_BODY);
    DrawPoly((Vector2){c.x, c.y - R * 0.6f}, 3, R * 0.5f, -90, ROCKET_FIN);
    DrawCircle(c.x, c.y - R * 0.1f, R * 0.24f, SKYBLUE); // window
  }
}

// Ground: soil below, grassy top with a tufted edge. Drawn at FLOOR_Y in the
// current space — shared by the gameplay world and the menu's attract floor.
void drawGround(void) {
  DrawRectangle(0, FLOOR_Y, SCREEN_WIDTH, SCREEN_HEIGHT, GRASS_DK);
  DrawRectangle(0, FLOOR_Y, SCREEN_WIDTH, 16, GRASS);
  for (int x = 0; x <= SCREEN_WIDTH; x += 28) {
    DrawCircle(x, FLOOR_Y, 11, GRASS);
  }
}

void drawWorld(const World *world) {
  float t = (float)GetTime();

  drawGround();

  // Iterate active elements of world->arr[]. Assumes local `world`.
#define FOREACH_ACTIVE(arr, count, i)                                          \
  for (int i = 0; i < (count); i++)                                            \
    if (world->arr[i].active)

  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i)
  drawCloudPad(world->platforms[i].position);
  FOREACH_ACTIVE(coins, MAX_COINS, i)
  drawCoin(world->coins[i].position, t);
  FOREACH_ACTIVE(powerups, MAX_POWERUPS, i)
  drawPowerUp(&world->powerups[i], t);
  FOREACH_ACTIVE(monsters, MAX_MONSTERS, i)
  drawMonster(world->monsters[i].position, t);

#undef FOREACH_ACTIVE

  // Particles as spinning confetti flecks (on top).
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (world->particles[i].life <= 0.0f) {
      continue;
    }
    Particle pt = world->particles[i];
    float sz = 6.0f * pt.life;
    float ang = atan2f(pt.velocity.y, pt.velocity.x) * RAD2DEG;
    DrawRectanglePro((Rectangle){pt.position.x, pt.position.y, sz, sz * 0.6f},
                     (Vector2){sz * 0.5f, sz * 0.3f}, ang,
                     Fade(pt.color, pt.life));
  }

  // Collect "pop" ring.
  if (world->flash > 0.0f) {
    float r = COIN_RADIUS + (1.0f - world->flash) * 42.0f;
    DrawRing(world->flashPos, r * 0.72f, r, 0, 360, 24,
             Fade(RAYWHITE, world->flash));
  }
}

bool isPlayerFallen(const World *world, const Player *player) {
  return player->position.y - player->radius > world->cameraY + SCREEN_HEIGHT;
}

int getScore(const World *world) {
  float ascent = FLOOR_Y - world->bestY;
  return (int)(ascent / HEIGHT_PER_POINT) +
         world->coinsCollected * POINTS_PER_COIN;
}
