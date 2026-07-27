#include "ui.h"

#include "core.h"
#include "svg.h"
#include "theme.h"

#include <stddef.h>

// Rounded-rect drawing shares these: corner roundness, segment count, and the
// 3D "lip" (the button face sits this many px above its drop shadow).
#define BTN_ROUNDNESS 0.4f
#define ROUND_SEGMENTS 8
#define BTN_LIP 4.0f
#define LABEL_PAD 12.0f // horizontal breathing room for a button's text
#define ICON_SCALE 1.8f // drawIcon's half-extent `s` -> drawSvg's box side

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

void drawTextShadow(const char *text, int x, int y, int size, Color color) {
  DrawText(text, x + 2, y + 2, size, Fade(BLACK, 0.35f));
  DrawText(text, x, y, size, color);
}

void drawCenteredText(const char *text, int y, int fontSize, Color color) {
  int x = (SCREEN_WIDTH - MeasureText(text, fontSize)) / 2;
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
    while (fs > 8 && label && MeasureText(label, fs) > avail) {
      fs--;
    }
    if (icon != ICON_NONE) {
      drawIcon(icon, r.x + iconCol * 0.5f + 4, cy, r.height * 0.4f, RAYWHITE);
    }
    int tw = label ? MeasureText(label, fs) : 0;
    float ix = r.x + iconCol + (r.width - iconCol - tw) / 2.0f;
    if (label) {
      DrawText(label, (int)ix, (int)(cy - fs / 2.0f), fs, RAYWHITE);
    }
  }
}
