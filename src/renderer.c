// Main rendering and raycasting module
#include "renderer.h"
#include "texture.h"
#include "utils.h"
#include "map.h"
#include "ui.h"
#include "display.h"
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

                    draw_pixel(pixels, x, y, color);
                }
            }
        }
    }
    return highlight;
}

int render_displays(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                    double *zbuffer) {
    const Player *player = &game->player;
    int highlight = -1;
    double highlightDepth = 1e9;
    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    int count = game->display_count < MAX_DISPLAYS ? game->display_count : MAX_DISPLAYS;

#if DEBUG_MODE
    static int frame_counter = 0;
    bool should_log = (frame_counter % 120 == 0); // Log every 2 seconds
    if (should_log) {
        printf("[DEBUG RENDER] Frame %d: Rendering %d displays\n", frame_counter, count);
    }
    frame_counter++;
#endif

    for (int i = 0; i < count; ++i) {
        const DisplayEntry *display = &game->displays[i];

#if DEBUG_MODE
        if (should_log && i == 0) {
            printf("[DEBUG RENDER] Display #%d: pos(%.2f,%.2f) normal(%.2f,%.2f)\n",
                   i, display->x, display->y, display->normal_x, display->normal_y);
        }
#endif

        // Check if display is facing the player (dot product with view direction)
        double toPlayerX = player->x - display->x;
        double toPlayerY = player->y - display->y;
        double dotProduct = display->normal_x * toPlayerX + display->normal_y * toPlayerY;

        // Only render if player is in front of display (display faces player)
        if (dotProduct <= 0.0) {
#if DEBUG_MODE
            if (should_log && i == 0) {
                printf("[DEBUG RENDER] Display #%d CULLED: dotProduct=%.3f (player behind display)\n", i, dotProduct);
            }
#endif
            continue;
        }

        // Display center position (offset slightly from wall)
        double centerX = display->x + display->normal_x * 0.05;
        double centerY = display->y + display->normal_y * 0.05;

        // Display dimensions in world space
        double halfWidth = 0.4;
        double halfHeight = 0.3;

        // Project center to screen for bounds check
        double relX = centerX - player->x;
        double relY = centerY - player->y;
        double invDet = 1.0 / (planeX * dirY - dirX * planeY);
        double transformX = invDet * (dirY * relX - dirX * relY);
        double transformY = invDet * (-planeY * relX + planeX * relY);

        if (transformY <= 0.1) {
#if DEBUG_MODE
            if (should_log && i == 0) {
                printf("[DEBUG RENDER] Display #%d CULLED: transformY=%.3f (too close or behind)\n", i, transformY);
            }
#endif
            continue;
        }

        // Simple billboard rendering for displays (flat, axis-aligned)
        int screenCenterX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        int screenWidth = abs((int)((SCREEN_HEIGHT / transformY) * halfWidth * 2));
        int screenHeight = abs((int)((SCREEN_HEIGHT / transformY) * halfHeight * 2));

#if DEBUG_MODE
        if (should_log && i == 0) {
            printf("[DEBUG RENDER] Display #%d VISIBLE: transformY=%.3f, screenPos(%d,%d) size(%dx%d)\n",
                   i, transformY, screenCenterX, SCREEN_HEIGHT/2, screenWidth, screenHeight);
        }
#endif

        if (screenWidth < 4) screenWidth = 4;
        if (screenHeight < 4) screenHeight = 4;

        int drawStartX = screenCenterX - screenWidth / 2;
        int drawEndX = screenCenterX + screenWidth / 2;
        int drawStartY = SCREEN_HEIGHT / 2 - screenHeight / 2;
        int drawEndY = SCREEN_HEIGHT / 2 + screenHeight / 2;

        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= SCREEN_WIDTH) drawEndX = SCREEN_WIDTH - 1;
        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= SCREEN_HEIGHT) drawEndY = SCREEN_HEIGHT - 1;

        if (drawStartX >= drawEndX || drawStartY >= drawEndY) {
            continue;
        }

        // Check for crosshair hit
        if (crossX >= drawStartX && crossX <= drawEndX &&
            crossY >= drawStartY && crossY <= drawEndY) {
            if (transformY < highlightDepth) {
                highlightDepth = transformY;
                highlight = i;
            }
        }

        // Get terminal for this display
        const Terminal *term = NULL;
        if (display->terminal_index >= 0 && display->terminal_index < MAX_TERMINALS) {
            term = &game->terminals[display->terminal_index];
        }

