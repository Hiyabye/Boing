#ifndef PROGRESS_H
#define PROGRESS_H

#include "player.h" // Character (skin palette)
#include "world.h"  // World (bankRun reads run stats)

#include <stdbool.h>

// Persistent meta-progression: a coin bank that survives death, plus the
// unlock state for skins, permanent upgrades, and completed missions. One
// plain-text "progress" file beside the exe (5 ints), same convention as
// "highscore"/"settings" — runtime-generated, off the 1.44MB budget.
typedef struct {
  int coinBank;     // spendable currency (run coins banked at death + rewards)
  int skinMask;     // bit i set = skin i owned (bit 0 always the free default)
  int selectedSkin; // index into SKINS, must be owned
  int upgradeMask;  // bit UPG_* set = that upgrade purchased
  int missionMask;  // bit i set = mission i completed (reward already paid)
} Progress;

// Unlockable blob skins — original candy palettes, row 0 is the free default.
// A new skin is one more row (and one more bit of budget in skinMask).
typedef struct {
  Character character;
  int cost;
  const char *name;
} SkinDef;
extern const SkinDef SKINS[];
extern const int SKIN_COUNT;

// Permanent upgrades — one-time unlocks (bit index into upgradeMask), no tiers.
// Effects live at their hook sites in world.c, gated on worldSetUpgrades().
typedef enum {
  UPG_HEADSTART,  // begin each run with a Shield active
  UPG_LONGBOOST,  // good power-ups last 50% longer
  UPG_COMBOGRACE, // combo chain holds 50% longer between coins
  UPGRADE_COUNT
} UpgradeId;
typedef struct {
  const char *name, *desc;
  int cost;
} UpgradeDef;
extern const UpgradeDef UPGRADES[];

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
  int stat; // MissionStat
  int threshold;
  int reward; // coins added to the bank on completion
} MissionDef;
extern const MissionDef MISSIONS[];
extern const int MISSION_COUNT;

Progress loadProgress(void);
void saveProgress(const Progress *p);

bool skinOwned(const Progress *p, int i);
bool upgradeOwned(const Progress *p, int id);
// Spend `cost` from the bank if affordable; returns whether it went through.
// Caller sets the owning bit + saves on true.
bool tryBuy(Progress *p, int cost);
// Bank the run's coins and score any newly-completed missions (one-time).
// Returns the number of missions newly completed (for an optional toast).
int bankRun(Progress *p, const World *w);

#endif // PROGRESS_H
