// NPC management and AI module
#include "npc.h"
#include "utils.h"
#include "map.h"
#include "furniture.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void rebuild_npcs(Game *game) {
    game->npc_count = 0;
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            char decor = game->map.decor[y][x];
            NPCType type = npc_type_from_char(decor);
            if (type != NPC_NONE && game->npc_count < MAX_NPCS) {
                NPCEntry *npc = &game->npcs[game->npc_count];
                const NPCSpec *spec = npc_spec(type);
                npc->type = type;
                npc->grid_x = x;
                npc->grid_y = y;
                npc->x = x + 0.5;
                npc->y = y + 0.5;
                npc->angle = 0.0;
                npc->state = NPC_STATE_IDLE;
                npc->state_timer = 0.0;
                npc->target_x = npc->x;
                npc->target_y = npc->y;
                npc->name = spec ? spec->name : "NPC";
                npc->dialogue = spec ? spec->dialogue : "...";
                npc->active = true;
                game->npc_count++;
            }
        }
    }
}

bool npc_blocks_position(const Game *game, double x, double y) {
    for (int i = 0; i < game->npc_count; ++i) {
        const NPCEntry *npc = &game->npcs[i];
        if (!npc->active) {
            continue;
        }
        double dx = npc->x - x;
        double dy = npc->y - y;
        double dist_sq = dx * dx + dy * dy;
        if (dist_sq < 0.25) {  // radius of 0.5
            return true;
        }
    }
    return false;
}

static bool npc_can_move_to(const Game *game, int npc_index, double nx, double ny) {
    if (nx < 1 || ny < 1 || nx >= game->map.width - 1 || ny >= game->map.height - 1) {
        return false;
    }
    int cellX = (int)nx;
    int cellY = (int)ny;
    if (tile_is_wall(game->map.tiles[cellY][cellX])) {
        return false;
    }
    if (furniture_blocks_position(game, nx, ny)) {
        return false;
    }
    // Check collision with other NPCs
    for (int i = 0; i < game->npc_count; ++i) {
        if (i == npc_index) {
            continue;
        }
        const NPCEntry *other = &game->npcs[i];
        if (!other->active) {
            continue;
        }
        double dx = other->x - nx;
        double dy = other->y - ny;
        double dist_sq = dx * dx + dy * dy;
        if (dist_sq < 0.5) {
            return false;
        }
    }
    return true;
}

void npc_update_single(Game *game, NPCEntry *npc, double delta) {
    if (!npc->active) {
        return;
    }

    const NPCSpec *spec = npc_spec(npc->type);
    double move_speed = spec ? spec->move_speed : 1.0;

    npc->state_timer -= delta;
    if (npc->state_timer < 0.0) {
        npc->state_timer = 0.0;
    }

    switch (npc->state) {
    case NPC_STATE_IDLE: {
        // Random small movements
        if (npc->state_timer <= 0.0) {
            // Random chance to switch to wander
            if (rand() % 100 < 30) {
                npc->state = NPC_STATE_WANDER;
                npc->state_timer = 3.0 + (rand() % 5);
                // Pick a random nearby target
                double angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
                double dist = 1.0 + ((double)rand() / RAND_MAX) * 3.0;
                npc->target_x = npc->x + cos(angle) * dist;
                npc->target_y = npc->y + sin(angle) * dist;
            } else {
                npc->state_timer = 1.0 + (rand() % 3);
            }
        }
        break;
    }

    case NPC_STATE_WANDER: {
        // Move toward target
        double dx = npc->target_x - npc->x;
        double dy = npc->target_y - npc->y;
        double dist = sqrt(dx * dx + dy * dy);

        if (dist < 0.1 || npc->state_timer <= 0.0) {
            // Reached target or timeout, go back to idle
            npc->state = NPC_STATE_IDLE;
            npc->state_timer = 1.0 + (rand() % 4);
        } else {
            // Move toward target
            double move_dist = move_speed * delta;
            if (move_dist > dist) {
                move_dist = dist;
            }
            double dirX = dx / dist;
            double dirY = dy / dist;
            double nextX = npc->x + dirX * move_dist;
            double nextY = npc->y + dirY * move_dist;

            // Find NPC index for collision check
            int npc_index = -1;
            for (int i = 0; i < game->npc_count; ++i) {
                if (&game->npcs[i] == npc) {
                    npc_index = i;
                    break;
                }
            }

            // Try to move, if blocked try perpendicular directions
            if (npc_can_move_to(game, npc_index, nextX, nextY)) {
                npc->x = nextX;
                npc->y = nextY;
                npc->angle = atan2(dirY, dirX);
            } else {
                // Try perpendicular directions
                double perpX1 = -dirY;
                double perpY1 = dirX;
                double perpX2 = dirY;
                double perpY2 = -dirX;

                if (npc_can_move_to(game, npc_index, npc->x + perpX1 * move_dist, npc->y + perpY1 * move_dist)) {
                    npc->x += perpX1 * move_dist;
                    npc->y += perpY1 * move_dist;
                    npc->angle = atan2(perpY1, perpX1);
                } else if (npc_can_move_to(game, npc_index, npc->x + perpX2 * move_dist, npc->y + perpY2 * move_dist)) {
                    npc->x += perpX2 * move_dist;
                    npc->y += perpY2 * move_dist;
                    npc->angle = atan2(perpY2, perpX2);
                } else {
                    // Can't move, go back to idle
                    npc->state = NPC_STATE_IDLE;
                    npc->state_timer = 1.0 + (rand() % 3);
                }
            }
        }
        break;
    }

    case NPC_STATE_TALK: {
        // Just stand still while talking
        if (npc->state_timer <= 0.0) {
            npc->state = NPC_STATE_IDLE;
            npc->state_timer = 2.0;
        }
        break;
    }
    }
}

void npc_update_ai(Game *game, double delta) {
    for (int i = 0; i < game->npc_count; ++i) {
        npc_update_single(game, &game->npcs[i], delta);
    }
}

void npc_interact(Game *game, int npc_index) {
    if (npc_index < 0 || npc_index >= game->npc_count) {
        return;
    }
    NPCEntry *npc = &game->npcs[npc_index];
    if (!npc->active) {
        return;
    }
    npc->state = NPC_STATE_TALK;
    npc->state_timer = 5.0;  // Talk for 5 seconds
}
