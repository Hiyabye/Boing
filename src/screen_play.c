// PLAYING / PAUSED / GAMEOVER — the three states that share one draw handler
// over the live world, plus the HUD and the game-over run-summary card.
#include "game.h"

#include "background.h"
#include "core.h"
#include "player.h"
#include "progress.h"
#include "raylib.h"
#include "rlgl.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#include <math.h>

#define FIXED_DT (1.0f / 60.0f)
#define MAX_FRAME_TIME 0.25f // clamp on a slow frame so physics can't spiral
#define CYCLE_REVEAL_PHASE 0.18f

static Rectangle rPauseBtn(void) {
  return (Rectangle){SCREEN_WIDTH - 54, 10, 44, 44};
}
static Rectangle rResume(void) {
  return centeredRow(SCREEN_HEIGHT / 2.0f + 40, 160, 56, 0, 3);
}
static Rectangle rPauseSettings(void) {
  return centeredRow(SCREEN_HEIGHT / 2.0f + 40, 160, 56, 1, 3);
}
static Rectangle rPauseHome(void) {
  return centeredRow(SCREEN_HEIGHT / 2.0f + 40, 160, 56, 2, 3);
}
static Rectangle rRetry(void) {
  return centeredRow(SCREEN_HEIGHT * 2.0f / 3, 200, 60, 0, 2);
}
static Rectangle rGameoverHome(void) {
  return centeredRow(SCREEN_HEIGHT * 2.0f / 3, 200, 60, 1, 2);
}

// ---------------------------------------------------------------------------
// Update handlers
// ---------------------------------------------------------------------------
void updatePlaying(Game *g) {
  if (IsKeyPressed(KEY_P) || clicked(rPauseBtn())) {
    gotoState(g, GAMESTATE_PAUSED);
    return;
  }
  // Fixed 60Hz steps to catch up to real time; cap to avoid a spiral of death.
  g->accumulator += GetFrameTime();
  if (g->accumulator > MAX_FRAME_TIME) {
    g->accumulator = MAX_FRAME_TIME;
  }
  // Follow the mouse only once it moves; pressing an arrow reclaims control so
  // a keyboard player isn't tugged toward a resting cursor.
  Vector2 md = GetMouseDelta();
  if (md.x != 0.0f || md.y != 0.0f) {
    g->mouseControl = true;
  }
  if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT)) {
    g->mouseControl = false;
  }
  float mouseX = g->mouseControl ? virtualMouse().x : -1.0f;
  while (g->accumulator >= FIXED_DT) {
    g->accumulator -= FIXED_DT;
    if (g->world.hitstop > 0) {
      g->world.hitstop--; // impact freeze: hold everything for a few frames
      continue;
    }
    // Run-start anticipation: the blob is pinned to the grass squishing down,
    // so it takes no input and no gravity — but the world still runs, or the
    // coins and platforms it spawns would all pop in at once on release.
    if (g->player.crouch > 0) {
      updateCrouch(&g->player);
      updateWorld(&g->world, &g->player);
      continue;
    }
    handlePlayerInput(&g->player, mouseX, worldInputDir(&g->world));
    updatePlayer(&g->player);
    updateWorld(&g->world, &g->player);
    if (isPlayerFallen(&g->world, &g->player)) {
      g->player.expression = EXPR_SAD; // sad face through the game-over screen
      int score = getScore(&g->world);
      if (score > g->highScore) {
        g->highScore = score;
        g->newBest = true;
        saveHighScore(g->highScore);
        sfx(SFX_NEWBEST); // new best: dedicated celebratory jingle
      } else {
        sfx(SFX_GAMEOVER); // ordinary death: sad tone
      }
      // Bank coins, advance tasks/achievements, then persist the rewards.
      bankRun(&g->progress, &g->world);
      saveProgress(&g->progress);
      gotoState(g, GAMESTATE_GAMEOVER);
      return;
    }
  }
}

// PLAYING registers no focusable items on purpose — the arrow keys steer the
// blob there, and a focus ring would eat them. PAUSED and GAMEOVER register
// theirs unconditionally so the ring never jumps mid-frame.
void updatePaused(Game *g) {
  bool resume = uiItem(rResume());
  bool settings = uiItem(rPauseSettings());
  bool home = uiItem(rPauseHome());

  if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE) || resume) {
    gotoState(g, GAMESTATE_PLAYING);
  } else if (IsKeyPressed(KEY_S) || settings) {
    gotoState(g, GAMESTATE_SETTINGS);
    sfx(SFX_SELECT);
  } else if (home) {
    returnToMenu(g); // back to the menu's attract demo
    sfx(SFX_SELECT);
  }
}

