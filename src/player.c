// Player movement and collision module
#include "player.h"
#include "map.h"
#include "furniture.h"
#include "npc.h"
#include "cabinet.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

void normalize_angle(double *angle) {
    while (*angle < 0.0) {
        *angle += 2.0 * M_PI;
    }
    while (*angle >= 2.0 * M_PI) {
        *angle -= 2.0 * M_PI;
    }
}

int can_move(const Game *game, double nx, double ny) {
    if (!game->map.tiles || !game->door_state) {
        return 0;  // Safety check for dynamic arrays
    }
    if (nx < 1 || ny < 1 || nx >= game->map.width - 1 || ny >= game->map.height - 1) {
        return 0;
    }
    int cellX = (int)nx;
    int cellY = (int)ny;
    if (!door_is_passable(game, cellX, cellY)) {
        return 0;
    }
    if (furniture_blocks_position(game, nx, ny)) {
        return 0;
    }
    if (cabinet_blocks_position(game, nx, ny)) {
        return 0;
    }
    if (npc_blocks_position(game, nx, ny)) {
        return 0;
    }
    return 1;
}

void move_player(Game *game, double dx, double dy) {
    Player *player = &game->player;
    double nextX = player->x + dx;
    double nextY = player->y + dy;
    if (can_move(game, nextX, player->y)) {
        player->x = nextX;
    }
    if (can_move(game, player->x, nextY)) {
        player->y = nextY;
    }
}

bool ray_pick_wall(const Game *game, double angle, int *wallX, int *wallY, double *hitX, double *hitY,
                   double *normalX, double *normalY) {
    if (!game->map.tiles) {
        return false;  // Safety check for dynamic arrays
    }

    const Player *player = &game->player;
    double rayDirX = cos(angle);
    double rayDirY = sin(angle);
    int mapX = (int)player->x;
    int mapY = (int)player->y;

    double deltaDistX = (rayDirX == 0) ? 1e30 : fabs(1.0 / rayDirX);
    double deltaDistY = (rayDirY == 0) ? 1e30 : fabs(1.0 / rayDirY);
    int stepX = (rayDirX < 0) ? -1 : 1;
    int stepY = (rayDirY < 0) ? -1 : 1;
    double sideDistX = (rayDirX < 0) ? (player->x - mapX) * deltaDistX : (mapX + 1.0 - player->x) * deltaDistX;
    double sideDistY = (rayDirY < 0) ? (player->y - mapY) * deltaDistY : (mapY + 1.0 - player->y) * deltaDistY;
    int side = 0;

    while (mapX >= 0 && mapX < game->map.width && mapY >= 0 && mapY < game->map.height) {
        if (tile_is_wall(game->map.tiles[mapY][mapX])) {
            if (wallX) {
                *wallX = mapX;
            }
            if (wallY) {
                *wallY = mapY;
            }
            double dist;
            if (side == 0) {
                dist = (mapX - player->x + (1 - stepX) / 2.0) / rayDirX;
            } else {
                dist = (mapY - player->y + (1 - stepY) / 2.0) / rayDirY;
            }
            if (hitX) {
                *hitX = player->x + dist * rayDirX;
            }
            if (hitY) {
                *hitY = player->y + dist * rayDirY;
            }
            if (normalX && normalY) {
                if (side == 0) {
                    *normalX = (rayDirX > 0) ? -1.0 : 1.0;
                    *normalY = 0.0;
                } else {
                    *normalX = 0.0;
                    *normalY = (rayDirY > 0) ? -1.0 : 1.0;
                }
            }
            return true;
        }
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }
    }
    return false;
}

bool door_is_passable(const Game *game, int gx, int gy) {
    if (!game->map.tiles || !game->door_state) {
        return false;  // Safety check for dynamic arrays
    }
    if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
        return false;
    }
    if (game->door_state[gy][gx] == -1) {
        return !tile_is_wall(game->map.tiles[gy][gx]);
    }
    return game->door_state[gy][gx] == 1;
}

bool toggle_door_state(Game *game, int gx, int gy, bool notify) {
    if (!game->door_state) {
        return false;  // Safety check for dynamic arrays
    }
    if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
        return false;
    }
    if (game->door_state[gy][gx] == -1) {
        return false;
    }
    game->door_state[gy][gx] = game->door_state[gy][gx] ? 0 : 1;
    if (notify) {
        if (game->door_state[gy][gx]) {
            set_hud_message(game, "Door opened.");
        } else {
            set_hud_message(game, "Door closed.");
        }
    }
    return true;
}

bool interact_with_door(Game *game) {
    int targetX = (int)floor(game->player.x + cos(game->player.angle));
    int targetY = (int)floor(game->player.y + sin(game->player.angle));
    if (targetX < 0 || targetY < 0 || targetX >= game->map.width || targetY >= game->map.height) {
        set_hud_message(game, "No door ahead.");
        return false;
    }
    if (game->door_state[targetY][targetX] == -1) {
        set_hud_message(game, "No door ahead.");
        return false;
    }
    if (toggle_door_state(game, targetX, targetY, true)) {
        return true;
    }
    return false;
}

int npc_pick_target(const Game *game) {
    const Player *player = &game->player;
    double rayDirX = cos(player->angle);
    double rayDirY = sin(player->angle);

    int bestNPC = -1;
    double bestDist = 1e9;

    for (int i = 0; i < game->npc_count; ++i) {
        const NPCEntry *npc = &game->npcs[i];
        if (!npc->active) {
            continue;
        }

        double dx = npc->x - player->x;
        double dy = npc->y - player->y;
        double dist = sqrt(dx * dx + dy * dy);

        if (dist > 3.0) {  // Max interaction distance
            continue;
        }

        // Check if NPC is in front of player
        double dot = dx * rayDirX + dy * rayDirY;
        if (dot < 0.0) {
            continue;
        }

        // Check if NPC is within view cone
        double perpDist = fabs(dx * rayDirY - dy * rayDirX);
        if (perpDist > 0.5) {  // Narrow cone
            continue;
        }

        if (dist < bestDist) {
            bestDist = dist;
            bestNPC = i;
        }
    }

    return bestNPC;
}

bool interact_with_npc(Game *game) {
    int npc_index = npc_pick_target(game);
    if (npc_index < 0) {
        set_hud_message(game, "No NPC in range.");
        return false;
    }

    npc_interact(game, npc_index);
    begin_npc_dialogue(game, npc_index);

    const NPCEntry *npc = &game->npcs[npc_index];
    char msg[128];
    snprintf(msg, sizeof(msg), "Talking to %s...", npc->name);
    set_hud_message(game, msg);
    return true;
}
