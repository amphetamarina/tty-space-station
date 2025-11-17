// POOM Map Editor - GUI tool for creating and editing maps
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CELL_SIZE 24
#define PALETTE_WIDTH 200
#define TOOLBAR_HEIGHT 80
#define MIN_MAP_WIDTH 10
#define MIN_MAP_HEIGHT 10
#define MAX_MAP_WIDTH 100
#define MAX_MAP_HEIGHT 100
#define DEFAULT_MAP_WIDTH 48
#define DEFAULT_MAP_HEIGHT 48

typedef struct {
    char **tiles;
    int width;
    int height;
    char filename[512];
} EditorMap;

typedef struct {
    char tile;
    const char *name;
    SDL_Color color;
    const char *description;
} TilePalette;

static TilePalette palette[] = {
    {'.', "Floor 1", {70, 60, 50, 255}, "Basic floor"},
    {',', "Floor 2", {100, 100, 100, 255}, "Noise floor"},
    {';', "Floor 3", {150, 150, 180, 255}, "Marble floor"},
    {'1', "Wall 1", {90, 90, 120, 255}, "Checkered wall"},
    {'2', "Wall 2", {80, 120, 80, 255}, "Striped wall"},
    {'3', "Wall 3", {120, 60, 50, 255}, "Brick wall"},
    {'4', "Window", {140, 180, 220, 255}, "Window wall"},
    {'D', "Door", {130, 90, 40, 255}, "Door (toggleable)"},
    {'X', "Spawn", {255, 255, 0, 255}, "Player spawn point"},
    {'T', "Table", {120, 90, 50, 255}, "Square table"},
    {'R', "Round Table", {90, 70, 40, 255}, "Round table"},
    {'B', "Bed", {70, 80, 120, 255}, "Bed"},
    {'S', "Sofa", {80, 30, 30, 255}, "Sofa"},
    {'W', "Wardrobe", {80, 60, 30, 255}, "Wardrobe"},
    {'P', "Puppy", {139, 90, 43, 255}, "NPC: Puppy"},
    {'G', "Ghost", {200, 220, 255, 255}, "NPC: Ghost"},
    {'#', "Wall", {40, 40, 40, 255}, "Solid wall"},
};
#define PALETTE_SIZE (sizeof(palette) / sizeof(palette[0]))

static char selected_tile = '.';
static bool show_grid = true;
static bool show_help = true;
static int scroll_x = 0;
static int scroll_y = 0;

static void allocate_map(EditorMap *map, int width, int height) {
    map->width = width;
    map->height = height;
    map->tiles = malloc(sizeof(char *) * height);
    for (int y = 0; y < height; y++) {
        map->tiles[y] = malloc(width + 1);
        memset(map->tiles[y], '.', width);
        map->tiles[y][width] = '\0';
    }
}

static void free_map(EditorMap *map) {
    if (map->tiles) {
        for (int y = 0; y < map->height; y++) {
            free(map->tiles[y]);
        }
        free(map->tiles);
        map->tiles = NULL;
    }
}

static void init_map(EditorMap *map, int width, int height) {
    allocate_map(map, width, height);

    // Create border walls
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (y == 0 || y == height - 1 || x == 0 || x == width - 1) {
                map->tiles[y][x] = '1';
            } else {
                map->tiles[y][x] = '.';
            }
        }
    }

    // Place spawn point
    if (width > 2 && height > 2) {
        map->tiles[height / 2][width / 2] = 'X';
    }
}

static bool load_map(EditorMap *map, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        return false;
    }

    // First pass: count dimensions
    char line[512];
    int height = 0;
    int max_width = 0;

    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }
        if (len > max_width) {
            max_width = len;
        }
        height++;
    }

    if (height < MIN_MAP_HEIGHT || max_width < MIN_MAP_WIDTH) {
        fclose(f);
        return false;
    }

    // Allocate map
    allocate_map(map, max_width, height);

    // Second pass: load data
    rewind(f);
    int y = 0;
    while (fgets(line, sizeof(line), f) && y < height) {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        for (int x = 0; x < map->width; x++) {
            if (x < len) {
                map->tiles[y][x] = line[x];
            } else {
                map->tiles[y][x] = '.';
            }
        }
        y++;
    }

    fclose(f);
    strncpy(map->filename, filename, sizeof(map->filename) - 1);
    return true;
}

