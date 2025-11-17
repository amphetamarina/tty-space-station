#ifndef FURNITURE_H
#define FURNITURE_H

#include "types.h"
#include <stdbool.h>

// Furniture management functions
void rebuild_furniture(Game *game);
bool furniture_blocks_position(const Game *game, double x, double y);
bool furniture_blocks_point(const FurnitureEntry *entry, double px, double py);

#endif // FURNITURE_H
