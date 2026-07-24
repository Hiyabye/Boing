#include "raylib.h"
#include "rlgl.h"

#include "background.h"
#include "core.h"
#include "persist.h"
#include "player.h"
#include "progress.h"
#include "settings.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#include <math.h>

// ---------------------------------------------------------------------------
// Game context — the whole mutable state, threaded through every FSM handler.
// ---------------------------------------------------------------------------
typedef struct {
  GameState state;
  GameState prevState; // the state we came from (settings returns here)
  Player player;
  World world;
  Settings settings;
  Progress progress; // coin bank + skin/upgrade/mission unlocks (persisted)
  int highScore;
  float accumulator;   // fixed-timestep carry
  bool mouseControl;   // true once the mouse moves; arrows reclaim control
  bool quit;           // set by the menu EXIT button to end the loop
  bool showHelp;       // menu "How to Play" overlay toggled by the ? button
  float creditsAnim;   // 0 = fullscreen picture, 1 = zoomed out to the CRT
  float creditsStatic; // "change channel" static timer, runs once zoomed out
  bool creditsExiting; // BACK pressed: ramp creditsAnim back to 0, then MENU
  bool newBest;        // last run beat the high score (game-over banner)
} Game;

#define FIXED_DT (1.0f / 60.0f)
#define MAX_FRAME_TIME 0.25f // clamp on a slow frame so physics can't spiral

// The menu, frozen the instant CREDITS is pressed, shown on the CRT tube while
// the camera pulls back. Loaded beside the main render target in main().
static RenderTexture2D gMenuSnap;
static void
drawMenu(Game *g); // defined below; captured into gMenuSnap on enter

// Every state transition goes through here so `prevState` is always the screen
// we came from — that's what SETTINGS returns to, replacing a hand-threaded
// return field.
static void gotoState(Game *g, GameState next) {
  g->prevState = g->state;
  g->state = next;
}

// Back to the menu's attract demo: fresh wandering blob + MENU state. Callers
// fire their own SFX (the credits exit is silent), so this stays sfx-free.
static void returnToMenu(Game *g) {
  g->player = createPlayer();
  gotoState(g, GAMESTATE_MENU);
}

// Button rects, in virtual (800x800) space. Convention: each button is one rect
// helper here, referenced by uiButton(rect,...) in a draw() handler and by
// clicked(rect) in the matching update() handler — declare once, use in both.
// A w-wide, h-tall button horizontally centered at row y.
static Rectangle centeredBtn(float y, float w, float h) {
  return (Rectangle){SCREEN_WIDTH / 2.0f - w / 2.0f, y, w, h};
}
// Button `idx` of `n` equal w-wide buttons laid out as one centered row at y.
static Rectangle centeredRow(float y, float w, float h, int idx, int n) {
  float gap = 24.0f;
  float total = n * w + (n - 1) * gap;
  return (Rectangle){SCREEN_WIDTH / 2.0f - total / 2.0f + idx * (w + gap), y, w,
                     h};
}
static Rectangle rPauseBtn(void) {
  return (Rectangle){SCREEN_WIDTH - 54, 10, 44, 44};
}
static Rectangle rHelpBtn(void) { return (Rectangle){10, 10, 44, 44}; }
static Rectangle rHelpClose(void) {
  return centeredBtn(SCREEN_HEIGHT / 2.0f + 168, 180, 54);
}
static Rectangle rMenuPlay(void) { return centeredBtn(400, 240, 58); }
static Rectangle rMenuShop(void) { return centeredBtn(466, 240, 54); }
static Rectangle rMenuSettings(void) { return centeredBtn(530, 240, 54); }
static Rectangle rMenuCredits(void) { return centeredBtn(594, 240, 54); }
static Rectangle rMenuExit(void) { return centeredBtn(658, 240, 54); }
// Shop screen: 4 skin swatches, 3 upgrade rows, a Back button.
static Rectangle rShopSkin(int i) { return centeredRow(175, 120, 110, i, 4); }
static Rectangle rShopUpg(int i) {
  return (Rectangle){150, 330 + i * 52.0f, 500, 46};
}
static Rectangle rShopBack(void) { return centeredBtn(710, 200, 50); }
static Rectangle rCreditsBack(void) {
  return centeredBtn(SCREEN_HEIGHT - 110, 200, 54);
}
static Rectangle rResume(void) {
  return centeredRow(SCREEN_HEIGHT / 2.0f + 40, 160, 56, 0, 2);
}
static Rectangle rPauseSettings(void) {
  return centeredRow(SCREEN_HEIGHT / 2.0f + 40, 160, 56, 1, 2);
}
static Rectangle rPauseHome(void) {
  return centeredBtn(SCREEN_HEIGHT / 2.0f + 108, 160, 56);
}
static Rectangle rRetry(void) {
  return centeredRow(SCREEN_HEIGHT * 2.0f / 3, 200, 60, 0, 2);
}
static Rectangle rGameoverHome(void) {
  return centeredRow(SCREEN_HEIGHT * 2.0f / 3, 200, 60, 1, 2);
}