static bool save_map(const EditorMap *map, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        return false;
    }

    for (int y = 0; y < map->height; y++) {
        fprintf(f, "%s\n", map->tiles[y]);
    }

    fclose(f);
    printf("Map saved to: %s (%dx%d)\n", filename, map->width, map->height);
    return true;
}

static SDL_Color get_tile_color(char tile) {
    for (size_t i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].tile == tile) {
            return palette[i].color;
        }
    }
    return (SDL_Color){128, 128, 128, 255};
}

static const char* get_tile_name(char tile) {
    for (size_t i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].tile == tile) {
            return palette[i].name;
        }
    }
    return "Unknown";
}

static void render_text(SDL_Renderer *renderer, int x, int y, const char *text, SDL_Color color) {
    // Simple text rendering (just placeholder - would need SDL_ttf for proper text)
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    // For now, just render a rectangle as placeholder
    SDL_Rect rect = {x, y, (int)strlen(text) * 8, 12};
    SDL_RenderDrawRect(renderer, &rect);
}

static void render_editor(SDL_Renderer *renderer, const EditorMap *map, int window_width, int window_height) {
    // Clear background
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderClear(renderer);

    int map_area_width = window_width - PALETTE_WIDTH;
    int map_area_height = window_height - TOOLBAR_HEIGHT;

    // Render map
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int screen_x = x * CELL_SIZE - scroll_x;
            int screen_y = y * CELL_SIZE - scroll_y + TOOLBAR_HEIGHT;

            if (screen_x + CELL_SIZE < 0 || screen_x > map_area_width ||
                screen_y + CELL_SIZE < TOOLBAR_HEIGHT || screen_y > window_height) {
                continue;
            }

            SDL_Color color = get_tile_color(map->tiles[y][x]);
            SDL_Rect cell = {screen_x, screen_y, CELL_SIZE, CELL_SIZE};

            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &cell);

            if (show_grid) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
                SDL_RenderDrawRect(renderer, &cell);
            }
        }
    }

    // Render toolbar
    SDL_Rect toolbar = {0, 0, window_width, TOOLBAR_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 45, 45, 55, 255);
    SDL_RenderFillRect(renderer, &toolbar);
    SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
    SDL_RenderDrawRect(renderer, &toolbar);

    // Show current tile
    SDL_Rect current_tile_box = {10, 10, 60, 60};
    SDL_Color sel_color = get_tile_color(selected_tile);
    SDL_SetRenderDrawColor(renderer, sel_color.r, sel_color.g, sel_color.b, sel_color.a);
    SDL_RenderFillRect(renderer, &current_tile_box);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &current_tile_box);

    // Render palette
    SDL_Rect palette_area = {map_area_width, TOOLBAR_HEIGHT, PALETTE_WIDTH, map_area_height};
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderFillRect(renderer, &palette_area);

    int palette_y = TOOLBAR_HEIGHT + 10;
    for (size_t i = 0; i < PALETTE_SIZE; i++) {
        SDL_Rect tile_box = {map_area_width + 10, palette_y, 30, 30};
        SDL_SetRenderDrawColor(renderer, palette[i].color.r, palette[i].color.g,
                                palette[i].color.b, palette[i].color.a);
        SDL_RenderFillRect(renderer, &tile_box);

        if (palette[i].tile == selected_tile) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(renderer, &tile_box);
            SDL_Rect outer = {tile_box.x - 2, tile_box.y - 2, tile_box.w + 4, tile_box.h + 4};
            SDL_RenderDrawRect(renderer, &outer);
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &tile_box);
        }

        palette_y += 35;
    }

    // Render help text if enabled
    if (show_help) {
        SDL_Rect help_box = {80, 5, 600, 70};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 230);
        SDL_RenderFillRect(renderer, &help_box);
        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_RenderDrawRect(renderer, &help_box);
    }
}

