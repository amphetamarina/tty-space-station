// Network module
#include "network.h"
#include "map.h"
#include "memory.h"
#include "player.h"
#include "game.h"
#include "furniture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdarg.h>

static size_t strnlen_safe(const char *s, size_t maxlen);

void encode_text(const char *src, char *dst, size_t dst_size) {
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

void decode_text(const char *src, char *dst, size_t dst_size) {
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

int parse_port(const char *value, int fallback) {
    if (!value || !*value) {
        return fallback;
    }
    int p = atoi(value);
    if (p <= 0 || p > 65535) {
        return fallback;
    }
    return p;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

ssize_t net_send_all(int fd, const char *data, size_t len) {
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

void net_sendf(int fd, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    net_send_all(fd, buffer, strlen(buffer));
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

static int allocate_pending_map(NetworkState *net, int width, int height) {
    if (width < 10 || height < 10 || width > 200 || height > 200) {
        return 0;
    }

    net->pending_map = (char **)malloc(height * sizeof(char *));
    if (!net->pending_map) {
        return 0;
    }

    for (int y = 0; y < height; ++y) {
        net->pending_map[y] = (char *)malloc((width + 1) * sizeof(char));
        if (!net->pending_map[y]) {
            for (int i = 0; i < y; ++i) {
                free(net->pending_map[i]);
            }
            free(net->pending_map);
            net->pending_map = NULL;
            return 0;
        }
        net->pending_map[y][0] = '\0';
    }

    net->pending_map_width = width;
    net->pending_map_height = height;
    return 1;
}

static void free_pending_map(NetworkState *net) {
    if (net->pending_map) {
        for (int y = 0; y < net->pending_map_height; ++y) {
            if (net->pending_map[y]) {
                free(net->pending_map[y]);
            }
        }
        free(net->pending_map);
        net->pending_map = NULL;
    }
    net->pending_map_width = 0;
    net->pending_map_height = 0;
}

static void apply_received_map(Game *game, int rows, int width, char **data) {
    if (rows <= 0 || width <= 0 || !data) {
        return;
    }

    // Free existing map
    map_free(&game->map);
    game_free_game_maps(game);

    // Initialize map structure
    game->map.spawn_set = false;
    game->map.spawn_x = game->map.spawn_y = 0;

    // Allocate new map with received dimensions
    if (!map_allocate(&game->map, width, rows)) {
        return;
    }

    // Load the received data
    for (int y = 0; y < rows; ++y) {
        size_t len = strnlen_safe(data[y], width);
        for (int x = 0; x < width; ++x) {
            char raw = (x < (int)len) ? data[y][x] : '#';
            map_store_char(&game->map, x, y, raw);
        }
        game->map.tiles[y][width] = '\0';
    }

    map_enforce_border(&game->map);
    map_apply_wall_styles(&game->map);

    // Allocate game maps for the new dimensions
    if (!game_allocate_game_maps(game, game->map.width, game->map.height)) {
        map_free(&game->map);
        return;
    }

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
    net_sendf(net->peer_fd, "MAP %d %d\n", game->map.height, game->map.width);
    for (int y = 0; y < game->map.height; ++y) {
        char row[512];
        int width = game->map.width < 500 ? game->map.width : 500;
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

void network_shutdown(Game *game) {
    NetworkState *net = &game->net;
    if (net->peer_fd != -1) {
        close(net->peer_fd);
        net->peer_fd = -1;
    }
    if (net->listen_fd != -1) {
        close(net->listen_fd);
        net->listen_fd = -1;
    }
    free_pending_map(net);
    net->connected = false;
    net->remote.active = false;
    net->mode = NET_NONE;
}

void network_init(Game *game) {
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

void network_notify_memory_add(Game *game, int index) {
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

void network_notify_memory_edit(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1 || index < 0 || index >= game->memory_count) {
        return;
    }
    const MemoryEntry *entry = &game->memories[index];
    char encoded[MEMORY_TEXT * 2];
    encode_text(entry->text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "EDIT %d|%s\n", index, encoded);
}

void network_notify_memory_delete(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "DEL %d\n", index);
}

void network_send_request_add(Game *game, int gx, int gy, double nx, double ny, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    char encoded[MEMORY_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "REQADD %d %d %.3f %.3f|%s\n", gx, gy, nx, ny, encoded);
}

void network_send_request_edit(Game *game, int index, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    char encoded[MEMORY_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "REQEDIT %d|%s\n", index, encoded);
}

void network_send_request_delete(Game *game, int index) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "REQDEL %d\n", index);
}

void network_notify_door(Game *game, int gx, int gy, int state) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "DOOR %d %d %d\n", gx, gy, state);
}

void network_send_request_door(Game *game, int gx, int gy) {
    NetworkState *net = &game->net;
    if (net->mode != NET_CLIENT || net->peer_fd == -1) {
        return;
    }
    net_sendf(net->peer_fd, "REQDOOR %d %d\n", gx, gy);
}

void network_host_send_chat(Game *game, const char *sender, const char *text) {
    NetworkState *net = &game->net;
    if (net->mode != NET_HOST || net->peer_fd == -1) {
        return;
    }
    char encoded[CHAT_TEXT * 2];
    encode_text(text, encoded, sizeof(encoded));
    net_sendf(net->peer_fd, "CHAT %s|%s\n", sender ? sender : "Player", encoded);
    add_chat_message(game, sender ? sender : "Player", text);
}

void network_client_send_chat(Game *game, const char *text) {
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
        int height = 0, width = 0;
        if (sscanf(line + 4, "%d %d", &height, &width) == 2) {
            // Free any existing pending map
            free_pending_map(net);
            // Allocate new pending map with received dimensions
            if (allocate_pending_map(net, width, height)) {
                net->expected_map_rows = height;
                net->received_map_rows = 0;
                net->client_stage = CLIENT_STAGE_MAP_ROWS;
            }
        }
    } else if (strncmp(line, "ROW ", 4) == 0 && net->client_stage == CLIENT_STAGE_MAP_ROWS) {
        if (net->received_map_rows < net->pending_map_height && net->pending_map) {
            snprintf(net->pending_map[net->received_map_rows], net->pending_map_width + 1, "%s", line + 4);
            net->received_map_rows++;
        }
    } else if (strcmp(line, "ENDMAP") == 0 && net->client_stage == CLIENT_STAGE_MAP_ROWS) {
        apply_received_map(game, net->received_map_rows, net->pending_map_width, net->pending_map);
        free_pending_map(net);
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

void network_update(Game *game, double delta) {
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
