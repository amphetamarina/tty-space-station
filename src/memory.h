#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"
#include <stdbool.h>

// Memory loading and saving
void load_memories(Game *game);
void save_memories(const Game *game);

// Memory management
int add_memory_entry_at(Game *game, int gx, int gy, double nx, double ny, const char *text);
void edit_memory_entry(Game *game, int index, const char *text);
void delete_memory_internal(Game *game, int index, bool notify_network);
void delete_memory(Game *game, int index);

// Memory viewer functions
bool open_memory_view(Game *game, int index);
void close_memory_view(Game *game);
bool target_memory(const Game *game, int *index_out);

// Memory input functions
bool begin_memory_input(Game *game);
bool begin_memory_edit(Game *game, int index);
void cancel_memory_input(Game *game);
void finalize_memory_input(Game *game);
void handle_memory_text(Game *game, const char *text);
void memory_backspace(Game *game);

#endif // MEMORY_H
