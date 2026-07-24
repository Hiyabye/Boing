#include "player.h"

#include "core.h"
#include "theme.h"

#include <math.h>

#define GRAVITY 0.9f
#define MOVE_SPEED 8.0f
#define HORIZONTAL_DAMPING 0.9f
#define PLAYER_RADIUS 34.0f
#define FOLLOW_K 0.18f     // mouse-follow steering stiffness
#define ATTRACT_SPEED 2.6f // menu demo wander pace
#define ATTRACT_BOB 10.0f  // menu demo vertical bob amplitude

Player createPlayer(void) {
  Player p = {
      .position = {SCREEN_WIDTH / 2.0f, FLOOR_Y - PLAYER_RADIUS},
      .velocity = {0.0f, 0.0f},
      .acceleration = {0.0f, GRAVITY},
      .radius = PLAYER_RADIUS,
      .trailHead = 0,
      .character = {CANDY_PINK, CANDY_PINK_DK},
      .expression = EXPR_HAPPY,
      .attractTimer = 0.0f,
      .attractTarget = SCREEN_WIDTH / 2.0f,
  };
  for (int i = 0; i < TRAIL_LEN; i++) {
    p.trail[i] = p.position; // avoid a trail streaking from (0,0)
  }
  return p;
}

// Record the current position into the motion-trail ring buffer.
static void pushTrail(Player *player) {
  player->trail[player->trailHead] = player->position;
  player->trailHead = (player->trailHead + 1) % TRAIL_LEN;
}

void handlePlayerInput(Player *player, float targetX, float dir) {
  // dir is +1 normally, -1 while Dizzy — flips arrows and mouse-follow alike.
  if (IsKeyDown(KEY_LEFT)) {
    player->velocity.x = -MOVE_SPEED * dir;
  } else if (IsKeyDown(KEY_RIGHT)) {
    player->velocity.x = MOVE_SPEED * dir;
  } else if (targetX >= 0.0f && targetX <= SCREEN_WIDTH) {
    // No key held: steer toward the mouse column (away from it while reversed).
    player->velocity.x = clampf((targetX - player->position.x) * FOLLOW_K * dir,
                                -MOVE_SPEED, MOVE_SPEED);
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

  pushTrail(player);
}

// Menu demo: pick a new random target every ~1.5s, stroll toward it at a steady
// pace, wrap the edges, and bob gently at the floor line.
void updateAttract(Player *player) {
  player->attractTimer -= GetFrameTime();
  if (player->attractTimer <= 0.0f) {
    player->attractTarget = (float)GetRandomValue(0, SCREEN_WIDTH);
    player->attractTimer = 1.5f;
  }

  // Move toward the target, but stop within one step of it instead of
  // overshooting and flipping direction (which read as a sudden shake).
  float d = player->attractTarget - player->position.x;
  if (fabsf(d) <= ATTRACT_SPEED) {
    player->position.x = player->attractTarget;
    player->velocity.x = 0.0f;
  } else {
    player->velocity.x = (d > 0.0f ? 1.0f : -1.0f) * ATTRACT_SPEED;
    player->position.x += player->velocity.x;
  }

  player->position.y =
      FLOOR_Y - player->radius + sinf((float)GetTime() * 3.0f) * ATTRACT_BOB;

  pushTrail(player);
}

void drawPlayer(const Player *player) {
  float px = player->position.x;
  float py = player->position.y;
  float r = player->radius;

  // Squash & stretch: moving fast vertically stretches the blob tall & narrow.
  float st = clampf(player->velocity.y / 45.0f, -0.45f, 0.45f);
  float rx = r * (1.0f - 0.5f * st);
  float ry = r * (1.0f + 0.5f * st);

  Character c = player->character;

  // Motion trail: oldest (trailHead) faint & small, newest fuller.
  for (int k = 0; k < TRAIL_LEN; k++) {
    int idx = (player->trailHead + k) % TRAIL_LEN;
    float f = (float)(k + 1) / TRAIL_LEN;
    Vector2 tp = player->trail[idx];
    DrawEllipse(tp.x, tp.y, rx * f * 0.8f, ry * f * 0.8f,
                Fade(c.body, 0.22f * f));
  }

  // Body: solid two-tone candy fill (dark base + lighter inner). No glow/rim/
  // sheen — a firm flat blob, not a glossy bubble.
  DrawEllipse(px, py, rx, ry, c.bodyDark);
  DrawEllipse(px, py - ry * 0.12f, rx * 0.82f, ry * 0.82f, c.body);

  // Cheek blush.
  Color blush = Fade(ColorBrightness(c.bodyDark, 0.2f), 0.4f);
  DrawCircle(px - rx * 0.5f, py + ry * 0.12f, r * 0.12f, blush);
  DrawCircle(px + rx * 0.5f, py + ry * 0.12f, r * 0.12f, blush);

  // Googly eyes; pupils lean toward the direction of motion.
  float ex = rx * 0.38f;
  float ey = -ry * 0.18f;
  float pupx = clampf(player->velocity.x, -3.0f, 3.0f) / 3.0f * (r * 0.09f);
  float pupy = clampf(player->velocity.y, -8.0f, 8.0f) / 8.0f * (r * 0.07f);
  DrawCircle(px - ex, py + ey, r * 0.2f, RAYWHITE);
  DrawCircle(px + ex, py + ey, r * 0.2f, RAYWHITE);
  DrawCircle(px - ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
  DrawCircle(px + ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
  // Glossy catch-light on each pupil.
  DrawCircle(px - ex + pupx - r * 0.03f, py + ey + pupy - r * 0.03f, r * 0.035f,
             RAYWHITE);
  DrawCircle(px + ex + pupx - r * 0.03f, py + ey + pupy - r * 0.03f, r * 0.035f,
             RAYWHITE);

  // Eyebrows above the eyes; inner ends tilt with the expression (HAPPY raised,
  // SAD angled down) to amplify the mouth's emotion.
  float browY = py + ey - r * 0.32f, bw = r * 0.16f, bth = r * 0.055f;
  float tilt = player->expression == EXPR_HAPPY  ? -r * 0.07f
               : player->expression == EXPR_SAD  ? r * 0.09f
                                                 : 0.0f;
  DrawLineEx((Vector2){px - ex - bw, browY}, (Vector2){px - ex + bw, browY + tilt},
             bth, MOUTH);
  DrawLineEx((Vector2){px + ex + bw, browY}, (Vector2){px + ex - bw, browY + tilt},
             bth, MOUTH);

  // Mouth, below the eyes. DrawRing angle 0 is at 3 o'clock, +angle sweeps
  // clockwise (down); a smile is the bottom arc below its center, a frown the
  // top arc above its center.
  float mw = r * 0.34f, th = r * 0.09f; // half-width & line thickness
  switch (player->expression) {
  case EXPR_HAPPY:
    DrawRing((Vector2){px, py + ry * 0.16f}, mw - th, mw, 25, 155, 24, MOUTH);
    break;
  case EXPR_SAD:
    DrawRing((Vector2){px, py + ry * 0.52f}, mw - th, mw, 205, 335, 24, MOUTH);
    break;
  case EXPR_NEUTRAL:
    DrawLineEx((Vector2){px - mw * 0.7f, py + ry * 0.34f},
               (Vector2){px + mw * 0.7f, py + ry * 0.34f}, th, MOUTH);
    break;
  }
}
