#include "progress.h"

#include "core.h" // STATIC_ASSERT
#include "persist.h"

// Skin palettes: {body, bodyDark, hat, glasses}. Literal Colors (not the gTheme
// macros, which are runtime lvalues and can't initialize a static table).
// Bubblegum matches the original player pink so the default look is unchanged.
//
// The accessory slots are placeholders spread across the existing four skins to
// show the mechanism works — they cost nothing extra and are not separately
// purchasable, so the shop is unchanged.
const SkinDef SKINS[] = {
    {{{255, 127, 176, 255}, {228, 70, 139, 255}, HAT_BOW, GLS_NONE},
     0,
     "BUBBLEGUM"},
    {{{120, 224, 168, 255}, {56, 168, 110, 255}, HAT_CAP, GLS_NONE},
     60,
     "MINT"},
    {{{143, 155, 255, 255}, {74, 87, 208, 255}, HAT_NONE, GLS_ROUND},
     150,
     "BLUEBERRY"},
    {{{255, 178, 74, 255}, {224, 122, 30, 255}, HAT_CROWN, GLS_SHADES},
     300,
     "TANGERINE"},
};
const int SKIN_COUNT = (int)(sizeof(SKINS) / sizeof(SKINS[0]));
// skinMask is an int bitmask indexed `1 << i`, so row 31 shifts into the sign
// bit — undefined behaviour, and the skin could never be marked owned. 31 rows
// is the real capacity of the save format, not just of the loop below.
STATIC_ASSERT(sizeof(SKINS) / sizeof(SKINS[0]) <= 31, skins_fit_mask);

// Unsized on purpose, here and in the progress.h declaration — see POWERUP_DEFS
// in world.c for why both matter and for the designated-initializer limit.
const PuUpgradeDef PU_UPGRADES[] = {
    // label, base cost (level cost = base * (level+1)). Spawn cheapest, Effect
    // dearest; the shared debuff-suppress is the premium sink.
    // Short axis-only labels: the SHOP card header supplies the power-up name.
    [PU_MAGNET_SPAWN] = {"Spawn", 40},    [PU_MAGNET_EFFECT] = {"Range", 60},
    [PU_MAGNET_DURATION] = {"Time", 50},  [PU_ROCKET_SPAWN] = {"Spawn", 40},
    [PU_ROCKET_EFFECT] = {"Thrust", 60},  [PU_ROCKET_DURATION] = {"Time", 50},
    [PU_BALLOON_SPAWN] = {"Spawn", 40},   [PU_BALLOON_EFFECT] = {"Lift", 60},
    [PU_BALLOON_DURATION] = {"Time", 50}, [PU_SHIELD_SPAWN] = {"Spawn", 40},
    [PU_SHIELD_EFFECT] = {"Hits", 80},    [PU_DEBUFF_SUPPRESS] = {"Rarer", 100},
};
// Enum-indexed: a new PuTrack without a row here loops past the array end, and
// puLevel[] in Progress must match this size (both keyed by PU_UPGRADE_COUNT).
STATIC_ASSERT(sizeof(PU_UPGRADES) / sizeof(PU_UPGRADES[0]) == PU_UPGRADE_COUNT,
              pu_upgrades_full);

// This module writes one "pu%d" key per upgrade track (saveProgress below), so
// PuTrack is the only enum in the game whose growth eats save-file slots — and
// running out of them is silent, not an error: configSetInt drops the key and
// the level stops persisting. The other 9 keys are fixed: highScore, the four
// settings, and this module's four scalars. Fail the build instead.
#define CONFIG_FIXED_KEYS 9
STATIC_ASSERT(CONFIG_FIXED_KEYS + PU_UPGRADE_COUNT <= CONFIG_MAX_KEYS,
              save_keys_fit);

int upgCost(int track, int level) {
  return PU_UPGRADES[track].base * (level + 1);
}

const MissionDef MISSIONS[] = {
    {"Reach the Neon Maze", MSTAT_LEVEL, 2, 100},
    {"Collect 50 coins in one run", MSTAT_COINS_RUN, 50, 100},
    {"Chain a x10 combo", MSTAT_COMBO, 10, 100},
    {"Score 5000 in one run", MSTAT_SCORE, 5000, 150},
    {"Reach the Falling Blocks", MSTAT_LEVEL, 3, 200},
};
const int MISSION_COUNT = (int)(sizeof(MISSIONS) / sizeof(MISSIONS[0]));
// Same bitmask ceiling as SKINS above: missionMask pays each reward once via
// `1 << i`, so the 32nd mission would shift into the sign bit and never latch.
STATIC_ASSERT(sizeof(MISSIONS) / sizeof(MISSIONS[0]) <= 31, missions_fit_mask);

bool skinOwned(const Progress *p, int i) {
  return (p->skinMask & (1 << i)) != 0;
}
int upgLevel(const Progress *p, int track) { return p->puLevel[track]; }
bool missionDone(const Progress *p, int i) {
  return (p->missionMask & (1 << i)) != 0;
}

bool tryBuy(Progress *p, int cost) {
  if (p->coinBank < cost) {
    return false;
  }
  p->coinBank -= cost;
  return true;
}

int bankRun(Progress *p, const World *w) {
  p->coinBank += w->coinsCollected;
  int newly = 0;
  for (int i = 0; i < MISSION_COUNT; i++) {
    if (missionDone(p, i)) {
      continue; // already completed and paid
    }
    int v = 0;
    switch (MISSIONS[i].stat) {
    case MSTAT_COINS_RUN:
      v = w->coinsCollected;
      break;
    case MSTAT_COMBO:
      v = w->bestCombo;
      break;
    case MSTAT_LEVEL:
      v = w->level;
      break;
    case MSTAT_SCORE:
      v = getScore(w);
      break;
    }
    if (v >= MISSIONS[i].threshold) {
      p->missionMask |= (1 << i);
      p->coinBank += MISSIONS[i].reward;
      newly++;
    }
  }
  return newly;
}

Progress loadProgress(void) {
  Progress p = {0, 1, 0, 0, {0}}; // fresh: only the free default skin owned
  p.coinBank = configGetInt("coinBank", p.coinBank);
  p.skinMask = configGetInt("skinMask", p.skinMask);
  p.selectedSkin = configGetInt("selectedSkin", p.selectedSkin);
  p.missionMask = configGetInt("missionMask", p.missionMask);
  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    int lvl = configGetInt(TextFormat("pu%d", i), 0);
    p.puLevel[i] = lvl < 0 ? 0 : (lvl > MAX_UPG_LEVEL ? MAX_UPG_LEVEL : lvl);
  }
  // Guard a missing/garbage-partial file: default skin always owned, bank
  // non-negative, selection must point at an owned skin.
  p.skinMask |= 1;
  if (p.coinBank < 0) {
    p.coinBank = 0;
  }
  if (p.selectedSkin < 0 || p.selectedSkin >= SKIN_COUNT ||
      !skinOwned(&p, p.selectedSkin)) {
    p.selectedSkin = 0;
  }
  return p;
}

void saveProgress(const Progress *p) {
  configSetInt("coinBank", p->coinBank);
  configSetInt("skinMask", p->skinMask);
  configSetInt("selectedSkin", p->selectedSkin);
  configSetInt("missionMask", p->missionMask);
  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    configSetInt(TextFormat("pu%d", i), p->puLevel[i]);
  }
  configSave("save.json");
}