// Score badge + coin tally + level bar + power-up timer bars, in screen space
// (drawn after the world's rlPopMatrix, in the frame's scaled 800x800 space).
static void drawHud(const World *world) {
  const char *scoreStr = TextFormat("SCORE %d", getScore(world));
  DrawRectangleRounded((Rectangle){8, 8, MeasureText(scoreStr, 30) + 24, 40},
                       0.4f, 8, Fade(BLACK, 0.28f));
  drawTextShadow(scoreStr, 20, 13, 30, RAYWHITE);

  // Coin tally: a mini coin glyph + count, tucked below the score badge on the
  // left so the top-right pause button doesn't hide it.
  const char *coinStr = TextFormat("%d", world->coinsCollected);
  float cbx = 8, cby = 52;
  float cbw = 24 + 8 + MeasureText(coinStr, 30) + 20;
  DrawRectangleRounded((Rectangle){cbx, cby, cbw, 40}, 0.4f, 8,
                       Fade(BLACK, 0.28f));
  float gcx = cbx + 20, gcy = cby + 20;
  DrawEllipse(gcx, gcy, 11, 13, COIN_LO);
  DrawEllipse(gcx, gcy, 7.5f, 9, COIN_HI);
  drawTextShadow(coinStr, (int)gcx + 16, (int)cby + 5, 30, RAYWHITE);

  // Level progress bar (top-center): how far into the current theme band the
  // climb has reached (f = blend toward the next theme).
  int lo, hi;
  float f;
  themeBlend(&lo, &hi, &f);
  float pw = 360, ph = 12, px = (SCREEN_WIDTH - pw) / 2.0f, py = 30;
  const char *lvlStr = TextFormat("LEVEL %d", worldLevel(world) + 1);
  drawTextShadow(lvlStr, (int)(SCREEN_WIDTH - MeasureText(lvlStr, 18)) / 2, 8,
                 18, RAYWHITE);
  DrawRectangleRounded((Rectangle){px, py, pw, ph}, 0.9f, 8, Fade(BLACK, 0.3f));
  DrawRectangleRounded((Rectangle){px, py, pw * f, ph}, 0.9f, 8,
                       Fade(COIN_HI, 0.9f));

  // Combo multiplier: shown centered under the level bar while a chain is going,
  // bigger the higher it climbs.
  if (world->combo > 1) {
    drawCenteredText(TextFormat("COMBO x%d", world->combo), (int)py + 22,
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
    float frac = timer / d.duration;
    DrawRectangleRounded((Rectangle){bx, by, bw, 26}, 0.5f, 8,
                         Fade(BLACK, 0.3f));
    DrawRectangleRounded((Rectangle){bx, by, bw * frac, 26}, 0.5f, 8,
                         Fade(d.color, 0.85f));
    drawTextShadow(d.label, bx + 12, by + 4, 18, RAYWHITE);
    by += 32;
  }
}

// ---------------------------------------------------------------------------
// High score persistence (plain text file beside the exe)
// ---------------------------------------------------------------------------
static int loadHighScore(void) {
  int best = 0;
  readInts("highscore", &best, 1); // keeps 0 on a missing/garbage file
  return best;
}

static void saveHighScore(int best) { writeInts("highscore", &best, 1); }

// ---------------------------------------------------------------------------
// Transitions
// ---------------------------------------------------------------------------
// Enter gameplay. fromMenu: keep the wandering attract blob (position +
// velocity carry over, gravity already lives in acceleration.y) for a seamless
// handoff; otherwise (retry) respawn a fresh player.
static void startGame(Game *g, bool fromMenu) {
  worldSetUpgrades(g->progress.upgradeMask); // gate upgrade effects for this run
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
// Per-state update handlers
// ---------------------------------------------------------------------------
// Static "How to Play" lines — concise, no interactive walkthrough.
static const char *HELP_LINES[] = {
    "Move:  arrow keys or mouse",          "Grab coins to launch higher",
    "Bounce off the cloud platforms",      "Dodge the grape monsters",
    "Magnet & Rocket power-ups boost you", "Never fall below the screen",
};
#define HELP_LINE_COUNT ((int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0])))

static void drawHelpOverlay(void) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  Rectangle panel = {SCREEN_WIDTH / 2.0f - 300, 180, 600, 420};
  DrawRectangleRounded(panel, 0.08f, 12, Fade(BLACK, 0.55f));
  DrawRectangleRoundedLines(panel, 0.08f, 12, CANDY_PINK);
  drawCenteredText("HOW TO PLAY", 210, 48, CANDY_PINK);
  int y = 300;
  for (int i = 0; i < HELP_LINE_COUNT; i++) {
    drawCenteredText(HELP_LINES[i], y, 26, RAYWHITE);
    y += 44;
  }
  uiButton(rHelpClose(), "GOT IT", ICON_PLAY);
}

static void updateMenu(Game *g) {
  updateAttract(&g->player); // demo: blob wanders the menu
  if (g->showHelp) {         // modal open: only close-input is live
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
        clicked(rHelpClose())) {
      g->showHelp = false;
      sfx(SFX_SELECT);
    }
    return;
  }
  if (clicked(rHelpBtn())) {
    g->showHelp = true;
    sfx(SFX_SELECT);
    return;
  }
  if (IsKeyPressed(KEY_SPACE) || clicked(rMenuPlay())) {
    startGame(g, true);
  } else if (clicked(rMenuShop())) {
    gotoState(g, GAMESTATE_SHOP);
    sfx(SFX_SELECT);
  } else if (IsKeyPressed(KEY_S) || clicked(rMenuSettings())) {
    gotoState(g, GAMESTATE_SETTINGS);
    sfx(SFX_SELECT);
  } else if (clicked(rMenuCredits())) {
    // Freeze the current menu onto the tube, then start the zoom-out.
    BeginTextureMode(gMenuSnap);
    drawMenu(g);
    EndTextureMode();
    g->creditsAnim = 0.0f;
    g->creditsStatic = 0.5f; // static burst plays when the zoom-out finishes
    g->creditsExiting = false;
    gotoState(g, GAMESTATE_CREDITS);
    sfx(SFX_SELECT);
  } else if (clicked(rMenuExit())) {
    g->quit = true;
  }
}

