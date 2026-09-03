// MENU — the title screen: attract-mode blob, the button column, and the two
// modal read-only overlays (How to Play, Missions) that hang off it.
#include "game.h"

#include "background.h"
#include "core.h"
#include "progress.h"
#include "raylib.h"
#include "sound.h"
#include "theme.h"
#include "ui.h"
#include "world.h"

#include <math.h>

static Rectangle rHelpBtn(void) { return (Rectangle){10, 10, 44, 44}; }
static Rectangle rMenuPlay(void) { return centeredBtn(352, 240, 56); }
static Rectangle rMenuShop(void) { return centeredBtn(412, 240, 52); }
static Rectangle rMenuMissions(void) { return centeredBtn(470, 240, 52); }
static Rectangle rMenuSettings(void) { return centeredBtn(528, 240, 52); }
static Rectangle rMenuCredits(void) { return centeredBtn(586, 240, 52); }
static Rectangle rMenuExit(void) { return centeredBtn(644, 240, 52); }
// Both overlays are modal and mutually exclusive, so they share close-button
// geometry even though the mission tabs need their own input branch.
static Rectangle rOverlayClose(void) {
  return centeredBtn(SCREEN_HEIGHT / 2.0f + 168, 180, 54);
}

typedef enum {
  MISSION_TAB_DAILY,
  MISSION_TAB_REPEATABLE,
  MISSION_TAB_ACHIEVEMENTS,
  MISSION_TAB_COUNT,
} MissionTab;

static Rectangle rMissionTab(int tab) {
  return centeredRow(262, 160, 42, tab, MISSION_TAB_COUNT);
}

// The whole game loop in six short lines. Details belong in play, not here.
static const char *HELP_LINES[] = {
    "Move with arrow keys or mouse",
    "Grab coins to climb higher",
    "Land on platforms",
    "Grab power-ups",
    "Avoid monsters and hazards",
    "Never fall below the screen",
};
#define HELP_LINE_COUNT ((int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0])))

// Shared modal frame: the menu stays visible under a dim scrim, while each
// overlay chooses its panel fill. Returns the panel rect for row layout.
static Rectangle overlayPanel(const char *title, Color fill) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  Rectangle panel = {SCREEN_WIDTH / 2.0f - 300, 180, 600, 420};
  DrawRectangleRounded(panel, 0.08f, 12, fill);
  DrawRectangleRoundedLines(panel, 0.08f, 12, UI_ACCENT);
  drawCenteredText(title, 210, 48, UI_ACCENT);
  return panel;
}

// Keep the final line above the close button and every line inside the panel.
#define HELP_TOP 286
#define HELP_STEP 44
#define HELP_FS 28
static void drawHelpOverlay(void) {
  overlayPanel("HOW TO PLAY", (Color){8, 17, 22, 255});
  for (int i = 0; i < HELP_LINE_COUNT; i++) {
    drawCenteredText(HELP_LINES[i], HELP_TOP + i * HELP_STEP, HELP_FS,
                     RAYWHITE);
  }
  uiButton(rOverlayClose(), "GOT IT", ICON_PLAY);
}

static const char *MISSION_TAB_LABELS[] = {"DAILY", "REPEATABLE",
                                           "ACHIEVEMENTS"};
STATIC_ASSERT(sizeof(MISSION_TAB_LABELS) / sizeof(MISSION_TAB_LABELS[0]) ==
                  MISSION_TAB_COUNT,
              mission_tab_labels_full);

static void drawMissionTabs(int selected) {
  for (int i = 0; i < MISSION_TAB_COUNT; i++) {
    Rectangle r = rMissionTab(i);
    uiButton(r, MISSION_TAB_LABELS[i], ICON_NONE);
    if (i == selected) {
      DrawRectangleRoundedLinesEx(r, 0.4f, 8, 3.0f, GOLD);
    }
  }
}

