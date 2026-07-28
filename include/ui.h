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
  ICON_BACK,
  ICON_HOME,
  ICON_EXIT,
  ICON_HEART,
  ICON_SHOP,     // stacked-coins glyph for the shop
  ICON_MISSIONS, // checklist glyph (check tick + task lines) for the missions
                 // tab
} IconKind;

// Window mouse position mapped into the virtual 800x800 render space.
Vector2 virtualMouse(void);
bool mouseInRect(Rectangle r); // virtual-space hit test against the mouse
bool clicked(Rectangle r);     // left-click landed inside r this frame

// ---------------------------------------------------------------------------
// Keyboard focus — makes every clickable reachable without a mouse.
// ---------------------------------------------------------------------------
// One shared list, not a per-screen index: screens are mutually exclusive, so
// the focused item is a single global. A screen's update() calls uiItem() in
// place of clicked() for each of its rects; main.c drives the rest. A screen
// that registers nothing (SPLASH, PLAYING, SETTINGS) keeps the arrow keys for
// itself and draws no ring.
//
// Registers r as a focusable item and reports activation: a left-click inside
// it, or Enter while it holds focus. Call once per item per frame, in a stable
// order.
bool uiItem(Rectangle r);
// Moves focus with the arrow keys, then clears the frame's item list.
// main.c calls this straight after the screen's update().
void uiFocusStep(void);
// Draws the focus ring and the key-hint line. main.c calls it after the
// screen's draw(), so it lands on top of any overlay.
void uiFocusDraw(void);
// Focus back to the first item — game.c calls this on every state change.
void uiFocusReset(void);

// The custom mouse pointer, in window points. Drawn by main.c *after* postEnd
// so no post effect can warp it away from the real cursor position.
void drawCursor(void);

// Text with a soft drop shadow; drawCenteredText horizontally centers it.
void drawTextShadow(const char *text, int x, int y, int size, Color color);
void drawCenteredText(const char *text, int y, int fontSize, Color color);

// Rounded button with hover highlight + optional shape icon and label, drawn
// in draw(); detect its click separately with clicked(rect) in update().
void uiButton(Rectangle r, const char *label, IconKind icon);

// Draw one primitive-only icon centered at (cx,cy), sized ~s, in color c.
void drawIcon(IconKind kind, float cx, float cy, float s, Color c);

#endif // UI_H
