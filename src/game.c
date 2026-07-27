// Shared FSM plumbing: the state transitions, the high-score keys, and the bits
// of screen furniture more than one screen_*.c needs. Everything
// screen-specific lives in that screen's own file.
#include "game.h"

#include "persist.h"
#include "settings.h"
#include "sound.h"
#include "svg.h"
#include "theme.h"
#include "world.h"

#define COIN_GLYPH_BOX 26.0f // side of the coin glyph's 26x26 viewBox

RenderTexture2D gMenuSnap;

// ---------------------------------------------------------------------------
// Transitions
// ---------------------------------------------------------------------------
void gotoState(Game *g, GameState next) {
  g->prevState = g->state;
  g->state = next;
}

void startTransition(Game *g, GameState next) {
  if (g->fadePhase == 0) {
    g->fadePhase = 1;
    g->fadeNext = next;
  }
}

void returnToMenu(Game *g) {
  g->player = createPlayer();
  gotoState(g, GAMESTATE_MENU);
}

// fromMenu: keep the wandering attract blob (position + velocity carry over,
// gravity already lives in acceleration.y) for a seamless handoff; otherwise
// (retry) respawn a fresh player.
void startGame(Game *g, bool fromMenu) {
  worldSetUpgrades(g->progress.puLevel); // gate upgrade effects for this run
  g->world = createWorld();
  if (!fromMenu) {
    g->player = createPlayer();
  }
  g->accumulator = 0.0f;
  g->newBest = false;
  gotoState(g, GAMESTATE_PLAYING);
  sfx(SFX_SELECT);
}

// ---------------------------------------------------------------------------
// High score persistence (shared save.json beside the exe)
// ---------------------------------------------------------------------------
int loadHighScore(void) {
  return configGetInt("highScore", 0); // keeps 0 on a missing/garbage file
}

void saveHighScore(int best) {
  configSetInt("highScore", best);
  configSave("save.json");
}

// ---------------------------------------------------------------------------
// Shared screen furniture
// ---------------------------------------------------------------------------
Rectangle centeredBtn(float y, float w, float h) {
  return (Rectangle){SCREEN_WIDTH / 2.0f - w / 2.0f, y, w, h};
}

Rectangle centeredRow(float y, float w, float h, int idx, int n) {
  float gap = 24.0f;
  float total = n * w + (n - 1) * gap;
  return (Rectangle){SCREEN_WIDTH / 2.0f - total / 2.0f + idx * (w + gap), y, w,
                     h};
}

// Two layers rather than one glyph: the coin's colors are theme state, so each
// layer is authored white and tinted here. The face has a star punched out of
// it, letting the rim beneath read through as a struck mint mark.
void drawCoinGlyph(float x, float y) {
  drawSvg(SVG_COIN_RIM, x, y, COIN_GLYPH_BOX, 0.0f, COIN_LO);
  drawSvg(SVG_COIN_FACE, x, y, COIN_GLYPH_BOX, 0.0f, COIN_HI);
}

// ---------------------------------------------------------------------------
// SETTINGS — an overlay, so it draws whichever screen opened it underneath
// ---------------------------------------------------------------------------
void updateSettingsState(Game *g) {
  updateSettingsMenu(&g->settings, &g->state, g->prevState);
}

void drawSettingsState(Game *g) {
  if (g->prevState == GAMESTATE_MENU) {
    drawMenu(g);
  } else {
    drawPlay(g);
  }
  drawSettingsMenu(&g->settings);
}