static void updateCredits(Game *g) {
  float step = 2.0f * GetFrameTime(); // ~0.5s zoom either direction
  if (g->creditsExiting) {
    g->creditsAnim = clampf(g->creditsAnim - step, 0.0f, 1.0f);
    if (g->creditsAnim <= 0.0f) {
      g->creditsExiting = false;
      returnToMenu(g); // resume the menu's attract demo
    }
    return;
  }
  g->creditsAnim = clampf(g->creditsAnim + step, 0.0f, 1.0f);
  if (g->creditsAnim >= 1.0f) { // count down the channel-change static
    g->creditsStatic = clampf(g->creditsStatic - GetFrameTime(), 0.0f, 1.0f);
  }
  // BACK only after the tube has fully zoomed out (don't interrupt the intro).
  if (g->creditsAnim >= 1.0f &&
      (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
       clicked(rCreditsBack()))) {
    g->creditsExiting = true;
    sfx(SFX_SELECT);
  }
}

static void updatePlaying(Game *g) {
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

static void updatePaused(Game *g) {
  if (IsKeyPressed(KEY_P) || clicked(rResume())) {
    gotoState(g, GAMESTATE_PLAYING);
  } else if (IsKeyPressed(KEY_S) || clicked(rPauseSettings())) {
    gotoState(g, GAMESTATE_SETTINGS);
    sfx(SFX_SELECT);
  } else if (clicked(rPauseHome())) {
    returnToMenu(g); // back to the menu's attract demo
    sfx(SFX_SELECT);
  }
}

static void updateGameover(Game *g) {
  if (IsKeyPressed(KEY_SPACE) || clicked(rRetry())) {
    startGame(g, false);
  } else if (clicked(rGameoverHome())) {
    returnToMenu(g); // back to the menu's attract demo
    sfx(SFX_SELECT);
  }
}

static void updateSettingsState(Game *g) {
  updateSettingsMenu(&g->settings, &g->state, g->prevState);
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

// Shop: spend banked coins on skins (buy, then click again to equip) and
// permanent upgrades. Opened only from the menu, so it returns straight there.
static void updateShop(Game *g) {
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
  for (int i = 0; i < UPGRADE_COUNT; i++) {
    if (!clicked(rShopUpg(i))) {
      continue;
    }
    if (upgradeOwned(p, i)) {
      sfx(SFX_SELECT); // already owned: no-op
    } else if (tryBuy(p, UPGRADES[i].cost)) {
      p->upgradeMask |= (1 << i);
      saveProgress(p);
      sfx(SFX_POWERUP);
    } else {
      sfx(SFX_HIT);
    }
  }
}

// ---------------------------------------------------------------------------
// Per-state draw handlers
// ---------------------------------------------------------------------------
static void drawMenu(Game *g) {
  drawMenuBackground();
  drawGround();           // so the attract blob stands on grass, like gameplay
  drawPlayer(&g->player); // the wandering attract blob
  int bob = (int)(sinf((float)GetTime() * 2.0f) * 10.0f);
  drawCenteredText("BOING!", SCREEN_HEIGHT / 4 + bob, 80, CANDY_PINK);
  drawCenteredText(TextFormat("BEST %d", g->highScore), SCREEN_HEIGHT / 4 + 100,
                   24, COIN_LO);
  uiButton(rMenuPlay(), "PLAY", ICON_PLAY);
  uiButton(rMenuShop(), "SHOP", ICON_SHOP);
  uiButton(rMenuSettings(), "SETTINGS", ICON_GEAR);
  uiButton(rMenuCredits(), "CREDITS", ICON_HEART);
  uiButton(rMenuExit(), "EXIT", ICON_EXIT);
  uiButton(rHelpBtn(), "?", ICON_NONE);
  if (g->showHelp) {
    drawHelpOverlay();
  }
}

// Theater-style credits reel scrolled up the tube — homage credit, not a claim
// of association. Lines starting with '-' render as pink section headers; blank
// lines are spacers, and the leading/trailing ones pad the seamless loop wrap.
static const char *REEL_LINES[] = {
    "",
    "",
    "",
    "- A GAME BY -",
    "Jihoon Lee",
    "",
    "",
    "- BUILT WITH -",
    "raylib  (raylib.com)",
    "",
    "",
    "- INSPIRED BY -",
    "Mega Jump  (Halfbrick)",
    "homage only, not affiliated",
    "",
    "",
    "- SHOWN AT -",
    "2PG Arcade 1.44MB contest",
    "",
    "",
    "- THANK YOU FOR PLAYING -",
    "",
    "",
    "",
};
#define REEL_LINE_COUNT ((int)(sizeof(REEL_LINES) / sizeof(REEL_LINES[0])))
#define REEL_LINE_H 34

// The retro CRT shell around the tube: wood body, control column with dials +
// power LED, bezel bevel, and rabbit-ear antennae. Everything fades/scales in
// with `e` (the eased zoom-out amount) so it grows out of the fullscreen
// picture.
static void drawCrtCabinet(Rectangle tube, float e) {
  Color shell = Fade((Color){74, 60, 48, 255}, e);
  Color shellHi = Fade((Color){104, 86, 68, 255}, e);
  Color metal = Fade((Color){150, 150, 160, 255}, e);
  Color knob = Fade((Color){40, 34, 30, 255}, e);

  // Symmetric side margins (s) so the shell stays centered on the tube; the
  // left margin holds a speaker grille, the right one the control column.
  float m = 30 * e, s = 130 * e;
  Rectangle body = {tube.x - s, tube.y - m, tube.width + 2 * s,
                    tube.height + 2 * m};

  // Rabbit-ear antennae poking up from the top center of the shell.
  Vector2 base = {body.x + body.width * 0.5f, body.y};
  Vector2 ltip = {base.x - 55 * e, base.y - 75 * e};
  Vector2 rtip = {base.x + 55 * e, base.y - 95 * e};
  DrawLineEx(base, ltip, 4 * e, metal);
  DrawLineEx(base, rtip, 4 * e, metal);
  DrawCircleV(ltip, 4 * e, metal);
  DrawCircleV(rtip, 4 * e, metal);

  DrawRectangleRounded(body, 0.10f, 10, shell);
  DrawRectangleRoundedLines(body, 0.10f, 10, shellHi);

  // Beveled bezel right around the glass.
  Rectangle bezel = {tube.x - 8 * e, tube.y - 8 * e, tube.width + 16 * e,
                     tube.height + 16 * e};
  DrawRectangleRoundedLines(bezel, 0.06f, 8, knob);

  // Left speaker grille, centered in the left margin.
  float gx = tube.x - s * 0.5f;
  for (int i = -2; i <= 2; i++) {
    float gy = tube.y + tube.height * 0.5f + i * 16 * e;
    DrawLineEx((Vector2){gx - 34 * e, gy}, (Vector2){gx + 34 * e, gy}, 3 * e,
               knob);
  }

  // Right control column: two dials + a power LED.
  float cx = tube.x + tube.width + s * 0.5f;
  DrawCircle((int)cx, (int)(tube.y + tube.height * 0.28f), 22 * e, knob);
  DrawCircle((int)cx, (int)(tube.y + tube.height * 0.28f), 14 * e, metal);
  DrawCircle((int)cx, (int)(tube.y + tube.height * 0.55f), 22 * e, knob);
  DrawCircle((int)cx, (int)(tube.y + tube.height * 0.55f), 14 * e, metal);
  DrawCircle((int)cx, (int)(tube.y + tube.height * 0.80f), 6 * e, Fade(RED, e));
}

// A frame of TV snow: random bright bars over near-black. Hides the hard cut
// from the frozen menu to the reel.
static void drawStatic(Rectangle r) {
  DrawRectangleRec(r, (Color){20, 20, 20, 255});
  for (int i = 0; i < 220; i++) {
    int x = (int)r.x + GetRandomValue(0, (int)r.width - 1);
    int y = (int)r.y + GetRandomValue(0, (int)r.height - 1);
    unsigned char v = (unsigned char)GetRandomValue(90, 255);
    DrawRectangle(x, y, GetRandomValue(2, 10), 2, (Color){v, v, v, 255});
  }
}

// The looping end-credits reel, centered on the tube (== SCREEN_WIDTH center).
static void drawReel(Rectangle r) {
  DrawRectangleRec(r, (Color){10, 10, 20, 255});
  float total = REEL_LINE_COUNT * REEL_LINE_H;
  float scroll = fmodf((float)GetTime() * 34.0f, total);
  for (int i = 0; i < REEL_LINE_COUNT; i++) {
    float y = r.y + r.height - scroll + i * REEL_LINE_H;
    if (y < r.y - REEL_LINE_H) {
      y += total; // wrapped past the top: reappear from the bottom
    }
    Color c = REEL_LINES[i][0] == '-' ? CANDY_PINK : RAYWHITE;
    drawCenteredText(REEL_LINES[i], (int)y, 22, c);
  }
}

static void drawCredits(Game *g) {
  float a = g->creditsAnim;
  float e = a * a * (3.0f - 2.0f * a); // smoothstep the zoom-out

  // Dark room the picture pulls back into.
  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                         (Color){18, 16, 26, 255}, (Color){6, 5, 10, 255});

  // Tube face: fullscreen at a=0, inset CRT glass at a=1. Centered on screen
  // (tube center 400,400) so the cabinet is centered and the reel text (drawn
  // at SCREEN_WIDTH center) lands centered on the glass.
  Rectangle inset = {190, 240, 420, 320};
  Rectangle tube = {inset.x * e, inset.y * e,
                    SCREEN_WIDTH + (inset.width - SCREEN_WIDTH) * e,
                    SCREEN_HEIGHT + (inset.height - SCREEN_HEIGHT) * e};

  if (e > 0.01f) {
    drawCrtCabinet(tube, e);
  }

  // Scissor is GL screen-space (the scale matrix doesn't touch it), so
  // pre-scale the 800-space tube rect to native window pixels here only.
  float s = GetScreenWidth() / (float)SCREEN_WIDTH;
  BeginScissorMode((int)(tube.x * s), (int)(tube.y * s), (int)(tube.width * s),
                   (int)(tube.height * s));
  if (a < 1.0f) {
    // Frozen menu on the tube while zooming (src y-flipped, like the main
    // blit).
    DrawTexturePro(gMenuSnap.texture,
                   (Rectangle){0, 0, SCREEN_WIDTH, -SCREEN_HEIGHT}, tube,
                   (Vector2){0, 0}, 0.0f, WHITE);
  } else if (g->creditsStatic > 0.0f) {
    drawStatic(tube);
  } else {
    drawReel(tube);
  }
  // CRT scanlines over whatever the tube is showing.
  for (int y = (int)tube.y; y < tube.y + tube.height; y += 4) {
    DrawLine((int)tube.x, y, (int)(tube.x + tube.width), y, Fade(BLACK, 0.12f));
  }
  EndScissorMode();

  if (a >= 1.0f && !g->creditsExiting) {
    uiButton(rCreditsBack(), "BACK", ICON_BACK);
  }
}

// Era names in climb order (themes loop every 4 bands). Used by the game-over
// summary; homage to a genre/era each, never a named product.
static const char *ERA_NAMES[] = {"SUNNY HILLS", "NIGHT BLOCKS", "NEON MAZE",
                                  "FALLING BLOCKS"};

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
      {"ERA", ERA_NAMES[worldLevel(w) % 4]},
  };
  int ry = 268;
  for (int i = 0; i < 4; i++) {
    drawTextShadow(rows[i].label, (int)panel.x + 30, ry, 24, RAYWHITE);
    int vw = MeasureText(rows[i].value, 24);
    drawTextShadow(rows[i].value, (int)(panel.x + panel.width - 30 - vw), ry, 24,
                   COIN_LO);
    ry += 40;
  }

  drawCenteredText(TextFormat("SCORE %d", getScore(w)), 452, 40, RAYWHITE);
  drawCenteredText(TextFormat("BEST %d", g->highScore), 500, 24, GOLD);
}

