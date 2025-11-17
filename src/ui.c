// UI rendering module
#include "ui.h"
#include "utils.h"
#include "map.h"
#include "../include/font8x8_basic.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>

void draw_pixel(uint32_t *pixels, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
        return;
    }
    pixels[y * SCREEN_WIDTH + x] = color;
}

void draw_rect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    int endX = x + w;
    int endY = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (endX > SCREEN_WIDTH) {
        endX = SCREEN_WIDTH;
    }
    if (endY > SCREEN_HEIGHT) {
        endY = SCREEN_HEIGHT;
    }
    for (int yy = y; yy < endY; ++yy) {
        for (int xx = x; xx < endX; ++xx) {
            pixels[yy * SCREEN_WIDTH + xx] = color;
        }
    }
}

void draw_text(uint32_t *pixels, int x, int y, const char *text, uint32_t color) {
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char ch = *p;
        if (ch >= 128) {
            ch = '?';
        }
        for (int row = 0; row < 8; ++row) {
            unsigned char bits = font8x8_basic[ch][row];
            for (int col = 0; col < 8; ++col) {
                if (bits & (1 << col)) {
                    draw_pixel(pixels, x + col, y + row, color);
                }
            }
        }
        x += 8;
    }
}

int raw_longest_line(const char *text) {
    int max_len = 0;
    int current = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            if (current > max_len) {
                max_len = current;
            }
            current = 0;
        } else {
            current++;
        }
    }
    if (current > max_len) {
        max_len = current;
    }
    if (max_len <= 0) {
        max_len = 1;
    }
    return max_len;
}

int layout_text_lines(const char *text, int chars_per_line, char lines[][MAX_LAYOUT_LINES], int max_lines,
                      int *longest_line) {
    if (!text || max_lines <= 0) {
        if (longest_line) {
            *longest_line = 0;
        }
        return 0;
    }
    if (chars_per_line < 1) {
        chars_per_line = 1;
    }
    char temp[256];
    snprintf(temp, sizeof(temp), "%s", text);
    int count = 0;
    int longest = 0;
    char *segment = temp;
    while (segment && count < max_lines) {
        char *newline = strchr(segment, '\n');
        if (newline) {
            *newline = '\0';
        }
        size_t len = strlen(segment);
        size_t offset = 0;
        if (len == 0) {
            lines[count][0] = '\0';
            count++;
        }
        while (offset < len && count < max_lines) {
            size_t chunk = len - offset;
            if (chunk > (size_t)chars_per_line) {
                size_t break_pos = offset + chars_per_line;
                size_t last_space = break_pos;
                while (last_space > offset && segment[last_space] != ' ') {
                    last_space--;
                }
                if (last_space == offset) {
                    last_space = break_pos;
                }
                chunk = last_space - offset;
            }
            if (chunk >= sizeof(lines[0])) {
                chunk = sizeof(lines[0]) - 1;
            }
            memcpy(lines[count], segment + offset, chunk);
            lines[count][chunk] = '\0';
            if ((int)chunk > longest) {
                longest = (int)chunk;
            }
            offset += chunk;
            while (offset < len && segment[offset] == ' ') {
                offset++;
            }
            count++;
        }
        if (!newline) {
            break;
        }
        segment = newline + 1;
    }
    if (count == 0) {
        lines[0][0] = '\0';
        count = 1;
    }
    if (longest_line) {
        *longest_line = longest;
    }
    return count;
}

void render_minimap(uint32_t *pixels, const Game *game) {
    if (!game->map.tiles || !game->door_state) {
        return;  // Safety check for dynamic arrays
    }

    int scale = 4;
    int offsetX = 10;
    int offsetY = 10;
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            uint32_t color = tile_is_wall(game->map.tiles[y][x]) ? pack_color(40, 40, 40) : pack_color(25, 70, 25);
            if (game->door_state[y][x] != -1) {
                color = game->door_state[y][x] ? pack_color(70, 190, 90) : pack_color(70, 70, 160);
            }
            for (int yy = 0; yy < scale; ++yy) {
                for (int xx = 0; xx < scale; ++xx) {
                    draw_pixel(pixels, offsetX + x * scale + xx, offsetY + y * scale + yy, color);
                }
            }
        }
    }
    int px = (int)(game->player.x * scale);
    int py = (int)(game->player.y * scale);
    for (int yy = -2; yy <= 2; ++yy) {
        for (int xx = -2; xx <= 2; ++xx) {
            draw_pixel(pixels, offsetX + px + xx, offsetY + py + yy, pack_color(255, 255, 255));
        }
    }
}
