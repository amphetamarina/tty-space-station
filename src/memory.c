// Memory system module
#include "memory.h"
#include "map.h"
#include "player.h"
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void load_memories(Game *game) {
    if (!game->has_save_path) {
        return;
    }
    FILE *fp = fopen(game->save_path, "r");
    if (!fp) {
        return;
    }
    char line[MEMORY_TEXT + 64];
    while (fgets(line, sizeof(line), fp)) {
        if (game->memory_count >= MAX_MEMORIES) {
            break;
        }
        char *newline = strchr(line, '\n');
        if (newline) {
            *newline = '\0';
        }
        char *sep = strchr(line, '|');
        if (!sep) {
            continue;
        }
        *sep = '\0';
        int gx = 0, gy = 0, nx = 0, ny = 0;
        if (sscanf(line, "%d %d %d %d", &gx, &gy, &nx, &ny) != 4) {
            continue;
        }
        const char *text = sep + 1;
        if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
            continue;
        }
        if (!tile_is_wall(game->map.tiles[gy][gx]) || game->memory_map[gy][gx] != -1) {
            continue;
        }
        MemoryEntry *entry = &game->memories[game->memory_count];
        entry->grid_x = gx;
        entry->grid_y = gy;
        entry->normal_x = (double)nx;
        entry->normal_y = (double)ny;
        if (!entry->normal_x && !entry->normal_y) {
            entry->normal_x = 0.0;
            entry->normal_y = -1.0;
        }
        entry->x = gx + 0.5 + entry->normal_x * 0.45;
        entry->y = gy + 0.5 + entry->normal_y * 0.45;
        snprintf(entry->text, sizeof(entry->text), "%s", text[0] ? text : "Untitled memory");
        game->memory_map[gy][gx] = game->memory_count;
        game->memory_count++;
    }
    fclose(fp);
}

void save_memories(const Game *game) {
    if (!game->has_save_path) {
        return;
    }
    FILE *fp = fopen(game->save_path, "w");
    if (!fp) {
        return;
    }
    for (int i = 0; i < game->memory_count; ++i) {
        const MemoryEntry *entry = &game->memories[i];
        int nx = (int)lrint(entry->normal_x);
        int ny = (int)lrint(entry->normal_y);
        fprintf(fp, "%d %d %d %d|%s\n", entry->grid_x, entry->grid_y, nx, ny, entry->text);
    }
    fclose(fp);
}

int add_memory_entry_at(Game *game, int gx, int gy, double nx, double ny, const char *text) {
    if (game->memory_count >= MAX_MEMORIES) {
        return -1;
    }
    if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
        return -1;
    }
    if (!tile_is_wall(game->map.tiles[gy][gx]) || game->memory_map[gy][gx] != -1) {
        return -1;
    }
    MemoryEntry *entry = &game->memories[game->memory_count];
    entry->grid_x = gx;
    entry->grid_y = gy;
    entry->normal_x = nx;
    entry->normal_y = ny;
    entry->x = gx + 0.5 + nx * 0.02;
    entry->y = gy + 0.5 + ny * 0.02;
    snprintf(entry->text, sizeof(entry->text), "%s", (text && text[0]) ? text : "Untitled memory");
    game->memory_map[gy][gx] = game->memory_count;
    game->memory_count++;
    return game->memory_count - 1;
}

void edit_memory_entry(Game *game, int index, const char *text) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    MemoryEntry *entry = &game->memories[index];
    snprintf(entry->text, sizeof(entry->text), "%s", (text && text[0]) ? text : "Untitled memory");
}

void delete_memory_internal(Game *game, int index) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    int gx = game->memories[index].grid_x;
    int gy = game->memories[index].grid_y;
    if (gy >= 0 && gy < game->map.height && gx >= 0 && gx < game->map.width && game->memory_map[gy][gx] == index) {
        game->memory_map[gy][gx] = -1;
    }
    for (int i = index + 1; i < game->memory_count; ++i) {
        game->memories[i - 1] = game->memories[i];
        int wx = game->memories[i - 1].grid_x;
        int wy = game->memories[i - 1].grid_y;
        if (wy >= 0 && wy < game->map.height && wx >= 0 && wx < game->map.width) {
            game->memory_map[wy][wx] = i - 1;
        }
    }
    game->memory_count--;
    if (game->viewer_index == index) {
        close_memory_view(game);
    } else if (game->viewer_index > index) {
        game->viewer_index--;
    }
    if (game->has_save_path) {
        save_memories(game);
    }
}

void delete_memory(Game *game, int index) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    delete_memory_internal(game, index);
    set_hud_message(game, "Memory deleted.");
}

