#ifndef CORE_H
#define CORE_H

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 800
#define FLOOR_Y (SCREEN_HEIGHT * 4.0f / 5.0f) // starting ground line

typedef enum {
  GAMESTATE_SPLASH, // one-time startup logo, shown once before the menu
  GAMESTATE_MENU,
  GAMESTATE_SETTINGS,
  GAMESTATE_PLAYING,
  GAMESTATE_PAUSED,
  GAMESTATE_GAMEOVER,
  GAMESTATE_CREDITS,
  GAMESTATE_SHOP,
  GAMESTATE_COUNT // sentinel: checked against STATES[] in main.c
} GameState;

// Compile-time assertion for C99 (no _Static_assert until C11): a false
// condition makes an illegal negative-sized array and fails the build.
#define STATIC_ASSERT(cond, name)                                              \
  typedef char static_assert_##name[(cond) ? 1 : -1]

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

#endif // CORE_H
