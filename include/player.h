#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"

#define TRAIL_LEN 8

typedef struct Player {
  Vector2 position;
  Vector2 velocity;
  Vector2 acceleration;
  float radius;
  Vector2 trail[TRAIL_LEN]; // recent positions for the motion trail
  int trailHead;            // next slot to write (also the oldest sample)
} Player;

Player createPlayer(void);
// Keyboard arrows steer directly; with no arrow held the blob follows targetX
// (the mouse column, in world x). Pass a sentinel outside the screen to ignore.
void handlePlayerInput(Player *player, float targetX);
void updatePlayer(Player *player);
void updateAttract(Player *player); // menu demo: wander horizontally + bob
void drawPlayer(const Player *player);

#endif // PLAYER_H