bool open_memory_view(Game *game, int index) {
    if (index < 0 || index >= game->memory_count) {
        return false;
    }
    game->viewer_active = true;
    game->viewer_index = index;
    game->viewer_delete_prompt = false;
    set_hud_message(game, "Viewing memory. E edit, Delete remove, Esc close.");
    return true;
}

void close_memory_view(Game *game) {
    game->viewer_active = false;
    game->viewer_index = -1;
    game->viewer_delete_prompt = false;
}

bool target_memory(const Game *game, int *index_out) {
    int wallX, wallY;
    double hx, hy, nx, ny;
    if (!ray_pick_wall(game, game->player.angle, &wallX, &wallY, &hx, &hy, &nx, &ny)) {
        return false;
    }
    if (wallX < 0 || wallY < 0 || wallX >= game->map.width || wallY >= game->map.height) {
        return false;
    }
    int idx = game->memory_map[wallY][wallX];
    if (idx < 0 || idx >= game->memory_count) {
        return false;
    }
    if (index_out) {
        *index_out = idx;
    }
    return true;
}

bool begin_memory_input(Game *game) {
    if (game->input.active) {
        return false;
    }
    if (game->memory_count >= MAX_MEMORIES) {
        set_hud_message(game, "Memory palace full.");
        return false;
    }
    int wallX, wallY;
    double hitX, hitY, normalX, normalY;
    if (!ray_pick_wall(game, game->player.angle, &wallX, &wallY, &hitX, &hitY, &normalX, &normalY)) {
        set_hud_message(game, "Aim at a wall within range to place a memory.");
        return false;
    }
    if (game->memory_map[wallY][wallX] != -1) {
        set_hud_message(game, "This wall already holds a memory.");
        return false;
    }
    game->input.active = true;
    game->input.targetX = wallX;
    game->input.targetY = wallY;
    game->input.hitX = hitX;
    game->input.hitY = hitY;
    game->input.normalX = normalX;
    game->input.normalY = normalY;
    game->input.length = 0;
    game->input.buffer[0] = '\0';
    game->input.editing = false;
    game->input.edit_index = -1;
    refresh_text_input(game);
    set_hud_message(game, "Type memory text (Shift+Enter adds newline). Enter to store, Esc to cancel.");
    return true;
}

bool begin_memory_edit(Game *game, int index) {
    if (index < 0 || index >= game->memory_count || game->input.active) {
        return false;
    }
    MemoryEntry *entry = &game->memories[index];
    game->input.active = true;
    game->input.editing = true;
    game->input.edit_index = index;
    game->input.targetX = entry->grid_x;
    game->input.targetY = entry->grid_y;
    game->input.normalX = entry->normal_x;
    game->input.normalY = entry->normal_y;
    snprintf(game->input.buffer, sizeof(game->input.buffer), "%s", entry->text);
    game->input.length = strlen(game->input.buffer);
    refresh_text_input(game);
    set_hud_message(game, "Editing memory. Enter to save, Esc to cancel.");
    close_memory_view(game);
    return true;
}

void cancel_memory_input(Game *game) {
    if (!game->input.active) {
        return;
    }
    game->input.active = false;
    game->input.editing = false;
    game->input.edit_index = -1;
    refresh_text_input(game);
    set_hud_message(game, "Memory placement canceled.");
}

void finalize_memory_input(Game *game) {
    if (!game->input.active) {
        return;
    }
    const char *text = game->input.buffer[0] ? game->input.buffer : "Untitled memory";
    if (game->input.editing) {
        edit_memory_entry(game, game->input.edit_index, text);
        set_hud_message(game, "Memory updated.");
    } else {
        int idx = add_memory_entry_at(game, game->input.targetX, game->input.targetY, game->input.normalX,
                                      game->input.normalY, text);
        if (idx < 0) {
            cancel_memory_input(game);
            set_hud_message(game, "Unable to place memory.");
            return;
        }
        set_hud_message(game, "Memory stored.");
    }
    game->input.active = false;
    game->input.editing = false;
    game->input.edit_index = -1;
    refresh_text_input(game);
    if (game->has_save_path) {
        save_memories(game);
    }
}

void handle_memory_text(Game *game, const char *text) {
    if (!game->input.active || !text) {
        return;
    }
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 32 && c < 127 && game->input.length < MEMORY_TEXT - 1) {
            game->input.buffer[game->input.length++] = (char)c;
            game->input.buffer[game->input.length] = '\0';
        }
        text++;
    }
}

void memory_backspace(Game *game) {
    if (!game->input.active || game->input.length == 0) {
        return;
    }
    game->input.length--;
    game->input.buffer[game->input.length] = '\0';
}