// Shared by PLAYING / PAUSED / GAMEOVER: the world, then state-specific
// overlay.
static void drawPlay(Game *g) {
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
  drawPlayer(&g->player);
  drawPowerAura(&g->world, &g->player); // rotating rings for active power-ups
  rlPopMatrix();

  // Fog debuff: dim the scene (screen space, so it's under the HUD). Fade the
  // dim out over the last ~30 frames so it lifts smoothly instead of popping.
  float fog = worldFog(&g->world);
  if (fog > 0.0f) {
    float k = fminf(fog, 30.0f) / 30.0f;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f * k));
  }

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

static void drawSettingsState(Game *g) {
  // Draw the return context underneath, then the settings overlay on top.
  if (g->prevState == GAMESTATE_MENU) {
    drawMenu(g);
  } else {
    drawPlay(g);
  }
  drawSettingsMenu(&g->settings);
}

static void drawShop(Game *g) {
  Progress *p = &g->progress;
  drawMenuBackground();
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  drawCenteredText("SHOP", 40, 60, CANDY_PINK);

  // Coin bank: a coin glyph + total, centered under the title.
  const char *bankStr = TextFormat("%d", p->coinBank);
  float bx = SCREEN_WIDTH / 2.0f - (MeasureText(bankStr, 30) + 30) / 2.0f;
  DrawEllipse(bx, 118, 11, 13, COIN_LO);
  DrawEllipse(bx, 118, 7.5f, 9, COIN_HI);
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
    const char *tag = owned ? (sel ? "EQUIPPED" : "OWNED")
                            : TextFormat("%d", SKINS[i].cost);
    drawTextShadow(tag, (int)(cx - MeasureText(tag, 16) / 2),
                   (int)(r.y + r.height - 22), 16, owned ? RAYWHITE : GOLD);
  }

  // Upgrades — one-time unlocks: name + blurb + cost/OWNED.
  drawCenteredText("UPGRADES", 300, 24, RAYWHITE);
  for (int i = 0; i < UPGRADE_COUNT; i++) {
    Rectangle r = rShopUpg(i);
    bool owned = upgradeOwned(p, i);
    DrawRectangleRounded(r, 0.3f, 8, Fade(BLACK, owned ? 0.25f : 0.4f));
    if (!owned && mouseInRect(r)) {
      DrawRectangleRoundedLines(r, 0.3f, 8, RAYWHITE);
    }
    drawTextShadow(UPGRADES[i].name, (int)r.x + 16, (int)r.y + 5, 22,
                   owned ? GOLD : RAYWHITE);
    drawTextShadow(UPGRADES[i].desc, (int)r.x + 16, (int)r.y + 28, 14,
                   Fade(RAYWHITE, 0.8f));
    const char *tag = owned ? "OWNED" : TextFormat("%d", UPGRADES[i].cost);
    drawTextShadow(tag, (int)(r.x + r.width - 20 - MeasureText(tag, 20)),
                   (int)r.y + 14, 20, owned ? RAYWHITE : GOLD);
  }

  // Missions — read-only checklist with the reward, ticked once completed.
  drawCenteredText("MISSIONS", 490, 24, RAYWHITE);
  int my = 524;
  for (int i = 0; i < MISSION_COUNT; i++) {
    bool done = missionDone(p, i);
    drawTextShadow(TextFormat("%s %s", done ? "[x]" : "[ ]", MISSIONS[i].desc),
                   150, my, 18, done ? GOLD : RAYWHITE);
    const char *rw = TextFormat("+%d", MISSIONS[i].reward);
    drawTextShadow(rw, SCREEN_WIDTH - 150 - MeasureText(rw, 18), my, 18,
                   done ? Fade(GOLD, 0.6f) : COIN_LO);
    my += 26;
  }

  uiButton(rShopBack(), "BACK", ICON_BACK);
}

