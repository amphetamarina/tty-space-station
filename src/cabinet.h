#ifndef CABINET_H
#define CABINET_H

#include "types.h"

// Cabinet management functions
void rebuild_cabinets(Game *game);
bool cabinet_blocks_position(const Game *game, double x, double y);
int find_cabinet_at(const Game *game, int gx, int gy);
void activate_cabinet(Game *game, int cabinet_index);
bool remove_cabinet(Game *game, int cabinet_index);
bool place_cabinet(Game *game, int gx, int gy);
void set_cabinet_custom_name(CabinetEntry *cabinet, const char *name);
void set_cabinet_custom_color(CabinetEntry *cabinet, uint32_t color);
const char* get_cabinet_display_name(const CabinetEntry *cabinet);

#endif // CABINET_H
