#include "progress.h"

#include "core.h" // STATIC_ASSERT
#include "persist.h"

#include <limits.h>

// Independent cosmetic catalogs. Color rows carry literal palettes (theme
// macros are runtime lvalues); wearable rows only need names and prices because
// their positional index is the corresponding player.h enum value.
static const CosmeticDef COLOR_ITEMS[] = {
    {"BUBBLEGUM", 0, {255, 127, 176, 255}, {228, 70, 139, 255}},
    {"MINT", 60, {120, 224, 168, 255}, {56, 168, 110, 255}},
    {"BLUEBERRY", 120, {143, 155, 255, 255}, {74, 87, 208, 255}},
    {"TANGERINE", 180, {255, 178, 74, 255}, {224, 122, 30, 255}},
    {"LEMON", 220, {255, 226, 92, 255}, {218, 166, 37, 255}},
    {"GRAPE", 260, {195, 126, 238, 255}, {126, 69, 184, 255}},
    {"SNOW", 320, {242, 246, 255, 255}, {165, 182, 212, 255}},
    {"MIDNIGHT", 360, {74, 77, 112, 255}, {35, 37, 67, 255}},
};
static const CosmeticDef HAT_ITEMS[] = {
    {.name = "NONE", .cost = 0},     {.name = "CAP", .cost = 50},
    {.name = "CROWN", .cost = 220},  {.name = "BOW", .cost = 80},
    {.name = "PARTY", .cost = 100},  {.name = "WIZARD", .cost = 180},
    {.name = "FLOWER", .cost = 140}, {.name = "HALO", .cost = 240},
};
static const CosmeticDef EYE_ITEMS[] = {
    {.name = "NONE", .cost = 0},      {.name = "ROUND", .cost = 70},
    {.name = "SHADES", .cost = 120},  {.name = "HEARTS", .cost = 150},
    {.name = "STARS", .cost = 170},   {.name = "MONOCLE", .cost = 110},
    {.name = "GOGGLES", .cost = 200}, {.name = "VISOR", .cost = 230},
};
static const CosmeticDef MOUTH_ITEMS[] = {
    {.name = "NONE", .cost = 0},       {.name = "MUSTACHE", .cost = 70},
    {.name = "PACIFIER", .cost = 90},  {.name = "MASK", .cost = 130},
    {.name = "LOLLIPOP", .cost = 100}, {.name = "PARTY HORN", .cost = 160},
    {.name = "FANGS", .cost = 180},    {.name = "BUBBLE", .cost = 140},
};
STATIC_ASSERT(sizeof(COLOR_ITEMS) / sizeof(COLOR_ITEMS[0]) == COLOR_COUNT,
              color_items_full);
STATIC_ASSERT(sizeof(HAT_ITEMS) / sizeof(HAT_ITEMS[0]) == HAT_COUNT,
              hat_items_full);
STATIC_ASSERT(sizeof(EYE_ITEMS) / sizeof(EYE_ITEMS[0]) == GLS_COUNT,
              eye_items_full);
STATIC_ASSERT(sizeof(MOUTH_ITEMS) / sizeof(MOUTH_ITEMS[0]) == MOUTHWEAR_COUNT,
              mouth_items_full);
STATIC_ASSERT(COLOR_COUNT <= COSMETIC_MAX_ITEMS, colors_fit_save);
STATIC_ASSERT(HAT_COUNT <= COSMETIC_MAX_ITEMS, hats_fit_save);
STATIC_ASSERT(GLS_COUNT <= COSMETIC_MAX_ITEMS, eyewear_fits_save);
STATIC_ASSERT(MOUTHWEAR_COUNT <= COSMETIC_MAX_ITEMS, mouthwear_fits_save);

const CosmeticCatalog COSMETICS[] = {
    {"COLOR", COLOR_ITEMS, COLOR_COUNT},
    {"HEADWEAR", HAT_ITEMS, HAT_COUNT},
    {"EYEWEAR", EYE_ITEMS, GLS_COUNT},
    {"MOUTH-WEAR", MOUTH_ITEMS, MOUTHWEAR_COUNT},
};
STATIC_ASSERT(sizeof(COSMETICS) / sizeof(COSMETICS[0]) ==
                  COSMETIC_CATEGORY_COUNT,
              cosmetic_catalogs_full);

// Unsized and positional on purpose, here and in the progress.h declaration:
// dropping any PuTrack row must shorten the table and fail the count check.
const PuUpgradeDef PU_UPGRADES[] = {
    // label, base cost (level cost = base * (level+1)). Spawn cheapest, Effect
    // dearest; the shared debuff-suppress is the premium sink.
    // Short axis-only labels: the SHOP card header supplies the power-up name.
    {"Spawn", 40},  {"Range", 60},  {"Time", 50}, // Magnet
    {"Spawn", 40},  {"Thrust", 60}, {"Time", 50}, // Rocket
    {"Spawn", 40},  {"Lift", 60},   {"Time", 50}, // Balloon
    {"Spawn", 40},  {"Hits", 80},                 // Shield
    {"Rarer", 100},                               // Debuffs
};
// Enum-indexed: a new PuTrack without a row here loops past the array end, and
// puLevel[] in Progress must match this size (both keyed by PU_UPGRADE_COUNT).
STATIC_ASSERT(sizeof(PU_UPGRADES) / sizeof(PU_UPGRADES[0]) == PU_UPGRADE_COUNT,
              pu_upgrades_full);

