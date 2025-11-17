# POOM Terminal Emulation System

## Overview
A complete terminal emulation system has been implemented for POOM, allowing players to interact with "server cabinets" in the game world that open shell sessions.

## Features Implemented

### 1. Terminal Module (`terminal.c/h`)
- **Full PTY (Pseudo-Terminal) Support**: Spawns real bash/sh shells using forkpty()
- **ANSI/VT100 Parser**: Handles escape sequences for colors, cursor movement, and screen clearing
- **Non-blocking I/O**: Terminal updates don't block the game loop
- **80x24 Character Grid**: Standard terminal dimensions

**Supported ANSI Sequences:**
- Cursor movement: `[H`, `[row;colH`, `[A/B/C/D` (up/down/right/left)
- Screen control: `[2J` (clear screen), `[K` (clear line)
- Colors: `[30-37m` (foreground), `[40-47m` (background), `[90-97m`, `[100-107m` (bright)
- Attributes: `[0m` (reset), `[1m` (bold), `[4m` (underline)

### 2. Cabinet System (`cabinet.c/h`)
- **Cabinet Entities**: New entity type that can be placed on maps with 'C' marker
- **Collision Detection**: Cabinets block player movement
- **Terminal Association**: Each cabinet has its own dedicated terminal instance
- **Activation**: Press 'U' key while facing a cabinet to open its terminal

### 3. Rendering System
- **Full-screen Terminal Display**: Terminal overlays the game view when active
- **8x8 Font Rendering**: Uses existing font8x8_basic.h
- **16-Color ANSI Palette**: Supports all standard ANSI colors
- **Cursor Rendering**: Visible underline cursor at current position

### 4. Input Handling
- **Text Input**: All text is sent directly to the shell
- **Special Keys**:
  - `ESC`: Exit terminal mode and return to game
  - `Enter`: Send newline to shell
  - `Backspace`: Send backspace
  - `Arrow Keys`: Send ANSI escape sequences for shell navigation
  - `Tab`: Send tab character for completion

### 5. Game Integration
- **Automatic Initialization**: Terminals initialized when game starts
- **Automatic Cleanup**: All shells properly terminated on game exit
- **Collision System**: Player cannot walk through cabinets
- **State Management**: Terminal mode prevents game movement/input

## Map Integration

To place cabinets in your map:
1. Use the map editor or manually edit .map files
2. Add 'C' character in the decor layer where you want a cabinet
3. The cabinet will automatically appear with collision and be activatable

## Controls

### In Game Mode:
- `U`: Activate cabinet (when facing one)
- Normal POOM controls for movement

### In Terminal Mode:
- `ESC`: Exit terminal and return to game
- All other keys: Sent to the shell session
- Arrow keys: Navigate command history / move cursor in shell
- Tab: Shell auto-completion

## Technical Details

### Files Created:
- `src/terminal.h` - Terminal data structures and API
- `src/terminal.c` - PTY management and ANSI parser (370+ lines)
- `src/cabinet.h` - Cabinet entity API
- `src/cabinet.c` - Cabinet management logic

### Files Modified:
- `src/types.h` - Added Terminal and Cabinet structures
- `src/renderer.h/c` - Added render_terminal() function
- `src/game.h/c` - Added terminal lifecycle management
- `src/main.c` - Added terminal mode input routing and rendering
- `src/player.c` - Added cabinet collision detection
- `Makefile` - Added new source files and -lutil linker flag

### Dependencies:
- POSIX PTY functions (forkpty, etc.)
- `-lutil` library for PTY support
- Standard C libraries

## Architecture

```
Game Loop
    |
    +-- Terminal Mode Check
    |       |
    |       +-- Yes: Render Terminal & Route Input to PTY
    |       |
    |       +-- No: Normal Game Rendering & Input
    |
    +-- Terminal Update (read from PTY, parse ANSI)
    |
    +-- Cabinet Collision Check (player movement)
```

## Example Usage

1. Start the game: `./poom`
2. Find a cabinet (marked with 'C' on map)
3. Face the cabinet and press 'U'
4. A shell session opens in full-screen terminal
5. Run commands: `ls`, `ps`, `echo "Hello from POOM!"`, etc.
6. Press ESC to exit terminal and return to game
7. The shell continues running in the background
8. Press 'U' again to return to the same session

## Safety Features

- Proper PTY cleanup on exit
- Shell process termination with SIGTERM
- Non-blocking I/O prevents game freezing
- Error handling for failed shell spawns
- Collision detection prevents clipping through cabinets

## Performance

- Minimal overhead when not in terminal mode
- Terminal updates only when active
- Efficient ANSI parser (single-pass state machine)
- No dynamic memory allocation during operation

---

Implementation complete and fully functional!
