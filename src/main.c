// Main entry point and event loop
#include "types.h"
#include "texture.h"
#include "game.h"
#include "map.h"
#include "memory.h"
#include "player.h"
#include "renderer.h"
#include "npc.h"
#include "cabinet.h"
#include "display.h"
#include "terminal.h"
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
                } else if (game.input.active) {
                    handle_memory_text(&game, event.text.text);
                }
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode sym = event.key.keysym.sym;

                // Terminal mode input handling
                if (game.terminal_mode && !event.key.repeat) {
                    if (sym == SDLK_ESCAPE) {
                        game.terminal_mode = false;
                        game.active_terminal = -1;
                        continue;
                    } else if (game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
                        Terminal *term = &game.terminals[game.active_terminal];
                        char buf[8];
                        size_t len = 0;

                        if (sym == SDLK_RETURN) {
                            buf[len++] = '\n';
                        } else if (sym == SDLK_BACKSPACE) {
                            buf[len++] = '\b';
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
                        }

                        if (len > 0) {
                            terminal_write(term, buf, len);
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
                if (game.dialogue_active) {
                    if (!event.key.repeat) {
                        if (sym == SDLK_ESCAPE || sym == SDLK_e || sym == SDLK_n) {
                            close_npc_dialogue(&game);
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
                    } else if (sym == SDLK_e) {
                        // E key: Check for display first, then NPC
                        double rayX = game.player.x + cos(game.player.angle) * 1.5;
                        double rayY = game.player.y + sin(game.player.angle) * 1.5;
                        int gx = (int)rayX;
                        int gy = (int)rayY;

                        int disp_idx = find_display_at(&game, gx, gy);
                        if (disp_idx >= 0) {
                            activate_display(&game, disp_idx);
                        } else {
                            // No display found, try NPC interaction
                            interact_with_npc(&game);
                        }
                    } else if (sym == SDLK_n) {
                        interact_with_npc(&game);
                    } else if (sym == SDLK_u) {
                        // Activate cabinet - find cabinet player is facing
                        double rayX = game.player.x + cos(game.player.angle) * 1.5;
                        double rayY = game.player.y + sin(game.player.angle) * 1.5;
                        int gx = (int)rayX;
                        int gy = (int)rayY;
#if DEBUG_MODE
                        printf("[DEBUG] U key pressed:\n");
                        printf("[DEBUG]   Player position: (%.2f, %.2f)\n", game.player.x, game.player.y);
                        printf("[DEBUG]   Player angle: %.2f\n", game.player.angle);
                        printf("[DEBUG]   Raycast position: (%.2f, %.2f)\n", rayX, rayY);
                        printf("[DEBUG]   Grid position: (%d, %d)\n", gx, gy);
#endif
                        int cab_idx = find_cabinet_at(&game, gx, gy);
                        if (cab_idx >= 0) {
#if DEBUG_MODE
                            printf("[DEBUG] Activating cabinet #%d\n", cab_idx);
#endif
                            activate_cabinet(&game, cab_idx);
                        } else {
#if DEBUG_MODE
                            printf("[DEBUG] No cabinet found, showing error message\n");
#endif
                            set_hud_message(&game, "No cabinet nearby. Face a cabinet and press U.");
                        }
                    }
                }
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        uint64_t currentTicks = SDL_GetTicks64();
        double delta = (currentTicks - lastTicks) / 1000.0;
        lastTicks = currentTicks;

        if (!game.input.active && !game.viewer_active && !game.dialogue_active && !game.terminal_mode) {
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

        // Update terminals if in terminal mode
        if (game.terminal_mode && game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
            terminal_update(&game.terminals[game.active_terminal]);
        }

        npc_update_ai(&game, delta);

        // Render terminal or normal scene
        if (game.terminal_mode && game.active_terminal >= 0 && game.active_terminal < MAX_TERMINALS) {
            render_terminal(&game.terminals[game.active_terminal], pixels);
        } else {
            render_scene(&game, pixels, zbuffer);
        }

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
    game_cleanup_terminals(&game);
    game_free_game_maps(&game);
    map_free(&game.map);
    video_destroy(&video);
    return EXIT_SUCCESS;
}
