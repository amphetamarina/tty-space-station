// Display management module - wall-mounted monitors
#define _POSIX_C_SOURCE 200809L
#include "display.h"
#include "terminal.h"
#include "utils.h"
#include "map.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline bool is_display_tile(const Game *game, int x, int y) {
    if (!game || !game->map.tiles) {
        return false;
    }
    if (x < 0 || y < 0 || x >= game->map.width || y >= game->map.height) {
        return false;
    }
    char tile = game->map.tiles[y][x];
    return tile == 'D' || tile == 'd';
}

static inline bool is_open_space(const Game *game, int x, int y) {
    if (!game || !game->map.tiles) {
        return false;
    }
    if (x < 0 || y < 0 || x >= game->map.width || y >= game->map.height) {
        return false;
    }
    char tile = game->map.tiles[y][x];
    if (tile == 'D' || tile == 'd') {
        return false;
    }
    return !tile_is_wall(tile);
}

void rebuild_displays(Game *game) {
    if (!game || !game->map.tiles || !game->map.decor) {
        return;
    }

#if DEBUG_MODE
    printf("[DEBUG] rebuild_displays: Scanning map %dx%d for displays...\n",
           game->map.width, game->map.height);
#endif

    for (int i = 0; i < MAX_DISPLAYS; ++i) {
        if (game->displays[i].name) {
            free((void *)game->displays[i].name);
            game->displays[i].name = NULL;
        }
        game->displays[i].terminal_index = -1;
    }

    game->display_count = 0;
    bool terminal_used[MAX_TERMINALS] = {0};
    for (int i = 0; i < game->cabinet_count; ++i) {
        int idx = game->cabinets[i].terminal_index;
        if (idx >= 0 && idx < MAX_TERMINALS) {
            terminal_used[idx] = true;
        }
    }

    // Track which tiles are already part of a display
    bool processed[game->map.height][game->map.width];
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            processed[y][x] = false;
        }
    }

    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            if (is_display_tile(game, x, y) && !processed[y][x]) {
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
                if (is_open_space(game, x - 1, y)) {
                    // Open space to the left, display faces left
                    normal_x = -1.0;
                    normal_y = 0.0;
                    has_orientation = true;
                } else if (is_open_space(game, x + 1, y)) {
                    // Open space to the right, display faces right
                    normal_x = 1.0;
                    normal_y = 0.0;
                    has_orientation = true;
                } else if (is_open_space(game, x, y - 1)) {
                    // Open space above, display faces up
                    normal_x = 0.0;
                    normal_y = -1.0;
                    has_orientation = true;
                } else if (is_open_space(game, x, y + 1)) {
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
                    while (x + stack_width < game->map.width && is_display_tile(game, x + stack_width, y)) {
                        stack_width++;
                    }
                }

                // Vertical stacking (only if facing left/right)
                if (normal_x != 0) {
                    while (y + stack_height < game->map.height && is_display_tile(game, x, y + stack_height)) {
                        stack_height++;
                    }
                }

                int total_tiles = stack_width * stack_height;
                if (total_tiles < 4) {
                    for (int dy = 0; dy < stack_height; ++dy) {
                        for (int dx = 0; dx < stack_width; ++dx) {
                            processed[y + dy][x + dx] = true;
                        }
                    }
#if DEBUG_MODE
                    printf("[DEBUG] Display at (%d,%d) is only %dx%d (<4 tiles). Treating as regular wall.\n",
                           x, y, stack_width, stack_height);
#endif
                    continue;
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

                int terminal_slot = -1;
                for (int t = 0; t < MAX_TERMINALS; ++t) {
                    if (!terminal_used[t]) {
                        terminal_slot = t;
                        terminal_used[t] = true;
                        break;
                    }
                }

                // Create unique name
                static char name_buf[64];
                if (stack_width > 1 || stack_height > 1) {
                    snprintf(name_buf, sizeof(name_buf), "Display-%d (%dx%d)",
                             game->display_count + 1, stack_width, stack_height);
                } else {
                    snprintf(name_buf, sizeof(name_buf), "Display-%d", game->display_count + 1);
                }
                display->name = strdup(name_buf);

                display->terminal_index = terminal_slot;
                if (terminal_slot >= 0 && terminal_slot < MAX_TERMINALS) {
                    terminal_init(&game->terminals[terminal_slot]);
                } else {
#if DEBUG_MODE
                    printf("[DEBUG] No free terminal slot for display %s\n", name_buf);
#endif
                }

                // Mark all tiles in this display as processed
                for (int dy = 0; dy < stack_height; ++dy) {
                    for (int dx = 0; dx < stack_width; ++dx) {
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

    // Initialize terminal if not already active
    if (!term->active) {
        if (!terminal_spawn_shell(term)) {
            fprintf(stderr, "Failed to spawn shell for display\n");
            return;
        }
    }

    // Enter terminal mode
    game->terminal_mode = true;
    game->active_terminal = term_idx;
}
