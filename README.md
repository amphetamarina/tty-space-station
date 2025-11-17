# poom

Palace of organized memories.

POOM is a lightweight, Doom2-inspired memory palace rendered with SDL2.
Walk a tile-based maze, drop custom “memory buildings”, and recall their text
as you revisit them. The engine ray-casts a pseudo-3D corridor with coherent
wall themes, textured floors/ceilings, HUD overlays, a minimap, and an in-game
text UI. Every memory appears as a glowing plaque embedded in the wall.

## Building

First install the SDL2 development package (`libsdl2-dev` on Debian/Ubuntu, `sdl2` on Arch,
`SDL2` via Homebrew on macOS).

```bash
make           # builds ./poom (requires SDL2 headers/libs)
make run       # builds (if needed) and launches the game
```

Linux (X11/Wayland) and macOS are supported. The SDL build opens a resizable window;
close it or press `Esc` to exit. When adding a memory the game pauses and a prompt
appears in the terminal where POOM was launched.

## Controls

- `W` / `S` – move forward and backward
- `A` / `D` or Left/Right arrows – rotate the camera
- `Q` / `E` – strafe left and right
- `M` – point the crosshair at a wall, then type the memory text directly in-game
- `V` – inspect the plaque you're aiming at (full-screen view, edit, delete)
- `E` / `N` – interact with NPCs (talk to puppy or ghost)
- `U` – activate server cabinet (opens terminal session)
- `F` – toggle doors you are aiming at
- `Esc` – quit the game, close dialogue, or exit terminal

### Terminal Controls (when in terminal mode):
- `ESC` – exit terminal and return to game
- `Enter` – send command to shell
- `Backspace`, `Tab`, `Arrow Keys` – standard shell navigation
- All keyboard input is sent to the shell session

Step near a memory plaque to see its contents in the HUD overlay and on the minimap.
Up to 96 memories can be stored per session.

### Memory entry UI

After pressing `M`, the camera pauses and a text box appears in the middle of the
screen. Aim at a wall, type up to 159 characters, press `Enter` to confirm, or
`Esc` to cancel. `Shift+Enter` inserts a newline so you can craft multi-line
descriptions, and each note is rendered as a framed plaque on the wall you selected.
Point at an existing plaque and press `V` for a full-screen viewer where you can
press `E` to edit or `Delete` to remove the entry.

### Persistence

When you load a map from disk, POOM automatically restores memories from a
companion `*.mem` file (e.g., `palace.map.mem`) and saves new entries there on
the fly. Extra controls:

- Set `POOM_SAVE_FILE=/path/to/memories.mem` to override the save path.
- Set `POOM_GENERATED_MAP=/path/to/new_palace.map` to export procedurally
  generated mazes and keep their notes between runs (POOM writes `*.mem` beside it).

## NPCs and Interaction

POOM now features interactive NPCs that bring life to your memory palace:

- **Puppy** (`P` in maps) - A friendly companion that wanders around. Woof!
- **Ghost** (`G` in maps) - A mysterious translucent entity that haunts your palace

NPCs have basic AI with idle, wander, and talk states. They avoid walls and furniture,
move around the palace, and can be interacted with by aiming at them and pressing `E` or `N`.
Each NPC has unique dialogue and personality. NPCs are rendered with Doom-style angle-based
sprites that change appearance based on your viewing angle (8 directions).

## Server Cabinets & Embedded Terminals

POOM features **embedded terminal emulation** - you can place server cabinets in your memory palace that open real shell sessions!

- **Server Cabinet** (`C` in maps) - Interactive terminal access points
- Press `U` when facing a cabinet to activate it
- Opens a full-screen terminal with real bash/sh shell
- Each cabinet maintains its own persistent session
- Full ANSI/VT100 terminal emulation with 16-color support
- Use terminals for:
  - Running commands and scripts
  - Managing files and processes
  - Organizing development workflows
  - Creating your perfect "memory palace server room"

Press `ESC` to exit terminal mode and return to the game. The shell session remains active in the background.

**Terminal Features:**
- 80x24 character display
- Full ANSI color support
- PTY (pseudo-terminal) with real shell process
- Arrow keys for command history
- Tab completion
- All standard bash/shell features

**Example:** Create a memory palace with server cabinets at strategic locations - each one a portal to different development environments, tmux sessions, or system administration tasks.

## Custom maps

A default layout is stored in `maps/palace.map`. Floors accept `.`, `,`, or `;` to pick
between `floor0/1/2.bmp`. Walls use `1`, `2`, `3` (standard) or `4` (window, tied
to `wall3.bmp`, rendered semi-transparent). Use `D` for a door (toggled with `F`)
and `X` for a spawn point.

