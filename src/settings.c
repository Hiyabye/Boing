#include "settings.h"

#include "persist.h"
#include "raylib.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"

static const int FPS_OPTIONS[] = {30, 60, 90, 120, 144, 165, 240};
// 100-point window-size steps around the native 800x800 layout. Only sizes
// within 90% of the current display are exposed, leaving room for macOS window
// chrome, the menu bar, and the Dock.
static const int RES_OPTIONS[] = {400,  500,  600,  700,  800, 900,
                                  1000, 1100, 1200, 1300, 1400};
#define FPS_COUNT ((int)(sizeof(FPS_OPTIONS) / sizeof(FPS_OPTIONS[0])))
#define RES_COUNT ((int)(sizeof(RES_OPTIONS) / sizeof(RES_OPTIONS[0])))
#define DEFAULT_RES 800

// The settings screen is one row per option. Adding a row = a new RowKind, a
// table entry, and a case in changeRow()/formatValue() below.
typedef enum { ROW_FPS, ROW_RES, ROW_MUSIC, ROW_SFX, ROW_COUNT } RowKind;
// Positional in exact RowKind order, so a missing label shortens the table.
static const char *const ROW_LABELS[] = {"FRAMERATE", "RESOLUTION", "MUSIC",
                                         "SFX"};
// Every RowKind must have a label (and a case in the row switches below).
STATIC_ASSERT(sizeof(ROW_LABELS) / sizeof(ROW_LABELS[0]) == ROW_COUNT,
              rows_full);

static int selRow = 0;
static int dragRow = -1; // row whose slider is being dragged, -1 = none
static bool suppressResize = false; // hold the window size steady mid res-drag

static int wrapi(int v, int n) { return ((v % n) + n) % n; }

static int resolutionCountFor(int width, int height) {
  int limit = (width < height ? width : height) * 9 / 10;
  int count = 0;
  while (count < RES_COUNT && RES_OPTIONS[count] <= limit) {
    count++;
  }
  return count > 0 ? count : 1;
}

static int resolutionCount(void) {
  int monitor = GetCurrentMonitor();
  return resolutionCountFor(GetMonitorWidth(monitor),
                            GetMonitorHeight(monitor));
}

// Pick the nearest preset, preferring the smaller size on an exact tie.
static int resolutionIndex(int size, int count) {
  for (int i = 1; i < count; i++) {
    if (size <= (RES_OPTIONS[i - 1] + RES_OPTIONS[i]) / 2) {
      return i - 1;
    }
  }
  return count - 1;
}

// Volume is persisted/displayed as a rounded int percent.
static int volumePercent(float volume) { return (int)(volume * 100.0f + 0.5f); }

// Shared hit/draw geometry so update (drag) and draw (render) agree. Each row
// is a horizontal slider: a full-width track with a knob at the current stop.
#define ROW_Y(row) (SCREEN_HEIGHT / 2 - 60 + (row) * 60)
#define TRACK_X0 180
#define TRACK_X1 620
#define TRACK_W (TRACK_X1 - TRACK_X0)
static Rectangle rowRect(int row) {
  return (Rectangle){160, ROW_Y(row) - 6, 480, 54};
}
static float trackY(int row) { return ROW_Y(row) + 38.0f; }
static Rectangle trackHitRect(int row) { // fat grab band around the thin bar
  return (Rectangle){TRACK_X0 - 12, trackY(row) - 15, TRACK_W + 24, 30};
}
static Rectangle backRect(void) {
  return (Rectangle){SCREEN_WIDTH / 2.0f - 80, 700, 160, 52};
}

// Each row is a slider with a fixed number of discrete stops: one per option
// for fps/res, 21 stops (5% steps) for volume.
#define VOLUME_STOPS 21
static int rowStops(int row) {
  switch (row) {
  case ROW_FPS:
    return FPS_COUNT;
  case ROW_RES:
    return resolutionCount();
  case ROW_MUSIC:
  case ROW_SFX:
    return VOLUME_STOPS;
  }
  return 1;
}
static int rowStop(const Settings *s, int row) {
  switch (row) {
  case ROW_FPS:
    return s->fpsIndex;
  case ROW_RES:
    return s->resIndex;
  case ROW_MUSIC:
    return (int)(s->musicVolume * (VOLUME_STOPS - 1) + 0.5f);
  case ROW_SFX:
    return (int)(s->sfxVolume * (VOLUME_STOPS - 1) + 0.5f);
  }
  return 0;
}
static void setRowStop(Settings *s, int row, int stop) {
  stop = stop < 0 ? 0 : (stop >= rowStops(row) ? rowStops(row) - 1 : stop);
  switch (row) {
  case ROW_FPS:
    s->fpsIndex = stop;
    break;
  case ROW_RES:
    s->resIndex = stop;
    break;
  case ROW_MUSIC:
    s->musicVolume = stop / (float)(VOLUME_STOPS - 1);
    break;
  case ROW_SFX:
    s->sfxVolume = stop / (float)(VOLUME_STOPS - 1);
    break;
  }
  applySettings(s);
  saveSettings(s);
  sfx(SFX_SELECT);
}

