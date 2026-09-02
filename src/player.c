#include "player.h"

#include "core.h"
#include "svg.h"
#include "tex.h"
#include "theme.h"

#include <math.h>

#define GRAVITY 0.9f
#define MOVE_SPEED 8.0f
#define HORIZONTAL_DAMPING 0.9f
#define PLAYER_RADIUS 34.0f
#define FOLLOW_K 0.18f     // mouse-follow steering stiffness
#define ATTRACT_SPEED 2.6f // menu demo wander pace
#define ATTRACT_BOB 10.0f  // menu demo vertical bob amplitude
#define CROUCH_FRAMES 26   // ~0.43s of pre-jump anticipation @60Hz
#define CROUCH_DEPTH 0.8f  // peak compression: height traded for width
#define POP_STRETCH 0.5f   // stretch on the release frame
#define SQUASH_DECAY 0.86f // per-step decay of squash back to a round blob

// ponytail: the selected shop outfit lives in a module global (mirrors how this
// file already reads gTheme), so createPlayer stays arg-free and its call sites
// do not grow cosmetic parameters. Defaults to the free Bubblegum outfit until
// main() applies saved progress. Literal colors because CANDY_PINK is runtime.
static Character gCharacter = {{255, 127, 176, 255},
                               {228, 70, 139, 255},
                               HAT_NONE,
                               GLS_NONE,
                               MOUTHWEAR_NONE};
void setPlayerCharacter(Character c) { gCharacter = c; }

