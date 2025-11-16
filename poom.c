#include <math.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include "font8x8_basic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#define MAP_WIDTH 48
#define MAP_HEIGHT 48
#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 600
#define MAX_DEPTH 32.0
#define MOVE_SPEED 3.7
#define STRAFE_SPEED 3.0
#define ROT_SPEED 2.4
#define MAX_MEMORIES 96
#define MEMORY_TEXT 160
#define FOV (M_PI / 3.0)

#define TEX_SIZE 64
#define NUM_WALL_TEXTURES 4
#define NUM_FLOOR_TEXTURES 3
#define NUM_CEIL_TEXTURES 2
#define MAX_LAYOUT_LINES 32
#define INPUT_MAX_LINES 10
#define NET_BUFFER 4096
#define CHAT_HISTORY 10
#define CHAT_TEXT 200

#define MAP_FILE_DEFAULT "palace.map"

#define MAX_FURNITURE 64

#define clamp_int(value, min_value, max_value) ((value) < (min_value) ? (min_value) : ((value) > (max_value) ? (max_value) : (value)))
#define PACK_COLOR_LITERAL(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

typedef struct {
    double x;
    double y;
    double angle;
    double fov;
} Player;

typedef struct {
    int width;
    int height;
    char tiles[MAP_HEIGHT][MAP_WIDTH + 1];
    char decor[MAP_HEIGHT][MAP_WIDTH];
    bool spawn_set;
    int spawn_x;
    int spawn_y;
} Map;

typedef struct {
    double x;
    double y;
    int grid_x;
    int grid_y;
    double normal_x;
    double normal_y;
    char text[MEMORY_TEXT];
} MemoryEntry;

typedef enum {
    FURN_NONE = 0,
    FURN_TABLE_SQUARE,
    FURN_TABLE_ROUND,
    FURN_BED,
    FURN_SOFA,
    FURN_WARDROBE
} FurnitureType;

typedef struct {
    FurnitureType type;
    int grid_x;
    int grid_y;
    double x;
    double y;
    double angle;
} FurnitureEntry;

typedef struct {
    const char *name;
    const char *asset;
    double half_width;
    double half_depth;
    double height;
    uint32_t primary_color;
    uint32_t detail_color;
} FurnitureSpec;

typedef struct {
    bool active;
    int targetX;
    int targetY;
    double hitX;
    double hitY;
    double normalX;
    double normalY;
    size_t length;
    char buffer[MEMORY_TEXT];
    bool editing;
    int edit_index;
} MemoryInput;

typedef struct {
    int id;
    char name[32];
    double x;
    double y;
    double angle;
    bool active;
} RemotePlayer;

typedef enum {
    NET_NONE = 0,
    NET_HOST,
    NET_CLIENT
} NetworkMode;

typedef enum {
    CLIENT_STAGE_WAIT_MAP = 0,
    CLIENT_STAGE_MAP_ROWS,
    CLIENT_STAGE_WAIT_MEM,
    CLIENT_STAGE_MEM_ROWS,
    CLIENT_STAGE_READY
} ClientStage;

typedef struct {
    NetworkMode mode;
    int port;
    char host_addr[128];
    char player_name[32];
    int listen_fd;
    int peer_fd;
    char peer_buffer[NET_BUFFER];
    size_t peer_buffer_len;
    bool connected;
    int self_id;
    RemotePlayer remote;
    double last_state_send;
    bool awaiting_map;
    bool awaiting_mem;
    int expected_map_rows;
    int received_map_rows;
    int expected_mem_entries;
    int received_mem_entries;
    int pending_map_height;
    char pending_map[MAP_HEIGHT][MAP_WIDTH + 1];
    ClientStage client_stage;
} NetworkState;

typedef struct {
    char sender[32];
    char text[CHAT_TEXT];
    double ttl;
} ChatMessage;

typedef struct {
    Map map;
    Player player;
    MemoryEntry memories[MAX_MEMORIES];
    int memory_map[MAP_HEIGHT][MAP_WIDTH];
    int door_state[MAP_HEIGHT][MAP_WIDTH];
    int memory_count;
    FurnitureEntry furniture[MAX_FURNITURE];
    int furniture_count;
    MemoryInput input;
    char hud_message[128];
    double hud_message_timer;
    char save_path[512];
    bool has_save_path;
    bool viewer_active;
    int viewer_index;
    bool viewer_delete_prompt;
    NetworkState net;
    ChatMessage chat_log[CHAT_HISTORY];
    int chat_count;
    bool chat_input_active;
    size_t chat_input_len;
    char chat_input[CHAT_TEXT];
} Game;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *framebuffer;
} Video;

static uint32_t wall_textures[NUM_WALL_TEXTURES][TEX_SIZE * TEX_SIZE];
static uint32_t floor_textures[NUM_FLOOR_TEXTURES][TEX_SIZE * TEX_SIZE];
static uint32_t ceiling_textures[NUM_CEIL_TEXTURES][TEX_SIZE * TEX_SIZE];
static uint32_t door_texture[TEX_SIZE * TEX_SIZE];
#define NUM_FURNITURE_TYPES (FURN_WARDROBE + 1)
static uint32_t furniture_textures[NUM_FURNITURE_TYPES][TEX_SIZE * TEX_SIZE];

static void video_destroy(Video *video);
static void load_memories(Game *game);
static void save_memories(const Game *game);
static void network_init(Game *game);
static void network_shutdown(Game *game);
static void network_update(Game *game, double delta);
static void map_apply_wall_styles(Map *map);
static void network_notify_memory_add(Game *game, int index);
static void network_notify_memory_edit(Game *game, int index);
static void network_notify_memory_delete(Game *game, int index);
static void network_notify_memory_edit(Game *game, int index);
static void network_notify_memory_delete(Game *game, int index);
static void network_send_request_add(Game *game, int gx, int gy, double nx, double ny, const char *text);
static void network_send_request_edit(Game *game, int index, const char *text);
static void network_send_request_delete(Game *game, int index);
static int add_memory_entry_at(Game *game, int gx, int gy, double nx, double ny, const char *text);
static void edit_memory_entry(Game *game, int index, const char *text);
static void network_host_send_chat(Game *game, const char *sender, const char *text);
static void network_client_send_chat(Game *game, const char *text);
static void add_chat_message(Game *game, const char *sender, const char *text);
static void cancel_chat_input(Game *game);
static void submit_chat_input(Game *game);
static bool toggle_door_state(Game *game, int gx, int gy, bool notify);
static bool door_is_passable(const Game *game, int gx, int gy);
static void network_notify_door(Game *game, int gx, int gy, int state);
static void network_send_request_door(Game *game, int gx, int gy);
static bool load_texture_from_bmp(const char *path, uint32_t *target);
static void load_custom_textures(void);

static uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static uint32_t blend_colors(uint32_t base, uint32_t overlay, double alpha) {
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

static const FurnitureSpec furniture_specs[NUM_FURNITURE_TYPES] = {
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

static const FurnitureSpec *furniture_spec(FurnitureType type) {
    if (type <= FURN_NONE || type >= NUM_FURNITURE_TYPES) {
        return NULL;
    }
    return &furniture_specs[type];
}

static FurnitureType furniture_type_from_char(char c) {
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

static double furniture_angle_for_marker(char marker) {
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

static const char *furniture_display_name(FurnitureType type) {
    const FurnitureSpec *spec = furniture_spec(type);
    return spec && spec->name ? spec->name : "Furniture";
}

static void generate_wall_textures(void) {
    for (int t = 0; t < NUM_WALL_TEXTURES; ++t) {
        for (int y = 0; y < TEX_SIZE; ++y) {
            for (int x = 0; x < TEX_SIZE; ++x) {
                uint32_t color = 0;
                if (t == 0) {
                    int checker = ((x / 8) + (y / 8)) & 1;
                    color = checker ? pack_color(90, 90, 120) : pack_color(140, 140, 180);
                } else if (t == 1) {
                    int stripe = (x % 16 < 8);
                    color = stripe ? pack_color(80, 120, 80) : pack_color(50, 80, 50);
                } else {
                    int brick = ((x % 32) < 28 && (y % 16) < 14);
                    color = brick ? pack_color(120, 60, 50) : pack_color(50, 25, 20);
                }
                wall_textures[t][y * TEX_SIZE + x] = color;
            }
        }
    }
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int stripe = (x % 8 < 4);
            door_texture[y * TEX_SIZE + x] = stripe ? pack_color(130, 90, 40) : pack_color(160, 110, 60);
        }
    }
}

static void generate_floor_textures(void) {
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int checker = (x / 8 + y / 8) & 1;
            uint32_t stone = checker ? pack_color(70, 60, 50) : pack_color(90, 80, 70);
            floor_textures[0][y * TEX_SIZE + x] = stone;

            double noise = sin((x + 1) * 0.4) + cos((y + 3) * 0.3);
            uint8_t color = (uint8_t)(100 + noise * 30);
            floor_textures[1][y * TEX_SIZE + x] = pack_color(color, color, color + 20);

            double marble = sin((x + y) * 0.2) + cos((x - y) * 0.25);
            uint8_t marbleColor = (uint8_t)(150 + marble * 40);
            floor_textures[2][y * TEX_SIZE + x] = pack_color(marbleColor, marbleColor, marbleColor + 30);
        }
    }
}

