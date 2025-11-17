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

int map_allocate(Map *map, int width, int height) {
    if (width < 10 || height < 10 || width > 200 || height > 200) {
        return 0;  // Safety bounds check
    }

    // Allocate tiles array
    map->tiles = (char **)malloc(height * sizeof(char *));
    if (!map->tiles) {
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        map->tiles[y] = (char *)malloc((width + 1) * sizeof(char));
        if (!map->tiles[y]) {
            // Cleanup on failure
            for (int i = 0; i < y; ++i) {
                free(map->tiles[i]);
            }
            free(map->tiles);
            map->tiles = NULL;
            return 0;
        }
    }

    // Allocate decor array
    map->decor = (char **)malloc(height * sizeof(char *));
    if (!map->decor) {
        // Cleanup tiles on failure
        for (int y = 0; y < height; ++y) {
            free(map->tiles[y]);
        }
        free(map->tiles);
        map->tiles = NULL;
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        map->decor[y] = (char *)malloc(width * sizeof(char));
        if (!map->decor[y]) {
            // Cleanup on failure
            for (int i = 0; i < y; ++i) {
                free(map->decor[i]);
            }
            free(map->decor);
            map->decor = NULL;
            for (int i = 0; i < height; ++i) {
                free(map->tiles[i]);
            }
            free(map->tiles);
            map->tiles = NULL;
            return 0;
        }
    }

    map->width = width;
    map->height = height;
    return 1;
}

void map_free(Map *map) {
    if (map->tiles) {
        for (int y = 0; y < map->height; ++y) {
            if (map->tiles[y]) {
                free(map->tiles[y]);
            }
        }
        free(map->tiles);
        map->tiles = NULL;
    }

    if (map->decor) {
        for (int y = 0; y < map->height; ++y) {
            if (map->decor[y]) {
                free(map->decor[y]);
            }
        }
        free(map->decor);
        map->decor = NULL;
    }

    map->width = 0;
    map->height = 0;
}

void map_init(Map *map) {
    map->tiles = NULL;
    map->decor = NULL;
    map->width = 0;
    map->height = 0;
    map->spawn_set = false;
    map->spawn_x = map->spawn_y = 0;

    // Allocate default size (48x48 for backwards compatibility)
    if (!map_allocate(map, MAP_WIDTH, MAP_HEIGHT)) {
        return;
    }

    // Initialize all tiles to walls
    for (int y = 0; y < map->height; ++y) {
        for (int x = 0; x < map->width; ++x) {
            map->tiles[y][x] = '1';
            map->decor[y][x] = '\0';
        }
        map->tiles[y][map->width] = '\0';
    }
}

void map_enforce_border(Map *map) {
    for (int x = 0; x < map->width; ++x) {
        map->tiles[0][x] = '1';
        map->tiles[map->height - 1][x] = '1';
        map->decor[0][x] = '\0';
        map->decor[map->height - 1][x] = '\0';
    }
    for (int y = 0; y < map->height; ++y) {
        map->tiles[y][0] = '1';
        map->tiles[y][map->width - 1] = '1';
        map->decor[y][0] = '\0';
        map->decor[y][map->width - 1] = '\0';
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
    case 'C':
    case 'c':
        return c;
    case 'X':
        return 'X';
    case 'D':
    case 'd':
        return c;
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
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
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
    } else if (tile == 'C' || tile == 'c') {
        // Cabinet decor characters
        map->decor[y][x] = tile;
        map->tiles[y][x] = '.';
#if DEBUG_MODE
        printf("[DEBUG] Cabinet marker '%c' stored at decor[%d][%d]\n", tile, y, x);
#endif
    } else if (tile == 'D' || tile == 'd') {
        // Display wall tiles - keep as wall, not decor
        map->tiles[y][x] = tile;
#if DEBUG_MODE
        printf("[DEBUG] Display wall '%c' stored at tiles[%d][%d]\n", tile, y, x);
#endif
    } else {
        map->tiles[y][x] = tile;
    }
}

char map_export_char(const Map *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
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

    // First pass: count lines and determine max width
    char line[512];
    int height = 0;
    int max_width = 0;
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\r\n");
        if ((int)len > max_width) {
            max_width = (int)len;
        }
        height++;
    }

    if (height == 0 || max_width == 0) {
        fclose(fp);
        return 0;
    }

    // Clamp dimensions to safe bounds
    if (height < 10) height = 10;
    if (height > 200) height = 200;
    if (max_width < 10) max_width = 10;
    if (max_width > 200) max_width = 200;

    // Free any existing map data
    map_free(map);

    // Initialize map structure
    map->spawn_set = false;
    map->spawn_x = map->spawn_y = 0;

    // Allocate with detected dimensions
    if (!map_allocate(map, max_width, height)) {
        fclose(fp);
        return 0;
    }

    // Initialize all cells to walls
    for (int y = 0; y < map->height; ++y) {
        for (int x = 0; x < map->width; ++x) {
            map->tiles[y][x] = '#';
            map->decor[y][x] = '\0';
        }
        map->tiles[y][map->width] = '\0';
    }

    // Second pass: load the actual map data
    rewind(fp);
    int y = 0;
    while (y < height && fgets(line, sizeof(line), fp)) {
        size_t len = strcspn(line, "\r\n");
        int usable = (len > (size_t)max_width) ? max_width : (int)len;
        for (int x = 0; x < usable; ++x) {
            map_store_char(map, x, y, line[x]);
        }
        for (int x = usable; x < max_width; ++x) {
            map->tiles[y][x] = '#';
            map->decor[y][x] = '\0';
        }
        map->tiles[y][map->width] = '\0';
        y++;
    }

    fclose(fp);
    map_enforce_border(map);
    map_apply_wall_styles(map);
    return 1;
}

static void carve_passages(Map *map, int steps) {
    int x = map->width / 2;
    int y = map->height / 2;
    for (int i = 0; i < steps; ++i) {
        map->tiles[y][x] = '.';
        int dir = rand() % 4;
        if (dir == 0 && y > 1) {
            y--;
        } else if (dir == 1 && y < map->height - 2) {
            y++;
        } else if (dir == 2 && x > 1) {
            x--;
        } else if (dir == 3 && x < map->width - 2) {
            x++;
        }
    }
}

void map_generate(Map *map) {
    map_init(map);
    carve_passages(map, map->width * map->height * 4);
    for (int i = 0; i < map->width * map->height / 6; ++i) {
        int rx = rand() % (map->width - 2) + 1;
        int ry = rand() % (map->height - 2) + 1;
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
