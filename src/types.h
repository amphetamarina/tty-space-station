#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

#define MAP_FILE_DEFAULT "maps/palace.map"

#define MAX_FURNITURE 64
#define MAX_NPCS 32

#define clamp_int(value, min_value, max_value) \
    ((value) < (min_value) ? (min_value) : ((value) > (max_value) ? (max_value) : (value)))
#define PACK_COLOR_LITERAL(r, g, b) \
    (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

typedef struct {
    double x;
    double y;
    double angle;
    double fov;
} Player;

typedef struct {
    int width;
    int height;
    char **tiles;  // Dynamic allocation: [height][width+1]
    char **decor;  // Dynamic allocation: [height][width]
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

#define NUM_FURNITURE_TYPES (FURN_WARDROBE + 1)

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

// NPC types
typedef enum {
    NPC_NONE = 0,
    NPC_PUPPY,
    NPC_GHOST
} NPCType;

typedef enum {
    NPC_STATE_IDLE = 0,
    NPC_STATE_WANDER,
    NPC_STATE_TALK
} NPCState;

typedef struct {
    NPCType type;
    int grid_x;
    int grid_y;
    double x;
    double y;
    double angle;
    NPCState state;
    double state_timer;
    double target_x;
    double target_y;
    const char *name;
    const char *dialogue;
    bool active;
} NPCEntry;

typedef struct {
    const char *name;
    const char *dialogue;
    uint32_t color;
    double move_speed;
    double sprite_height;
} NPCSpec;

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
    int pending_map_width;
    char **pending_map;  // Dynamic allocation: [height][width+1]
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
    int **memory_map;  // Dynamic allocation: [height][width]
    int **door_state;  // Dynamic allocation: [height][width]
    int memory_count;
    FurnitureEntry furniture[MAX_FURNITURE];
    int furniture_count;
    NPCEntry npcs[MAX_NPCS];
    int npc_count;
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
    int dialogue_npc_index;
    bool dialogue_active;
} Game;

#endif // TYPES_H
