#include "world.h"

#include "core.h"
#include "progress.h" // PuTrack + MAX_UPG_LEVEL for the upgrade hooks
#include "sound.h"
#include "svg.h"
#include "tex.h"
#include "theme.h"

#include <math.h>

// ponytail: purchased upgrade levels passed in as a module global (mirrors
// gTheme), so createWorld/updateWorld callers don't thread a param. Set per run
// start; indexed by PuTrack.
static int gPuUpg[PU_UPGRADE_COUNT] = {0};
void worldSetUpgrades(const int *puLevel) {
  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    gPuUpg[i] = puLevel[i];
  }
}

// Per-level effect scaling (tuning knobs — game feel lives here, not in the
// shop table). A track's effect grows 1 + STEP*level; spawn weight grows by
// whole points; the shared debuff-suppress subtracts levels from the bad roll.
#define EFFECT_STEP                                                            \
  0.14f // +14%/level: magnet range, rocket thrust, balloon lift
#define DUR_STEP                                                               \
  0.12f // +12%/level good-power-up duration (was flat +50% LONGBOOST)
#define SPAWN_STEP 1 // +1 spawnWeight/level for the upgraded good power-up

// Effect multiplier (>= 1) for a good power-up's Effect axis.
static float effectMul(PuTrack track) {
  return 1.0f + EFFECT_STEP * gPuUpg[track];
}
// Duration multiplier (>= 1) for a good power-up's Duration axis.
static float durMul(PuTrack track) { return 1.0f + DUR_STEP * gPuUpg[track]; }

// Tuning knobs — hand-tune after playtesting.
#define BOUNCE_VELOCITY -37.0f // floor bounce at the start of a run
#define COIN_BOOST -20.0f      // upward kick per coin collected
#define COIN_RADIUS 12.0f
#define COIN_SPACING 55.0f // vertical distance between spawned coins
#define COIN_WANDER 150.0f // max horizontal step between consecutive coins
#define COIN_DOUBLE_CAP                                                        \
  2 // levels below this get 2 coins/row (L0-L1); L2+ single
#define COMBO_TIMEOUT                                                          \
  150.0f                    // frames (~2.5s) w/o a coin before the chain resets
#define COIN_PAIR_GAP 90.0f // horizontal gap of the bonus second coin
#define SPAWN_MARGIN 40.0f  // keep spawns away from screen edges
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
// Altitude difficulty ramp (keyed on world->level; hand-tune for feel).
#define DIFFICULTY_CAP 3           // level past which the ramp plateaus
#define DIFFICULTY_SPEED 0.25f     // +25% monster drift per level, capped
#define DIFFICULTY_TIGHTEN 150.0f  // px closer monster bands per level
#define MONSTER_SPACING_MIN 450.0f // floor on band spacing
#define COIN_SCATTER_R                                                         \
  60.0f // ring radius knocked-off coins start at (> collect dist)
#define COIN_SCATTER_SPEED 5.0f // outward px/frame at launch
#define COIN_DAMP 0.92f         // scattered coins decelerate to a rest
#define POWERUP_RADIUS 18.0f
#define POWERUP_SPACING 1500.0f // rare
#define MAGNET_RADIUS 260.0f    // coins within this get pulled
#define MAGNET_PULL 6.0f        // px/frame toward the player
#define ROCKET_VELOCITY -34.0f  // upward thrust while active
#define BALLOON_LIFT -8.0f      // gentle guaranteed rise while active
#define ANCHOR_GRAVITY                                                         \
  0.9f // extra downward vel/frame while anchored (doubles gravity)
#define PARTICLE_GRAVITY 0.3f
#define PARTICLE_DECAY 0.03f
#define SHAKE_HIT 14.0f   // screen shake on a monster hit
#define SHAKE_SPLAT 9.0f  // shake on an invincible plow-through splat
#define SHAKE_LAUNCH 6.0f // shake on floor bounce / rocket pickup
#define SHAKE_DECAY 0.85f // per-frame shake falloff
#define FLASH_DECAY 0.08f // pop-ring fade per frame
#define DEATH_DROP                                                             \
  700.0f // how far below the peak view the camera chases a fall
#define BOTTOM_MARGIN                                                          \
  140.0f // keep the player this far above the screen bottom (see below it)
#define TWO_PI (2.0f * PI) // raylib defines PI

// Power-up table (indexed by PowerUpType). Ordered exactly as the enum.
//              duration color       label     weight shake        invincible
//              debuff
const PowerUpDef POWERUP_DEFS[POWERUP_COUNT] = {
    // HUD timer-bar colors are fixed (not themed): the table is static config.
    // spawnWeight only distributes picks *within* a category — pickPowerup(bad)
    // weighted-picks among the good rows or among the debuff rows. The good-vs-
    // bad ratio itself is the altitude roll in spawnPowerups (0/5 bad at L0, up
    // to 3/5 at L3+), not these weights. Tune knob.
    [POWERUP_MAGNET] = {480.0f, SKYBLUE, "MAGNET", 3, 0.0f, false,
                        false}, // ~8s
    [POWERUP_ROCKET] = {150.0f, ORANGE, "ROCKET", 3, SHAKE_LAUNCH, true,
                        false}, // ~2.5s
    [POWERUP_BALLOON] = {360.0f, LIME, "BALLOON", 3, 0.0f, false, false}, // ~6s
    [POWERUP_SHIELD] = {1.0f, GOLD, "SHIELD", 3, SHAKE_LAUNCH, true,
                        false}, // flag: lasts until a monster hit (excluded
                                // from the tick-down; HUD bar stays full)
    // Debuffs: no boost, no immunity, SAD reaction + SFX_HIT on pickup. All ~3s
    // (short — Anchor/Dizzy are run-ending at 5s).
    [POWERUP_ANCHOR] = {180.0f, DARKGRAY, "ANCHOR", 1, SHAKE_HIT, false,
                        true}, // ~3s
    [POWERUP_DIZZY] = {180.0f, VIOLET, "DIZZY", 1, SHAKE_HIT, false,
                       true},                                         // ~3s
    [POWERUP_FOG] = {180.0f, GRAY, "FOG", 1, SHAKE_HIT, false, true}, // ~3s
    [POWERUP_BUTTER] = {180.0f, BROWN, "BUTTER", 1, SHAKE_HIT, false,
                        true}, // ~3s
};
// A dropped row would leave a zero-duration slot (divide-by-zero in the HUD
// timer bar) — fail the build instead.
STATIC_ASSERT(sizeof(POWERUP_DEFS) / sizeof(POWERUP_DEFS[0]) == POWERUP_COUNT,
              powerups_full);

float worldCameraY(const World *world) { return world->cameraY; }
float worldShake(const World *world) { return world->shake; }
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

