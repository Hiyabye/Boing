// SPLASH — one-time startup logo (gold serif "H" on white + spinning loader).
// Not cosmetic: everything in this game is generated rather than loaded from
// disk, and this screen is where that generation happens. Three queues drain
// behind the logo, one item per frame each — a music track, a vector glyph, a
// procedural texture — and the splash lasts exactly as long as the slowest of
// them takes to finish.
#include "game.h"

#include "core.h"
#include "raylib.h"
#include "sound.h"
#include "svg.h"
#include "tex.h"
#include "ui.h"

// Wordmark placement. SVG_LOGO's 200x231 viewBox is fitted to a LOGO_BOX-sided
// square, so LOGO_BOX is the viewBox's taller side and LOGO_CY the center that
// puts the "H" back where the old primitive layout had it (y 263..417).
#define LOGO_BOX 231.0f
#define LOGO_CY 378.5f

void updateSplash(Game *g) {
  // The splash lasts exactly as long as the load: bake one music track, one
  // vector glyph and one procedural texture per frame, and leave the moment all
  // three queues report drained. There is no minimum hold — anything added to a
  // preload queue extends the splash by its own cost and nothing else. Every
  // stepper runs every frame (no short-circuit) so no queue waits on another.
  bool audioReady = audioPreloadStep();
  bool artReady = svgPreloadStep();
  bool texReady = texPreloadStep();
  if (audioReady && artReady && texReady)
    startTransition(g, GAMESTATE_MENU); // dip to black, then reveal the menu
}

void drawSplash(Game *g) {
  (void)g; // preload lives in updateSplash; the logo draws the same every frame
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RAYWHITE);

  Color lo = {198, 148, 34, 255}; // deep gold; matches the wordmark's low stop
  float cx = SCREEN_WIDTH / 2.0f;

  // Wordmark: serif "H" with bracketed serifs plus the flourish, all one vector
  // glyph. Drawn solid, no fade — it must be visible from the first frame,
  // through the whole preload, which is now the splash's entire lifetime. The
  // gold gradient is baked into the SVG, so the shadow pass is the same glyph
  // tinted near-black and the logo pass tints WHITE.
  drawSvg(SVG_LOGO, cx + 7, LOGO_CY + 7, LOGO_BOX, 0.0f, (Color){0, 0, 0, 41});
  drawSvg(SVG_LOGO, cx, LOGO_CY, LOGO_BOX, 0.0f, WHITE);

  // Spinning ring loader: faint full track + a rotating 270-degree arc.
  Vector2 c = {cx, 590};
  DrawRing(c, 20, 27, 0, 360, 48, Fade(lo, 0.15f));
  float start = GetTime() * 180.0f;
  DrawRing(c, 20, 27, start, start + 270, 48, lo);

  drawCenteredText("LOADING", 636, 20, lo);
}
