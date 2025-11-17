// Main rendering and raycasting module
#include "renderer.h"
#include "texture.h"
#include "utils.h"
#include "map.h"
#include "ui.h"
#include "npc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

int render_furniture(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                     double *zbuffer) {
    const Player *player = &game->player;
    int highlight = -1;
    double highlightDepth = 1e9;
    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    int count = game->furniture_count < MAX_FURNITURE ? game->furniture_count : MAX_FURNITURE;
    int order[MAX_FURNITURE];
    double distSq[MAX_FURNITURE];
    for (int i = 0; i < count; ++i) {
        order[i] = i;
        double dx = game->furniture[i].x - player->x;
        double dy = game->furniture[i].y - player->y;
        distSq[i] = dx * dx + dy * dy;
    }
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (distSq[i] < distSq[j]) {
                double tmpD = distSq[i];
                distSq[i] = distSq[j];
                distSq[j] = tmpD;
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    for (int pass = 0; pass < 2; ++pass) {
        for (int ord = 0; ord < count; ++ord) {
            int i = order[ord];
            const FurnitureEntry *entry = &game->furniture[i];
            if (entry->type <= FURN_NONE) {
                continue;
            }
            const FurnitureSpec *spec = furniture_spec(entry->type);
            if (!spec) {
                continue;
            }
            double relX = entry->x - player->x;
            double relY = entry->y - player->y;
            double invDet = 1.0 / (planeX * dirY - dirX * planeY);
            double transformX = invDet * (dirY * relX - dirX * relY);
            double transformY = invDet * (-planeY * relX + planeX * relY);
            if (transformY <= 0.1) {
                continue;
            }
            int spriteScreenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));

            // Calculate angle between player view and furniture for Doom-style sprite selection
            double furnitureAngle = atan2(relY, relX);
            double viewAngle = player->angle;
            double angleDiff = furnitureAngle - viewAngle;
            while (angleDiff < -M_PI) angleDiff += 2.0 * M_PI;
            while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;

            // Select sprite frame based on 8 directions (Doom-style)
            int spriteDir = (int)((angleDiff + M_PI + M_PI / 8.0) / (M_PI / 4.0)) % 8;
            double worldHeight = spec->height > 0.1 ? spec->height : 0.6;
            double worldWidth = (spec->half_width > 0.01 ? spec->half_width * 2.0 : 0.6);
            int spriteHeight = abs((int)((SCREEN_HEIGHT / transformY) * worldHeight));
            int spriteWidth = abs((int)((SCREEN_HEIGHT / transformY) * worldWidth));
            if (spriteHeight < 4) {
                spriteHeight = 4;
            }
            if (spriteWidth < 4) {
                spriteWidth = 4;
            }
            int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2;
            int drawEndY = spriteHeight / 2 + SCREEN_HEIGHT / 2;
            if (drawStartY < 0) {
                drawStartY = 0;
            }
            if (drawEndY >= SCREEN_HEIGHT) {
                drawEndY = SCREEN_HEIGHT - 1;
            }
            int drawStartX = -spriteWidth / 2 + spriteScreenX;
            int drawEndX = spriteWidth / 2 + spriteScreenX;
            if (drawStartX < 0) {
                drawStartX = 0;
            }
            if (drawEndX >= SCREEN_WIDTH) {
                drawEndX = SCREEN_WIDTH - 1;
            }
            if (drawStartX >= drawEndX || drawStartY >= drawEndY) {
                continue;
            }
            if (pass == 0) {
                if (crossX >= drawStartX && crossX <= drawEndX && crossY >= drawStartY && crossY <= drawEndY) {
                    if (transformY < zbuffer[crossX] && transformY < highlightDepth) {
                        highlightDepth = transformY;
                        highlight = i;
                    }
                }
                continue;
            }
            for (int stripe = drawStartX; stripe <= drawEndX; ++stripe) {
                if (stripe < 0 || stripe >= SCREEN_WIDTH) {
                    continue;
                }
                if (transformY >= zbuffer[stripe]) {
                    continue;
                }
                int texX = (int)((double)(stripe - drawStartX) / (double)(spriteWidth ? spriteWidth : 1) * TEX_SIZE);
                if (texX < 0) {
                    texX = 0;
                }
                if (texX >= TEX_SIZE) {
                    texX = TEX_SIZE - 1;
                }
                for (int y = drawStartY; y <= drawEndY; ++y) {
                    if (y < 0 || y >= SCREEN_HEIGHT) {
                        continue;
                    }
                    int texY =
                        (int)((double)(y - drawStartY) / (double)(spriteHeight ? spriteHeight : 1) * TEX_SIZE);
                    if (texY < 0) {
                        texY = 0;
                    }
                    if (texY >= TEX_SIZE) {
                        texY = TEX_SIZE - 1;
                    }
                    uint32_t color = furniture_textures[entry->type][texY * TEX_SIZE + texX];

                    // Apply Doom-style sprite angle variation (slight brightness change)
                    double brightness = 1.0 - (spriteDir % 2) * 0.08;
                    uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * brightness);
                    uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * brightness);
                    uint8_t b = (uint8_t)((color & 0xFF) * brightness);
                    color = pack_color(r, g, b);

                    if (i == highlight) {
                        color = blend_colors(color, pack_color(255, 255, 255), 0.35);
                    }
                    draw_pixel(pixels, stripe, y, color);
                }
            }
        }
    }
    return highlight;
}

