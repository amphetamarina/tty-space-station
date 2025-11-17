// Map management module
#include "map.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t strnlen_safe(const char *s, size_t maxlen) {
    if (!s) {
        return 0;
    }
    size_t len = 0;
    while (len < maxlen && s[len]) {
        len++;
    }
    return len;
}

void map_init(Map *map) {
    map->width = MAP_WIDTH;
    map->height = MAP_HEIGHT;
    map->spawn_set = false;
    map->spawn_x = map->spawn_y = 0;
    memset(map->decor, 0, sizeof(map->decor));
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            map->tiles[y][x] = '1';
        }
        map->tiles[y][MAP_WIDTH] = '\0';
    }
}

void map_enforce_border(Map *map) {
    for (int x = 0; x < MAP_WIDTH; ++x) {
        map->tiles[0][x] = '1';
        map->tiles[MAP_HEIGHT - 1][x] = '1';
        map->decor[0][x] = '\0';
        map->decor[MAP_HEIGHT - 1][x] = '\0';
    }
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        map->tiles[y][0] = '1';
        map->tiles[y][MAP_WIDTH - 1] = '1';
        map->decor[y][0] = '\0';
        map->decor[y][MAP_WIDTH - 1] = '\0';
    }
}

char sanitize_tile(char c) {
    switch (c) {
    case '.':
    case ' ':
        return '.';
    case ',':
        return ',';
    case ';':
        return ';';
    case 'T':
    case 't':
        return c;
    case 'R':
    case 'r':
        return 'R';
    case 'B':
    case 'b':
        return 'B';
    case 'S':
    case 's':
        return 'S';
    case 'W':
    case 'w':
        return 'W';
    case 'P':
    case 'p':
        return c;
    case 'G':
    case 'g':
        return c;
    case 'X':
        return 'X';
    case 'D':
        return 'D';
    case '1':
    case '2':
    case '3':
    case '4':
        return c;
    case '#':
    default:
        return '#';
    }
}

void map_apply_wall_styles(Map *map) {
    for (int y = 0; y < map->height; ++y) {
        for (int x = 0; x < map->width; ++x) {
            char *tile = &map->tiles[y][x];
            if (*tile == '.' || *tile == ',' || *tile == ';') {
                continue;
            }
            if (*tile == 'D' || *tile == 'X') {
                if (*tile == 'X') {
                    *tile = '.';
                }
                continue;
            }
            if (*tile >= '1' && *tile < '1' + NUM_WALL_TEXTURES) {
                continue;
            }
            int zone = ((x / 8) + (y / 12) * 3) % NUM_WALL_TEXTURES;
            if (zone < 0) {
                zone = 0;
            }
            *tile = (char)('1' + zone);
        }
    }
}

int tile_texture_index(char tile) {
    switch (tile) {
    case '1':
        return 0;
    case '2':
        return 1;
    case '3':
        return 2;
    case '4':
        return 3;
    default:
        return 0;
    }
}

int floor_index_for_char(char tile) {
    switch (tile) {
    case ',':
        return 1;
    case ';':
        return 2;
    default:
        return 0;
    }
}

bool tile_is_wall(char tile) {
    switch (tile) {
    case '.':
    case ',':
    case ';':
    case 'D':
        return false;
    default:
        return true;
    }
}

void map_store_char(Map *map, int x, int y, char raw) {
    if (x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT) {
        return;
    }
    char tile = sanitize_tile(raw);
    map->decor[y][x] = '\0';
    if (tile == 'X') {
        map->spawn_set = true;
        map->spawn_x = x;
        map->spawn_y = y;
        map->tiles[y][x] = '.';
    } else if (furniture_type_from_char(tile) != FURN_NONE) {
        map->decor[y][x] = tile;
        map->tiles[y][x] = '.';
    } else if (tile == 'P' || tile == 'p' || tile == 'G' || tile == 'g') {
        // NPC decor characters
        map->decor[y][x] = tile;
        map->tiles[y][x] = '.';
    } else {
        map->tiles[y][x] = tile;
    }
}

char map_export_char(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT) {
        return '#';
    }
    if (map->spawn_set && x == map->spawn_x && y == map->spawn_y) {
        return 'X';
    }
    if (map->decor[y][x]) {
        return map->decor[y][x];
    }
    return map->tiles[y][x];
}

int load_map_from_file(const char *path, Map *map) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return 0;
    }
    map_init(map);
    char line[MAP_WIDTH + 16];
    int y = 0;
    while (y < MAP_HEIGHT && fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\r\n");
        int usable = (len > (size_t)MAP_WIDTH) ? MAP_WIDTH : (int)len;
        for (int x = 0; x < usable; ++x) {
            map_store_char(map, x, y, line[x]);
        }
        for (int x = usable; x < MAP_WIDTH; ++x) {
            map->tiles[y][x] = '#';
            map->decor[y][x] = '\0';
        }
        map->tiles[y][MAP_WIDTH] = '\0';
        y++;
    }
    fclose(fp);
    if (y == 0) {
        return 0;
    }
    map->height = y < MAP_HEIGHT ? y : MAP_HEIGHT;
    map_enforce_border(map);
    map_apply_wall_styles(map);
    return 1;
}

static void carve_passages(Map *map, int steps) {
    int x = MAP_WIDTH / 2;
    int y = MAP_HEIGHT / 2;
    for (int i = 0; i < steps; ++i) {
        map->tiles[y][x] = '.';
        int dir = rand() % 4;
        if (dir == 0 && y > 1) {
            y--;
        } else if (dir == 1 && y < MAP_HEIGHT - 2) {
            y++;
        } else if (dir == 2 && x > 1) {
            x--;
        } else if (dir == 3 && x < MAP_WIDTH - 2) {
            x++;
        }
    }
}

void map_generate(Map *map) {
    map_init(map);
    carve_passages(map, MAP_WIDTH * MAP_HEIGHT * 4);
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT / 6; ++i) {
        int rx = rand() % (MAP_WIDTH - 2) + 1;
        int ry = rand() % (MAP_HEIGHT - 2) + 1;
        map->tiles[ry][rx] = '.';
    }
    map_enforce_border(map);
    map_apply_wall_styles(map);
}

void map_save_to_file(const Map *map, const char *path) {
    if (!path || !*path) {
        return;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return;
    }
    for (int y = 0; y < map->height; ++y) {
        for (int x = 0; x < map->width; ++x) {
            fputc(map_export_char(map, x, y), fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
}
