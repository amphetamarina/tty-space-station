// Main rendering and raycasting module
#include "renderer.h"
#include "texture.h"
#include "utils.h"
#include "map.h"
#include "ui.h"
#include "display.h"
#include "cabinet.h"
#include "../include/font8x8_basic.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

// ANSI color palette (16 colors)
static const uint32_t ansi_colors[16] = {
    0xFF000000, // 0: Black
    0xFFAA0000, // 1: Red
    0xFF00AA00, // 2: Green
    0xFFAA5500, // 3: Yellow/Brown
    0xFF0000AA, // 4: Blue
    0xFFAA00AA, // 5: Magenta
    0xFF00AAAA, // 6: Cyan
    0xFFAAAAAA, // 7: White/Gray
    0xFF555555, // 8: Bright Black/Gray
    0xFFFF5555, // 9: Bright Red
    0xFF55FF55, // 10: Bright Green
    0xFFFFFF55, // 11: Bright Yellow
    0xFF5555FF, // 12: Bright Blue
    0xFFFF55FF, // 13: Bright Magenta
    0xFF55FFFF, // 14: Bright Cyan
    0xFFFFFFFF  // 15: Bright White
};

int render_cabinets(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                    double *zbuffer) {
    const Player *player = &game->player;
    int highlight = -1;
    double highlightDepth = 1e9;
    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    int count = game->cabinet_count < MAX_CABINETS ? game->cabinet_count : MAX_CABINETS;

    // Cabinet dimensions (oriented box aligned to grid)
    double boxWidth = 0.8;   // X/Y size
    double boxDepth = 0.5;   // Depth
    double boxHeight = 1.2;  // Tall server cabinet

    for (int i = 0; i < count; ++i) {
        const CabinetEntry *entry = &game->cabinets[i];

        // Render cabinet as a 3D box - scan each vertical stripe
        // Cabinet is axis-aligned in grid space
        double minX = entry->x - boxWidth / 2.0;
        double maxX = entry->x + boxWidth / 2.0;
        double minY = entry->y - boxDepth / 2.0;
        double maxY = entry->y + boxDepth / 2.0;

        // For each screen column, raycast to find if we hit the box
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            double cameraX = 2.0 * x / (double)SCREEN_WIDTH - 1.0;
            double rayDirX = dirX + planeX * cameraX;
            double rayDirY = dirY + planeY * cameraX;

            // Check intersection with the 4 vertical faces of the box
            double hitDist = 1e9;
            int hitFace = -1;  // 0=front, 1=right, 2=back, 3=left
            double hitTexU = 0;

            // Face 0: Front face (Y = minY)
            if (fabs(rayDirY) > 0.001) {
                double t = (minY - player->y) / rayDirY;
                if (t > 0.1) {
                    double hitX = player->x + t * rayDirX;
                    if (hitX >= minX && hitX <= maxX) {
                        if (t < hitDist) {
                            hitDist = t;
                            hitFace = 0;
                            hitTexU = (hitX - minX) / boxWidth;
                        }
                    }
                }
            }

            // Face 1: Right face (X = maxX)
            if (fabs(rayDirX) > 0.001) {
                double t = (maxX - player->x) / rayDirX;
                if (t > 0.1) {
                    double hitY = player->y + t * rayDirY;
                    if (hitY >= minY && hitY <= maxY) {
                        if (t < hitDist) {
                            hitDist = t;
                            hitFace = 1;
                            hitTexU = (hitY - minY) / boxDepth;
                        }
                    }
                }
            }

            // Face 2: Back face (Y = maxY)
            if (fabs(rayDirY) > 0.001) {
                double t = (maxY - player->y) / rayDirY;
                if (t > 0.1) {
                    double hitX = player->x + t * rayDirX;
                    if (hitX >= minX && hitX <= maxX) {
                        if (t < hitDist) {
                            hitDist = t;
                            hitFace = 2;
                            hitTexU = (maxX - hitX) / boxWidth;
                        }
                    }
                }
            }

            // Face 3: Left face (X = minX)
            if (fabs(rayDirX) > 0.001) {
                double t = (minX - player->x) / rayDirX;
                if (t > 0.1) {
                    double hitY = player->y + t * rayDirY;
                    if (hitY >= minY && hitY <= maxY) {
                        if (t < hitDist) {
                            hitDist = t;
                            hitFace = 3;
                            hitTexU = (maxY - hitY) / boxDepth;
                        }
                    }
                }
            }

            // If we hit a face, render it
            if (hitFace >= 0 && hitDist < zbuffer[x]) {
                zbuffer[x] = hitDist;

                // Calculate wall height on screen
                int wallHeight = (int)(SCREEN_HEIGHT / hitDist * boxHeight);
                if (wallHeight < 1) wallHeight = 1;

                int drawStartY = -wallHeight / 2 + SCREEN_HEIGHT / 2;
                int drawEndY = wallHeight / 2 + SCREEN_HEIGHT / 2;
                if (drawStartY < 0) drawStartY = 0;
                if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;

                // Check for crosshair hit
                if (x == crossX && crossY >= drawStartY && crossY <= drawEndY) {
                    if (hitDist < highlightDepth) {
                        highlightDepth = hitDist;
                        highlight = i;
                    }
                }

                // Texture coordinates
                int texX = (int)(hitTexU * TEX_SIZE) & (TEX_SIZE - 1);

                // Render vertical stripe
                for (int y = drawStartY; y <= drawEndY; ++y) {
                    double texYf = (double)(y - drawStartY) / (double)wallHeight;
                    int texY = (int)(texYf * TEX_SIZE) & (TEX_SIZE - 1);

                    // Use cabinet's texture variation
                    int texIdx = entry->texture_index % NUM_CABINET_TEXTURES;
                    uint32_t color = cabinet_textures[texIdx][texY * TEX_SIZE + texX];

                    // Darken side faces for depth perception
                    if (hitFace == 1 || hitFace == 3) {
                        color = blend_colors(color, pack_color(0, 0, 0), 0.3);
                    }

                    // Highlight if targeted
                    if (i == highlight) {
                        color = blend_colors(color, pack_color(255, 255, 255), 0.35);
                    }

                    // Apply custom color aura if set
                    if (entry->has_custom_color) {
                        // Create aura effect on edges
                        double normalizedY = (double)(y - drawStartY) / (double)wallHeight;
                        bool isEdge = (normalizedY < 0.05 || normalizedY > 0.95 ||
                                      hitTexU < 0.05 || hitTexU > 0.95);
                        if (isEdge) {
                            color = blend_colors(color, entry->custom_color, 0.7);
                        } else {
                            // Subtle glow even in the center
                            color = blend_colors(color, entry->custom_color, 0.15);
                        }
                    }

                    draw_pixel(pixels, x, y, color);
                }
            }
        }
    }
    return highlight;
}

