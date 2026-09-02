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
      // Bank this run's coins + score any newly-cleared missions, then persist.
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
// Era names in climb order — one per THEMES[] row (theme.c), same order. Themes
// loop, so callers index worldEra(). Homage to a genre/era each, never a named
// product.
static const char *ERA_NAMES[] = {"SUNNY HILLS", "BIRDSONG SKIES",
                                  "NIGHT BLOCKS", "NEON MAZE",
                                  "FALLING BLOCKS"};
// A theme without a name here would show the wrong era (or read past the end).
STATIC_ASSERT(sizeof(ERA_NAMES) / sizeof(ERA_NAMES[0]) == THEME_COUNT,
              era_names_full);

// Score badge + coin tally + level bar + power-up timer bars, in screen space
// (drawn after the world's rlPopMatrix, in the frame's scaled 800x800 space).
static void drawHud(const World *world) {
  float pw = 360, ph = 12, px = (SCREEN_WIDTH - pw) / 2.0f, py = 30;
  const char *scoreStr = TextFormat("SCORE %d", getScore(world));
  int scoreSize = 30;
  // Keep 8px clear of the level bar; the badge starts at x=8 and adds 24px
  // of horizontal padding around the text.
  const int scoreMaxWidth = (int)px - 40;
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

  // Level progress bar (top-center): how far into the current theme band the
  // climb has reached (f = blend toward the next theme).
  int lo, hi;
  float f;
  themeBlend(&lo, &hi, &f);
  const char *lvlStr = TextFormat("LEVEL %d", worldLevel(world) + 1);
  drawTextShadow(lvlStr, (int)(SCREEN_WIDTH - uiTextWidth(lvlStr, 18)) / 2, 8,
                 18, RAYWHITE);
  DrawRectangleRounded((Rectangle){px, py, pw, ph}, 0.9f, 8, Fade(BLACK, 0.3f));
  DrawRectangleRounded((Rectangle){px, py, pw * f, ph}, 0.9f, 8,
                       Fade(COIN_HI, 0.9f));

  // Combo multiplier: shown centered under the level bar while a chain is
  // going, bigger the higher it climbs.
  if (world->combo > 1) {
    drawCenteredText(TextFormat("COMBO x%d", world->combo), (int)py + 22,
                     20 + (world->combo > 10 ? 10 : world->combo), GOLD);
  }

  // Era callout: each band changes the *rules*, not just the palette, so the
  // level-up says which era you just walked into. Fades out over its last
  // ~0.7s; sits clear of the combo readout above it.
  if (world->levelBanner > 0.0f) {
    float a = fminf(world->levelBanner / 40.0f, 1.0f);
    drawCenteredText(ERA_NAMES[worldEra(world)], 112, 34, Fade(GOLD, a));
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
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  if (g->newBest) {
    drawCenteredText("NEW BEST!", 150, 60, GOLD);
  } else {
    drawCenteredText("GAME OVER", 150, 60, RAYWHITE);
  }

  Rectangle panel = {SCREEN_WIDTH / 2.0f - 210, 240, 420, 300};
  DrawRectangleRounded(panel, 0.08f, 12, Fade(BLACK, 0.5f));
  DrawRectangleRoundedLines(panel, 0.08f, 12, CANDY_PINK);

  struct {
    const char *label, *value;
  } rows[] = {
      {"HEIGHT", TextFormat("%d m", (int)((FLOOR_Y - w->bestY) / 10.0f))},
      {"COINS", TextFormat("%d", w->coinsCollected)},
      {"BEST COMBO", TextFormat("x%d", w->bestCombo)},
      {"ERA", ERA_NAMES[worldEra(w)]},
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
  // drawGameBackground already ran themeUpdate above, so the glow is current.
  g->post.bloom = themeGlow();
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
