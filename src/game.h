#ifndef GAME_H
#define GAME_H

#include "types.h"

// Game initialization and state management
void game_init(Game *game);
int game_allocate_game_maps(Game *game, int width, int height);
void game_free_game_maps(Game *game);
void game_reset_memory(Game *game);
void game_pick_spawn(Game *game);

// HUD message functions
void set_hud_message(Game *game, const char *msg);

// Chat functions
void add_chat_message(Game *game, const char *sender, const char *text);
bool begin_chat_input(Game *game);
void cancel_chat_input(Game *game);
void submit_chat_input(Game *game);
void handle_chat_text(Game *game, const char *text);

// Text input management
void refresh_text_input(Game *game);

// NPC dialogue functions
void begin_npc_dialogue(Game *game, int npc_index);
void close_npc_dialogue(Game *game);

#endif // GAME_H