int render_npcs(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                double *zbuffer) {
    const Player *player = &game->player;
    int highlight = -1;
    double highlightDepth = 1e9;
    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    int count = game->npc_count < MAX_NPCS ? game->npc_count : MAX_NPCS;

    // Depth sort NPCs
    int order[MAX_NPCS];
    double distSq[MAX_NPCS];
    for (int i = 0; i < count; ++i) {
        order[i] = i;
        double dx = game->npcs[i].x - player->x;
        double dy = game->npcs[i].y - player->y;
        distSq[i] = dx * dx + dy * dy;
    }
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (distSq[i] < distSq[j]) {
                double tmpD = distSq[i];
                distSq[i] = distSq[j];
                distSq[j] = tmpD;
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    // Two-pass rendering: first pass for highlight detection, second for drawing
    for (int pass = 0; pass < 2; ++pass) {
        for (int ord = 0; ord < count; ++ord) {
            int i = order[ord];
            const NPCEntry *npc = &game->npcs[i];
            if (!npc->active || npc->type <= NPC_NONE) {
                continue;
            }
            const NPCSpec *spec = npc_spec(npc->type);
            if (!spec) {
                continue;
            }

            double relX = npc->x - player->x;
            double relY = npc->y - player->y;
            double invDet = 1.0 / (planeX * dirY - dirX * planeY);
            double transformX = invDet * (dirY * relX - dirX * relY);
            double transformY = invDet * (-planeY * relX + planeX * relY);

            if (transformY <= 0.1) {
                continue;
            }

            int spriteScreenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));

            // Calculate angle between player view and NPC for sprite selection
            double npcAngle = atan2(relY, relX);
            double viewAngle = player->angle;
            double angleDiff = npcAngle - viewAngle;
            while (angleDiff < -M_PI) angleDiff += 2.0 * M_PI;
            while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;

            // Select sprite frame based on 8 directions
            int spriteDir = (int)((angleDiff + M_PI + M_PI / 8.0) / (M_PI / 4.0)) % 8;

            double spriteHeight = spec->sprite_height > 0.1 ? spec->sprite_height : 0.6;
            int screenHeight = abs((int)((SCREEN_HEIGHT / transformY) * spriteHeight));
            int screenWidth = screenHeight / 2;

            if (screenHeight < 4) {
                screenHeight = 4;
            }
            if (screenWidth < 4) {
                screenWidth = 4;
            }

            int drawStartY = -screenHeight / 2 + SCREEN_HEIGHT / 2;
            int drawEndY = screenHeight / 2 + SCREEN_HEIGHT / 2;
            if (drawStartY < 0) {
                drawStartY = 0;
            }
            if (drawEndY >= SCREEN_HEIGHT) {
                drawEndY = SCREEN_HEIGHT - 1;
            }

            int drawStartX = -screenWidth / 2 + spriteScreenX;
            int drawEndX = screenWidth / 2 + spriteScreenX;
            if (drawStartX < 0) {
                drawStartX = 0;
            }
            if (drawEndX >= SCREEN_WIDTH) {
                drawEndX = SCREEN_WIDTH - 1;
            }

            if (drawStartX >= drawEndX || drawStartY >= drawEndY) {
                continue;
            }

            // First pass: check for highlight
            if (pass == 0) {
                if (crossX >= drawStartX && crossX <= drawEndX && crossY >= drawStartY && crossY <= drawEndY) {
                    if (transformY < zbuffer[crossX] && transformY < highlightDepth) {
                        highlightDepth = transformY;
                        highlight = i;
                    }
                }
                continue;
            }

            // Second pass: draw NPC
            for (int stripe = drawStartX; stripe <= drawEndX; ++stripe) {
                if (stripe < 0 || stripe >= SCREEN_WIDTH) {
                    continue;
                }
                if (transformY >= zbuffer[stripe]) {
                    continue;
                }

                for (int y = drawStartY; y <= drawEndY; ++y) {
                    if (y < 0 || y >= SCREEN_HEIGHT) {
                        continue;
                    }

                    // Simple colored sprite based on NPC type and angle
                    uint32_t color = spec->color;

                    // Add slight color variation based on sprite direction
                    double brightness = 1.0 - (spriteDir % 2) * 0.1;
                    uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * brightness);
                    uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * brightness);
                    uint8_t b = (uint8_t)((color & 0xFF) * brightness);
                    color = pack_color(r, g, b);

                    // Highlight if this is the targeted NPC
                    if (i == highlight) {
                        color = blend_colors(color, pack_color(255, 255, 255), 0.35);
                    }

                    // Add ghost transparency effect
                    if (npc->type == NPC_GHOST) {
                        uint32_t base = pixels[y * SCREEN_WIDTH + stripe];
                        color = blend_colors(base, color, 0.6);
                    }

                    draw_pixel(pixels, stripe, y, color);
                }
            }
        }
    }
    return highlight;
}