static uint32_t sample_display_pixel(const DisplayEntry *display, const Terminal *term,
                                     double surface_u, double rel_height, int mapX, int mapY) {
    const double border = 0.08;
    uint32_t frameColor = pack_color(25, 35, 50);
    uint32_t glassColor = pack_color(8, 12, 18);

    bool vertical_wall = fabs(display->normal_x) > fabs(display->normal_y);
    double axisTiles = vertical_wall ? (display->height > 0 ? display->height : 1) :
                                       (display->width > 0 ? display->width : 1);
    if (axisTiles <= 0.0) axisTiles = 1.0;

    double axisCoord = vertical_wall ? (mapY - display->grid_y) + surface_u
                                     : (mapX - display->grid_x) + surface_u;
    bool flip_u = vertical_wall ? (display->normal_x > 0) : (display->normal_y > 0);
    if (flip_u) {
        axisCoord = axisTiles - axisCoord;
    }
    axisCoord = fmod(axisCoord + axisTiles, axisTiles);

    double u = axisCoord / axisTiles;
    if (u < 0.0) u = 0.0;
    if (u > 1.0) u = 1.0;

    double v = rel_height;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;

    bool inside_screen = (u > border && u < 1.0 - border && v > border && v < 1.0 - border);
    if (!inside_screen) {
        return frameColor;
    }

    if (!term || !term->active) {
        return glassColor;
    }

    double screenU = (u - border) / (1.0 - border * 2.0);
    double screenV = (v - border) / (1.0 - border * 2.0);
    if (screenU < 0.0) screenU = 0.0;
    if (screenU > 1.0) screenU = 1.0;
    if (screenV < 0.0) screenV = 0.0;
    if (screenV > 1.0) screenV = 1.0;

    double termXF = screenU * TERM_COLS;
    double termYF = screenV * TERM_ROWS;
    int termX = clamp_int((int)termXF, 0, TERM_COLS - 1);
    int termY = clamp_int((int)termYF, 0, TERM_ROWS - 1);
    double glyphXF = termXF - termX;
    double glyphYF = termYF - termY;
    int glyphX = clamp_int((int)(glyphXF * 8.0), 0, 7);
    int glyphY = clamp_int((int)(glyphYF * 8.0), 0, 7);

    const TermCell *cell = &term->cells[termY][termX];
    unsigned char ch = (unsigned char)cell->ch;
    if (ch < 32 || ch > 126) {
        ch = ' ';
    }

    const unsigned char *bitmap = font8x8_basic[ch];
    bool pixel_on = (bitmap[glyphY] & (1 << glyphX)) != 0;
    uint32_t fg_color = ansi_colors[cell->fg_color & 0x0F];
    uint32_t bg_color = ansi_colors[cell->bg_color & 0x0F];
    uint32_t glyphColor = pixel_on ? fg_color : bg_color;

    double glow = pixel_on ? 0.2 : 0.5;
    return blend_colors(glyphColor, glassColor, glow);
}

