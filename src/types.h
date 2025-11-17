#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

// Debug mode - set to 1 to enable verbose logging
#define DEBUG_MODE 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#define MAP_WIDTH 48
#define MAP_HEIGHT 48
#define SCREEN_WIDTH 1366
#define SCREEN_HEIGHT 768
#define MAX_DEPTH 32.0
#define MOVE_SPEED 3.7
#define STRAFE_SPEED 3.0
#define ROT_SPEED 2.4
#define FOV (M_PI / 3.0)

#define TEX_SIZE 64
#define NUM_WALL_TEXTURES 4
#define NUM_FLOOR_TEXTURES 3
#define NUM_CEIL_TEXTURES 2
#define NUM_CABINET_TEXTURES 4
#define MAX_LAYOUT_LINES 32

#define MAP_FILE_DEFAULT "maps/palace.map"

#define MAX_CABINETS 16
#define MAX_TERMINALS 16
#define MAX_DISPLAYS 32

#define SKY_TEXTURE_WIDTH 512
#define SKY_TEXTURE_HEIGHT 128

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

// Terminal emulation structures
#define TERM_COLS 80
#define TERM_ROWS 24

typedef struct {
    char ch;
    uint8_t fg_color;  // 0-15 (ANSI colors)
    uint8_t bg_color;  // 0-15
    uint8_t attrs;     // bold, underline, etc.
} TermCell;

typedef enum {
    PARSE_NORMAL,
    PARSE_ESC,
    PARSE_CSI,
    PARSE_CSI_PARAM
} ParseState;

typedef struct {
    TermCell cells[TERM_ROWS][TERM_COLS];
    int cursor_x;
    int cursor_y;
    bool cursor_visible;
    int saved_cursor_x;  // For CSI s/u (save/restore cursor)
    int saved_cursor_y;
    int pty_fd;          // PTY file descriptor
    pid_t shell_pid;     // Shell process ID
    bool active;
    char read_buffer[4096];
    ParseState parse_state;     // For ANSI parser state machine
    int ansi_params[16]; // Parameters from escape sequences
    int ansi_param_count;
    uint8_t current_fg;  // Current foreground color
    uint8_t current_bg;  // Current background color
    uint8_t current_attrs; // Current attributes
    char csi_buffer[64]; // Buffer for CSI sequence
    int csi_buffer_len;
} Terminal;

// Cabinet types
typedef enum {
    CABINET_SERVER = 0,
} CabinetType;

typedef struct {
    CabinetType type;
    int grid_x;
    int grid_y;
    double x;
    double y;
    const char *name;
    int terminal_index;  // Index into game terminals array
    int texture_index;   // Which cabinet texture variation to use (0-3)
} CabinetEntry;

// Display types (wall-mounted monitors)
typedef struct {
    int grid_x;      // Starting grid position (top-left)
    int grid_y;
    double x;        // World position (center)
    double y;
    double normal_x;  // Wall normal direction
    double normal_y;
    const char *name;
    int terminal_index;  // Index into game terminals array
    int width;       // Number of tiles wide (for stacked displays)
    int height;      // Number of tiles tall (for stacked displays)
} DisplayEntry;

typedef struct {
    Map map;
    Player player;
    int **door_state;  // Dynamic allocation: [height][width]
    char hud_message[128];
    double hud_message_timer;
    Terminal terminals[MAX_TERMINALS];
    CabinetEntry cabinets[MAX_CABINETS];
    int cabinet_count;
    DisplayEntry displays[MAX_DISPLAYS];
    int display_count;
    bool terminal_mode;
    int active_terminal;  // Which terminal is currently being viewed
    int skip_display_frames;  // Skip display rendering for N frames after exit
} Game;

#endif // TYPES_H
