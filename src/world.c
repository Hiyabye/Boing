#include "world.h"

#include "background.h" // TETRO[] + drawTetromino: a Falling Blocks pad *is* a
                        // tetromino, and shares the backdrop's shape table so
                        // the piece you land on is the piece you can see
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
  THEME_COUNT // first full theme cycle gets 2 coins/row; later cycles single
#define COMBO_TIMEOUT                                                          \
  150.0f                    // frames (~2.5s) w/o a coin before the chain resets
#define COIN_PAIR_GAP 90.0f // horizontal gap of the bonus second coin
#define SPAWN_MARGIN 40.0f  // keep spawns away from screen edges
#define PLATFORM_BOUNCE -28.0f
#define PLATFORM_SPACING 300.0f // vertical distance between platforms
#define PLATFORM_WIDTH 100.0f
#define PLATFORM_HEIGHT 16.0f
// Per-era platform mechanics (stamped at spawn, see worldEra). Hills uses
// neither, so the baseline jumper is exactly what it always was.
#define CLOUD_DRIFT 1.15f    // Birdsong Skies: horizontal platform speed
#define CRUMBLE_FRAMES 24.0f // Night Blocks: frames from landing to break
#define CRUMBLE_JITTER 2.0f  // px of death-rattle shake while it crumbles
#define BLOCK_FALL 1.4f      // Falling Blocks: px/frame a pad descends
#define TETRO_CELL 25.0f     // one tetromino cell (a piece is 2-4 cells wide)
#define HEIGHT_PER_POINT 10.0f
#define POINTS_PER_COIN 5

#define MONSTER_RADIUS 24.0f
#define MONSTER_SPACING 900.0f // vertical distance between monster bands
#define MONSTER_DRIFT 1.5f     // horizontal speed
#define MONSTER_COIN_LOSS 5    // coins scattered/lost on a monster hit
// Neon Maze: a monster stops drifting and hunts the blob (see updateMonsters).
#define GHOST_ACCEL 0.14f // steering accel toward (or away from) the player
#define GHOST_VX 3.4f     // horizontal chase speed cap
#define GHOST_VY                                                               \
  1.2f                     // vertical cap — deliberately far under climb speed,
                           // so a ghost crowds you but can never follow forever
#define GHOST_FLEE 0.55f   // speed multiplier while frightened by a pellet
#define GHOST_EAT_BONUS 25 // points for the first ghost eaten, doubling after
// Altitude difficulty ramp (keyed on the monotonic peak level; tune for feel).
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
#define SHAKE_HIT 14.0f            // screen shake on a monster hit
#define SHAKE_SPLAT 9.0f           // shake on an invincible plow-through splat
#define SHAKE_LAUNCH 6.0f          // shake on floor bounce / rocket pickup
#define SHAKE_DECAY 0.85f          // per-frame shake falloff
#define FLASH_DECAY 0.08f          // pop-ring fade per frame
#define LEVEL_BANNER_FRAMES 120.0f // ~2s of era name under the HUD level bar
#define DEATH_DROP                                                             \
  700.0f // how far below the peak view the camera chases a fall
#define BOTTOM_MARGIN                                                          \
  140.0f // keep the player this far above the screen bottom (see below it)
#define TWO_PI (2.0f * PI) // raylib defines PI

// Power-up table (indexed by PowerUpType). Ordered exactly as the enum.
//              duration color       label     weight shake        invincible
//              debuff
// Unsized on purpose, here *and* in the world.h declaration: either one
// spelling a length gives this definition that length as its composite type,
// and the assert below would then measure the spelling instead of the table.
// This is the one table whose completeness matters at runtime — a missing row
// means a zero duration, which the HUD timer bar divides by.
//
// Rows are positional in exact PowerUpType order, so dropping one anywhere
// shortens the table and fails the count check instead of leaving a hidden
// zero-filled hole.
const PowerUpDef POWERUP_DEFS[] = {
    // HUD timer-bar colors are fixed (not themed): the table is static config.
    // spawnWeight only distributes picks *within* a category — pickPowerup(bad)
    // weighted-picks among the good rows or among the debuff rows. The good-vs-
    // bad ratio itself is the altitude roll in spawnPowerups (0/5 bad at L0, up
    // to 3/5 at L3+), not these weights. Tune knob.
    {480.0f, SKYBLUE, "MAGNET", 3, 0.0f, false, false},       // ~8s
    {150.0f, ORANGE, "ROCKET", 3, SHAKE_LAUNCH, true, false}, // ~2.5s
    {360.0f, LIME, "BALLOON", 3, 0.0f, false, false},         // ~6s
    {1.0f, GOLD, "SHIELD", 3, SHAKE_LAUNCH, true,
     false}, // flag: lasts until a monster hit (excluded from the tick-down;
             // HUD bar stays full)
    // Neon Maze's power pellet. Weight 0 here and overridden per-era in
    // spawnWeightOf, so it can only ever drop on the era that has ghosts.
    {360.0f, YELLOW, "PELLET", 0, SHAKE_LAUNCH, true,
     false}, // ~6s of frightened ghosts
    // Debuffs: no boost, no immunity, SAD reaction + SFX_DEBUFF. All ~3s
    // (short - Anchor/Dizzy are run-ending at 5s).
    {180.0f, DARKGRAY, "ANCHOR", 1, SHAKE_HIT, false, true}, // ~3s
    {180.0f, VIOLET, "DIZZY", 1, SHAKE_HIT, false, true},    // ~3s
    {180.0f, GRAY, "FOG", 1, SHAKE_HIT, false, true},        // ~3s
    {180.0f, BROWN, "BUTTER", 1, SHAKE_HIT, false, true},    // ~3s
};
// A dropped row would leave a zero-duration slot (divide-by-zero in the HUD
// timer bar) — fail the build instead.
STATIC_ASSERT(sizeof(POWERUP_DEFS) / sizeof(POWERUP_DEFS[0]) == POWERUP_COUNT,
              powerups_full);

// One semantic pickup sound per power-up. The sound engine voices it with the
// active era's own instrument palette.
static const SfxId POWERUP_SFX[] = {
    SFX_MAGNET, SFX_ROCKET, SFX_BALLOON, SFX_SHIELD, SFX_PELLET,
    SFX_DEBUFF, SFX_DEBUFF, SFX_DEBUFF,  SFX_DEBUFF,
};
STATIC_ASSERT(sizeof(POWERUP_SFX) / sizeof(POWERUP_SFX[0]) == POWERUP_COUNT,
              powerup_sfx_full);

static float audioPan(float worldX) {
  return clampf(worldX / SCREEN_WIDTH * 2.0f - 1.0f, -0.9f, 0.9f);
}

float worldCameraY(const World *world) { return world->cameraY; }

float worldPeakAscent(const World *world) {
  return fmaxf(0.0f, SCREEN_HEIGHT / 2.0f - world->bestY);
}

