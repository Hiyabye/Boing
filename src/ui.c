#include "ui.h"

#include "core.h"
#include "font.h"
#include "svg.h"
#include "tex.h"
#include "theme.h"

#include <math.h>
#include <stddef.h>

// Rounded-rect drawing shares these: corner roundness, segment count, and the
// 3D "lip" (the button face sits this many px above its drop shadow).
#define BTN_ROUNDNESS 0.4f
#define ROUND_SEGMENTS 8
#define BTN_LIP 4.0f
#define LABEL_PAD 12.0f // horizontal breathing room for a button's text
#define ICON_SCALE 1.8f // drawIcon's half-extent `s` -> drawSvg's box side

// Keyboard focus ring + its hint line. HINT_Y clears the lowest button on any
// screen (the shop's BACK bottoms out at 760).
#define FOCUS_PAD 6.0f
#define FOCUS_THICK 3.0f
#define HINT_Y 776
#define HINT_FONT 16
#define CURSOR_PX 30.0f // pointer box side, in window points at 800 wide
#define CURSOR_TRAIL 6
#define CURSOR_CLICK_TIME 0.38f

Vector2 virtualMouse(void) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  return (Vector2){GetMouseX() * (float)SCREEN_WIDTH / (sw ? sw : 1),
                   GetMouseY() * (float)SCREEN_HEIGHT / (sh ? sh : 1)};
}

bool mouseInRect(Rectangle r) {
  return CheckCollisionPointRec(virtualMouse(), r);
}

bool clicked(Rectangle r) {
  return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouseInRect(r);
}

// ---------------------------------------------------------------------------
// Keyboard focus
// ---------------------------------------------------------------------------
// The current screen's focusable rects, rebuilt every frame by its update().
// 32 covers the busiest screen (shop: 4 outfit categories + 12 axes + back).
#define UI_MAX_ITEMS 32
static Rectangle gItems[UI_MAX_ITEMS];
static int gCount;
static int gFocus;