static void drawTaskRow(Rectangle panel, const MissionDef *task, int progress,
                        int y) {
  bool done = progress >= task->threshold;
  Rectangle row = {panel.x + 32, (float)y, panel.width - 64, 58};
  DrawRectangleRounded(row, 0.22f, 8,
                       Fade(done ? GOLD : RAYWHITE, done ? 0.16f : 0.09f));
  drawTextShadow(task->desc, (int)row.x + 14, y + 6, 19,
                 done ? GOLD : RAYWHITE);
  const char *reward = TextFormat("+%d", task->reward);
  drawTextShadow(reward,
                 (int)(row.x + row.width - 14 - uiTextWidth(reward, 19)), y + 6,
                 19, done ? Fade(GOLD, 0.65f) : COIN_LO);

  Rectangle bar = {row.x + 14, row.y + 36, row.width - 28, 10};
  float fill = clampf(progress / (float)task->threshold, 0.0f, 1.0f);
  DrawRectangleRounded(bar, 0.9f, 8, Fade(BLACK, 0.35f));
  DrawRectangleRounded((Rectangle){bar.x, bar.y, bar.width * fill, bar.height},
                       0.9f, 8, done ? GOLD : UI_ACCENT);
  const char *count = TextFormat("%d OF %d", progress, task->threshold);
  drawTextShadow(count, (int)(row.x + (row.width - uiTextWidth(count, 14)) / 2),
                 y + 33, 14, RAYWHITE);
}

// Three read-only task lists. Rewards are paid automatically at game over, so
// only the tab strip and shared close button need input.
static void drawMissionsOverlay(const Game *g) {
  const Progress *p = &g->progress;
  Rectangle panel = overlayPanel("MISSIONS", (Color){8, 17, 22, 255});
  drawMissionTabs(g->missionTab);

  if (g->missionTab == MISSION_TAB_DAILY) {
    drawCenteredText("3 TASKS FOR TODAY", 308, 16, Fade(RAYWHITE, 0.7f));
    for (int i = 0; i < DAILY_TASK_COUNT; i++) {
      drawTaskRow(panel, dailyTask(p, i), p->dailyProgress[i], 330 + i * 66);
    }
  } else if (g->missionTab == MISSION_TAB_REPEATABLE) {
    drawCenteredText("PROGRESS RESETS AFTER EACH REWARD", 308, 16,
                     Fade(RAYWHITE, 0.7f));
    for (int i = 0; i < REPEATABLE_TASK_COUNT; i++) {
      drawTaskRow(panel, &REPEATABLE_TASKS[i], p->repeatableProgress[i],
                  330 + i * 66);
    }
  } else {
    drawCenteredText("PERMANENT MILESTONES", 308, 16, Fade(RAYWHITE, 0.7f));
    for (int i = 0; i < ACHIEVEMENT_COUNT; i++) {
      bool done = achievementDone(p, i);
      int y = 338 + i * 40;
      drawTextShadow(
          TextFormat("%s %s", done ? "[x]" : "[ ]", ACHIEVEMENTS[i].desc),
          (int)panel.x + 40, y, 19, done ? GOLD : RAYWHITE);
      const char *reward = TextFormat("+%d", ACHIEVEMENTS[i].reward);
      drawTextShadow(
          reward, (int)(panel.x + panel.width - 40 - uiTextWidth(reward, 19)),
          y, 19, done ? Fade(GOLD, 0.6f) : COIN_LO);
    }
  }
  uiButton(rOverlayClose(), "GOT IT", ICON_PLAY);
}

