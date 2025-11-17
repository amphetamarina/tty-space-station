// UI rendering module
#include "ui.h"
#include "utils.h"
#include "map.h"
#include "../include/font8x8_basic.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define HUD_BAR_HEIGHT 96
#define HUD_DIGIT_WIDTH 5
#define HUD_DIGIT_HEIGHT 7

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    bool loaded;
} HudSprite;

typedef enum {
    HUD_SPRITE_FACE = 0,
    HUD_SPRITE_HAND,
    HUD_SPRITE_KEYBOARD,
    HUD_SPRITE_AXE,
    HUD_SPRITE_DEPLOY,
    HUD_SPRITE_COUNT
} HudSpriteId;

static HudSprite hud_sprites[HUD_SPRITE_COUNT];
static bool hud_sprites_initialized = false;

static const char *hud_sprite_paths[HUD_SPRITE_COUNT] = {
    "assets/hud/face.bmp",
    "assets/hud/hand.bmp",
    "assets/hud/keyboard.bmp",
    "assets/hud/axe.bmp",
    "assets/hud/cabinet.bmp"
};

static const uint8_t hud_digit_glyphs[10][HUD_DIGIT_HEIGHT] = {
    {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x1F, 0x01, 0x01, 0x1F, 0x10, 0x10, 0x1F}, // 2
    {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x1F}, // 3
    {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01}, // 4
    {0x1F, 0x10, 0x10, 0x1F, 0x01, 0x01, 0x1F}, // 5
    {0x1F, 0x10, 0x10, 0x1F, 0x11, 0x11, 0x1F}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x1F}, // 8
    {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x1F}  // 9
};

static const char *tool_labels[NUM_HUD_TOOLS] = {
    "KEYBOARD",
    "AXE",
    "CABINET"
};

static bool load_hud_sprite(const char *path, HudSprite *sprite) {
    if (!path || !sprite) {
        return false;
    }
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        return false;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        return false;
    }
    if (converted->w <= 0 || converted->h <= 0) {
        SDL_FreeSurface(converted);
        return false;
    }
    size_t total = (size_t)converted->w * (size_t)converted->h;
    uint32_t *pixels = (uint32_t *)malloc(total * sizeof(uint32_t));
    if (!pixels) {
        SDL_FreeSurface(converted);
        return false;
    }
    uint32_t *src = (uint32_t *)converted->pixels;
    int pitch = converted->pitch / 4;
    for (int y = 0; y < converted->h; ++y) {
        for (int x = 0; x < converted->w; ++x) {
            pixels[y * converted->w + x] = src[y * pitch + x];
        }
    }
    int width = converted->w;
    int height = converted->h;
    SDL_FreeSurface(converted);
    sprite->pixels = pixels;
    sprite->width = width;
    sprite->height = height;
    sprite->loaded = true;
    return true;
}

static void ensure_hud_sprites_loaded(void) {
    if (hud_sprites_initialized) {
        return;
    }
    hud_sprites_initialized = true;
    for (int i = 0; i < HUD_SPRITE_COUNT; ++i) {
        load_hud_sprite(hud_sprite_paths[i], &hud_sprites[i]);
    }
}

static const HudSprite *get_hud_sprite(HudSpriteId id) {
    if (id < 0 || id >= HUD_SPRITE_COUNT) {
        return NULL;
    }
    if (!hud_sprites_initialized) {
        ensure_hud_sprites_loaded();
    }
    return hud_sprites[id].loaded ? &hud_sprites[id] : NULL;
}

