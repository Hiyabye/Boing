// The game loop and the FSM wiring. Every screen lives in its own screen_*.c
// and is reached only through the STATES[] dispatch table below; the shared
// context struct and transitions are in game.c/game.h.
#include "raylib.h"
#include "rlgl.h"

#include "core.h"
#include "font.h"
#include "game.h"
#include "persist.h"
#include "player.h"
#include "post.h"
#include "progress.h"
#include "settings.h"
#include "sound.h"
#include "svg.h"
#include "tex.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#define FADE_SPEED 3.0f // black-dip transition rate (~0.33s each direction)

// Full cues belong to durable contexts. Lightweight overlays keep their
// underlying score, MENU/SHOP preserve one shared playhead, and pause filters
// the live era instead of resetting it.
static int trackForState(const Game *g) {
  switch (g->state) {
  case GAMESTATE_SPLASH:
    return TRACK_NONE;
  case GAMESTATE_MENU:
    return TRACK_MENU;
  case GAMESTATE_SHOP:
    return TRACK_SHOP;
  case GAMESTATE_CREDITS:
    return TRACK_CREDITS;
  case GAMESTATE_GAMEOVER:
    return TRACK_RESULTS;
  case GAMESTATE_SETTINGS:
    return g->prevState == GAMESTATE_PAUSED ? THEMES[worldEra(&g->world)].track
                                            : TRACK_MENU;
  default:
    return THEMES[worldEra(&g->world)].track;
  }
}

static void updateMusicLayers(const Game *g) {
  if (g->state != GAMESTATE_PLAYING) {
    setMusicDynamics(0, 0.0f, 0.0f);
    return;
  }

  float screenY = g->player.position.y - g->world.cameraY;
  float fallSpeed = clampf((g->player.velocity.y - 2.0f) / 18.0f, 0.0f, 1.0f);
  float depth = clampf(
      (screenY - SCREEN_HEIGHT * 0.5f) / (SCREEN_HEIGHT * 0.35f), 0.0f, 1.0f);
  // Match updateWorld's peak reference: the camera starts climbing once the
  // player's best point rises above screen center.
  float ascent = SCREEN_HEIGHT / 2.0f - g->world.bestY;
  float finale = worldEra(&g->world) == THEME_COUNT - 1 &&
                         themeBandProgressAt(ascent) >= 0.75f
                     ? 1.0f
                     : 0.0f;
  setMusicDynamics(g->world.combo, fallSpeed * depth, finale);
}

// ---------------------------------------------------------------------------
// FSM dispatch table
// ---------------------------------------------------------------------------
static const struct {
  void (*update)(Game *);
  void (*draw)(Game *);
} STATES[] = {
    {updateSplash, drawSplash},
    {updateMenu, drawMenu},
    {updateSettingsState, drawSettingsState},
    {updatePlaying, drawPlay},
    {updatePaused, drawPlay},
    {updateGameover, drawPlay},
    {updateCredits, drawCredits},
    {updateShop, drawShop},
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
  // blob, monsters, coins, backdrops, HUD bars). It is no longer what does that
  // work — the frame now resolves through post's supersampled offscreen target,
  // which a multisampled *backbuffer* can't help — but it is what antialiases
  // the fallback path when that target or its shader is unavailable. It is a
  // hint: a driver that can't provide it silently ignores it.
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boing!");
  SetExitKey(KEY_NULL); // Esc leaves the settings menu, it doesn't quit
  HideCursor();         // ui.c's drawCursor() replaces the OS arrow

  AudioStream stream;
  initAudio(&stream);

  configLoadUser(); // populate the shared map before any loader reads it
  Progress progress = loadProgress();
  setPlayerCharacter(progressCharacter(&progress)); // saved outfit + UI accent
  Game game = {
      .state = GAMESTATE_SPLASH,
      .prevState = GAMESTATE_MENU,
      .player = createPlayer(),
      .world = createWorld(),
      .settings = loadSettings(),
      .progress = progress,
      .highScore = loadHighScore(),
  };
  applySettings(&game.settings); // target FPS, window size, audio levels
  // raylib does not seed its mouse position or on-screen flag until the first
  // cursor event. Put the hidden OS pointer at a valid initial hotspot so the
  // custom cursor is visible immediately, after settings has resized the
  // window.
  SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);

  // Draw runs in fixed 800x800 logical coords; a GPU scale matrix maps them to
  // post's offscreen target each frame, so the resolution setting resizes the
  // window and everything rasterizes natively (no upscaled-image blur).
  gMenuSnap =
      LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT); // CRT freeze-frame
  if (IsRenderTextureValid(gMenuSnap)) {
    SetTextureFilter(gMenuSnap.texture, TEXTURE_FILTER_BILINEAR);
  } else {
    UnloadRenderTexture(gMenuSnap); // also releases a partially-created target
    gMenuSnap = (RenderTexture2D){0};
  }

  while (!WindowShouldClose() && !game.quit) {
    STATES[game.state].update(&game);
    // The screen's update() just re-registered its focusable rects via uiItem;
    // move the ring now so this frame's draw shows where it landed.
    uiFocusStep();

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

    // Route music after gameplay/UI updates so state changes and one-shot
    // events reach the next stream buffer with no extra frame of latency.
    setTrack(trackForState(&game));
    bool pausedScore =
        game.state == GAMESTATE_PAUSED || (game.state == GAMESTATE_SETTINGS &&
                                           game.prevState == GAMESTATE_PAUSED);
    setMusicPaused(pausedScore);
    updateMusicLayers(&game);
    updateAudio(&stream);

    // Effects are opt-in per frame: the drawing screen raises what it wants
    // (bloom, fog, the credits CRT) into this mailbox, and postEnd consumes it.
    // The vignette is the one always-on effect, so it starts raised here.
    game.post = (PostFx){.vignette = 0.28f};

    // Draw 800-space, scaled to the post target (square, so width == scale).
    BeginDrawing();
    postBegin();
    ClearBackground(BLACK);
    float scale = postScale();
    rlPushMatrix();
    rlScalef(scale, scale, 1.0f);
    STATES[game.state].draw(&game);
    uiFocusDraw(); // on top of the screen's own overlays, still in 800-space
    if (game.fade > 0.0f) { // transition dip, over the current state's draw
      DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, game.fade));
    }
    rlPopMatrix();
    postEnd(&game.post); // resolve the offscreen target back to the window
    // Pointer last, outside the post pass: the CRT barrel would otherwise slide
    // it off the real cursor (virtualMouse has no inverse warp) and Fog would
    // blur it.
    drawCursor();
    EndDrawing();
  }

  if (IsRenderTextureValid(gMenuSnap)) {
    UnloadRenderTexture(gMenuSnap);
  }
  unloadPost();
  unloadSvgCache();
  unloadTexCache();
  unloadGameFont();
  deinitAudio(&stream);
  CloseWindow();
  return 0;
}
