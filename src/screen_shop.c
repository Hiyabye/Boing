// SHOP — the coin-economy screen: buy/equip blob skins and buy per-power-up
// upgrade levels out of the banked coins. Tables live in the progress module;
// this file is only the layout and the click routing.
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

// Skin swatches across one row, then one card per power-up laid out the same
// way, each card stacking its upgrade axes vertically, then Back.
static Rectangle rShopSkin(int i) { return centeredRow(175, 120, 110, i, 4); }

// Power-up columns for the upgrade grid: header name, an icon (PowerUpType for
// drawPowerupGlyph — the Debuffs card borrows the Anchor glyph), and its
// PuTrack axes (-1 pads the ragged cards — Shield has 2 axes, Debuffs 1).
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

static int shopPuAxes(int p) { // count of real axes on card p (1..3)
  int n = 0;
  while (n < 3 && SHOP_PU[p].track[n] >= 0) {
    n++;
  }
  return n;
}

// Card holds: title, power-up icon below it, then the axis rows — with generous
// vertical spacing so the section doesn't crowd.
static Rectangle rShopUpgCard(int p) {
  return (Rectangle){22 + p * 154.0f, 336, 138, 312};
}
static Rectangle rShopUpgAxis(int p, int a) {
  Rectangle c = rShopUpgCard(p);
  return (Rectangle){c.x + 8, c.y + 84 + a * 74.0f, c.width - 16, 52};
}
static Rectangle rShopBack(void) { return centeredBtn(712, 200, 48); }

// Level indicator: a rounded bar split into MAX_UPG_LEVEL segments; the first
// `level` are filled gold (with a top gloss), the rest dark. Purely visual.
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

// Equip skin i: record the selection, set the global for future createPlayer
// calls (retry/return-to-menu), and recolor the live blob so it changes right
// away — startGame(fromMenu) carries this same player into play without a
// respawn, so the live update is what makes the very next run wear it too.
static void equipSkin(Game *g, int i) {
  g->progress.selectedSkin = i;
  setPlayerSkin(SKINS[i].character);
  g->player.character = SKINS[i].character;
}

// Buy, then click again to equip. Opened only from the menu, so BACK returns
// straight there.
void updateShop(Game *g) {
  Progress *p = &g->progress;
  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
      clicked(rShopBack())) {
    gotoState(g, GAMESTATE_MENU);
    sfx(SFX_SELECT);
    return;
  }
  for (int i = 0; i < SKIN_COUNT; i++) {
    if (!clicked(rShopSkin(i))) {
      continue;
    }
    if (skinOwned(p, i)) { // equip an owned skin
      equipSkin(g, i);
      saveProgress(p);
      sfx(SFX_SELECT);
    } else if (tryBuy(p, SKINS[i].cost)) { // buy + auto-equip
      p->skinMask |= (1 << i);
      equipSkin(g, i);
      saveProgress(p);
      sfx(SFX_POWERUP);
    } else {
      sfx(SFX_HIT); // can't afford
    }
  }
  for (int pu = 0; pu < SHOP_PU_N; pu++) {
    for (int a = 0; a < shopPuAxes(pu); a++) {
      if (!clicked(rShopUpgAxis(pu, a))) {
        continue;
      }
      int track = SHOP_PU[pu].track[a];
      if (track < 0) {
        continue; // padded slot (unreachable via shopPuAxes; keeps analyzer
                  // happy)
      }
      int lvl = upgLevel(p, track);
      if (lvl >= MAX_UPG_LEVEL) {
        sfx(SFX_SELECT); // maxed: no-op
      } else if (tryBuy(p, upgCost(track, lvl))) {
        p->puLevel[track]++;
        saveProgress(p);
        sfx(SFX_POWERUP);
      } else {
        sfx(SFX_HIT);
      }
    }
  }
}

