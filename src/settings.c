#include "settings.h"

#include "persist.h"
#include "raylib.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"

const int FPS_OPTIONS[] = {30, 60, 120, 144};
const int FPS_COUNT = 4;
// Square window sizes — game renders at a fixed SCREEN_WIDTH x SCREEN_HEIGHT
// target and is blitted scaled to the window, so any square size works.
const int RES_OPTIONS[] = {600, 800, 1000, 1200};
const int RES_COUNT = 4;

#define ROW_COUNT 3

static int selRow = 0;

static int wrapi(int v, int n) { return ((v % n) + n) % n; }

// Shared hit/draw geometry so update (click) and draw (render) agree.
#define ROW_Y(row) (SCREEN_HEIGHT / 2 - 60 + (row) * 60)
static Rectangle rowRect(int row) {
  return (Rectangle){160, ROW_Y(row) - 6, 480, 44};
}
static Rectangle arrowRect(int row, int side) { // side: -1 left, +1 right
  return (Rectangle){side < 0 ? 400 : 560, ROW_Y(row) - 6, 52, 44};
}
static Rectangle backRect(void) {
  return (Rectangle){SCREEN_WIDTH / 2.0f - 80, 700, 160, 52};
}

static void changeRow(Settings *s, int row, int dir) {
  switch (row) {
  case 0:
    s->fpsIndex = wrapi(s->fpsIndex + dir, FPS_COUNT);
    break;
  case 1:
    s->resIndex = wrapi(s->resIndex + dir, RES_COUNT);
    break;
  case 2:
    s->volume = clampf(s->volume + dir * 0.1f, 0.0f, 1.0f);
    break;
  }
  applySettings(s);
  saveSettings(s);
  sfxSelect();
}

Settings loadSettings(void) {
  Settings s = {
      .fpsIndex = 1, .resIndex = 1, .volume = 1.0f}; // 60fps, 800, max
  int buf[3];
  if (readInts("settings", buf, 3)) {
    s.fpsIndex = buf[0];
    s.resIndex = buf[1];
    s.volume = buf[2] / 100.0f; // stored as an int percent
  }
  s.fpsIndex = wrapi(s.fpsIndex, FPS_COUNT);
  s.resIndex = wrapi(s.resIndex, RES_COUNT);
  s.volume = clampf(s.volume, 0.0f, 1.0f);
  return s;
}

void saveSettings(const Settings *s) {
  int buf[3] = {s->fpsIndex, s->resIndex, (int)(s->volume * 100.0f + 0.5f)};
  writeInts("settings", buf, 3);
}

void applySettings(const Settings *s) {
  SetTargetFPS(FPS_OPTIONS[s->fpsIndex]);
  SetMasterVolume(s->volume);
  int size = RES_OPTIONS[s->resIndex];
  SetWindowSize(size, size);
}

void updateSettingsMenu(Settings *s, GameState *state, GameState returnTo) {
  if (IsKeyPressed(KEY_DOWN)) {
    selRow = wrapi(selRow + 1, ROW_COUNT);
    sfxSelect();
  }
  if (IsKeyPressed(KEY_UP)) {
    selRow = wrapi(selRow - 1, ROW_COUNT);
    sfxSelect();
  }

  int dir = IsKeyPressed(KEY_RIGHT) ? 1 : (IsKeyPressed(KEY_LEFT) ? -1 : 0);

  // Mouse: click a row to select it, click its < / > to change it, Back to
  // exit.
  if (clicked(backRect())) {
    *state = returnTo;
    sfxSelect();
    return;
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    for (int r = 0; r < ROW_COUNT; r++) {
      if (mouseInRect(arrowRect(r, -1))) {
        selRow = r;
        dir = -1;
      } else if (mouseInRect(arrowRect(r, 1))) {
        selRow = r;
        dir = 1;
      } else if (mouseInRect(rowRect(r))) {
        selRow = r;
        sfxSelect();
      }
    }
  }

  if (dir != 0) {
    changeRow(s, selRow, dir);
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE)) {
    *state = returnTo;
    sfxSelect();
  }
}

static void drawRow(int row, const char *label, const char *value) {
  bool sel = (row == selRow);
  int y = ROW_Y(row);
  Color col = sel ? COIN_HI : RAYWHITE;
  int fs = 30;
  if (sel) {
    DrawRectangleRounded(rowRect(row), 0.5f, 8, Fade(BLACK, 0.25f));
  }
  DrawText(label, 180, y, fs, col);

  // Value centered between the two clickable arrows (highlight on hover).
  Rectangle la = arrowRect(row, -1), ra = arrowRect(row, 1);
  float lcx = la.x + la.width / 2, rcx = ra.x + ra.width / 2;
  int vw = MeasureText(value, fs);
  DrawText(value, (int)((lcx + rcx) / 2) - vw / 2, y, fs, col);
  drawIcon(ICON_ARROW_L, lcx, la.y + la.height / 2, 20,
           mouseInRect(la) ? COIN_HI : col);
  drawIcon(ICON_ARROW_R, rcx, ra.y + ra.height / 2, 20,
           mouseInRect(ra) ? COIN_HI : col);
}

void drawSettingsMenu(const Settings *s) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(SKY_TOP_HIGH, 0.85f));

  drawCenteredText("SETTINGS", SCREEN_HEIGHT / 5, 60, CANDY_PINK);

  drawRow(0, "FRAMERATE", TextFormat("%d FPS", FPS_OPTIONS[s->fpsIndex]));
  drawRow(
      1, "RESOLUTION",
      TextFormat("%dx%d", RES_OPTIONS[s->resIndex], RES_OPTIONS[s->resIndex]));
  drawRow(2, "VOLUME", TextFormat("%d%%", (int)(s->volume * 100.0f + 0.5f)));

  uiButton(backRect(), "BACK", ICON_BACK);
}