static void generate_ceiling_textures(void) {
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            double swirl = sin((x + y) * 0.15);
            uint8_t base = (uint8_t)(40 + swirl * 20);
            ceiling_textures[0][y * TEX_SIZE + x] = pack_color(base, base + 10, base + 20);

            double stripe = (sin(x * 0.3) + 1.0) * 0.5;
            uint8_t c = (uint8_t)(80 + stripe * 40);
            ceiling_textures[1][y * TEX_SIZE + x] = pack_color(c, c, c + 30);
        }
    }
}

static void generate_furniture_textures(void) {
    for (int t = 0; t < NUM_FURNITURE_TYPES; ++t) {
        if (t == FURN_NONE) {
            continue;
        }
        const FurnitureSpec *spec = furniture_spec((FurnitureType)t);
        uint32_t primary = spec && spec->primary_color ? spec->primary_color : pack_color(120, 120, 120);
        uint32_t detail = spec && spec->detail_color ? spec->detail_color : pack_color(180, 180, 180);
        for (int y = 0; y < TEX_SIZE; ++y) {
            for (int x = 0; x < TEX_SIZE; ++x) {
                double gradient = (double)y / (double)TEX_SIZE;
                double noise = ((x ^ y) & 7) / 8.0;
                double mix = gradient * 0.5 + noise * 0.2;
                furniture_textures[t][y * TEX_SIZE + x] = blend_colors(primary, detail, mix);
            }
        }
    }
}

static bool load_texture_from_bmp(const char *path, uint32_t *target) {
    if (!path || !*path || !target) {
        return false;
    }
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (!surface) {
        return false;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        return false;
    }
    uint32_t *src = (uint32_t *)converted->pixels;
    int pitch = converted->pitch / 4;
    for (int y = 0; y < TEX_SIZE; ++y) {
        int srcY = y * converted->h / TEX_SIZE;
        for (int x = 0; x < TEX_SIZE; ++x) {
            int srcX = x * converted->w / TEX_SIZE;
            target[y * TEX_SIZE + x] = src[srcY * pitch + srcX];
        }
    }
    SDL_FreeSurface(converted);
    return true;
}

static void load_custom_textures(void) {
    char path[256];
    for (int i = 0; i < NUM_WALL_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/wall%d.bmp", i);
        load_texture_from_bmp(path, wall_textures[i]);
    }
    for (int i = 0; i < NUM_FLOOR_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/floor%d.bmp", i);
        load_texture_from_bmp(path, floor_textures[i]);
    }
    for (int i = 0; i < NUM_CEIL_TEXTURES; ++i) {
        snprintf(path, sizeof(path), "assets/ceiling%d.bmp", i);
        load_texture_from_bmp(path, ceiling_textures[i]);
    }
    snprintf(path, sizeof(path), "assets/door.bmp");
    load_texture_from_bmp(path, door_texture);
    for (int i = 1; i < NUM_FURNITURE_TYPES; ++i) {
        const FurnitureSpec *spec = furniture_spec((FurnitureType)i);
        if (!spec || !spec->asset) {
            continue;
        }
        snprintf(path, sizeof(path), "assets/%s", spec->asset);
        load_texture_from_bmp(path, furniture_textures[i]);
    }
}

