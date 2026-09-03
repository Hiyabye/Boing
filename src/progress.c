#include "progress.h"

#include "core.h" // STATIC_ASSERT
#include "persist.h"

#include <limits.h>
#include <stdint.h>
#include <time.h>

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
    {"Spawn", 100}, {"Range", 150},  {"Time", 100}, // Magnet
    {"Spawn", 100}, {"Thrust", 150}, {"Time", 100}, // Rocket
    {"Spawn", 100}, {"Lift", 150},   {"Time", 100}, // Balloon
    {"Spawn", 100}, {"Hits", 200},                  // Shield
    {"Rarer", 200},                                 // Debuffs
};
// Enum-indexed: a new PuTrack without a row here loops past the array end, and
// puLevel[] in Progress must match this size (both keyed by PU_UPGRADE_COUNT).
STATIC_ASSERT(sizeof(PU_UPGRADES) / sizeof(PU_UPGRADES[0]) == PU_UPGRADE_COUNT,
              pu_upgrades_full);

// Ten scalar keys cover high score, settings, and fixed progress fields. Task
// slots and upgrades each add one key, so their macros must drive the capacity
// check instead of a hand-maintained total. Legacy bundle skins add two more.
#define CONFIG_SCALAR_KEYS 10
#define CONFIG_LEGACY_KEYS 2
#define CONFIG_FIXED_KEYS                                                      \
  (CONFIG_SCALAR_KEYS + DAILY_TASK_COUNT + REPEATABLE_TASK_COUNT)
STATIC_ASSERT(CONFIG_FIXED_KEYS + CONFIG_LEGACY_KEYS + PU_UPGRADE_COUNT <=
                  CONFIG_MAX_KEYS,
              save_keys_fit);

int upgCost(int track, int level) {
  return PU_UPGRADES[track].base * (level + 1);
}

static const MissionDef DAILY_TASK_POOL[] = {
    {"Collect 80 coins", MSTAT_COINS_RUN, 80, 50},
    {"Earn 10000 score", MSTAT_SCORE, 10000, 50},
    {"Build 100 combo points", MSTAT_COMBO, 100, 50},
    {"Visit 10 era bands", MSTAT_ERAS, 10, 50},
    {"Finish 5 runs", MSTAT_RUNS, 5, 50},
};
enum {
  DAILY_POOL_COUNT = (int)(sizeof(DAILY_TASK_POOL) / sizeof(DAILY_TASK_POOL[0]))
};
STATIC_ASSERT(DAILY_TASK_COUNT <= DAILY_POOL_COUNT, daily_slots_fit_pool);

const MissionDef REPEATABLE_TASKS[] = {
    {"Collect 1000 coins", MSTAT_COINS_RUN, 1000, 100},
    {"Earn 100000 score", MSTAT_SCORE, 100000, 100},
    {"Finish 30 runs", MSTAT_RUNS, 30, 100},
};
STATIC_ASSERT(sizeof(REPEATABLE_TASKS) / sizeof(REPEATABLE_TASKS[0]) ==
                  REPEATABLE_TASK_COUNT,
              repeatable_tasks_full);

const MissionDef ACHIEVEMENTS[] = {
    {"Reach the third Neon Maze", MSTAT_ERAS, 14, 1000},
    {"Collect 1000 coins in one run", MSTAT_COINS_RUN, 1000, 1000},
    {"Chain a x300 combo", MSTAT_COMBO, 300, 1000},
    {"Score 300000 in one run", MSTAT_SCORE, 300000, 1000},
    {"Reach the third Falling Blocks", MSTAT_ERAS, 15, 1000},
};
const int ACHIEVEMENT_COUNT =
    (int)(sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]));
// achievementMask pays each reward once via `1 << i`, so the 32nd row would
// shift into the sign bit and never latch.
STATIC_ASSERT(sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]) <= 31,
              achievements_fit_mask);

// A tiny local shuffle gives each calendar date three unique tasks without
// storing the lineup or perturbing raylib's gameplay RNG.
static int dailyTaskIndex(int day, int slot) {
  int order[DAILY_POOL_COUNT];
  for (int i = 0; i < DAILY_POOL_COUNT; i++) {
    order[i] = i;
  }
  uint32_t state = (uint32_t)day ^ 0x9e3779b9u;
  for (int i = DAILY_POOL_COUNT - 1; i > 0; i--) {
    state = state * 1664525u + 1013904223u;
    int j = (int)(state % (uint32_t)(i + 1));
    int swap = order[i];
    order[i] = order[j];
    order[j] = swap;
  }
  return order[slot];
}

const MissionDef *dailyTask(const Progress *p, int i) {
  return &DAILY_TASK_POOL[dailyTaskIndex(p->dailyStamp, i)];
}

