#include "progress.h"

#include "core.h" // STATIC_ASSERT
#include "persist.h"

// Skin palettes: {body, bodyDark}. Literal Colors (not the gTheme macros, which
// are runtime lvalues and can't initialize a static table). Bubblegum matches
// the original player pink so the default look is unchanged.
const SkinDef SKINS[] = {
    {{{255, 127, 176, 255}, {228, 70, 139, 255}}, 0, "BUBBLEGUM"},
    {{{120, 224, 168, 255}, {56, 168, 110, 255}}, 60, "MINT"},
    {{{143, 155, 255, 255}, {74, 87, 208, 255}}, 150, "BLUEBERRY"},
    {{{255, 178, 74, 255}, {224, 122, 30, 255}}, 300, "TANGERINE"},
};
const int SKIN_COUNT = (int)(sizeof(SKINS) / sizeof(SKINS[0]));

const UpgradeDef UPGRADES[] = {
    [UPG_HEADSTART] = {"HEAD START", "Begin each run shielded", 200},
    [UPG_LONGBOOST] = {"LONG BOOST", "Power-ups last 50% longer", 250},
    [UPG_COMBOGRACE] = {"COMBO GRACE", "Combo holds 50% longer", 150},
};
// Enum-indexed: a new UpgradeId without a row here loops past the array end.
STATIC_ASSERT(sizeof(UPGRADES) / sizeof(UPGRADES[0]) == UPGRADE_COUNT,
              upgrades_full);

const MissionDef MISSIONS[] = {
    {"Reach the Neon Maze", MSTAT_LEVEL, 2, 100},
    {"Collect 50 coins in one run", MSTAT_COINS_RUN, 50, 100},
    {"Chain a x10 combo", MSTAT_COMBO, 10, 100},
    {"Score 5000 in one run", MSTAT_SCORE, 5000, 150},
    {"Reach the Falling Blocks", MSTAT_LEVEL, 3, 200},
};
const int MISSION_COUNT = (int)(sizeof(MISSIONS) / sizeof(MISSIONS[0]));

bool skinOwned(const Progress *p, int i) {
  return (p->skinMask & (1 << i)) != 0;
}
bool upgradeOwned(const Progress *p, int id) {
  return (p->upgradeMask & (1 << id)) != 0;
}
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
  Progress p = {0, 1, 0, 0, 0}; // fresh: only the free default skin owned
  p.coinBank = configGetInt("coinBank", p.coinBank);
  p.skinMask = configGetInt("skinMask", p.skinMask);
  p.selectedSkin = configGetInt("selectedSkin", p.selectedSkin);
  p.upgradeMask = configGetInt("upgradeMask", p.upgradeMask);
  p.missionMask = configGetInt("missionMask", p.missionMask);
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
  configSetInt("upgradeMask", p->upgradeMask);
  configSetInt("missionMask", p->missionMask);
  configSave("save.json");
}