// This module writes one "pu%d" key per upgrade track. New saves still use
// nine fixed keys across the game; an old bundle-skin save can retain two stale
// keys alongside the new cosmetic pair, so guard that migration peak too.
#define CONFIG_FIXED_KEYS 9
#define CONFIG_LEGACY_KEYS 2
STATIC_ASSERT(CONFIG_FIXED_KEYS + CONFIG_LEGACY_KEYS + PU_UPGRADE_COUNT <=
                  CONFIG_MAX_KEYS,
              save_keys_fit);

int upgCost(int track, int level) {
  return PU_UPGRADES[track].base * (level + 1);
}

const MissionDef MISSIONS[] = {
    {"Reach the Neon Maze", MSTAT_LEVEL, 3, 100},
    {"Collect 50 coins in one run", MSTAT_COINS_RUN, 50, 100},
    {"Chain a x10 combo", MSTAT_COMBO, 10, 100},
    {"Score 5000 in one run", MSTAT_SCORE, 5000, 150},
    {"Reach the Falling Blocks", MSTAT_LEVEL, 4, 200},
};
const int MISSION_COUNT = (int)(sizeof(MISSIONS) / sizeof(MISSIONS[0]));
// missionMask pays each reward once via `1 << i`, so the 32nd mission would
// shift into the sign bit and never latch.
STATIC_ASSERT(sizeof(MISSIONS) / sizeof(MISSIONS[0]) <= 31, missions_fit_mask);

bool cosmeticOwned(const Progress *p, int category, int item) {
  return (p->cosmeticMask[category] & (1 << item)) != 0;
}

Character progressCharacter(const Progress *p) {
  const CosmeticDef *color = &COLOR_ITEMS[p->selectedCosmetic[COSMETIC_COLOR]];
  return (Character){
      color->body, color->bodyDark, p->selectedCosmetic[COSMETIC_HEAD],
      p->selectedCosmetic[COSMETIC_EYES], p->selectedCosmetic[COSMETIC_MOUTH]};
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

// Bank additions are non-negative. Saturate rather than overflow if a save was
// edited to INT_MAX or an exceptionally long-running profile reaches it.
static void addCoins(Progress *p, int amount) {
  p->coinBank = amount > INT_MAX - p->coinBank ? INT_MAX : p->coinBank + amount;
}

int bankRun(Progress *p, const World *w) {
  addCoins(p, w->coinsCollected);
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
      addCoins(p, MISSIONS[i].reward);
      newly++;
    }
  }
  return newly;
}

// Seven ownership bits per category live in cosOwned's four bytes. Each eighth
// bit shares cosEquip with the four three-bit selections; the high marker still
// rejects pre-component saves and starts their cosmetics from free defaults.
#define COSMETIC_SAVE_MAGIC 0x434F0000
#define COSMETIC_SAVE_MAGIC_MASK 0xFFFF0000
#define COSMETIC_SELECT_BITS 3
#define COSMETIC_EXTRA_MASK_SHIFT 12
// The serialized bit layout has four 3-bit selections followed by four eighth
// ownership bits. Any category or catalog growth needs a deliberate migration,
// not a larger enum that silently overlaps the format marker.
STATIC_ASSERT(COSMETIC_CATEGORY_COUNT == 4, cosmetic_category_layout_stable);
STATIC_ASSERT(COSMETIC_MAX_ITEMS == (1 << COSMETIC_SELECT_BITS),
              cosmetic_item_layout_stable);

static int packCosmeticMasks(const Progress *p) {
  int packed = 0;
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    packed |= (p->cosmeticMask[i] & 0x7F) << (i * 8);
  }
  return packed;
}

static int packCosmeticSelections(const Progress *p) {
  int packed = COSMETIC_SAVE_MAGIC;
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    packed |= p->selectedCosmetic[i] << (i * COSMETIC_SELECT_BITS);
    packed |= ((p->cosmeticMask[i] >> 7) & 1)
              << (COSMETIC_EXTRA_MASK_SHIFT + i);
  }
  return packed;
}

