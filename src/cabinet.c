// Cabinet management module
#include "cabinet.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rebuild_cabinets(Game *game) {
    game->cabinet_count = 0;

#if DEBUG_MODE
    printf("[DEBUG] rebuild_cabinets: Starting scan (map size: %dx%d)\n",
           game->map.width, game->map.height);
#endif

    // Scan the map for cabinet markers ('C')
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            if (game->cabinet_count >= MAX_CABINETS) {
#if DEBUG_MODE
                printf("[DEBUG] rebuild_cabinets: MAX_CABINETS reached\n");
#endif
                return;
            }

            char marker = game->map.decor[y][x];
            if (marker == 'C' || marker == 'c') {
                CabinetEntry *entry = &game->cabinets[game->cabinet_count];
                entry->type = CABINET_SERVER;
                entry->grid_x = x;
                entry->grid_y = y;
                entry->x = x + 0.5;
                entry->y = y + 0.5;
                entry->name = "Server Cabinet";
                entry->terminal_index = game->cabinet_count; // Each cabinet gets its own terminal
                entry->texture_index = rand() % NUM_CABINET_TEXTURES; // Random texture variation

                // Initialize the corresponding terminal
                if (entry->terminal_index < MAX_TERMINALS) {
                    terminal_init(&game->terminals[entry->terminal_index]);
                }

                game->cabinet_count++;
            }
        }
    }

#if DEBUG_MODE
    printf("[DEBUG] rebuild_cabinets: Finished. Total cabinets found: %d\n", game->cabinet_count);
#endif
}

bool cabinet_blocks_position(const Game *game, double x, double y) {
    // Cabinets have a collision box (similar to furniture)
    const double CABINET_RADIUS = 0.3;

    for (int i = 0; i < game->cabinet_count; ++i) {
        const CabinetEntry *cab = &game->cabinets[i];
        double dx = x - cab->x;
        double dy = y - cab->y;
        double dist_sq = dx * dx + dy * dy;

        if (dist_sq < CABINET_RADIUS * CABINET_RADIUS) {
            return true;
        }
    }

    return false;
}

int find_cabinet_at(const Game *game, int gx, int gy) {
    // Note: Debug logging removed from this function as it's called many times per frame
    for (int i = 0; i < game->cabinet_count; ++i) {
        if (game->cabinets[i].grid_x == gx && game->cabinets[i].grid_y == gy) {
            return i;
        }
    }
    return -1;
}

void activate_cabinet(Game *game, int cabinet_index) {
    if (cabinet_index < 0 || cabinet_index >= game->cabinet_count) {
        return;
    }

    CabinetEntry *cabinet = &game->cabinets[cabinet_index];
    int term_idx = cabinet->terminal_index;

    if (term_idx < 0 || term_idx >= MAX_TERMINALS) {
        return;
    }

    Terminal *term = &game->terminals[term_idx];

    // If terminal is not active, spawn a shell
    if (!term->active) {
        if (!terminal_spawn_shell(term)) {
            fprintf(stderr, "Failed to spawn shell for cabinet\n");
            return;
        }
    }

    // Enter terminal mode
    game->terminal_mode = true;
    game->active_terminal = term_idx;
}
