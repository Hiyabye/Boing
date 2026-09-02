#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "raylib.h"

void drawMenuBackground(void);
void drawGameBackground(float cameraY);
// Near-camera theme details drawn after the world, kept to the screen edges so
// they add depth without hiding the player, hazards, or HUD.
void drawGameForeground(float cameraY);

// The seven tetromino footprints as (dx,dy) cells, all non-negative, so
// `origin + d*cell` is both where a cell is drawn and where it collides. The
// Falling Blocks backdrop rains them and world.c's platforms *are* them — the
// shared table is what keeps the pad you land on the shape you can see.
// Declared unsized deliberately (see the robustness convention in AGENTS.md):
// spelling the length here would make background.c's completeness assert
// measure this declaration instead of the table.
#define TETRO_COUNT 7
extern const signed char TETRO[][4][2];
// Four beveled cells of one shape, `origin` at its top-left cell.
void drawTetromino(float x, float y, float cell, int shape, Color c, float a);

#endif // BACKGROUND_H
