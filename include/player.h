#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"

#define TRAIL_LEN 8

// Wearable slots. The art is fixed-color SVG (see the SVG_ACC_* block in
// svg.h) and player.c maps these onto it, so adding one is a new enum value, a
// new glyph, and one table row. Each NONE value is first and has no glyph row.
typedef enum {
  HAT_NONE,
  HAT_CAP,
  HAT_CROWN,
  HAT_BOW,
  HAT_PARTY,
  HAT_WIZARD,
  HAT_FLOWER,
  HAT_HALO,
  HAT_COUNT,
} HatId;

typedef enum {
  GLS_NONE,
  GLS_ROUND,
  GLS_SHADES,
  GLS_HEARTS,
  GLS_STARS,
  GLS_MONOCLE,
  GLS_GOGGLES,
  GLS_VISOR,
  GLS_COUNT,
} GlassesId;

typedef enum {
  MOUTHWEAR_NONE,
  MOUTHWEAR_MUSTACHE,
  MOUTHWEAR_PACIFIER,
  MOUTHWEAR_MASK,
  MOUTHWEAR_LOLLIPOP,
  MOUTHWEAR_PARTY_HORN,
  MOUTHWEAR_FANGS,
  MOUTHWEAR_BUBBLE,
  MOUTHWEAR_COUNT,
} MouthwearId;

// Blob appearance. Kept separate from physics so the shop can compose colors
// and three wearable slots independently over the same body-drawing code.
typedef struct Character {
  Color body;              // main fill
  Color bodyDark;          // outline / gradient base
  unsigned char hat;       // HatId
  unsigned char glasses;   // GlassesId
  unsigned char mouthwear; // MouthwearId
} Character;

// Face mouth shape. Drives drawPlayer's mouth so future code can react to game
// events (bounce = happy, hit = sad, …) by setting player->expression.
typedef enum {
  EXPR_HAPPY, // smile (default)
  EXPR_NEUTRAL,
  EXPR_SAD,   // frown
  EXPR_DIZZY, // spiral eyes + open "o" mouth (Dizzy debuff)
} Expression;

typedef struct Player {
  Vector2 position;
  Vector2 velocity;
  Vector2 acceleration;
  float radius;
  Vector2 trail[TRAIL_LEN]; // recent positions for the motion trail
  int trailHead;            // next slot to write (also the oldest sample)
  Character character;      // colors drawPlayer renders with
  Expression expression;    // current face; drawPlayer renders the mouth
  float attractTimer;       // menu-demo: time until next wander target
  float attractTarget;      // menu-demo: x the blob is strolling toward
  int crouch;   // fixed-steps of pre-jump crouch left (play loop skips physics)
  float squash; // draw-only body deform: >0 crouching down, <0 popping tall
} Player;

Player createPlayer(void);
// Set the composed outfit createPlayer stamps onto new blobs and its UI accent.
void setPlayerCharacter(Character c);
// Keyboard arrows steer directly; with no arrow held the blob follows targetX
// (the mouse column, in world x). Pass a sentinel outside the screen to ignore.
// dir scales the steering direction: +1 normal, -1 to reverse it (Dizzy
// debuff).
void handlePlayerInput(Player *player, float targetX, float dir);
void updatePlayer(Player *player);
void updateAttract(Player *player); // menu demo: wander horizontally + bob
// Pre-jump anticipation. beginJump plants the blob on the floor and starts the
// crouch; while `crouch` > 0 the play loop calls updateCrouch instead of
// input+gravity. At 0 the pin lifts and world.c's floor bounce launches the
// blob on the next step — with its particles, shake and SFX.
void beginJump(Player *player);
void updateCrouch(Player *player);
void drawPlayer(const Player *player);

// The blob's two-tone body and its worn accessories, split out so the shop's
// previews show the same pieces the live blob does instead of re-deriving
// them. Both take the deformed half-extents (rx, ry) rather than a radius: the
// body squashes non-uniformly and the accessories have to ride that, or a hat
// detaches from the head mid-crouch.
void drawBlobBody(float cx, float cy, float rx, float ry, Character c);
void drawAccessories(float cx, float cy, float rx, float ry, Character c);

#endif // PLAYER_H