// Keyboard nudge: move one stop (clamped, no wrap — it's a slider now).
static void changeRow(Settings *s, int row, int dir) {
  setRowStop(s, row, rowStop(s, row) + dir);
}

// Current value of a row, formatted for display.
static const char *formatValue(const Settings *s, int row) {
  switch (row) {
  case ROW_FPS:
    return TextFormat("%d FPS", FPS_OPTIONS[s->fpsIndex]);
  case ROW_RES:
    return TextFormat("%dx%d", RES_OPTIONS[s->resIndex],
                      RES_OPTIONS[s->resIndex]);
  case ROW_MUSIC:
    return TextFormat("%d%%", volumePercent(s->musicVolume));
  case ROW_SFX:
    return TextFormat("%d%%", volumePercent(s->sfxVolume));
  }
  return "";
}

Settings loadSettings(void) {
  Settings s = {.fpsIndex = 1,
                .resIndex = resolutionIndex(DEFAULT_RES, RES_COUNT),
                .musicVolume = 1.0f,
                .sfxVolume = 1.0f}; // 60fps, 800, both max
  s.fpsIndex = configGetInt("fpsIndex", s.fpsIndex);
  // Older saves stored an index into 600..1400 in 100-point steps. New saves
  // store the actual size so future preset changes do not reinterpret it.
  int legacyIndex = configGetInt("resIndex", -1);
  int legacySize = (legacyIndex >= 0 && legacyIndex < 9)
                       ? 600 + legacyIndex * 100
                       : DEFAULT_RES;
  int savedSize = configGetInt("windowSize", legacySize);
  s.resIndex = resolutionIndex(savedSize, RES_COUNT);
  s.musicVolume =
      configGetInt("musicPct", 100) / 100.0f; // stored as int percents
  s.sfxVolume = configGetInt("sfxPct", 100) / 100.0f;
  s.fpsIndex = wrapi(s.fpsIndex, FPS_COUNT);
  s.musicVolume = clampf(s.musicVolume, 0.0f, 1.0f);
  s.sfxVolume = clampf(s.sfxVolume, 0.0f, 1.0f);
  return s;
}

void saveSettings(const Settings *s) {
  configSetInt("fpsIndex", s->fpsIndex);
  configSetInt("windowSize", RES_OPTIONS[s->resIndex]);
  configSetInt("musicPct", volumePercent(s->musicVolume));
  configSetInt("sfxPct", volumePercent(s->sfxVolume));
  configSaveUser();
}

void applySettings(Settings *s) {
  SetTargetFPS(FPS_OPTIONS[s->fpsIndex]);
  setVolumes(s->musicVolume, s->sfxVolume);
  int count = resolutionCount();
  if (s->resIndex >= count) {
    s->resIndex = count - 1;
  }
  if (!suppressResize) {
    int size = RES_OPTIONS[s->resIndex];
    SetWindowSize(size, size);
  }
}

