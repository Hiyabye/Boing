// The game loop and the FSM wiring. Every screen lives in its own screen_*.c
// and is reached only through the STATES[] dispatch table below; the shared
// context struct and transitions are in game.c/game.h.
#include "raylib.h"
#include "rlgl.h"

#include "core.h"
#include "game.h"
#include "persist.h"
#include "player.h"
#include "progress.h"
#include "settings.h"
#include "sound.h"
#include "svg.h"
#include "theme.h"
#include "world.h"

#define FADE_SPEED 3.0f // black-dip transition rate (~0.33s each direction)

// Music follows the FSM state: menu/credits/settings/shop share the menu track,
// game over has its own, and gameplay follows the theme the draw pass resolved.
// setTrack cuts instantly across these UI boundaries and only crossfades
// between the two gameplay tracks as the theme morphs mid-climb.
static int trackForState(GameState state) {
  switch (state) {
  case GAMESTATE_SPLASH:
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
    [GAMESTATE_SPLASH] = {updateSplash, drawSplash},
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
  // MSAA smooths the primitive draws that are still polygon fans (the player
  // blob, monsters, coins, backdrops, HUD bars). The vector glyphs don't need
  // it — nanosvg antialiases when it rasterizes them — but everything else has
  // hard stair-stepped diagonals without it. It is a hint: a driver that can't
  // provide it silently ignores it.
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boing!");
  SetExitKey(KEY_NULL); // Esc leaves the settings menu, it doesn't quit

  AudioStream stream;
  initAudio(&stream);

  themeUpdate(0.0f); // seed gTheme so createPlayer reads the nature palette

  configLoad("save.json"); // populate the shared map before any loader reads it
  Progress progress = loadProgress();
  setPlayerSkin(SKINS[progress.selectedSkin].character); // equip saved skin
  Game game = {
      .state = GAMESTATE_SPLASH,
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
    // Splash is a silent loading screen: don't feed the stream (no music/SFX)
    // until the menu, so audio begins with the first real screen.
    if (game.state != GAMESTATE_SPLASH) {
      updateAudio(&stream);
    }
    STATES[game.state].update(&game);

    // Black-dip transition: fall to black, swap state at the bottom, lift back.
    if (game.fadePhase == 1) {
      game.fade += FADE_SPEED * GetFrameTime();
      if (game.fade >= 1.0f) {
        game.fade = 1.0f;
        gotoState(&game, game.fadeNext);
        game.fadePhase = 2;
      }
    } else if (game.fadePhase == 2) {
      game.fade -= FADE_SPEED * GetFrameTime();
      if (game.fade <= 0.0f) {
        game.fade = 0.0f;
        game.fadePhase = 0;
      }
    }

    setTrack(trackForState(game.state));

    // Draw 800-space, scaled to the native window (square, so width == scale).
    float scale = GetScreenWidth() / (float)SCREEN_WIDTH;
    BeginDrawing();
    ClearBackground(BLACK);
    rlPushMatrix();
    rlScalef(scale, scale, 1.0f);
    STATES[game.state].draw(&game);
    if (game.fade > 0.0f) { // transition dip, over the current state's draw
      DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, game.fade));
    }
    rlPopMatrix();
    EndDrawing();
  }

  UnloadRenderTexture(gMenuSnap);
  unloadSvgCache();
  deinitAudio(&stream);
  CloseWindow();
  return 0;
}
