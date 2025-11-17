// Display management module - wall-mounted monitors
#define _POSIX_C_SOURCE 200809L
#include "display.h"
#include "terminal.h"
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

    // Track which tiles are already part of a display
    bool processed[game->map.height][game->map.width];
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            processed[y][x] = false;
        }
    }

    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            char tile = game->map.tiles[y][x];
            if ((tile == 'D' || tile == 'd') && !processed[y][x]) {
                if (game->display_count >= MAX_DISPLAYS) {
#if DEBUG_MODE
                    printf("[DEBUG] Maximum displays (%d) reached, skipping display at (%d,%d)\n",
                           MAX_DISPLAYS, x, y);
#endif
                    continue;
                }

                // Display IS a wall - determine orientation based on adjacent floor tiles
                double normal_x = 0;
                double normal_y = 0;
                bool has_orientation = false;

                // Check which side has floor/open space - display faces that direction
                if (x > 0 && (game->map.tiles[y][x - 1] == '.' || game->map.tiles[y][x - 1] == 'S')) {
                    // Open space to the left, display faces left
                    normal_x = -1.0;
                    normal_y = 0.0;
                    has_orientation = true;
                } else if (x < game->map.width - 1 && (game->map.tiles[y][x + 1] == '.' || game->map.tiles[y][x + 1] == 'S')) {
                    // Open space to the right, display faces right
                    normal_x = 1.0;
                    normal_y = 0.0;
                    has_orientation = true;
                } else if (y > 0 && (game->map.tiles[y - 1][x] == '.' || game->map.tiles[y - 1][x] == 'S')) {
                    // Open space above, display faces up
                    normal_x = 0.0;
                    normal_y = -1.0;
                    has_orientation = true;
                } else if (y < game->map.height - 1 && (game->map.tiles[y + 1][x] == '.' || game->map.tiles[y + 1][x] == 'S')) {
                    // Open space below, display faces down
                    normal_x = 0.0;
                    normal_y = 1.0;
                    has_orientation = true;
                }

                if (!has_orientation) {
#if DEBUG_MODE
                    printf("[DEBUG] Display at (%d,%d) has no adjacent open space, skipping\n", x, y);
#endif
                    continue;
                }

                // Check for stacked displays - scan horizontally and vertically
                int stack_width = 1;
                int stack_height = 1;

                // Horizontal stacking (only if facing up/down)
                if (normal_y != 0) {
                    while (x + stack_width < game->map.width &&
                           (game->map.tiles[y][x + stack_width] == 'D' || game->map.tiles[y][x + stack_width] == 'd')) {
                        stack_width++;
                    }
                }

                // Vertical stacking (only if facing left/right)
                if (normal_x != 0) {
                    while (y + stack_height < game->map.height &&
                           (game->map.tiles[y + stack_height][x] == 'D' || game->map.tiles[y + stack_height][x] == 'd')) {
                        stack_height++;
                    }
                }

                DisplayEntry *display = &game->displays[game->display_count];
                display->grid_x = x;
                display->grid_y = y;
                display->x = x + stack_width * 0.5;  // Center of stacked display
                display->y = y + stack_height * 0.5;
                display->normal_x = normal_x;
                display->normal_y = normal_y;
                display->width = stack_width;
                display->height = stack_height;

                // Create unique name
                static char name_buf[64];
                if (stack_width > 1 || stack_height > 1) {
                    snprintf(name_buf, sizeof(name_buf), "Display-%d (%dx%d)",
                             game->display_count + 1, stack_width, stack_height);
                } else {
                    snprintf(name_buf, sizeof(name_buf), "Display-%d", game->display_count + 1);
                }
                display->name = strdup(name_buf);

                // Assign terminal (cycle through available terminals)
                display->terminal_index = game->display_count % MAX_TERMINALS;

                // Spawn the terminal immediately so it shows content on the display
                Terminal *term = &game->terminals[display->terminal_index];
                if (!term->active) {
                    if (!terminal_spawn_shell(term)) {
                        fprintf(stderr, "[WARN] Failed to spawn shell for display #%d\n", game->display_count + 1);
                    } else {
#if DEBUG_MODE
                        printf("[DEBUG] Spawned terminal %d for display #%d\n",
                               display->terminal_index, game->display_count + 1);
#endif
                    }
                }

                // Mark all tiles in this display as processed
                for (int dy = 0; dy < stack_height; dy++) {
                    for (int dx = 0; dx < stack_width; dx++) {
                        processed[y + dy][x + dx] = true;
                    }
                }

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
        const DisplayEntry *display = &game->displays[i];
        // Check if point is within display bounds
        if (grid_x >= display->grid_x && grid_x < display->grid_x + display->width &&
            grid_y >= display->grid_y && grid_y < display->grid_y + display->height) {
            return i;
        }
    }
    return -1;
}

void activate_display(Game *game, int display_index) {
    if (display_index < 0 || display_index >= game->display_count) {
        return;
    }

    const DisplayEntry *display = &game->displays[display_index];
    int term_idx = display->terminal_index;

    if (term_idx < 0 || term_idx >= MAX_TERMINALS) {
        return;
    }

    Terminal *term = &game->terminals[term_idx];

    // Spawn shell if not already active (shouldn't happen since we spawn at display creation)
    if (!term->active) {
        if (!terminal_spawn_shell(term)) {
            fprintf(stderr, "Failed to spawn shell for display\n");
            return;
        }
    }

    // Enter terminal mode (fullscreen)
    game->terminal_mode = true;
    game->active_terminal = term_idx;
}
