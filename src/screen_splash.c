// SPLASH — one-time startup logo (gold serif "H" on white + spinning loader).
// Cosmetic: everything in this game is procedural, so there's nothing to load
// but the music, which bakes a track per frame behind the logo.
#include "game.h"

#include "core.h"
#include "raylib.h"
#include "sound.h"
#include "svg.h"
#include "ui.h"

#define SPLASH_MIN                                                             \
  1.5f // splash holds at least this long (arcade beat), longer
       // if tracks are still baking on a slow machine

// Wordmark placement. SVG_LOGO's 200x231 viewBox is fitted to a LOGO_BOX-sided
// square, so LOGO_BOX is the viewBox's taller side and LOGO_CY the center that
// puts the "H" back where the old primitive layout had it (y 263..417).
#define LOGO_BOX 231.0f
#define LOGO_CY 378.5f

void updateSplash(Game *g) {
  // Bake one music track and rasterize one vector glyph per frame. Each bake is
  // heavy (a track is millions of samples), so those frames' GetFrameTime is
  // huge — don't count them toward the arcade beat or it'd be eaten by the load
  // and the splash would blink by. Freeze the timer until fully loaded; only
  // smooth post-load frames count. Both steppers run every frame (no
  // short-circuit) so neither queue waits on the other to drain.
  bool audioReady = audioPreloadStep();
  bool artReady = svgPreloadStep();
  if (!audioReady || !artReady)
    return;
  g->splashT += GetFrameTime();
  // Loaded: hold the arcade-beat minimum, or let an impatient tap skip it.
  if (g->splashT >= SPLASH_MIN || GetKeyPressed() ||
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    startTransition(g, GAMESTATE_MENU); // dip to black, then reveal the menu
}

void drawSplash(Game *g) {
  (void)g; // timer lives in updateSplash; the logo draws the same every frame
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RAYWHITE);

  Color lo = {198, 148, 34, 255}; // deep gold; matches the wordmark's low stop
  float cx = SCREEN_WIDTH / 2.0f;

  // Wordmark: serif "H" with bracketed serifs plus the flourish, all one vector
  // glyph. Drawn solid, no fade — it must be visible from the first frame,
  // through the whole preload (which freezes splashT), not just the post-load
  // arcade beat. The gold gradient is baked into the SVG, so the shadow pass is
  // the same glyph tinted near-black and the logo pass tints WHITE.
  drawSvg(SVG_LOGO, cx + 7, LOGO_CY + 7, LOGO_BOX, 0.0f, (Color){0, 0, 0, 41});
  drawSvg(SVG_LOGO, cx, LOGO_CY, LOGO_BOX, 0.0f, WHITE);

  // Spinning ring loader: faint full track + a rotating 270-degree arc.
  Vector2 c = {cx, 590};
  DrawRing(c, 20, 27, 0, 360, 48, Fade(lo, 0.15f));
  float start = GetTime() * 180.0f;
  DrawRing(c, 20, 27, start, start + 270, 48, lo);

  drawCenteredText("LOADING", 636, 20, lo);
}
