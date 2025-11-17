// Game state management module
#include "game.h"
#include "map.h"
#include "memory.h"
#include "furniture.h"
#include "npc.h"
#include "cabinet.h"
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

int game_allocate_game_maps(Game *game, int width, int height) {
    if (width < 10 || height < 10 || width > 200 || height > 200) {
        return 0;
    }

    // Allocate memory_map
    game->memory_map = (int **)malloc(height * sizeof(int *));
    if (!game->memory_map) {
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        game->memory_map[y] = (int *)malloc(width * sizeof(int));
        if (!game->memory_map[y]) {
            // Cleanup on failure
            for (int i = 0; i < y; ++i) {
                free(game->memory_map[i]);
            }
            free(game->memory_map);
            game->memory_map = NULL;
            return 0;
        }
    }

    // Allocate door_state
    game->door_state = (int **)malloc(height * sizeof(int *));
    if (!game->door_state) {
        // Cleanup memory_map on failure
        for (int y = 0; y < height; ++y) {
            free(game->memory_map[y]);
        }
        free(game->memory_map);
        game->memory_map = NULL;
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
            for (int i = 0; i < height; ++i) {
                free(game->memory_map[i]);
            }
            free(game->memory_map);
            game->memory_map = NULL;
            return 0;
        }
    }

    return 1;
}

void game_free_game_maps(Game *game) {
    if (game->memory_map) {
        for (int y = 0; y < game->map.height; ++y) {
            if (game->memory_map[y]) {
                free(game->memory_map[y]);
            }
        }
        free(game->memory_map);
        game->memory_map = NULL;
    }

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

void game_reset_memory(Game *game) {
    game->memory_count = 0;
    if (game->memory_map) {
        for (int y = 0; y < game->map.height; ++y) {
            for (int x = 0; x < game->map.width; ++x) {
                game->memory_map[y][x] = -1;
            }
        }
    }
    game->input.active = false;
    game->input.length = 0;
    game->input.buffer[0] = '\0';
    game->input.editing = false;
    game->input.edit_index = -1;
    game->hud_message[0] = '\0';
    game->hud_message_timer = 0.0;
    game->viewer_active = false;
    game->viewer_index = -1;
    game->viewer_delete_prompt = false;
    game->dialogue_active = false;
    game->dialogue_npc_index = -1;
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
    game->memory_map = NULL;
    game->door_state = NULL;

    const char *custom_map = getenv("POOM_MAP_FILE");
    const char *custom_save = getenv("POOM_SAVE_FILE");
    const char *generated_out = getenv("POOM_GENERATED_MAP");
    const char *map_source = NULL;
    if (custom_map && load_map_from_file(custom_map, &game->map)) {
        map_source = custom_map;
    } else if (load_map_from_file(MAP_FILE_DEFAULT, &game->map)) {
        map_source = MAP_FILE_DEFAULT;
    } else {
        map_generate(&game->map);
        if (generated_out && *generated_out) {
            map_save_to_file(&game->map, generated_out);
            map_source = generated_out;
        }
    }

    // Allocate game maps based on loaded map dimensions
    if (!game_allocate_game_maps(game, game->map.width, game->map.height)) {
        // Failed to allocate - cleanup and exit
        map_free(&game->map);
        return;
    }

    game_reset_memory(game);
    game->player.angle = 0.0;
    game->player.fov = FOV;
    game_pick_spawn(game);
    game_init_terminals(game);
    rebuild_furniture(game);
    rebuild_npcs(game);
    rebuild_cabinets(game);
    if (custom_save && *custom_save) {
        snprintf(game->save_path, sizeof(game->save_path), "%s", custom_save);
        game->has_save_path = true;
        load_memories(game);
    } else if (map_source) {
        snprintf(game->save_path, sizeof(game->save_path), "%s.mem", map_source);
        game->has_save_path = true;
        load_memories(game);
    } else {
        game->has_save_path = false;
    }
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

void refresh_text_input(Game *game) {
    if (game->input.active) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
}

void begin_npc_dialogue(Game *game, int npc_index) {
    if (npc_index < 0 || npc_index >= game->npc_count) {
        return;
    }
    if (!game->npcs[npc_index].active) {
        return;
    }
    game->dialogue_active = true;
    game->dialogue_npc_index = npc_index;
}

void close_npc_dialogue(Game *game) {
    game->dialogue_active = false;
    game->dialogue_npc_index = -1;
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
