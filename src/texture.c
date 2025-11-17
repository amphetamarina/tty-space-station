// Texture generation and loading module
#include "texture.h"
#include "utils.h"
#include <SDL2/SDL.h>
#include <stdio.h>

// Texture arrays
uint32_t wall_textures[NUM_WALL_TEXTURES][TEX_SIZE * TEX_SIZE];
uint32_t floor_textures[NUM_FLOOR_TEXTURES][TEX_SIZE * TEX_SIZE];
uint32_t ceiling_textures[NUM_CEIL_TEXTURES][TEX_SIZE * TEX_SIZE];
uint32_t door_texture[TEX_SIZE * TEX_SIZE];
uint32_t furniture_textures[NUM_FURNITURE_TYPES][TEX_SIZE * TEX_SIZE];
uint32_t cabinet_texture[TEX_SIZE * TEX_SIZE];
uint32_t sky_texture[SKY_TEXTURE_HEIGHT * SKY_TEXTURE_WIDTH];
uint32_t display_texture[TEX_SIZE * TEX_SIZE];

void generate_wall_textures(void) {
    for (int t = 0; t < NUM_WALL_TEXTURES; ++t) {
        for (int y = 0; y < TEX_SIZE; ++y) {
            for (int x = 0; x < TEX_SIZE; ++x) {
                uint32_t color = 0;
                if (t == 0) {
                    int checker = ((x / 8) + (y / 8)) & 1;
                    color = checker ? pack_color(90, 90, 120) : pack_color(140, 140, 180);
                } else if (t == 1) {
                    int stripe = (x % 16 < 8);
                    color = stripe ? pack_color(80, 120, 80) : pack_color(50, 80, 50);
                } else {
                    int brick = ((x % 32) < 28 && (y % 16) < 14);
                    color = brick ? pack_color(120, 60, 50) : pack_color(50, 25, 20);
                }
                wall_textures[t][y * TEX_SIZE + x] = color;
            }
        }
    }
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int stripe = (x % 8 < 4);
            door_texture[y * TEX_SIZE + x] = stripe ? pack_color(130, 90, 40) : pack_color(160, 110, 60);
        }
    }
}

void generate_floor_textures(void) {
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int checker = (x / 8 + y / 8) & 1;
            uint32_t stone = checker ? pack_color(70, 60, 50) : pack_color(90, 80, 70);
            floor_textures[0][y * TEX_SIZE + x] = stone;

            double noise = sin((x + 1) * 0.4) + cos((y + 3) * 0.3);
            uint8_t color = (uint8_t)(100 + noise * 30);
            floor_textures[1][y * TEX_SIZE + x] = pack_color(color, color, color + 20);

            double marble = sin((x + y) * 0.2) + cos((x - y) * 0.25);
            uint8_t marbleColor = (uint8_t)(150 + marble * 40);
            floor_textures[2][y * TEX_SIZE + x] = pack_color(marbleColor, marbleColor, marbleColor + 30);
        }
    }
}

void generate_ceiling_textures(void) {
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            double swirl = sin((x + y) * 0.15);
            uint8_t base = (uint8_t)(40 + swirl * 20);
            ceiling_textures[0][y * TEX_SIZE + x] = pack_color(base, base + 10, base + 20);

            double stripe = (sin(x * 0.3) + 1.0) * 0.5;
            uint8_t c = (uint8_t)(80 + stripe * 40);
            ceiling_textures[1][y * TEX_SIZE + x] = pack_color(c, c, c + 30);
        }
    }
}

void generate_furniture_textures(void) {
    for (int t = 0; t < NUM_FURNITURE_TYPES; ++t) {
        if (t == FURN_NONE) {
            continue;
        }
        const FurnitureSpec *spec = furniture_spec((FurnitureType)t);
        uint32_t primary = spec && spec->primary_color ? spec->primary_color : pack_color(120, 120, 120);
        uint32_t detail = spec && spec->detail_color ? spec->detail_color : pack_color(180, 180, 180);
        for (int y = 0; y < TEX_SIZE; ++y) {
            for (int x = 0; x < TEX_SIZE; ++x) {
                double gradient = (double)y / (double)TEX_SIZE;
                double noise = ((x ^ y) & 7) / 8.0;
                double mix = gradient * 0.5 + noise * 0.2;
                furniture_textures[t][y * TEX_SIZE + x] = blend_colors(primary, detail, mix);
            }
        }
    }
}

void generate_cabinet_texture(void) {
    // Server cabinet - dark metallic look with panel details
    uint32_t metal = pack_color(60, 60, 80);
    uint32_t panel = pack_color(40, 40, 50);
    uint32_t light = pack_color(80, 180, 100);  // Green indicator lights

    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            uint32_t color = metal;

            // Panel sections
            if (y % 16 < 2) {
                color = panel;
            }

            // Indicator lights (small green dots)
            if (x > TEX_SIZE / 4 && x < TEX_SIZE / 4 + 3 && y % 16 == 8) {
                color = light;
            }

            // Metallic grain
            double noise = ((x * 7 + y * 11) % 13) / 13.0;
            uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * (0.9 + noise * 0.1));
            uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * (0.9 + noise * 0.1));
            uint8_t b = (uint8_t)((color & 0xFF) * (0.9 + noise * 0.1));

            cabinet_texture[y * TEX_SIZE + x] = pack_color(r, g, b);
        }
    }
}

