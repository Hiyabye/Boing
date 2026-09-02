// SHOP: compose and buy outfit pieces, then buy per-power-up upgrade levels.
// Catalog data lives in progress.c; this file owns only layout and click
// routing.
#include "game.h"

#include "background.h"
#include "core.h"
#include "player.h"
#include "progress.h"
#include "raylib.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#define PREVIEW_RX 27.0f
#define PREVIEW_RY 24.0f

// Four outfit categories replace the old four bundled-skin swatches.
static Rectangle rShopCategory(int i) {
  return centeredRow(175, 120, 110, i, COSMETIC_CATEGORY_COUNT);
}

// Power-up columns: title, icon, and up to three upgrade axes.
static const struct {
  const char *name;
  int icon; // PowerUpType
  int track[3];
} SHOP_PU[] = {
    {"MAGNET",
     POWERUP_MAGNET,
     {PU_MAGNET_SPAWN, PU_MAGNET_EFFECT, PU_MAGNET_DURATION}},
    {"ROCKET",
     POWERUP_ROCKET,
     {PU_ROCKET_SPAWN, PU_ROCKET_EFFECT, PU_ROCKET_DURATION}},
    {"BALLOON",
     POWERUP_BALLOON,
     {PU_BALLOON_SPAWN, PU_BALLOON_EFFECT, PU_BALLOON_DURATION}},
    {"SHIELD", POWERUP_SHIELD, {PU_SHIELD_SPAWN, PU_SHIELD_EFFECT, -1}},
    {"DEBUFFS", POWERUP_ANCHOR, {PU_DEBUFF_SUPPRESS, -1, -1}},
};
#define SHOP_PU_N (int)(sizeof(SHOP_PU) / sizeof(SHOP_PU[0]))

static int shopPuAxes(int p) {
  int n = 0;
  while (n < 3 && SHOP_PU[p].track[n] >= 0) {
    n++;
  }
  return n;
}

static Rectangle rShopUpgCard(int p) {
  return (Rectangle){22 + p * 154.0f, 336, 138, 312};
}
static Rectangle rShopUpgAxis(int p, int a) {
  Rectangle c = rShopUpgCard(p);
  return (Rectangle){c.x + 8, c.y + 84 + a * 74.0f, c.width - 16, 52};
}
static Rectangle rShopBack(void) { return centeredBtn(712, 200, 48); }

static Rectangle rPickerPanel(void) { return (Rectangle){60, 138, 680, 430}; }
static Rectangle rPickerItem(int i) {
  return centeredRow(214 + (i / 4) * 136.0f, 145, 120, i % 4, 4);
}
static Rectangle rPickerClose(void) { return centeredBtn(508, 170, 44); }

// Level indicator: five gold segments, one per purchased level.
static void drawLevelBar(float x, float y, float w, float h, int level) {
  const float gap = 3.0f;
  float segW = (w - (MAX_UPG_LEVEL - 1) * gap) / MAX_UPG_LEVEL;
  for (int i = 0; i < MAX_UPG_LEVEL; i++) {
    Rectangle seg = {x + i * (segW + gap), y, segW, h};
    bool on = i < level;
    DrawRectangleRounded(seg, 0.45f, 4, on ? GOLD : Fade(BLACK, 0.45f));
    if (on) {
      Rectangle gloss = {seg.x + 2, seg.y + 2, seg.width - 4, h / 2 - 2};
      DrawRectangleRounded(gloss, 0.5f, 3, Fade(RAYWHITE, 0.3f));
    }
  }
}

// Show only the category being browsed, over the currently selected body color.
static Character previewCharacter(const Progress *p, int category, int item) {
  Character c = progressCharacter(p);
  c.hat = HAT_NONE;
  c.glasses = GLS_NONE;
  c.mouthwear = MOUTHWEAR_NONE;
  switch (category) {
  case COSMETIC_COLOR:
    c.body = COSMETICS[category].items[item].body;
    c.bodyDark = COSMETICS[category].items[item].bodyDark;
    break;
  case COSMETIC_HEAD:
    c.hat = (unsigned char)item;
    break;
  case COSMETIC_EYES:
    c.glasses = (unsigned char)item;
    break;
  case COSMETIC_MOUTH:
    c.mouthwear = (unsigned char)item;
    break;
  }
  return c;
}