void updateSettingsMenu(Settings *s, GameState *state, GameState returnTo) {
  if (IsKeyPressed(KEY_DOWN)) {
    selRow = wrapi(selRow + 1, ROW_COUNT);
    sfx(SFX_SELECT);
  }
  if (IsKeyPressed(KEY_UP)) {
    selRow = wrapi(selRow - 1, ROW_COUNT);
    sfx(SFX_SELECT);
  }

  int dir = IsKeyPressed(KEY_RIGHT) ? 1 : (IsKeyPressed(KEY_LEFT) ? -1 : 0);
  if (dir != 0) {
    changeRow(s, selRow, dir);
  }

  // Mouse: Back to exit, else grab a row's slider and drag the knob to its
  // stop.
  if (clicked(backRect())) {
    *state = returnTo;
    sfx(SFX_SELECT);
    return;
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    for (int r = 0; r < ROW_COUNT; r++) {
      if (mouseInRect(trackHitRect(r))) {
        dragRow = r;
        selRow = r;
      }
    }
  }
  if (dragRow >= 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    // Resolution resizes the window, which moves the cursor's slider fraction
    // under us and fights the drag — hold the size steady until release.
    suppressResize = (dragRow == ROW_RES);
    float frac =
        clampf((virtualMouse().x - TRACK_X0) / (float)TRACK_W, 0.0f, 1.0f);
    int stop = (int)(frac * (rowStops(dragRow) - 1) + 0.5f);
    if (stop != rowStop(s, dragRow)) {
      setRowStop(s, dragRow, stop);
    }
  } else if (dragRow >= 0) { // released: commit any deferred window resize
    suppressResize = false;
    applySettings(s);
    dragRow = -1;
  }

  if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE)) {
    if (dragRow >= 0) {
      suppressResize = false;
      applySettings(s); // commit a resolution drag before leaving mid-hold
      dragRow = -1;
    }
    *state = returnTo;
    sfx(SFX_SELECT);
  }
}

static void drawRow(const Settings *s, int row, const char *label,
                    const char *value) {
  bool sel = (row == selRow);
  int y = ROW_Y(row);
  Color col = sel ? COIN_HI : RAYWHITE;
  int fs = 28;
  if (sel) {
    DrawRectangleRounded(rowRect(row), 0.4f, 8, Fade(BLACK, 0.25f));
  }
  uiText(label, 180, y, fs, col);
  int vw = uiTextWidth(value, fs);
  uiText(value, TRACK_X1 - vw, y, fs, col); // right-aligned over the track end

  // Slider: dim full track, bright fill up to the knob, round knob on top.
  float ty = trackY(row);
  int stops = rowStops(row);
  float frac = stops > 1 ? rowStop(s, row) / (float)(stops - 1) : 0.0f;
  float knobX = TRACK_X0 + frac * TRACK_W;
  DrawRectangleRounded((Rectangle){TRACK_X0, ty - 3, TRACK_W, 6}, 1.0f, 4,
                       Fade(RAYWHITE, 0.25f));
  if (knobX > TRACK_X0) {
    DrawRectangleRounded((Rectangle){TRACK_X0, ty - 3, knobX - TRACK_X0, 6},
                         1.0f, 4, COIN_HI);
  }
  DrawCircle((int)knobX, (int)ty, sel ? 11 : 9, col);
  DrawCircle((int)knobX, (int)ty, sel ? 5 : 4, Fade(BLACK, 0.35f));
}

void drawSettingsMenu(const Settings *s) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(SKY_TOP_HIGH, 0.98f));

  drawCenteredText("SETTINGS", SCREEN_HEIGHT / 5, 60, UI_ACCENT);

  for (int r = 0; r < ROW_COUNT; r++) {
    drawRow(s, r, ROW_LABELS[r], formatValue(s, r));
  }

  uiButton(backRect(), "BACK", ICON_BACK);
}

#ifdef SETTINGS_SELFTEST
// Pure preset and legacy-save checks. Dead stripping drops the renderer:
//   cc -DSETTINGS_SELFTEST -ffunction-sections src/settings.c src/persist.c \
//      -Iinclude -Ilib/raylib/src -Wl,-dead_strip -o /tmp/settingstest && \
//      /tmp/settingstest
#include <assert.h>

int main(void) {
  assert(resolutionCountFor(1512, 982) == 5);   // 400 through 800
  assert(resolutionCountFor(800, 600) == 2);    // 400 and 500
  assert(resolutionCountFor(0, 0) == 1);        // always expose one safe stop
  assert(resolutionIndex(450, RES_COUNT) == 0); // ties prefer smaller
  assert(resolutionIndex(451, RES_COUNT) == 1);
  assert(resolutionIndex(2000, RES_COUNT) == RES_COUNT - 1);

  configLoad("/no/such/settings.json");
  configSetInt("resIndex", 3); // old 600..1400 table: index 3 was 900
  Settings s = loadSettings();
  assert(RES_OPTIONS[s.resIndex] == 900);

  configSetInt("windowSize", 500); // explicit size supersedes legacy index
  s = loadSettings();
  assert(RES_OPTIONS[s.resIndex] == 500);
  return 0;
}
#endif