void render_memory_plaques(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                            double *zbuffer) {
    const Player *player = &game->player;
    for (int i = 0; i < game->memory_count; ++i) {
        const MemoryEntry *entry = &game->memories[i];
        double worldX = entry->x + entry->normal_x * 0.02;
        double worldY = entry->y + entry->normal_y * 0.02;
        double relX = worldX - player->x;
        double relY = worldY - player->y;
        double invDet = 1.0 / (planeX * dirY - dirX * planeY);
        double transformX = invDet * (dirY * relX - dirX * relY);
        double transformY = invDet * (-planeY * relX + planeX * relY);

        // Only render if in front and at reasonable distance
        if (transformY <= 0.1 || transformY > 5.0) {
            continue;
        }

        int screenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        if (screenX < 0 || screenX >= SCREEN_WIDTH) {
            continue;
        }
        if (transformY > zbuffer[screenX] + 0.1) {
            continue;
        }

        // Only show plaque if roughly centered on screen (player is looking at it)
        if (abs(screenX - SCREEN_WIDTH / 2) > SCREEN_WIDTH / 4) {
            continue;
        }

        // Fixed-size plaque with slight scaling based on distance
        double distScale = 1.0 / (1.0 + transformY * 0.2);
        distScale = clamp_int((int)(distScale * 100), 50, 100) / 100.0;

        int baseWidth = clamp_int((int)(((raw_longest_line(entry->text) + 4) * 8) * distScale), 100, 320);
        int chars_per_line = clamp_int((baseWidth - 24) / 8, 4, 40);
        char lines[MAX_LAYOUT_LINES][MEMORY_TEXT];
        int longestLine = 0;
        int lineCount = layout_text_lines(entry->text, chars_per_line, lines, 6, &longestLine);
        if (lineCount <= 0) {
            lineCount = 1;
            lines[0][0] = '\0';
        }

        int plaqueWidth = baseWidth;
        int plaqueHeight = (int)((lineCount * 14 + 28) * distScale);

        // Fixed position near top of screen
        int screenY = 100;
        int boxX = SCREEN_WIDTH / 2 - plaqueWidth / 2;
        int boxY = screenY;

        draw_rect(pixels, boxX - 2, boxY - 2, plaqueWidth + 4, plaqueHeight + 4, pack_color(10, 10, 20));
        draw_rect(pixels, boxX, boxY, plaqueWidth, plaqueHeight, pack_color(25, 25, 50));
        draw_rect(pixels, boxX + 2, boxY + 2, plaqueWidth - 4, plaqueHeight - 4, pack_color(40, 40, 70));

        int fontSize = (int)(8 * distScale);
        if (fontSize < 6) fontSize = 6;
        if (fontSize > 8) fontSize = 8;

        for (int line = 0; line < lineCount; ++line) {
            draw_text(pixels, boxX + 10, boxY + 10 + line * 14, lines[line], pack_color(240, 220, 170));
        }
    }
}