static void blit_sprite(uint32_t *pixels, int dstX, int dstY, int dstW, int dstH, const HudSprite *sprite) {
    if (!pixels || !sprite || !sprite->pixels || dstW <= 0 || dstH <= 0) {
        return;
    }
    for (int y = 0; y < dstH; ++y) {
        int screenY = dstY + y;
        if (screenY < 0 || screenY >= SCREEN_HEIGHT) {
            continue;
        }
        int srcY = sprite->height ? (y * sprite->height) / dstH : 0;
        for (int x = 0; x < dstW; ++x) {
            int screenX = dstX + x;
            if (screenX < 0 || screenX >= SCREEN_WIDTH) {
                continue;
            }
            int srcX = sprite->width ? (x * sprite->width) / dstW : 0;
            uint32_t color = sprite->pixels[srcY * sprite->width + srcX];
            uint8_t alpha = (color >> 24) & 0xFF;
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            bool transparent = (alpha == 0);
            // Bright magenta fallback for editors that export without alpha
            if (r == 255 && g == 0 && b == 255) {
                transparent = true;
            }
            if (!transparent) {
                pixels[screenY * SCREEN_WIDTH + screenX] = color;
            }
        }
    }
}

static void draw_frame(uint32_t *pixels, int x, int y, int w, int h, uint32_t color) {
    for (int xx = x; xx < x + w; ++xx) {
        draw_pixel(pixels, xx, y, color);
        draw_pixel(pixels, xx, y + h - 1, color);
    }
    for (int yy = y; yy < y + h; ++yy) {
        draw_pixel(pixels, x, yy, color);
        draw_pixel(pixels, x + w - 1, yy, color);
    }
}

static void draw_digit_sprite(uint32_t *pixels, int x, int y, int digit, int scale, uint32_t baseColor) {
    if (digit < 0 || digit > 9) {
        return;
    }
    for (int row = 0; row < HUD_DIGIT_HEIGHT; ++row) {
        uint8_t bits = hud_digit_glyphs[digit][row];
        for (int col = 0; col < HUD_DIGIT_WIDTH; ++col) {
            if (bits & (1 << (HUD_DIGIT_WIDTH - 1 - col))) {
                for (int yy = 0; yy < scale; ++yy) {
                    for (int xx = 0; xx < scale; ++xx) {
                        draw_pixel(pixels, x + col * scale + xx, y + row * scale + yy, baseColor);
                    }
                }
            }
        }
    }
}

static void draw_keyboard_icon(uint32_t *pixels, int x, int y, int size, uint32_t color) {
    int padding = 6;
    int usableW = size - padding * 2;
    int usableH = size - padding * 2;
    int rows = 3;
    int cols = 4;
    int keyW = (usableW - (cols - 1) * 2) / cols;
    int keyH = (usableH - (rows - 1) * 3) / rows;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int keyX = x + padding + col * (keyW + 2);
            int keyY = y + padding + row * (keyH + 3);
            draw_rect(pixels, keyX, keyY, keyW, keyH, color);
        }
    }
}

static void draw_axe_icon(uint32_t *pixels, int x, int y, int size, uint32_t color) {
    int handleW = size / 8;
    int handleH = size - size / 4;
    int handleX = x + size / 2 - handleW / 2;
    int handleY = y + size / 6;
    draw_rect(pixels, handleX, handleY, handleW, handleH, color);

    int headW = size / 2;
    int headH = size / 4;
    int headX = handleX - headW / 2;
    int headY = handleY - headH / 2;
    draw_rect(pixels, headX, headY, headW, headH, color);

    // Cutout for axe edge
    uint32_t cutoutColor = pack_color(4, 8, 12);
    draw_rect(pixels, headX + headW - headW / 4, headY, headW / 4, headH, cutoutColor);
}

static void draw_cabinet_icon(uint32_t *pixels, int x, int y, int size, uint32_t color) {
    int bodyW = size - 12;
    int bodyH = size - 16;
    int bodyX = x + (size - bodyW) / 2;
    int bodyY = y + (size - bodyH) / 2;
    draw_rect(pixels, bodyX, bodyY, bodyW, bodyH, color);

    uint32_t slotColor = blend_colors(color, pack_color(0, 0, 0), 0.4);
    int slotH = 4;
    for (int i = 0; i < 3; ++i) {
        int slotY = bodyY + 6 + i * 10;
        draw_rect(pixels, bodyX + 4, slotY, bodyW - 8, slotH, slotColor);
    }

    // Indicator light
    draw_rect(pixels, bodyX + bodyW - 10, bodyY + bodyH - 12, 6, 6, pack_color(180, 40, 40));
}