void updateGameover(Game *g) {
  bool retry = uiItem(rRetry());
  bool home = uiItem(rGameoverHome());

  if (IsKeyPressed(KEY_SPACE) || retry) {
    startGame(g, false);
  } else if (home) {
    returnToMenu(g); // back to the menu's attract demo
    sfx(SFX_SELECT);
  }
}

// ---------------------------------------------------------------------------
// HUD + game-over card
// ---------------------------------------------------------------------------
// Score, coin, continuous height, and power-up timers in screen space (drawn
// after the world's rlPopMatrix, in the frame's scaled 800x800 space).
static void drawHud(const World *world) {
  float centerW = 360, centerX = (SCREEN_WIDTH - centerW) / 2.0f;
  const char *scoreStr = TextFormat("SCORE %d", getScore(world));
  int scoreSize = 30;
  // Keep 8px clear of the centered height badge; the score badge starts at x=8
  // and adds 24px of horizontal padding around the text.
  const int scoreMaxWidth = (int)centerX - 40;
  while (scoreSize > 8 && uiTextWidth(scoreStr, scoreSize) > scoreMaxWidth) {
    scoreSize--;
  }
  DrawRectangleRounded(
      (Rectangle){8, 8, uiTextWidth(scoreStr, scoreSize) + 24, 40}, 0.4f, 8,
      Fade(BLACK, 0.28f));
  drawTextShadow(scoreStr, 20, 8 + (40 - scoreSize) / 2, scoreSize, RAYWHITE);

  // Coin tally: a mini coin glyph + count, tucked below the score badge on the
  // left so the top-right pause button doesn't hide it.
  const char *coinStr = TextFormat("%d", world->coinsCollected);
  float cbx = 8, cby = 52;
  float cbw = 24 + 8 + uiTextWidth(coinStr, 30) + 20;
  DrawRectangleRounded((Rectangle){cbx, cby, cbw, 40}, 0.4f, 8,
                       Fade(BLACK, 0.28f));
  float gcx = cbx + 20, gcy = cby + 20;
  drawCoinGlyph(gcx, gcy);
  drawTextShadow(coinStr, (int)gcx + 16, (int)cby + 5, 30, RAYWHITE);

  // Height never resets at an era seam. The cycle label appears only after the
  // rebuilt Hills safe-arrival window; on later loops the previous number
  // holds until then instead of ticking over at the mathematical boundary.
  ThemeSample journey = themeSampleAt(worldPeakAscent(world));
  int height = (int)((FLOOR_Y - world->bestY) / 10.0f);
  int cycle = journey.cycle + 1;
  if (journey.era == ERA_HILLS && journey.phase < CYCLE_REVEAL_PHASE) {
    cycle--;
  }
  const char *climbStr =
      cycle > 1 ? TextFormat("HEIGHT %d m  CYCLE %d", height, cycle)
                : TextFormat("HEIGHT %d m", height);
  int climbSize = 20;
  while (climbSize > 8 &&
         uiTextWidth(climbStr, climbSize) > (int)centerW - 24) {
    climbSize--;
  }
  DrawRectangleRounded((Rectangle){centerX, 8, centerW, 40}, 0.4f, 8,
                       Fade(BLACK, 0.28f));
  drawTextShadow(climbStr,
                 (SCREEN_WIDTH - uiTextWidth(climbStr, climbSize)) / 2,
                 8 + (40 - climbSize) / 2, climbSize, RAYWHITE);

  // Combo multiplier sits below the height badge, growing with the chain.
  if (world->combo > 1) {
    drawCenteredText(TextFormat("COMBO x%d", world->combo), 54,
                     20 + (world->combo > 10 ? 10 : world->combo), GOLD);
  }

  // One timer bar per active power-up, stacked (magnet + rocket can overlap).
  int bx = 12, by = 98, bw = 150; // below the score + coin badges
  for (PowerUpType t = 0; t < POWERUP_COUNT; t++) {
    float timer = worldPowerTimer(world, t);
    if (timer <= 0.0f) {
      continue;
    }
    PowerUpDef d = POWERUP_DEFS[t];
    float frac =
        fminf(timer / d.duration,
              1.0f); // clamp: a Duration upgrade sets timer > base duration
    DrawRectangleRounded((Rectangle){bx, by, bw, 26}, 0.5f, 8,
                         Fade(BLACK, 0.3f));
    DrawRectangleRounded((Rectangle){bx, by, bw * frac, 26}, 0.5f, 8,
                         Fade(d.color, 0.85f));
    drawTextShadow(d.label, bx + 12, by + 4, 18, RAYWHITE);
    by += 32;
  }
}