void render_scene(const Game *game, uint32_t *pixels, double *zbuffer) {
    if (!game->map.tiles || !game->door_state) {
        return;  // Safety check for dynamic arrays
    }

    const Player *player = &game->player;
    double dirX = cos(player->angle);
    double dirY = sin(player->angle);
    double planeX = -sin(player->angle) * tan(player->fov / 2.0);
    double planeY = cos(player->angle) * tan(player->fov / 2.0);

    // Render sky first (Doom-style cylindrical panorama)
    for (int y = 0; y < SCREEN_HEIGHT / 2; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            // Calculate horizontal angle for this column
            double cameraX = 2.0 * x / (double)SCREEN_WIDTH - 1.0;
            double columnAngle = atan2(dirY + planeY * cameraX, dirX + planeX * cameraX);

            // Normalize angle to [0, 2*PI]
            while (columnAngle < 0) columnAngle += 2.0 * M_PI;
            while (columnAngle >= 2.0 * M_PI) columnAngle -= 2.0 * M_PI;

            // Map angle to texture X coordinate (wrapping horizontally)
            int skyX = (int)(columnAngle / (2.0 * M_PI) * SKY_TEXTURE_WIDTH) % SKY_TEXTURE_WIDTH;

            // Map screen Y to texture Y coordinate - use only middle portion of texture to avoid stretching
            // Map top of screen to middle of texture, stretch less
            double skyV = (double)y / (double)(SCREEN_HEIGHT / 2);  // 0.0 at top, 1.0 at horizon
            int skyY = (int)(skyV * SKY_TEXTURE_HEIGHT * 0.6);  // Only use 60% of texture height
            if (skyY >= SKY_TEXTURE_HEIGHT) skyY = SKY_TEXTURE_HEIGHT - 1;

            uint32_t skyColor = sky_texture[skyY * SKY_TEXTURE_WIDTH + skyX];
            pixels[y * SCREEN_WIDTH + x] = skyColor;
        }
    }

    double rayDirX0 = dirX - planeX;
    double rayDirY0 = dirY - planeY;
    double rayDirX1 = dirX + planeX;
    double rayDirY1 = dirY + planeY;
    for (int y = SCREEN_HEIGHT / 2; y < SCREEN_HEIGHT; ++y) {
        double row = y - SCREEN_HEIGHT / 2.0;
        if (row == 0.0) {
            row = 0.0001;
        }
        double posZ = 0.5 * SCREEN_HEIGHT;
        double rowDist = posZ / row;
        double floorStepX = rowDist * (rayDirX1 - rayDirX0) / SCREEN_WIDTH;
        double floorStepY = rowDist * (rayDirY1 - rayDirY0) / SCREEN_WIDTH;
        double floorX = game->player.x + rowDist * rayDirX0;
        double floorY = game->player.y + rowDist * rayDirY0;
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            int cellX = (int)floorX;
            int cellY = (int)floorY;
            double fracX = floorX - cellX;
            double fracY = floorY - cellY;
            int texX = (int)(fracX * TEX_SIZE) & (TEX_SIZE - 1);
            int texY = (int)(fracY * TEX_SIZE) & (TEX_SIZE - 1);
            uint32_t floorColor = pack_color(50, 40, 30);
            if (cellX >= 0 && cellX < game->map.width && cellY >= 0 && cellY < game->map.height) {
                int floorIdx = floor_index_for_char(game->map.tiles[cellY][cellX]);
                floorColor = floor_textures[floorIdx][texY * TEX_SIZE + texX];
            }
            pixels[y * SCREEN_WIDTH + x] = floorColor;
            floorX += floorStepX;
            floorY += floorStepY;
        }
    }

    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    int displayHighlight = -1;
    double displayHighlightDepth = 1e9;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        double cameraX = 2.0 * x / (double)SCREEN_WIDTH - 1.0;
        double rayDirX = dirX + planeX * cameraX;
        double rayDirY = dirY + planeY * cameraX;

        int mapX = (int)player->x;
        int mapY = (int)player->y;
        char hitTile = '1';

        double deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1.0 / rayDirX);
        double deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1.0 / rayDirY);

        double sideDistX;
        double sideDistY;
        int stepX = (rayDirX < 0) ? -1 : 1;
        int stepY = (rayDirY < 0) ? -1 : 1;

        if (rayDirX < 0) {
            sideDistX = (player->x - mapX) * deltaDistX;
        } else {
            sideDistX = (mapX + 1.0 - player->x) * deltaDistX;
        }
        if (rayDirY < 0) {
            sideDistY = (player->y - mapY) * deltaDistY;
        } else {
            sideDistY = (mapY + 1.0 - player->y) * deltaDistY;
        }

        int hit = 0;
        int side = 0;
        double doorOverlayDist = -1.0;
        int doorOverlayTexX = 0;
        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            if (mapX < 0 || mapX >= game->map.width || mapY < 0 || mapY >= game->map.height) {
                hit = 1;
                break;
            }
            bool doorTile = (game->map.tiles[mapY][mapX] == 'D');
            if (doorTile) {
                int state = game->door_state[mapY][mapX];
                if (state == 1) {
                    double doorDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
                    double doorWallX =
                        (side == 0) ? player->y + doorDist * rayDirY : player->x + doorDist * rayDirX;
                    doorWallX -= floor(doorWallX);
                    int texX = (int)(doorWallX * TEX_SIZE);
                    if (side == 0 && rayDirX > 0) {
                        texX = TEX_SIZE - texX - 1;
                    }
                    if (side == 1 && rayDirY < 0) {
                        texX = TEX_SIZE - texX - 1;
                    }
                    if (doorOverlayDist < 0 || doorDist < doorOverlayDist) {
                        doorOverlayDist = doorDist;
                        doorOverlayTexX = texX;
                    }
                    continue;
                }
                hitTile = 'D';
                hit = 1;
            } else if (tile_is_wall(game->map.tiles[mapY][mapX])) {
                hitTile = game->map.tiles[mapY][mapX];
                hit = 1;
            }
        }

        double perpWallDist;
        if (side == 0) {
            perpWallDist = (sideDistX - deltaDistX);
        } else {
            perpWallDist = (sideDistY - deltaDistY);
        }
        if (perpWallDist <= 0.0001) {
            perpWallDist = 0.0001;
        }
        zbuffer[x] = perpWallDist;

        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) {
            drawStart = 0;
        }
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawEnd >= SCREEN_HEIGHT) {
            drawEnd = SCREEN_HEIGHT - 1;
        }

        double wallX;
        if (side == 0) {
            wallX = player->y + perpWallDist * rayDirY;
        } else {
            wallX = player->x + perpWallDist * rayDirX;
        }
        wallX -= floor(wallX);

        double surfaceU = wallX;
        if (side == 0 && rayDirX > 0) {
            surfaceU = 1.0 - surfaceU;
        }
        if (side == 1 && rayDirY < 0) {
            surfaceU = 1.0 - surfaceU;
        }

        int texX = (int)(wallX * (double)TEX_SIZE);
        if (side == 0 && rayDirX > 0) {
            texX = TEX_SIZE - texX - 1;
        }
        if (side == 1 && rayDirY < 0) {
            texX = TEX_SIZE - texX - 1;
        }
        int texIndex = tile_texture_index(hitTile);
        bool renderDisplayWall = (hitTile == 'D' || hitTile == 'd') && game->skip_display_frames <= 0;
        int displayIndex = -1;
        const DisplayEntry *columnDisplay = NULL;
        const Terminal *columnTerm = NULL;

        if (renderDisplayWall) {
            displayIndex = find_display_at(game, mapX, mapY);
            if (displayIndex >= 0 && displayIndex < game->display_count) {
                columnDisplay = &game->displays[displayIndex];
                if (abs(x - crossX) <= 1 && perpWallDist < displayHighlightDepth) {
                    displayHighlightDepth = perpWallDist;
                    displayHighlight = displayIndex;
                }
                if (columnDisplay->terminal_index >= 0 && columnDisplay->terminal_index < MAX_TERMINALS) {
                    columnTerm = &game->terminals[columnDisplay->terminal_index];
                }
            } else {
                renderDisplayWall = false;
            }
        }

        for (int y = drawStart; y <= drawEnd; ++y) {
            int d = y * 256 - SCREEN_HEIGHT * 128 + lineHeight * 128;
            int texY = ((d * TEX_SIZE) / lineHeight) / 256;
            uint32_t color = wall_textures[texIndex][texY * TEX_SIZE + texX];

            if (renderDisplayWall && columnDisplay) {
                double relY = (double)(y - drawStart) / (double)lineHeight;
                color = sample_display_pixel(columnDisplay, columnTerm, surfaceU, relY, mapX, mapY);
                if (displayHighlight == displayIndex && abs(x - crossX) <= 1) {
                    color = blend_colors(color, pack_color(255, 255, 120), 0.35);
                }
            } else if (hitTile == 'D' || hitTile == 'd') {
                color = pack_color(10, 25, 35);
            } else if (side == 1) {
                color = blend_colors(color, pack_color(0, 0, 0), 0.3);
            }

            if (hitTile == '4') {
                double t = lineHeight > 0 ? ((double)(y - drawStart) / (double)lineHeight) : 0.0;
                double alpha = (t < 0.4) ? 0.65 : 0.35;
                color = blend_colors(color, pack_color(140, 180, 220), alpha);
            }
            pixels[y * SCREEN_WIDTH + x] = color;
        }

        if (doorOverlayDist > 0.0) {
            int doorLineHeight = (int)(SCREEN_HEIGHT / doorOverlayDist);
            int doorStart = -doorLineHeight / 2 + SCREEN_HEIGHT / 2;
            if (doorStart < 0) {
                doorStart = 0;
            }
            int doorEnd = doorLineHeight / 2 + SCREEN_HEIGHT / 2;
            if (doorEnd >= SCREEN_HEIGHT) {
                doorEnd = SCREEN_HEIGHT - 1;
            }
            for (int y = doorStart; y <= doorEnd; ++y) {
                int d = y * 256 - SCREEN_HEIGHT * 128 + doorLineHeight * 128;
                int texY = ((d * TEX_SIZE) / doorLineHeight) / 256;
                texY = texY < 0 ? 0 : (texY >= TEX_SIZE ? TEX_SIZE - 1 : texY);
                uint32_t overlayColor = door_texture[texY * TEX_SIZE + doorOverlayTexX];
                uint32_t base = pixels[y * SCREEN_WIDTH + x];
                pixels[y * SCREEN_WIDTH + x] = blend_colors(base, overlayColor, 0.35);
            }
        }
    }

    int cabinetHighlight = render_cabinets(game, pixels, dirX, dirY, planeX, planeY, zbuffer);

    for (int i = -10; i <= 10; ++i) {
        draw_pixel(pixels, crossX + i, crossY, pack_color(255, 255, 255));
        draw_pixel(pixels, crossX, crossY + i, pack_color(255, 255, 255));
    }
    if (cabinetHighlight >= 0 && cabinetHighlight < game->cabinet_count) {
        const CabinetEntry *entry = &game->cabinets[cabinetHighlight];
        const char *name = get_cabinet_display_name(entry);
        if (name && *name) {
            int labelWidth = (int)strlen(name) * 8;
            int labelX = crossX - labelWidth / 2;
            if (labelX < 10) {
                labelX = 10;
            }
            if (labelX + labelWidth >= SCREEN_WIDTH - 10) {
                labelX = SCREEN_WIDTH - 10 - labelWidth;
            }
            draw_text(pixels, labelX, crossY + 40, name, pack_color(150, 255, 180));
        }
    } else if (displayHighlight >= 0 && displayHighlight < game->display_count) {
        const DisplayEntry *entry = &game->displays[displayHighlight];
        const char *name = entry->name;
        if (name && *name) {
            int labelWidth = (int)strlen(name) * 8;
            int labelX = crossX - labelWidth / 2;
            if (labelX < 10) {
                labelX = 10;
            }
            if (labelX + labelWidth >= SCREEN_WIDTH - 10) {
                labelX = SCREEN_WIDTH - 10 - labelWidth;
            }
            draw_text(pixels, labelX, crossY + 40, name, pack_color(100, 200, 255));
        }
    }

    render_minimap(pixels, game);
    render_hud(pixels, game);
}

