#include "ui.h"

#include "core.h"
#include "theme.h"

#include <stddef.h>

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

void drawIcon(IconKind kind, float cx, float cy, float s, Color c) {
  switch (kind) {
  case ICON_PLAY:
    DrawTriangle((Vector2){cx - s * 0.5f, cy - s * 0.6f},
                 (Vector2){cx - s * 0.5f, cy + s * 0.6f},
                 (Vector2){cx + s * 0.7f, cy}, c);
    break;
  case ICON_PAUSE:
    DrawRectangle(cx - s * 0.45f, cy - s * 0.6f, s * 0.35f, s * 1.2f, c);
    DrawRectangle(cx + s * 0.1f, cy - s * 0.6f, s * 0.35f, s * 1.2f, c);
    break;
  case ICON_GEAR: {
    for (int i = 0; i < 6; i++) {
      float a = i * (PI / 3.0f);
      DrawRectanglePro((Rectangle){cx, cy, s * 0.5f, s * 1.5f},
                       (Vector2){s * 0.25f, s * 0.75f}, a * RAD2DEG, c);
    }
    DrawCircle(cx, cy, s * 0.6f, c);
    DrawCircle(cx, cy, s * 0.28f, Fade(BLACK, 0.55f));
    break;
  }
  case ICON_RETRY:
    DrawRing((Vector2){cx, cy}, s * 0.45f, s * 0.68f, -40.0f, 220.0f, 24, c);
    DrawTriangle((Vector2){cx + s * 0.75f, cy - s * 0.35f},
                 (Vector2){cx + s * 0.15f, cy - s * 0.35f},
                 (Vector2){cx + s * 0.45f, cy + s * 0.25f}, c);
    break;
  case ICON_ARROW_L:
    DrawTriangle((Vector2){cx + s * 0.4f, cy - s * 0.55f},
                 (Vector2){cx - s * 0.5f, cy},
                 (Vector2){cx + s * 0.4f, cy + s * 0.55f}, c);
    break;
  case ICON_BACK:
  case ICON_ARROW_R:
    DrawTriangle((Vector2){cx - s * 0.4f, cy - s * 0.55f},
                 (Vector2){cx - s * 0.4f, cy + s * 0.55f},
                 (Vector2){cx + s * 0.5f, cy}, c);
    break;
  case ICON_HOME:
    DrawTriangle((Vector2){cx, cy - s * 0.7f},
                 (Vector2){cx - s * 0.7f, cy - s * 0.05f},
                 (Vector2){cx + s * 0.7f, cy - s * 0.05f}, c);    // roof
    DrawRectangle(cx - s * 0.5f, cy - s * 0.05f, s, s * 0.7f, c); // body
    DrawRectangle(cx - s * 0.15f, cy + s * 0.2f, s * 0.3f, s * 0.45f,
                  Fade(BLACK, 0.55f)); // door
    break;
  case ICON_EXIT:
    DrawLineEx((Vector2){cx - s * 0.5f, cy - s * 0.5f},
               (Vector2){cx + s * 0.5f, cy + s * 0.5f}, s * 0.24f, c);
    DrawLineEx((Vector2){cx - s * 0.5f, cy + s * 0.5f},
               (Vector2){cx + s * 0.5f, cy - s * 0.5f}, s * 0.24f, c);
    break;
  case ICON_HEART:
    DrawCircle(cx - s * 0.3f, cy - s * 0.18f, s * 0.36f, c); // left lobe
    DrawCircle(cx + s * 0.3f, cy - s * 0.18f, s * 0.36f, c); // right lobe
    DrawTriangle((Vector2){cx, cy + s * 0.7f}, (Vector2){cx + 0.62f * s, cy},
                 (Vector2){cx - 0.62f * s, cy}, c); // point
    break;
  case ICON_NONE:
    break;
  }
}

// Renders the button; clicks are detected by the caller via clicked(r).
void uiButton(Rectangle r, const char *label, IconKind icon) {
  bool hover = mouseInRect(r);
  Color base = hover ? CANDY_PINK : CANDY_PINK_DK;
  DrawRectangleRounded(r, 0.4f, 8, Fade(BLACK, 0.25f));
  DrawRectangleRounded((Rectangle){r.x, r.y, r.width, r.height - 4}, 0.4f, 8,
                       base);

  float cx = r.x + r.width / 2.0f, cy = r.y + r.height / 2.0f;
  if (icon != ICON_NONE && (label == NULL || label[0] == '\0')) {
    drawIcon(icon, cx, cy, r.height * 0.5f, RAYWHITE);
  } else {
    // Slim icon column + a font that shrinks to fit so labels never overflow.
    float iconCol = (icon != ICON_NONE) ? r.height * 0.72f : 0.0f;
    float avail = r.width - iconCol - 12.0f;
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
