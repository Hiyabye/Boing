#include "raylib.h"

#include "background.h"
#include "core.h"
#include "persist.h"
#include "player.h"
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
  int highScore;
  float accumulator; // fixed-timestep carry
  bool mouseControl; // true once the mouse moves; arrows reclaim control
  bool quit;         // set by the menu EXIT button to end the loop
  bool showHelp;     // menu "How to Play" overlay toggled by the ? button
  float creditsAnim;   // 0 = fullscreen picture, 1 = zoomed out to the CRT
  float creditsStatic; // "change channel" static timer, runs once zoomed out
  bool creditsExiting; // BACK pressed: ramp creditsAnim back to 0, then MENU
} Game;

#define FIXED_DT (1.0f / 60.0f)
#define MAX_FRAME_TIME 0.25f // clamp on a slow frame so physics can't spiral

// The menu, frozen the instant CREDITS is pressed, shown on the CRT tube while
// the camera pulls back. Loaded beside the main render target in main().
static RenderTexture2D gMenuSnap;
static void drawMenu(Game *g); // defined below; captured into gMenuSnap on enter

// Every state transition goes through here so `prevState` is always the screen
// we came from — that's what SETTINGS returns to, replacing a hand-threaded
// return field.
static void gotoState(Game *g, GameState next) {
  g->prevState = g->state;
  g->state = next;
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
static Rectangle rMenuPlay(void) { return centeredBtn(408, 240, 60); }
static Rectangle rMenuSettings(void) { return centeredBtn(476, 240, 54); }
static Rectangle rMenuCredits(void) { return centeredBtn(540, 240, 54); }
static Rectangle rMenuExit(void) { return centeredBtn(604, 240, 54); }
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
  return centeredBtn(SCREEN_HEIGHT * 2.0f / 3, 200, 60);
}

// Score badge + power-up timer bar, in screen space (outside BeginMode2D).
static void drawHud(const World *world) {
  const char *scoreStr = TextFormat("SCORE %d", getScore(world));
  DrawRectangleRounded((Rectangle){8, 8, MeasureText(scoreStr, 30) + 24, 40},
                       0.4f, 8, Fade(BLACK, 0.28f));
  drawTextShadow(scoreStr, 20, 13, 30, RAYWHITE);

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

  // One timer bar per active power-up, stacked (magnet + rocket can overlap).
  int bx = 12, by = 56, bw = 150;
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
  g->world = createWorld();
  if (!fromMenu) {
    g->player = createPlayer();
  }
  g->accumulator = 0.0f;
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
      g->player = createPlayer(); // resume the menu's attract demo
      gotoState(g, GAMESTATE_MENU);
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
    handlePlayerInput(&g->player, mouseX);
    updatePlayer(&g->player);
    updateWorld(&g->world, &g->player);
    if (isPlayerFallen(&g->world, &g->player)) {
      g->player.expression = EXPR_SAD; // sad face through the game-over screen
      int score = getScore(&g->world);
      if (score > g->highScore) {
        g->highScore = score;
        saveHighScore(g->highScore);
        sfx(SFX_LEVELUP); // new best: celebratory jingle
      } else {
        sfx(SFX_GAMEOVER); // ordinary death: sad tone
      }
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
    g->player = createPlayer(); // back to the menu's attract demo
    gotoState(g, GAMESTATE_MENU);
    sfx(SFX_SELECT);
  }
}

static void updateGameover(Game *g) {
  if (IsKeyPressed(KEY_SPACE) || clicked(rRetry())) {
    startGame(g, false);
  }
}

