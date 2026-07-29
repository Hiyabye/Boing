#include "ui.h"

#include "core.h"
#include "font.h"
#include "svg.h"
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
#define CURSOR_PX 26.0f // pointer box side, in window points at 800 wide

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
// 32 covers the busiest screen (the shop: 4 skins + 12 upgrade axes + back).
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
void drawCursor(void) {
  Vector2 m = GetMousePosition();
  // Window points, not 800-space: this draws after postEnd, outside the frame's
  // scale matrix. Sizing off GetScreenWidth keeps it constant on screen at
  // every resolution option.
  float s = CURSOR_PX * (float)GetScreenWidth() / (float)SCREEN_WIDTH;
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    s *= 0.86f; // click feedback
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
    [ICON_PLAY - 1] = SVG_PLAY, [ICON_PAUSE - 1] = SVG_PAUSE,
    [ICON_GEAR - 1] = SVG_GEAR, [ICON_RETRY - 1] = SVG_RETRY,
    [ICON_BACK - 1] = SVG_BACK, [ICON_HOME - 1] = SVG_HOME,
    [ICON_EXIT - 1] = SVG_EXIT, [ICON_HEART - 1] = SVG_HEART,
    [ICON_SHOP - 1] = SVG_SHOP, [ICON_MISSIONS - 1] = SVG_MISSIONS,
};
// A missing row would silently draw SVG_PLAY (a zero-initialized slot) for the
// icon that lost it, so tie the table to the enum.
STATIC_ASSERT(sizeof(ICON_SVG) / sizeof(ICON_SVG[0]) == ICON_MISSIONS,
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

// Renders the button; clicks are detected by the caller via clicked(r).
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
// Focus-move check for the two layouts the game actually has: the menu's button
// column and the shop's swatch row over its upgrade grid. nearestIn is pure
// geometry over the item list, so this needs no window and no input — it fills
// gItems directly. Build:
//   cc -DUI_SELFTEST src/ui.c src/svg.c src/theme.c lib/nanosvg_impl.c \
//      -Iinclude -Ilib/nanosvg/src -Ilib/raylib/src \
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

  // The shop: 4 skin swatches (centeredRow(175,120,110,i,4)) over a 5x3 grid of
  // upgrade axes (rShopUpgAxis: x = 30 + card*154, y = 420 + axis*74).
  gCount = 0;
  for (int i = 0; i < 4; i++) {
    gItems[gCount++] = (Rectangle){124 + i * 144, 175, 120, 110};
  }
  int grid = gCount; // first axis cell; card c axis a is grid + c*3 + a
  for (int c = 0; c < 5; c++) {
    for (int a = 0; a < 3; a++) {
      gItems[gCount++] = (Rectangle){30 + c * 154, 420 + a * 74, 122, 52};
    }
  }
  assert(moveFrom(0, 1, 0) == 1);         // along the swatch row
  assert(moveFrom(3, -1, 0) == 2);        // and back
  assert(moveFrom(1, 0, 1) == grid + 6);  // down into the card nearest below
  assert(moveFrom(grid + 6, 0, -1) == 1); // and back up to that swatch
  assert(moveFrom(grid + 6, 0, 1) == grid + 7);   // next axis down the card
  assert(moveFrom(grid + 6, 1, 0) == grid + 9);   // same axis, next card over
  assert(moveFrom(grid + 9, -1, 0) == grid + 6);  // and back
  assert(moveFrom(grid + 12, 1, 0) == grid + 12); // nothing right of the last

  printf("ok — focus moves correctly over %d items\n", gCount);
  return 0;
}
#endif