#if DEBUG_MODE
        if (should_log && i == 0) { // Log first display only
            if (term) {
                printf("[DEBUG RENDER] Display #%d: term_idx=%d, active=%d, pty_fd=%d\n",
                       i, display->terminal_index, term->active, term->pty_fd);
                // Sample first few cells
                printf("[DEBUG RENDER] First 10 chars: ");
                for (int c = 0; c < 10 && c < TERM_COLS; c++) {
                    char ch = term->cells[0][c].ch;
                    printf("'%c'(%d) ", (ch >= 32 && ch < 127) ? ch : '?', (int)ch);
                }
                printf("\n");
            } else {
                printf("[DEBUG RENDER] Display #%d: NO TERMINAL\n", i);
            }
        }
#endif

        // Render the display
        for (int y = drawStartY; y <= drawEndY; ++y) {
            if (y < 0 || y >= SCREEN_HEIGHT) continue;

            int texY = (int)((double)(y - drawStartY) / (double)screenHeight * TEX_SIZE);
            if (texY < 0) texY = 0;
            if (texY >= TEX_SIZE) texY = TEX_SIZE - 1;

            for (int x = drawStartX; x <= drawEndX; ++x) {
                if (x < 0 || x >= SCREEN_WIDTH) continue;
                if (transformY >= zbuffer[x]) continue;

                int texX = (int)((double)(x - drawStartX) / (double)screenWidth * TEX_SIZE);
                if (texX < 0) texX = 0;
                if (texX >= TEX_SIZE) texX = TEX_SIZE - 1;

                uint32_t color = display_texture[texY * TEX_SIZE + texX];

                // If this is in the screen area (not frame) and we have a terminal, show terminal content
                if (term && term->active && texX >= 6 && texX < TEX_SIZE - 6 && texY >= 6 && texY < TEX_SIZE - 6) {
#if DEBUG_MODE
                    static int render_pixel_counter = 0;
                    if (render_pixel_counter == 0 && should_log && i == 0) {
                        printf("[DEBUG RENDER] Rendering terminal content pixels for display #%d\n", i);
                    }
                    render_pixel_counter++;
#endif
                    // Map texture coordinates to terminal character grid
                    int screenTexX = texX - 6;
                    int screenTexY = texY - 6;
                    int screenWidthTex = TEX_SIZE - 12;
                    int screenHeightTex = TEX_SIZE - 12;

                    int termX = (screenTexX * TERM_COLS) / screenWidthTex;
                    int termY = (screenTexY * TERM_ROWS) / screenHeightTex;

                    if (termX >= 0 && termX < TERM_COLS && termY >= 0 && termY < TERM_ROWS) {
                        const TermCell *cell = &term->cells[termY][termX];

                        // Get character position within the cell
                        int charWidth = screenWidthTex / TERM_COLS;
                        int charHeight = screenHeightTex / TERM_ROWS;
                        int charPosX = screenTexX - (termX * charWidth);
                        int charPosY = screenTexY - (termY * charHeight);

                        // Map to 8x8 font bitmap coordinates
                        int fontX = (charPosX * 8) / charWidth;
                        int fontY = (charPosY * 8) / charHeight;

                        // Clamp to valid range
                        if (fontX < 0) fontX = 0;
                        if (fontX > 7) fontX = 7;
                        if (fontY < 0) fontY = 0;
                        if (fontY > 7) fontY = 7;

                        // Get character bitmap
                        unsigned char ch = (unsigned char)cell->ch;
                        if (ch < 32 || ch > 126) {
                            ch = ' ';
                        }
                        const unsigned char *bitmap = font8x8_basic[ch];

                        // Check if this pixel is set in the font bitmap
                        bool pixel_set = bitmap[fontY] & (1 << fontX);

                        // Get colors from ANSI palette
                        uint32_t bg_color = ansi_colors[cell->bg_color & 0x0F];
                        uint32_t fg_color = ansi_colors[cell->fg_color & 0x0F];

                        // Use foreground or background color based on bitmap
                        color = pixel_set ? fg_color : bg_color;
                    }
                }

                // Highlight if this is the targeted display
                if (i == highlight) {
                    color = blend_colors(color, pack_color(255, 255, 100), 0.25);
                }

                draw_pixel(pixels, x, y, color);
            }
        }
    }
    return highlight;
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