static void applyOutfit(Game *g) {
  Character c = progressCharacter(&g->progress);
  setPlayerCharacter(c);
  g->player.character = c;
}

static void selectCosmetic(Game *g, int category, int item) {
  Progress *p = &g->progress;
  const CosmeticDef *def = &COSMETICS[category].items[item];
  if (cosmeticOwned(p, category, item)) {
    p->selectedCosmetic[category] = (unsigned char)item;
    applyOutfit(g);
    saveProgress(p);
    sfx(SFX_SELECT);
  } else if (tryBuy(p, def->cost)) {
    p->cosmeticMask[category] |= 1 << item;
    p->selectedCosmetic[category] = (unsigned char)item;
    applyOutfit(g);
    saveProgress(p);
    sfx(SFX_POWERUP);
  } else {
    sfx(SFX_HIT);
  }
}

void updateShop(Game *g) {
  Progress *p = &g->progress;

  // Modal picker: only its choices and close button are live.
  if (g->shopCategory > 0) {
    int category = g->shopCategory - 1;
    int count = COSMETICS[category].count;
    for (int i = 0; i < count; i++) {
      if (uiItem(rPickerItem(i))) {
        selectCosmetic(g, category, i);
      }
    }
    bool close = uiItem(rPickerClose());
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || close) {
      g->shopCategory = 0;
      uiFocusReset();
      sfx(SFX_SELECT);
    }
    return;
  }

  bool categoryPressed[COSMETIC_CATEGORY_COUNT];
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    categoryPressed[i] = uiItem(rShopCategory(i));
  }

  for (int pu = 0; pu < SHOP_PU_N; pu++) {
    for (int a = 0; a < shopPuAxes(pu); a++) {
      if (!uiItem(rShopUpgAxis(pu, a))) {
        continue;
      }
      int track = SHOP_PU[pu].track[a];
      int lvl = upgLevel(p, track);
      if (lvl >= MAX_UPG_LEVEL) {
        sfx(SFX_SELECT);
      } else if (tryBuy(p, upgCost(track, lvl))) {
        p->puLevel[track]++;
        saveProgress(p);
        sfx(SFX_POWERUP);
      } else {
        sfx(SFX_HIT);
      }
    }
  }

  bool back = uiItem(rShopBack());
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    if (categoryPressed[i]) {
      g->shopCategory = i + 1;
      uiFocusReset();
      sfx(SFX_SELECT);
      return;
    }
  }
  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || back) {
    gotoState(g, GAMESTATE_MENU);
    sfx(SFX_SELECT);
  }
}

static void drawCosmeticCard(const Progress *p, int category, int item,
                             Rectangle r, bool categoryCard) {
  bool owned = cosmeticOwned(p, category, item);
  bool selected = p->selectedCosmetic[category] == item;
  DrawRectangleRounded(r, 0.2f, 8, Fade(BLACK, 0.52f));
  if (!categoryCard && selected) {
    DrawRectangleRoundedLines(r, 0.2f, 8, GOLD);
  } else if (mouseInRect(r)) {
    DrawRectangleRoundedLines(r, 0.2f, 8, RAYWHITE);
  }

  Character c = previewCharacter(p, category, item);
  float cx = r.x + r.width / 2.0f;
  float cy = r.y + (categoryCard ? 62.0f : 55.0f);
  drawBlobBody(cx, cy, PREVIEW_RX, PREVIEW_RY, c);
  drawAccessories(cx, cy, PREVIEW_RX, PREVIEW_RY, c);

  const CosmeticDef *def = &COSMETICS[category].items[item];
  int nameY = (int)(r.y + (categoryCard ? 86 : 82));
  drawTextShadow(def->name, (int)(cx - uiTextWidth(def->name, 14) / 2), nameY,
                 14, RAYWHITE);
  if (!categoryCard) {
    const char *tag = selected ? "EQUIPPED"
                      : owned  ? "OWNED"
                               : TextFormat("%d", def->cost);
    drawTextShadow(tag, (int)(cx - uiTextWidth(tag, 14) / 2), (int)r.y + 101,
                   14, owned ? (selected ? GOLD : RAYWHITE) : GOLD);
  }
}