float worldViewAscent(const World *world) {
  return fmaxf(0.0f, -world->cameraY);
}

float worldShake(const World *world) { return world->shake; }
int worldLevel(const World *world) {
  return themeSampleAt(worldPeakAscent(world)).level;
}

// Mechanics follow the monotonic peak sample. Entity-local ownership comes in
// Phase 3; until then this preserves the existing global era gate.
EraId worldEra(const World *world) {
  return themeSampleAt(worldPeakAscent(world)).era;
}

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

// Deterministic per-instance seed from an entity's spawn y, so coins get stable
// random-looking variety without a struct field. **Only valid for an entity
// that never moves vertically** — a monster on Neon Maze does, which is why it
// carries a `seed` field stamped from this at spawn instead of calling it live.
static unsigned entSeed(Vector2 p) {
  unsigned h = (unsigned)(int)fabsf(p.y) * 2654435761u;
  h ^= h >> 15;
  return h;
}

// A pad's collision boxes: one board for a plain platform, one per cell for a
// Falling Blocks tetromino. Built from the same offsets drawTetromino draws
// with, so the art and the collision cannot drift apart.
#define PAD_BOXES 4
static int padBoxes(const Platform *pl, Rectangle *out) {
  if (pl->vy <= 0.0f) {
    out[0] = (Rectangle){pl->position.x, pl->position.y, PLATFORM_WIDTH,
                         PLATFORM_HEIGHT};
    return 1;
  }
  for (int k = 0; k < PAD_BOXES; k++) {
    out[k] = (Rectangle){pl->position.x + TETRO[pl->shape][k][0] * TETRO_CELL,
                         pl->position.y + TETRO[pl->shape][k][1] * TETRO_CELL,
                         TETRO_CELL, TETRO_CELL};
  }
  return PAD_BOXES;
}

// Px width of a shape — the spawn clamp needs it, since an I is four cells wide
// and an O is two.
static float tetroWidth(int shape) {
  int w = 0;
  for (int k = 0; k < PAD_BOXES; k++) {
    if (TETRO[shape][k][0] + 1 > w) {
      w = TETRO[shape][k][0] + 1;
    }
  }
  return (float)w * TETRO_CELL;
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
      if (worldLevel(world) < COIN_DOUBLE_CAP) {
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
  // The era is stamped on the pad at birth rather than read at landing time, so
  // a Night Blocks pad can *look* cracked before you commit to it and a pad
  // never changes the rules underfoot as the band boundary passes.
  EraId e = worldEra(world);
  while (world->platformSpawnY > world->cameraY - SCREEN_HEIGHT) {
    FIND_FREE(s, platforms, MAX_PLATFORMS);
    if (s >= 0) {
      // Every piece is drawn, so the clamp has to use *its* width — an I spans
      // four cells and an O two.
      int shape = GetRandomValue(0, TETRO_COUNT - 1);
      float w = (e == ERA_BLOCKS) ? tetroWidth(shape) : PLATFORM_WIDTH;
      float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN - w);
      float vx = e == ERA_SKIES
                     ? (GetRandomValue(0, 1) ? CLOUD_DRIFT : -CLOUD_DRIFT)
                     : 0.0f;
      world->platforms[s] = (Platform){
          {x, world->platformSpawnY},          true, e == ERA_NIGHT, 0.0f, vx,
          e == ERA_BLOCKS ? BLOCK_FALL : 0.0f, shape};
    }
    world->platformSpawnY -= PLATFORM_SPACING;
  }
}

// Difficulty level for the altitude ramp: the monotonic climb level, capped at
// DIFFICULTY_CAP (0..CAP). Drives monster density/speed/spacing and the debuff
// spawn odds.
static int difficulty(const World *world) {
  int level = worldLevel(world);
  return level < DIFFICULTY_CAP ? level : DIFFICULTY_CAP;
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
        // vy stays 0: only a Neon Maze ghost ever leaves its spawn row, and
        // steerGhost is what gives it one.
        Vector2 at = {x, world->monsterSpawnY};
        world->monsters[s] = (Monster){at, vx, true, 0.0f, entSeed(at)};
      }
    }
    float step = MONSTER_SPACING - DIFFICULTY_TIGHTEN * d;
    world->monsterSpawnY -=
        step < MONSTER_SPACING_MIN ? MONSTER_SPACING_MIN : step;
  }
}

