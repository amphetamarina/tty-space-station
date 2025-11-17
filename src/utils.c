// Utility functions module
#include "utils.h"
#include <math.h>

uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

uint32_t blend_colors(uint32_t base, uint32_t overlay, double alpha) {
    if (alpha <= 0.0) {
        return base;
    }
    if (alpha >= 1.0) {
        return overlay;
    }
    uint8_t br = (base >> 16) & 0xFF;
    uint8_t bg = (base >> 8) & 0xFF;
    uint8_t bb = base & 0xFF;
    uint8_t or = (overlay >> 16) & 0xFF;
    uint8_t og = (overlay >> 8) & 0xFF;
    uint8_t ob = overlay & 0xFF;
    uint8_t rr = (uint8_t)((1.0 - alpha) * br + alpha * or);
    uint8_t rg = (uint8_t)((1.0 - alpha) * bg + alpha * og);
    uint8_t rb = (uint8_t)((1.0 - alpha) * bb + alpha * ob);
    return pack_color(rr, rg, rb);
}

const FurnitureSpec furniture_specs[NUM_FURNITURE_TYPES] = {
    [FURN_NONE] = {"None", NULL, 0.0, 0.0, 0.0, 0, 0},
    [FURN_TABLE_SQUARE] = {"Square table", "table_square.bmp", 0.35, 0.35, 0.65, PACK_COLOR_LITERAL(120, 90, 50),
                           PACK_COLOR_LITERAL(160, 130, 80)},
    [FURN_TABLE_ROUND] = {"Round table", "table_round.bmp", 0.32, 0.32, 0.6, PACK_COLOR_LITERAL(90, 70, 40),
                          PACK_COLOR_LITERAL(150, 110, 60)},
    [FURN_BED] = {"Bed", "bed.bmp", 0.45, 0.25, 0.5, PACK_COLOR_LITERAL(70, 80, 120),
                  PACK_COLOR_LITERAL(120, 140, 200)},
    [FURN_SOFA] = {"Sofa", "sofa.bmp", 0.48, 0.28, 0.6, PACK_COLOR_LITERAL(80, 30, 30),
                   PACK_COLOR_LITERAL(150, 60, 60)},
    [FURN_WARDROBE] = {"Wardrobe", "wardrobe.bmp", 0.35, 0.2, 1.1, PACK_COLOR_LITERAL(80, 60, 30),
                       PACK_COLOR_LITERAL(140, 110, 70)},
};

const FurnitureSpec *furniture_spec(FurnitureType type) {
    if (type <= FURN_NONE || type >= NUM_FURNITURE_TYPES) {
        return NULL;
    }
    return &furniture_specs[type];
}

FurnitureType furniture_type_from_char(char c) {
    switch (c) {
    case 'T':
    case 't':
        return FURN_TABLE_SQUARE;
    case 'R':
    case 'r':
        return FURN_TABLE_ROUND;
    case 'B':
    case 'b':
        return FURN_BED;
    case 'S':
    case 's':
        return FURN_SOFA;
    case 'W':
    case 'w':
        return FURN_WARDROBE;
    default:
        return FURN_NONE;
    }
}

double furniture_angle_for_marker(char marker) {
    switch (marker) {
    case 't':
        return M_PI_2;
    case 'B':
    case 'b':
        return M_PI_2;
    default:
        return 0.0;
    }
}

const char *furniture_display_name(FurnitureType type) {
    const FurnitureSpec *spec = furniture_spec(type);
    return spec && spec->name ? spec->name : "Furniture";
}

const NPCSpec npc_specs[NUM_NPC_TYPES] = {
    [NPC_NONE] = {"None", "", 0, 0.0, 0.0},
    [NPC_PUPPY] = {"Puppy", "Woof! *wags tail*", PACK_COLOR_LITERAL(139, 90, 43), 2.0, 0.4},
    [NPC_GHOST] = {"Ghost", "Booooo... welcome traveler...", PACK_COLOR_LITERAL(200, 220, 255), 1.5, 0.8},
};

const NPCSpec *npc_spec(NPCType type) {
    if (type <= NPC_NONE || type >= NUM_NPC_TYPES) {
        return NULL;
    }
    return &npc_specs[type];
}

NPCType npc_type_from_char(char c) {
    switch (c) {
    case 'P':
    case 'p':
        return NPC_PUPPY;
    case 'G':
    case 'g':
        return NPC_GHOST;
    default:
        return NPC_NONE;
    }
}

const char *npc_display_name(NPCType type) {
    const NPCSpec *spec = npc_spec(type);
    return spec && spec->name ? spec->name : "NPC";
}
