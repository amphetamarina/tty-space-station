#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <stdint.h>

// Color utility functions
uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t blend_colors(uint32_t base, uint32_t overlay, double alpha);

#endif // UTILS_H