static void updateSettingsState(Game *g) {
  updateSettingsMenu(&g->settings, &g->state, g->prevState);
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
    "",  "",  "",
    "- A GAME BY -",           "Jihoon Lee",
    "",  "",
    "- BUILT WITH -",          "raylib  (raylib.com)",
    "",  "",
    "- INSPIRED BY -",         "Mega Jump  (Halfbrick)",
    "homage only, not affiliated",
    "",  "",
    "- SHOWN AT -",            "2PG Arcade 1.44MB contest",
    "",  "",
    "- THANK YOU FOR PLAYING -",
    "",  "",  "",
};
#define REEL_LINE_COUNT ((int)(sizeof(REEL_LINES) / sizeof(REEL_LINES[0])))
#define REEL_LINE_H 34

// The retro CRT shell around the tube: wood body, control column with dials +
// power LED, bezel bevel, and rabbit-ear antennae. Everything fades/scales in
// with `e` (the eased zoom-out amount) so it grows out of the fullscreen picture.
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
  // (tube center 400,400) so the cabinet is centered and the reel text (drawn at
  // SCREEN_WIDTH center) lands centered on the glass.
  Rectangle inset = {190, 240, 420, 320};
  Rectangle tube = {inset.x * e, inset.y * e,
                    SCREEN_WIDTH + (inset.width - SCREEN_WIDTH) * e,
                    SCREEN_HEIGHT + (inset.height - SCREEN_HEIGHT) * e};

  if (e > 0.01f) {
    drawCrtCabinet(tube, e);
  }

  BeginScissorMode((int)tube.x, (int)tube.y, (int)tube.width, (int)tube.height);
  if (a < 1.0f) {
    // Frozen menu on the tube while zooming (src y-flipped, like the main blit).
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
  Camera2D camera = {.target = {sx, cameraY + sy}, .zoom = 1.0f};
  BeginMode2D(camera);
  drawWorld(&g->world);
  drawPlayer(&g->player);
  EndMode2D();

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
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));
    drawCenteredText("GAME OVER", SCREEN_HEIGHT / 3, 60, RAYWHITE);
    drawCenteredText(TextFormat("SCORE %d", getScore(&g->world)),
                     SCREEN_HEIGHT / 2 - 20, 40, RAYWHITE);
    drawCenteredText(TextFormat("BEST %d", g->highScore),
                     SCREEN_HEIGHT / 2 + 25, 28, GOLD);
    uiButton(rRetry(), "RETRY", ICON_RETRY);
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
};
// Every state must have a handler — a NULL slot would crash the dispatch below.
STATIC_ASSERT(sizeof(STATES) / sizeof(STATES[0]) == GAMESTATE_COUNT, states_full);

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boing!");
  SetExitKey(KEY_NULL); // Esc leaves the settings menu, it doesn't quit

  AudioStream stream;
  initAudio(&stream);

  themeUpdate(0.0f); // seed gTheme so createPlayer reads the nature palette

  Game game = {
      .state = GAMESTATE_MENU,
      .prevState = GAMESTATE_MENU,
      .player = createPlayer(),
      .world = createWorld(),
      .settings = loadSettings(),
      .highScore = loadHighScore(),
  };
  applySettings(&game.settings); // target FPS, window size, master volume

  // Render to a fixed-size target, then blit scaled to the window so the
  // resolution setting can resize the window without touching draw code.
  RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
  gMenuSnap = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT); // CRT freeze-frame
  SetTextureFilter(gMenuSnap.texture, TEXTURE_FILTER_BILINEAR);

  while (!WindowShouldClose() && !game.quit) {
    updateAudio(&stream);
    STATES[game.state].update(&game);
    setTrack(themeTrack()); // music follows the theme the draw pass resolved

    BeginTextureMode(target);
    STATES[game.state].draw(&game);
    EndTextureMode();

    // Blit the target scaled to the actual window (src y flipped for GL).
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(
        target.texture,
        (Rectangle){0, 0, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT},
        (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
        (Vector2){0, 0}, 0.0f, WHITE);
    EndDrawing();
  }

  UnloadRenderTexture(target);
  UnloadRenderTexture(gMenuSnap);
  deinitAudio(&stream);
  CloseWindow();
  return 0;
}
