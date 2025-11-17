// Terminal emulation module with PTY support
#define _POSIX_C_SOURCE 200809L
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <pty.h>
#include <utmp.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#else
#include <pty.h>
#endif

void terminal_init(Terminal *term) {
    memset(term, 0, sizeof(*term));
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->cursor_visible = true;
    term->saved_cursor_x = 0;
    term->saved_cursor_y = 0;
    term->pty_fd = -1;
    term->shell_pid = -1;
    term->active = false;
    term->parse_state = PARSE_NORMAL;
    term->current_fg = 7;  // White
    term->current_bg = 0;  // Black
    term->current_attrs = 0;

    // Initialize all cells to blank
    for (int y = 0; y < TERM_ROWS; y++) {
        for (int x = 0; x < TERM_COLS; x++) {
            term->cells[y][x].ch = ' ';
            term->cells[y][x].fg_color = 7;
            term->cells[y][x].bg_color = 0;
            term->cells[y][x].attrs = 0;
        }
    }
}

int terminal_spawn_shell(Terminal *term) {
    if (term->active) {
        return 0; // Already active
    }

    struct winsize ws = {
        .ws_row = TERM_ROWS,
        .ws_col = TERM_COLS,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);

    if (pid < 0) {
        perror("forkpty");
        return 0;
    }

    if (pid == 0) {
        // Child process - exec shell
        setenv("TERM", "ansi", 1);
        setenv("COLORTERM", "truecolor", 1);

        // Try bash, then sh
        execl("/bin/bash", "bash", NULL);
        execl("/bin/sh", "sh", NULL);

        // If we get here, exec failed
        fprintf(stderr, "Failed to exec shell\n");
        exit(1);
    }

    // Parent process
    term->pty_fd = master_fd;
    term->shell_pid = pid;
    term->active = true;

    // Set non-blocking mode
    int flags = fcntl(master_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
    }

    return 1;
}

void terminal_close(Terminal *term) {
    if (!term->active) {
        return;
    }

    if (term->pty_fd >= 0) {
        close(term->pty_fd);
        term->pty_fd = -1;
    }

    if (term->shell_pid > 0) {
        kill(term->shell_pid, SIGTERM);
        waitpid(term->shell_pid, NULL, WNOHANG);
        term->shell_pid = -1;
    }

    term->active = false;
}

void terminal_write(Terminal *term, const char *data, size_t len) {
    if (!term->active || term->pty_fd < 0) {
        return;
    }

    ssize_t written = write(term->pty_fd, data, len);
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error writing, close terminal
        terminal_close(term);
    }
}

void terminal_scroll_up(Terminal *term) {
    // Move all rows up by one
    for (int y = 0; y < TERM_ROWS - 1; y++) {
        memcpy(term->cells[y], term->cells[y + 1], sizeof(TermCell) * TERM_COLS);
    }

    // Clear bottom row
    for (int x = 0; x < TERM_COLS; x++) {
        term->cells[TERM_ROWS - 1][x].ch = ' ';
        term->cells[TERM_ROWS - 1][x].fg_color = term->current_fg;
        term->cells[TERM_ROWS - 1][x].bg_color = term->current_bg;
        term->cells[TERM_ROWS - 1][x].attrs = term->current_attrs;
    }
}

void terminal_newline(Terminal *term) {
    term->cursor_y++;
    if (term->cursor_y >= TERM_ROWS) {
        terminal_scroll_up(term);
        term->cursor_y = TERM_ROWS - 1;
    }
}

void terminal_carriage_return(Terminal *term) {
    term->cursor_x = 0;
}

