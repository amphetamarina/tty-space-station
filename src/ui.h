#ifndef UI_H
#define UI_H

#include "types.h"
#include <stdint.h>

// Basic drawing functions
void draw_pixel(uint32_t *pixels, int x, int y, uint32_t color);
void draw_rect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color);
void draw_text(uint32_t *pixels, int x, int y, const char *text, uint32_t color);

// Text layout functions
int raw_longest_line(const char *text);
int layout_text_lines(const char *text, int chars_per_line, char lines[][MAX_LAYOUT_LINES], int max_lines,
                      int *longest_line);

// UI rendering functions
void render_minimap(uint32_t *pixels, const Game *game);
void render_hud(uint32_t *pixels, const Game *game);

#endif // UI_H