Progress loadProgress(void) {
  Progress p = {.cosmeticMask = {1, 1, 1, 1}};
  p.coinBank = configGetInt("coinBank", p.coinBank);
  p.missionMask = configGetInt("missionMask", p.missionMask);

  int selections = configGetInt("cosEquip", -1);
  if ((selections & COSMETIC_SAVE_MAGIC_MASK) == COSMETIC_SAVE_MAGIC) {
    int masks = configGetInt("cosOwned", 0);
    for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
      int valid = (1 << COSMETICS[i].count) - 1;
      int extra = ((selections >> (COSMETIC_EXTRA_MASK_SHIFT + i)) & 1) << 7;
      p.cosmeticMask[i] = (((masks >> (i * 8)) & 0x7F) | extra) & valid;
      p.cosmeticMask[i] |= 1;
      int selected = (selections >> (i * COSMETIC_SELECT_BITS)) & 0x07;
      if (selected < COSMETICS[i].count && cosmeticOwned(&p, i, selected)) {
        p.selectedCosmetic[i] = (unsigned char)selected;
      }
    }
  }

  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    int lvl = configGetInt(TextFormat("pu%d", i), 0);
    p.puLevel[i] = lvl < 0 ? 0 : (lvl > MAX_UPG_LEVEL ? MAX_UPG_LEVEL : lvl);
  }
  if (p.coinBank < 0) {
    p.coinBank = 0;
  }
  return p;
}

void saveProgress(const Progress *p) {
  configSetInt("coinBank", p->coinBank);
  configSetInt("cosOwned", packCosmeticMasks(p));
  configSetInt("cosEquip", packCosmeticSelections(p));
  configSetInt("missionMask", p->missionMask);
  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    configSetInt(TextFormat("pu%d", i), p->puLevel[i]);
  }
  configSave("save.json");
}

#ifdef PROGRESS_SELFTEST
// Headless check for fresh migration, packed cosmetic round-trip, spending, and
// mission rewards. Build:
//   cc -DPROGRESS_SELFTEST src/progress.c src/persist.c -Iinclude \
//      -Ilib/raylib/src -o /tmp/progresstest && /tmp/progresstest
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

// Minimal stand-ins for the two external helpers this translation unit uses.
const char *TextFormat(const char *text, ...) {
  static char out[64];
  va_list args;
  va_start(args, text);
  vsnprintf(out, sizeof(out), text, args);
  va_end(args);
  return out;
}
int getScore(const World *w) { return w->comboBonus; }

int main(void) {
  configLoad("/no/such/save.json");
  configSetInt("skinMask", 15);    // legacy bundle keys are ignored
  configSetInt("selectedSkin", 3); // instead of partially migrating an outfit
  Progress fresh = loadProgress();
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    assert(fresh.cosmeticMask[i] == 1);
    assert(fresh.selectedCosmetic[i] == 0);
  }

  fresh.cosmeticMask[COSMETIC_COLOR] |= 1 << COLOR_MIDNIGHT;
  fresh.cosmeticMask[COSMETIC_HEAD] |= 1 << HAT_HALO;
  fresh.cosmeticMask[COSMETIC_EYES] |= 1 << GLS_VISOR;
  fresh.cosmeticMask[COSMETIC_MOUTH] |= 1 << MOUTHWEAR_BUBBLE;
  fresh.selectedCosmetic[COSMETIC_COLOR] = COLOR_MIDNIGHT;
  fresh.selectedCosmetic[COSMETIC_HEAD] = HAT_HALO;
  fresh.selectedCosmetic[COSMETIC_EYES] = GLS_VISOR;
  fresh.selectedCosmetic[COSMETIC_MOUTH] = MOUTHWEAR_BUBBLE;
  configSetInt("cosOwned", packCosmeticMasks(&fresh));
  configSetInt("cosEquip", packCosmeticSelections(&fresh));

  Progress loaded = loadProgress();
  assert(loaded.selectedCosmetic[COSMETIC_COLOR] == COLOR_MIDNIGHT);
  assert(loaded.selectedCosmetic[COSMETIC_HEAD] == HAT_HALO);
  assert(loaded.selectedCosmetic[COSMETIC_EYES] == GLS_VISOR);
  assert(loaded.selectedCosmetic[COSMETIC_MOUTH] == MOUTHWEAR_BUBBLE);
  Character c = progressCharacter(&loaded);
  assert(c.hat == HAT_HALO && c.glasses == GLS_VISOR &&
         c.mouthwear == MOUTHWEAR_BUBBLE);

  configSetInt("cosOwned", 0);
  configSetInt("cosEquip", COSMETIC_SAVE_MAGIC | (HAT_HALO << 3));
  Progress invalid = loadProgress();
  assert(invalid.cosmeticMask[COSMETIC_HEAD] == 1);
  assert(invalid.selectedCosmetic[COSMETIC_HEAD] == HAT_NONE);

  loaded.coinBank = 100;
  assert(tryBuy(&loaded, 70) && loaded.coinBank == 30);
  assert(!tryBuy(&loaded, 40) && loaded.coinBank == 30);
  World w = {.coinsCollected = 50, .bestCombo = 10, .level = 3};
  assert(bankRun(&loaded, &w) == 3);
  assert(loaded.coinBank == 380); // 30 + 50 run coins + three 100 rewards
  puts("ok - cosmetic migration, packing, spending, and rewards");
  return 0;
}
#endif