static void draw_tool_icon(uint32_t *pixels, int x, int y, int size, HudToolType tool, bool active, bool available) {
    uint32_t baseColor = available ? pack_color(20, 28, 36) : pack_color(12, 12, 12);
    draw_rect(pixels, x, y, size, size, baseColor);

    uint32_t frameColor = active ? pack_color(230, 210, 140) : pack_color(80, 90, 120);
    draw_frame(pixels, x, y, size, size, frameColor);

    const HudSprite *custom = NULL;
    switch (tool) {
    case HUD_TOOL_KEYBOARD:
        custom = get_hud_sprite(HUD_SPRITE_KEYBOARD);
        break;
    case HUD_TOOL_AXE:
        custom = get_hud_sprite(HUD_SPRITE_AXE);
        break;
    case HUD_TOOL_DEPLOY:
    default:
        custom = get_hud_sprite(HUD_SPRITE_DEPLOY);
        break;
    }

    if (custom) {
        blit_sprite(pixels, x + 4, y + 4, size - 8, size - 8, custom);
        return;
    }

    uint32_t iconColor = available ? pack_color(200, 200, 200) : pack_color(80, 80, 80);
    switch (tool) {
    case HUD_TOOL_KEYBOARD:
        draw_keyboard_icon(pixels, x, y, size, iconColor);
        break;
    case HUD_TOOL_AXE:
        draw_axe_icon(pixels, x, y, size, iconColor);
        break;
    case HUD_TOOL_DEPLOY:
    default:
        draw_cabinet_icon(pixels, x, y, size, iconColor);
        break;
    }
}

static void draw_face_indicator(uint32_t *pixels, int x, int y, int size, int face_state) {
    const HudSprite *sprite = get_hud_sprite(HUD_SPRITE_FACE);
    if (sprite) {
        blit_sprite(pixels, x, y, size, size, sprite);
        draw_frame(pixels, x, y, size, size, pack_color(110, 120, 150));
        return;
    }

    uint32_t faceColor = pack_color(50, 60, 80);
    if (face_state == 1) {
        faceColor = pack_color(80, 90, 110);
    } else if (face_state == 2) {
        faceColor = pack_color(30, 40, 60);
    }
    draw_rect(pixels, x, y, size, size, faceColor);
    draw_frame(pixels, x, y, size, size, pack_color(110, 120, 150));

    int eyeSize = size / 6;
    int eyeY = y + size / 3;
    int eyeXOffset = size / 5;
    draw_rect(pixels, x + eyeXOffset, eyeY, eyeSize, eyeSize, pack_color(0, 0, 0));
    draw_rect(pixels, x + size - eyeXOffset - eyeSize, eyeY, eyeSize, eyeSize, pack_color(0, 0, 0));

    int mouthWidth = size / 2;
    int mouthX = x + (size - mouthWidth) / 2;
    int mouthY = y + size - size / 3;
    int moodHeight = (face_state == 1) ? 3 : (face_state == 2 ? 1 : 2);
    draw_rect(pixels, mouthX, mouthY, mouthWidth, moodHeight, pack_color(0, 0, 0));
}

static void draw_hand_overlay(uint32_t *pixels, const HudStatus *status, int handX, int handY, int handWidth, int handHeight) {
    if (!status) {
        return;
    }

    const HudSprite *toolSprite = NULL;
    switch (status->active_tool) {
    case HUD_TOOL_KEYBOARD:
        toolSprite = get_hud_sprite(HUD_SPRITE_KEYBOARD);
        break;
    case HUD_TOOL_AXE:
        toolSprite = get_hud_sprite(HUD_SPRITE_AXE);
        break;
    case HUD_TOOL_DEPLOY:
    default:
        toolSprite = get_hud_sprite(HUD_SPRITE_DEPLOY);
        break;
    }

    int spriteWidth = handWidth;
    int spriteHeight = handHeight;
    int spriteX = handX;
    int spriteY = handY;

    if (toolSprite) {
        blit_sprite(pixels, spriteX, spriteY, spriteWidth, spriteHeight, toolSprite);
        return;
    }

    const HudSprite *fallbackHand = get_hud_sprite(HUD_SPRITE_HAND);
    if (fallbackHand) {
        blit_sprite(pixels, spriteX, spriteY, spriteWidth, spriteHeight, fallbackHand);
        return;
    }

    uint32_t handColor = pack_color(150, 120, 80);
    if (status->active_tool == HUD_TOOL_AXE) {
        handColor = pack_color(140, 100, 70);
    } else if (status->active_tool == HUD_TOOL_DEPLOY) {
        handColor = pack_color(110, 130, 150);
    }

    draw_rect(pixels, spriteX, spriteY, spriteWidth, spriteHeight, handColor);
    draw_frame(pixels, spriteX, spriteY, spriteWidth, spriteHeight, pack_color(50, 30, 20));
}

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

