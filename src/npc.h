#ifndef NPC_H
#define NPC_H

#include "types.h"
#include <stdbool.h>

// NPC initialization and management
void rebuild_npcs(Game *game);

// NPC collision detection
bool npc_blocks_position(const Game *game, double x, double y);

// NPC AI update functions
void npc_update_ai(Game *game, double delta);
void npc_update_single(Game *game, NPCEntry *npc, double delta);

// NPC interaction
void npc_interact(Game *game, int npc_index);

#endif // NPC_H