Furniture glyphs let you decorate and add collision: `T`/`t` drop a square table
(lowercase rotates it), `R` a round table, `B` a bed, `S` a sofa, and `W` a wardrobe.

NPCs can be placed with: `P` for a puppy, `G` for a ghost.

Server cabinets: `C` for a terminal access point.

All props, NPCs, and cabinets block movement, show up on the minimap, and highlight their name
when you aim at them. Edit the file or point the game to a different map path:

```bash
POOM_MAP_FILE=~/my_palace.map ./poom
```

If no map is provided, a random maze is generated at runtime.

### Map Editor

POOM includes a graphical map editor for easy map creation:

```bash
# Build the editor
make mapeditor

# Edit an existing map
./mapeditor maps/palace.map

# Create a new map with custom dimensions
./mapeditor maps/newmap.map 60 40

# Quick launch
make editor
```

The map editor provides:
- **Point-and-click editing** with visual tile palette
- **Support for all tile types** (walls, floors, doors, furniture, NPCs)
- **Variable map sizes** from 10x10 to 100x100
- **Color-coded tiles** matching their in-game appearance
- **Grid overlay** for precision
- **Pan and zoom** for large maps
- **Real-time preview** as you edit

See `tools/README.md` for detailed editor documentation.

## Custom textures & assets

Drop optional BMP textures into `assets/textures/` to override the procedural materials:

- `assets/textures/wall0.bmp`, `wall1.bmp`, `wall2.bmp`, `wall3.bmp`
- `assets/textures/floor0.bmp`, `floor1.bmp`, `floor2.bmp`
- `assets/textures/ceiling0.bmp`, `ceiling1.bmp`
- `assets/textures/table_square.bmp`, `table_round.bmp`, `bed.bmp`, `sofa.bmp`, `wardrobe.bmp`
- `assets/textures/door.bmp`

Future NPC sprites can be placed in `assets/sprites/` for custom appearances.

Images are scaled to 64×64; missing files fall back to the built-in art. Great
free sources for retro walls/floors include:

- [Kenney Assets](https://www.kenney.nl/assets) (CC0/CC-BY tilesets)
- [OpenGameArt.org](https://opengameart.org/) (community textures/sprites)
- [Freedoom](https://freedoom.github.io/) (GPL Doom-compatible graphics)

Convert PNG/JPG textures to BMP (e.g., `convert file.png file.bmp`) before
placing them here so SDL can load them without extra libraries.

## Code Organization

The codebase has been reorganized into clean, modular components for better maintainability:

### Directory Structure
```
poom/
├── src/              # Source files
│   ├── main.c        # Entry point and event loop
│   ├── game.c/h      # Game state management
│   ├── player.c/h    # Player movement and collision
│   ├── map.c/h       # Map loading and generation
│   ├── memory.c/h    # Memory palace system
│   ├── furniture.c/h # Furniture management
│   ├── cabinet.c/h   # Server cabinet management
│   ├── terminal.c/h  # Terminal emulation and PTY
│   ├── npc.c/h       # NPC AI and behavior
│   ├── renderer.c/h  # Raycasting and scene rendering
│   ├── ui.c/h        # HUD, minimap, dialogue boxes
│   ├── texture.c/h   # Texture generation and loading
│   ├── utils.c/h     # Utility functions
│   └── types.h       # Core data structures
├── include/          # External headers (font8x8)
├── assets/
│   ├── textures/     # Custom BMP textures
│   └── sprites/      # NPC and object sprites
└── maps/             # Map files and memory saves
```

### Recent Improvements

**Terminal Emulation System (NEW!):**
- Full PTY-based terminal emulation embedded in the game
- Real bash/sh shell sessions inside server cabinets
- Complete ANSI/VT100 escape sequence parser
- 80x24 character display with 16-color support
- Per-cabinet persistent sessions
- Transform your memory palace into a server room!

**Visual Enhancements:**
- Fixed memory plaque rendering (no more nausea-inducing zoom)
- Doom-style angle-based sprite rendering (8 directions) for furniture and NPCs
- Memory plaques display as stable framed text boxes
- Ceiling/floor rendering reverted to original stable values

**NPC System:**
- Interactive NPCs with AI (idle, wander, talk states)
- Collision detection and pathfinding
- Dialogue system with on-screen text boxes
- Doom-style billboard sprites with viewing angle variation

**Map System:**
- Variable map sizes (10x10 to 200x200)
- Dynamic memory allocation
- GUI map editor with point-and-click editing

**Code Quality:**
- Removed all networking/multiplayer code (simplified codebase)
- Modular architecture with clear separation of concerns
- Clean header organization and dependency management
- Comprehensive documentation in each module
- Easy to extend with new features
