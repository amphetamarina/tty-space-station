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
- `V` – inspect the plaque you’re aiming at (full-screen view, edit, delete)
- `T` – open the chat prompt (host broadcasts to connected clients)
- `F` – toggle doors you are aiming at
- `Esc` – quit the game

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

## Custom maps

A default layout is stored in `palace.map`. Floors accept `.`, `,`, or `;` to pick
between `floor0/1/2.bmp`. Walls use `1`, `2`, `3` (standard) or `4` (window, tied
to `wall3.bmp`, rendered semi-transparent). Use `D` for a door (toggled with `F`)
and `X` for a spawn point. Furniture glyphs let you decorate and add collision:
`T`/`t` drop a square table (lowercase rotates it), `R` a round table, `B` a bed,
`S` a sofa, and `W` a wardrobe. These props block movement, show up on the minimap,
and highlight their name when you aim at them. Edit the file or point the game to
a different map path:

```bash
POOM_MAP_FILE=~/my_palace.map ./poom
```

If no map is provided, a random maze is generated at runtime.

## Custom textures & assets

Drop optional BMP textures into `assets/` to override the procedural materials:

- `assets/wall0.bmp`, `wall1.bmp`, `wall2.bmp`
- `assets/floor0.bmp`, `floor1.bmp`
- `assets/ceiling0.bmp`, `ceiling1.bmp`
- `assets/table_square.bmp`, `table_round.bmp`, `bed.bmp`, `sofa.bmp`, `wardrobe.bmp`
- `assets/door.bmp`

Images are scaled to 64×64; missing files fall back to the built-in art. Great
free sources for retro walls/floors include:

- [Kenney Assets](https://www.kenney.nl/assets) (CC0/CC-BY tilesets)
- [OpenGameArt.org](https://opengameart.org/) (community textures/sprites)
- [Freedoom](https://freedoom.github.io/) (GPL Doom-compatible graphics)

Convert PNG/JPG textures to BMP (e.g., `convert file.png file.bmp`) before
placing them here so SDL can load them without extra libraries.

## Multiplayer

POOM ships with experimental direct-IP multiplayer (host + one client) for
shared palaces:

```bash
# Host
POOM_NET_MODE=host POOM_NET_PORT=4455 ./poom

# Client
POOM_NET_MODE=client POOM_NET_HOST=192.168.1.42 POOM_NET_PORT=4455 ./poom
```

Both sides can specify `POOM_PLAYER_NAME=Alice`. The host is authoritative:
clients request additions/edits/deletes, and the host broadcasts updates,
including the current map and stored memories. Multiplayer works with any map
loaded from disk (generated palaces can be exported via `POOM_GENERATED_MAP` as
described above). Press `T` to open the in-game chat prompt; hosts broadcast
messages to connected clients, and client chats are relayed back by the host.