static int localDayStamp(void) {
  time_t now = time(NULL);
  struct tm *date = localtime(&now);
  return date ? (date->tm_year + 1900) * 10000 + (date->tm_mon + 1) * 100 +
                    date->tm_mday
              : 0;
}

static bool refreshDailyForDay(Progress *p, int day) {
  if (day <= 0 || p->dailyStamp == day) {
    return false;
  }
  p->dailyStamp = day;
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    p->dailyProgress[i] = 0;
  }
  return true;
}

bool progressRefreshDaily(Progress *p) {
  return refreshDailyForDay(p, localDayStamp());
}

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
bool achievementDone(const Progress *p, int i) {
  return (p->achievementMask & (1 << i)) != 0;
}

bool tryBuy(Progress *p, int cost) {
  if (cost < 0 || p->coinBank < cost) {
    return false;
  }
  p->coinBank -= cost;
  return true;
}

// Bank additions are non-negative. Use a wide amount because one huge run can
// clear a repeatable more than once, then saturate the persisted int balance.
static void addCoins(Progress *p, long long amount) {
  p->coinBank = amount >= (long long)INT_MAX - p->coinBank
                    ? INT_MAX
                    : p->coinBank + (int)amount;
}

static int missionValue(const World *w, MissionStat stat) {
  int value = 0;
  switch (stat) {
  case MSTAT_COINS_RUN:
    value = w->coinsCollected;
    break;
  case MSTAT_COMBO:
    value = w->bestCombo;
    break;
  case MSTAT_ERAS:
    value = w->level < INT_MAX ? w->level + 1 : INT_MAX;
    break;
  case MSTAT_SCORE:
    value = getScore(w);
    break;
  case MSTAT_RUNS:
    value = 1;
    break;
  }
  return value > 0 ? value : 0;
}

static int advanceDaily(Progress *p, const World *w) {
  int paid = 0;
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    const MissionDef *task = dailyTask(p, i);
    int left = task->threshold - p->dailyProgress[i];
    if (left <= 0) {
      continue;
    }
    int value = missionValue(w, task->stat);
    if (value >= left) {
      p->dailyProgress[i] = task->threshold;
      addCoins(p, task->reward);
      paid++;
    } else {
      p->dailyProgress[i] += value;
    }
  }
  return paid;
}

static int advanceRepeatables(Progress *p, const World *w) {
  long long paid = 0;
  for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
    const MissionDef *task = &REPEATABLE_TASKS[i];
    long long total =
        (long long)p->repeatableProgress[i] + missionValue(w, task->stat);
    long long completions = total / task->threshold;
    p->repeatableProgress[i] = (int)(total % task->threshold);
    addCoins(p, completions * task->reward);
    paid += completions;
  }
  return paid > INT_MAX ? INT_MAX : (int)paid;
}

static int advanceAchievements(Progress *p, const World *w) {
  int paid = 0;
  for (int i = 0; i < ACHIEVEMENT_COUNT; i++) {
    if (!achievementDone(p, i) &&
        missionValue(w, ACHIEVEMENTS[i].stat) >= ACHIEVEMENTS[i].threshold) {
      p->achievementMask |= (1 << i);
      addCoins(p, ACHIEVEMENTS[i].reward);
      paid++;
    }
  }
  return paid;
}

int bankRun(Progress *p, const World *w) {
  progressRefreshDaily(p);
  addCoins(p, w->coinsCollected);
  long long paid = (long long)advanceDaily(p, w) + advanceRepeatables(p, w) +
                   advanceAchievements(p, w);
  return paid > INT_MAX ? INT_MAX : (int)paid;
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
  // Keep the old key so the five original missions migrate into achievements.
  p.achievementMask = configGetInt("missionMask", p.achievementMask);
  p.dailyStamp = configGetInt("dailyStamp", 0);
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    p.dailyProgress[i] = configGetInt(TextFormat("day%d", i), 0);
  }
  for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
    p.repeatableProgress[i] = configGetInt(TextFormat("rep%d", i), 0);
  }
  progressRefreshDaily(&p);

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
  unsigned achievementBits = (1u << ACHIEVEMENT_COUNT) - 1u;
  p.achievementMask &= (int)achievementBits;
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    int max = dailyTask(&p, i)->threshold;
    if (p.dailyProgress[i] < 0) {
      p.dailyProgress[i] = 0;
    } else if (p.dailyProgress[i] > max) {
      p.dailyProgress[i] = max;
    }
  }
  for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
    int max = REPEATABLE_TASKS[i].threshold - 1;
    if (p.repeatableProgress[i] < 0) {
      p.repeatableProgress[i] = 0;
    } else if (p.repeatableProgress[i] > max) {
      p.repeatableProgress[i] = max;
    }
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
  configSetInt("missionMask", p->achievementMask);
  configSetInt("dailyStamp", p->dailyStamp);
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    configSetInt(TextFormat("day%d", i), p->dailyProgress[i]);
  }
  for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
    configSetInt(TextFormat("rep%d", i), p->repeatableProgress[i]);
  }
  for (int i = 0; i < PU_UPGRADE_COUNT; i++) {
    configSetInt(TextFormat("pu%d", i), p->puLevel[i]);
  }
  configSaveUser();
}

