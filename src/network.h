#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"
#include <stdbool.h>
#include <sys/types.h>

// Network initialization and management
void network_init(Game *game);
void network_shutdown(Game *game);
void network_update(Game *game, double delta);

// Network notification functions (host to client)
void network_notify_memory_add(Game *game, int index);
void network_notify_memory_edit(Game *game, int index);
void network_notify_memory_delete(Game *game, int index);
void network_notify_door(Game *game, int gx, int gy, int state);

// Network request functions (client to host)
void network_send_request_add(Game *game, int gx, int gy, double nx, double ny, const char *text);
void network_send_request_edit(Game *game, int index, const char *text);
void network_send_request_delete(Game *game, int index);
void network_send_request_door(Game *game, int gx, int gy);

// Chat functions
void network_host_send_chat(Game *game, const char *sender, const char *text);
void network_client_send_chat(Game *game, const char *text);

// Utility functions
void encode_text(const char *src, char *dst, size_t dst_size);
void decode_text(const char *src, char *dst, size_t dst_size);
int parse_port(const char *value, int fallback);
int set_nonblocking(int fd);
ssize_t net_send_all(int fd, const char *data, size_t len);
void net_sendf(int fd, const char *fmt, ...);

#endif // NETWORK_H
