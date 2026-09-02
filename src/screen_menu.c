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
// Both overlays are modal and mutually exclusive, so they share one close
// button (and one close-input branch in updateMenu).
static Rectangle rOverlayClose(void) {
  return centeredBtn(SCREEN_HEIGHT / 2.0f + 168, 180, 54);
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

// The dimmed rounded panel both overlays sit on. Returns the panel rect so the
// caller can lay its rows out against the edges.
static Rectangle overlayPanel(const char *title) {
  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));
  Rectangle panel = {SCREEN_WIDTH / 2.0f - 300, 180, 600, 420};
  DrawRectangleRounded(panel, 0.08f, 12, Fade(BLACK, 0.55f));
  DrawRectangleRoundedLines(panel, 0.08f, 12, CANDY_PINK);
  drawCenteredText(title, 210, 48, CANDY_PINK);
  return panel;
}

// Keep the final line above the close button and every line inside the panel.
#define HELP_TOP 286
#define HELP_STEP 44
#define HELP_FS 28
static void drawHelpOverlay(void) {
  overlayPanel("HOW TO PLAY");
  for (int i = 0; i < HELP_LINE_COUNT; i++) {
    drawCenteredText(HELP_LINES[i], HELP_TOP + i * HELP_STEP, HELP_FS,
                     RAYWHITE);
  }
  uiButton(rOverlayClose(), "GOT IT", ICON_PLAY);
}

// Missions overlay — read-only checklist with rewards, ticked once completed.
static void drawMissionsOverlay(const Game *g) {
  const Progress *p = &g->progress;
  Rectangle panel = overlayPanel("MISSIONS");
  int y = 300;
  for (int i = 0; i < MISSION_COUNT; i++) {
    bool done = missionDone(p, i);
    drawTextShadow(TextFormat("%s %s", done ? "[x]" : "[ ]", MISSIONS[i].desc),
                   (int)panel.x + 40, y, 22, done ? GOLD : RAYWHITE);
    const char *rw = TextFormat("+%d", MISSIONS[i].reward);
    drawTextShadow(rw, (int)(panel.x + panel.width - 40 - uiTextWidth(rw, 22)),
                   y, 22, done ? Fade(GOLD, 0.6f) : COIN_LO);
    y += 46;
  }
  uiButton(rOverlayClose(), "GOT IT", ICON_PLAY);
}

void updateMenu(Game *g) {
  updateAttract(&g->player);            // demo: blob wanders the menu
  if (g->showHelp || g->showMissions) { // modal open: only close-input is live
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) ||
        uiItem(rOverlayClose())) {
      g->showHelp = g->showMissions = false;
      sfx(SFX_SELECT);
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
    sfx(SFX_SELECT);
    return;
  }
  if (missions) {
    g->showMissions = true;
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
    // Freeze the current menu onto the tube, then start the zoom-out.
    BeginTextureMode(gMenuSnap);
    drawMenu(g);
    EndTextureMode();
    g->creditsAnim = 0.0f;
    g->creditsStatic = 0.5f; // static burst plays when the zoom-out finishes
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
  drawCenteredText("BOING!", SCREEN_HEIGHT / 4 - 40 + bob, 80, CANDY_PINK);
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