// Music follows the FSM state: menu/credits/settings share the menu track,
// game over has its own, and gameplay follows the theme the draw pass resolved.
// setTrack cuts instantly across these UI boundaries and only crossfades
// between the two gameplay tracks as the theme morphs mid-climb.
static int trackForState(GameState state) {
  switch (state) {
  case GAMESTATE_MENU:
  case GAMESTATE_CREDITS:
  case GAMESTATE_SETTINGS:
  case GAMESTATE_SHOP:
    return TRACK_MENU;
  case GAMESTATE_GAMEOVER:
    return TRACK_GAMEOVER;
  default:
    return themeTrack();
  }
}

// ---------------------------------------------------------------------------
// FSM dispatch table
// ---------------------------------------------------------------------------
static const struct {
  void (*update)(Game *);
  void (*draw)(Game *);
} STATES[] = {
    [GAMESTATE_MENU] = {updateMenu, drawMenu},
    [GAMESTATE_SETTINGS] = {updateSettingsState, drawSettingsState},
    [GAMESTATE_PLAYING] = {updatePlaying, drawPlay},
    [GAMESTATE_PAUSED] = {updatePaused, drawPlay},
    [GAMESTATE_GAMEOVER] = {updateGameover, drawPlay},
    [GAMESTATE_CREDITS] = {updateCredits, drawCredits},
    [GAMESTATE_SHOP] = {updateShop, drawShop},
};
// Every state must have a handler — a NULL slot would crash the dispatch below.
STATIC_ASSERT(sizeof(STATES) / sizeof(STATES[0]) == GAMESTATE_COUNT,
              states_full);

