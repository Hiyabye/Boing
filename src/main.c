#include "raylib.h"

#include "background.h"
#include "core.h"
#include "player.h"
#include "settings.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Game context — the whole mutable state, threaded through every FSM handler.
// ---------------------------------------------------------------------------
typedef struct {
  GameState state;
  GameState settingsReturn; // where leaving the settings screen goes back to
  Player player;
  World world;
  Settings settings;
  int highScore;
  float accumulator; // fixed-timestep carry
  bool mouseControl; // true once the mouse moves; arrows reclaim control
  bool quit;         // set by the menu EXIT button to end the loop
} Game;

#define FIXED_DT (1.0f / 60.0f)

// Button rects, in virtual (800x800) space. Convention: each button is one rect
// helper here, referenced by uiButton(rect,...) in a draw() handler and by
// clicked(rect) in the matching update() handler — declare once, use in both.
static Rectangle rPauseBtn(void) { return (Rectangle){SCREEN_WIDTH - 54, 10, 44, 44}; }
static Rectangle rMenuPlay(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 120, 424, 240, 62}; }
static Rectangle rMenuSettings(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 120, 498, 240, 54}; }
static Rectangle rMenuExit(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 120, 564, 240, 54}; }
static Rectangle rResume(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 172, SCREEN_HEIGHT / 2.0f + 40, 160, 56}; }
static Rectangle rPauseSettings(void) { return (Rectangle){SCREEN_WIDTH / 2.0f + 12, SCREEN_HEIGHT / 2.0f + 40, 160, 56}; }
static Rectangle rPauseHome(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 80, SCREEN_HEIGHT / 2.0f + 108, 160, 56}; }
static Rectangle rRetry(void) { return (Rectangle){SCREEN_WIDTH / 2.0f - 100, SCREEN_HEIGHT * 2.0f / 3, 200, 60}; }

// clicked(), drawTextShadow(), drawCenteredText() now live in the ui module.

// Score badge + power-up timer bar, in screen space (outside BeginMode2D).
static void drawHud(const World *world) {
  const char *scoreStr = TextFormat("SCORE %d", getScore(world));
  DrawRectangleRounded((Rectangle){8, 8, MeasureText(scoreStr, 30) + 24, 40},
                       0.4f, 8, Fade(BLACK, 0.28f));
  drawTextShadow(scoreStr, 20, 13, 30, RAYWHITE);

  if (world->rocketTimer > 0.0f || world->magnetTimer > 0.0f) {
    bool rocket = world->rocketTimer > 0.0f;
    const char *label = rocket ? "ROCKET" : "MAGNET";
    Color col = rocket ? ROCKET_FIN : SKYBLUE;
    float frac = rocket ? world->rocketTimer / ROCKET_DURATION
                        : world->magnetTimer / MAGNET_DURATION;
    int bx = 12, by = 56, bw = 150;
    DrawRectangleRounded((Rectangle){bx, by, bw, 26}, 0.5f, 8, Fade(BLACK, 0.3f));
    DrawRectangleRounded((Rectangle){bx, by, bw * frac, 26}, 0.5f, 8,
                         Fade(col, 0.85f));
    drawTextShadow(label, bx + 12, by + 4, 18, RAYWHITE);
  }
}

// ---------------------------------------------------------------------------
// High score persistence (plain text file beside the exe)
// ---------------------------------------------------------------------------
static int loadHighScore(void) {
  int best = 0;
  FILE *f = fopen("highscore", "r");
  if (f) {
    if (fscanf(f, "%d", &best) != 1) {
      best = 0;
    }
    fclose(f);
  }
  return best;
}

static void saveHighScore(int best) {
  FILE *f = fopen("highscore", "w");
  if (f) {
    fprintf(f, "%d\n", best);
    fclose(f);
  }
}

// ---------------------------------------------------------------------------
// Transitions
// ---------------------------------------------------------------------------
// Enter gameplay. fromMenu: keep the wandering attract blob (position + velocity
// carry over, gravity already lives in acceleration.y) for a seamless handoff;
// otherwise (retry) respawn a fresh player.
static void startGame(Game *g, bool fromMenu) {
  g->world = createWorld();
  if (!fromMenu) {
    g->player = createPlayer();
  }
  g->accumulator = 0.0f;
  g->state = GAMESTATE_PLAYING;
  sfxSelect();
}