void drawShop(Game *g) {
  Progress *p = &g->progress;
  drawMenuBackground();
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  drawCenteredText("SHOP", 40, 60, CANDY_PINK);

  // Coin bank: a coin glyph + total, centered under the title.
  const char *bankStr = TextFormat("%d", p->coinBank);
  float bx = SCREEN_WIDTH / 2.0f - (MeasureText(bankStr, 30) + 30) / 2.0f;
  drawCoinGlyph(bx, 118);
  drawTextShadow(bankStr, (int)bx + 16, 103, 30, GOLD);

  // Skins — swatch per palette with a mini blob preview + owned/selected/cost.
  drawCenteredText("SKINS", 150, 24, RAYWHITE);
  for (int i = 0; i < SKIN_COUNT; i++) {
    Rectangle r = rShopSkin(i);
    bool owned = skinOwned(p, i), sel = (p->selectedSkin == i);
    DrawRectangleRounded(r, 0.2f, 8, Fade(BLACK, 0.4f));
    if (sel) {
      DrawRectangleRoundedLines(r, 0.2f, 8, GOLD);
    } else if (mouseInRect(r)) {
      DrawRectangleRoundedLines(r, 0.2f, 8, RAYWHITE);
    }
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2 - 6;
    DrawEllipse(cx, cy, 30, 28, SKINS[i].character.bodyDark);
    DrawEllipse(cx, cy - 4, 24, 22, SKINS[i].character.body);
    const char *tag =
        owned ? (sel ? "EQUIPPED" : "OWNED") : TextFormat("%d", SKINS[i].cost);
    drawTextShadow(tag, (int)(cx - MeasureText(tag, 16) / 2),
                   (int)(r.y + r.height - 22), 16, owned ? RAYWHITE : GOLD);
  }

  // Upgrades — one card per power-up laid out horizontally (like the skins):
  // title, the power-up icon below it, then the axis rows stacked. Per axis:
  // short label + next cost, and a 5-segment level bar. Click an axis to buy.
  drawCenteredText("UPGRADES", 306, 20, RAYWHITE);
  for (int pu = 0; pu < SHOP_PU_N; pu++) {
    Rectangle card = rShopUpgCard(pu);
    DrawRectangleRounded(card, 0.12f, 8, Fade(BLACK, 0.35f));
    drawTextShadow(
        SHOP_PU[pu].name,
        (int)(card.x + card.width / 2 - MeasureText(SHOP_PU[pu].name, 18) / 2),
        (int)card.y + 12, 18, CANDY_PINK);
    // Power-up icon between the title and its upgrades.
    drawPowerupGlyph(SHOP_PU[pu].icon,
                     (Vector2){card.x + card.width / 2, card.y + 54}, 18.0f,
                     (float)GetTime());
    for (int a = 0; a < shopPuAxes(pu); a++) {
      int track = SHOP_PU[pu].track[a];
      if (track < 0) {
        continue; // padded slot (unreachable via shopPuAxes; keeps analyzer
                  // happy)
      }
      Rectangle ax = rShopUpgAxis(pu, a);
      int lvl = upgLevel(p, track);
      bool max = lvl >= MAX_UPG_LEVEL;
      if (!max && mouseInRect(ax)) {
        DrawRectangleRounded(ax, 0.25f, 6, Fade(RAYWHITE, 0.1f));
      }
      drawTextShadow(PU_UPGRADES[track].label, (int)ax.x + 4, (int)ax.y, 16,
                     max ? GOLD : RAYWHITE);
      const char *tag = max ? "MAX" : TextFormat("%d", upgCost(track, lvl));
      drawTextShadow(tag, (int)(ax.x + ax.width - 4 - MeasureText(tag, 16)),
                     (int)ax.y, 16, max ? Fade(GOLD, 0.7f) : GOLD);
      drawLevelBar(ax.x + 2, ax.y + 28, ax.width - 4, 16, lvl);
    }
  }

  uiButton(rShopBack(), "BACK", ICON_BACK);
}
