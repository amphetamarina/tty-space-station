// Cabinet management module
#include "cabinet.h"
#include "terminal.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _GNU_SOURCE
extern char *strdup(const char *s);
#endif

static int find_free_terminal_slot(const Game *game) {
    bool used[MAX_TERMINALS] = {0};
    for (int i = 0; i < game->cabinet_count; ++i) {
        int idx = game->cabinets[i].terminal_index;
        if (idx >= 0 && idx < MAX_TERMINALS) {
            used[idx] = true;
        }
    }
    for (int i = 0; i < MAX_TERMINALS; ++i) {
        if (!used[i]) {
            return i;
        }
    }
    return -1;
}

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
#if DEBUG_MODE
                printf("[DEBUG] rebuild_cabinets: Found cabinet 'C' at decor[%d][%d] (grid %d,%d)\n",
                       y, x, x, y);
#endif
                CabinetEntry *entry = &game->cabinets[game->cabinet_count];
                entry->type = CABINET_SERVER;
                entry->grid_x = x;
                entry->grid_y = y;
                entry->x = x + 0.5;
                entry->y = y + 0.5;
                entry->name = "Server Cabinet";
                entry->terminal_index = find_free_terminal_slot(game);
                if (entry->terminal_index < 0) {
                    continue;
                }
                entry->texture_index = rand() % NUM_CABINET_TEXTURES; // Random texture variation
                entry->custom_name = NULL;
                entry->custom_color = 0;
                entry->has_custom_color = false;

                // Initialize the corresponding terminal
                if (entry->terminal_index < MAX_TERMINALS) {
                    terminal_init(&game->terminals[entry->terminal_index]);
                }

                game->cabinet_count++;
#if DEBUG_MODE
                printf("[DEBUG] rebuild_cabinets: Cabinet #%d added at (%d,%d)\n",
                       game->cabinet_count - 1, x, y);
#endif
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
#if DEBUG_MODE
    printf("[DEBUG] find_cabinet_at: Searching for cabinet at grid (%d,%d)\n", gx, gy);
    printf("[DEBUG] find_cabinet_at: Total cabinets to check: %d\n", game->cabinet_count);
#endif
    for (int i = 0; i < game->cabinet_count; ++i) {
#if DEBUG_MODE
        printf("[DEBUG] find_cabinet_at: Cabinet #%d is at grid (%d,%d)\n",
               i, game->cabinets[i].grid_x, game->cabinets[i].grid_y);
#endif
        if (game->cabinets[i].grid_x == gx && game->cabinets[i].grid_y == gy) {
#if DEBUG_MODE
            printf("[DEBUG] find_cabinet_at: MATCH! Found cabinet #%d\n", i);
#endif
            return i;
        }
    }
#if DEBUG_MODE
    printf("[DEBUG] find_cabinet_at: No cabinet found at (%d,%d)\n", gx, gy);
#endif
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

bool remove_cabinet(Game *game, int cabinet_index) {
    if (!game || cabinet_index < 0 || cabinet_index >= game->cabinet_count) {
        return false;
    }

    CabinetEntry *entry = &game->cabinets[cabinet_index];
    if (entry->grid_y >= 0 && entry->grid_y < game->map.height &&
        entry->grid_x >= 0 && entry->grid_x < game->map.width &&
        game->map.decor && game->map.decor[entry->grid_y]) {
        game->map.decor[entry->grid_y][entry->grid_x] = '\0';
    }

    if (entry->terminal_index >= 0 && entry->terminal_index < MAX_TERMINALS) {
        Terminal *term = &game->terminals[entry->terminal_index];
        if (term->active) {
            terminal_close(term);
        }
        terminal_init(term);
    }

    // Free custom name if allocated
    if (entry->custom_name) {
        free(entry->custom_name);
        entry->custom_name = NULL;
    }

    for (int i = cabinet_index; i < game->cabinet_count - 1; ++i) {
        game->cabinets[i] = game->cabinets[i + 1];
    }
    game->cabinet_count--;
    return true;
}

bool place_cabinet(Game *game, int gx, int gy) {
    if (!game || !game->map.tiles || !game->map.decor) {
        return false;
    }
    if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
        return false;
    }
    if (game->cabinet_count >= MAX_CABINETS) {
        return false;
    }
    char tile = game->map.tiles[gy][gx];
    if (tile_is_wall(tile) || tile == 'D' || tile == 'd') {
        return false;
    }
    if (find_cabinet_at(game, gx, gy) >= 0) {
        return false;
    }

    double cx = gx + 0.5;
    double cy = gy + 0.5;
    double dx = game->player.x - cx;
    double dy = game->player.y - cy;
    if ((dx * dx + dy * dy) < 0.4) {
        return false;
    }

    int terminal_slot = find_free_terminal_slot(game);
    if (terminal_slot < 0) {
        return false;
    }

    CabinetEntry *entry = &game->cabinets[game->cabinet_count];
    entry->type = CABINET_SERVER;
    entry->grid_x = gx;
    entry->grid_y = gy;
    entry->x = gx + 0.5;
    entry->y = gy + 0.5;
    entry->name = "Server Cabinet";
    entry->terminal_index = terminal_slot;
    entry->texture_index = rand() % NUM_CABINET_TEXTURES;
    entry->custom_name = NULL;
    entry->custom_color = 0;
    entry->has_custom_color = false;
    terminal_init(&game->terminals[entry->terminal_index]);
    game->map.decor[gy][gx] = 'C';
    game->cabinet_count++;
    return true;
}

void set_cabinet_custom_name(CabinetEntry *cabinet, const char *name) {
    if (!cabinet) {
        return;
    }

    // Free existing custom name if any
    if (cabinet->custom_name) {
        free(cabinet->custom_name);
        cabinet->custom_name = NULL;
    }

    // Set new custom name
    if (name && name[0] != '\0') {
        cabinet->custom_name = strdup(name);
    }
}

void set_cabinet_custom_color(CabinetEntry *cabinet, uint32_t color) {
    if (!cabinet) {
        return;
    }
    cabinet->custom_color = color;
    cabinet->has_custom_color = (color != 0);
}

const char* get_cabinet_display_name(const CabinetEntry *cabinet) {
    if (!cabinet) {
        return "Unknown";
    }
    return cabinet->custom_name ? cabinet->custom_name : cabinet->name;
}
