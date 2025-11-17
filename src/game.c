// Game state management module
#include "game.h"
#include "map.h"
#include "cabinet.h"
#include "display.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

int game_allocate_game_maps(Game *game, int width, int height) {
    if (width < 10 || height < 10 || width > 200 || height > 200) {
        return 0;
    }

    // Allocate door_state
    game->door_state = (int **)malloc(height * sizeof(int *));
    if (!game->door_state) {
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        game->door_state[y] = (int *)malloc(width * sizeof(int));
        if (!game->door_state[y]) {
            // Cleanup on failure
            for (int i = 0; i < y; ++i) {
                free(game->door_state[i]);
            }
            free(game->door_state);
            game->door_state = NULL;
            return 0;
        }
    }

    return 1;
}

void game_free_game_maps(Game *game) {
    if (game->door_state) {
        for (int y = 0; y < game->map.height; ++y) {
            if (game->door_state[y]) {
                free(game->door_state[y]);
            }
        }
        free(game->door_state);
        game->door_state = NULL;
    }
}

void game_reset_state(Game *game) {
    game->hud_message[0] = '\0';
    game->hud_message_timer = 0.0;
    if (game->door_state) {
        for (int y = 0; y < game->map.height; ++y) {
            for (int x = 0; x < game->map.width; ++x) {
                game->door_state[y][x] = (game->map.tiles[y][x] == 'D') ? 0 : -1;
            }
        }
    }
}

void game_pick_spawn(Game *game) {
    const Map *map = &game->map;
    if (map->spawn_set) {
        game->player.x = map->spawn_x + 0.5;
        game->player.y = map->spawn_y + 0.5;
        return;
    }
    for (int y = 1; y < map->height - 1; ++y) {
        for (int x = 1; x < map->width - 1; ++x) {
            if (map->tiles[y][x] == '.') {
                game->player.x = x + 0.5;
                game->player.y = y + 0.5;
                return;
            }
        }
    }
    game->player.x = 2.5;
    game->player.y = 2.5;
}

void game_init(Game *game) {
    memset(game, 0, sizeof(*game));

    // Initialize pointers to NULL
    game->map.tiles = NULL;
    game->map.decor = NULL;
    game->door_state = NULL;

    const char *custom_map = getenv("TSS_MAP_FILE");
    const char *generated_out = getenv("TSS_GENERATED_MAP");
    if (custom_map && load_map_from_file(custom_map, &game->map)) {
        // Loaded custom map
    } else if (load_map_from_file(MAP_FILE_DEFAULT, &game->map)) {
        // Loaded default map
    } else {
        map_generate(&game->map);
        if (generated_out && *generated_out) {
            map_save_to_file(&game->map, generated_out);
        }
    }

    // Allocate game maps based on loaded map dimensions
    if (!game_allocate_game_maps(game, game->map.width, game->map.height)) {
        // Failed to allocate - cleanup and exit
        map_free(&game->map);
        return;
    }

    game_reset_state(game);
    game->player.angle = 0.0;
    game->player.fov = FOV;
    game_pick_spawn(game);
    game_init_terminals(game);
    rebuild_cabinets(game);
    rebuild_displays(game);
}

void set_hud_message(Game *game, const char *msg) {
    if (!msg) {
        game->hud_message[0] = '\0';
        game->hud_message_timer = 0.0;
        return;
    }
    snprintf(game->hud_message, sizeof(game->hud_message), "%s", msg);
    game->hud_message_timer = 3.0;
}

void game_init_terminals(Game *game) {
    // Initialize all terminals
    for (int i = 0; i < MAX_TERMINALS; i++) {
        terminal_init(&game->terminals[i]);
    }
    game->terminal_mode = false;
    game->active_terminal = -1;
    game->cabinet_count = 0;
}

void game_cleanup_terminals(Game *game) {
    // Close all active terminals
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (game->terminals[i].active) {
            terminal_close(&game->terminals[i]);
        }
    }
    game->terminal_mode = false;
    game->active_terminal = -1;
}
