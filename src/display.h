#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

// Initialize displays from game state
void rebuild_displays(Game *game);

// Find display at grid position
int find_display_at(const Game *game, int grid_x, int grid_y);

#endif // DISPLAY_H