Player createPlayer(void) {
  Player p = {
      .position = {SCREEN_WIDTH / 2.0f, FLOOR_Y - PLAYER_RADIUS},
      .velocity = {0.0f, 0.0f},
      .acceleration = {0.0f, GRAVITY},
      .radius = PLAYER_RADIUS,
      .trailHead = 0,
      .character = gCharacter,
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

// Keep the whole blob inside the side walls and stop outward movement on hit.
static void constrainHorizontal(Player *player) {
  float x =
      clampf(player->position.x, player->radius, SCREEN_WIDTH - player->radius);
  if (x != player->position.x) {
    player->velocity.x = 0.0f;
    player->position.x = x;
  }
}

void updatePlayer(Player *player) {
  player->velocity.x += player->acceleration.x;
  player->velocity.y += player->acceleration.y;
  player->position.x += player->velocity.x;
  player->position.y += player->velocity.y;

  player->velocity.x *= HORIZONTAL_DAMPING;
  player->squash *= SQUASH_DECAY; // the launch pop relaxes back to round

  constrainHorizontal(player);

  pushTrail(player);
}

// Start of a run: plant the blob on the grass and wind up. The 1px gap above
// the floor line is load-bearing — updateWorld's floor-bounce test (which is
// what actually launches the blob, particles and all) must stay false until the
// crouch releases. The snap also absorbs the attract blob's bob offset, on the
// frame the menu cuts away.
void beginJump(Player *player) {
  player->crouch = CROUCH_FRAMES;
  player->velocity = (Vector2){0.0f, 0.0f};
  player->position.y = FLOOR_Y - player->radius - 1.0f;
  player->squash = 0.0f;
  player->expression = EXPR_HAPPY;
}

// One crouch step. Ease-out, so the blob presses down fast and holds near full
// compression; the frame the counter empties it snaps tall instead, and the
// next physics step drops it onto the floor to launch.
void updateCrouch(Player *player) {
  float t = 1.0f - (float)(--player->crouch) / CROUCH_FRAMES; // 0 -> 1
  player->squash = CROUCH_DEPTH * t * (2.0f - t);
  if (player->crouch == 0) {
    player->squash = -POP_STRETCH;
  }
}

// Menu demo: pick a new random target every ~1.5s, stroll toward it at a steady
// pace between the side walls, and bob gently at the floor line.
void updateAttract(Player *player) {
  player->attractTimer -= GetFrameTime();
  if (player->attractTimer <= 0.0f) {
    player->attractTarget = (float)GetRandomValue(
        (int)player->radius, SCREEN_WIDTH - (int)player->radius);
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

// Accessory slot -> glyph. The NONE values have no row, so each positional
// table is indexed by `id - 1` and compile-checked against its enum.
static const SvgId HAT_SVG[] = {
    SVG_ACC_CAP,    SVG_ACC_CROWN,  SVG_ACC_BOW,  SVG_ACC_PARTY,
    SVG_ACC_WIZARD, SVG_ACC_FLOWER, SVG_ACC_HALO,
};
static const SvgId GLS_SVG[] = {
    SVG_ACC_GLASSES, SVG_ACC_SHADES,  SVG_ACC_HEARTS, SVG_ACC_STARS,
    SVG_ACC_MONOCLE, SVG_ACC_GOGGLES, SVG_ACC_VISOR,
};
static const SvgId MOUTH_SVG[] = {
    SVG_ACC_MUSTACHE,   SVG_ACC_PACIFIER, SVG_ACC_MASK,   SVG_ACC_LOLLIPOP,
    SVG_ACC_PARTY_HORN, SVG_ACC_FANGS,    SVG_ACC_BUBBLE,
};
STATIC_ASSERT(sizeof(HAT_SVG) / sizeof(HAT_SVG[0]) == HAT_COUNT - 1,
              hat_svg_table_complete);
STATIC_ASSERT(sizeof(GLS_SVG) / sizeof(GLS_SVG[0]) == GLS_COUNT - 1,
              gls_svg_table_complete);
STATIC_ASSERT(sizeof(MOUTH_SVG) / sizeof(MOUTH_SVG[0]) == MOUTHWEAR_COUNT - 1,
              mouth_svg_table_complete);

// Accessory box geometry, in multiples of the *deformed* half-extents so the
// art squashes and stretches with the body. The glyphs are authored where they
// want to sit inside their 24x24 viewBox, so these only set size and anchor:
// the glasses box width is what lands the lenses (authored at 12 +/- 5.8) on
// the eyes at +/- 0.38*rx.
#define HAT_W 2.2f
#define HAT_H 1.5f
// Hat anchor, tuned by eye: the hats sit ~0.17*ry into the crown. Raising it
// further floats them off the head; lowering it drops the cap's brim onto the
// eyes during a deep crouch (the eyes scale with the *undeformed* r, so they
// keep their size while ry shrinks under them).
#define HAT_Y 1.18f // box center above the blob's center, in ry
#define GLS_W 1.6f
#define GLS_H 1.15f
#define GLS_Y 0.18f // matches drawPlayer's eye line, `ey`
#define MOUTH_W 1.55f
#define MOUTH_H 1.0f
#define MOUTH_Y 0.35f // below center, over drawPlayer's procedural mouth

void drawBlobBody(float cx, float cy, float rx, float ry, Character c) {
  // Solid two-tone candy fill (dark base + lighter inner). No glow/rim/sheen —
  // a firm flat blob, not a glossy bubble.
  DrawEllipse(cx, cy, rx, ry, c.bodyDark);
  DrawEllipse(cx, cy - ry * 0.12f, rx * 0.82f, ry * 0.82f, c.body);
}

void drawAccessories(float cx, float cy, float rx, float ry, Character c) {
  // Fixed-color art draws tinted WHITE. Out-of-range slots draw nothing rather
  // than reading past their lookup table.
  if (c.mouthwear > MOUTHWEAR_NONE && c.mouthwear < MOUTHWEAR_COUNT) {
    drawSvgEx(MOUTH_SVG[c.mouthwear - 1], cx, cy + ry * MOUTH_Y, rx * MOUTH_W,
              ry * MOUTH_H, 0.0f, WHITE);
  }
  if (c.glasses > GLS_NONE && c.glasses < GLS_COUNT) {
    drawSvgEx(GLS_SVG[c.glasses - 1], cx, cy - ry * GLS_Y, rx * GLS_W,
              ry * GLS_H, 0.0f, WHITE);
  }
  if (c.hat > HAT_NONE && c.hat < HAT_COUNT) {
    drawSvgEx(HAT_SVG[c.hat - 1], cx, cy - ry * HAT_Y, rx * HAT_W, ry * HAT_H,
              0.0f, WHITE);
  }
}

void drawPlayer(const Player *player) {
  float px = player->position.x;
  float py = player->position.y;
  float r = player->radius;

  // Squash & stretch: moving fast vertically stretches the blob tall & narrow —
  // magnitude, not sign, so a rise stretches like a fall does. On top of that,
  // `squash` is the explicit pre-jump deform: >0 crouches (wide and low, feet
  // planted), <0 pops tall.
  float st = clampf(fabsf(player->velocity.y) / 45.0f, 0.0f, 0.45f);
  float sq = player->squash;
  float rx = r * (1.0f - 0.5f * st + 0.45f * sq);
  float ry = r * (1.0f + 0.5f * st - 0.45f * sq);
  py += 0.45f * r * sq; // keep the feet planted while the body deforms

  Character c = player->character;

  // Motion trail: oldest (trailHead) faint & small, newest fuller. Soft-edged
  // stamps rather than hard ellipses — the trail is the blob's afterimage, and
  // eight crisp outlines behind it read as eight blobs. Boxes run wider than
  // the old ellipses because TEX_PUFF's falloff puts its visible body well
  // inside its own box. The blob itself stays untouched.
  for (int k = 0; k < TRAIL_LEN; k++) {
    int idx = (player->trailHead + k) % TRAIL_LEN;
    float f = (float)(k + 1) / TRAIL_LEN;
    Vector2 tp = player->trail[idx];
    drawTex(TEX_PUFF, tp.x, tp.y, rx * f * 2.2f, ry * f * 2.2f, 0.0f,
            Fade(c.body, 0.5f * f));
  }

  drawBlobBody(px, py, rx, ry, c);

  // Cheek blush.
  Color blush = Fade(ColorBrightness(c.bodyDark, 0.2f), 0.4f);
  DrawCircle(px - rx * 0.5f, py + ry * 0.12f, r * 0.12f, blush);
  DrawCircle(px + rx * 0.5f, py + ry * 0.12f, r * 0.12f, blush);

  float ex = rx * 0.38f;
  float ey = -ry * 0.18f;
  if (player->expression == EXPR_DIZZY) {
    // Dizzy: spiral eyes — a white base with two counter-rotating gapped rings
    // per eye reads as a hypnotic swirl (primitive-only, no pupils).
    float spin = (float)GetTime() * 220.0f;
    for (int s = -1; s <= 1; s += 2) {
      float cx = px + s * ex;
      DrawCircle(cx, py + ey, r * 0.2f, RAYWHITE);
      float a = s > 0 ? spin : -spin; // eyes swirl opposite ways
      DrawRing((Vector2){cx, py + ey}, r * 0.12f, r * 0.16f, a, a + 300.0f, 16,
               BLACK);
      DrawRing((Vector2){cx, py + ey}, r * 0.04f, r * 0.08f, -a, -a + 300.0f,
               16, BLACK);
    }
  } else {
    // Googly eyes; pupils lean toward the direction of motion.
    float pupx = clampf(player->velocity.x, -3.0f, 3.0f) / 3.0f * (r * 0.09f);
    float pupy = clampf(player->velocity.y, -8.0f, 8.0f) / 8.0f * (r * 0.07f);
    DrawCircle(px - ex, py + ey, r * 0.2f, RAYWHITE);
    DrawCircle(px + ex, py + ey, r * 0.2f, RAYWHITE);
    DrawCircle(px - ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
    DrawCircle(px + ex + pupx, py + ey + pupy, r * 0.1f, BLACK);
    // Glossy catch-light on each pupil.
    DrawCircle(px - ex + pupx - r * 0.03f, py + ey + pupy - r * 0.03f,
               r * 0.035f, RAYWHITE);
    DrawCircle(px + ex + pupx - r * 0.03f, py + ey + pupy - r * 0.03f,
               r * 0.035f, RAYWHITE);
  }

  // Eyebrows above the eyes; inner ends tilt with the expression (HAPPY raised,
  // SAD angled down) to amplify the mouth's emotion.
  float browY = py + ey - r * 0.32f, bw = r * 0.16f, bth = r * 0.055f;
  float tilt = player->expression == EXPR_HAPPY ? -r * 0.07f
               : player->expression == EXPR_SAD ? r * 0.09f
                                                : 0.0f;
  DrawLineEx((Vector2){px - ex - bw, browY},
             (Vector2){px - ex + bw, browY + tilt}, bth, MOUTH);
  DrawLineEx((Vector2){px + ex + bw, browY},
             (Vector2){px + ex - bw, browY + tilt}, bth, MOUTH);

  // Mouth, below the eyes. DrawRing angle 0 is at 3 o'clock, +angle sweeps
  // clockwise (down); a smile is the bottom arc below its center, a frown the
  // top arc above its center.
  if (c.mouthwear <= MOUTHWEAR_NONE || c.mouthwear >= MOUTHWEAR_COUNT) {
    float mw = r * 0.34f, th = r * 0.09f; // half-width & line thickness
    switch (player->expression) {
    case EXPR_HAPPY:
      DrawRing((Vector2){px, py + ry * 0.16f}, mw - th, mw, 25, 155, 24, MOUTH);
      break;
    case EXPR_SAD:
      DrawRing((Vector2){px, py + ry * 0.52f}, mw - th, mw, 205, 335, 24,
               MOUTH);
      break;
    case EXPR_NEUTRAL:
      DrawLineEx((Vector2){px - mw * 0.7f, py + ry * 0.34f},
                 (Vector2){px + mw * 0.7f, py + ry * 0.34f}, th, MOUTH);
      break;
    case EXPR_DIZZY:
      // Dazed little open "o" mouth.
      DrawRing((Vector2){px, py + ry * 0.34f}, mw * 0.32f - th, mw * 0.32f, 0,
               360, 24, MOUTH);
      break;
    }
  }

  // Wearables sit over the completed face: glasses cover the eyes, mouth-wear
  // replaces the procedural mouth, and headwear stays on the crown.
  drawAccessories(px, py, rx, ry, c);
}
