#ifndef UI_H
#define UI_H

#include "raylib.h"

// Mouse-driven UI, shared by every screen. All hit-testing is done in virtual
// (SCREEN_WIDTH x SCREEN_HEIGHT) coordinates so it matches the fixed-size
// render target regardless of the actual window resolution.

typedef enum {
  ICON_NONE,
  ICON_PLAY,
  ICON_PAUSE,
  ICON_GEAR,
  ICON_RETRY,
  ICON_ARROW_L,
  ICON_ARROW_R,
  ICON_BACK,
  ICON_HOME,
  ICON_EXIT,
  ICON_HEART,
} IconKind;

// Window mouse position mapped into the virtual 800x800 render space.
Vector2 virtualMouse(void);
bool mouseInRect(Rectangle r); // virtual-space hit test against the mouse
bool clicked(Rectangle r);     // left-click landed inside r this frame

// Text with a soft drop shadow; drawCenteredText horizontally centers it.
void drawTextShadow(const char *text, int x, int y, int size, Color color);
void drawCenteredText(const char *text, int y, int fontSize, Color color);

// Rounded button with hover highlight + optional shape icon and label, drawn
// in draw(); detect its click separately with clicked(rect) in update().
void uiButton(Rectangle r, const char *label, IconKind icon);

// Draw one primitive-only icon centered at (cx,cy), sized ~s, in color c.
void drawIcon(IconKind kind, float cx, float cy, float s, Color c);

#endif // UI_H