float worldInputDir(const World *world) {
  return world->powerTimer[POWERUP_DIZZY] > 0.0f ? -1.0f : 1.0f;
}

float worldFog(const World *world) { return world->powerTimer[POWERUP_FOG]; }

// Pool iteration idioms, shared by the fixed-array pools (all liveness-flagged
// with `.active`). `world` is assumed in scope; particles key off `.life`
// instead, so they keep their own loops.
//
// FOREACH_ACTIVE — run the next statement/block for each live slot.
#define FOREACH_ACTIVE(arr, count, i)                                          \
  for (int i = 0; i < (count); i++)                                            \
    if (world->arr[i].active)
// FIND_FREE — declare `slot` = index of the first free slot, or -1 if full.
#define FIND_FREE(slot, arr, count)                                            \
  int slot = -1;                                                               \
  for (int fi_ = 0; fi_ < (count); fi_++)                                      \
    if (!world->arr[fi_].active) {                                             \
      slot = fi_;                                                              \
      break;                                                                   \
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
    FIND_FREE(s, coins, MAX_COINS);
    if (s >= 0) {
      float x = world->lastSpawnX + GetRandomValue(-COIN_WANDER, COIN_WANDER);
      x = clampf(x, SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
      world->coins[s] = (Coin){{x, world->spawnY}, true, {0, 0}}; // at rest
      world->lastSpawnX = x;
      // Low levels are richer: drop a bonus coin beside the trail coin on the
      // same row. lastSpawnX stays the trail coin so the followable line holds.
      if (world->level < COIN_DOUBLE_CAP) {
        FIND_FREE(s2, coins, MAX_COINS);
        if (s2 >= 0) {
          float x2 = clampf(
              x + (GetRandomValue(0, 1) ? COIN_PAIR_GAP : -COIN_PAIR_GAP),
              SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
          world->coins[s2] = (Coin){{x2, world->spawnY}, true, {0, 0}};
        }
      }
    }
    world->spawnY -= COIN_SPACING;
  }
}

static void spawnPlatforms(World *world) {
  while (world->platformSpawnY > world->cameraY - SCREEN_HEIGHT) {
    FIND_FREE(s, platforms, MAX_PLATFORMS);
    if (s >= 0) {
      float x = GetRandomValue(SPAWN_MARGIN,
                               SCREEN_WIDTH - SPAWN_MARGIN - PLATFORM_WIDTH);
      world->platforms[s] = (Platform){{x, world->platformSpawnY}, true};
    }
    world->platformSpawnY -= PLATFORM_SPACING;
  }
}

// Difficulty level for the altitude ramp: the monotonic climb level, capped at
// DIFFICULTY_CAP (0..CAP). Drives monster density/speed/spacing and the debuff
// spawn odds.
static int difficulty(const World *world) {
  return world->level < DIFFICULTY_CAP ? world->level : DIFFICULTY_CAP;
}

static void spawnMonsters(World *world) {
  int d = difficulty(world);
  while (world->monsterSpawnY > world->cameraY - SCREEN_HEIGHT) {
    // Higher levels: more bands carry a monster (L0:1/5 .. L3+:4/5).
    if (GetRandomValue(0, 4) <= d) {
      FIND_FREE(s, monsters, MAX_MONSTERS);
      if (s >= 0) {
        float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
        float speed = MONSTER_DRIFT * (1.0f + DIFFICULTY_SPEED * d);
        float vx = GetRandomValue(0, 1) ? speed : -speed;
        world->monsters[s] = (Monster){{x, world->monsterSpawnY}, vx, true};
      }
    }
    float step = MONSTER_SPACING - DIFFICULTY_TIGHTEN * d;
    world->monsterSpawnY -=
        step < MONSTER_SPACING_MIN ? MONSTER_SPACING_MIN : step;
  }
}

// Spawn-axis upgrade: extra weight for an upgraded good power-up so it
// dominates good picks. Debuffs (and untracked types) get no bonus.
static int spawnWeightOf(PowerUpType t) {
  int bonus = 0;
  switch (t) {
  case POWERUP_MAGNET:
    bonus = SPAWN_STEP * gPuUpg[PU_MAGNET_SPAWN];
    break;
  case POWERUP_ROCKET:
    bonus = SPAWN_STEP * gPuUpg[PU_ROCKET_SPAWN];
    break;
  case POWERUP_BALLOON:
    bonus = SPAWN_STEP * gPuUpg[PU_BALLOON_SPAWN];
    break;
  case POWERUP_SHIELD:
    bonus = SPAWN_STEP * gPuUpg[PU_SHIELD_SPAWN];
    break;
  default:
    break;
  }
  return POWERUP_DEFS[t].spawnWeight + bonus;
}

// Weighted pick over spawnWeightOf(), restricted to one category (good or
// debuff). Both categories have positive total weight, so the fallback return
// is unreachable.
static PowerUpType pickPowerup(bool debuff) {
  int total = 0;
  for (int k = 0; k < POWERUP_COUNT; k++) {
    if (POWERUP_DEFS[k].debuff == debuff) {
      total += spawnWeightOf(k);
    }
  }
  int roll = GetRandomValue(0, total - 1);
  for (PowerUpType t = 0; t < POWERUP_COUNT; t++) {
    if (POWERUP_DEFS[t].debuff == debuff) {
      int w = spawnWeightOf(t);
      if (roll < w) {
        return t;
      }
      roll -= w;
    }
  }
  return POWERUP_MAGNET;
}

static void spawnPowerups(World *world) {
  // Debuff chance ramps with altitude: GetRandomValue(0,4) < d gives L0:0/5 bad
  // .. L3+:3/5 bad (strictly-less, so level 0 is all good pickups — unlike
  // spawnMonsters' <= d, which puts a monster on 1/5 of bands even at L0).
  // "Fewer Debuffs" upgrade subtracts levels from the effective altitude, so
  // higher levels make bad pickups rarer (and clear them entirely once the
  // level reaches d).
  int d = difficulty(world) - gPuUpg[PU_DEBUFF_SUPPRESS];
  while (world->powerupSpawnY > world->cameraY - SCREEN_HEIGHT) {
    FIND_FREE(s, powerups, MAX_POWERUPS);
    if (s >= 0) {
      float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
      PowerUpType t = pickPowerup(GetRandomValue(0, 4) < d);
      world->powerups[s] = (PowerUp){{x, world->powerupSpawnY}, t, true};
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
    float a = GetRandomValue(0, 1000) / 1000.0f * TWO_PI; // 0..2π
    float sp = GetRandomValue(2, 7);
    Color c = confetti ? gTheme.confetti[GetRandomValue(0, CONFETTI_COUNT - 1)]
                       : color;
    world->particles[i] = (Particle){
        at, {cosf(a) * sp, sinf(a) * sp - 2.0f}, 1.0f, c}; // slight pop upward
    made++;
  }
}

// Knock `count` real coins off in an even ring around `at`, flung outward so
// they scatter clear of the player and can be chased down and re-collected.
// Started just past the collect radius so they don't get instantly re-grabbed.
static void scatterCoins(World *world, Vector2 at, int count) {
  int made = 0;
  for (int i = 0; i < MAX_COINS && made < count; i++) {
    if (world->coins[i].active) {
      continue;
    }
    float a = (float)made / (float)count * TWO_PI; // even wedge per coin
    Vector2 dir = {cosf(a), sinf(a)};
    world->coins[i] =
        (Coin){{at.x + dir.x * COIN_SCATTER_R, at.y + dir.y * COIN_SCATTER_R},
               true,
               {dir.x * COIN_SCATTER_SPEED, dir.y * COIN_SCATTER_SPEED}};
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
  // Cull past the DEEPEST the camera can chase a fall (peak + DEATH_DROP), not
  // the current view bottom — otherwise entities in the "reappear" band get
  // freed while the camera is still locked high, then vanish on the way down.
  float cullY = world->cameraY + SCREEN_HEIGHT + DEATH_DROP + COIN_RADIUS;
  CULL(coins, MAX_COINS, 0.0f);
  CULL(platforms, MAX_PLATFORMS, PLATFORM_HEIGHT);
  CULL(monsters, MAX_MONSTERS, MONSTER_RADIUS);
  CULL(powerups, MAX_POWERUPS, POWERUP_RADIUS);
}
#undef CULL

static void collectCoins(World *world, Player *player) {
  if (world->powerTimer[POWERUP_BUTTER] > 0.0f) {
    return; // butterfingers: coins slip through, can't be picked up
  }
  FOREACH_ACTIVE(coins, MAX_COINS, i) {
    if (CheckCollisionCircles(player->position, player->radius,
                              world->coins[i].position, COIN_RADIUS)) {
      // Combo: each coin chained (no fall to the floor / hit between) raises
      // the multiplier, and with it the pop size, shake, and SFX pitch.
      world->combo++;
      if (world->combo > world->bestCombo) {
        world->bestCombo = world->combo;
      }
      world->comboTimer = COMBO_TIMEOUT;     // idle-reset window
      world->comboBonus += world->combo - 1; // rising multiplier bonus
      int pop = 14 + world->combo * 2;
      if (pop > 40) {
        pop = 40;
      }
      spawnParticles(world, world->coins[i].position, WHITE, pop, true);
      world->shake = fmaxf(world->shake, fminf(world->combo * 0.6f, 7.0f));
      world->flashPos = world->coins[i].position;
      world->flash = 1.0f;
      world->coins[i].active = false;
      world->coinsCollected++;
      player->velocity.y = COIN_BOOST;
      player->expression = EXPR_HAPPY;
      float pitch = 1.0f + fminf((float)world->combo, 20.0f) * 0.045f;
      sfxPitched(SFX_COIN, pitch);
    }
  }
}

// Integrate + damp coin velocity. Normal coins sit at {0} velocity and don't
// move; only ones knocked off by a hit drift outward and settle.
static void moveCoins(World *world) {
  FOREACH_ACTIVE(coins, MAX_COINS, i) {
    world->coins[i].position.x += world->coins[i].velocity.x;
    world->coins[i].position.y += world->coins[i].velocity.y;
    world->coins[i].velocity.x *= COIN_DAMP;
    world->coins[i].velocity.y *= COIN_DAMP;
  }
}

// Coin magnet: while active, drag nearby coins toward the player so the
// existing overlap check sweeps them up next frame.
static void applyMagnet(World *world, const Player *player) {
  float radius =
      MAGNET_RADIUS * effectMul(PU_MAGNET_EFFECT); // Effect: wider reach
  FOREACH_ACTIVE(coins, MAX_COINS, i) {
    Vector2 c = world->coins[i].position;
    float dx = player->position.x - c.x;
    float dy = player->position.y - c.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 0.0f && d < radius) {
      world->coins[i].position.x += dx / d * MAGNET_PULL;
      world->coins[i].position.y += dy / d * MAGNET_PULL;
    }
  }
}

static void collectPowerups(World *world, Player *player) {
  FOREACH_ACTIVE(powerups, MAX_POWERUPS, i) {
    if (CheckCollisionCircles(player->position, player->radius,
                              world->powerups[i].position, POWERUP_RADIUS)) {
      PowerUpType t = world->powerups[i].type;
      // Same collect juice as a coin: pop particles, flash, and an upward kick.
      spawnParticles(world, world->powerups[i].position, WHITE, 14, true);
      world->flashPos = world->powerups[i].position;
      world->flash = 1.0f;
      world->powerups[i].active = false;
      float dur = POWERUP_DEFS[t].duration;
      // Duration upgrade: scale each good, timed power-up by its own track.
      switch (t) {
      case POWERUP_MAGNET:
        dur *= durMul(PU_MAGNET_DURATION);
        break;
      case POWERUP_ROCKET:
        dur *= durMul(PU_ROCKET_DURATION);
        break;
      case POWERUP_BALLOON:
        dur *= durMul(PU_BALLOON_DURATION);
        break;
      case POWERUP_SHIELD:
        world->shieldHits = 1 + gPuUpg[PU_SHIELD_EFFECT];
        break;
      default:
        break; // debuffs and any untracked type keep base duration
      }
      world->powerTimer[t] = dur;
      world->shake = fmaxf(world->shake, POWERUP_DEFS[t].pickupShake);
      if (POWERUP_DEFS[t].debuff) {
        // Bad pickup: no upward kick, sour face, same hit sting as a monster.
        player->expression = EXPR_SAD;
        sfx(SFX_HIT);
      } else {
        player->velocity.y = COIN_BOOST;
        player->expression = EXPR_HAPPY;
        sfx((t == POWERUP_BALLOON || t == POWERUP_SHIELD) ? SFX_PICKUP
                                                          : SFX_POWERUP);
      }
    }
  }
}

static void updateMonsters(World *world, Player *player) {
  FOREACH_ACTIVE(monsters, MAX_MONSTERS, i) {
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
        world->shake = fmaxf(world->shake, SHAKE_SPLAT);
        world->hitstop = 3; // brief freeze reads as impact
        // Shield absorbs this hit; break only once its charges run out (Effect
        // upgrade grants extra charges). Rocket-only invincibility leaves the
        // shield untouched (shieldHits is 0 when no shield is active).
        if (world->powerTimer[POWERUP_SHIELD] > 0.0f &&
            --world->shieldHits <= 0) {
          world->powerTimer[POWERUP_SHIELD] = 0.0f;
        }
        sfx(SFX_SPLAT);
      } else {
        // Mega-Jump-style: a hit costs coins (they scatter out) and knocks you
        // down + pops the monster, but does NOT kill — only a fall ends the
        // run.
        int lost = world->coinsCollected < MONSTER_COIN_LOSS
                       ? world->coinsCollected
                       : MONSTER_COIN_LOSS;
        world->coinsCollected -= lost;
        scatterCoins(world, player->position,
                     lost); // only the coins actually lost
        m->active = false;
        world->shake = SHAKE_HIT;
        world->hitstop = 2;            // brief freeze reads as impact
        world->combo = 0;              // hit broke the chain
        player->velocity.y = 12.0f;    // knocked downward
        player->expression = EXPR_SAD; // shakes off on the next coin/power-up
        sfx(SFX_HIT);
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
  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i) {
    Vector2 p = world->platforms[i].position;
    if (prevFeet <= p.y && feet >= p.y && player->position.x >= p.x &&
        player->position.x <= p.x + PLATFORM_WIDTH) {
      player->position.y = p.y - player->radius;
      player->velocity.y = PLATFORM_BOUNCE;
      spawnParticles(world, (Vector2){player->position.x, p.y}, CLOUD, 8,
                     false);
      sfx(SFX_BOUNCE);
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
    world->combo = 0; // fell all the way to the floor: chain broken
    sfx(SFX_BOUNCE);
  }

  bounceOnPlatforms(world, player);
  collectPowerups(world, player);

  // Per-type active effects (the physics is inherently type-specific)...
  if (world->powerTimer[POWERUP_MAGNET] > 0.0f) {
    applyMagnet(world, player);
  }
  moveCoins(world);
  collectCoins(world, player);
  if (world->powerTimer[POWERUP_ANCHOR] > 0.0f) {
    player->velocity.y +=
        ANCHOR_GRAVITY; // heavier pull; rocket below overrides
  }
  if (world->powerTimer[POWERUP_ROCKET] > 0.0f) {
    player->velocity.y =
        ROCKET_VELOCITY * effectMul(PU_ROCKET_EFFECT); // Effect: more thrust
  }
  float lift =
      BALLOON_LIFT * effectMul(PU_BALLOON_EFFECT); // Effect: stronger lift
  if (world->powerTimer[POWERUP_BALLOON] > 0.0f && player->velocity.y > lift) {
    player->velocity.y = lift; // never plummet; faster coin boosts win
  }
  // ...then tick every power-up timer down uniformly.
  for (int t = 0; t < POWERUP_COUNT; t++) {
    // Shield is a flag (no duration): it persists until a monster hit, so skip
    // it.
    if (t != POWERUP_SHIELD && world->powerTimer[t] > 0.0f) {
      world->powerTimer[t] -= 1.0f;
    }
  }

  updateMonsters(world, player);

  // Dizzy face for the whole debuff: drive it from world state each frame so a
  // coin collected mid-spin (which set EXPR_HAPPY above) can't clear it early.
  if (world->powerTimer[POWERUP_DIZZY] > 0.0f) {
    player->expression = EXPR_DIZZY;
  }

  if (player->position.y < world->bestY) {
    world->bestY = player->position.y;
  }
  // Camera locks to the highest point reached; when the player falls below the
  // view it chases their feet downward, but only DEATH_DROP past that peak — a
  // fall beyond it leaves the player off-screen and isPlayerFallen ends the
  // run.
  float peakY = fminf(0.0f, world->bestY - SCREEN_HEIGHT / 2.0f);
  float need =
      player->position.y + player->radius - SCREEN_HEIGHT + BOTTOM_MARGIN;
  float floorY = peakY + DEATH_DROP;
  world->cameraY = clampf(need, peakY, floorY);

  // Level-up fanfare on first arrival in a new theme band. Keyed off the peak
  // (monotonic), so a fall-and-reclimb never re-triggers it.
  int lvl = themeLevelAt(-peakY);
  if (lvl > world->level) {
    world->level = lvl;
    sfx(SFX_LEVELUP);
  }

  // Idle combo reset: no coin grabbed within COMBO_TIMEOUT drops the chain.
  if (world->combo > 0 && (world->comboTimer -= 1.0f) <= 0.0f) {
    world->combo = 0;
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

// Deterministic per-instance seed from an entity's (fixed) spawn y, so coins
// and monsters get stable random-looking variety without extra struct fields.
static unsigned entSeed(Vector2 p) {
  unsigned h = (unsigned)(int)fabsf(p.y) * 2654435761u;
  h ^= h >> 15;
  return h;
}

// One additive soft-light stamp — the shared idiom behind every glow in this
// file (coin, monster, power-up, pop ring, particles). `w` is the full box
// width, so a glow of radius r asks for 2*r.
//
// The theme's bloom weight is *added to* a base rather than replacing it: the
// day era has themeGlow() == 0, and weighting it out entirely would leave the
// daylight coins flatter than they were before any of this. So Sunny Hills
// keeps the light it always had and the neon eras blaze on top of it.
static void glowStamp(Vector2 c, float w, Color tint, float base, float lit) {
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, c.x, c.y, w, w, 0.0f, Fade(tint, base + lit * themeGlow()));
  EndBlendMode();
}

static void drawCoin(Vector2 c, float t) {
  unsigned seed = entSeed(c);
  float phase = (seed & 255) / 255.0f * TWO_PI; // out-of-sync twinkle offset
  float spin = fabsf(cosf(t * 4.0f + c.y * 0.05f)); // 0 (edge-on) .. 1 (face)
  float w = COIN_RADIUS * (0.22f + 0.78f * spin);
  glowStamp(c, COIN_RADIUS * 4.5f, COIN_HI, 0.22f, 0.3f);
  DrawEllipse(c.x, c.y, w * 1.12f, COIN_RADIUS * 1.12f,
              ColorBrightness(COIN_LO, -0.35f));                  // minted rim
  DrawEllipse(c.x, c.y, w, COIN_RADIUS, COIN_LO);                 // edge
  DrawEllipse(c.x, c.y, w * 0.68f, COIN_RADIUS * 0.68f, COIN_HI); // face
  if (w > COIN_RADIUS * 0.45f) {
    float fw = w * 0.68f, fh = COIN_RADIUS * 0.68f;
    Color emb = ColorBrightness(COIN_LO, -0.15f);
    switch (seed % 3) { // embossed emblem, varies per coin
    case 0:
      DrawPoly(c, 5, fh * 0.6f, t * 40.0f, emb);
      break;
    case 1:
      DrawRing(c, fh * 0.34f, fh * 0.52f, 0, 360, 16, emb);
      break;
    default:
      DrawCircle(c.x, c.y, fh * 0.32f, emb);
      break;
    }
    // Rotating glint streak sweeping across the face.
    float g = t * 3.0f;
    DrawLineEx((Vector2){c.x - cosf(g) * fw * 0.7f, c.y - sinf(g) * fh * 0.7f},
               (Vector2){c.x + cosf(g) * fw * 0.7f, c.y + sinf(g) * fh * 0.7f},
               2.0f, Fade(RAYWHITE, 0.5f));
    DrawCircle(c.x - w * 0.32f, c.y - COIN_RADIUS * 0.34f, 2.5f, RAYWHITE);
  }
  // Periodic sparkle beside the coin, phased by the seed.
  float tw = sinf(t * 3.0f + phase);
  if (tw > 0.6f) {
    float s = (tw - 0.6f) / 0.4f * 4.0f;
    float sx = c.x + COIN_RADIUS * 0.9f, sy = c.y - COIN_RADIUS * 0.9f;
    DrawLineEx((Vector2){sx - s, sy}, (Vector2){sx + s, sy}, 1.5f,
               Fade(RAYWHITE, tw));
    DrawLineEx((Vector2){sx, sy - s}, (Vector2){sx, sy + s}, 1.5f,
               Fade(RAYWHITE, tw));
  }
}

static void drawCloudPad(Vector2 p) {
  float cx = p.x + PLATFORM_WIDTH / 2.0f;
  drawTex(TEX_PUFF, cx, p.y + PLATFORM_HEIGHT + 6.0f, PLATFORM_WIDTH * 1.5f,
          40.0f, 0.0f, Fade(SKY_BOT_HIGH, 0.5f)); // soft shadow
  // Cloud-puff silhouette behind the board, as a soft backing accent. Same
  // TEX_CLOUD the backdrop's clouds are made of, so a pad reads as the same
  // weather as the sky it hangs in.
  drawTex(TEX_CLOUD, p.x + 24.0f, p.y, 62.0f, 50.0f, 0.0f, CLOUD);
  drawTex(TEX_CLOUD, p.x + PLATFORM_WIDTH - 24.0f, p.y, 62.0f, 50.0f, 0.0f,
          CLOUD);
  drawTex(TEX_CLOUD, cx, p.y - 5.0f, 78.0f, 62.0f, 0.0f, CLOUD);

  // The landing board: a defined solid surface so the jump-off edge reads
  // clearly against the sky. Dark outline + body + a bright top face strip.
  Rectangle board = {p.x, p.y, PLATFORM_WIDTH, PLATFORM_HEIGHT};
  DrawRectangleRounded(board, 0.6f, 8, CLOUD);
  DrawRectangleRounded(
      (Rectangle){p.x, p.y, PLATFORM_WIDTH, PLATFORM_HEIGHT * 0.4f}, 0.9f, 8,
      RAYWHITE); // lit top face (where feet land)
  DrawRectangleRounded((Rectangle){p.x, p.y + PLATFORM_HEIGHT * 0.55f,
                                   PLATFORM_WIDTH, PLATFORM_HEIGHT * 0.45f},
                       0.9f, 8, Fade(CLOUD_SHADE, 0.85f)); // shaded underside
  DrawRectangleRoundedLines(board, 0.6f, 8, GRASS_DK);     // crisp rim
}

static void drawMonster(Vector2 m, float t) {
  float R = MONSTER_RADIUS;
  unsigned seed = entSeed(m);
  float wob = 1.0f + 0.08f * sinf(t * 6.0f + m.x * 0.1f);
  Color horn = ColorBrightness(GRAPE_DK, -0.15f);

  drawTex(TEX_PUFF, m.x, m.y + R * 1.05f, R * 2.6f, R * 0.9f, 0.0f,
          Fade(BLACK, 0.3f)); // drop shadow
  glowStamp(m, R * 3.5f, GRAPE, 0.2f, 0.45f);

  // Wobbling stubby legs (2 or 3, seed-picked).
  int legs = (seed & 1) ? 3 : 2;
  for (int k = 0; k < legs; k++) {
    float lx = m.x + (k - (legs - 1) * 0.5f) * R * 0.5f;
    float ly = m.y + R * 0.7f + sinf(t * 7.0f + k + m.x * 0.1f) * R * 0.14f;
    DrawCircle(lx, ly + R * 0.18f, R * 0.2f, GRAPE_DK);
  }

  // Head decor: two horns / a bobbing antenna / bald, seed-picked.
  switch (seed % 3) {
  case 0:
    DrawPoly((Vector2){m.x - R * 0.4f, m.y - R * 0.75f}, 3, R * 0.32f, -90,
             horn);
    DrawPoly((Vector2){m.x + R * 0.4f, m.y - R * 0.75f}, 3, R * 0.32f, -90,
             horn);
    break;
  case 1: {
    float ax = m.x + sinf(t * 4.0f + m.x) * R * 0.2f;
    DrawLineEx((Vector2){m.x, m.y - R * 0.7f}, (Vector2){ax, m.y - R * 1.25f},
               3.0f, horn);
    DrawCircle(ax, m.y - R * 1.3f, R * 0.16f, GRAPE);
    break;
  }
  default:
    break;
  }

  // Body: dark base, brightened rim, lit body.
  DrawEllipse(m.x, m.y, R * wob, R / wob, GRAPE_DK);
  DrawEllipse(m.x, m.y, R * 0.9f * wob, R * 0.9f / wob,
              ColorBrightness(GRAPE, 0.3f));
  DrawEllipse(m.x, m.y - R * 0.1f, R * 0.78f * wob, R * 0.78f / wob, GRAPE);

  // Eyes, seed-picked: even / one wonky-bigger / cyclops.
  int eyeStyle = seed % 3;
  if (eyeStyle == 2) {
    DrawCircle(m.x, m.y - 4, 8, RAYWHITE);
    DrawCircle(m.x, m.y - 3, 4, BLACK);
  } else {
    float rr = (eyeStyle == 1) ? 8.0f : 6.0f; // wonky: right eye bigger
    DrawCircle(m.x - 8, m.y - 5, 6, RAYWHITE);
    DrawCircle(m.x + 8, m.y - 5, rr, RAYWHITE);
    DrawCircle(m.x - 8, m.y - 4, 3, BLACK);
    DrawCircle(m.x + 8, m.y - 4, 3, BLACK);
  }
  // Angry brows.
  DrawLineEx((Vector2){m.x - 13, m.y - 12}, (Vector2){m.x - 4, m.y - 8}, 2.5f,
             BLACK);
  DrawLineEx((Vector2){m.x + 13, m.y - 12}, (Vector2){m.x + 4, m.y - 8}, 2.5f,
             BLACK);

  // Mouth, seed-picked: zig-zag / open maw with fangs / grin.
  switch (seed % 3) {
  case 0:
    for (int k = 0; k < 4; k++) {
      float x0 = m.x - 9 + k * 5;
      float y0 = m.y + (k % 2 ? 12 : 9);
      float y1 = m.y + (k % 2 ? 9 : 12);
      DrawLineEx((Vector2){x0, y0}, (Vector2){x0 + 5, y1}, 2.0f, BLACK);
    }
    break;
  case 1:
    DrawCircle(m.x, m.y + 11, 5, BLACK);
    DrawPoly((Vector2){m.x - 3, m.y + 9}, 3, 3.0f, 90, RAYWHITE);
    DrawPoly((Vector2){m.x + 3, m.y + 9}, 3, 3.0f, 90, RAYWHITE);
    break;
  default:
    DrawLineEx((Vector2){m.x - 8, m.y + 10}, (Vector2){m.x + 8, m.y + 10}, 2.0f,
               BLACK);
    break;
  }
}

static void drawPowerUp(const PowerUp *pu, float t) {
  Vector2 c = {pu->position.x,
               pu->position.y + sinf(t * 3.0f + pu->position.x * 0.1f) * 4.0f};
  float R = POWERUP_RADIUS;
  float pulse = 0.25f + 0.12f * sinf(t * 4.0f + pu->position.y * 0.05f);
  glowStamp(c, R * (4.0f + 0.4f * sinf(t * 4.0f)), RAYWHITE, pulse,
            0.25f); // pulsing halo
  drawPowerupGlyph(pu->type, c, R, t);
}

// The glyph-only power-up icon (no halo, no bob) at center `c`, radius `R` —
// shared by the world draw above and the SHOP upgrade cards. Colors read the
// current gTheme. `t` drives the small self-animations (spark, spin, flame).
void drawPowerupGlyph(PowerUpType type, Vector2 c, float R, float t) {
  // Each glyph is several theme colors and a texture tint is uniform, so a
  // glyph is one drawSvg per color role, back to front. The layers share a
  // 24x24 viewBox where 12 units == R, so a layer drawn at box 2*R lands
  // exactly where the primitives it replaced did; a layer drawn smaller (the
  // shield's plate, the dizzy lens) just scales that same box.
  //
  // Three effects stay procedural because they reshape every frame and no
  // fixed picture can stand in: the rocket's flame, the balloon's wavy string,
  // and the magnet's blinking spark.
  switch (type) {
  case POWERUP_MAGNET: {
    // Horseshoe: one true arc now, not a 24-segment ring fan.
    drawSvg(SVG_PU_MAGNET, c.x, c.y, R * 2.0f, 0.0f, MAGNET_RED);
    drawSvg(SVG_PU_MAGNET_TIPS, c.x, c.y, R * 2.0f, 0.0f, LIGHTGRAY);
    if (sinf(t * 8.0f) > 0.7f) {
      DrawCircle(c.x, c.y + R * 0.95f, 2.5f, Fade(RAYWHITE, 0.9f)); // spark
    }
    break;
  }
  case POWERUP_BALLOON: {
    Color body = GRASS, bodyDk = ColorBrightness(GRASS, -0.28f);
    Vector2 knot = {c.x, c.y + R * 0.9f}, a = knot;
    for (int k = 1; k <= 4; k++) { // wavy string
      Vector2 b = {c.x + sinf(t * 4.0f + k) * 3.0f, knot.y + k * R * 0.28f};
      DrawLineEx(a, b, 2.0f, DARKGRAY);
      a = b;
    }
    drawSvg(SVG_PU_BALLOON_DK, c.x, c.y, R * 2.0f, 0.0f, bodyDk);
    drawSvg(SVG_PU_BALLOON, c.x, c.y, R * 2.0f, 0.0f, body);
    drawSvg(SVG_PU_BALLOON_GLOSS, c.x, c.y, R * 2.0f, 0.0f,
            Fade(RAYWHITE, 0.6f));
    break;
  }
  case POWERUP_SHIELD: {
    // One hex glyph at four sizes: outer glow, bevel rim, plate, top sheen.
    Color plate = COIN_LO, rim = ColorBrightness(COIN_LO, -0.35f),
          sheen = COIN_HI;
    drawSvg(SVG_PU_SHIELD_HEX, c.x, c.y, R * 2.24f, 0.0f, Fade(sheen, 0.3f));
    drawSvg(SVG_PU_SHIELD_HEX, c.x, c.y, R * 2.0f, 0.0f, rim);
    drawSvg(SVG_PU_SHIELD_HEX, c.x, c.y, R * 1.64f, 0.0f, plate);
    drawSvg(SVG_PU_SHIELD_HEX, c.x, c.y - R * 0.18f, R, 0.0f,
            Fade(sheen, 0.7f));
    drawSvg(SVG_PU_SHIELD_CREST, c.x, c.y, R * 0.84f, t * 30.0f, GRAPE);
    break;
  }
  case POWERUP_ANCHOR: {
    drawSvg(SVG_PU_ANCHOR, c.x, c.y, R * 2.0f, 0.0f, DARKGRAY);
    drawSvg(SVG_PU_ANCHOR_FLUKES, c.x, c.y, R * 2.0f, 0.0f,
            ColorBrightness(DARKGRAY, 0.35f));
    break;
  }
  case POWERUP_DIZZY: {
    // Two lenses, each a disc drawn twice (rim, then face) under a spiral the
    // draw call spins — opposite ways, so the two eyes counter-rotate.
    Color frame = VIOLET, dk = ColorBrightness(VIOLET, -0.4f);
    for (int e = -1; e <= 1; e += 2) {
      float ox = c.x + e * R * 0.5f;
      drawSvg(SVG_PU_DISC, ox, c.y, R, 0.0f, dk);
      drawSvg(SVG_PU_DISC, ox, c.y, R * 0.84f, 0.0f, RAYWHITE);
      drawSvg(SVG_PU_SPIRAL, ox, c.y, R * 2.0f, t * 120.0f * e, frame);
    }
    DrawRectangle(c.x - R * 0.06f, c.y - R * 0.06f, R * 0.12f, R * 0.12f,
                  frame); // bridge
    break;
  }
  case POWERUP_FOG: {
    drawSvg(SVG_PU_FOG_DK, c.x, c.y, R * 2.0f, 0.0f,
            ColorBrightness(GRAY, -0.3f));
    drawSvg(SVG_PU_FOG, c.x, c.y, R * 2.0f, 0.0f, GRAY);
    drawSvg(SVG_PU_FOG_CROWN, c.x, c.y, R * 2.0f, 0.0f,
            ColorBrightness(GRAY, 0.25f));
    break;
  }
  case POWERUP_BUTTER: {
    // Reuses the HUD coin's two layers under a prohibition ring. The boxes
    // differ because the rim and face fill their shared viewBox by different
    // fractions, so each is scaled to land at the size the old ellipses had.
    drawSvg(SVG_COIN_RIM, c.x, c.y, R * 1.6f, 0.0f,
            ColorBrightness(COIN_LO, -0.3f));
    drawSvg(SVG_COIN_FACE, c.x, c.y, R * 1.85f, 0.0f, COIN_HI);
    drawSvg(SVG_PU_NO, c.x, c.y, R * 2.0f, 0.0f, RED);
    break;
  }
  case POWERUP_ROCKET:
  default: {
    float fl = R * (0.6f + 0.35f * sinf(t * 22.0f)); // flame flicker
    DrawPoly((Vector2){c.x, c.y + R * 0.9f + fl * 0.4f}, 3, fl, 90,
             Fade(FLAME, 0.7f));
    DrawPoly((Vector2){c.x, c.y + R * 0.9f + fl * 0.2f}, 3, fl * 0.6f, 90,
             ColorBrightness(FLAME, 0.4f));
    DrawPoly((Vector2){c.x, c.y + R * 0.85f}, 3, fl * 0.3f, 90,
             Fade(RAYWHITE, 0.8f)); // hot core
    drawSvg(SVG_PU_ROCKET_BODY, c.x, c.y, R * 2.0f, 0.0f, ROCKET_BODY);
    drawSvg(SVG_PU_ROCKET_SHADE, c.x, c.y, R * 2.0f, 0.0f,
            ColorBrightness(ROCKET_BODY, -0.28f));
    drawSvg(SVG_PU_ROCKET_FIN, c.x, c.y, R * 2.0f, 0.0f, ROCKET_FIN);
    drawSvg(SVG_PU_ROCKET_WINDOW, c.x, c.y, R * 2.0f, 0.0f,
            ColorBrightness(ROCKET_FIN, 0.35f));
    DrawCircle(c.x - R * 0.08f, c.y - R * 0.18f, R * 0.08f,
               Fade(RAYWHITE, 0.8f)); // window shine
    break;
  }
  }
}

// Ground: soil below, grassy top with a tufted edge. Drawn at FLOOR_Y in the
// current space — shared by the gameplay world and the menu's attract floor.
void drawGround(void) {
  DrawRectangle(0, FLOOR_Y, SCREEN_WIDTH, SCREEN_HEIGHT, GRASS_DK);
  // Soil grain: TEX_ROCK tiled over the soil rect only. Tiled rather than
  // stretched so the cell size stays put however tall the rect is, and only
  // over the rectangle — cellular noise fills its own square box. The tile is
  // small on purpose: TEX_ROCK is 8 cells across, so a big tile turns soil
  // into cobblestones. At 70px a cell is ~9px and reads as grit.
  Texture2D rock = texGet(TEX_ROCK);
  if (rock.id != 0) {
    const float TILE = 70.0f;
    DrawTexturePro(rock,
                   (Rectangle){0, 0, SCREEN_WIDTH / TILE * rock.width,
                               SCREEN_HEIGHT / TILE * rock.height},
                   (Rectangle){0, FLOOR_Y, SCREEN_WIDTH, SCREEN_HEIGHT},
                   (Vector2){0, 0}, 0.0f,
                   Fade(ColorBrightness(GRASS_DK, -0.4f), 0.28f));
  }
  DrawRectangle(0, FLOOR_Y, SCREEN_WIDTH, 16, GRASS);
  for (int x = 0; x <= SCREEN_WIDTH; x += 28) {
    DrawCircle(x, FLOOR_Y, 11, GRASS);
  }
}

// Particles as spinning confetti flecks (on top of the scene), each over an
// additive soft-edged streak so a burst throws light as well as paper.
//
// Two passes, not one loop drawing both: a blend switch flushes the batch, so
// per-particle switching would cost 128 flushes a frame instead of two. The
// solid fleck stays — additive alone vanishes against the daylight sky, which
// saturates to white, and confetti is what a coin burst is supposed to be.
static void drawParticles(const World *world) {
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (world->particles[i].life <= 0.0f) {
      continue;
    }
    Particle pt = world->particles[i];
    float sz = 26.0f * pt.life;
    float ang = atan2f(pt.velocity.y, pt.velocity.x) * RAD2DEG;
    drawTex(TEX_PUFF, pt.position.x, pt.position.y, sz, sz * 0.5f, ang,
            Fade(pt.color, pt.life * (0.4f + 0.6f * themeGlow())));
  }
  EndBlendMode();
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
}

// The expanding "pop" ring flashed at the last coin/power-up collect, over an
// additive bloom so the pop throws light instead of just outlining itself.
static void drawFlash(const World *world) {
  if (world->flash > 0.0f) {
    float r = COIN_RADIUS + (1.0f - world->flash) * 42.0f;
    glowStamp(world->flashPos, r * 2.4f, RAYWHITE, world->flash * 0.5f, 0.3f);
    DrawRing(world->flashPos, r * 0.72f, r, 0, 360, 24,
             Fade(RAYWHITE, world->flash));
  }
}

// Rotating power-up rings around the player, one per active power-up, stacked
// at increasing radii so multiple boosts read as concentric orbits. Drawn in
// world space (after drawPlayer, inside the camera matrix).
void drawPowerAura(const World *world, const Player *player) {
  float t = (float)GetTime();
  int slot = 0;
  BeginBlendMode(BLEND_ADDITIVE); // the aura is light the blob is giving off
  for (PowerUpType type = 0; type < POWERUP_COUNT; type++) {
    if (world->powerTimer[type] <= 0.0f) {
      continue;
    }
    Color col = POWERUP_DEFS[type].color;
    float r = player->radius * 1.35f + slot * 9.0f;
    float dir = (slot % 2 == 0) ? 1.0f : -1.0f; // alternate spin direction
    float ang = fmodf(t * (90.0f + slot * 40.0f) * dir, 360.0f);
    DrawRing(player->position, r - 3.0f, r, ang, ang + 300.0f, 32,
             Fade(col, 0.85f)); // arc with a gap so the spin is visible
    DrawRing(player->position, r - 3.0f, r, ang, ang + 300.0f, 32,
             Fade(col, 0.25f)); // faint glow doubling
    slot++;
  }
  EndBlendMode();
}

void drawWorld(const World *world) {
  float t = (float)GetTime();

  drawGround();

  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i)
  drawCloudPad(world->platforms[i].position);
  FOREACH_ACTIVE(coins, MAX_COINS, i)
  drawCoin(world->coins[i].position, t);
  FOREACH_ACTIVE(powerups, MAX_POWERUPS, i)
  drawPowerUp(&world->powerups[i], t);
  FOREACH_ACTIVE(monsters, MAX_MONSTERS, i)
  drawMonster(world->monsters[i].position, t);

  drawParticles(world);
  drawFlash(world);
}

bool isPlayerFallen(const World *world, const Player *player) {
  return player->position.y - player->radius > world->cameraY + SCREEN_HEIGHT;
}

int getScore(const World *world) {
  float ascent = FLOOR_Y - world->bestY;
  return (int)(ascent / HEIGHT_PER_POINT) +
         world->coinsCollected * POINTS_PER_COIN + world->comboBonus;
}

#ifdef WORLD_SELFTEST
// Contact strip of one fixed gameplay scene rendered once per era, so the
// entities can be eyeballed without playing to 60,000 units — the sibling of
// background.c's BG_SELFTEST, and the only way to see a monster and a power-up
// at the neon palette without a lucky drop.
//   cc -DWORLD_SELFTEST src/world.c src/background.c src/player.c src/theme.c \
//      src/tex.c src/svg.c src/sound.c src/progress.c src/persist.c \
//      lib/nanosvg_impl.c -Iinclude -Ilib/nanosvg/src -Ilib/raylib/src \
//      build/lib/raylib/raylib/libraylib.a -lm -framework Cocoa \
//      -framework IOKit -framework CoreVideo -framework OpenGL \
//      -o /tmp/worldtest && /tmp/worldtest /tmp/worldstrip.png
#include "background.h"

#include <assert.h>
#include <stdio.h>

#define BAND 20000.0f // see BG_SELFTEST: only has to land inside a band
#define CELL 400

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "worldtest");
  while (!texPreloadStep()) {
  }
  while (!svgPreloadStep()) {
  }
  RenderTexture rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
  Image sheet = GenImageColor(CELL * THEME_COUNT, CELL, BLACK);

  // The scene is built by assignment rather than by running the spawner, so
  // the strip shows the same picture every run and a change to the spawn
  // tables can't quietly empty it. cameraY stays 0, so world y == screen y.
  World w = {0};
  for (int i = 0; i < 8; i++) {
    w.coins[i].position =
        (Vector2){110.0f + i * 80.0f, 210.0f + (i % 3) * 55.0f};
    w.coins[i].active = true;
  }
  w.platforms[0] = (Platform){{80.0f, 470.0f}, true};
  w.platforms[1] = (Platform){{440.0f, 355.0f}, true};
  w.monsters[0] = (Monster){{240.0f, 120.0f}, 1.0f, true};
  w.monsters[1] = (Monster){{640.0f, 560.0f}, -1.0f, true};
  for (int i = 0; i < MAX_POWERUPS; i++) {
    w.powerups[i].position = (Vector2){130.0f + i * 190.0f, 640.0f};
    w.powerups[i].type = (PowerUpType)i; // magnet / rocket / balloon / shield
    w.powerups[i].active = true;
  }
  for (int i = 0; i < MAX_PARTICLES; i++) { // one burst, mid-flight
    float a = i * (2.0f * PI / MAX_PARTICLES);
    w.particles[i].position =
        (Vector2){400.0f + cosf(a) * 70.0f, 300.0f + sinf(a) * 70.0f};
    w.particles[i].velocity = (Vector2){cosf(a), sinf(a)};
    w.particles[i].life = 0.4f + 0.6f * ((i % 5) / 5.0f);
  }
  w.flashPos = (Vector2){400.0f, 300.0f};
  w.flash = 0.7f;
  w.powerTimer[POWERUP_MAGNET] = 100.0f; // two aura rings around the blob
  w.powerTimer[POWERUP_ROCKET] = 100.0f;

  for (int i = 0; i < THEME_COUNT; i++) {
    float cameraY = -(i * BAND + BAND * 0.05f);
    // Ahead of the draw, and not once at setup: gTheme is zero-initialized
    // until the first themeUpdate, so confetti picked before this point is
    // pure transparent black and the whole burst renders invisible.
    themeUpdate(-cameraY);
    for (int k = 0; k < MAX_PARTICLES; k++) {
      w.particles[k].color = gTheme.confetti[k % CONFETTI_COUNT];
    }
    Player p = createPlayer();
    p.position = (Vector2){400.0f, 430.0f};
    p.velocity = (Vector2){0.0f, -30.0f}; // stretched, and a trail behind it
    for (int k = 0; k < TRAIL_LEN; k++) {
      p.trail[k] =
          (Vector2){400.0f - k * 4.0f, 430.0f + (TRAIL_LEN - k) * 16.0f};
    }

    BeginTextureMode(rt);
    ClearBackground(BLACK);
    drawGameBackground(cameraY); // also runs themeUpdate for this band
    drawWorld(&w);
    drawPlayer(&p);
    drawPowerAura(&w, &p);
    EndTextureMode();

    Image im = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&im); // framebuffer textures are bottom-up
    // Every translucent draw blends the target's *alpha* channel down as well
    // as its color, so the readback is a correct picture carrying junk alpha.
    // Left alone, ImageDrawImage would then blend the cell onto the black
    // sheet and every soft-edged sprite would read as a dark smudge — an
    // artifact of this strip, not of the game.
    Color *q = im.data;
    for (int k = 0, n = im.width * im.height; k < n; k++) {
      q[k].a = 255;
    }

    // The two failure modes this art pass can produce without crashing: a
    // texture that never baked leaves entities invisible, and a BeginBlendMode
    // whose EndBlendMode is missing leaks additive blending into everything
    // drawn after it and blows the frame to white.
    const Color *px = im.data;
    long sum = 0;
    int lo = 255, hi = 0;
    for (int k = 0, n = im.width * im.height; k < n; k++) {
      int l = (px[k].r + px[k].g + px[k].b) / 3;
      sum += l;
      lo = (l < lo) ? l : lo;
      hi = (l > hi) ? l : hi;
    }
    int mean = (int)(sum / (im.width * im.height));
    printf("era %d: luma %3d..%3d  mean %3d\n", i, lo, hi, mean);
    assert(hi - lo > 32 && "scene came out flat");
    assert(mean > 8 && mean < 247 && "scene blown to black or white");

    ImageResize(&im, CELL, CELL);
    ImageDrawImage(&sheet, im, i * CELL, 0, WHITE);
    UnloadImage(im);
  }
  ExportImage(sheet, argc > 1 ? argv[1] : "worldstrip.png");
  printf("ok — %d eras\n", THEME_COUNT);
  CloseWindow();
  return 0;
}
#endif