// Spawn-axis upgrade: extra weight for an upgraded good power-up so it
// dominates good picks. Debuffs (and untracked types) get no bonus.
static int spawnWeightOf(PowerUpType t, EraId era) {
  int bonus = 0;
  switch (t) {
  case POWERUP_PELLET:
    // Era-gated rather than table-weighted: the pellet only means anything
    // where there are ghosts to frighten, and a 0 total weight is enough to
    // make pickPowerup unable to choose it anywhere else.
    return era == ERA_MAZE ? 4 : 0;
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
static PowerUpType pickPowerup(bool debuff, EraId era) {
  int total = 0;
  for (int k = 0; k < POWERUP_COUNT; k++) {
    if (POWERUP_DEFS[k].debuff == debuff) {
      total += spawnWeightOf(k, era);
    }
  }
  int roll = GetRandomValue(0, total - 1);
  for (PowerUpType t = 0; t < POWERUP_COUNT; t++) {
    if (POWERUP_DEFS[t].debuff == debuff) {
      int w = spawnWeightOf(t, era);
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
  EraId e = worldEra(world);
  while (world->powerupSpawnY > world->cameraY - SCREEN_HEIGHT) {
    FIND_FREE(s, powerups, MAX_POWERUPS);
    if (s >= 0) {
      float x = GetRandomValue(SPAWN_MARGIN, SCREEN_WIDTH - SPAWN_MARGIN);
      PowerUpType t = pickPowerup(GetRandomValue(0, 4) < d, e);
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
      sfxAt(SFX_COIN, pitch, audioPan(world->coins[i].position.x));
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
        // Bad pickup: no upward kick, sour face, its own warning cue.
        player->expression = EXPR_SAD;
      } else {
        player->velocity.y = COIN_BOOST;
        player->expression = EXPR_HAPPY;
      }
      sfxAt(POWERUP_SFX[t], 1.0f, audioPan(world->powerups[i].position.x));
    }
  }
}

// Taking a hit — the one definition of it, shared by monster contact and a
// Falling Blocks crush. Mega-Jump-style: it costs coins (they scatter out) and
// knocks you down, but does NOT kill; only a fall ends the run.
static void hitPlayer(World *world, Player *player) {
  int lost = world->coinsCollected < MONSTER_COIN_LOSS ? world->coinsCollected
                                                       : MONSTER_COIN_LOSS;
  world->coinsCollected -= lost;
  scatterCoins(world, player->position, lost); // only the coins actually lost
  world->shake = SHAKE_HIT;
  world->hitstop = 2;            // brief freeze reads as impact
  world->combo = 0;              // hit broke the chain
  player->velocity.y = 12.0f;    // knocked downward
  player->expression = EXPR_SAD; // shakes off on the next coin/power-up
  sfxAt(SFX_HIT, 1.0f, audioPan(player->position.x));
}

// Plowing through a monster while invincible. Also the ghost-eat on Neon Maze:
// there it chains, each ghost on one pellet worth double the last.
static void splatMonster(World *world, Monster *m) {
  spawnParticles(world, m->position, GRAPE, 12, false);
  m->active = false;
  world->shake = fmaxf(world->shake, SHAKE_SPLAT);
  world->hitstop = 3; // brief freeze reads as impact
  // Shield absorbs this hit; break only once its charges run out (Effect
  // upgrade grants extra charges). Rocket-only invincibility leaves the
  // shield untouched (shieldHits is 0 when no shield is active).
  if (world->powerTimer[POWERUP_SHIELD] > 0.0f && --world->shieldHits <= 0) {
    world->powerTimer[POWERUP_SHIELD] = 0.0f;
  }
  float pitch = 1.0f;
  if (world->powerTimer[POWERUP_PELLET] > 0.0f) {
    int n = world->ghostChain < 3 ? world->ghostChain : 3; // 25/50/100/200
    world->comboBonus += GHOST_EAT_BONUS << n;
    world->ghostChain++;
    pitch += 0.15f * (float)n;
  }
  sfxAt(SFX_SPLAT, pitch, audioPan(m->position.x));
}

// Neon Maze steering: the drifting monster becomes a ghost that hunts the blob,
// and runs from it while a pellet is lit. The vertical cap is the whole reason
// this is survivable — a ghost crowds your climb, it never keeps pace with it.
static void steerGhost(Monster *m, const Player *player, bool frightened) {
  float dir = frightened ? -1.0f : 1.0f;
  float top = frightened ? GHOST_FLEE : 1.0f;
  m->vx += copysignf(GHOST_ACCEL, (player->position.x - m->position.x) * dir);
  m->vy += copysignf(GHOST_ACCEL, (player->position.y - m->position.y) * dir);
  m->vx = clampf(m->vx, -GHOST_VX * top, GHOST_VX * top);
  m->vy = clampf(m->vy, -GHOST_VY * top, GHOST_VY * top);
}

static void updateMonsters(World *world, Player *player) {
  bool ghosts = worldEra(world) == ERA_MAZE;
  bool frightened = world->powerTimer[POWERUP_PELLET] > 0.0f;
  FOREACH_ACTIVE(monsters, MAX_MONSTERS, i) {
    Monster *m = &world->monsters[i];
    if (ghosts) {
      steerGhost(m, player, frightened);
    }
    m->position.x += m->vx;
    m->position.y += m->vy;
    if (m->position.x < MONSTER_RADIUS ||
        m->position.x > SCREEN_WIDTH - MONSTER_RADIUS) {
      m->vx = -m->vx;
    }
    if (CheckCollisionCircles(player->position, player->radius, m->position,
                              MONSTER_RADIUS)) {
      if (worldInvincible(world)) {
        splatMonster(world, m);
      } else {
        m->active = false;
        musicMonsterHit();
        hitPlayer(world, player);
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
    Platform *pl = &world->platforms[i];
    Rectangle box[PAD_BOXES];
    int n = padBoxes(pl, box);
    // Land on the *topmost* crossed box: a tetromino stacks cells in a column
    // (S and Z leave a lower cell exposed, the rest do not), and dropping onto
    // a buried cell would put the blob inside the piece.
    float top = 0.0f;
    bool landed = false;
    for (int k = 0; k < n; k++) {
      // The sweep is in the *platform's* frame: updatePlatforms has already
      // moved it this step, so a box's previous top edge is box.y - vy. A
      // falling pad would otherwise be tunnelled by its own motion.
      if (prevFeet <= box[k].y - pl->vy && feet >= box[k].y &&
          player->position.x >= box[k].x &&
          player->position.x <= box[k].x + box[k].width &&
          (!landed || box[k].y < top)) {
        top = box[k].y;
        landed = true;
      }
    }
    if (landed) {
      player->position.y = top - player->radius;
      player->velocity.y = PLATFORM_BOUNCE;
      spawnParticles(world, (Vector2){player->position.x, top}, CLOUD, 8,
                     false);
      sfxAt(SFX_BOUNCE, 1.0f, audioPan(player->position.x));
      if (pl->fragile && pl->fuse <= 0.0f) {
        pl->fuse =
            CRUMBLE_FRAMES; // Night Blocks: it holds for a beat, then goes
      }
      return;
    }
  }
}

// Platform motion + the Night Blocks crumble countdown. Runs before
// bounceOnPlatforms so a moving pad's landing test sees this step's position.
static void updatePlatforms(World *world) {
  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i) {
    Platform *pl = &world->platforms[i];
    pl->position.x += pl->vx;
    pl->position.y += pl->vy;
    if (pl->vx != 0.0f) {
      if (pl->position.x <= 0.0f) {
        pl->position.x = 0.0f;
        pl->vx = fabsf(pl->vx);
      } else if (pl->position.x + PLATFORM_WIDTH >= SCREEN_WIDTH) {
        pl->position.x = SCREEN_WIDTH - PLATFORM_WIDTH;
        pl->vx = -fabsf(pl->vx);
      }
    }

    if (pl->fuse > 0.0f && (pl->fuse -= 1.0f) <= 0.0f) {
      spawnParticles(world,
                     (Vector2){pl->position.x + PLATFORM_WIDTH / 2.0f,
                               pl->position.y + PLATFORM_HEIGHT / 2.0f},
                     CLOUD, 10, false);
      sfxAt(SFX_CRUMBLE, 1.0f,
            audioPan(pl->position.x + PLATFORM_WIDTH / 2.0f));
      pl->active = false;
      continue;
    }
  }
}

// Falling Blocks: a descending piece caught from below or the side. Runs
// *after* bounceOnPlatforms, which is what makes the exclusion test trivial —
// a legitimate landing has already snapped the player's center a full radius
// above the surface, so anything still overlapping came in the wrong way.
// (Ordered before it instead, a fast enough fall would put the center inside
// the cell on the contact frame and get crushed by a pad it should have
// bounced off.)
static void crushOnBlocks(World *world, Player *player) {
  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i) {
    Platform *pl = &world->platforms[i];
    if (pl->vy <= 0.0f) {
      continue;
    }
    // Per cell, so the notch in a T or an L is genuinely empty space.
    Rectangle box[PAD_BOXES];
    int n = padBoxes(pl, box);
    for (int k = 0; k < n; k++) {
      if (player->position.y > box[k].y + 4.0f &&
          CheckCollisionCircleRec(player->position, player->radius, box[k])) {
        spawnParticles(world, player->position, CLOUD, 10, false);
        pl->active = false;
        if (worldInvincible(world)) {
          world->shake = fmaxf(world->shake, SHAKE_SPLAT);
          sfxAt(SFX_SPLAT, 0.82f, audioPan(player->position.x));
        } else {
          hitPlayer(world, player);
        }
        return;
      }
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
    sfxAt(SFX_BOUNCE, 0.92f, audioPan(player->position.x));
  }

  updatePlatforms(world);
  bounceOnPlatforms(world, player);
  crushOnBlocks(world, player);
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
  // The eat-chain belongs to one pellet. Clearing it whenever no pellet is lit
  // covers both ends: it can't survive the timer running out, and it is already
  // zero by the time the next pellet is picked up.
  if (world->powerTimer[POWERUP_PELLET] <= 0.0f) {
    world->ghostChain = 0;
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
  float peakAscent = worldPeakAscent(world);
  float peakY = -peakAscent;
  float need =
      player->position.y + player->radius - SCREEN_HEIGHT + BOTTOM_MARGIN;
  float floorY = peakY + DEATH_DROP;
  world->cameraY = clampf(need, peakY, floorY);

  // Level-up fanfare on first arrival in a new theme band. Keyed off the peak
  // (monotonic), so a fall-and-reclimb never re-triggers it.
  ThemeSample journey = themeSampleAt(peakAscent);
  if (journey.level > world->level) {
    world->level = journey.level;
    world->levelBanner = LEVEL_BANNER_FRAMES; // the era's rules just changed;
                                              // the HUD says which era it is
    musicEraTransition(THEMES[journey.era].track);
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
  if (world->levelBanner > 0.0f) {
    world->levelBanner -= 1.0f;
  }
  spawnCoins(world);
  spawnPlatforms(world);
  spawnMonsters(world);
  spawnPowerups(world);
  cullBelowView(world);
}

// One additive soft-light stamp — the shared idiom behind every glow in this
// file (coin, monster, power-up, pop ring, particles). `w` is the full box
// width, so a glow of radius r asks for 2*r.
//
// The theme's bloom weight is *added to* a base rather than replacing it: the
// day era's weight is deliberately small, and using it alone would leave the
// daylight coins flatter than they were before any of this. So Sunny Hills
// keeps the light it always had and the neon eras blaze on top of it.
static void glowStamp(Vector2 c, float w, Color tint, float base, float lit,
                      float glow) {
  BeginBlendMode(BLEND_ADDITIVE);
  drawTex(TEX_PUFF, c.x, c.y, w, w, 0.0f, Fade(tint, base + lit * glow));
  EndBlendMode();
}

static void drawCoin(Vector2 c, float t, bool pellet, float glow) {
  if (pellet) {
    // Neon Maze: the coin trail is a pellet lane. A pellet is a dot — the
    // minted-coin art below would fight the backdrop's own pellets.
    float r = COIN_RADIUS * 0.55f;
    glowStamp(c, COIN_RADIUS * 3.5f, COIN_HI, 0.25f, 0.45f, glow);
    DrawCircle(c.x, c.y, r, COIN_HI);
    DrawCircle(c.x - r * 0.3f, c.y - r * 0.3f, r * 0.34f, RAYWHITE);
    return;
  }
  unsigned seed = entSeed(c);
  float phase = (seed & 255) / 255.0f * TWO_PI; // out-of-sync twinkle offset
  float spin = fabsf(cosf(t * 4.0f + c.y * 0.05f)); // 0 (edge-on) .. 1 (face)
  float w = COIN_RADIUS * (0.22f + 0.78f * spin);
  glowStamp(c, COIN_RADIUS * 4.5f, COIN_HI, 0.22f, 0.3f, glow);
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

// True when a tetromino has a cell at this local grid coordinate.
static bool tetroCellAt(int shape, int x, int y) {
  for (int k = 0; k < PAD_BOXES; k++) {
    if (TETRO[shape][k][0] == x && TETRO[shape][k][1] == y) {
      return true;
    }
  }
  return false;
}

// Falling Blocks pads share the backdrop's shapes, but not its visual weight:
// a dark silhouette pulls them forward, white rails mark safe landing tops,
// and warm exposed sides mark the crush zone. Short trails make their descent
// readable even when the player and camera are both moving.
static void drawFallingPiece(const Platform *pl, Vector2 p, float a,
                             float glow) {
  Color c = gTheme.confetti[pl->shape % CONFETTI_COUNT];
  Color outline = {0x04, 0x0A, 0x0D, 0xFF};
  Color danger = {0xFF, 0x55, 0x66, 0xFF};
  float w = tetroWidth(pl->shape);
  glowStamp((Vector2){p.x + w * 0.5f, p.y + TETRO_CELL}, w * 1.4f, c, 0.10f * a,
            0.30f * a, glow);

  for (int k = 0; k < PAD_BOXES; k++) {
    int gx = TETRO[pl->shape][k][0], gy = TETRO[pl->shape][k][1];
    float x = p.x + gx * TETRO_CELL, y = p.y + gy * TETRO_CELL;
    if (!tetroCellAt(pl->shape, gx, gy - 1)) {
      DrawLineEx((Vector2){x + 7.0f, y - 20.0f}, (Vector2){x + 7.0f, y - 7.0f},
                 2.0f, Fade(c, 0.3f * a));
      DrawLineEx((Vector2){x + 18.0f, y - 13.0f},
                 (Vector2){x + 18.0f, y - 5.0f}, 1.5f,
                 Fade(RAYWHITE, 0.22f * a));
    }
    DrawRectangle(x - 2.0f, y - 2.0f, TETRO_CELL + 4.0f, TETRO_CELL + 4.0f,
                  Fade(outline, a));
  }

  drawTetromino(p.x, p.y, TETRO_CELL, pl->shape, c, a);
  for (int k = 0; k < PAD_BOXES; k++) {
    int gx = TETRO[pl->shape][k][0], gy = TETRO[pl->shape][k][1];
    float x = p.x + gx * TETRO_CELL, y = p.y + gy * TETRO_CELL;
    if (!tetroCellAt(pl->shape, gx, gy - 1)) {
      DrawRectangle(x + 2.0f, y + 2.0f, TETRO_CELL - 4.0f, 4.0f,
                    Fade(RAYWHITE, a));
    }
    if (!tetroCellAt(pl->shape, gx, gy + 1)) {
      DrawRectangle(x + 3.0f, y + TETRO_CELL - 6.0f, TETRO_CELL - 6.0f, 4.0f,
                    Fade(danger, a));
    }
    if (!tetroCellAt(pl->shape, gx - 1, gy)) {
      DrawRectangle(x + 2.0f, y + 8.0f, 3.0f, TETRO_CELL - 11.0f,
                    Fade(danger, a));
    }
    if (!tetroCellAt(pl->shape, gx + 1, gy)) {
      DrawRectangle(x + TETRO_CELL - 5.0f, y + 8.0f, 3.0f, TETRO_CELL - 11.0f,
                    Fade(danger, a));
    }
  }
}

static void drawCloudPad(const Platform *pl, float glow) {
  Vector2 p = pl->position;
  float a = 1.0f;
  if (pl->fuse > 0.0f) {
    // Death rattle: the pad shakes and thins out over its fuse, so the break is
    // telegraphed for the whole beat it takes rather than just happening.
    a = pl->fuse / CRUMBLE_FRAMES;
    p.x += GetRandomValue(-100, 100) / 100.0f * CRUMBLE_JITTER;
    p.y += GetRandomValue(-100, 100) / 100.0f * CRUMBLE_JITTER;
  }
  if (pl->vy > 0.0f) {
    drawFallingPiece(pl, p, a, glow);
    return;
  }
  if (pl->vx != 0.0f) {
    float edge = pl->vx > 0.0f ? p.x : p.x + PLATFORM_WIDTH;
    float dir = pl->vx > 0.0f ? -1.0f : 1.0f;
    for (int k = 0; k < 3; k++) {
      float y = p.y + 3.0f + k * 5.0f;
      DrawLineEx((Vector2){edge + dir * (12.0f + k * 7.0f), y},
                 (Vector2){edge + dir * (34.0f + k * 8.0f), y}, 2.0f,
                 Fade(SUN_CORE, 0.55f * a));
    }
  }
  float cx = p.x + PLATFORM_WIDTH / 2.0f;
  drawTex(TEX_PUFF, cx, p.y + PLATFORM_HEIGHT + 6.0f, PLATFORM_WIDTH * 1.5f,
          40.0f, 0.0f, Fade(SKY_BOT_HIGH, 0.5f * a)); // soft shadow
  // Cloud-puff silhouette behind the board, as a soft backing accent. Same
  // TEX_CLOUD the backdrop's clouds are made of, so a pad reads as the same
  // weather as the sky it hangs in.
  drawTex(TEX_CLOUD, p.x + 24.0f, p.y, 62.0f, 50.0f, 0.0f, Fade(CLOUD, a));
  drawTex(TEX_CLOUD, p.x + PLATFORM_WIDTH - 24.0f, p.y, 62.0f, 50.0f, 0.0f,
          Fade(CLOUD, a));
  drawTex(TEX_CLOUD, cx, p.y - 5.0f, 78.0f, 62.0f, 0.0f, Fade(CLOUD, a));

  // The landing board: a defined solid surface so the jump-off edge reads
  // clearly against the sky. Dark outline + body + a bright top face strip.
  Rectangle board = {p.x, p.y, PLATFORM_WIDTH, PLATFORM_HEIGHT};
  DrawRectangleRounded(board, 0.6f, 8,
                       Fade(pl->fragile ? CLOUD_SHADE : CLOUD, a));
  DrawRectangleRounded(
      (Rectangle){p.x, p.y, PLATFORM_WIDTH, PLATFORM_HEIGHT * 0.4f}, 0.9f, 8,
      Fade(RAYWHITE, a)); // lit top face (where feet land)
  DrawRectangleRounded((Rectangle){p.x, p.y + PLATFORM_HEIGHT * 0.55f,
                                   PLATFORM_WIDTH, PLATFORM_HEIGHT * 0.45f},
                       0.9f, 8, Fade(CLOUD_SHADE, 0.85f * a));  // shaded under
  DrawRectangleRoundedLines(board, 0.6f, 8, Fade(GRASS_DK, a)); // crisp rim
  if (pl->fragile) {
    // Two cracks: the pad announces it will break *before* you commit to it.
    Color crack = Fade(GRASS_DK, 0.8f * a);
    DrawLineEx((Vector2){p.x + 30.0f, p.y + 1.0f},
               (Vector2){p.x + 22.0f, p.y + PLATFORM_HEIGHT - 1.0f}, 2.0f,
               crack);
    DrawLineEx((Vector2){p.x + 64.0f, p.y + 1.0f},
               (Vector2){p.x + 72.0f, p.y + PLATFORM_HEIGHT - 1.0f}, 2.0f,
               crack);
  }
}

// `pellet` is the frightened timer (0 = hunting): on Neon Maze a lit pellet
// turns the monsters blue and running, flashing white as it wears off — the
// player's cue that the window is closing.
static void drawMonster(Vector2 m, float t, float pellet, unsigned seed,
                        float glow) {
  float R = MONSTER_RADIUS;
  float wob = 1.0f + 0.08f * sinf(t * 6.0f + m.x * 0.1f);
  Color body = GRAPE, bodyDk = GRAPE_DK;
  if (pellet > 0.0f) {
    bool blink = pellet < 60.0f && fmodf(pellet, 16.0f) < 8.0f;
    body = blink ? RAYWHITE : (Color){0x3A, 0x4C, 0xF0, 0xFF};
    bodyDk = blink ? LIGHTGRAY : (Color){0x1B, 0x22, 0x99, 0xFF};
  }
  Color horn = ColorBrightness(bodyDk, -0.15f);

  drawTex(TEX_PUFF, m.x, m.y + R * 1.05f, R * 2.6f, R * 0.9f, 0.0f,
          Fade(BLACK, 0.3f)); // drop shadow
  glowStamp(m, R * 3.5f, body, 0.2f, 0.45f, glow);

  // Wobbling stubby legs (2 or 3, seed-picked).
  int legs = (seed & 1) ? 3 : 2;
  for (int k = 0; k < legs; k++) {
    float lx = m.x + (k - (legs - 1) * 0.5f) * R * 0.5f;
    float ly = m.y + R * 0.7f + sinf(t * 7.0f + k + m.x * 0.1f) * R * 0.14f;
    DrawCircle(lx, ly + R * 0.18f, R * 0.2f, bodyDk);
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
    DrawCircle(ax, m.y - R * 1.3f, R * 0.16f, body);
    break;
  }
  default:
    break;
  }

  // Body: dark base, brightened rim, lit body.
  DrawEllipse(m.x, m.y, R * wob, R / wob, bodyDk);
  DrawEllipse(m.x, m.y, R * 0.9f * wob, R * 0.9f / wob,
              ColorBrightness(body, 0.3f));
  DrawEllipse(m.x, m.y - R * 0.1f, R * 0.78f * wob, R * 0.78f / wob, body);

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

static void drawPowerUp(const PowerUp *pu, float t, float glow) {
  Vector2 c = {pu->position.x,
               pu->position.y + sinf(t * 3.0f + pu->position.x * 0.1f) * 4.0f};
  float R = POWERUP_RADIUS;
  float pulse = 0.25f + 0.12f * sinf(t * 4.0f + pu->position.y * 0.05f);
  glowStamp(c, R * (4.0f + 0.4f * sinf(t * 4.0f)), RAYWHITE, pulse, 0.25f,
            glow); // pulsing halo
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
  case POWERUP_PELLET: {
    // A power pellet is a fat dot — no glyph, and none wanted: the shape is
    // the whole point of the reference, and it pulses to sell "grab me".
    float r = R * (0.62f + 0.08f * sinf(t * 6.0f));
    DrawCircle(c.x, c.y, r, COIN_HI);
    DrawCircle(c.x - r * 0.3f, c.y - r * 0.3f, r * 0.34f, RAYWHITE);
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
  case POWERUP_ROCKET: {
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
  case POWERUP_COUNT:
    break;
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
static void drawParticles(const World *world, float glow) {
  BeginBlendMode(BLEND_ADDITIVE);
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (world->particles[i].life <= 0.0f) {
      continue;
    }
    Particle pt = world->particles[i];
    float sz = 26.0f * pt.life;
    float ang = atan2f(pt.velocity.y, pt.velocity.x) * RAD2DEG;
    drawTex(TEX_PUFF, pt.position.x, pt.position.y, sz, sz * 0.5f, ang,
            Fade(pt.color, pt.life * (0.4f + 0.6f * glow)));
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
static void drawFlash(const World *world, float glow) {
  if (world->flash > 0.0f) {
    float r = COIN_RADIUS + (1.0f - world->flash) * 42.0f;
    glowStamp(world->flashPos, r * 2.4f, RAYWHITE, world->flash * 0.5f, 0.3f,
              glow);
    DrawRing(world->flashPos, r * 0.72f, r, 0, 360, 24,
             Fade(RAYWHITE, world->flash));
  }
}

// A living light field behind the player. Each active effect keeps its HUD
// color in one slow wisp and mote, while broad offset puffs blend into a soft
// breathing halo. The bright wisp cores feed the existing post bloom, so this
// needs no second shader or render target. Draw before the blob for clean
// silhouette and face readability.
void drawPowerAura(const World *world, const Player *player) {
  float t = (float)GetTime(), r = player->radius;
  int slot = 0;
  BeginBlendMode(BLEND_ADDITIVE);
  for (PowerUpType type = 0; type < POWERUP_COUNT; type++) {
    float timer = world->powerTimer[type];
    if (timer <= 0.0f) {
      continue;
    }
    PowerUpDef def = POWERUP_DEFS[type];
    float life = def.duration <= 1.0f ? 1.0f : fminf(timer / 30.0f, 1.0f);
    float dir = (slot & 1) ? -1.0f : 1.0f;
    float phase = type * 2.39996f + t * (0.55f + slot * 0.08f) * dir;
    float breathe = 1.0f + 0.09f * sinf(t * 2.2f + type);
    Vector2 axis = {cosf(phase), sinf(phase)};
    Vector2 glow = {player->position.x + axis.x * r * 0.18f,
                    player->position.y + axis.y * r * 0.18f};
    Vector2 wisp = {player->position.x + axis.x * r * 1.45f,
                    player->position.y + axis.y * r * 1.45f};
    Vector2 mote = {player->position.x + axis.x * r * 2.0f,
                    player->position.y + axis.y * r * 2.0f};

    drawTex(TEX_PUFF, glow.x, glow.y, r * 5.4f * breathe, r * 4.8f * breathe,
            0.0f, Fade(def.color, 0.20f * life));
    drawTex(TEX_PUFF, wisp.x, wisp.y, r * 1.9f, r * 0.65f,
            phase * RAD2DEG + 90.0f, Fade(def.color, 0.72f * life));
    float spark = 1.5f + 1.2f * (0.5f + 0.5f * sinf(t * 5.0f + type));
    DrawCircleV(mote, spark * 2.4f, Fade(def.color, 0.35f * life));
    DrawCircleV(mote, spark, Fade(RAYWHITE, 0.78f * life));
    slot++;
  }
  EndBlendMode();
}

void drawWorld(const World *world) {
  ThemeSample sample = themeSampleAt(worldViewAscent(world));
  themeUpdate(sample);
  float glow = themeGlow(sample);
  float t = (float)GetTime();

  drawGround();

  // Two era looks are read off world state rather than the entity: coins are a
  // pellet lane on Neon Maze, and its monsters turn frightened while a pellet
  // is lit. Everything else about an entity's look rides its own fields.
  bool pelletLane = worldEra(world) == ERA_MAZE;
  float fright = world->powerTimer[POWERUP_PELLET];

  FOREACH_ACTIVE(platforms, MAX_PLATFORMS, i)
  drawCloudPad(&world->platforms[i], glow);
  FOREACH_ACTIVE(coins, MAX_COINS, i)
  drawCoin(world->coins[i].position, t, pelletLane, glow);
  FOREACH_ACTIVE(powerups, MAX_POWERUPS, i)
  drawPowerUp(&world->powerups[i], t, glow);
  FOREACH_ACTIVE(monsters, MAX_MONSTERS, i)
  drawMonster(world->monsters[i].position, t, fright, world->monsters[i].seed,
              glow);

  drawParticles(world, glow);
  drawFlash(world, glow);
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
// entities can be eyeballed without playing to 80,000 units - the sibling of
// background.c's BG_SELFTEST, and the only way to see a monster and a power-up
// at the neon palette without a lucky drop.
//   cc -DWORLD_SELFTEST src/world.c src/background.c src/player.c src/theme.c \
//      src/tex.c src/svg.c src/sound.c src/progress.c src/persist.c \
//      lib/nanosvg_impl.c -Iinclude -Ilib/nanosvg/src -Ilib/raylib/src \
//      build/lib/raylib/raylib/libraylib.a -lm -framework Cocoa \
//      -framework IOKit -framework CoreVideo -framework OpenGL \
//      -o /tmp/worldtest && /tmp/worldtest /tmp/worldstrip.png
#include <assert.h>
#include <limits.h>
#include <stdio.h>

#define BAND 20000.0f // see BG_SELFTEST: only has to land inside a band
#define CELL 400

static World worldAtLevel(int level) {
  World w = createWorld();
  float ascent = (level + 0.05f) * BAND;
  w.level = level;
  w.bestY = SCREEN_HEIGHT / 2.0f - ascent;
  w.cameraY = -ascent;
  w.spawnY -= ascent;
  w.platformSpawnY -= ascent;
  w.monsterSpawnY -= ascent;
  w.powerupSpawnY -= ascent;
  return w;
}

// A world with the spawners silenced (their cursors parked below every
// reachable camera position), so a scene is exactly what the check places in
// it.
static World bareWorld(int level) {
  World w = worldAtLevel(level);
  w.spawnY = w.platformSpawnY = w.monsterSpawnY = w.powerupSpawnY = -1.0e6f;
  return w;
}

// The per-era mechanics, driven through updateWorld. These are the failures the
// contact strip below cannot see: an era gate that stops firing leaves the art
// intact and the game silently back to one mechanic for the whole climb.
static void checkEras(void) {
  // Player movement stops at both walls instead of wrapping across the screen.
  {
    Player p = createPlayer();
    p.acceleration = (Vector2){0.0f, 0.0f};
    p.position.x = p.radius;
    p.velocity.x = -8.0f;
    updatePlayer(&p);
    assert(p.position.x == p.radius && p.velocity.x == 0.0f &&
           "player crossed the left wall");
    p.position.x = SCREEN_WIDTH - p.radius;
    p.velocity.x = 8.0f;
    updatePlayer(&p);
    assert(p.position.x == SCREEN_WIDTH - p.radius && p.velocity.x == 0.0f &&
           "player crossed the right wall");
  }

  // The coin bank saturates instead of overflowing on an extreme save value.
  {
    Progress p = {.coinBank = INT_MAX - 1};
    World w = {.coinsCollected = 2};
    bankRun(&p, &w);
    assert(p.coinBank == INT_MAX && "coin bank overflowed");
  }

  // Peak progression stays monotonic while the camera can reveal an earlier
  // view sample again during a fall.
  {
    World w = worldAtLevel(1);
    w.cameraY = -BAND * 0.75f;
    ThemeSample peak = themeSampleAt(worldPeakAscent(&w));
    ThemeSample view = themeSampleAt(worldViewAscent(&w));
    assert(peak.level == 1 && peak.era == ERA_SKIES);
    assert(view.level == 0 && view.era == ERA_HILLS && view.phase == 0.75f);
    w.cameraY = -BAND * 0.25f;
    assert(themeSampleAt(worldPeakAscent(&w)).level == peak.level);
    assert(themeSampleAt(worldViewAscent(&w)).phase == 0.25f);
  }

  // Table values the completeness assert cannot prove. Every timer must be
  // drawable, and every era needs at least one weighted choice per category.
  for (int t = 0; t < POWERUP_COUNT; t++) {
    assert(POWERUP_DEFS[t].duration > 0.0f && "non-positive power-up duration");
    assert(POWERUP_DEFS[t].spawnWeight >= 0 && "negative power-up weight");
  }

  // The spawn gate: what stamps an era's rule onto an entity at birth.
  for (EraId e = ERA_HILLS; e < ERA_COUNT; e++) {
    assert(THEMES[e].name != NULL && THEMES[e].name[0] != '\0');
    assert(THEMES[e].glow >= 0.0f && THEMES[e].glow <= 1.0f);
    assert(THEMES[e].bridge == (BridgeStyle)e);
    assert(THEMES[e].track >= 0 && THEMES[e].track < TRACK_GAMEPLAY_COUNT);
    int goodWeight = 0, badWeight = 0;
    for (int t = 0; t < POWERUP_COUNT; t++) {
      int weight = spawnWeightOf((PowerUpType)t, e);
      assert(weight >= 0 && "negative effective power-up weight");
      if (POWERUP_DEFS[t].debuff) {
        badWeight += weight;
      } else {
        goodWeight += weight;
      }
    }
    assert(goodWeight > 0 && badWeight > 0 && "empty power-up category");

    World w = worldAtLevel(e);
    Player p = createPlayer();
    updateWorld(&w, &p); // one call generates a screen's worth of pads
    int drifting = 0, fragile = 0, falling = 0, total = 0;
    for (int i = 0; i < MAX_PLATFORMS; i++) {
      if (!w.platforms[i].active) {
        continue;
      }
      total++;
      drifting += w.platforms[i].vx != 0.0f;
      fragile += w.platforms[i].fragile;
      falling += w.platforms[i].vy > 0.0f;
    }
    assert(total > 0 && "no platforms spawned at all");
    assert(drifting == (e == ERA_SKIES ? total : 0) &&
           "drift gate is not Birdsong Skies");
    assert(fragile == (e == ERA_NIGHT ? total : 0) &&
           "crumble gate is not Night Blocks");
    assert(falling == (e == ERA_BLOCKS ? total : 0) &&
           "falling gate is not Falling Blocks");
    // The pellet is reachable only where there are ghosts to frighten.
    assert((spawnWeightOf(POWERUP_PELLET, e) > 0) == (e == ERA_MAZE));
    for (int k = 0; k < 50; k++) {
      PowerUpType good = pickPowerup(false, e);
      PowerUpType bad = pickPowerup(true, e);
      assert(!POWERUP_DEFS[good].debuff && POWERUP_DEFS[bad].debuff);
      assert((good == POWERUP_PELLET) <= (e == ERA_MAZE));
    }
  }

  // Every tetromino is a real spawnable pad: four distinct cells, inside its
  // own reported width, and narrow enough to fit between the spawn margins. A
  // zero-filled TETRO row (the failure a designated table hides) collapses all
  // four cells onto one square and trips the distinctness check.
  for (int s = 0; s < TETRO_COUNT; s++) {
    Platform pl = {{0.0f, 0.0f}, true, false, 0.0f, 0.0f, BLOCK_FALL, s};
    Rectangle b[PAD_BOXES];
    assert(padBoxes(&pl, b) == PAD_BOXES);
    assert(tetroWidth(s) <= SCREEN_WIDTH - 2.0f * SPAWN_MARGIN);
    for (int k = 0; k < PAD_BOXES; k++) {
      assert(b[k].x >= 0.0f && b[k].x + b[k].width <= tetroWidth(s));
      for (int j = k + 1; j < PAD_BOXES; j++) {
        assert((b[k].x != b[j].x || b[k].y != b[j].y) && "duplicate cell");
      }
    }
  }

  // Night Blocks: landing arms the fuse, and the fuse takes the pad with it.
  {
    World w = bareWorld(ERA_NIGHT);
    Player p = createPlayer();
    p.position = (Vector2){400.0f, w.bestY};
    p.velocity = (Vector2){0.0f, 10.0f};
    w.platforms[0] =
        (Platform){{350.0f, w.bestY + 40.0f}, true, true, 0.0f, 0.0f, 0.0f, 0};
    for (int i = 0; i < 20 && w.platforms[0].fuse <= 0.0f; i++) {
      updatePlayer(&p);
      updateWorld(&w, &p);
    }
    assert(w.platforms[0].fuse > 0.0f && "landing did not arm the crumble");
    for (int i = 0; i < (int)CRUMBLE_FRAMES + 2; i++) {
      updateWorld(&w, &p);
    }
    assert(!w.platforms[0].active && "the fuse ran out and the pad survived");
  }

  // Sunny Hills: the same landing on the same geometry changes nothing.
  {
    World w = bareWorld(ERA_HILLS);
    Player p = createPlayer();
    p.position = (Vector2){400.0f, w.bestY};
    p.velocity = (Vector2){0.0f, 10.0f};
    w.platforms[0] =
        (Platform){{350.0f, w.bestY + 40.0f}, true, false, 0.0f, 0.0f, 0.0f, 0};
    for (int i = 0; i < 20; i++) {
      updatePlayer(&p);
      updateWorld(&w, &p);
    }
    assert(w.platforms[0].active && w.platforms[0].fuse <= 0.0f &&
           "era 0 platform is not the baseline any more");
  }

  // Birdsong Skies: pads drift and reflect cleanly at the screen edge.
  {
    World w = bareWorld(ERA_SKIES);
    Player p = createPlayer();
    p.position = (Vector2){400.0f, w.bestY};
    w.platforms[0] = (Platform){
        {100.0f, w.bestY - 500.0f}, true, false, 0.0f, CLOUD_DRIFT, 0.0f, 0};
    updateWorld(&w, &p);
    assert(w.platforms[0].position.x > 100.0f && "sky platform did not drift");
    w.platforms[0].position.x = SCREEN_WIDTH - PLATFORM_WIDTH;
    w.platforms[0].vx = CLOUD_DRIFT;
    updateWorld(&w, &p);
    assert(w.platforms[0].position.x == SCREEN_WIDTH - PLATFORM_WIDTH &&
           w.platforms[0].vx < 0.0f && "sky platform missed the edge");
  }

  // Neon Maze: a ghost closes in, runs while a pellet is lit, and is worth a
  // doubling bonus when eaten.
  {
    World w = bareWorld(ERA_MAZE);
    Player p = createPlayer();
    p.position = (Vector2){700.0f, w.bestY};
    w.monsters[0] = (Monster){{100.0f, w.bestY}, 0.0f, true, 0.0f, 1};
    float gap0 = p.position.x - w.monsters[0].position.x;
    for (int i = 0; i < 10; i++) {
      updateWorld(&w, &p);
    }
    assert(p.position.x - w.monsters[0].position.x < gap0 &&
           "ghost not hunting");

    w.powerTimer[POWERUP_PELLET] = 300.0f;
    float gap1 = p.position.x - w.monsters[0].position.x;
    for (int i = 0; i < 30; i++) {
      updateWorld(&w, &p);
    }
    assert(p.position.x - w.monsters[0].position.x > gap1 &&
           "ghost not fleeing");

    w.monsters[1] =
        (Monster){{p.position.x, p.position.y}, 0.0f, true, 0.0f, 2};
    updateWorld(&w, &p);
    assert(!w.monsters[1].active && w.ghostChain == 1 &&
           w.comboBonus == GHOST_EAT_BONUS && "ghost was not eaten");
  }

  // Falling Blocks: the pads descend, and one caught from below hurts.
  {
    World w = bareWorld(ERA_BLOCKS);
    Player p = createPlayer();
    p.position = (Vector2){400.0f, w.bestY};
    w.coinsCollected = 5;
    w.platforms[0] = (Platform){
        {350.0f, w.bestY - 10.0f}, true, false, 0.0f, 0.0f, BLOCK_FALL, 0};
    w.platforms[1] = (Platform){
        {50.0f, w.bestY - 1000.0f}, true, false, 0.0f, 0.0f, BLOCK_FALL, 0};
    float y0 = w.platforms[1].position.y;
    updateWorld(&w, &p);
    assert(w.platforms[1].position.y > y0 && "the blocks are not falling");
    assert(w.coinsCollected < 5 && !w.platforms[0].active &&
           "a falling block did not crush");
  }
  printf("ok — era mechanics\n");
}

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "worldtest");
  checkEras();
  while (!texPreloadStep()) {
  }
  while (!svgPreloadStep()) {
  }
  RenderTexture rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
  Image sheet = GenImageColor(CELL * THEME_COUNT, CELL, BLACK);

  // The scene is built by assignment rather than by running the spawner, so
  // the strip shows the same picture every run and a change to the spawn
  // tables can't quietly empty it. Draw coordinates stay in screen space; the
  // camera field only selects the era sample because this harness does not
  // apply the gameplay camera transform.
  World w = {0};
  for (int i = 0; i < 8; i++) {
    w.coins[i].position =
        (Vector2){110.0f + i * 80.0f, 210.0f + (i % 3) * 55.0f};
    w.coins[i].active = true;
  }
  w.monsters[0] = (Monster){{240.0f, 120.0f}, 1.0f, true, 0.0f, 3};
  w.monsters[1] = (Monster){{640.0f, 560.0f}, -1.0f, true, 0.0f, 7};
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
  w.powerTimer[POWERUP_MAGNET] = 100.0f; // layered aura colors around the blob
  w.powerTimer[POWERUP_ROCKET] = 100.0f;

  for (int i = 0; i < THEME_COUNT; i++) {
    float cameraY = -(i * BAND + BAND * 0.05f);
    w.cameraY = cameraY;
    w.bestY = SCREEN_HEIGHT / 2.0f + cameraY;
    // Each cell shows its era's *mechanic*, not just its palette: wind trails
    // on Birdsong Skies, a cracked pad on Night Blocks, frightened ghosts and a
    // pellet on Neon Maze, and all seven pieces on Falling Blocks.
    w.level = i;
    for (int k = 0; k < MAX_PLATFORMS; k++) {
      w.platforms[k].active = false;
    }
    if (i == ERA_BLOCKS) {
      for (int k = 0; k < TETRO_COUNT; k++) {
        w.platforms[k] =
            (Platform){{20.0f + k * 110.0f, 300.0f + (k % 3) * 90.0f},
                       true,
                       false,
                       0.0f,
                       0.0f,
                       BLOCK_FALL,
                       k};
      }
    } else {
      float vx = i == ERA_SKIES ? CLOUD_DRIFT : 0.0f;
      w.platforms[0] = (Platform){{80.0f, 470.0f},
                                  true,
                                  i == ERA_NIGHT,
                                  i == ERA_NIGHT ? CRUMBLE_FRAMES * 0.6f : 0.0f,
                                  vx,
                                  0.0f,
                                  0};
      w.platforms[1] = (Platform){
          {440.0f, 355.0f}, true, i == ERA_NIGHT, 0.0f, -vx, 0.0f, 0};
    }
    w.powerTimer[POWERUP_PELLET] = (i == ERA_MAZE) ? 300.0f : 0.0f;
    w.powerups[MAX_POWERUPS - 1].type =
        (i == ERA_MAZE) ? POWERUP_PELLET : (PowerUpType)(MAX_POWERUPS - 1);
    // Ahead of the draw, and not once at setup: gTheme is zero-initialized
    // until the first themeUpdate, so confetti picked before this point is
    // pure transparent black and the whole burst renders invisible.
    themeUpdate(themeSampleAt(-cameraY));
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
    drawPowerAura(&w, &p);
    drawPlayer(&p);
    drawGameForeground(cameraY);
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
