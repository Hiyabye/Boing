#ifndef PROGRESS_H
#define PROGRESS_H

#include "player.h" // Character (composed cosmetic outfit)
#include "world.h"  // World (bankRun reads run stats)

#include <stdbool.h>

// Per-powerup upgrade tracks — each good power-up gets Spawn/Effect/Duration
// axes, plus one shared "Fewer Debuffs". 5 buyable levels each; the effect
// magnitudes live in world.c (tuning knobs), only the enum + shop cost/label
// live here. Shield has no Duration axis (its duration is a flag, until-hit).
// Adding a track is one enum value + one PU_UPGRADES row. (Declared before
// Progress so puLevel[] can be sized by PU_UPGRADE_COUNT — no magic literal.)
#define MAX_UPG_LEVEL 5
typedef enum {
  PU_MAGNET_SPAWN,
  PU_MAGNET_EFFECT,
  PU_MAGNET_DURATION,
  PU_ROCKET_SPAWN,
  PU_ROCKET_EFFECT,
  PU_ROCKET_DURATION,
  PU_BALLOON_SPAWN,
  PU_BALLOON_EFFECT,
  PU_BALLOON_DURATION,
  PU_SHIELD_SPAWN,
  PU_SHIELD_EFFECT,   // Shield: no duration axis (until-hit flag)
  PU_DEBUFF_SUPPRESS, // shared "Fewer Debuffs"
  PU_UPGRADE_COUNT
} PuTrack;
typedef struct {
  const char *label;
  int base; // level cost = base * (level + 1), so prices rise per level
} PuUpgradeDef;
// Unsized on purpose: a sized declaration would make progress.c's
// completeness assert measure this line instead of the positional table.
extern const PuUpgradeDef PU_UPGRADES[];
// Cost to buy the next level from `level` (level is the count already owned).
int upgCost(int track, int level);

// The outfit shop has four independent eight-item catalogs. Their ownership
// masks and selections pack into two positive 32-bit save values. Wearable
// item indices intentionally match the HatId/GlassesId/MouthwearId enums.
#define COSMETIC_MAX_ITEMS 8
typedef enum {
  COLOR_BUBBLEGUM,
  COLOR_MINT,
  COLOR_BLUEBERRY,
  COLOR_TANGERINE,
  COLOR_LEMON,
  COLOR_GRAPE,
  COLOR_SNOW,
  COLOR_MIDNIGHT,
  COLOR_COUNT,
} ColorId;

typedef enum {
  COSMETIC_COLOR,
  COSMETIC_HEAD,
  COSMETIC_EYES,
  COSMETIC_MOUTH,
  COSMETIC_CATEGORY_COUNT,
} CosmeticCategory;

typedef struct {
  const char *name;
  int cost;
  Color body;     // used by COSMETIC_COLOR only
  Color bodyDark; // used by COSMETIC_COLOR only
} CosmeticDef;

typedef struct {
  const char *name;
  const CosmeticDef *items;
  int count;
} CosmeticCatalog;

extern const CosmeticCatalog COSMETICS[];

// Persistent meta-progression: banked coins, independent cosmetic ownership
// and selections, per-powerup upgrade levels, and completed missions. Cosmetic
// masks and selections each pack into one save key, keeping the flat store at
// its existing key budget.
typedef struct {
  int coinBank; // spendable currency (run coins banked at death + rewards)
  int cosmeticMask[COSMETIC_CATEGORY_COUNT];
  unsigned char selectedCosmetic[COSMETIC_CATEGORY_COUNT];
  int missionMask; // bit i set = mission i completed (reward already paid)
  int puLevel[PU_UPGRADE_COUNT]; // per-track upgrade levels, 0..MAX_UPG_LEVEL
} Progress;

// Missions — data-driven threshold checks scored once at game over against the
// run's stats. No per-mission predicate code, no live per-frame tracking.
typedef enum {
  MSTAT_COINS_RUN, // coins collected this run
  MSTAT_COMBO,     // best combo this run
  MSTAT_LEVEL,     // theme band reached (0-based)
  MSTAT_SCORE,     // final run score
} MissionStat;
typedef struct {
  const char *desc;
  MissionStat stat;
  int threshold;
  int reward; // coins added to the bank on completion
} MissionDef;
extern const MissionDef MISSIONS[];
extern const int MISSION_COUNT;

Progress loadProgress(void);
void saveProgress(const Progress *p);

bool cosmeticOwned(const Progress *p, int category, int item);
// Resolve the four selected component ids into the Character used by player.c.
Character progressCharacter(const Progress *p);
int upgLevel(const Progress *p,
             int track); // levels owned on a PuTrack (0..MAX)
bool missionDone(const Progress *p, int i);
// Spend `cost` from the bank if affordable; returns whether it went through.
// Caller sets the owning bit + saves on true.
bool tryBuy(Progress *p, int cost);
// Bank the run's coins and score any newly-completed missions (one-time).
// Returns the number of missions newly completed (for an optional toast).
int bankRun(Progress *p, const World *w);

#endif // PROGRESS_H
