#ifndef GAME_H
#define GAME_H

#include "types.h"

// Game initialization and state management
void game_init(Game *game);
int game_allocate_game_maps(Game *game, int width, int height);
void game_free_game_maps(Game *game);
void game_reset_state(Game *game);
void game_pick_spawn(Game *game);

// HUD message functions
void set_hud_message(Game *game, const char *msg);

// Terminal and cabinet functions
void game_init_terminals(Game *game);
void game_cleanup_terminals(Game *game);

#endif // GAME_H