#if DEBUG_MODE
    static int scene_frame_counter = 0;
    if (scene_frame_counter % 120 == 0) {  // Log every 2 seconds
        printf("[DEBUG SCENE] Player at (%.2f, %.2f) angle=%.2f displays=%d\n",
               player->x, player->y, player->angle, game->display_count);
    }
    scene_frame_counter++;
#endif

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

        int texX = (int)(wallX * (double)TEX_SIZE);
        if (side == 0 && rayDirX > 0) {
            texX = TEX_SIZE - texX - 1;
        }
        if (side == 1 && rayDirY < 0) {
            texX = TEX_SIZE - texX - 1;
        }
        int texIndex = tile_texture_index(hitTile);

        for (int y = drawStart; y <= drawEnd; ++y) {
            int d = y * 256 - SCREEN_HEIGHT * 128 + lineHeight * 128;
            int texY = ((d * TEX_SIZE) / lineHeight) / 256;
            uint32_t color = wall_textures[texIndex][texY * TEX_SIZE + texX];

            // Render display walls with terminal content
            if (hitTile == 'D' || hitTile == 'd') {
                // Default to dark screen color
                color = pack_color(10, 25, 35);

#if DEBUG_MODE
                static int display_render_counter = 0;
                if (display_render_counter % 120 == 0) {
                    printf("[DEBUG DISPLAY] Rendering display wall at map(%d,%d)\n", mapX, mapY);
                }
                display_render_counter++;
#endif

                // SAFETY: Extra bounds checking and validation to prevent crashes
                if (mapX >= 0 && mapX < game->map.width && mapY >= 0 && mapY < game->map.height) {
                    int display_idx = find_display_at(game, mapX, mapY);

#if DEBUG_MODE
                    if (display_render_counter % 120 == 0) {
                        printf("[DEBUG DISPLAY] display_idx=%d, display_count=%d\n", display_idx, game->display_count);
                    }
#endif

                    if (display_idx >= 0 && display_idx < game->display_count && display_idx < MAX_DISPLAYS) {
                        const DisplayEntry *disp = &game->displays[display_idx];

#if DEBUG_MODE
                        if (display_render_counter % 120 == 0) {
                            printf("[DEBUG DISPLAY] Found display, terminal_index=%d\n", disp->terminal_index);
                        }
#endif

                        if (disp->terminal_index >= 0 && disp->terminal_index < MAX_TERMINALS) {
                            const Terminal *term = &game->terminals[disp->terminal_index];

#if DEBUG_MODE
                            if (display_render_counter % 120 == 0) {
                                printf("[DEBUG DISPLAY] Terminal: active=%d, pty_fd=%d\n", term->active, term->pty_fd);
                                if (term->active) {
                                    printf("[DEBUG DISPLAY] First 5 chars: ");
                                    for (int c = 0; c < 5 && c < TERM_COLS; c++) {
                                        printf("'%c'(%d) ", term->cells[0][c].ch, term->cells[0][c].ch);
                                    }
                                    printf("\n");
                                }
                            }
#endif

                            // SAFETY: Verify terminal is active and has valid PTY
                            if (term && term->active && term->pty_fd >= 0) {
                                // Map wall texture coordinates to terminal space
                                int screenTexX = texX;
                                int screenTexY = texY;

                                // Border region (6 pixels each side)
                                if (screenTexX >= 6 && screenTexX < TEX_SIZE - 6 &&
                                    screenTexY >= 6 && screenTexY < TEX_SIZE - 6) {

                                    int contentX = screenTexX - 6;
                                    int contentY = screenTexY - 6;
                                    int contentWidth = TEX_SIZE - 12;  // 52
                                    int contentHeight = TEX_SIZE - 12;  // 52

                                    // Map to terminal grid with bounds check
                                    if (contentWidth > 0 && contentHeight > 0) {
                                        int termX = (contentX * TERM_COLS) / contentWidth;
                                        int termY = (contentY * TERM_ROWS) / contentHeight;

                                        // SAFETY: Strict bounds check on terminal array access
                                        if (termX >= 0 && termX < TERM_COLS && termY >= 0 && termY < TERM_ROWS) {
                                            // Access terminal cell data
                                            const TermCell *cell = &term->cells[termY][termX];

                                            // Get character position within cell
                                            int charWidth = contentWidth / TERM_COLS;
                                            int charHeight = contentHeight / TERM_ROWS;

                                            if (charWidth > 0 && charHeight > 0) {
                                                int charPosX = contentX - (termX * charWidth);
                                                int charPosY = contentY - (termY * charHeight);

                                                // Map to 8x8 font bitmap with clamping
                                                int fontX = (charPosX * 8) / charWidth;
                                                int fontY = (charPosY * 8) / charHeight;
                                                fontX = (fontX < 0) ? 0 : ((fontX > 7) ? 7 : fontX);
                                                fontY = (fontY < 0) ? 0 : ((fontY > 7) ? 7 : fontY);

                                                // Get character with safety check
                                                unsigned char ch = (unsigned char)cell->ch;
                                                if (ch < 32 || ch > 126) ch = ' ';

                                                // SAFETY: Bounds check on font array
                                                if (ch >= 0 && ch < 128) {
                                                    const unsigned char *bitmap = font8x8_basic[ch];

                                                    // Check pixel in bitmap
                                                    bool pixel_set = bitmap[fontY] & (1 << fontX);

                                                    // Get ANSI colors with mask
                                                    uint32_t bg_color = ansi_colors[cell->bg_color & 0x0F];
                                                    uint32_t fg_color = ansi_colors[cell->fg_color & 0x0F];

                                                    color = pixel_set ? fg_color : bg_color;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
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
    // Displays are now rendered as walls, not sprites
    // int displayHighlight = render_displays(game, pixels, dirX, dirY, planeX, planeY, zbuffer);
    int displayHighlight = -1; // TODO: Implement wall-based display highlighting

    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    for (int i = -10; i <= 10; ++i) {
        draw_pixel(pixels, crossX + i, crossY, pack_color(255, 255, 255));
        draw_pixel(pixels, crossX, crossY + i, pack_color(255, 255, 255));
    }
    if (cabinetHighlight >= 0 && cabinetHighlight < game->cabinet_count) {
        const CabinetEntry *entry = &game->cabinets[cabinetHighlight];
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
    draw_text(pixels, 20, SCREEN_HEIGHT - 60,
              "Controls: WASD move, QE strafe, Arrows rotate, ESC quit",
              pack_color(255, 255, 255));
    char pos[128];
    snprintf(pos, sizeof(pos), "Position (%.1f, %.1f)", player->x, player->y);
    draw_text(pixels, 20, SCREEN_HEIGHT - 40, pos, pack_color(200, 200, 200));

    if (game->hud_message_timer > 0.0 && game->hud_message[0]) {
        draw_text(pixels, 20, SCREEN_HEIGHT - 80, game->hud_message, pack_color(250, 210, 140));
    }
}

void render_terminal(const Terminal *term, uint32_t *pixels) {
    if (!term || !term->active) {
        return;
    }

    // Calculate terminal position (centered on screen) - scaled for 1366x768 resolution
    int char_width = 14;  // Larger font for better readability at higher resolution
    int char_height = 20; // Proportionally taller
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