// Game-over run summary: a rounded card with height, coins, best combo, and the
// era reached — closure + a screenshot-worthy moment.
static void drawSummaryCard(Game *g) {
  const World *w = &g->world;
  ThemeSample journey = themeSampleAt(worldPeakAscent(w));
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  if (g->newBest) {
    drawCenteredText("NEW BEST!", 150, 60, GOLD);
  } else {
    drawCenteredText("GAME OVER", 150, 60, RAYWHITE);
  }

  Rectangle panel = {SCREEN_WIDTH / 2.0f - 210, 240, 420, 300};
  DrawRectangleRounded(panel, 0.08f, 12, Fade(BLACK, 0.5f));
  DrawRectangleRoundedLines(panel, 0.08f, 12, UI_ACCENT);

  struct {
    const char *label, *value;
  } rows[] = {
      {"HEIGHT", TextFormat("%d m", (int)((FLOOR_Y - w->bestY) / 10.0f))},
      {"COINS", TextFormat("%d", w->coinsCollected)},
      {"BEST COMBO", TextFormat("x%d", w->bestCombo)},
      {"ERA", THEMES[journey.era].name},
  };
  int ry = 268;
  for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); i++) {
    drawTextShadow(rows[i].label, (int)panel.x + 30, ry, 24, RAYWHITE);
    int vw = uiTextWidth(rows[i].value, 24);
    drawTextShadow(rows[i].value, (int)(panel.x + panel.width - 30 - vw), ry,
                   24, COIN_LO);
    ry += 40;
  }

  drawCenteredText(TextFormat("SCORE %d", getScore(w)), 452, 40, RAYWHITE);
  drawCenteredText(TextFormat("BEST %d", g->highScore), 500, 24, GOLD);
}

// ---------------------------------------------------------------------------
// Shared draw: the world, then the state-specific overlay
// ---------------------------------------------------------------------------
void drawPlay(Game *g) {
  float cameraY = worldCameraY(&g->world), shake = worldShake(&g->world);
  ThemeSample view = themeSampleAt(worldViewAscent(&g->world));
  drawGameBackground(cameraY);
  float sx = 0.0f, sy = 0.0f;
  if (shake > 0.0f) {
    sx = GetRandomValue(-100, 100) / 100.0f * shake;
    sy = GetRandomValue(-100, 100) / 100.0f * shake;
  }
  // Camera is a plain translation (zoom 1, no rotation): apply it as a
  // translate *inside* the frame's scale matrix instead of BeginMode2D, which
  // does rlLoadIdentity (discarding our scale) and whose EndMode2D restores
  // raylib's HiDPI screenScale — that left every screen-space draw after it
  // (HUD, pause overlay) doubled on Retina.
  rlPushMatrix();
  rlTranslatef(-sx, -(cameraY + sy), 0.0f);
  drawWorld(&g->world);
  drawPowerAura(&g->world, &g->player); // living glow behind the blob
  drawPlayer(&g->player);
  rlPopMatrix();
  // Near-camera theme details sit in front of the world but stay clipped to
  // the outer edges, preserving the playfield and HUD readability.
  drawGameForeground(cameraY);

  // Post-processing for this frame. Bloom follows the altitude's theme, from
  // subtle daylight to the strongest neon glow. The Fog debuff is a blur +
  // drifting haze in the same pass — which means it fogs the HUD too, unlike
  // the flat dim rect it replaced; that's the point, it's a hazard. Fades out
  // over the last ~30 frames so it lifts smoothly instead of popping.
  g->post.bloom = themeGlow(view);
  g->post.fog = fminf(worldFog(&g->world), 30.0f) / 30.0f;

  drawHud(&g->world);

  if (g->state == GAMESTATE_PLAYING) {
    uiButton(rPauseBtn(), "", ICON_PAUSE);
  } else if (g->state == GAMESTATE_PAUSED) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.35f));
    drawCenteredText("PAUSED", SCREEN_HEIGHT / 2 - 40, 50, RAYWHITE);
    uiButton(rResume(), "RESUME", ICON_PLAY);
    uiButton(rPauseSettings(), "SETTINGS", ICON_GEAR);
    uiButton(rPauseHome(), "HOME", ICON_HOME);
  } else if (g->state == GAMESTATE_GAMEOVER) {
    drawSummaryCard(g);
    uiButton(rRetry(), "RETRY", ICON_RETRY);
    uiButton(rGameoverHome(), "HOME", ICON_HOME);
  }
}
