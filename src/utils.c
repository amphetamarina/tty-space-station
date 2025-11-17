// Utility functions module
#include "utils.h"
#include <math.h>

uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

uint32_t blend_colors(uint32_t base, uint32_t overlay, double alpha) {
    if (alpha <= 0.0) {
        return base;
    }
    if (alpha >= 1.0) {
        return overlay;
    }
    uint8_t br = (base >> 16) & 0xFF;
    uint8_t bg = (base >> 8) & 0xFF;
    uint8_t bb = base & 0xFF;
    uint8_t or = (overlay >> 16) & 0xFF;
    uint8_t og = (overlay >> 8) & 0xFF;
    uint8_t ob = overlay & 0xFF;
    uint8_t rr = (uint8_t)((1.0 - alpha) * br + alpha * or);
    uint8_t rg = (uint8_t)((1.0 - alpha) * bg + alpha * og);
    uint8_t rb = (uint8_t)((1.0 - alpha) * bb + alpha * ob);
    return pack_color(rr, rg, rb);
}
