// Game state management module
#include "game.h"
#include "map.h"
#include "memory.h"
#include "furniture.h"
#include "npc.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

void game_reset_memory(Game *game) {
    game->memory_count = 0;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            game->memory_map[y][x] = -1;
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
    game->chat_count = 0;
    game->chat_input_active = false;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    game->dialogue_active = false;
    game->dialogue_npc_index = -1;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            game->door_state[y][x] = (game->map.tiles[y][x] == 'D') ? 0 : -1;
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
    game_reset_memory(game);
    game->player.angle = 0.0;
    game->player.fov = FOV;
    game_pick_spawn(game);
    rebuild_furniture(game);
    rebuild_npcs(game);
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
    game->net.mode = NET_NONE;
    game->net.listen_fd = -1;
    game->net.peer_fd = -1;
    game->net.self_id = 1;
    snprintf(game->net.player_name, sizeof(game->net.player_name), "%s",
             getenv("POOM_PLAYER_NAME") ? getenv("POOM_PLAYER_NAME") : "Explorer");
    game->net.connected = false;
    game->net.remote.active = false;
    game->net.remote.name[0] = '\0';
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

void add_chat_message(Game *game, const char *sender, const char *text) {
    if (!sender || !*sender) {
        sender = "Player";
    }
    if (!text) {
        text = "";
    }
    if (game->chat_count >= CHAT_HISTORY) {
        memmove(&game->chat_log[0], &game->chat_log[1], sizeof(ChatMessage) * (CHAT_HISTORY - 1));
        game->chat_count = CHAT_HISTORY - 1;
    }
    ChatMessage *msg = &game->chat_log[game->chat_count++];
    snprintf(msg->sender, sizeof(msg->sender), "%s", sender);
    snprintf(msg->text, sizeof(msg->text), "%s", text);
    msg->ttl = 8.0;
}

bool begin_chat_input(Game *game) {
    if (game->chat_input_active || game->input.active || game->viewer_active) {
        return false;
    }
    game->chat_input_active = true;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    refresh_text_input(game);
    set_hud_message(game, "Chat: Enter to send, Esc to cancel.");
    return true;
}

void cancel_chat_input(Game *game) {
    if (!game->chat_input_active) {
        return;
    }
    game->chat_input_active = false;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    refresh_text_input(game);
}

void submit_chat_input(Game *game) {
    if (!game->chat_input_active) {
        return;
    }
    game->chat_input_active = false;
    refresh_text_input(game);
    if (game->chat_input_len == 0) {
        return;
    }
    const char *sender = game->net.player_name;
    const char *text = game->chat_input;
    if (game->net.mode == NET_CLIENT) {
        network_client_send_chat(game, text);
        set_hud_message(game, "Chat sent.");
    } else {
        add_chat_message(game, sender, text);
        network_host_send_chat(game, sender, text);
    }
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
}

void handle_chat_text(Game *game, const char *text) {
    if (!game->chat_input_active || !text) {
        return;
    }
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 32 && c < 127 && game->chat_input_len < CHAT_TEXT - 1) {
            game->chat_input[game->chat_input_len++] = (char)c;
            game->chat_input[game->chat_input_len] = '\0';
        }
        text++;
    }
}

void refresh_text_input(Game *game) {
    if (game->input.active || game->chat_input_active) {
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
