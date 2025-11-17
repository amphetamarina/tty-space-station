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

int layout_text_lines(const char *text, int chars_per_line, char lines[][MEMORY_TEXT], int max_lines,
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
    char temp[MEMORY_TEXT];
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
    if (!game->map.tiles || !game->door_state || !game->memory_map) {
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
            if (game->memory_map[y][x] >= 0) {
                color = pack_color(200, 120, 20);
            } else if (game->input.active && y == game->input.targetY && x == game->input.targetX) {
                color = pack_color(80, 180, 200);
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
    if (game->net.remote.active) {
        int rx = (int)(game->net.remote.x * scale);
        int ry = (int)(game->net.remote.y * scale);
        for (int yy = -2; yy <= 2; ++yy) {
            for (int xx = -2; xx <= 2; ++xx) {
                draw_pixel(pixels, offsetX + rx + xx, offsetY + ry + yy, pack_color(200, 80, 80));
            }
        }
    }
    for (int i = 0; i < game->furniture_count; ++i) {
        int fx = (int)(game->furniture[i].x * scale);
        int fy = (int)(game->furniture[i].y * scale);
        uint32_t iconColor = pack_color(180, 140, 90);
        switch (game->furniture[i].type) {
        case FURN_BED:
            iconColor = pack_color(100, 120, 200);
            break;
        case FURN_SOFA:
            iconColor = pack_color(180, 60, 60);
            break;
        case FURN_WARDROBE:
            iconColor = pack_color(150, 110, 70);
            break;
        default:
            break;
        }
        for (int yy = -1; yy <= 1; ++yy) {
            for (int xx = -1; xx <= 1; ++xx) {
                draw_pixel(pixels, offsetX + fx + xx, offsetY + fy + yy, iconColor);
            }
        }
    }
}

void render_memory_viewer(const Game *game, uint32_t *pixels) {
    if (!game->viewer_active || game->viewer_index < 0 || game->viewer_index >= game->memory_count) {
        return;
    }
    const MemoryEntry *entry = &game->memories[game->viewer_index];
    int boxW = SCREEN_WIDTH - 120;
    if (boxW < 400) {
        boxW = 400;
    }
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = 60;
    int textWidth = boxW - 40;
    int chars_per_line = clamp_int(textWidth / 8, 8, 60);
    char lines[MAX_LAYOUT_LINES][MEMORY_TEXT];
    int lineCount = layout_text_lines(entry->text, chars_per_line, lines, MAX_LAYOUT_LINES - 4, NULL);
    if (lineCount <= 0) {
        lineCount = 1;
        lines[0][0] = '\0';
    }
    int boxH = lineCount * 14 + 160;
    if (boxH > SCREEN_HEIGHT - 100) {
        boxH = SCREEN_HEIGHT - 100;
    }
    draw_rect(pixels, boxX - 4, boxY - 4, boxW + 8, boxH + 8, pack_color(5, 5, 15));
    draw_rect(pixels, boxX, boxY, boxW, boxH, pack_color(15, 15, 30));
    draw_rect(pixels, boxX + 4, boxY + 4, boxW - 8, boxH - 8, pack_color(25, 25, 60));
    char title[128];
    snprintf(title, sizeof(title), "Memory #%d at (%d, %d)", game->viewer_index + 1, entry->grid_x, entry->grid_y);
    draw_text(pixels, boxX + 20, boxY + 20, title, pack_color(255, 255, 255));
    for (int i = 0; i < lineCount; ++i) {
        draw_text(pixels, boxX + 20, boxY + 48 + i * 14, lines[i], pack_color(235, 220, 180));
    }
    int instructionsY = boxY + boxH - 40;
    if (game->viewer_delete_prompt) {
        draw_text(pixels, boxX + 20, instructionsY, "Delete memory? Y to confirm, N to cancel.",
                  pack_color(255, 180, 180));
    } else {
        draw_text(pixels, boxX + 20, instructionsY, "E edit   Delete remove   Esc close", pack_color(200, 210, 255));
    }
}

void render_npc_dialogue(const Game *game, uint32_t *pixels) {
    if (!game->dialogue_active || game->dialogue_npc_index < 0 || game->dialogue_npc_index >= game->npc_count) {
        return;
    }
    const NPCEntry *npc = &game->npcs[game->dialogue_npc_index];
    if (!npc->active) {
        return;
    }

    int boxW = SCREEN_WIDTH - 200;
    if (boxW < 360) {
        boxW = 360;
    }
    int boxH = 150;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = SCREEN_HEIGHT - boxH - 40;

    draw_rect(pixels, boxX - 4, boxY - 4, boxW + 8, boxH + 8, pack_color(5, 5, 15));
    draw_rect(pixels, boxX, boxY, boxW, boxH, pack_color(15, 15, 30));
    draw_rect(pixels, boxX + 4, boxY + 4, boxW - 8, boxH - 8, pack_color(25, 25, 60));

    char title[128];
    snprintf(title, sizeof(title), "%s says:", npc->name);
    draw_text(pixels, boxX + 20, boxY + 20, title, pack_color(255, 220, 150));

    int textWidth = boxW - 40;
    int chars_per_line = clamp_int(textWidth / 8, 8, 60);
    char lines[MAX_LAYOUT_LINES][MEMORY_TEXT];
    int lineCount = layout_text_lines(npc->dialogue, chars_per_line, lines, 5, NULL);
    if (lineCount <= 0) {
        lineCount = 1;
        lines[0][0] = '\0';
    }

    for (int i = 0; i < lineCount && i < 5; ++i) {
        draw_text(pixels, boxX + 20, boxY + 45 + i * 14, lines[i], pack_color(235, 220, 180));
    }

    draw_text(pixels, boxX + 20, boxY + boxH - 30, "Press E to continue", pack_color(200, 210, 255));
}

void gather_nearby(const Game *game, char lines[3][128]) {
    int count = 0;
    for (int i = 0; i < game->memory_count && count < 3; ++i) {
        double dx = game->memories[i].x - game->player.x;
        double dy = game->memories[i].y - game->player.y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist < 6.0) {
            snprintf(lines[count], sizeof(lines[count]), "Memory %d (%.1f) %s", i + 1, dist, game->memories[i].text);
            count++;
        }
    }
    for (int i = count; i < 3; ++i) {
        snprintf(lines[i], sizeof(lines[i]), "%s", "");
    }
}