void render_hud(uint32_t *pixels, const Game *game) {
    if (!game) {
        return;
    }
    const HudStatus *status = &game->hud_status;
    int barY = SCREEN_HEIGHT - HUD_BAR_HEIGHT;
    if (barY < 0) {
        barY = 0;
    }

    int handWidth = 210;
    int handHeight = 165;
    int handX = SCREEN_WIDTH / 2 - handWidth / 2;
    if (handX < 0) {
        handX = 0;
    }
    int handY = SCREEN_HEIGHT - handHeight - 30 - handHeight / 3;
    int bobOffset = (int)game->hud_bob_offset;
    handY -= bobOffset;
    if (handY < 0) {
        handY = 0;
    }
    draw_hand_overlay(pixels, status, handX, handY, handWidth, handHeight);

    draw_rect(pixels, 0, barY, SCREEN_WIDTH, HUD_BAR_HEIGHT, pack_color(6, 10, 16));
    draw_rect(pixels, 0, barY - 2, SCREEN_WIDTH, 2, pack_color(20, 30, 45));

    // Shell counter
    int shellsX = 20;
    draw_text(pixels, shellsX, barY + 10, "SHELLS", pack_color(160, 200, 255));
    int digitScale = 8;
    char buffer[8];
    int shells = status->shells;
    if (shells < 0) {
        shells = 0;
    }
    if (shells > 9999) {
        shells = 9999;
    }
    snprintf(buffer, sizeof(buffer), "%d", shells);
    int digitsLen = (int)strlen(buffer);
    for (int i = 0; i < digitsLen; ++i) {
        int digit = buffer[i] - '0';
        int digitX = shellsX + i * (HUD_DIGIT_WIDTH * digitScale + 6);
        int digitY = barY + 28;
        draw_digit_sprite(pixels, digitX, digitY, digit, digitScale, pack_color(240, 240, 200));
    }

    // Face indicator or spacer
    int faceSize = 80;
    int faceX = SCREEN_WIDTH / 2 - faceSize / 2;
    int faceY = barY;
    draw_face_indicator(pixels, faceX, faceY, faceSize, status->face_state);

    // Tool icons on the right
    int iconSize = 54;
    int iconPadding = 14;
    int panelWidth = NUM_HUD_TOOLS * iconSize + (NUM_HUD_TOOLS - 1) * iconPadding;
    int iconStartX = SCREEN_WIDTH - panelWidth - 20;
    int iconY = barY + 12;
    for (int i = 0; i < NUM_HUD_TOOLS; ++i) {
        int iconX = iconStartX + i * (iconSize + iconPadding);
        bool available = status->tools[i] > 0;
        bool active = (status->active_tool == i);
        draw_tool_icon(pixels, iconX, iconY, iconSize, (HudToolType)i, active, available);
        draw_text(pixels, iconX, iconY + iconSize + 6, tool_labels[i], pack_color(190, 190, 210));
    }

    // HUD message inside the bar
    if (game->hud_message[0]) {
        int msgY = barY - 22;
        if (msgY < 0) {
            msgY = 0;
        }
        draw_text(pixels, shellsX, msgY, game->hud_message, pack_color(240, 210, 160));
    }
}