void render_remote_player(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX, double planeY,
                           double *zbuffer) {
    if (!game->net.remote.active) {
        return;
    }
    const Player *player = &game->player;
    double relX = game->net.remote.x - player->x;
    double relY = game->net.remote.y - player->y;
    double invDet = 1.0 / (planeX * dirY - dirX * planeY);
    double transformX = invDet * (dirY * relX - dirX * relY);
    double transformY = invDet * (-planeY * relX + planeX * relY);
    if (transformY <= 0.1) {
        return;
    }
    int spriteScreenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
    int spriteHeight = abs((int)(SCREEN_HEIGHT / transformY));
    int drawStartY = -spriteHeight / 2 + SCREEN_HEIGHT / 2;
    if (drawStartY < 0) {
        drawStartY = 0;
    }
    int drawEndY = spriteHeight / 2 + SCREEN_HEIGHT / 2;
    if (drawEndY >= SCREEN_HEIGHT) {
        drawEndY = SCREEN_HEIGHT - 1;
    }
    int spriteWidth = spriteHeight / 2;
    if (spriteWidth < 4) {
        spriteWidth = 4;
    }
    int drawStartX = -spriteWidth / 2 + spriteScreenX;
    if (drawStartX < 0) {
        drawStartX = 0;
    }
    int drawEndX = spriteWidth / 2 + spriteScreenX;
    if (drawEndX >= SCREEN_WIDTH) {
        drawEndX = SCREEN_WIDTH - 1;
    }
    for (int stripe = drawStartX; stripe < drawEndX; ++stripe) {
        if (transformY < zbuffer[stripe]) {
            for (int y = drawStartY; y < drawEndY; ++y) {
                draw_pixel(pixels, stripe, y, pack_color(220, 80, 80));
            }
        }
    }
    if (game->net.remote.name[0]) {
        int labelY = drawStartY - 14;
        if (labelY < 0) {
            labelY = 0;
        }
        int labelX = spriteScreenX - ((int)strlen(game->net.remote.name) * 4);
        if (labelX < 0) {
            labelX = 0;
        }
        draw_text(pixels, labelX, labelY, game->net.remote.name, pack_color(255, 200, 200));
    }
}