// ---------------------------------------------------------------------------
// Per-state update handlers
// ---------------------------------------------------------------------------
static void updateMenu(Game *g) {
  updateAttract(&g->player); // demo: blob wanders the menu
  if (IsKeyPressed(KEY_SPACE) || clicked(rMenuPlay())) {
    startGame(g, true);
  } else if (IsKeyPressed(KEY_S) || clicked(rMenuSettings())) {
    g->settingsReturn = GAMESTATE_MENU;
    g->state = GAMESTATE_SETTINGS;
    sfxSelect();
  } else if (clicked(rMenuExit())) {
    g->quit = true;
  }
}

static void updatePlaying(Game *g) {
  if (IsKeyPressed(KEY_P) || clicked(rPauseBtn())) {
    g->state = GAMESTATE_PAUSED;
    return;
  }
  // Fixed 60Hz steps to catch up to real time; cap to avoid a spiral of death.
  g->accumulator += GetFrameTime();
  if (g->accumulator > 0.25f) {
    g->accumulator = 0.25f;
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
    if (isPlayerFallen(&g->world, &g->player) || g->world.dead) {
      int score = getScore(&g->world);
      if (score > g->highScore) {
        g->highScore = score;
        saveHighScore(g->highScore);
      }
      g->state = GAMESTATE_GAMEOVER;
      return;
    }
  }
}

static void updatePaused(Game *g) {
  if (IsKeyPressed(KEY_P) || clicked(rResume())) {
    g->state = GAMESTATE_PLAYING;
  } else if (IsKeyPressed(KEY_S) || clicked(rPauseSettings())) {
    g->settingsReturn = GAMESTATE_PAUSED;
    g->state = GAMESTATE_SETTINGS;
    sfxSelect();
  } else if (clicked(rPauseHome())) {
    g->player = createPlayer(); // back to the menu's attract demo
    g->state = GAMESTATE_MENU;
    sfxSelect();
  }
}

static void updateGameover(Game *g) {
  if (IsKeyPressed(KEY_SPACE) || clicked(rRetry())) {
    startGame(g, false);
  }
}

static void updateSettingsState(Game *g) {
  updateSettingsMenu(&g->settings, &g->state, g->settingsReturn);
}

// ---------------------------------------------------------------------------
// Per-state draw handlers
// ---------------------------------------------------------------------------
static void drawMenu(Game *g) {
  drawMenuBackground();
  drawGround(); // so the attract blob stands on grass, like gameplay
  drawPlayer(&g->player); // the wandering attract blob
  int bob = (int)(sinf((float)GetTime() * 2.0f) * 10.0f);
  drawCenteredText("MEGA JUMP", SCREEN_HEIGHT / 4 + bob, 80, CANDY_PINK);
  drawCenteredText(TextFormat("BEST %d", g->highScore), SCREEN_HEIGHT / 4 + 100,
                   24, COIN_LO);
  uiButton(rMenuPlay(), "PLAY", ICON_PLAY);
  uiButton(rMenuSettings(), "SETTINGS", ICON_GEAR);
  uiButton(rMenuExit(), "EXIT", ICON_EXIT);
}

// Shared by PLAYING / PAUSED / GAMEOVER: the world, then state-specific overlay.
static void drawPlay(Game *g) {
  drawGameBackground(g->world.cameraY);
  float sx = 0.0f, sy = 0.0f;
  if (g->world.shake > 0.0f) {
    sx = GetRandomValue(-100, 100) / 100.0f * g->world.shake;
    sy = GetRandomValue(-100, 100) / 100.0f * g->world.shake;
  }
  Camera2D camera = {.target = {sx, g->world.cameraY + sy}, .zoom = 1.0f};
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
    drawCenteredText(TextFormat("BEST %d", g->highScore), SCREEN_HEIGHT / 2 + 25,
                     28, GOLD);
    uiButton(rRetry(), "RETRY", ICON_RETRY);
  }
}

static void drawSettingsState(Game *g) {
  // Draw the return context underneath, then the settings overlay on top.
  if (g->settingsReturn == GAMESTATE_MENU) {
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
};

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Mega Jump");
  SetExitKey(KEY_NULL); // Esc leaves the settings menu, it doesn't quit

  AudioStream stream;
  initAudio(&stream);

  Game game = {
      .state = GAMESTATE_MENU,
      .settingsReturn = GAMESTATE_MENU,
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

  while (!WindowShouldClose() && !game.quit) {
    updateAudio(&stream);
    STATES[game.state].update(&game);

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
  deinitAudio(&stream);
  CloseWindow();
  return 0;
}
