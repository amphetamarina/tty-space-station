// Display management module - wall-mounted monitors
#define _POSIX_C_SOURCE 200809L
#include "display.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void rebuild_displays(Game *game) {
    if (!game || !game->map.tiles || !game->map.decor) {
        return;
    }

#if DEBUG_MODE
    printf("[DEBUG] rebuild_displays: Scanning map %dx%d for displays...\n",
           game->map.width, game->map.height);
#endif

    game->display_count = 0;

    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            char marker = game->map.decor[y][x];
            if (marker == 'D' || marker == 'd') {
                if (game->display_count >= MAX_DISPLAYS) {
#if DEBUG_MODE
                    printf("[DEBUG] Maximum displays (%d) reached, skipping display at (%d,%d)\n",
                           MAX_DISPLAYS, x, y);
#endif
                    continue;
                }

                // Find adjacent wall to determine normal direction
                double normal_x = 0;
                double normal_y = 0;
                bool found_wall = false;

                // Check cardinal directions for walls
                if (x > 0 && game->map.tiles[y][x - 1] == '1') {
                    // Wall to the left, display faces right
                    normal_x = 1.0;
                    normal_y = 0.0;
                    found_wall = true;
                } else if (x < game->map.width - 1 && game->map.tiles[y][x + 1] == '1') {
                    // Wall to the right, display faces left
                    normal_x = -1.0;
                    normal_y = 0.0;
                    found_wall = true;
                } else if (y > 0 && game->map.tiles[y - 1][x] == '1') {
                    // Wall above, display faces down
                    normal_x = 0.0;
                    normal_y = 1.0;
                    found_wall = true;
                } else if (y < game->map.height - 1 && game->map.tiles[y + 1][x] == '1') {
                    // Wall below, display faces up
                    normal_x = 0.0;
                    normal_y = -1.0;
                    found_wall = true;
                }

                if (!found_wall) {
#if DEBUG_MODE
                    printf("[DEBUG] Display at (%d,%d) has no adjacent wall, skipping\n", x, y);
#endif
                    continue;
                }

                DisplayEntry *display = &game->displays[game->display_count];
                display->grid_x = x;
                display->grid_y = y;
                display->x = x + 0.5;
                display->y = y + 0.5;
                display->normal_x = normal_x;
                display->normal_y = normal_y;

                // Create unique name
                static char name_buf[64];
                snprintf(name_buf, sizeof(name_buf), "Display-%d", game->display_count + 1);
                display->name = strdup(name_buf);

                // Assign terminal (cycle through available terminals)
                display->terminal_index = game->display_count % MAX_TERMINALS;

                game->display_count++;

#if DEBUG_MODE
                printf("[DEBUG] Created display #%d at grid(%d,%d) world(%.2f,%.2f) normal(%.2f,%.2f) terminal=%d\n",
                       game->display_count, x, y, display->x, display->y,
                       normal_x, normal_y, display->terminal_index);
#endif
            }
        }
    }

#if DEBUG_MODE
    printf("[DEBUG] rebuild_displays: Total displays created: %d\n", game->display_count);
#endif
}

int find_display_at(const Game *game, int grid_x, int grid_y) {
    if (!game) {
        return -1;
    }
    for (int i = 0; i < game->display_count; ++i) {
        if (game->displays[i].grid_x == grid_x && game->displays[i].grid_y == grid_y) {
            return i;
        }
    }
    return -1;
}