void terminal_clear(Terminal *term) {
    for (int y = 0; y < TERM_ROWS; y++) {
        for (int x = 0; x < TERM_COLS; x++) {
            term->cells[y][x].ch = ' ';
            term->cells[y][x].fg_color = term->current_fg;
            term->cells[y][x].bg_color = term->current_bg;
            term->cells[y][x].attrs = term->current_attrs;
        }
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void terminal_put_char(Terminal *term, char ch) {
    if (term->cursor_y >= TERM_ROWS) {
        term->cursor_y = TERM_ROWS - 1;
    }

    // Auto-wrap to next line if we're at the edge
    if (term->cursor_x >= TERM_COLS) {
        term->cursor_x = 0;
        terminal_newline(term);
    }

    if (term->cursor_y < TERM_ROWS && term->cursor_x < TERM_COLS) {
        term->cells[term->cursor_y][term->cursor_x].ch = ch;
        term->cells[term->cursor_y][term->cursor_x].fg_color = term->current_fg;
        term->cells[term->cursor_y][term->cursor_x].bg_color = term->current_bg;
        term->cells[term->cursor_y][term->cursor_x].attrs = term->current_attrs;
        term->cursor_x++;

        // Auto-wrap when we hit the right edge
        if (term->cursor_x >= TERM_COLS) {
            term->cursor_x = 0;
            terminal_newline(term);
        }
    }
}

void terminal_handle_csi(Terminal *term) {
    // Parse CSI sequence
    if (term->csi_buffer_len == 0) {
        return;
    }

    char *buf = term->csi_buffer;
    char final = buf[term->csi_buffer_len - 1];

    // Check for private mode sequences (start with '?')
    bool private_mode = (buf[0] == '?');
    char *param_start = private_mode ? buf + 1 : buf;

    // Parse numeric parameters
    term->ansi_param_count = 0;
    char *ptr = param_start;
    while (*ptr && term->ansi_param_count < 16) {
        if (*ptr >= '0' && *ptr <= '9') {
            term->ansi_params[term->ansi_param_count] = atoi(ptr);
            term->ansi_param_count++;
            while (*ptr >= '0' && *ptr <= '9') ptr++;
        } else {
            ptr++;
        }
    }

    // Handle private mode sequences
    if (private_mode) {
        switch (final) {
            case 'h': // Set private mode
                if (term->ansi_param_count > 0 && term->ansi_params[0] == 25) {
                    // Show cursor
                    term->cursor_visible = true;
                }
                return;
            case 'l': // Reset private mode
                if (term->ansi_param_count > 0 && term->ansi_params[0] == 25) {
                    // Hide cursor
                    term->cursor_visible = false;
                }
                return;
        }
    }

    // Handle commands
    switch (final) {
        case 'H': // Cursor position
        case 'f': {
            int row = (term->ansi_param_count > 0 && term->ansi_params[0] > 0) ? term->ansi_params[0] - 1 : 0;
            int col = (term->ansi_param_count > 1 && term->ansi_params[1] > 0) ? term->ansi_params[1] - 1 : 0;
            term->cursor_y = row < TERM_ROWS ? row : TERM_ROWS - 1;
            term->cursor_x = col < TERM_COLS ? col : TERM_COLS - 1;
            break;
        }
        case 'A': { // Cursor up
            int n = (term->ansi_param_count > 0 && term->ansi_params[0] > 0) ? term->ansi_params[0] : 1;
            term->cursor_y -= n;
            if (term->cursor_y < 0) term->cursor_y = 0;
            break;
        }
        case 'B': { // Cursor down
            int n = (term->ansi_param_count > 0 && term->ansi_params[0] > 0) ? term->ansi_params[0] : 1;
            term->cursor_y += n;
            if (term->cursor_y >= TERM_ROWS) term->cursor_y = TERM_ROWS - 1;
            break;
        }
        case 'C': { // Cursor right
            int n = (term->ansi_param_count > 0 && term->ansi_params[0] > 0) ? term->ansi_params[0] : 1;
            term->cursor_x += n;
            if (term->cursor_x >= TERM_COLS) term->cursor_x = TERM_COLS - 1;
            break;
        }
        case 'D': { // Cursor left
            int n = (term->ansi_param_count > 0 && term->ansi_params[0] > 0) ? term->ansi_params[0] : 1;
            term->cursor_x -= n;
            if (term->cursor_x < 0) term->cursor_x = 0;
            break;
        }
        case 'J': { // Erase in Display (ED)
            int n = (term->ansi_param_count > 0) ? term->ansi_params[0] : 0;
            if (n == 0) {
                // Clear from cursor to end of screen
                // Clear rest of current line
                for (int x = term->cursor_x; x < TERM_COLS; x++) {
                    term->cells[term->cursor_y][x].ch = ' ';
                    term->cells[term->cursor_y][x].fg_color = term->current_fg;
                    term->cells[term->cursor_y][x].bg_color = term->current_bg;
                    term->cells[term->cursor_y][x].attrs = term->current_attrs;
                }
                // Clear all lines below cursor
                for (int y = term->cursor_y + 1; y < TERM_ROWS; y++) {
                    for (int x = 0; x < TERM_COLS; x++) {
                        term->cells[y][x].ch = ' ';
                        term->cells[y][x].fg_color = term->current_fg;
                        term->cells[y][x].bg_color = term->current_bg;
                        term->cells[y][x].attrs = term->current_attrs;
                    }
                }
            } else if (n == 1) {
                // Clear from cursor to beginning of screen
                // Clear all lines above cursor
                for (int y = 0; y < term->cursor_y; y++) {
                    for (int x = 0; x < TERM_COLS; x++) {
                        term->cells[y][x].ch = ' ';
                        term->cells[y][x].fg_color = term->current_fg;
                        term->cells[y][x].bg_color = term->current_bg;
                        term->cells[y][x].attrs = term->current_attrs;
                    }
                }
                // Clear from beginning of current line to cursor
                for (int x = 0; x <= term->cursor_x && x < TERM_COLS; x++) {
                    term->cells[term->cursor_y][x].ch = ' ';
                    term->cells[term->cursor_y][x].fg_color = term->current_fg;
                    term->cells[term->cursor_y][x].bg_color = term->current_bg;
                    term->cells[term->cursor_y][x].attrs = term->current_attrs;
                }
            } else if (n == 2) {
                // Clear entire screen
                terminal_clear(term);
            }
            break;
        }
        case 'K': { // Clear line
            int n = (term->ansi_param_count > 0) ? term->ansi_params[0] : 0;
            if (term->cursor_y < TERM_ROWS) {
                if (n == 0) {
                    // Clear to end of line
                    for (int x = term->cursor_x; x < TERM_COLS; x++) {
                        term->cells[term->cursor_y][x].ch = ' ';
                        term->cells[term->cursor_y][x].fg_color = term->current_fg;
                        term->cells[term->cursor_y][x].bg_color = term->current_bg;
                        term->cells[term->cursor_y][x].attrs = term->current_attrs;
                    }
                } else if (n == 1) {
                    // Clear from beginning of line
                    for (int x = 0; x <= term->cursor_x && x < TERM_COLS; x++) {
                        term->cells[term->cursor_y][x].ch = ' ';
                        term->cells[term->cursor_y][x].fg_color = term->current_fg;
                        term->cells[term->cursor_y][x].bg_color = term->current_bg;
                        term->cells[term->cursor_y][x].attrs = term->current_attrs;
                    }
                } else if (n == 2) {
                    // Clear entire line
                    for (int x = 0; x < TERM_COLS; x++) {
                        term->cells[term->cursor_y][x].ch = ' ';
                        term->cells[term->cursor_y][x].fg_color = term->current_fg;
                        term->cells[term->cursor_y][x].bg_color = term->current_bg;
                        term->cells[term->cursor_y][x].attrs = term->current_attrs;
                    }
                }
            }
            break;
        }
        case 'm': { // SGR - Set graphics rendition
            for (int i = 0; i < term->ansi_param_count; i++) {
                int param = term->ansi_params[i];
                if (param == 0) {
                    // Reset
                    term->current_fg = 7;
                    term->current_bg = 0;
                    term->current_attrs = 0;
                } else if (param >= 30 && param <= 37) {
                    // Foreground color
                    term->current_fg = param - 30;
                } else if (param >= 40 && param <= 47) {
                    // Background color
                    term->current_bg = param - 40;
                } else if (param >= 90 && param <= 97) {
                    // Bright foreground color
                    term->current_fg = param - 90 + 8;
                } else if (param >= 100 && param <= 107) {
                    // Bright background color
                    term->current_bg = param - 100 + 8;
                } else if (param == 1) {
                    // Bold
                    term->current_attrs |= 1;
                } else if (param == 4) {
                    // Underline
                    term->current_attrs |= 2;
                }
            }
            if (term->ansi_param_count == 0) {
                // No params means reset
                term->current_fg = 7;
                term->current_bg = 0;
                term->current_attrs = 0;
            }
            break;
        }
        case 's': { // Save cursor position
            term->saved_cursor_x = term->cursor_x;
            term->saved_cursor_y = term->cursor_y;
            break;
        }
        case 'u': { // Restore cursor position
            term->cursor_x = term->saved_cursor_x;
            term->cursor_y = term->saved_cursor_y;
            // Clamp to valid range
            if (term->cursor_x < 0) term->cursor_x = 0;
            if (term->cursor_x >= TERM_COLS) term->cursor_x = TERM_COLS - 1;
            if (term->cursor_y < 0) term->cursor_y = 0;
            if (term->cursor_y >= TERM_ROWS) term->cursor_y = TERM_ROWS - 1;
            break;
        }
    }
}

void terminal_parse_byte(Terminal *term, uint8_t byte) {
    switch (term->parse_state) {
        case PARSE_NORMAL:
            if (byte == '\033') {
                term->parse_state = PARSE_ESC;
            } else if (byte == '\n') {
                terminal_newline(term);
            } else if (byte == '\r') {
                terminal_carriage_return(term);
            } else if (byte == '\b') {
                if (term->cursor_x > 0) {
                    term->cursor_x--;
                }
            } else if (byte == '\t') {
                // Tab - move to next tab stop (every 8 columns)
                term->cursor_x = ((term->cursor_x / 8) + 1) * 8;
                if (term->cursor_x >= TERM_COLS) {
                    term->cursor_x = 0;
                    terminal_newline(term);
                }
            } else if (byte >= 32 && byte < 127) {
                terminal_put_char(term, (char)byte);
            }
            break;

        case PARSE_ESC:
            if (byte == '[') {
                term->parse_state = PARSE_CSI;
                term->csi_buffer_len = 0;
                memset(term->csi_buffer, 0, sizeof(term->csi_buffer));
            } else if (byte == 'c') {
                // RIS - Reset to Initial State (ESC c)
                terminal_init(term);
                term->active = true;  // Keep terminal active
                term->parse_state = PARSE_NORMAL;
            } else {
                // Unknown escape sequence, ignore
                term->parse_state = PARSE_NORMAL;
            }
            break;

        case PARSE_CSI:
            if ((size_t)term->csi_buffer_len < sizeof(term->csi_buffer) - 1) {
                term->csi_buffer[term->csi_buffer_len++] = byte;
            }

            // Check if this is a final byte
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) {
                terminal_handle_csi(term);
                term->parse_state = PARSE_NORMAL;
            }
            break;

        default:
            term->parse_state = PARSE_NORMAL;
            break;
    }
}

void terminal_update(Terminal *term) {
    if (!term->active || term->pty_fd < 0) {
        return;
    }

    // Check if shell has died
    int status;
    pid_t result = waitpid(term->shell_pid, &status, WNOHANG);
    if (result != 0) {
        // Shell has exited
        terminal_close(term);
        return;
    }

    // Read from PTY
    ssize_t nread = read(term->pty_fd, term->read_buffer, sizeof(term->read_buffer) - 1);

    if (nread < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error reading, close terminal
            terminal_close(term);
        }
        return;
    }

    if (nread == 0) {
        return;
    }

    // Parse each byte
    for (ssize_t i = 0; i < nread; i++) {
        terminal_parse_byte(term, (uint8_t)term->read_buffer[i]);
    }
}