bool uiItem(Rectangle r) {
  int idx = gCount;
  if (gCount < UI_MAX_ITEMS) {
    gItems[gCount++] = r;
  }
  // Enter tests against the focus set by *last* frame's uiFocusStep — which is
  // the ring the player was looking at when they pressed it.
  bool key = (idx == gFocus) &&
             (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
  return clicked(r) || key;
}

// Nearest registered item in direction (dx,dy) from the focused one, or -1.
//
// ponytail: nearest-centre heuristic, no wrap-around at the list edges. Cheap
// and layout-agnostic (it handles the menu column and the shop grid with the
// same code); swap in an explicit nav graph only if a screen stops being a
// grid.
static int nearestIn(float dx, float dy) {
  Rectangle f = gItems[gFocus];
  float fx = f.x + f.width * 0.5f, fy = f.y + f.height * 0.5f;
  int best = -1;
  float bestD = 0.0f;

  for (int i = 0; i < gCount; i++) {
    if (i == gFocus) {
      continue;
    }
    float tx = gItems[i].x + gItems[i].width * 0.5f - fx;
    float ty = gItems[i].y + gItems[i].height * 0.5f - fy;
    float along = tx * dx + ty * dy; // distance along the pressed direction
    if (along <= 0.0f) {
      continue; // behind us, or dead sideways
    }
    float perp = tx * dy - ty * dx;
    if (perp < 0.0f) {
      perp = -perp;
    }
    float d = along + perp * 2.0f; // weighted: prefer straight ahead
    if (best < 0 || d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

void uiFocusStep(void) {
  if (gCount == 0) {
    return; // this screen drives the keyboard itself (settings, gameplay)
  }
  if (gFocus >= gCount) {
    gFocus = 0; // the screen shrank its list (an overlay opened)
  }
  float dx = (float)(IsKeyPressed(KEY_RIGHT) - IsKeyPressed(KEY_LEFT));
  float dy = (float)(IsKeyPressed(KEY_DOWN) - IsKeyPressed(KEY_UP));
  if (dx != 0.0f || dy != 0.0f) {
    int next = nearestIn(dx, dy);
    if (next >= 0) {
      gFocus = next;
    }
  }
}

void uiFocusReset(void) {
  gFocus = 0;
  gCount = 0;
}

void uiFocusDraw(void) {
  // Clearing happens *here*, not in uiFocusStep: the loop order is
  // update -> uiFocusStep -> draw -> uiFocusDraw, so the list has to survive
  // until it has been drawn.
  int count = gCount;
  gCount = 0; // the next frame's update() rebuilds the list

  if (count == 0 || gFocus >= count) {
    return;
  }
  Rectangle r = gItems[gFocus];
  // Pulse so the ring reads as "here", not as another button border.
  float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 4.0f);
  Rectangle ring = {r.x - FOCUS_PAD, r.y - FOCUS_PAD, r.width + 2 * FOCUS_PAD,
                    r.height + 2 * FOCUS_PAD};
  DrawRectangleRoundedLinesEx(ring, BTN_ROUNDNESS, ROUND_SEGMENTS, FOCUS_THICK,
                              Fade(RAYWHITE, 0.45f + 0.45f * pulse));

  drawCenteredText("ARROWS MOVE    ENTER SELECT    ESC BACK", HINT_Y, HINT_FONT,
                   Fade(RAYWHITE, 0.6f));
}

// ---------------------------------------------------------------------------
// Custom mouse pointer
// ---------------------------------------------------------------------------
static Vector2 gCursorTrail[CURSOR_TRAIL];
static Vector2 gClickPos;
static double gClickAt = -1.0;
static bool gCursorReady;

static bool cursorOutside(bool onScreen, Vector2 m) {
  return !onScreen && m.x == 0.0f && m.y == 0.0f;
}

static void cursorTrailStep(Vector2 m, float dt) {
  if (!gCursorReady) {
    for (int i = 0; i < CURSOR_TRAIL; i++) {
      gCursorTrail[i] = m;
    }
    gCursorReady = true;
  }
  gCursorTrail[0] = m;
  float follow = 1.0f - expf(-24.0f * dt);
  for (int i = 1; i < CURSOR_TRAIL; i++) {
    gCursorTrail[i].x += (gCursorTrail[i - 1].x - gCursorTrail[i].x) * follow;
    gCursorTrail[i].y += (gCursorTrail[i - 1].y - gCursorTrail[i].y) * follow;
  }
}

void drawCursor(void) {
  Vector2 m = GetMousePosition();
  // GLFW leaves cursorOnScreen false at startup until an enter event, even
  // though main has seeded a valid position. On a real exit raylib also resets
  // the position to zero, which lets us distinguish it without hiding frame 1.
  if (cursorOutside(IsCursorOnScreen(), m)) {
    gCursorReady = false; // do not streak across the window on re-entry
    return;
  }
  cursorTrailStep(m, GetFrameTime());

  double now = GetTime();
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    gClickAt = now;
    gClickPos = m;
  }

  // Window points, not 800-space: this draws after postEnd, outside the frame's
  // scale matrix. Sizing off GetScreenWidth keeps it constant on screen at
  // every resolution option.
  float s = CURSOR_PX * (float)GetScreenWidth() / (float)SCREEN_WIDTH;
  bool down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  float clickAge = (float)((now - gClickAt) / CURSOR_CLICK_TIME);

  BeginBlendMode(BLEND_ADDITIVE);
  if (clickAge >= 0.0f && clickAge < 1.0f) {
    float fade = 1.0f - clickAge;
    float ease = 1.0f - fade * fade * fade;
    float r = s * (0.25f + 1.35f * ease);
    DrawRing(gClickPos, r - s * 0.08f, r, 0.0f, 360.0f, 32,
             Fade((Color){255, 215, 92, 255}, fade * 0.9f));
    for (int i = 0; i < 6; i++) {
      float a = i * PI / 3.0f + 0.25f;
      Vector2 d = {cosf(a), sinf(a)};
      DrawLineEx((Vector2){gClickPos.x + d.x * r * 0.58f,
                           gClickPos.y + d.y * r * 0.58f},
                 (Vector2){gClickPos.x + d.x * r, gClickPos.y + d.y * r},
                 fmaxf(1.0f, s * 0.055f * fade),
                 Fade((Color){255, 102, 177, 255}, fade * 0.75f));
    }
  }

  // Oldest first, then a broad light under the pointer body. Once movement
  // stops the points converge into a quiet halo instead of leaving a tail.
  for (int i = CURSOR_TRAIL - 1; i > 0; i--) {
    float near = 1.0f - i / (float)CURSOR_TRAIL;
    Vector2 p = {gCursorTrail[i].x + s * 0.34f, gCursorTrail[i].y + s * 0.38f};
    Color c = (i & 1) ? (Color){255, 94, 172, 255} : (Color){255, 213, 86, 255};
    drawTex(TEX_PUFF, p.x, p.y, s * (0.40f + near * 0.45f),
            s * (0.40f + near * 0.45f), 0.0f, Fade(c, 0.08f + near * 0.13f));
  }
  drawTex(TEX_PUFF, m.x + s * 0.38f, m.y + s * 0.42f, s * 1.35f, s * 1.35f,
          0.0f, Fade((Color){255, 76, 164, 255}, 0.34f));
  if (down) {
    drawTex(TEX_PUFF, m.x, m.y, s * 0.85f, s * 0.85f, 0.0f,
            Fade((Color){255, 222, 103, 255}, 0.55f));
  }
  EndBlendMode();

  // Two crisp candy flecks keep the trail readable over bright backgrounds,
  // where additive light alone fades into the sky.
  for (int i = 2; i < CURSOR_TRAIL; i += 2) {
    float near = 1.0f - i / (float)CURSOR_TRAIL;
    Color c = (i & 2) ? (Color){255, 73, 157, 255} : (Color){238, 177, 45, 255};
    DrawCircleV(gCursorTrail[i], fmaxf(1.0f, s * 0.045f * near),
                Fade(c, 0.35f + near * 0.4f));
  }

  if (down) {
    s *= 0.86f; // click compression
  }
  // The glyph's tip is at its viewBox origin, i.e. the top-left of the box, so
  // centering it half a box down-right puts the tip exactly on the pointer.
  drawSvg(SVG_CURSOR, m.x + s * 0.5f, m.y + s * 0.5f, s, 0.0f, WHITE);
}

// Letterspacing lives in each glyph's own advance, so raylib's spacing term
// ((n-1)*spacing, added by both DrawTextEx and MeasureTextEx) stays zero and
// measuring matches drawing exactly.
#define TEXT_SPACING 0.0f

void uiText(const char *text, float x, float y, int size, Color color) {
  DrawTextEx(gameFont(), text, (Vector2){x, y}, (float)size, TEXT_SPACING,
             color);
}

int uiTextWidth(const char *text, int size) {
  return (int)MeasureTextEx(gameFont(), text, (float)size, TEXT_SPACING).x;
}

void drawTextShadow(const char *text, int x, int y, int size, Color color) {
  uiText(text, x + 2.0f, y + 2.0f, size, Fade(BLACK, 0.35f));
  uiText(text, (float)x, (float)y, size, color);
}

void drawCenteredText(const char *text, int y, int fontSize, Color color) {
  int x = (SCREEN_WIDTH - uiTextWidth(text, fontSize)) / 2;
  drawTextShadow(text, x, y, fontSize, color);
}

// IconKind -> vector glyph. ICON_NONE has no row: it is the enum's first value,
// so the table is indexed by `kind - 1` and drawing it is a no-op.
static const SvgId ICON_SVG[] = {
    SVG_PLAY, SVG_PAUSE, SVG_GEAR,  SVG_RETRY, SVG_BACK,
    SVG_HOME, SVG_EXIT,  SVG_HEART, SVG_SHOP,  SVG_MISSIONS,
};
// Positional rows make a missing icon shorten the table and fail the build.
STATIC_ASSERT(sizeof(ICON_SVG) / sizeof(ICON_SVG[0]) == ICON_COUNT - 1,
              icon_svg_table_complete);

void drawIcon(IconKind kind, float cx, float cy, float s, Color c) {
  if (kind == ICON_NONE) {
    return;
  }
  // Callers size icons by half-extent (the old primitives spanned ~1.2*s tall),
  // so the box is 2*s wide; ICON_SCALE trims it to the glyphs' 24x24 viewBox,
  // which carries its own margin.
  drawSvg(ICON_SVG[kind - 1], cx, cy, s * ICON_SCALE, 0.0f, c);
}

// Renders the button; activation is detected by the caller via uiItem(r).
void uiButton(Rectangle r, const char *label, IconKind icon) {
  bool hover = mouseInRect(r);
  Color base = hover ? CANDY_PINK : CANDY_PINK_DK;
  DrawRectangleRounded(r, BTN_ROUNDNESS, ROUND_SEGMENTS, Fade(BLACK, 0.25f));
  DrawRectangleRounded((Rectangle){r.x, r.y, r.width, r.height - BTN_LIP},
                       BTN_ROUNDNESS, ROUND_SEGMENTS, base);

  float cx = r.x + r.width / 2.0f, cy = r.y + r.height / 2.0f;
  if (icon != ICON_NONE && (label == NULL || label[0] == '\0')) {
    drawIcon(icon, cx, cy, r.height * 0.5f, RAYWHITE);
  } else {
    // Slim icon column + a font that shrinks to fit so labels never overflow.
    float iconCol = (icon != ICON_NONE) ? r.height * 0.72f : 0.0f;
    float avail = r.width - iconCol - LABEL_PAD;
    int fs = (int)(r.height * 0.42f);
    while (fs > 8 && label && uiTextWidth(label, fs) > avail) {
      fs--;
    }
    if (icon != ICON_NONE) {
      drawIcon(icon, r.x + iconCol * 0.5f + 4, cy, r.height * 0.4f, RAYWHITE);
    }
    int tw = label ? uiTextWidth(label, fs) : 0;
    float ix = r.x + iconCol + (r.width - iconCol - tw) / 2.0f;
    if (label) {
      uiText(label, ix, cy - fs / 2.0f, fs, RAYWHITE);
    }
  }
}

#ifdef UI_SELFTEST
// Headless checks for cursor startup, trail convergence, and focus movement
// over the two layouts the game actually has: the menu column and shop grid.
// All are pure state or geometry and need no window or input. Build:
//   cc -DUI_SELFTEST src/ui.c src/svg.c src/tex.c src/theme.c src/font.c \
//      lib/nanosvg_impl.c -Iinclude -Ilib/nanosvg/src -Ilib/raylib/src \
//      build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
//      \ -o /tmp/uitest && /tmp/uitest
#include <assert.h>
#include <stdio.h>

// What uiFocusStep would land on: the nearest item that way, or stay put.
static int moveFrom(int from, float dx, float dy) {
  gFocus = from;
  int next = nearestIn(dx, dy);
  return (next < 0) ? from : next;
}

int main(void) {
  // raylib's on-screen flag starts false. A seeded position is still drawable;
  // only the false flag plus raylib's exit position means truly outside.
  assert(!cursorOutside(false, (Vector2){400, 400}));
  assert(cursorOutside(false, (Vector2){0, 0}));
  assert(!cursorOutside(true, (Vector2){0, 0}));

  // The smoothed trail follows the hotspot, preserves ordered lag, and fully
  // settles instead of leaving a stationary streak.
  gCursorReady = false;
  cursorTrailStep((Vector2){0, 0}, 1.0f / 60.0f);
  cursorTrailStep((Vector2){100, 50}, 1.0f / 60.0f);
  assert(gCursorTrail[0].x == 100 && gCursorTrail[0].y == 50);
  assert(gCursorTrail[1].x > 0 && gCursorTrail[1].x < 100);
  assert(gCursorTrail[2].x < gCursorTrail[1].x);
  for (int i = 0; i < 120; i++) {
    cursorTrailStep((Vector2){100, 50}, 1.0f / 60.0f);
  }
  assert(fabsf(gCursorTrail[CURSOR_TRAIL - 1].x - 100) < 0.01f);
  gCursorReady = false;

  // The menu: one centered column, centeredBtn(352 + i*58, 240, 52).
  gCount = 0;
  for (int i = 0; i < 5; i++) {
    gItems[gCount++] = (Rectangle){280, 352 + i * 58, 240, 52};
  }
  assert(moveFrom(0, 0, 1) == 1);  // down steps one row
  assert(moveFrom(4, 0, -1) == 3); // up steps back
  assert(moveFrom(0, 0, -1) == 0); // no wrap past the top
  assert(moveFrom(4, 0, 1) == 4);  // no wrap past the bottom
  assert(moveFrom(2, 1, 0) == 2);  // nothing sideways in a column

  // The shop: 4 outfit categories over the ragged 3/3/3/2/1 upgrade axes,
  // then BACK. These counts mirror SHOP_PU in screen_shop.c.
  gCount = 0;
  for (int i = 0; i < 4; i++) {
    gItems[gCount++] = (Rectangle){124 + i * 144, 175, 120, 110};
  }
  const int axes[] = {3, 3, 3, 2, 1};
  int card[5];
  for (int c = 0; c < 5; c++) {
    card[c] = gCount;
    for (int a = 0; a < axes[c]; a++) {
      gItems[gCount++] = (Rectangle){30 + c * 154, 420 + a * 74, 122, 52};
    }
  }
  int back = gCount;
  gItems[gCount++] = (Rectangle){300, 712, 200, 48};

  assert(moveFrom(0, 1, 0) == 1);        // along the swatch row
  assert(moveFrom(3, -1, 0) == 2);       // and back
  assert(moveFrom(1, 0, 1) == card[2]);  // down into the card nearest below
  assert(moveFrom(card[2], 0, -1) == 1); // and back up to that swatch
  assert(moveFrom(card[2], 0, 1) == card[2] + 1); // next axis down the card
  assert(moveFrom(card[2], 1, 0) == card[3]);     // same axis, next card over
  assert(moveFrom(card[3], -1, 0) == card[2]);    // and back
  assert(moveFrom(card[4], 1, 0) == card[4]);     // nothing right of the last
  assert(moveFrom(card[2] + 2, 0, 1) == back);    // bottom axis reaches BACK
  assert(moveFrom(back, 0, -1) == card[2] + 2);   // and BACK returns to it

  // A cosmetic picker: eight choices in a 4x2 grid, then DONE below.
  gCount = 0;
  for (int i = 0; i < 8; i++) {
    gItems[gCount++] =
        (Rectangle){74 + (i % 4) * 169, 214 + (i / 4) * 136, 145, 120};
  }
  int done = gCount;
  gItems[gCount++] = (Rectangle){315, 508, 170, 44};
  assert(moveFrom(0, 1, 0) == 1);
  assert(moveFrom(3, 1, 0) == 3); // no wrap past the top row
  assert(moveFrom(0, 0, 1) == 4);
  assert(moveFrom(4, 0, -1) == 0);
  assert(moveFrom(6, 1, 0) == 7);
  assert(moveFrom(7, 1, 0) == 7); // no wrap past the bottom row
  assert(moveFrom(5, 0, 1) == done);
  assert(moveFrom(done, 0, -1) == 5);

  printf("ok - cursor trail and focus over %d items\n", gCount);
  return 0;
}
#endif
