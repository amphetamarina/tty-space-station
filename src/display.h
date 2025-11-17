#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"

// Initialize displays from game state
void rebuild_displays(Game *game);

// Find display at grid position
int find_display_at(const Game *game, int grid_x, int grid_y);

// Activate display (enter terminal mode)
void activate_display(Game *game, int display_index);

#endif // DISPLAY_H
