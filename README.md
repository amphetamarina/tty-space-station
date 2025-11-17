# TTY Space Station

> ⚠️ **ALPHA SOFTWARE** - This project is highly experimental and under active development. Expect bugs, crashes, and breaking changes!

![alt text](logo.png)
A Doom-style 3D raycasting game where you explore a space station filled with functional Unix terminals. Walk through corridors, interact with server cabinets, and use wall-mounted displays - all with real embedded terminal emulation.

![Status: Alpha](https://img.shields.io/badge/status-alpha-orange)
![Platform: Linux/macOS](https://img.shields.io/badge/platform-linux%20%7C%20macos-blue)

## Demo

https://github.com/user-attachments/assets/0f88f341-2d11-4c52-9216-f0beaec4ea48


## What is This?

TTY Space Station combines retro first-person raycasting graphics (think Wolfenstein 3D or classic Doom) with functional Unix terminal emulation. Explore a cyberpunk space station where:

- **Server Cabinets** open real bash/shell sessions
- **Wall-mounted Displays** show terminal output (currently disabled due to bugs)
- **Fully explorable 3D environment** with Doom-style rendering
- **Real PTY terminal emulation** with ANSI/VT100 support

Think of it as a 3D terminal multiplexer meets classic FPS.

## Features

### Terminal Emulation
- Full PTY-based real shell sessions (bash/sh)
- ANSI/VT100 escape sequence support
- 16-color terminal display
- Ctrl+key combinations (Ctrl+C, Ctrl+D, etc.)
- Works with vim, emacs, htop, and other terminal apps
- Each cabinet maintains its own persistent session

### Graphics & Rendering
- Raycasting 3D engine (Doom/Wolfenstein style)
- Textured walls, floors, and ceilings
- Doom-style cylindrical sky panorama (starfield)
- 3D server cabinets (4 texture variations)
- Wall-mounted terminal displays
- Minimap with real-time position tracking
- Smooth movement and rotation

### Map System
- Variable map sizes (10x10 to 100x100)
- Custom map loading from `.map` files
- Procedural maze generation (fallback)
- Doors that open/close
- Display stacking for larger screens
- GUI map editor included

## Building

### Prerequisites

Install SDL2 development package:
- **Debian/Ubuntu**: `sudo apt install libsdl2-dev`
- **Arch Linux**: `sudo pacman -S sdl2`
- **macOS**: `brew install sdl2`

### Compile

```bash
make           # builds ./tty-space-station
make run       # build and launch immediately
make editor    # launch the map editor
```

Linux (X11/Wayland) and macOS are supported.

## Controls

### Movement
- `W` / `S` - Move forward/backward
- `A` / `D` - Rotate camera left/right
- `Q` / `E` - Strafe left/right
- `Arrow Keys` - Also rotate camera
- `ESC` - Quit game

### Interaction
- `U` - Activate server cabinet (when facing one)
- `E` - Activate wall display (when facing one)
- `F` - Toggle door (when facing one)

### Terminal Mode Controls
When inside a terminal (after pressing `U` on a cabinet):

- **F1** - Exit terminal and return to game
- `Enter` - Send command
- `Backspace`, `Tab` - Standard keys
- `Arrow Keys` - Command history / cursor movement
- `Ctrl+A` through `Ctrl+Z` - Full Ctrl combinations
- `Delete`, `Home`, `End`, `PageUp`, `PageDown` - Navigation keys
- `ESC` - Sends ESC to terminal (for vim, etc.)

**Note**: Exit terminal with `F1`, not `ESC` - this allows vim and other apps to work properly!

## Custom Maps

Maps are text files defining the station layout. Default map: `maps/palace.map`

### Map Tile Reference

**Walls:**
- `1`, `2`, `3` - Different wall textures
- `4` - Window wall (semi-transparent)

**Floors:**
- `.` - Floor texture 0
- `,` - Floor texture 1
- `;` - Floor texture 2

**Interactive:**
- `D` - Door (toggle with `F`)
- `C` - Server cabinet (activate with `U`)
- `d` - Wall-mounted display (activate with `E`)

**Special:**
- `X` - Player spawn point
- ` ` (space) - Empty/void

### Display Stacking

Place displays adjacent to each other to create larger screens:
```
DDD    Creates a 3-wide display
DDD    stacked 2 tiles high
```

Displays only stack horizontally (when facing up/down) or vertically (when facing left/right).

### Load Custom Maps

```bash
TSS_MAP_FILE=~/my_station.map ./tty-space-station
```

If no map is provided, a random maze is generated at runtime.

### Export Generated Maps

```bash
TSS_GENERATED_MAP=~/generated.map ./tty-space-station
```

## Map Editor

A graphical map editor is included for easy map creation:

```bash
# Build the editor
make mapeditor

# Edit existing map
./mapeditor maps/palace.map

# Create new map with custom size
./mapeditor maps/newstation.map 60 40

# Quick launch with default map
make editor
```

See `tools/README.md` for detailed editor usage.

## Custom Textures

Drop optional 64×64 BMP textures into `assets/textures/` to override procedural textures:

**Walls:**
- `wall0.bmp`, `wall1.bmp`, `wall2.bmp`, `wall3.bmp`

**Floors/Ceilings:**
- `floor0.bmp`, `floor1.bmp`, `floor2.bmp`
- `ceiling0.bmp`, `ceiling1.bmp`

**Objects:**
- `door.bmp`
- `cabinet.bmp`, `cabinet1.bmp`, `cabinet2.bmp`, `cabinet3.bmp`
- `display.bmp`

**Sky:**
- `sky.bmp` (512×128 cylindrical panorama)

Missing files fall back to built-in procedural generation.

### Texture Resources

- [Kenney Assets](https://www.kenney.nl/assets) - CC0/CC-BY tilesets
- [OpenGameArt.org](https://opengameart.org/) - Community textures
- [Freedoom](https://freedoom.github.io/) - GPL Doom-compatible graphics

Convert PNG/JPG to BMP: `convert texture.png texture.bmp`

## Project Structure

```
tty-space-station/
├── src/              # Source code
│   ├── main.c        # Entry point and event loop
│   ├── game.c/h      # Game state management
│   ├── player.c/h    # Player movement and collision
│   ├── map.c/h       # Map loading and generation
│   ├── cabinet.c/h   # Server cabinet management
│   ├── display.c/h   # Wall-mounted displays
│   ├── terminal.c/h  # Terminal emulation (PTY + ANSI parsing)
│   ├── renderer.c/h  # Raycasting engine
│   ├── ui.c/h        # HUD and minimap
│   ├── texture.c/h   # Texture generation and loading
│   ├── utils.c/h     # Utility functions
│   └── types.h       # Core data structures
├── include/          # External headers
│   └── font8x8_basic.h
├── tools/            # Map editor and utilities
├── assets/
│   └── textures/     # Custom BMP textures (optional)
└── maps/             # Map files (.map)
```

## Known Issues & Limitations

- **Display terminal text is currently disabled** due to crashes when exiting terminals
- Displays show as solid dark blue/teal screens instead of live terminal content
- Some terminal apps may not work perfectly (clear command issues reported)
- Performance could be better (no optimization yet)
- Map editor is basic and could use many improvements
- No sound or music
- Raycasting has some visual artifacts

See `TODO.md` for planned improvements.

## Technical Details

### Terminal Emulation
- Uses `forkpty()` to spawn real shell processes
- ANSI/VT100 escape sequence state machine parser
- 80x24 character grid (configurable via `TERM_COLS`/`TERM_ROWS` in types.h)
- Non-blocking PTY I/O
- Proper signal handling for shell lifecycle

### Rendering
- Raycasting DDA (Digital Differential Analysis) for walls
- Textured floor/ceiling with perspective-correct mapping
- Depth-sorted sprite rendering for cabinets
- Vertical door rendering with transparency
- Fixed-point arithmetic for performance

### Supported Platforms
- Linux (X11, Wayland)
- macOS
- Requires SDL2 2.0+
- C11 compiler (gcc, clang)

## Development

This project was created as an experiment in combining retro game rendering with practical Unix tools. It's a playground for:
- Raycasting graphics programming
- Terminal emulation techniques
- Game engine architecture
- Procedural texture generation

The codebase is modular and relatively easy to extend. See `CLAUDE.md` for development notes.

## Credits

- Raycasting inspired by Wolfenstein 3D, Doom, and Lode Vandevenne's tutorials
- Terminal rendering uses [font8x8](https://github.com/dhepper/font8x8) by dhepper
- Built with SDL2

## License

MIT License - See LICENSE file for details.

## Contributing

This is an experimental alpha project. Bug reports and suggestions are welcome, but expect rapid changes and refactoring.

---

**Remember**: This is ALPHA software! Save your work frequently and don't rely on it for anything critical. Have fun exploring! 🚀
