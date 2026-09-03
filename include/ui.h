#ifndef UI_H
#define UI_H

#include "raylib.h"

// Mouse-driven UI, shared by every screen. All hit-testing is done in virtual
// (SCREEN_WIDTH x SCREEN_HEIGHT) coordinates so it matches the logical draw
// space regardless of the actual window resolution.

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
  ICON_SHOP,     // candy storefront glyph for the shop
  ICON_MISSIONS, // clipboard, checks, and star seal for missions
  ICON_COUNT
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
// Focus back to the first item. Routed state entries and modal list changes
// call this; SETTINGS' direct return starts with an empty list already.
void uiFocusReset(void);

// The candy-comet pointer and its trail/click burst, in window points. Drawn by
// main.c after postEnd so no post effect can warp it away from the real cursor.
void drawCursor(void);

// Text in the game's own vector font (the `font` subsystem) — these four are
// the only text surface, nothing else calls raylib's DrawText/MeasureText.
// uiText is the bare draw; uiTextWidth is the drop-in for MeasureText and
// returns int for exactly that reason, since every call site does integer
// arithmetic on the result.
void uiText(const char *text, float x, float y, int size, Color color);
int uiTextWidth(const char *text, int size);

// Text with a soft drop shadow; drawCenteredText horizontally centers it.
void drawTextShadow(const char *text, int x, int y, int size, Color color);
void drawCenteredText(const char *text, int y, int fontSize, Color color);

// Rounded button with hover highlight + optional icon and label, drawn in
// draw(); pair it with uiItem(rect) in update() for mouse and keyboard input.
void uiButton(Rectangle r, const char *label, IconKind icon);

// Draw one SVG icon centered at (cx,cy), sized ~s, in color c.
void drawIcon(IconKind kind, float cx, float cy, float s, Color c);

#endif // UI_H