int main(int argc, char *argv[]) {
    EditorMap map = {0};
    int map_width = DEFAULT_MAP_WIDTH;
    int map_height = DEFAULT_MAP_HEIGHT;

    // Parse command line arguments
    if (argc > 1) {
        if (!load_map(&map, argv[1])) {
            printf("Creating new map: %s\n", argv[1]);
            init_map(&map, map_width, map_height);
            strncpy(map.filename, argv[1], sizeof(map.filename) - 1);
        } else {
            printf("Loaded map: %s (%dx%d)\n", argv[1], map.width, map.height);
        }
    } else {
        printf("Usage: %s <mapfile.map> [width] [height]\n", argv[0]);
        printf("Creating default map\n");
        init_map(&map, map_width, map_height);
        strcpy(map.filename, "maps/newmap.map");
    }

    if (argc > 3) {
        int w = atoi(argv[2]);
        int h = atoi(argv[3]);
        if (w >= MIN_MAP_WIDTH && w <= MAX_MAP_WIDTH && h >= MIN_MAP_HEIGHT && h <= MAX_MAP_HEIGHT) {
            free_map(&map);
            init_map(&map, w, h);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free_map(&map);
        return 1;
    }

    int window_width = 1400;
    int window_height = 800;

    SDL_Window *window = SDL_CreateWindow("POOM Map Editor",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          window_width, window_height,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free_map(&map);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        free_map(&map);
        return 1;
    }

    bool running = true;
    bool mouse_down = false;
    bool panning = false;
    int last_mouse_x = 0;
    int last_mouse_y = 0;

    printf("\n=== POOM Map Editor ===\n");
    printf("Controls:\n");
    printf("  Left Click: Place selected tile\n");
    printf("  Right Click: Pan view\n");
    printf("  Mouse Wheel: Scroll\n");
    printf("  G: Toggle grid\n");
    printf("  H: Toggle help\n");
    printf("  Ctrl+S: Save map\n");
    printf("  Ctrl+N: New map\n");
    printf("  ESC: Quit\n");
    printf("  Click palette on right to select tiles\n\n");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN: {
                    SDL_Keymod mod = SDL_GetModState();
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.keysym.sym == SDLK_g) {
                        show_grid = !show_grid;
                    } else if (event.key.keysym.sym == SDLK_h) {
                        show_help = !show_help;
                    } else if (event.key.keysym.sym == SDLK_s && (mod & KMOD_CTRL)) {
                        save_map(&map, map.filename);
                    }
                    break;
                }

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = true;
                        // Check if clicking palette
                        int map_area_width = window_width - PALETTE_WIDTH;
                        if (event.button.x >= map_area_width) {
                            int palette_idx = (event.button.y - TOOLBAR_HEIGHT - 10) / 35;
                            if (palette_idx >= 0 && palette_idx < (int)PALETTE_SIZE) {
                                selected_tile = palette[palette_idx].tile;
                                printf("Selected: %c (%s)\n", selected_tile, get_tile_name(selected_tile));
                            }
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        panning = true;
                        last_mouse_x = event.button.x;
                        last_mouse_y = event.button.y;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = false;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        panning = false;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    if (mouse_down) {
                        int map_area_width = window_width - PALETTE_WIDTH;
                        if (event.motion.x < map_area_width && event.motion.y >= TOOLBAR_HEIGHT) {
                            int grid_x = (event.motion.x + scroll_x) / CELL_SIZE;
                            int grid_y = (event.motion.y - TOOLBAR_HEIGHT + scroll_y) / CELL_SIZE;
                            if (grid_x >= 0 && grid_x < map.width && grid_y >= 0 && grid_y < map.height) {
                                map.tiles[grid_y][grid_x] = selected_tile;
                            }
                        }
                    } else if (panning) {
                        int dx = event.motion.x - last_mouse_x;
                        int dy = event.motion.y - last_mouse_y;
                        scroll_x -= dx;
                        scroll_y -= dy;
                        if (scroll_x < 0) scroll_x = 0;
                        if (scroll_y < 0) scroll_y = 0;
                        last_mouse_x = event.motion.x;
                        last_mouse_y = event.motion.y;
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    scroll_y -= event.wheel.y * 20;
                    if (scroll_y < 0) scroll_y = 0;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        window_width = event.window.data1;
                        window_height = event.window.data2;
                    }
                    break;
            }
        }

        render_editor(renderer, &map, window_width, window_height);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    // Save before exit
    printf("Save changes? (y/n): ");
    char choice = getchar();
    if (choice == 'y' || choice == 'Y') {
        save_map(&map, map.filename);
    }

    free_map(&map);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