void generate_sky_texture(void) {
    // Space station aesthetic - starfield with nebula gradients
    for (int y = 0; y < SKY_TEXTURE_HEIGHT; ++y) {
        for (int x = 0; x < SKY_TEXTURE_WIDTH; ++x) {
            // Vertical gradient - darker at top, lighter purple/blue at horizon
            double v_gradient = (double)y / (double)SKY_TEXTURE_HEIGHT;

            // Base color - deep space blue/purple
            uint8_t base_r = (uint8_t)(10 + v_gradient * 30);
            uint8_t base_g = (uint8_t)(5 + v_gradient * 20);
            uint8_t base_b = (uint8_t)(25 + v_gradient * 40);

            // Add some nebula-like variation using pseudo-random noise
            unsigned int seed = (unsigned int)(x * 7 + y * 13);
            seed = (seed * 1103515245u + 12345u) & 0x7fffffff;
            double noise = (seed % 256) / 256.0;

            // Stars - random bright pixels
            if (noise > 0.98) {
                uint8_t brightness = (uint8_t)(200 + noise * 55);
                sky_texture[y * SKY_TEXTURE_WIDTH + x] = pack_color(brightness, brightness, brightness);
            } else {
                // Apply subtle color variation
                uint8_t r = (uint8_t)(base_r + noise * 15);
                uint8_t g = (uint8_t)(base_g + noise * 10);
                uint8_t b = (uint8_t)(base_b + noise * 20);
                sky_texture[y * SKY_TEXTURE_WIDTH + x] = pack_color(r, g, b);
            }
        }
    }
}

void generate_display_texture(void) {
    // Monitor/display frame - dark metallic with screen bezel
    uint32_t frame = pack_color(40, 40, 50);     // Dark gray frame
    uint32_t screen = pack_color(10, 15, 20);    // Very dark screen
    uint32_t bezel = pack_color(60, 60, 70);     // Lighter bezel edge

    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            uint32_t color = frame;

            // Outer bezel (2 pixel border)
            if (x < 2 || x >= TEX_SIZE - 2 || y < 2 || y >= TEX_SIZE - 2) {
                color = bezel;
            }
            // Inner frame (4 pixel border)
            else if (x < 6 || x >= TEX_SIZE - 6 || y < 6 || y >= TEX_SIZE - 6) {
                color = frame;
            }
            // Screen area (will be replaced with terminal content during rendering)
            else {
                color = screen;
            }

            display_texture[y * TEX_SIZE + x] = color;
        }
    }
}

bool load_texture_from_bmp(const char *path, uint32_t *target) {
    if (!path || !*path || !target) {
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
    uint32_t *src = (uint32_t *)converted->pixels;
    int pitch = converted->pitch / 4;
    for (int y = 0; y < TEX_SIZE; ++y) {
        int srcY = y * converted->h / TEX_SIZE;
        for (int x = 0; x < TEX_SIZE; ++x) {
            int srcX = x * converted->w / TEX_SIZE;
            target[y * TEX_SIZE + x] = src[srcY * pitch + srcX];
        }
    }
    SDL_FreeSurface(converted);
    return true;
}

bool load_sky_texture_from_bmp(const char *path) {
    if (!path || !*path) {
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
    uint32_t *src = (uint32_t *)converted->pixels;
    int pitch = converted->pitch / 4;
    for (int y = 0; y < SKY_TEXTURE_HEIGHT; ++y) {
        int srcY = y * converted->h / SKY_TEXTURE_HEIGHT;
        for (int x = 0; x < SKY_TEXTURE_WIDTH; ++x) {
            int srcX = x * converted->w / SKY_TEXTURE_WIDTH;
            sky_texture[y * SKY_TEXTURE_WIDTH + x] = src[srcY * pitch + srcX];
        }
    }
    SDL_FreeSurface(converted);
    return true;
}

void load_custom_textures(void) {
    char path[256];
    for (int i = 0; i < NUM_WALL_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/textures/wall%d.bmp", i);
        load_texture_from_bmp(path, wall_textures[i]);
    }
    for (int i = 0; i < NUM_FLOOR_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/textures/floor%d.bmp", i);
        load_texture_from_bmp(path, floor_textures[i]);
    }
    for (int i = 0; i < NUM_CEIL_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/textures/ceiling%d.bmp", i);
        load_texture_from_bmp(path, ceiling_textures[i]);
    }
    snprintf(path, sizeof(path), "assets/textures/door.bmp");
    load_texture_from_bmp(path, door_texture);
    for (int i = 1; i < NUM_FURNITURE_TYPES; ++i) {
        const FurnitureSpec *spec = furniture_spec((FurnitureType)i);
        if (!spec || !spec->asset) {
            continue;
        }
        snprintf(path, sizeof(path), "assets/textures/%s", spec->asset);
        load_texture_from_bmp(path, furniture_textures[i]);
    }
    snprintf(path, sizeof(path), "assets/textures/cabinet.bmp");
    load_texture_from_bmp(path, cabinet_texture);
    snprintf(path, sizeof(path), "assets/textures/sky.bmp");
    load_sky_texture_from_bmp(path);
    snprintf(path, sizeof(path), "assets/textures/display.bmp");
    load_texture_from_bmp(path, display_texture);
}
