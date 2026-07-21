#include "player.h"

#include "core.h"
#include "theme.h"

#include <math.h>

#define GRAVITY 1.0f
#define MOVE_SPEED 8.0f
#define HORIZONTAL_DAMPING 0.9f
#define PLAYER_RADIUS 34.0f
#define FOLLOW_K 0.18f       // mouse-follow steering stiffness
#define ATTRACT_SPEED 2.6f   // menu demo wander pace
#define ATTRACT_BOB 10.0f    // menu demo vertical bob amplitude

Player createPlayer(void) {
  Player p = {
      .position = {SCREEN_WIDTH / 2.0f, FLOOR_Y - PLAYER_RADIUS},
      .velocity = {0.0f, 0.0f},
      .acceleration = {0.0f, GRAVITY},
      .radius = PLAYER_RADIUS,
      .trailHead = 0,
  };
  for (int i = 0; i < TRAIL_LEN; i++) {
    p.trail[i] = p.position; // avoid a trail streaking from (0,0)
  }
  return p;
}

void handlePlayerInput(Player *player, float targetX) {
  if (IsKeyDown(KEY_LEFT)) {
    player->velocity.x = -MOVE_SPEED;
  } else if (IsKeyDown(KEY_RIGHT)) {
    player->velocity.x = MOVE_SPEED;
  } else if (targetX >= 0.0f && targetX <= SCREEN_WIDTH) {
    // No key held: steer toward the mouse column.
    player->velocity.x =
        clampf((targetX - player->position.x) * FOLLOW_K, -MOVE_SPEED, MOVE_SPEED);
  }
}

// Wrap x across the screen edges, shifting the trail by the same delta so it
// doesn't streak across the whole view on the wrap frame.
static void wrapHorizontal(Player *player) {
  float dx = 0.0f;
  if (player->position.x < 0.0f) {
    dx = SCREEN_WIDTH;
  } else if (player->position.x > SCREEN_WIDTH) {
    dx = -SCREEN_WIDTH;
  }
  if (dx != 0.0f) {
    player->position.x += dx;
    for (int i = 0; i < TRAIL_LEN; i++) {
      player->trail[i].x += dx;
    }
  }
}

void updatePlayer(Player *player) {
  player->velocity.x += player->acceleration.x;
  player->velocity.y += player->acceleration.y;
  player->position.x += player->velocity.x;
  player->position.y += player->velocity.y;

  player->velocity.x *= HORIZONTAL_DAMPING;

  wrapHorizontal(player);

  player->trail[player->trailHead] = player->position;
  player->trailHead = (player->trailHead + 1) % TRAIL_LEN;
}

// Menu demo: pick a new random target every ~1.5s, stroll toward it at a steady
// pace, wrap the edges, and bob gently at the floor line.
void updateAttract(Player *player) {
  static float timer = 0.0f;
  static float target = SCREEN_WIDTH / 2.0f;
  timer -= GetFrameTime();
  if (timer <= 0.0f) {
    target = (float)GetRandomValue(0, SCREEN_WIDTH);
    timer = 1.5f;
  }

  // Move toward the target, but stop within one step of it instead of
  // overshooting and flipping direction (which read as a sudden shake).
  float d = target - player->position.x;
  if (fabsf(d) <= ATTRACT_SPEED) {
    player->position.x = target;
    player->velocity.x = 0.0f;
  } else {
    player->velocity.x = (d > 0.0f ? 1.0f : -1.0f) * ATTRACT_SPEED;
    player->position.x += player->velocity.x;
  }

  player->position.y =
      FLOOR_Y - player->radius + sinf((float)GetTime() * 3.0f) * ATTRACT_BOB;

  player->trail[player->trailHead] = player->position;
  player->trailHead = (player->trailHead + 1) % TRAIL_LEN;
}

void drawPlayer(const Player *player) {
  float px = player->position.x;
  float py = player->position.y;
  float r = player->radius;

  // Squash & stretch: moving fast vertically stretches the blob tall & narrow.
  float st = clampf(player->velocity.y / 45.0f, -0.45f, 0.45f);
  float rx = r * (1.0f - 0.5f * st);
  float ry = r * (1.0f + 0.5f * st);

  // Motion trail: oldest (trailHead) faint & small, newest fuller.
  for (int k = 0; k < TRAIL_LEN; k++) {
    int idx = (player->trailHead + k) % TRAIL_LEN;
    float f = (float)(k + 1) / TRAIL_LEN;
    Vector2 tp = player->trail[idx];
    DrawEllipse(tp.x, tp.y, rx * f * 0.8f, ry * f * 0.8f,
                Fade(CANDY_PINK, 0.22f * f));
  }

  // Soft glow.
  DrawEllipse(px, py, rx * 1.25f, ry * 1.25f, Fade(CANDY_PINK, 0.25f));

  // Body (layered ellipses fake a gradient) + belly highlight.
  DrawEllipse(px, py, rx, ry, CANDY_PINK_DK);
  DrawEllipse(px, py - ry * 0.12f, rx * 0.82f, ry * 0.82f, CANDY_PINK);
  DrawEllipse(px, py + ry * 0.30f, rx * 0.5f, ry * 0.38f, BELLY);

  // Googly eyes; pupils lean toward the direction of motion.
  float ex = rx * 0.38f;
  float ey = -ry * 0.18f;
  float pupx = clampf(player->velocity.x, -3.0f, 3.0f) / 3.0f * (r * 0.09f);
  float pupy = clampf(player->velocity.y, -8.0f, 8.0f) / 8.0f * (r * 0.07f);
  DrawCircle(px - ex, py + ey, r * 0.2f, RAYWHITE);
  DrawCircle(px + ex, py + ey, r * 0.2f, RAYWHITE);
  DrawCircle(px - ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
  DrawCircle(px + ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
}
