// CREDITS — the menu freeze-frame pulls back into a retro CRT set, cuts to
// channel static, then plays a looping credits reel on the tube. Homage credit
// only, never a claim of association.
#include "game.h"

#include "core.h"
#include "raylib.h"
#include "sound.h"
#include "tex.h"
#include "theme.h"
#include "ui.h"

#include <math.h>

static Rectangle rCreditsBack(void) {
  return centeredBtn(SCREEN_HEIGHT - 110, 200, 54);
}

// Theater-style credits reel scrolled up the tube. Lines starting with '-'
// use the selected UI accent and blank lines separate sections. After the
// final row, the first row follows at REEL_GAP_LINES so the reel never empties.
static const char *REEL_LINES[] = {
    "- BOING! -",
    "",
    "- CREATED BY -",
    "Jihoon Lee",
    "",
    "- ORIGINAL GAME -",
    "Code, art, music and sound",
    "created from scratch",
    "",
    "- BUILT WITH -",
    "raylib  (raylib.com)",
    "nanosvg  (Mikko Mononen)",
    "",
    "- INSPIRED BY -",
    "Mega Jump  (Halfbrick)",
    "homage only, not affiliated",
    "",
    "- MADE FOR -",
    "2PG Arcade 1.44MB contest",
    "",
    "- THANK YOU FOR PLAYING -",
};
#define REEL_LINE_COUNT ((int)(sizeof(REEL_LINES) / sizeof(REEL_LINES[0])))
#define REEL_LINE_H 34
#define REEL_GAP_LINES 3
#define REEL_SPEED 34.0f
#define REEL_CYCLE ((REEL_LINE_COUNT + REEL_GAP_LINES) * REEL_LINE_H)

static float reelRowY(Rectangle r, int row, float scroll) {
  float y = r.y + r.height - REEL_LINE_H - scroll + row * REEL_LINE_H;
  if (y < r.y - REEL_LINE_H) {
    y += REEL_CYCLE;
  }
  return y;
}

void updateCredits(Game *g) {
  float dt = GetFrameTime();
  float step = 2.0f * dt; // ~0.5s zoom either direction
  if (g->creditsExiting) {
    g->creditsAnim = clampf(g->creditsAnim - step, 0.0f, 1.0f);
    if (g->creditsAnim <= 0.0f) {
      g->creditsExiting = false;
      returnToMenu(g); // resume the menu's attract demo
    }
    return;
  }
  float oldAnim = g->creditsAnim;
  g->creditsAnim = clampf(g->creditsAnim + step, 0.0f, 1.0f);
  if (oldAnim < 1.0f && g->creditsAnim >= 1.0f) {
    sfx(SFX_STATIC); // the visual channel-change burst gets its matching sound
  }
  if (g->creditsAnim >= 1.0f) { // count down the channel-change static
    if (g->creditsStatic > 0.0f) {
      g->creditsStatic = clampf(g->creditsStatic - dt, 0.0f, 1.0f);
    } else {
      g->creditsScroll =
          fmodf(g->creditsScroll + REEL_SPEED * dt, (float)REEL_CYCLE);
    }
  }
  // BACK only after the tube has fully zoomed out (don't interrupt the intro).
  if (g->creditsAnim >= 1.0f &&
      (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
       uiItem(rCreditsBack()))) {
    g->creditsExiting = true;
    uiFocusReset();
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

// A frame of TV snow, hiding the hard cut from the frozen menu to the reel.
// One TEX_STATIC blit with a per-frame jittered source offset instead of 220
// RNG-placed bars: real per-pixel speckle, and because the texture is
// GL_REPEAT any offset at all is a valid, seamless frame of noise. The source
// rect is half the destination in texels, so one noise pixel covers a 2x2 block
// — snow that reads as snow instead of shimmering to flat grey under the post
// pass's supersampling. The sweeping roll bars are new: they are what reads as
// a *tube* rather than as noise, and the old scattered rects never did it.
static void drawStatic(Rectangle r) {
  DrawRectangleRec(r, (Color){20, 20, 20, 255});
  Texture2D t = texGet(TEX_STATIC);
  if (t.id != 0) {
    DrawTexturePro(t,
                   (Rectangle){(float)GetRandomValue(0, t.width),
                               (float)GetRandomValue(0, t.height),
                               r.width * 0.5f, r.height * 0.5f},
                   r, (Vector2){0, 0}, 0.0f, Fade(WHITE, 0.75f));
  }
  for (int i = 0; i < 3; i++) { // bright roll bars sweeping the tube
    float y = r.y + fmodf((float)GetTime() * (90.0f + i * 40.0f) + i * 200.0f,
                          r.height);
    DrawRectangle((int)r.x, (int)y, (int)r.width, 2 + i,
                  Fade(WHITE, 0.35f - i * 0.08f));
  }
}

// The looping end-credits reel, centered on the tube (== SCREEN_WIDTH center).
static void drawReel(Rectangle r, float scroll) {
  DrawRectangleRec(r, (Color){10, 10, 20, 255});
  for (int i = 0; i < REEL_LINE_COUNT; i++) {
    float y = reelRowY(r, i, scroll);
    if (y < r.y + r.height) {
      Color c = REEL_LINES[i][0] == '-' ? UI_ACCENT : RAYWHITE;
      drawCenteredText(REEL_LINES[i], (int)y, 22, c);
    }
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

  // Hand the tube to the post pass: barrel bulge, chroma split, scanlines and
  // phosphor mask, all confined to this rect and ramping in with the zoom.
  g->post.crt = e;
  g->post.crtRect = tube;

  // Scissor is GL target-space (the scale matrix doesn't touch it), so
  // pre-scale the 800-space tube rect to target pixels here only.
  float s = postScale();
  BeginScissorMode((int)(tube.x * s), (int)(tube.y * s), (int)(tube.width * s),
                   (int)(tube.height * s));
  if (a < 1.0f && IsRenderTextureValid(gMenuSnap)) {
    // Frozen menu on the tube while zooming (src y-flipped, like the main
    // blit).
    DrawTexturePro(gMenuSnap.texture,
                   (Rectangle){0, 0, SCREEN_WIDTH, -SCREEN_HEIGHT}, tube,
                   (Vector2){0, 0}, 0.0f, WHITE);
  } else if (a < 1.0f || g->creditsStatic > 0.0f) {
    drawStatic(tube); // also the fallback if the menu snapshot was unavailable
  } else {
    drawReel(tube, g->creditsScroll);
  }
  EndScissorMode();

  if (a >= 1.0f && !g->creditsExiting) {
    uiButton(rCreditsBack(), "BACK", ICON_BACK);
  }
}

#ifdef CREDITS_SELFTEST
// Pure reel-geometry check; dead stripping drops the unused screen renderer:
//   cc -DCREDITS_SELFTEST -ffunction-sections src/screen_credits.c -Iinclude \
//      -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo \
//      -framework OpenGL -Wl,-dead_strip -o /tmp/creditstest && \
//      /tmp/creditstest
#include <assert.h>

int main(void) {
  Rectangle tube = {190, 240, 420, 320};
  float first = reelRowY(tube, 0, 0.0f);
  assert(first == tube.y + tube.height - REEL_LINE_H);

  float loopFirst = reelRowY(tube, 0, REEL_CYCLE);
  float loopLast = reelRowY(tube, REEL_LINE_COUNT - 1, REEL_CYCLE);
  assert(loopFirst == first);
  assert(loopFirst - loopLast == (REEL_GAP_LINES + 1) * REEL_LINE_H);
  return 0;
}
#endif