void render_terminal(const Terminal *term, uint32_t *pixels) {
    if (!term || !term->active) {
        return;
    }

    // Calculate terminal position (centered on screen) - 50% bigger with proper scaling
    int char_width = 10;  // Slightly bigger, properly spaced
    int char_height = 14; // Taller for better readability
    int term_pixel_width = TERM_COLS * char_width;
    int term_pixel_height = TERM_ROWS * char_height;
    int start_x = (SCREEN_WIDTH - term_pixel_width) / 2;
    int start_y = (SCREEN_HEIGHT - term_pixel_height) / 2;

    // Clear screen to dark blue
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        pixels[i] = 0xFF001020;
    }

    // Draw help bar at top
    const char *help_text = "TERMINAL MODE - Press F1 to exit and return to game";
    int help_x = (SCREEN_WIDTH - ((int)strlen(help_text) * 8)) / 2;
    draw_text(pixels, help_x, 10, help_text, pack_color(255, 255, 100));

    // Render each cell
    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            const TermCell *cell = &term->cells[row][col];
            int px = start_x + col * char_width;
            int py = start_y + row * char_height;

            // Get colors
            uint32_t bg_color = ansi_colors[cell->bg_color & 0x0F];
            uint32_t fg_color = ansi_colors[cell->fg_color & 0x0F];

            // Get character bitmap
            unsigned char ch = (unsigned char)cell->ch;
            if (ch < 32 || ch > 126) {
                ch = ' ';
            }
            const unsigned char *bitmap = font8x8_basic[ch];

            // Render character scaled to fit char_width x char_height
            // Use simple nearest-neighbor scaling
            for (int cy = 0; cy < char_height; cy++) {
                int src_y = (cy * 8) / char_height;  // Map to 0-7
                for (int cx = 0; cx < char_width; cx++) {
                    int src_x = (cx * 8) / char_width;  // Map to 0-7
                    int screen_x = px + cx;
                    int screen_y = py + cy;

                    if (screen_x >= 0 && screen_x < SCREEN_WIDTH &&
                        screen_y >= 0 && screen_y < SCREEN_HEIGHT) {

                        bool pixel_set = bitmap[src_y] & (1 << src_x);
                        uint32_t color = pixel_set ? fg_color : bg_color;
                        pixels[screen_y * SCREEN_WIDTH + screen_x] = color;
                    }
                }
            }
        }
    }

    // Render cursor if visible
    if (term->cursor_visible && term->cursor_x >= 0 && term->cursor_x < TERM_COLS &&
        term->cursor_y >= 0 && term->cursor_y < TERM_ROWS) {

        int px = start_x + term->cursor_x * char_width;
        int py = start_y + term->cursor_y * char_height;

        // Blinking cursor - draw underscore at bottom of cell
        uint32_t cursor_color = 0xFFAAFFAA; // Light green cursor

        int cursor_height = 2;  // Thin underscore
        for (int cy = char_height - cursor_height; cy < char_height; cy++) {
            for (int cx = 0; cx < char_width; cx++) {
                int screen_x = px + cx;
                int screen_y = py + cy;

                if (screen_x >= 0 && screen_x < SCREEN_WIDTH &&
                    screen_y >= 0 && screen_y < SCREEN_HEIGHT) {

                    // Draw cursor as underline
                    if (cy >= 6) {
                        pixels[screen_y * SCREEN_WIDTH + screen_x] = cursor_color;
                    }
                }
            }
        }
    }
}
