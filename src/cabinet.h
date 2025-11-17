#ifndef CABINET_H
#define CABINET_H

#include "types.h"

// Cabinet management functions
void rebuild_cabinets(Game *game);
bool cabinet_blocks_position(const Game *game, double x, double y);
int find_cabinet_at(const Game *game, int gx, int gy);
void activate_cabinet(Game *game, int cabinet_index);

#endif // CABINET_H
