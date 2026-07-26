// SPLASH — one-time startup logo (gold serif "H" on white + spinning loader).
// Cosmetic: everything in this game is procedural, so there's nothing to load
// but the music, which bakes a track per frame behind the logo.
#include "game.h"

#include "core.h"
#include "raylib.h"
#include "sound.h"
#include "ui.h"

#define SPLASH_MIN 1.5f // splash holds at least this long (arcade beat), longer
                        // if tracks are still baking on a slow machine

void updateSplash(Game *g) {
  // Bake one music track per frame. Each bake is heavy (a track is millions of
  // samples), so those frames' GetFrameTime is huge — don't count them toward
  // the arcade beat or it'd be eaten by the load and the splash would blink by.
  // Freeze the timer until fully loaded; only smooth post-load frames count.
  if (!audioPreloadStep())
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

  // Solid, no fade: the logo must be visible from the first frame — through the
  // whole preload (which freezes splashT), not just the post-load arcade beat.
  float a = 1.0f;
  Color hi = {255, 228, 140, 255}; // light gold (top)
  Color lo = {198, 148, 34, 255};  // deep gold (bottom)
  float cx = SCREEN_WIDTH / 2.0f, cy = 340.0f;

  // Serif "H" as primitive rects: 2 columns + crossbar + 4 serif caps. All
  // offsets scale by k about (cx,cy), so the glyph stays centered as it shrinks.
  float k = 0.7f;
  Rectangle parts[] = {
      {cx - 90 * k, cy - 110 * k, 40 * k, 220 * k}, // left column
      {cx + 50 * k, cy - 110 * k, 40 * k, 220 * k}, // right column
      {cx - 50 * k, cy - 20 * k, 100 * k, 40 * k},  // crossbar
      {cx - 105 * k, cy - 110 * k, 70 * k, 16 * k}, // serif: top-left
      {cx - 105 * k, cy + 94 * k, 70 * k, 16 * k},  // serif: bottom-left
      {cx + 35 * k, cy - 110 * k, 70 * k, 16 * k},  // serif: top-right
      {cx + 35 * k, cy + 94 * k, 70 * k, 16 * k},   // serif: bottom-right
  };
  int n = sizeof(parts) / sizeof(parts[0]);

  for (int i = 0; i < n; i++) { // soft drop shadow
    Rectangle s = parts[i];
    s.x += 7;
    s.y += 7;
    DrawRectangleRec(s, Fade(BLACK, 0.16f * a));
  }
  for (int i = 0; i < n; i++) // gold gradient fill
    DrawRectangleGradientV(parts[i].x, parts[i].y, parts[i].width,
                           parts[i].height, Fade(hi, a), Fade(lo, a));

  // Thin flourish underline with end dots.
  DrawLineEx((Vector2){cx - 100, cy + 150}, (Vector2){cx + 100, cy + 150}, 3,
             Fade(lo, a));
  DrawCircleV((Vector2){cx - 100, cy + 150}, 4, Fade(lo, a));
  DrawCircleV((Vector2){cx + 100, cy + 150}, 4, Fade(lo, a));

  // Spinning ring loader: faint full track + a rotating 270-degree arc.
  Vector2 c = {cx, 590};
  DrawRing(c, 20, 27, 0, 360, 48, Fade(lo, 0.15f * a));
  float start = GetTime() * 180.0f;
  DrawRing(c, 20, 27, start, start + 270, 48, Fade(lo, a));

  drawCenteredText("LOADING", 636, 20, Fade(lo, a));
}
