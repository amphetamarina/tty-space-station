#ifndef MAP_H
#define MAP_H

#include "types.h"
#include <stdbool.h>

// Map initialization and management
void map_init(Map *map);
void map_enforce_border(Map *map);
void map_store_char(Map *map, int x, int y, char raw);
void map_generate(Map *map);
void map_save_to_file(const Map *map, const char *path);
int load_map_from_file(const char *path, Map *map);
void map_apply_wall_styles(Map *map);

// Map utility functions
char sanitize_tile(char c);
int tile_texture_index(char tile);
int floor_index_for_char(char tile);
bool tile_is_wall(char tile);
char map_export_char(const Map *map, int x, int y);

#endif // MAP_H