static void map_init(Map *map) {
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

static void map_enforce_border(Map *map) {
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

static char sanitize_tile(char c) {
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

static void map_apply_wall_styles(Map *map) {
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

static inline int tile_texture_index(char tile) {
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

static int floor_index_for_char(char tile) {
    switch (tile) {
    case ',':
        return 1;
    case ';':
        return 2;
    default:
        return 0;
    }
}

static inline bool tile_is_wall(char tile) {
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

static void map_store_char(Map *map, int x, int y, char raw) {
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
    } else {
        map->tiles[y][x] = tile;
    }
}

static char map_export_char(const Map *map, int x, int y) {
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

static int load_map_from_file(const char *path, Map *map) {
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

static void map_generate(Map *map) {
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

static void map_save_to_file(const Map *map, const char *path) {
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

static void rebuild_furniture(Game *game) {
    game->furniture_count = 0;
    for (int y = 0; y < game->map.height && y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < game->map.width && x < MAP_WIDTH; ++x) {
            if (game->furniture_count >= MAX_FURNITURE) {
                return;
            }
            char marker = game->map.decor[y][x];
            FurnitureType type = furniture_type_from_char(marker);
            if (type == FURN_NONE) {
                continue;
            }
            FurnitureEntry *entry = &game->furniture[game->furniture_count++];
            entry->type = type;
            entry->grid_x = x;
            entry->grid_y = y;
            entry->x = x + 0.5;
            entry->y = y + 0.5;
            entry->angle = furniture_angle_for_marker(marker);
        }
    }
}

static void game_reset_memory(Game *game) {
    game->memory_count = 0;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            game->memory_map[y][x] = -1;
        }
    }
    game->input.active = false;
    game->input.length = 0;
    game->input.buffer[0] = '\0';
    game->input.editing = false;
    game->input.edit_index = -1;
    game->hud_message[0] = '\0';
    game->hud_message_timer = 0.0;
    game->viewer_active = false;
    game->viewer_index = -1;
    game->viewer_delete_prompt = false;
    game->chat_count = 0;
    game->chat_input_active = false;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            game->door_state[y][x] = (game->map.tiles[y][x] == 'D') ? 0 : -1;
        }
    }
}

static void game_pick_spawn(Game *game) {
    const Map *map = &game->map;
    if (map->spawn_set) {
        game->player.x = map->spawn_x + 0.5;
        game->player.y = map->spawn_y + 0.5;
        return;
    }
    for (int y = 1; y < map->height - 1; ++y) {
        for (int x = 1; x < map->width - 1; ++x) {
            if (map->tiles[y][x] == '.') {
                game->player.x = x + 0.5;
                game->player.y = y + 0.5;
                return;
            }
        }
    }
    game->player.x = 2.5;
    game->player.y = 2.5;
}

static void game_init(Game *game) {
    memset(game, 0, sizeof(*game));
    const char *custom_map = getenv("POOM_MAP_FILE");
    const char *custom_save = getenv("POOM_SAVE_FILE");
    const char *generated_out = getenv("POOM_GENERATED_MAP");
    const char *map_source = NULL;
    if (custom_map && load_map_from_file(custom_map, &game->map)) {
        map_source = custom_map;
    } else if (load_map_from_file(MAP_FILE_DEFAULT, &game->map)) {
        map_source = MAP_FILE_DEFAULT;
    } else {
        map_generate(&game->map);
        if (generated_out && *generated_out) {
            map_save_to_file(&game->map, generated_out);
            map_source = generated_out;
        }
    }
    game_reset_memory(game);
    game->player.angle = 0.0;
    game->player.fov = FOV;
    game_pick_spawn(game);
    rebuild_furniture(game);
    if (custom_save && *custom_save) {
        snprintf(game->save_path, sizeof(game->save_path), "%s", custom_save);
        game->has_save_path = true;
        load_memories(game);
    } else if (map_source) {
        snprintf(game->save_path, sizeof(game->save_path), "%s.mem", map_source);
        game->has_save_path = true;
        load_memories(game);
    } else {
        game->has_save_path = false;
    }
    game->net.mode = NET_NONE;
    game->net.listen_fd = -1;
    game->net.peer_fd = -1;
    game->net.self_id = 1;
    snprintf(game->net.player_name, sizeof(game->net.player_name), "%s",
             getenv("POOM_PLAYER_NAME") ? getenv("POOM_PLAYER_NAME") : "Explorer");
    game->net.connected = false;
    game->net.remote.active = false;
    game->net.remote.name[0] = '\0';
}

static void normalize_angle(double *angle) {
    while (*angle < 0.0) {
        *angle += 2.0 * M_PI;
    }
    while (*angle >= 2.0 * M_PI) {
        *angle -= 2.0 * M_PI;
    }
}

static bool furniture_blocks_point(const FurnitureEntry *entry, double px, double py) {
    if (!entry || entry->type <= FURN_NONE) {
        return false;
    }
    const FurnitureSpec *spec = furniture_spec(entry->type);
    if (!spec) {
        return false;
    }
    double dx = px - entry->x;
    double dy = py - entry->y;
    double cosA = cos(entry->angle);
    double sinA = sin(entry->angle);
    double localX = cosA * dx + sinA * dy;
    double localY = -sinA * dx + cosA * dy;
    double margin = 0.05;
    return fabs(localX) <= spec->half_width + margin && fabs(localY) <= spec->half_depth + margin;
}

static bool furniture_blocks_position(const Game *game, double x, double y) {
    for (int i = 0; i < game->furniture_count; ++i) {
        if (furniture_blocks_point(&game->furniture[i], x, y)) {
            return true;
        }
    }
    return false;
}

static int can_move(const Game *game, double nx, double ny) {
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
    return 1;
}

static void move_player(Game *game, double dx, double dy) {
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

static bool ray_pick_wall(const Game *game, double angle, int *wallX, int *wallY, double *hitX, double *hitY,
                          double *normalX, double *normalY) {
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


static void draw_pixel(uint32_t *pixels, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
        return;
    }
    pixels[y * SCREEN_WIDTH + x] = color;
}

static void draw_rect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    int endX = x + w;
    int endY = y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (endX > SCREEN_WIDTH) {
        endX = SCREEN_WIDTH;
    }
    if (endY > SCREEN_HEIGHT) {
        endY = SCREEN_HEIGHT;
    }
    for (int yy = y; yy < endY; ++yy) {
        for (int xx = x; xx < endX; ++xx) {
            pixels[yy * SCREEN_WIDTH + xx] = color;
        }
    }
}

static void draw_text(uint32_t *pixels, int x, int y, const char *text, uint32_t color) {
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char ch = *p;
        if (ch >= 128) {
            ch = '?';
        }
        for (int row = 0; row < 8; ++row) {
            unsigned char bits = font8x8_basic[ch][row];
            for (int col = 0; col < 8; ++col) {
                if (bits & (1 << col)) {
                    draw_pixel(pixels, x + col, y + row, color);
                }
            }
        }
        x += 8;
    }
}

static int raw_longest_line(const char *text) {
    int max_len = 0;
    int current = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            if (current > max_len) {
                max_len = current;
            }
            current = 0;
        } else {
            current++;
        }
    }
    if (current > max_len) {
        max_len = current;
    }
    if (max_len <= 0) {
        max_len = 1;
    }
    return max_len;
}

static int layout_text_lines(const char *text, int chars_per_line, char lines[][MEMORY_TEXT], int max_lines,
                             int *longest_line) {
    if (!text || max_lines <= 0) {
        if (longest_line) {
            *longest_line = 0;
        }
        return 0;
    }
    if (chars_per_line < 1) {
        chars_per_line = 1;
    }
    char temp[MEMORY_TEXT];
    snprintf(temp, sizeof(temp), "%s", text);
    int count = 0;
    int longest = 0;
    char *segment = temp;
    while (segment && count < max_lines) {
        char *newline = strchr(segment, '\n');
        if (newline) {
            *newline = '\0';
        }
        size_t len = strlen(segment);
        size_t offset = 0;
        if (len == 0) {
            lines[count][0] = '\0';
            count++;
        }
        while (offset < len && count < max_lines) {
            size_t chunk = len - offset;
            if (chunk > (size_t)chars_per_line) {
                size_t break_pos = offset + chars_per_line;
                size_t last_space = break_pos;
                while (last_space > offset && segment[last_space] != ' ') {
                    last_space--;
                }
                if (last_space == offset) {
                    last_space = break_pos;
                }
                chunk = last_space - offset;
            }
            if (chunk >= sizeof(lines[0])) {
                chunk = sizeof(lines[0]) - 1;
            }
            memcpy(lines[count], segment + offset, chunk);
            lines[count][chunk] = '\0';
            if ((int)chunk > longest) {
                longest = (int)chunk;
            }
            offset += chunk;
            while (offset < len && segment[offset] == ' ') {
                offset++;
            }
            count++;
        }
        if (!newline) {
            break;
        }
        segment = newline + 1;
    }
    if (count == 0) {
        lines[0][0] = '\0';
        count = 1;
    }
    if (longest_line) {
        *longest_line = longest;
    }
    return count;
}

static void encode_text(const char *src, char *dst, size_t dst_size) {
    size_t pos = 0;
    while (*src && pos + 1 < dst_size) {
        unsigned char c = (unsigned char)*src++;
        if (c == '\n') {
            dst[pos++] = '\r';
        } else if (c == '|') {
            dst[pos++] = '\x1f';
        } else {
            dst[pos++] = (char)c;
        }
    }
    dst[pos] = '\0';
}

static void decode_text(const char *src, char *dst, size_t dst_size) {
    size_t pos = 0;
    while (*src && pos + 1 < dst_size) {
        unsigned char c = (unsigned char)*src++;
        if (c == '\r') {
            dst[pos++] = '\n';
        } else if (c == '\x1f') {
            dst[pos++] = '|';
        } else {
            dst[pos++] = (char)c;
        }
    }
    dst[pos] = '\0';
}

static int parse_port(const char *value, int fallback) {
    if (!value || !*value) {
        return fallback;
    }
    int p = atoi(value);
    if (p <= 0 || p > 65535) {
        return fallback;
    }
    return p;
}

static void refresh_text_input(Game *game) {
    if (game->input.active || game->chat_input_active) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

static ssize_t net_send_all(int fd, const char *data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(fd, data + total, len - total, 0);
        if (sent <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += (size_t)sent;
    }
    return (ssize_t)total;
}

static void net_sendf(int fd, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    net_send_all(fd, buffer, strlen(buffer));
}


static void render_minimap(uint32_t *pixels, const Game *game) {
    int scale = 4;
    int offsetX = 10;
    int offsetY = 10;
    for (int y = 0; y < game->map.height; ++y) {
        for (int x = 0; x < game->map.width; ++x) {
            uint32_t color = tile_is_wall(game->map.tiles[y][x]) ? pack_color(40, 40, 40) : pack_color(25, 70, 25);
            if (game->door_state[y][x] != -1) {
                color = game->door_state[y][x] ? pack_color(70, 190, 90) : pack_color(70, 70, 160);
            }
            if (game->memory_map[y][x] >= 0) {
                color = pack_color(200, 120, 20);
            } else if (game->input.active && y == game->input.targetY && x == game->input.targetX) {
                color = pack_color(80, 180, 200);
            }
            for (int yy = 0; yy < scale; ++yy) {
                for (int xx = 0; xx < scale; ++xx) {
                    draw_pixel(pixels, offsetX + x * scale + xx, offsetY + y * scale + yy, color);
                }
            }
        }
    }
    int px = (int)(game->player.x * scale);
    int py = (int)(game->player.y * scale);
    for (int yy = -2; yy <= 2; ++yy) {
        for (int xx = -2; xx <= 2; ++xx) {
            draw_pixel(pixels, offsetX + px + xx, offsetY + py + yy, pack_color(255, 255, 255));
        }
    }
    if (game->net.remote.active) {
        int rx = (int)(game->net.remote.x * scale);
        int ry = (int)(game->net.remote.y * scale);
        for (int yy = -2; yy <= 2; ++yy) {
            for (int xx = -2; xx <= 2; ++xx) {
                draw_pixel(pixels, offsetX + rx + xx, offsetY + ry + yy, pack_color(200, 80, 80));
            }
        }
    }
    for (int i = 0; i < game->furniture_count; ++i) {
        int fx = (int)(game->furniture[i].x * scale);
        int fy = (int)(game->furniture[i].y * scale);
        uint32_t iconColor = pack_color(180, 140, 90);
        switch (game->furniture[i].type) {
        case FURN_BED:
            iconColor = pack_color(100, 120, 200);
            break;
        case FURN_SOFA:
            iconColor = pack_color(180, 60, 60);
            break;
        case FURN_WARDROBE:
            iconColor = pack_color(150, 110, 70);
            break;
        default:
            break;
        }
        for (int yy = -1; yy <= 1; ++yy) {
            for (int xx = -1; xx <= 1; ++xx) {
                draw_pixel(pixels, offsetX + fx + xx, offsetY + fy + yy, iconColor);
            }
        }
    }
}

static int render_furniture(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX,
                            double planeY, double *zbuffer) {
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

static void gather_nearby(const Game *game, char lines[3][128]) {
    int count = 0;
    for (int i = 0; i < game->memory_count && count < 3; ++i) {
        double dx = game->memories[i].x - game->player.x;
        double dy = game->memories[i].y - game->player.y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist < 6.0) {
            snprintf(lines[count], sizeof(lines[count]), "Memory %d (%.1f) %s", i + 1, dist, game->memories[i].text);
            count++;
        }
    }
    for (int i = count; i < 3; ++i) {
        snprintf(lines[i], sizeof(lines[i]), "%s", "");
    }
}

static void render_memory_plaques(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX,
                                  double planeY, double *zbuffer) {
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
        if (transformY <= 0.1) {
            continue;
        }
        int screenX = (int)((SCREEN_WIDTH / 2) * (1 + transformX / transformY));
        if (screenX < 0 || screenX >= SCREEN_WIDTH) {
            continue;
        }
        if (transformY > zbuffer[screenX] + 0.1) {
            continue;
        }
        int baseWidth = clamp_int(((raw_longest_line(entry->text) + 4) * 8), 140, 360);
        int chars_per_line = clamp_int((baseWidth - 24) / 8, 4, 40);
        char lines[MAX_LAYOUT_LINES][MEMORY_TEXT];
        int longestLine = 0;
        int lineCount = layout_text_lines(entry->text, chars_per_line, lines, 6, &longestLine);
        if (lineCount <= 0) {
            lineCount = 1;
            lines[0][0] = '\0';
        }
        int widthFromText = clamp_int(((longestLine + 4) * 8), 140, 360);
        if (widthFromText > baseWidth) {
            baseWidth = widthFromText;
        }
        chars_per_line = clamp_int((baseWidth - 24) / 8, 4, 40);
        lineCount = layout_text_lines(entry->text, chars_per_line, lines, 6, &longestLine);
        if (lineCount <= 0) {
            lineCount = 1;
            lines[0][0] = '\0';
        }
        int plaqueWidth = baseWidth;
        int plaqueHeight = lineCount * 14 + 28;
        int screenY = SCREEN_HEIGHT / 2 - plaqueHeight;
        int boxX = screenX - plaqueWidth / 2;
        int boxY = screenY;
        draw_rect(pixels, boxX - 2, boxY - 2, plaqueWidth + 4, plaqueHeight + 4, pack_color(10, 10, 20));
        draw_rect(pixels, boxX, boxY, plaqueWidth, plaqueHeight, pack_color(25, 25, 50));
        draw_rect(pixels, boxX + 2, boxY + 2, plaqueWidth - 4, plaqueHeight - 4, pack_color(40, 40, 70));
        for (int line = 0; line < lineCount; ++line) {
            draw_text(pixels, boxX + 10, boxY + 10 + line * 14, lines[line], pack_color(240, 220, 170));
        }
    }
}

static void render_remote_player(const Game *game, uint32_t *pixels, double dirX, double dirY, double planeX,
                                 double planeY, double *zbuffer) {
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

static void set_hud_message(Game *game, const char *msg) {
    if (!msg) {
        game->hud_message[0] = '\0';
        game->hud_message_timer = 0.0;
        return;
    }
    snprintf(game->hud_message, sizeof(game->hud_message), "%s", msg);
    game->hud_message_timer = 3.0;
}

static void render_memory_viewer(const Game *game, uint32_t *pixels) {
    if (!game->viewer_active || game->viewer_index < 0 || game->viewer_index >= game->memory_count) {
        return;
    }
    const MemoryEntry *entry = &game->memories[game->viewer_index];
    int boxW = SCREEN_WIDTH - 120;
    if (boxW < 400) {
        boxW = 400;
    }
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = 60;
    int textWidth = boxW - 40;
    int chars_per_line = clamp_int(textWidth / 8, 8, 60);
    char lines[MAX_LAYOUT_LINES][MEMORY_TEXT];
    int lineCount = layout_text_lines(entry->text, chars_per_line, lines, MAX_LAYOUT_LINES - 4, NULL);
    if (lineCount <= 0) {
        lineCount = 1;
        lines[0][0] = '\0';
    }
    int boxH = lineCount * 14 + 160;
    if (boxH > SCREEN_HEIGHT - 100) {
        boxH = SCREEN_HEIGHT - 100;
    }
    draw_rect(pixels, boxX - 4, boxY - 4, boxW + 8, boxH + 8, pack_color(5, 5, 15));
    draw_rect(pixels, boxX, boxY, boxW, boxH, pack_color(15, 15, 30));
    draw_rect(pixels, boxX + 4, boxY + 4, boxW - 8, boxH - 8, pack_color(25, 25, 60));
    char title[128];
    snprintf(title, sizeof(title), "Memory #%d at (%d, %d)", game->viewer_index + 1, entry->grid_x, entry->grid_y);
    draw_text(pixels, boxX + 20, boxY + 20, title, pack_color(255, 255, 255));
    for (int i = 0; i < lineCount; ++i) {
        draw_text(pixels, boxX + 20, boxY + 48 + i * 14, lines[i], pack_color(235, 220, 180));
    }
    int instructionsY = boxY + boxH - 40;
    if (game->viewer_delete_prompt) {
        draw_text(pixels, boxX + 20, instructionsY, "Delete memory? Y to confirm, N to cancel.",
                  pack_color(255, 180, 180));
    } else {
        draw_text(pixels, boxX + 20, instructionsY, "E edit   Delete remove   Esc close", pack_color(200, 210, 255));
    }
}

static bool open_memory_view(Game *game, int index) {
    if (index < 0 || index >= game->memory_count) {
        return false;
    }
    game->viewer_active = true;
    game->viewer_index = index;
    game->viewer_delete_prompt = false;
    set_hud_message(game, "Viewing memory. E edit, Delete remove, Esc close.");
    return true;
}

static void close_memory_view(Game *game) {
    game->viewer_active = false;
    game->viewer_index = -1;
    game->viewer_delete_prompt = false;
}

static bool target_memory(const Game *game, int *index_out) {
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

static bool begin_memory_input(Game *game) {
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

static bool begin_memory_edit(Game *game, int index) {
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

static void cancel_memory_input(Game *game) {
    if (!game->input.active) {
        return;
    }
    game->input.active = false;
    game->input.editing = false;
    game->input.edit_index = -1;
    refresh_text_input(game);
    set_hud_message(game, "Memory placement canceled.");
}

static void finalize_memory_input(Game *game) {
    if (!game->input.active) {
        return;
    }
    const char *text = game->input.buffer[0] ? game->input.buffer : "Untitled memory";
    if (game->net.mode == NET_CLIENT) {
        if (game->input.editing) {
            network_send_request_edit(game, game->input.edit_index, text);
        } else {
            network_send_request_add(game, game->input.targetX, game->input.targetY, game->input.normalX,
                                     game->input.normalY, text);
        }
        game->input.active = false;
        game->input.editing = false;
        refresh_text_input(game);
        set_hud_message(game, "Sent request to host.");
        return;
    }
    if (game->input.editing) {
        edit_memory_entry(game, game->input.edit_index, text);
        set_hud_message(game, "Memory updated.");
        network_notify_memory_edit(game, game->input.edit_index);
    } else {
        int idx = add_memory_entry_at(game, game->input.targetX, game->input.targetY, game->input.normalX,
                                      game->input.normalY, text);
        if (idx < 0) {
            cancel_memory_input(game);
            set_hud_message(game, "Unable to place memory.");
            return;
        }
        set_hud_message(game, "Memory stored.");
        network_notify_memory_add(game, idx);
    }
    game->input.active = false;
    game->input.editing = false;
    game->input.edit_index = -1;
    refresh_text_input(game);
    if (game->has_save_path) {
        save_memories(game);
    }
}

static void handle_memory_text(Game *game, const char *text) {
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

static void memory_backspace(Game *game) {
    if (!game->input.active || game->input.length == 0) {
        return;
    }
    game->input.length--;
    game->input.buffer[game->input.length] = '\0';
}

static void handle_chat_text(Game *game, const char *text) {
    if (!game->chat_input_active || !text) {
        return;
    }
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 32 && c < 127 && game->chat_input_len < CHAT_TEXT - 1) {
            game->chat_input[game->chat_input_len++] = (char)c;
            game->chat_input[game->chat_input_len] = '\0';
        }
        text++;
    }
}

static int add_memory_entry_at(Game *game, int gx, int gy, double nx, double ny, const char *text) {
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

static void edit_memory_entry(Game *game, int index, const char *text) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    MemoryEntry *entry = &game->memories[index];
    snprintf(entry->text, sizeof(entry->text), "%s", (text && text[0]) ? text : "Untitled memory");
}

static void delete_memory_internal(Game *game, int index, bool notify_network) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    int gx = game->memories[index].grid_x;
    int gy = game->memories[index].grid_y;
    if (gy >= 0 && gy < MAP_HEIGHT && gx >= 0 && gx < MAP_WIDTH && game->memory_map[gy][gx] == index) {
        game->memory_map[gy][gx] = -1;
    }
    for (int i = index + 1; i < game->memory_count; ++i) {
        game->memories[i - 1] = game->memories[i];
        int wx = game->memories[i - 1].grid_x;
        int wy = game->memories[i - 1].grid_y;
        if (wy >= 0 && wy < MAP_HEIGHT && wx >= 0 && wx < MAP_WIDTH) {
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
    if (notify_network) {
        network_notify_memory_delete(game, index);
    }
}

static void delete_memory(Game *game, int index) {
    if (index < 0 || index >= game->memory_count) {
        return;
    }
    if (game->net.mode == NET_CLIENT) {
        network_send_request_delete(game, index);
        set_hud_message(game, "Delete request sent to host.");
        return;
    }
    delete_memory_internal(game, index, true);
    set_hud_message(game, "Memory deleted.");
}

static void add_chat_message(Game *game, const char *sender, const char *text) {
    if (!sender || !*sender) {
        sender = "Player";
    }
    if (!text) {
        text = "";
    }
    if (game->chat_count >= CHAT_HISTORY) {
        memmove(&game->chat_log[0], &game->chat_log[1], sizeof(ChatMessage) * (CHAT_HISTORY - 1));
        game->chat_count = CHAT_HISTORY - 1;
    }
    ChatMessage *msg = &game->chat_log[game->chat_count++];
    snprintf(msg->sender, sizeof(msg->sender), "%s", sender);
    snprintf(msg->text, sizeof(msg->text), "%s", text);
    msg->ttl = 8.0;
}

static bool begin_chat_input(Game *game) {
    if (game->chat_input_active || game->input.active || game->viewer_active) {
        return false;
    }
    game->chat_input_active = true;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    refresh_text_input(game);
    set_hud_message(game, "Chat: Enter to send, Esc to cancel.");
    return true;
}

static void cancel_chat_input(Game *game) {
    if (!game->chat_input_active) {
        return;
    }
    game->chat_input_active = false;
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
    refresh_text_input(game);
}

static void submit_chat_input(Game *game) {
    if (!game->chat_input_active) {
        return;
    }
    game->chat_input_active = false;
    refresh_text_input(game);
    if (game->chat_input_len == 0) {
        return;
    }
    const char *sender = game->net.player_name;
    const char *text = game->chat_input;
    if (game->net.mode == NET_CLIENT) {
        network_client_send_chat(game, text);
        set_hud_message(game, "Chat sent.");
    } else {
        add_chat_message(game, sender, text);
        network_host_send_chat(game, sender, text);
    }
    game->chat_input_len = 0;
    game->chat_input[0] = '\0';
}

static bool door_is_passable(const Game *game, int gx, int gy) {
    if (gx < 0 || gy < 0 || gx >= game->map.width || gy >= game->map.height) {
        return false;
    }
    if (game->door_state[gy][gx] == -1) {
        return !tile_is_wall(game->map.tiles[gy][gx]);
    }
    return game->door_state[gy][gx] == 1;
}

static bool toggle_door_state(Game *game, int gx, int gy, bool notify) {
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

static bool interact_with_door(Game *game) {
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
    if (game->net.mode == NET_CLIENT) {
        network_send_request_door(game, targetX, targetY);
        set_hud_message(game, "Door request sent to host.");
        return true;
    }
    if (toggle_door_state(game, targetX, targetY, true)) {
        network_notify_door(game, targetX, targetY, game->door_state[targetY][targetX]);
        return true;
    }
    return false;
}

static void network_notify_door(Game *game, int gx, int gy, int state) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "DOOR %d %d %d\n", gx, gy, state);
}

static void network_send_request_door(Game *game, int gx, int gy) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "REQDOOR %d %d\n", gx, gy);
}

static void load_memories(Game *game) {
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

static void save_memories(const Game *game) {
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

static void network_shutdown(Game *game);

static void apply_received_map(Game *game, int rows, char data[][MAP_WIDTH + 1]) {
    if (rows <= 0) {
        return;
    }
    map_init(&game->map);
    int maxRows = rows < MAP_HEIGHT ? rows : MAP_HEIGHT;
    game->map.height = maxRows;
    for (int y = 0; y < maxRows; ++y) {
        size_t len = strnlen_safe(data[y], MAP_WIDTH);
        for (int x = 0; x < MAP_WIDTH; ++x) {
            char raw = (x < (int)len) ? data[y][x] : '#';
            map_store_char(&game->map, x, y, raw);
        }
        game->map.tiles[y][MAP_WIDTH] = '\0';
    }
    map_enforce_border(&game->map);
    map_apply_wall_styles(&game->map);
    rebuild_furniture(game);
    game_reset_memory(game);
    game_pick_spawn(game);
}

static void network_send_map(Game *game) {
    NetworkState *net = &game->net;
    if (net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "WELCOME %d\n", 2);
    net_sendf(net->peer_fd, "MAP %d\n", game->map.height);
    for (int y = 0; y < game->map.height; ++y) {
        char row[MAP_WIDTH + 1];
        int width = game->map.width < MAP_WIDTH ? game->map.width : MAP_WIDTH;
        for (int x = 0; x < width; ++x) {
            row[x] = map_export_char(&game->map, x, y);
        }
        row[width] = '\0';
        net_sendf(net->peer_fd, "ROW %s\n", row);
    }
    net_sendf(net->peer_fd, "ENDMAP\n");
    if (game->map.spawn_set) {
        net_sendf(net->peer_fd, "SPAWN %d %d\n", game->map.spawn_x, game->map.spawn_y);
    } else {
        net_sendf(net->peer_fd, "SPAWN -1 -1\n");
    }
    net_sendf(net->peer_fd, "MEM %d\n", game->memory_count);
    for (int i = 0; i < game->memory_count; ++i) {
        char encoded[MEMORY_TEXT * 2];
        encode_text(game->memories[i].text, encoded, sizeof(encoded));
        net_sendf(net->peer_fd, "MEMENTRY %d %d %.3f %.3f %d|%s\n", game->memories[i].grid_x, game->memories[i].grid_y,
                  game->memories[i].normal_x, game->memories[i].normal_y, i, encoded);
    }
    net_sendf(net->peer_fd, "ENDMEM\n");
    net_sendf(net->peer_fd, "NAME %d %s\n", 1, net->player_name);
}

static void network_shutdown(Game *game) {
    NetworkState *net = &game->net;
    if (net->peer_fd != -1) {
        close(net->peer_fd);
        net->peer_fd = -1;
    }
    if (net->listen_fd != -1) {
        close(net->listen_fd);
        net->listen_fd = -1;
    }
    net->connected = false;
    net->remote.active = false;
    net->mode = NET_NONE;
}

static void network_init(Game *game) {
    NetworkState *net = &game->net;
    const char *mode_env = getenv("POOM_NET_MODE");
    net->listen_fd = -1;
    net->peer_fd = -1;
    net->peer_buffer_len = 0;
    net->connected = false;
    net->remote.active = false;
    net->self_id = 1;
    net->client_stage = CLIENT_STAGE_READY;
    if (!mode_env || !*mode_env || strcasecmp(mode_env, "none") == 0) {
        net->mode = NET_NONE;
        return;
    }
    net->port = parse_port(getenv("POOM_NET_PORT"), 4455);
    snprintf(net->player_name, sizeof(net->player_name), "%s",
             getenv("POOM_PLAYER_NAME") ? getenv("POOM_PLAYER_NAME") : "Explorer");
    if (strcasecmp(mode_env, "host") == 0) {
        net->mode = NET_HOST;
        net->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (net->listen_fd == -1) {
            perror("socket");
            net->mode = NET_NONE;
            return;
        }
        int opt = 1;
        setsockopt(net->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((uint16_t)net->port);
        if (bind(net->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("bind");
            network_shutdown(game);
            return;
        }
        if (listen(net->listen_fd, 1) == -1) {
            perror("listen");
            network_shutdown(game);
            return;
        }
        set_nonblocking(net->listen_fd);
        net->last_state_send = 0.0;
        printf("[POOM] Hosting multiplayer on port %d\n", net->port);
        set_hud_message(game, "Hosting multiplayer session.");
    } else if (strcasecmp(mode_env, "client") == 0) {
        net->mode = NET_CLIENT;
        const char *host_env = getenv("POOM_NET_HOST");
        if (!host_env || !*host_env) {
            fprintf(stderr, "[POOM] POOM_NET_HOST required for client mode\n");
            net->mode = NET_NONE;
            return;
        }
        snprintf(net->host_addr, sizeof(net->host_addr), "%s", host_env);
        net->peer_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (net->peer_fd == -1) {
            perror("socket");
            net->mode = NET_NONE;
            return;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)net->port);
        if (inet_pton(AF_INET, net->host_addr, &addr.sin_addr) != 1) {
            fprintf(stderr, "[POOM] Invalid host address %s\n", net->host_addr);
            network_shutdown(game);
            return;
        }
        if (connect(net->peer_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("connect");
            network_shutdown(game);
            return;
        }
        set_nonblocking(net->peer_fd);
        net_sendf(net->peer_fd, "HELLO %s\n", net->player_name);
        net->client_stage = CLIENT_STAGE_WAIT_MAP;
        net->last_state_send = 0.0;
        printf("[POOM] Connected to host %s:%d\n", net->host_addr, net->port);
        set_hud_message(game, "Connected to host. Awaiting sync...");
    } else {
        net->mode = NET_NONE;
    }
}

static void network_notify_memory_add(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1 || index < 0 || index >= game->memory_count) {
        return;
    }
    const MemoryEntry *entry = &game->memories[index];
    char encoded[MEMORY_TEXT * 2];
    encode_text(entry->text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "ADD %d %d %d %.3f %.3f|%s\n", index, entry->grid_x, entry->grid_y, entry->normal_x,
              entry->normal_y, encoded);
}

static void network_notify_memory_edit(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1 || index < 0 || index >= game->memory_count) {
        return;
    }
    const MemoryEntry *entry = &game->memories[index];
    char encoded[MEMORY_TEXT * 2];
    encode_text(entry->text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "EDIT %d|%s\n", index, encoded);
}

static void network_notify_memory_delete(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "DEL %d\n", index);
}

static void network_send_request_add(Game *game, int gx, int gy, double nx, double ny, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    char encoded[MEMORY_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "REQADD %d %d %.3f %.3f|%s\n", gx, gy, nx, ny, encoded);
}

static void network_send_request_edit(Game *game, int index, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    char encoded[MEMORY_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "REQEDIT %d|%s\n", index, encoded);
}

static void network_send_request_delete(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "REQDEL %d\n", index);
}

static void network_host_send_chat(Game *game, const char *sender, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    char encoded[CHAT_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "CHAT %s|%s\n", sender ? sender : "Player", encoded);
    add_chat_message(game, sender ? sender : "Player", text);
}

static void network_client_send_chat(Game *game, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    char encoded[CHAT_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "CHATREQ %s\n", encoded);
    add_chat_message(game, net->player_name, text);
}

static void network_handle_host_line(Game *game, const char *line) {
    NetworkState *net = &game->net;
    if (strncmp(line, "HELLO ", 6) == 0) {
        snprintf(net->remote.name, sizeof(net->remote.name), "%s", line + 6);
        net->remote.id = 2;
        net->remote.active = true;
        net->connected = true;
        network_send_map(game);
        net_sendf(net->peer_fd, "NAME %d %s\n", net->remote.id, net->remote.name);
    } else if (strncmp(line, "PLY ", 4) == 0) {
        double x, y, angle;
        int id;
        if (sscanf(line + 4, "%d %lf %lf %lf", &id, &x, &y, &angle) == 4) {
            net->remote.id = id;
            net->remote.x = x;
            net->remote.y = y;
            net->remote.angle = angle;
            net->remote.active = true;
        }
    } else if (strncmp(line, "REQADD ", 7) == 0) {
        int gx, gy;
        double nx, ny;
        const char *payload = strchr(line + 7, '|');
        if (!payload) {
            return;
        }
        if (sscanf(line + 7, "%d %d %lf %lf", &gx, &gy, &nx, &ny) != 4) {
            return;
        }
        char decoded[MEMORY_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        int idx = add_memory_entry_at(game, gx, gy, nx, ny, decoded);
        if (idx >= 0) {
            set_hud_message(game, "Client added memory.");
            network_notify_memory_add(game, idx);
        }
    } else if (strncmp(line, "REQEDIT ", 8) == 0) {
        const char *payload = strchr(line + 8, '|');
        if (!payload) {
            return;
        }
        int idx = atoi(line + 8);
        char decoded[MEMORY_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        edit_memory_entry(game, idx, decoded);
        network_notify_memory_edit(game, idx);
    } else if (strncmp(line, "REQDEL ", 7) == 0) {
        int idx = atoi(line + 7);
        delete_memory_internal(game, idx, true);
        set_hud_message(game, "Client deleted a memory.");
    } else if (strncmp(line, "CHATREQ ", 8) == 0) {
        char decoded[CHAT_TEXT];
        decode_text(line + 8, decoded, sizeof(decoded));
        const char *sender = net->remote.name[0] ? net->remote.name : "Client";
        add_chat_message(game, sender, decoded);
        network_host_send_chat(game, sender, decoded);
    } else if (strncmp(line, "REQDOOR ", 8) == 0) {
        int gx, gy;
        if (sscanf(line + 8, "%d %d", &gx, &gy) == 2) {
            if (toggle_door_state(game, gx, gy, true)) {
                network_notify_door(game, gx, gy, game->door_state[gy][gx]);
            }
        }
    }
}

static void network_handle_client_line(Game *game, const char *line) {
    NetworkState *net = &game->net;
    if (strncmp(line, "WELCOME ", 8) == 0) {
        net->self_id = atoi(line + 8);
    } else if (strncmp(line, "MAP ", 4) == 0) {
        net->expected_map_rows = atoi(line + 4);
        net->received_map_rows = 0;
        net->client_stage = CLIENT_STAGE_MAP_ROWS;
    } else if (strncmp(line, "ROW ", 4) == 0 && net->client_stage == CLIENT_STAGE_MAP_ROWS) {
        if (net->received_map_rows < MAP_HEIGHT) {
            snprintf(net->pending_map[net->received_map_rows], MAP_WIDTH + 1, "%s", line + 4);
            net->received_map_rows++;
        }
    } else if (strcmp(line, "ENDMAP") == 0 && net->client_stage == CLIENT_STAGE_MAP_ROWS) {
        apply_received_map(game, net->received_map_rows, net->pending_map);
        net->client_stage = CLIENT_STAGE_WAIT_MEM;
    } else if (strncmp(line, "SPAWN ", 6) == 0) {
        int sx, sy;
        if (sscanf(line + 6, "%d %d", &sx, &sy) == 2 && sx >= 0 && sy >= 0) {
            game->map.spawn_set = true;
            game->map.spawn_x = sx;
            game->map.spawn_y = sy;
            game_pick_spawn(game);
        } else {
            game->map.spawn_set = false;
        }
    } else if (strncmp(line, "MEM ", 4) == 0) {
        net->expected_mem_entries = atoi(line + 4);
        net->received_mem_entries = 0;
        net->client_stage = CLIENT_STAGE_MEM_ROWS;
        game_reset_memory(game);
    } else if (strncmp(line, "MEMENTRY ", 9) == 0 && net->client_stage == CLIENT_STAGE_MEM_ROWS) {
        const char *payload = strchr(line, '|');
        if (!payload) {
            return;
        }
        int gx, gy, idx;
        double nx, ny;
        if (sscanf(line + 9, "%d %d %lf %lf %d", &gx, &gy, &nx, &ny, &idx) != 5) {
            return;
        }
        char decoded[MEMORY_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        add_memory_entry_at(game, gx, gy, nx, ny, decoded);
        net->received_mem_entries++;
    } else if (strcmp(line, "ENDMEM") == 0) {
        net->client_stage = CLIENT_STAGE_READY;
        net->connected = true;
        set_hud_message(game, "Synchronized with host.");
    } else if (strncmp(line, "ADD ", 4) == 0) {
        int gx, gy, idx;
        double nx, ny;
        const char *payload = strchr(line, '|');
        if (!payload) {
            return;
        }
        if (sscanf(line + 4, "%d %d %d %lf %lf", &idx, &gx, &gy, &nx, &ny) != 5) {
            return;
        }
        char decoded[MEMORY_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        add_memory_entry_at(game, gx, gy, nx, ny, decoded);
    } else if (strncmp(line, "EDIT ", 5) == 0) {
        const char *payload = strchr(line, '|');
        if (!payload) {
            return;
        }
        int idx = atoi(line + 5);
        char decoded[MEMORY_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        edit_memory_entry(game, idx, decoded);
    } else if (strncmp(line, "DEL ", 4) == 0) {
        int idx = atoi(line + 4);
        delete_memory_internal(game, idx, false);
    } else if (strncmp(line, "CHAT ", 5) == 0) {
        const char *payload = strchr(line + 5, '|');
        if (!payload) {
            return;
        }
        char name[32];
        strncpy(name, line + 5, (size_t)(payload - (line + 5)));
        name[payload - (line + 5)] = '\0';
        char decoded[CHAT_TEXT];
        decode_text(payload + 1, decoded, sizeof(decoded));
        if (strcmp(name, game->net.player_name) != 0) {
            add_chat_message(game, name, decoded);
        }
    } else if (strncmp(line, "DOOR ", 5) == 0) {
        int gx, gy, state;
        if (sscanf(line + 5, "%d %d %d", &gx, &gy, &state) == 3) {
            if (gx >= 0 && gy >= 0 && gx < game->map.width && gy < game->map.height) {
                game->door_state[gy][gx] = state ? 1 : 0;
            }
        }
    } else if (strncmp(line, "PLY ", 4) == 0) {
        int id;
        double x, y, angle;
        if (sscanf(line + 4, "%d %lf %lf %lf", &id, &x, &y, &angle) == 4) {
            net->remote.id = id;
            net->remote.x = x;
            net->remote.y = y;
            net->remote.angle = angle;
            net->remote.active = true;
        }
    } else if (strncmp(line, "NAME ", 5) == 0) {
        int id;
        char name[32];
        if (sscanf(line + 5, "%d %31[^\n]", &id, name) == 2) {
            if (id != net->self_id) {
                snprintf(net->remote.name, sizeof(net->remote.name), "%s", name);
                net->remote.active = true;
            }
        }
    }
}

static void network_process_buffer(Game *game, bool host) {
    NetworkState *net = &game->net;
    size_t start = 0;
    for (size_t i = 0; i < net->peer_buffer_len; ++i) {
        if (net->peer_buffer[i] == '\n') {
            net->peer_buffer[i] = '\0';
            const char *line = net->peer_buffer + start;
            if (host) {
                network_handle_host_line(game, line);
            } else {
                network_handle_client_line(game, line);
            }
            start = i + 1;
        }
    }
    if (start > 0) {
        memmove(net->peer_buffer, net->peer_buffer + start, net->peer_buffer_len - start);
        net->peer_buffer_len -= start;
    }
}

static void network_update(Game *game, double delta) {
    NetworkState *net = &game->net;
    if (net->mode == NET_NONE) {
        return;
    }
    if (net->mode == NET_HOST) {
        if (net->listen_fd != -1 && net->peer_fd == -1) {
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
        int fd = accept(net->listen_fd, (struct sockaddr *)&addr, &len);
        if (fd >= 0) {
            set_nonblocking(fd);
            net->peer_fd = fd;
            net->peer_buffer_len = 0;
            net->remote.active = false;
            printf("[POOM] Client connected.\n");
            set_hud_message(game, "Client connected.");
        }
    }
        if (net->peer_fd != -1) {
            char buf[512];
            ssize_t received = recv(net->peer_fd, buf, sizeof(buf), 0);
            if (received > 0) {
                if (net->peer_buffer_len + (size_t)received < sizeof(net->peer_buffer)) {
                    memcpy(net->peer_buffer + net->peer_buffer_len, buf, (size_t)received);
                    net->peer_buffer_len += (size_t)received;
                    network_process_buffer(game, true);
                } else {
                    net->peer_buffer_len = 0;
                }
            } else if (received == 0) {
                printf("[POOM] Client disconnected.\n");
                set_hud_message(game, "Client disconnected.");
                close(net->peer_fd);
                net->peer_fd = -1;
                net->connected = false;
                net->remote.active = false;
            }
            if (net->peer_fd != -1) {
                net->last_state_send += delta;
                if (net->last_state_send > 0.05) {
                    net->last_state_send = 0.0;
                    net_sendf(net->peer_fd, "PLY %d %.3f %.3f %.3f\n", net->self_id, game->player.x, game->player.y,
                              game->player.angle);
                }
            }
        }
    } else if (net->mode == NET_CLIENT) {
        if (net->peer_fd == -1) {
            return;
        }
        char buf[512];
        ssize_t received = recv(net->peer_fd, buf, sizeof(buf), 0);
        if (received > 0) {
            if (net->peer_buffer_len + (size_t)received < sizeof(net->peer_buffer)) {
                memcpy(net->peer_buffer + net->peer_buffer_len, buf, (size_t)received);
                net->peer_buffer_len += (size_t)received;
                network_process_buffer(game, false);
            } else {
                net->peer_buffer_len = 0;
            }
        } else if (received == 0) {
            printf("[POOM] Disconnected from host.\n");
            set_hud_message(game, "Disconnected from host.");
            close(net->peer_fd);
            net->peer_fd = -1;
            net->remote.active = false;
            net->mode = NET_NONE;
            return;
        }
        net->last_state_send += delta;
        if (net->peer_fd != -1 && net->client_stage == CLIENT_STAGE_READY && net->last_state_send > 0.05) {
            net->last_state_send = 0.0;
            net_sendf(net->peer_fd, "PLY %d %.3f %.3f %.3f\n", net->self_id, game->player.x, game->player.y,
                      game->player.angle);
        }
    }
}

static void render_scene(const Game *game, uint32_t *pixels, double *zbuffer) {
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
            } else if (memoryHit >= 0) {
                color = pack_color(210, 170, 60);
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
    render_memory_plaques(game, pixels, dirX, dirY, planeX, planeY, zbuffer);
    render_remote_player(game, pixels, dirX, dirY, planeX, planeY, zbuffer);

    int crossX = SCREEN_WIDTH / 2;
    int crossY = SCREEN_HEIGHT / 2;
    for (int i = -10; i <= 10; ++i) {
        draw_pixel(pixels, crossX + i, crossY, pack_color(255, 255, 255));
        draw_pixel(pixels, crossX, crossY + i, pack_color(255, 255, 255));
    }
    if (furnitureHighlight >= 0 && furnitureHighlight < game->furniture_count) {
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
}

static bool video_init(Video *video) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    video->window =
        SDL_CreateWindow("POOM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!video->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        video_destroy(video);
        return false;
    }
    video->renderer = SDL_CreateRenderer(video->window, -1, SDL_RENDERER_ACCELERATED);
    if (!video->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        video_destroy(video);
        return false;
    }
    video->framebuffer =
        SDL_CreateTexture(video->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                          SCREEN_HEIGHT);
    if (!video->framebuffer) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        video_destroy(video);
        return false;
    }
    return true;
}

static void video_destroy(Video *video) {
    if (video->framebuffer) {
        SDL_DestroyTexture(video->framebuffer);
    }
    if (video->renderer) {
        SDL_DestroyRenderer(video->renderer);
    }
    if (video->window) {
        SDL_DestroyWindow(video->window);
    }
    SDL_Quit();
}

int main(void) {
    srand((unsigned)time(NULL));
    Video video = {0};
    if (!video_init(&video)) {
        return EXIT_FAILURE;
    }

    generate_wall_textures();
    generate_floor_textures();
    generate_ceiling_textures();
    generate_furniture_textures();
    load_custom_textures();

    Game game;
    game_init(&game);
    network_init(&game);

    uint32_t *pixels = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(uint32_t));
    double *zbuffer = malloc(sizeof(double) * SCREEN_WIDTH);
    bool running = true;
    uint64_t lastTicks = SDL_GetTicks64();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_TEXTINPUT) {
                if (game.input.active) {
                    handle_memory_text(&game, event.text.text);
                } else if (game.chat_input_active) {
                    handle_chat_text(&game, event.text.text);
                }
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode sym = event.key.keysym.sym;
                if (game.chat_input_active) {
                    if (sym == SDLK_BACKSPACE) {
                        if (game.chat_input_len > 0) {
                            game.chat_input[--game.chat_input_len] = '\0';
                        }
                        continue;
                    }
                    if (!event.key.repeat) {
                        if (sym == SDLK_RETURN) {
                            submit_chat_input(&game);
                        } else if (sym == SDLK_ESCAPE) {
                            cancel_chat_input(&game);
                        }
                    }
                    continue;
                }
                if (game.input.active) {
                    if (sym == SDLK_BACKSPACE) {
                        memory_backspace(&game);
                        continue;
                    }
                    if (!event.key.repeat) {
                        if (sym == SDLK_RETURN) {
                            if (event.key.keysym.mod & KMOD_SHIFT) {
                                if (game.input.length < MEMORY_TEXT - 1) {
                                    game.input.buffer[game.input.length++] = '\n';
                                    game.input.buffer[game.input.length] = '\0';
                                }
                            } else {
                                finalize_memory_input(&game);
                            }
                        } else if (sym == SDLK_ESCAPE) {
                            cancel_memory_input(&game);
                        }
                    }
                    continue;
                }
                if (game.viewer_active) {
                    if (!event.key.repeat) {
                        if (sym == SDLK_ESCAPE) {
                            close_memory_view(&game);
                        } else if (sym == SDLK_e) {
                            begin_memory_edit(&game, game.viewer_index);
                        } else if (sym == SDLK_DELETE) {
                            game.viewer_delete_prompt = true;
                            set_hud_message(&game, "Delete memory? Y to confirm, N to cancel.");
                        } else if (sym == SDLK_y && game.viewer_delete_prompt) {
                            delete_memory(&game, game.viewer_index);
                            close_memory_view(&game);
                        } else if (sym == SDLK_n && game.viewer_delete_prompt) {
                            game.viewer_delete_prompt = false;
                        }
                    }
                    continue;
                }
                if (!event.key.repeat) {
                    if (sym == SDLK_ESCAPE) {
                        running = false;
                    } else if (sym == SDLK_m) {
                        begin_memory_input(&game);
                    } else if (sym == SDLK_f) {
                        interact_with_door(&game);
                    } else if (sym == SDLK_v) {
                        int idx;
                        if (target_memory(&game, &idx)) {
                            open_memory_view(&game, idx);
                        } else {
                            set_hud_message(&game, "Aim at a memory plaque to view it.");
                        }
                    } else if (sym == SDLK_t) {
                        if (!begin_chat_input(&game)) {
                            set_hud_message(&game, "Finish current action before chatting.");
                        }
                    }
                }
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        uint64_t currentTicks = SDL_GetTicks64();
        double delta = (currentTicks - lastTicks) / 1000.0;
        lastTicks = currentTicks;

        if (!game.input.active && !game.chat_input_active && !game.viewer_active) {
            if (state[SDL_SCANCODE_W]) {
                move_player(&game, cos(game.player.angle) * MOVE_SPEED * delta,
                            sin(game.player.angle) * MOVE_SPEED * delta);
            }
            if (state[SDL_SCANCODE_S]) {
                move_player(&game, -cos(game.player.angle) * MOVE_SPEED * delta,
                            -sin(game.player.angle) * MOVE_SPEED * delta);
            }
            if (state[SDL_SCANCODE_Q]) {
                move_player(&game, cos(game.player.angle - M_PI_2) * STRAFE_SPEED * delta,
                            sin(game.player.angle - M_PI_2) * STRAFE_SPEED * delta);
            }
            if (state[SDL_SCANCODE_E]) {
                move_player(&game, cos(game.player.angle + M_PI_2) * STRAFE_SPEED * delta,
                            sin(game.player.angle + M_PI_2) * STRAFE_SPEED * delta);
            }
            if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT]) {
                game.player.angle -= ROT_SPEED * delta;
                normalize_angle(&game.player.angle);
            }
            if (state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT]) {
                game.player.angle += ROT_SPEED * delta;
                normalize_angle(&game.player.angle);
            }
        }

        if (game.hud_message_timer > 0.0) {
            game.hud_message_timer -= delta;
            if (game.hud_message_timer < 0.0) {
                game.hud_message_timer = 0.0;
                game.hud_message[0] = '\0';
            }
        }
        for (int i = 0; i < game.chat_count;) {
            game.chat_log[i].ttl -= delta;
            if (game.chat_log[i].ttl <= 0.0) {
                memmove(&game.chat_log[i], &game.chat_log[i + 1], sizeof(ChatMessage) * (game.chat_count - i - 1));
                game.chat_count--;
            } else {
                ++i;
            }
        }

        network_update(&game, delta);
        render_scene(&game, pixels, zbuffer);
        SDL_UpdateTexture(video.framebuffer, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(video.renderer);
        SDL_RenderCopy(video.renderer, video.framebuffer, NULL, NULL);
        SDL_RenderPresent(video.renderer);
    }

    free(pixels);
    free(zbuffer);
    if (game.has_save_path) {
        save_memories(&game);
    }
    network_shutdown(&game);
    video_destroy(&video);
    return EXIT_SUCCESS;
}
