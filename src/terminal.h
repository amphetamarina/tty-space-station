#ifndef TERMINAL_H
#define TERMINAL_H

#include "types.h"

// Terminal initialization and lifecycle
void terminal_init(Terminal *term);
int terminal_spawn_shell(Terminal *term);
void terminal_close(Terminal *term);

// Terminal I/O
void terminal_write(Terminal *term, const char *data, size_t len);
void terminal_update(Terminal *term);

// Terminal manipulation
void terminal_clear(Terminal *term);
void terminal_put_char(Terminal *term, char ch);
void terminal_parse_byte(Terminal *term, uint8_t byte);

// Helper functions
void terminal_scroll_up(Terminal *term);
void terminal_newline(Terminal *term);
void terminal_carriage_return(Terminal *term);
void terminal_handle_csi(Terminal *term);

#endif // TERMINAL_H
