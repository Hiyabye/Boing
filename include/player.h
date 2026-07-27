#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"

#define TRAIL_LEN 8

// Blob appearance. Kept separate from physics so multiple characters are just
// different palettes over the same body-drawing code.
typedef struct Character {
  Color body;     // main fill
  Color bodyDark; // outline / gradient base
} Character;

// Face mouth shape. Drives drawPlayer's mouth so future code can react to game
// events (bounce = happy, hit = sad, …) by setting player->expression.
typedef enum {
  EXPR_HAPPY, // smile (default)
  EXPR_NEUTRAL,
  EXPR_SAD,   // frown
  EXPR_DIZZY, // spiral eyes + wavy mouth (Dizzy debuff)
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
} Player;

Player createPlayer(void);
// Set the palette createPlayer stamps onto new blobs (the selected shop skin).
void setPlayerSkin(Character c);
// Keyboard arrows steer directly; with no arrow held the blob follows targetX
// (the mouse column, in world x). Pass a sentinel outside the screen to ignore.
// dir scales the steering direction: +1 normal, -1 to reverse it (Dizzy
// debuff).
void handlePlayerInput(Player *player, float targetX, float dir);
void updatePlayer(Player *player);
void updateAttract(Player *player); // menu demo: wander horizontally + bob
void drawPlayer(const Player *player);

#endif // PLAYER_H