static void drawPicker(const Progress *p, int category) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.38f));
  Rectangle panel = rPickerPanel();
  DrawRectangleRounded(panel, 0.07f, 12, Fade(BLACK, 0.92f));
  DrawRectangleRoundedLines(panel, 0.07f, 12, CANDY_PINK);

  const CosmeticCatalog *catalog = &COSMETICS[category];
  drawCenteredText(TextFormat("CHOOSE %s", catalog->name), 158, 30, CANDY_PINK);
  for (int i = 0; i < catalog->count; i++) {
    drawCosmeticCard(p, category, i, rPickerItem(i), false);
  }
  uiButton(rPickerClose(), "DONE", ICON_BACK);
}

void drawShop(Game *g) {
  Progress *p = &g->progress;
  drawMenuBackground();
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  drawCenteredText("SHOP", 40, 60, CANDY_PINK);

  const char *bankStr = TextFormat("%d", p->coinBank);
  float bx = SCREEN_WIDTH / 2.0f - (uiTextWidth(bankStr, 30) + 30) / 2.0f;
  drawCoinGlyph(bx, 118);
  drawTextShadow(bankStr, (int)bx + 16, 103, 30, GOLD);

  drawCenteredText("OUTFIT", 150, 24, RAYWHITE);
  for (int i = 0; i < COSMETIC_CATEGORY_COUNT; i++) {
    Rectangle r = rShopCategory(i);
    drawCosmeticCard(p, i, p->selectedCosmetic[i], r, true);
    const char *name = COSMETICS[i].name;
    drawTextShadow(name, (int)(r.x + r.width / 2 - uiTextWidth(name, 15) / 2),
                   (int)r.y + 7, 15, CANDY_PINK);
  }

  drawCenteredText("UPGRADES", 306, 20, RAYWHITE);
  for (int pu = 0; pu < SHOP_PU_N; pu++) {
    Rectangle card = rShopUpgCard(pu);
    DrawRectangleRounded(card, 0.12f, 8, Fade(BLACK, 0.35f));
    drawTextShadow(
        SHOP_PU[pu].name,
        (int)(card.x + card.width / 2 - uiTextWidth(SHOP_PU[pu].name, 18) / 2),
        (int)card.y + 12, 18, CANDY_PINK);
    drawPowerupGlyph(SHOP_PU[pu].icon,
                     (Vector2){card.x + card.width / 2, card.y + 54}, 18.0f,
                     (float)GetTime());
    for (int a = 0; a < shopPuAxes(pu); a++) {
      int track = SHOP_PU[pu].track[a];
      Rectangle ax = rShopUpgAxis(pu, a);
      int lvl = upgLevel(p, track);
      bool max = lvl >= MAX_UPG_LEVEL;
      if (!max && mouseInRect(ax)) {
        DrawRectangleRounded(ax, 0.25f, 6, Fade(RAYWHITE, 0.1f));
      }
      drawTextShadow(PU_UPGRADES[track].label, (int)ax.x + 4, (int)ax.y, 16,
                     max ? GOLD : RAYWHITE);
      const char *tag = max ? "MAX" : TextFormat("%d", upgCost(track, lvl));
      drawTextShadow(tag, (int)(ax.x + ax.width - 4 - uiTextWidth(tag, 16)),
                     (int)ax.y, 16, max ? Fade(GOLD, 0.7f) : GOLD);
      drawLevelBar(ax.x + 2, ax.y + 28, ax.width - 4, 16, lvl);
    }
  }

  uiButton(rShopBack(), "BACK", ICON_BACK);
  if (g->shopCategory > 0) {
    drawPicker(p, g->shopCategory - 1);
  }
}
