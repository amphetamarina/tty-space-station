// Furniture management module
#include "furniture.h"
#include "utils.h"
#include <math.h>

void rebuild_furniture(Game *game) {
    game->furniture_count = 0;
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            if (game->furniture_count >= MAX_FURNITURE) {
                return;
            }
            char marker = game->map.decor[y][x];
            FurnitureType type = furniture_type_from_char(marker);
            if (type == FURN_NONE) {
                continue;
            }
            FurnitureEntry *entry = &game->furniture[game->furniture_count++];
            entry->type = type;
            entry->grid_x = x;
            entry->grid_y = y;
            entry->x = x + 0.5;
            entry->y = y + 0.5;
            entry->angle = furniture_angle_for_marker(marker);
        }
    }
}

bool furniture_blocks_point(const FurnitureEntry *entry, double px, double py) {
    if (!entry || entry->type <= FURN_NONE) {
        return false;
    }
    const FurnitureSpec *spec = furniture_spec(entry->type);
    if (!spec) {
        return false;
    }
    double dx = px - entry->x;
    double dy = py - entry->y;
    double cosA = cos(entry->angle);
    double sinA = sin(entry->angle);
    double localX = cosA * dx + sinA * dy;
    double localY = -sinA * dx + cosA * dy;
    double margin = 0.05;
    return fabs(localX) <= spec->half_width + margin && fabs(localY) <= spec->half_depth + margin;
}

bool furniture_blocks_position(const Game *game, double x, double y) {
    for (int i = 0; i < game->furniture_count; ++i) {
        if (furniture_blocks_point(&game->furniture[i], x, y)) {
            return true;
        }
    }
    return false;
}
