#ifndef GAME_H
#define GAME_H

#include "raylib.h"

#include "core.h"
#include "player.h"
#include "post.h"
#include "progress.h"
#include "settings.h"
#include "world.h"

#include <stdbool.h>

// The FSM's shared surface. Each screen lives in its own screen_*.c and exposes
// nothing but the {update, draw} pair at the bottom of this header; main.c
// wires those into the STATES[] dispatch table and owns the loop. Anything two
// screens both need (the context struct, transitions, button geometry) lives in
// game.c.

// ---------------------------------------------------------------------------
// Game context — the whole mutable state, threaded through every FSM handler.
// ---------------------------------------------------------------------------
typedef struct {
  GameState state;
  GameState prevState; // the state we came from (settings returns here)
  Player player;
  World world;
  Settings settings;
  Progress progress; // bank + outfit/upgrade/mission unlocks (persisted)
  int highScore;
  float accumulator;   // fixed-timestep carry
  bool mouseControl;   // true once the mouse moves; arrows reclaim control
  bool quit;           // set by the menu EXIT button to end the loop
  bool showHelp;       // menu "How to Play" overlay toggled by the ? button
  bool showMissions;   // menu "Missions" overlay toggled by the MISSIONS button
  int shopCategory;    // 0 closed, otherwise CosmeticCategory + 1 modal picker
  float creditsAnim;   // 0 = fullscreen picture, 1 = zoomed out to the CRT
  float creditsStatic; // "change channel" static timer, runs once zoomed out
  bool creditsExiting; // BACK pressed: ramp creditsAnim back to 0, then MENU
  bool newBest;        // last run beat the high score (game-over banner)
  PostFx post;         // per-frame post-FX mailbox: zeroed by main(), raised by
                       // the drawing screen, consumed by postEnd()
  float fade;         // black-overlay alpha for a state transition (0 clear..1)
  int fadePhase;      // 0 none, 1 dipping to black, 2 lifting back in
  GameState fadeNext; // state to switch to at the bottom of the dip
} Game;

// ---------------------------------------------------------------------------
// Transitions (game.c)
// ---------------------------------------------------------------------------
// Every state change goes through gotoState so `prevState` is always the screen
// we came from — that's what SETTINGS returns to.
void gotoState(Game *g, GameState next);
// Fade to black, swap to `next` at the bottom of the dip, fade back in. The dip
// itself is driven by the main loop.
void startTransition(Game *g, GameState next);
// Back to the menu's attract demo (fresh wandering blob + MENU state). Callers
// fire their own SFX — the credits exit is deliberately silent.
void returnToMenu(Game *g);
// Enter gameplay. fromMenu keeps the wandering attract blob for a seamless
// handoff; otherwise (retry) it respawns a fresh player.
void startGame(Game *g, bool fromMenu);

// High score, in the shared save.json beside the exe.
int loadHighScore(void);
void saveHighScore(int best);

// ---------------------------------------------------------------------------
// Shared screen furniture (game.c)
// ---------------------------------------------------------------------------
// Button rects live in virtual (800x800) space. Convention: each button is one
// rect helper in its screen's file, referenced by uiButton(rect,...) in draw()
// and by uiItem(rect) in the matching update(). These two build the common
// layouts: a w-wide, h-tall button centered at row y, and button `idx` of `n`
// equal buttons laid out as one centered row.
Rectangle centeredBtn(float y, float w, float h);
Rectangle centeredRow(float y, float w, float h, int idx, int n);
// Mini coin glyph (rim + face) centered at (x,y) — HUD tally and shop bank.
void drawCoinGlyph(float x, float y);

// The menu, frozen the instant CREDITS is pressed (written by the menu screen,
// zoomed onto the tube by the credits screen). Owned by main().
extern RenderTexture2D gMenuSnap;

// ---------------------------------------------------------------------------
// Per-screen handlers — one pair per GameState, wired up by STATES[] in main.c
// ---------------------------------------------------------------------------
void updateSplash(Game *g); // screen_splash.c
void drawSplash(Game *g);

void updateMenu(Game *g); // screen_menu.c
void drawMenu(Game *g);

void updateShop(Game *g); // screen_shop.c
void drawShop(Game *g);

void updateCredits(Game *g); // screen_credits.c
void drawCredits(Game *g);

void updatePlaying(Game *g); // screen_play.c — PLAYING/PAUSED/GAMEOVER share
void updatePaused(Game *g);  // one draw handler over the live world
void updateGameover(Game *g);
void drawPlay(Game *g);

void updateSettingsState(
    Game *g); // game.c — thin bridge to the settings module
void drawSettingsState(Game *g);

#endif // GAME_H