void updateMenu(Game *g) {
  updateAttract(&g->player); // demo: blob wanders the menu
  if (g->showHelp) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
        uiItem(rOverlayClose())) {
      g->showHelp = false;
      uiFocusReset();
      sfx(SFX_SELECT);
    }
    return;
  }
  if (g->showMissions) {
    if (progressRefreshDaily(&g->progress)) {
      saveProgress(&g->progress); // refresh even if the menu spans midnight
    }
    bool tabs[MISSION_TAB_COUNT];
    for (int i = 0; i < MISSION_TAB_COUNT; i++) {
      tabs[i] = uiItem(rMissionTab(i));
    }
    bool close = uiItem(rOverlayClose());
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || close) {
      g->showMissions = false;
      uiFocusReset();
      sfx(SFX_SELECT);
    } else {
      for (int i = 0; i < MISSION_TAB_COUNT; i++) {
        if (tabs[i]) {
          g->missionTab = i;
          sfx(SFX_SELECT);
        }
      }
    }
    return;
  }
  // Every button registers with uiItem every frame, in a stable order — an
  // early-out chain would drop the ones below whatever was pressed and the
  // focus ring would jump. PLAY registers first, so that's where the ring
  // starts when the menu is entered.
  bool play = uiItem(rMenuPlay());
  bool shop = uiItem(rMenuShop());
  bool missions = uiItem(rMenuMissions());
  bool settings = uiItem(rMenuSettings());
  bool credits = uiItem(rMenuCredits());
  bool quit = uiItem(rMenuExit());
  bool help = uiItem(rHelpBtn());

  if (help) {
    g->showHelp = true;
    uiFocusReset();
    sfx(SFX_SELECT);
    return;
  }
  if (missions) {
    g->showMissions = true;
    g->missionTab = MISSION_TAB_DAILY;
    uiFocusReset();
    sfx(SFX_SELECT);
    return;
  }
  if (IsKeyPressed(KEY_SPACE) || play) {
    startGame(g, true);
  } else if (shop) {
    gotoState(g, GAMESTATE_SHOP);
    sfx(SFX_SELECT);
  } else if (IsKeyPressed(KEY_S) || settings) {
    gotoState(g, GAMESTATE_SETTINGS);
    sfx(SFX_SELECT);
  } else if (credits) {
    // Freeze the current menu onto the tube, then start the zoom-out. If its
    // optional target failed to allocate, the credits screen falls back to
    // static during the pull-back instead.
    if (IsRenderTextureValid(gMenuSnap)) {
      BeginTextureMode(gMenuSnap);
      drawMenu(g);
      EndTextureMode();
    }
    g->creditsAnim = 0.0f;
    g->creditsStatic = 0.5f; // static burst plays when the zoom-out finishes
    g->creditsScroll = 0.0f;
    g->creditsExiting = false;
    gotoState(g, GAMESTATE_CREDITS);
    sfx(SFX_SELECT);
  } else if (quit) {
    g->quit = true;
  }
}

void drawMenu(Game *g) {
  drawMenuBackground();
  drawGround();           // so the attract blob stands on grass, like gameplay
  drawPlayer(&g->player); // the wandering attract blob
  int bob = (int)(sinf((float)GetTime() * 2.0f) * 10.0f);
  drawCenteredText("BOING!", SCREEN_HEIGHT / 4 - 40 + bob, 80, UI_ACCENT);
  drawCenteredText(TextFormat("BEST %d", g->highScore), SCREEN_HEIGHT / 4 + 60,
                   24, COIN_LO);
  uiButton(rMenuPlay(), "PLAY", ICON_PLAY);
  uiButton(rMenuShop(), "SHOP", ICON_SHOP);
  uiButton(rMenuMissions(), "MISSIONS", ICON_MISSIONS);
  uiButton(rMenuSettings(), "SETTINGS", ICON_GEAR);
  uiButton(rMenuCredits(), "CREDITS", ICON_HEART);
  uiButton(rMenuExit(), "EXIT", ICON_EXIT);
  uiButton(rHelpBtn(), "?", ICON_NONE);
  if (g->showHelp) {
    drawHelpOverlay();
  } else if (g->showMissions) {
    drawMissionsOverlay(g);
  }
}