int main(void) {
  // HighDPI: render at the display's true pixel density (crisp on Retina) and,
  // crucially, keep GetScreenWidth/mouse/scissor all in logical *points* so the
  // draw scale below is correct. Without it, macOS reports doubled pixels for a
  // resized window while the GL drawable stays point-sized — content overflows.
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boing!");
  SetExitKey(KEY_NULL); // Esc leaves the settings menu, it doesn't quit

  AudioStream stream;
  initAudio(&stream);

  themeUpdate(0.0f); // seed gTheme so createPlayer reads the nature palette

  Progress progress = loadProgress();
  setPlayerSkin(SKINS[progress.selectedSkin].character); // equip saved skin
  Game game = {
      .state = GAMESTATE_MENU,
      .prevState = GAMESTATE_MENU,
      .player = createPlayer(),
      .world = createWorld(),
      .settings = loadSettings(),
      .progress = progress,
      .highScore = loadHighScore(),
  };
  applySettings(&game.settings); // target FPS, window size, master volume

  // Draw runs in fixed 800x800 logical coords; a GPU scale matrix maps them to
  // the actual window each frame, so the resolution setting resizes the window
  // and everything rasterizes natively (no upscaled-image blur) untouched.
  gMenuSnap =
      LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT); // CRT freeze-frame
  SetTextureFilter(gMenuSnap.texture, TEXTURE_FILTER_BILINEAR);

  while (!WindowShouldClose() && !game.quit) {
    updateAudio(&stream);
    STATES[game.state].update(&game);
    setTrack(trackForState(game.state));

    // Draw 800-space, scaled to the native window (square, so width == scale).
    float scale = GetScreenWidth() / (float)SCREEN_WIDTH;
    BeginDrawing();
    ClearBackground(BLACK);
    rlPushMatrix();
    rlScalef(scale, scale, 1.0f);
    STATES[game.state].draw(&game);
    rlPopMatrix();
    EndDrawing();
  }

  UnloadRenderTexture(gMenuSnap);
  deinitAudio(&stream);
  CloseWindow();
  return 0;
}
