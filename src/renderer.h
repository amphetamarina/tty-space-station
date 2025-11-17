#ifndef RENDERER_H
#define RENDERER_H

#include "types.h"
#include <stdint.h>

// Main rendering function
void render_scene(const Game *game, uint32_t *pixels, double *zbuffer);

// Sprite rendering functions
int render_furniture(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                     double *zbuffer);
int render_npcs(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                double *zbuffer);
void render_memory_plaques(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                            double *zbuffer);
void render_remote_player(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                           double *zbuffer);

#endif // RENDERER_H
