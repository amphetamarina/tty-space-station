#ifndef TEXTURE_H
#define TEXTURE_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

// Texture arrays
extern uint32_t wall_textures[NUM_WALL_TEXTURES][TEX_SIZE * TEX_SIZE];
extern uint32_t floor_textures[NUM_FLOOR_TEXTURES][TEX_SIZE * TEX_SIZE];
extern uint32_t ceiling_textures[NUM_CEIL_TEXTURES][TEX_SIZE * TEX_SIZE];
extern uint32_t door_texture[TEX_SIZE * TEX_SIZE];
extern uint32_t furniture_textures[NUM_FURNITURE_TYPES][TEX_SIZE * TEX_SIZE];

// Texture generation functions
void generate_wall_textures(void);
void generate_floor_textures(void);
void generate_ceiling_textures(void);
void generate_furniture_textures(void);

// Texture loading functions
bool load_texture_from_bmp(const char *path, uint32_t *target);
void load_custom_textures(void);

#endif // TEXTURE_H
