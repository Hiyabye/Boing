// CREDITS — the menu freeze-frame pulls back into a retro CRT set, cuts to
// channel static, then plays a looping credits reel on the tube. Homage credit
// only, never a claim of association.
#include "game.h"

#include "core.h"
#include "raylib.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"

#include <math.h>

static Rectangle rCreditsBack(void) {
  return centeredBtn(SCREEN_HEIGHT - 110, 200, 54);
}

// Theater-style credits reel scrolled up the tube. Lines starting with '-'
// render as pink section headers; blank lines are spacers, and the
// leading/trailing ones pad the seamless loop wrap.
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

void updateCredits(Game *g) {
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

void drawCredits(Game *g) {
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
