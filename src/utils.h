#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <stdint.h>

// Color utility functions
uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t blend_colors(uint32_t base, uint32_t overlay, double alpha);

// Furniture spec functions
const FurnitureSpec *furniture_spec(FurnitureType type);
FurnitureType furniture_type_from_char(char c);
double furniture_angle_for_marker(char marker);
const char *furniture_display_name(FurnitureType type);

// Furniture specs array
extern const FurnitureSpec furniture_specs[NUM_FURNITURE_TYPES];

// NPC spec functions
const NPCSpec *npc_spec(NPCType type);
NPCType npc_type_from_char(char c);
const char *npc_display_name(NPCType type);

// NPC specs array
#define NUM_NPC_TYPES (NPC_GHOST + 1)
extern const NPCSpec npc_specs[NUM_NPC_TYPES];

#endif // UTILS_H
