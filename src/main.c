// Main entry point and event loop
#include "types.h"
#include "texture.h"
#include "game.h"
#include "map.h"
#include "player.h"
#include "renderer.h"
#include "cabinet.h"
#include "display.h"
#include "terminal.h"
#include "ui.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *framebuffer;
} Video;

static bool video_init(Video *video) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    video->window =
        SDL_CreateWindow("POOM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!video->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    video->renderer = SDL_CreateRenderer(video->window, -1, SDL_RENDERER_ACCELERATED);
    if (!video->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(video->window);
        SDL_Quit();
        return false;
    }
    video->framebuffer =
        SDL_CreateTexture(video->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                          SCREEN_HEIGHT);
    if (!video->framebuffer) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(video->renderer);
        SDL_DestroyWindow(video->window);
        SDL_Quit();
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

static void select_tool(Game *game, HudToolType tool) {
    if (!game || tool < 0 || tool >= NUM_HUD_TOOLS) {
        return;
    }
    if (game->hud_status.tools[tool] <= 0) {
        set_hud_message(game, "Tool unavailable.");
        return;
    }
    if (game->hud_status.active_tool == (int)tool) {
        return;
    }
    game->hud_status.active_tool = tool;
    switch (tool) {
    case HUD_TOOL_KEYBOARD:
        set_hud_message(game, "Keyboard ready for cabinet sessions.");
        break;
    case HUD_TOOL_AXE:
        set_hud_message(game, "Axe selected. Target a cabinet.");
        break;
    case HUD_TOOL_DEPLOY:
        set_hud_message(game, "Cabinet builder equipped.");
        break;
    default:
        break;
    }
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
    generate_cabinet_texture();
    generate_sky_texture();
    generate_display_texture();
    load_custom_textures();

    Game game;
    game_init(&game);

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
                if (game.terminal_mode && game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
                    // Send text input to terminal
                    terminal_write(&game.terminals[game.active_terminal], event.text.text, strlen(event.text.text));
                } else if (game.rename_mode) {
                    // Handle text input in rename mode
                    const char *text = event.text.text;
                    size_t len = strlen(text);
                    for (size_t i = 0; i < len; ++i) {
                        if (game.rename_cursor < 63) {  // Leave room for null terminator
                            game.rename_buffer[game.rename_cursor] = text[i];
                            game.rename_cursor++;
                            game.rename_buffer[game.rename_cursor] = '\0';
                        }
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode sym = event.key.keysym.sym;

                // Rename mode input handling
                if (game.rename_mode) {
                    if (sym == SDLK_RETURN) {
                        // Confirm rename
                        if (game.rename_cabinet_index >= 0 && game.rename_cabinet_index < game.cabinet_count) {
                            CabinetEntry *cabinet = &game.cabinets[game.rename_cabinet_index];
                            set_cabinet_custom_name(cabinet, game.rename_buffer);
                            set_cabinet_custom_color(cabinet, get_cabinet_color_by_index(game.rename_color_index));

                            // Show the new name in confirmation message
                            char msg[128];
                            const char *new_name = get_cabinet_display_name(cabinet);
                            snprintf(msg, sizeof(msg), "Renamed to: %s", new_name);
                            set_hud_message(&game, msg);
                        }
                        game.rename_mode = false;
                        game.rename_cabinet_index = -1;
                    } else if (sym == SDLK_ESCAPE) {
                        // Cancel rename
                        game.rename_mode = false;
                        game.rename_cabinet_index = -1;
                        set_hud_message(&game, "Rename cancelled");
                    } else if (sym == SDLK_BACKSPACE) {
                        // Delete character
                        if (game.rename_cursor > 0) {
                            game.rename_cursor--;
                            game.rename_buffer[game.rename_cursor] = '\0';
                        }
                    } else if (sym == SDLK_LEFT) {
                        // Previous color
                        game.rename_color_index--;
                        if (game.rename_color_index < 0) {
                            game.rename_color_index = NUM_CABINET_COLORS - 1;
                        }
                    } else if (sym == SDLK_RIGHT) {
                        // Next color
                        game.rename_color_index++;
                        if (game.rename_color_index >= NUM_CABINET_COLORS) {
                            game.rename_color_index = 0;
                        }
                    }
                    continue;
                }

                // Terminal mode input handling
                if (game.terminal_mode) {
                    // F1 exits terminal mode (not ESC, so vim works)
                    if (sym == SDLK_F1) {
                        game.terminal_mode = false;
                        game.active_terminal = -1;
                        game.skip_display_frames = 3;  // Skip rendering for 3 frames
                        continue;
                    } else if (game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
                        Terminal *term = &game.terminals[game.active_terminal];
                        char buf[8];
                        size_t len = 0;

                        SDL_Keymod mod = SDL_GetModState();
                        bool ctrl = (mod & KMOD_CTRL) != 0;

                        // Ctrl+key combinations
                        if (ctrl) {
                            if (sym >= SDLK_a && sym <= SDLK_z) {
                                // Ctrl+A through Ctrl+Z
                                buf[len++] = (char)(sym - SDLK_a + 1);
                            } else if (sym == SDLK_LEFTBRACKET) {
                                // Ctrl+[ = ESC
                                buf[len++] = '\033';
                            } else if (sym == SDLK_BACKSLASH) {
                                // Ctrl+\ = FS
                                buf[len++] = '\034';
                            } else if (sym == SDLK_RIGHTBRACKET) {
                                // Ctrl+] = GS
                                buf[len++] = '\035';
                            }
                        } else {
                            // Regular keys
                            if (sym == SDLK_RETURN) {
                                buf[len++] = '\n';
                            } else if (sym == SDLK_BACKSPACE) {
                                buf[len++] = '\b';
                            } else if (sym == SDLK_ESCAPE) {
                                buf[len++] = '\033';  // ESC now goes to terminal, not exits
                            } else if (sym == SDLK_UP) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'A';
                            } else if (sym == SDLK_DOWN) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'B';
                            } else if (sym == SDLK_RIGHT) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'C';
                            } else if (sym == SDLK_LEFT) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'D';
                            } else if (sym == SDLK_TAB) {
                                buf[len++] = '\t';
                            } else if (sym == SDLK_DELETE) {
                                // Delete key sequence
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = '3';
                                buf[len++] = '~';
                            } else if (sym == SDLK_HOME) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'H';
                            } else if (sym == SDLK_END) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = 'F';
                            } else if (sym == SDLK_PAGEUP) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = '5';
                                buf[len++] = '~';
                            } else if (sym == SDLK_PAGEDOWN) {
                                buf[len++] = '\033';
                                buf[len++] = '[';
                                buf[len++] = '6';
                                buf[len++] = '~';
                            }
                        }

                        if (len > 0) {
                            terminal_write(term, buf, len);
                        }
                    }
                    continue;
                }

                if (!event.key.repeat) {
                    if (sym == SDLK_ESCAPE) {
                        running = false;
                    } else if (sym == SDLK_1 || sym == SDLK_KP_1) {
                        select_tool(&game, HUD_TOOL_KEYBOARD);
                    } else if (sym == SDLK_2 || sym == SDLK_KP_2) {
                        select_tool(&game, HUD_TOOL_AXE);
                    } else if (sym == SDLK_3 || sym == SDLK_KP_3) {
                        select_tool(&game, HUD_TOOL_DEPLOY);
                    } else if (sym == SDLK_e) {
                        // E key: Activate display
                        double rayX = game.player.x + cos(game.player.angle) * 1.5;
                        double rayY = game.player.y + sin(game.player.angle) * 1.5;
                        int gx = (int)rayX;
                        int gy = (int)rayY;

                        int disp_idx = find_display_at(&game, gx, gy);
                        if (disp_idx >= 0) {
                            activate_display(&game, disp_idx);
                        }
                    } else if (sym == SDLK_u) {
                        double rayX = game.player.x + cos(game.player.angle) * 1.5;
                        double rayY = game.player.y + sin(game.player.angle) * 1.5;
                        int gx = (int)rayX;
                        int gy = (int)rayY;
                        HudToolType activeTool = (HudToolType)game.hud_status.active_tool;
                        if (activeTool == HUD_TOOL_KEYBOARD) {
#if DEBUG_MODE
                            printf("[DEBUG] U key pressed in keyboard mode.\n");
                            printf("[DEBUG]   Player position: (%.2f, %.2f)\n", game.player.x, game.player.y);
                            printf("[DEBUG]   Raycast position: (%.2f, %.2f)\n", rayX, rayY);
                            printf("[DEBUG]   Grid position: (%d, %d)\n", gx, gy);
#endif
                            int cab_idx = find_cabinet_at(&game, gx, gy);
                            if (cab_idx >= 0) {
                                activate_cabinet(&game, cab_idx);
                            } else {
                                set_hud_message(&game, "No cabinet nearby. Face a cabinet and press U.");
                            }
                        } else if (activeTool == HUD_TOOL_AXE) {
                            int cab_idx = find_cabinet_at(&game, gx, gy);
                            if (cab_idx >= 0 && remove_cabinet(&game, cab_idx)) {
                                set_hud_message(&game, "Cabinet dismantled.");
                            } else {
                                set_hud_message(&game, "Nothing to dismantle.");
                            }
                        } else if (activeTool == HUD_TOOL_DEPLOY) {
                            if (place_cabinet(&game, gx, gy)) {
                                set_hud_message(&game, "Cabinet deployed.");
                            } else {
                                set_hud_message(&game, "Cannot deploy cabinet here.");
                            }
                        } else {
                            set_hud_message(&game, "Select a tool before using U.");
                        }
                    } else if (sym == SDLK_f) {
                        // Toggle door
                        interact_with_door(&game);
                    } else if (sym == SDLK_r) {
                        // R key: Rename cabinet when keyboard tool is equipped
                        if (game.hud_status.active_tool == HUD_TOOL_KEYBOARD) {
                            double rayX = game.player.x + cos(game.player.angle) * 1.5;
                            double rayY = game.player.y + sin(game.player.angle) * 1.5;
                            int gx = (int)rayX;
                            int gy = (int)rayY;
                            int cab_idx = find_cabinet_at(&game, gx, gy);
                            if (cab_idx >= 0) {
                                // Enter rename mode
                                game.rename_mode = true;
                                game.rename_cabinet_index = cab_idx;

                                // Initialize with current name or empty
                                const char *current_name = get_cabinet_display_name(&game.cabinets[cab_idx]);
                                if (game.cabinets[cab_idx].custom_name) {
                                    strncpy(game.rename_buffer, current_name, 63);
                                    game.rename_buffer[63] = '\0';
                                    game.rename_cursor = (int)strlen(game.rename_buffer);
                                } else {
                                    game.rename_buffer[0] = '\0';
                                    game.rename_cursor = 0;
                                }

                                // Initialize color index
                                if (game.cabinets[cab_idx].has_custom_color) {
                                    // Find the current color index
                                    game.rename_color_index = 0;
                                    for (int i = 0; i < NUM_CABINET_COLORS; ++i) {
                                        if (get_cabinet_color_by_index(i) == game.cabinets[cab_idx].custom_color) {
                                            game.rename_color_index = i;
                                            break;
                                        }
                                    }
                                } else {
                                    game.rename_color_index = 0;  // Default to red
                                }
                            } else {
                                set_hud_message(&game, "No cabinet to rename. Face a cabinet and press R.");
                            }
                        }
                    }
                }
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        uint64_t currentTicks = SDL_GetTicks64();
        double delta = (currentTicks - lastTicks) / 1000.0;
        lastTicks = currentTicks;

        bool moving_input = false;
        if (!game.terminal_mode && !game.rename_mode) {
            if (state[SDL_SCANCODE_W]) {
                move_player(&game, cos(game.player.angle) * MOVE_SPEED * delta,
                            sin(game.player.angle) * MOVE_SPEED * delta);
                moving_input = true;
            }
            if (state[SDL_SCANCODE_S]) {
                move_player(&game, -cos(game.player.angle) * MOVE_SPEED * delta,
                            -sin(game.player.angle) * MOVE_SPEED * delta);
                moving_input = true;
            }
            if (state[SDL_SCANCODE_Q]) {
                move_player(&game, cos(game.player.angle - M_PI_2) * STRAFE_SPEED * delta,
                            sin(game.player.angle - M_PI_2) * STRAFE_SPEED * delta);
                moving_input = true;
            }
            if (state[SDL_SCANCODE_E]) {
                move_player(&game, cos(game.player.angle + M_PI_2) * STRAFE_SPEED * delta,
                            sin(game.player.angle + M_PI_2) * STRAFE_SPEED * delta);
                moving_input = true;
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
        game_update_hud_bob(&game, moving_input, delta);

        if (game.hud_message_timer > 0.0) {
            game.hud_message_timer -= delta;
            if (game.hud_message_timer < 0.0) {
                game.hud_message_timer = 0.0;
                game.hud_message[0] = '\0';
            }
        }

        // Show interaction hints when no HUD message is active
        if (!game.terminal_mode && !game.rename_mode && game.hud_message_timer <= 0.0) {
            double rayX = game.player.x + cos(game.player.angle) * 1.5;
            double rayY = game.player.y + sin(game.player.angle) * 1.5;
            int gx = (int)rayX;
            int gy = (int)rayY;

            // Check what the player is facing
            int cab_idx = find_cabinet_at(&game, gx, gy);
            if (cab_idx >= 0) {
                const char *display_name = get_cabinet_display_name(&game.cabinets[cab_idx]);
                if (game.hud_status.active_tool == HUD_TOOL_KEYBOARD) {
                    snprintf(game.hud_message, sizeof(game.hud_message),
                             "%s - Press U to activate, R to rename", display_name);
                } else {
                    snprintf(game.hud_message, sizeof(game.hud_message),
                             "%s - Press U to activate", display_name);
                }
            } else if (find_display_at(&game, gx, gy) >= 0) {
                snprintf(game.hud_message, sizeof(game.hud_message), "Press E to use display");
            } else if (game.door_state && gx >= 0 && gx < game.map.width &&
                       gy >= 0 && gy < game.map.height &&
                       game.map.tiles[gy][gx] == 'D') {
                bool is_open = game.door_state[gy][gx] == 1;
                snprintf(game.hud_message, sizeof(game.hud_message),
                         "Press F to %s door", is_open ? "close" : "open");
            }
        }

        // Update only the active terminal when in terminal mode
        if (game.terminal_mode && game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
            terminal_update(&game.terminals[game.active_terminal]);
        }

        // Decrement skip counter
        if (game.skip_display_frames > 0) {
            game.skip_display_frames--;
        }

        game_update_hud_status(&game);

        // Render terminal or normal scene
        if (game.terminal_mode && game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
            render_terminal(&game.terminals[game.active_terminal], pixels);
        } else {
            render_scene(&game, pixels, zbuffer);
            // Render rename dialog on top if active
            if (game.rename_mode) {
                render_rename_dialog(pixels, &game);
            }
        }

        SDL_UpdateTexture(video.framebuffer, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(video.renderer);
        SDL_RenderCopy(video.renderer, video.framebuffer, NULL, NULL);
        SDL_RenderPresent(video.renderer);
    }

    free(pixels);
    free(zbuffer);
    game_cleanup_terminals(&game);
    game_free_game_maps(&game);
    map_free(&game.map);
    video_destroy(&video);
    return EXIT_SUCCESS;
}