#ifdef PROGRESS_SELFTEST
// Headless check for task invariants, migration, cosmetic/task round-trips,
// daily rotation/reset, repeatable overflow, spending, and rewards. Build:
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

static void checkTaskDefs(const MissionDef *tasks, int count) {
  for (int i = 0; i < count; i++) {
    assert(tasks[i].stat >= MSTAT_COINS_RUN && tasks[i].stat <= MSTAT_RUNS);
    assert(tasks[i].threshold > 0 && "task threshold must be positive");
    assert(tasks[i].reward >= 0 && "task reward must be non-negative");
  }
}

int main(void) {
  checkTaskDefs(DAILY_TASK_POOL, DAILY_POOL_COUNT);
  checkTaskDefs(REPEATABLE_TASKS, REPEATABLE_TASK_COUNT);
  checkTaskDefs(ACHIEVEMENTS, ACHIEVEMENT_COUNT);
  configLoad("/no/such/save.json");
  configSetInt("skinMask", 15);    // legacy bundle keys are ignored
  configSetInt("selectedSkin", 3); // instead of partially migrating an outfit
  configSetInt("missionMask", 1);  // old missions become achievements
  Progress fresh = loadProgress();
  assert(fresh.achievementMask == 1);
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
  configSetInt("dailyStamp", localDayStamp());
  configSetInt("day0", 7);
  configSetInt("rep0", 8);

  Progress loaded = loadProgress();
  assert(loaded.dailyProgress[0] == 7);
  assert(loaded.repeatableProgress[0] == 8);
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

  Progress daily = {0};
  assert(refreshDailyForDay(&daily, 20260701));
  const MissionDef *d0 = dailyTask(&daily, 0);
  const MissionDef *d1 = dailyTask(&daily, 1);
  const MissionDef *d2 = dailyTask(&daily, 2);
  assert(d0 != d1 && d0 != d2 && d1 != d2); // no duplicate daily slots
  daily.dailyProgress[0] = 7;
  assert(!refreshDailyForDay(&daily, 20260701));
  assert(daily.dailyProgress[0] == 7);
  assert(refreshDailyForDay(&daily, 20260702));
  assert(daily.dailyProgress[0] == 0);

  Progress cumulative = {0};
  assert(refreshDailyForDay(&cumulative, localDayStamp()));
  cumulative.achievementMask = (1 << ACHIEVEMENT_COUNT) - 1;
  int dailyRewards = 0;
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    dailyRewards += dailyTask(&cumulative, i)->reward;
  }
  World taskRun = {
      .coinsCollected = 60, .bestCombo = 10, .level = 1, .comboBonus = 13000};
  for (int i = 0; i < 30; i++) {
    bankRun(&cumulative, &taskRun);
  }
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    assert(cumulative.dailyProgress[i] == dailyTask(&cumulative, i)->threshold);
  }
  assert(cumulative.repeatableProgress[0] == 800);   // 1800 total
  assert(cumulative.repeatableProgress[1] == 90000); // 390000 total
  assert(cumulative.repeatableProgress[2] == 0);
  assert(cumulative.coinBank == 1800 + 100 * 5 + dailyRewards);

  loaded.coinBank = 100;
  assert(!tryBuy(&loaded, -1) && loaded.coinBank == 100);
  assert(tryBuy(&loaded, 70) && loaded.coinBank == 30);
  assert(!tryBuy(&loaded, 40) && loaded.coinBank == 30);
  loaded.achievementMask = 0;
  progressRefreshDaily(&loaded);
  for (int i = 0; i < DAILY_TASK_COUNT; i++) {
    loaded.dailyProgress[i] = dailyTask(&loaded, i)->threshold;
  }
  for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
    loaded.repeatableProgress[i] = 0;
  }
  World w = {.coinsCollected = 1000,
             .bestCombo = 300,
             .level = 14,
             .comboBonus = 300000};
  assert(bankRun(&loaded, &w) == 9);
  assert(loaded.coinBank == 6430); // 30 + 1000 run + 400 repeat + 5000 rewards
  puts("ok - cosmetics, daily tasks, repeatables, and achievements");
  return 0;
}
#endif
