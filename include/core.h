#ifndef CORE_H
#define CORE_H

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define FLOOR_Y (SCREEN_HEIGHT * 4.0f / 5.0f) // starting ground line

typedef enum {
  GAMESTATE_MENU,
  GAMESTATE_SETTINGS,
  GAMESTATE_PLAYING,
  GAMESTATE_PAUSED,
  GAMESTATE_GAMEOVER
} GameState;

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

#endif // CORE_H
