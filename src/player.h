#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include <stdbool.h>

// Player movement functions
void move_player(Game *game, double dx, double dy);
int can_move(const Game *game, double nx, double ny);
void normalize_angle(double *angle);

// Ray picking for wall interaction
bool ray_pick_wall(const Game *game, double angle, int *wallX, int *wallY, double *hitX, double *hitY,
                   double *normalX, double *normalY);

// Door functions
bool door_is_passable(const Game *game, int gx, int gy);
bool toggle_door_state(Game *game, int gx, int gy, bool notify);
bool interact_with_door(Game *game);

#endif // PLAYER_H