void render_scene(const Game *game, uint32_t *pixels, double *zbuffer) {
    const Player *player = &game->player;
    double dirX = cos(player->angle);
    double dirY = sin(player->angle);
    double planeX = -sin(player->angle) * tan(player->fov / 2.0);
    double planeY = cos(player->angle) * tan(player->fov / 2.0);

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
            uint32_t ceilingColor = pack_color(20, 20, 40);
            if (cellX >= 0 && cellX < game->map.width && cellY >= 0 && cellY < game->map.height) {
                int floorIdx = floor_index_for_char(game->map.tiles[cellY][cellX]);
                int ceilIdx =
                    ((cellX + cellY) % NUM_CEIL_TEXTURES + NUM_CEIL_TEXTURES) % NUM_CEIL_TEXTURES;
                floorColor = floor_textures[floorIdx][texY * TEX_SIZE + texX];
                ceilingColor = ceiling_textures[ceilIdx][texY * TEX_SIZE + texX];
            }
            pixels[y * SCREEN_WIDTH + x] = floorColor;
            int mirrorY = SCREEN_HEIGHT - y - 1;
            if (mirrorY >= 0 && mirrorY < SCREEN_HEIGHT) {
                pixels[mirrorY * SCREEN_WIDTH + x] = ceilingColor;
            }
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
        int memoryHit = -1;

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
                memoryHit = -1;
                hit = 1;
            } else if (tile_is_wall(game->map.tiles[mapY][mapX])) {
                hitTile = game->map.tiles[mapY][mapX];
                memoryHit = game->memory_map[mapY][mapX];
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
            if (hitTile == 'D') {
                color = door_texture[texY * TEX_SIZE + texX];
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

    int furnitureHighlight = render_furniture(game, pixels, dirX, dirY, planeX, planeY, zbuffer);
    int npcHighlight = render_npcs(game, pixels, dirX, dirY, planeX, planeY, zbuffer);
    render_memory_plaques(game, pixels, dirX, dirY, planeX, planeY, zbuffer);
    render_remote_player(game, pixels, dirX, dirY, planeX, planeY, zbuffer);

    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    for (int i = -10; i <= 10; ++i) {
        draw_pixel(pixels, crossX + i, crossY, pack_color(255, 255, 255));
        draw_pixel(pixels, crossX, crossY + i, pack_color(255, 255, 255));
    }
    if (npcHighlight >= 0 && npcHighlight < game->npc_count) {
        const NPCEntry *npc = &game->npcs[npcHighlight];
        const char *name = npc_display_name(npc->type);
        if (name && *name) {
            int labelWidth = (int)strlen(name) * 8;
            int labelX = crossX - labelWidth / 2;
            if (labelX < 10) {
                labelX = 10;
            }
            if (labelX + labelWidth >= SCREEN_WIDTH - 10) {
                labelX = SCREEN_WIDTH - 10 - labelWidth;
            }
            draw_text(pixels, labelX, crossY + 40, name, pack_color(255, 220, 150));
        }
    } else if (furnitureHighlight >= 0 && furnitureHighlight < game->furniture_count) {
        const FurnitureEntry *entry = &game->furniture[furnitureHighlight];
        const char *name = furniture_display_name(entry->type);
        if (name && *name) {
            int labelWidth = (int)strlen(name) * 8;
            int labelX = crossX - labelWidth / 2;
            if (labelX < 10) {
                labelX = 10;
            }
            if (labelX + labelWidth >= SCREEN_WIDTH - 10) {
                labelX = SCREEN_WIDTH - 10 - labelWidth;
            }
            draw_text(pixels, labelX, crossY + 40, name, pack_color(245, 230, 180));
        }
    }

    render_minimap(pixels, game);
    char lines[3][128];
    gather_nearby(game, lines);
    draw_text(pixels, 20, SCREEN_HEIGHT - 60,
              "Controls: WASD move, QE strafe, Arrows rotate, M place, V view, T chat, ESC quit",
              pack_color(255, 255, 255));
    char pos[128];
    snprintf(pos, sizeof(pos), "Position (%.1f, %.1f) Memories: %d", player->x, player->y, game->memory_count);
    draw_text(pixels, 20, SCREEN_HEIGHT - 40, pos, pack_color(200, 200, 200));
    draw_text(pixels, 20, SCREEN_HEIGHT - 25, lines[0], pack_color(255, 200, 150));
    draw_text(pixels, 20, SCREEN_HEIGHT - 15, lines[1], pack_color(255, 200, 150));

    char netStatus[128];
    if (game->net.mode == NET_HOST) {
        if (game->net.peer_fd != -1) {
            snprintf(netStatus, sizeof(netStatus), "Net: Hosting (client connected)");
        } else {
            snprintf(netStatus, sizeof(netStatus), "Net: Hosting on port %d (waiting)", game->net.port);
        }
    } else if (game->net.mode == NET_CLIENT) {
        if (game->net.peer_fd != -1 && game->net.connected) {
            snprintf(netStatus, sizeof(netStatus), "Net: Client connected to %.64s:%d", game->net.host_addr,
                     game->net.port);
        } else {
            snprintf(netStatus, sizeof(netStatus), "Net: Client connecting to %.64s:%d", game->net.host_addr,
                     game->net.port);
        }
    } else {
        snprintf(netStatus, sizeof(netStatus), "Net: Offline");
    }
    draw_text(pixels, 20, 72, netStatus, pack_color(180, 200, 255));

    if (game->hud_message_timer > 0.0 && game->hud_message[0]) {
        draw_text(pixels, 20, SCREEN_HEIGHT - 80, game->hud_message, pack_color(250, 210, 140));
    }

    int show_chat = game->chat_count < 4 ? game->chat_count : 4;
    for (int i = 0; i < show_chat; ++i) {
        const ChatMessage *msg = &game->chat_log[game->chat_count - show_chat + i];
        char line[CHAT_TEXT + 48];
        snprintf(line, sizeof(line), "%s: %s", msg->sender, msg->text);
        draw_text(pixels, 20, 90 + i * 12, line, pack_color(180, 220, 255));
    }

    if (game->input.active) {
        int boxW = SCREEN_WIDTH - 140;
        if (boxW < 360) {
            boxW = 360;
        }
        int textWidth = boxW - 40;
        int chars_per_line = clamp_int(textWidth / 8, 6, 60);
        char lines_buf[INPUT_MAX_LINES][MEMORY_TEXT];
        int lineCount = layout_text_lines(game->input.buffer, chars_per_line, lines_buf, INPUT_MAX_LINES, NULL);
        if (lineCount <= 0) {
            lineCount = 1;
            lines_buf[0][0] = '\0';
        }
        int boxH = lineCount * 14 + 140;
        int boxX = (SCREEN_WIDTH - boxW) / 2;
        int boxY = SCREEN_HEIGHT / 2 - boxH / 2;
        draw_rect(pixels, boxX - 2, boxY - 2, boxW + 4, boxH + 4, pack_color(15, 15, 35));
        draw_rect(pixels, boxX, boxY, boxW, boxH, pack_color(20, 20, 60));
        draw_rect(pixels, boxX + 2, boxY + 2, boxW - 4, boxH - 4, pack_color(30, 30, 80));
        draw_text(pixels, boxX + 14, boxY + 14, "Enter memory text", pack_color(255, 255, 255));
        draw_text(pixels, boxX + 14, boxY + 32,
                  "Enter: store   Esc: cancel   Shift+Enter: newline   Backspace: edit", pack_color(200, 210, 240));
        int textY = boxY + 56;
        uint64_t ticks = SDL_GetTicks64();
        bool cursor_on = ((ticks / 400) % 2) == 0;
        for (int i = 0; i < lineCount; ++i) {
            char lineOut[MEMORY_TEXT + 6];
            const char *prefix = (i == 0) ? "> " : "  ";
            memcpy(lineOut, prefix, 2);
            size_t avail = sizeof(lineOut) - 3;
            size_t copyLen = strlen(lines_buf[i]);
            if (copyLen > avail) {
                copyLen = avail;
            }
            memcpy(lineOut + 2, lines_buf[i], copyLen);
            lineOut[2 + copyLen] = '\0';
            if (i == lineCount - 1 && cursor_on) {
                size_t len = strlen(lineOut);
                if (len < sizeof(lineOut) - 2) {
                    lineOut[len] = '_';
                    lineOut[len + 1] = '\0';
                }
            }
            draw_text(pixels, boxX + 14, textY + i * 14, lineOut, pack_color(200, 240, 255));
        }
    }

    if (game->chat_input_active) {
        int boxW = SCREEN_WIDTH - 200;
        if (boxW < 320) {
            boxW = 320;
        }
        int boxX = 40;
        int boxY = SCREEN_HEIGHT - 120;
        draw_rect(pixels, boxX - 2, boxY - 2, boxW + 4, 64, pack_color(10, 10, 30));
        draw_rect(pixels, boxX, boxY, boxW, 64, pack_color(25, 25, 60));
        draw_text(pixels, boxX + 12, boxY + 12, "Chat:", pack_color(255, 255, 255));
        char buffer[CHAT_TEXT + 4];
        snprintf(buffer, sizeof(buffer), "> %s%s", game->chat_input,
                 ((SDL_GetTicks64() / 400) % 2) == 0 ? "_" : " ");
        draw_text(pixels, boxX + 12, boxY + 30, buffer, pack_color(200, 240, 255));
    }

    if (game->viewer_active) {
        render_memory_viewer(game, pixels);
    }

    if (game->dialogue_active) {
        render_npc_dialogue(game, pixels);
    }
}
