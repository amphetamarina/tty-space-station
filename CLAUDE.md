# Claude Development Notes

This document provides context and guidance for future Claude Code sessions working on TTY Space Station.

## Project Overview

**TTY Space Station** is a Doom-style raycasting 3D game with embedded Unix terminal emulation. It was originally called "POOM" (Palace of Organized Memories) and has undergone significant refactoring to focus on the terminal/display aspects.

### Core Concept
A first-person 3D space station where players can:
- Walk through Doom-style raycasted corridors
- Interact with server cabinets that open real bash/sh terminals
- Use wall-mounted displays (currently buggy)
- Customize the station layout with a map editor

## Recent Major Changes

### What Was Removed (Latest Session)
1. **Memory Palace System** - Entire system for placing/editing text plaques removed
   - Removed files: `src/memory.c`, `src/memory.h`
   - Removed 'P' and 'p' map tiles
   - Removed all save/load memory functionality

2. **NPC System** - Dogs, ghosts, and dialogue removed
   - Removed files: `src/npc.c`, `src/npc.h`
   - Removed 'G', 'g', 'H', 'h' map tiles
   - Removed NPC AI and pathfinding

3. **Furniture System** - Beds, sofas, tables removed
   - Removed files: `src/furniture.c`, `src/furniture.h`
   - Removed 'B', 'b', 'S', 'T', 't', 'R', 'W', 'w' map tiles
   - Removed furniture collision and rendering

4. **Display Terminal Text Rendering** - Temporarily disabled
   - Displays now show as solid dark blue/teal instead of live terminal content
   - This was causing "instrucao ilegal" (illegal instruction) crashes
   - Code is still in src/renderer.c but commented out/disabled

### What Remains
- Server cabinet system ('C' tiles) - **WORKING**
- Wall display system ('D', 'd' tiles) - Interaction works, text rendering disabled
- Terminal emulation with PTY - **WORKING** (use F1 to exit, not ESC)
- Raycasting engine - **WORKING**
- Map system with doors - **WORKING**
- Map editor - **WORKING** (basic)
- Sky rendering - **WORKING**
- Cabinet 3D box rendering with 4 texture variations - **WORKING**

## Key Technical Details

### Terminal System
- **Location**: `src/terminal.c/h`
- Uses `forkpty()` to spawn real shell processes
- ANSI/VT100 escape sequence parser with state machine
- Each terminal has its own PTY file descriptor
- **Important**: F1 exits terminal mode (not ESC, so vim works)
- Ctrl+key combinations fully supported

### Display System Issues
The wall-mounted displays were causing crashes when:
1. User activates display with 'E'
2. Display opens terminal in fullscreen
3. User exits with F1
4. Crash occurs when re-rendering the scene

**Attempted Fixes** (all failed):
- Frame skip mechanism (3 frames after exit)
- Only updating active terminal vs all terminals
- Adding null/bounds checking
- Verifying pty_fd status

**Current Solution**:
Disabled text rendering entirely in `src/renderer.c` around line 867. Displays now just show solid color.

**For Future Sessions**:
The root cause might be:
- Terminal cells array access during rendering
- Race condition between terminal update and rendering
- PTY buffer state after exit
Consider alternative approaches like double-buffering terminal cells or copying data before rendering.

### File Organization

**Core Files:**
- `src/main.c` - Event loop, SDL init, input handling
- `src/game.c` - Game state, initialization, map loading
- `src/player.c` - Movement, collision detection
- `src/renderer.c` - Raycasting, wall/floor/ceiling rendering
- `src/terminal.c` - PTY management, ANSI parsing
- `src/cabinet.c` - Server cabinet management
- `src/display.c` - Wall display management
- `src/types.h` - All structs and constants

**Support Files:**
- `src/texture.c` - Procedural texture generation
- `src/ui.c` - HUD, minimap, text rendering
- `src/map.c` - Map loading/saving/generation
- `src/utils.c` - Helper functions

### Build System
- **Makefile** - Builds `tty-space-station` binary and `mapeditor`
- **Environment Variables**:
  - `TSS_MAP_FILE` - Load custom map
  - `TSS_GENERATED_MAP` - Export generated map

## Common Patterns

### Adding New Interactive Objects
1. Add entry to types.h (e.g., `FooEntry` struct with position, terminal_index)
2. Create `src/foo.c` and `src/foo.h`
3. Add tile character to map.c parsing
4. Add rendering in renderer.c (either sprite or wall-based)
5. Add interaction handling in main.c input section
6. Add to minimap rendering in ui.c

### Debugging Terminal Issues
- Enable `DEBUG_MODE` in types.h for verbose logging
- Check PTY file descriptor with `game.terminals[idx].pty_fd`
- Verify terminal active status with `game.terminals[idx].active`
- Monitor ANSI parser state with `term->parse_state`

### Map Format
Text-based, one character per tile:
```
1111111
1.....1
1..C..1  (C = cabinet, . = floor)
1.....1
1111D11  (D = door)
```

## Known Bugs & Issues

### Critical
1. **Display terminal text crashes on exit** - Currently disabled
   - Symptom: "instrucao ilegal" when pressing F1 in display terminal
   - Location: Somewhere in display rendering or terminal update loop
   - Status: Text rendering disabled as workaround

### Medium
2. **Terminal `clear` command doesn't work properly**
   - Likely ANSI escape sequence parsing issue
   - Check `terminal_handle_csi()` in terminal.c

3. **Some cursor positioning apps behave strangely**
   - May need better CSI parameter parsing
   - Test with: vim, htop, top, less

### Minor
4. **Sky texture stretches vertically**
   - Partially fixed by using only 60% of texture height
   - Could use better perspective correction

5. **Map editor is very basic**
   - No undo/redo
   - No copy/paste
   - Limited palette

## Testing Checklist

When making changes, test:
- [ ] Server cabinets activate properly (U key)
- [ ] Terminal input works (typing, Enter, Backspace)
- [ ] F1 exits terminal without crashing
- [ ] Ctrl+C works in terminal
- [ ] vim can be opened and exited
- [ ] Displays activate (E key) - even if showing blank screen
- [ ] Doors toggle (F key)
- [ ] Map loading works
- [ ] Custom map loads: `TSS_MAP_FILE=maps/palace.map ./tty-space-station`
- [ ] No segfaults during normal movement

## Performance Notes

- Terminal updates happen every frame for active terminal only
- Background terminals (displays) currently don't update to avoid crashes
- Raycasting is single-threaded, no optimizations yet
- Texture lookups are array-based (fast)
- No texture caching beyond initial load

## Code Style

- C11 standard
- Indent with spaces (likely 4 spaces, check existing code)
- Function names: lowercase_with_underscores
- Struct names: PascalCase
- Constants: UPPERCASE_WITH_UNDERSCORES
- Always check bounds before array access
- Always null-check pointers from lookups

## Future Development Ideas

See `TODO.md` for prioritized list, but some possibilities:
- Fix display text rendering (top priority)
- Improve terminal performance (scrollback, buffering)
- Add sound effects
- Networking/multiplayer (removed in past, could re-add)
- More map editor features
- Scripting support for interactive terminals
- Quest/goal system using terminal commands

## Useful Commands

```bash
# Build and run
make && ./tty-space-station

# Run with custom map
TSS_MAP_FILE=maps/palace.map make run

# Debug with GDB
make clean && make CFLAGS="-g -O0" && gdb ./tty-space-station

# Check for memory leaks
valgrind --leak-check=full ./tty-space-station

# Edit default map
./mapeditor maps/palace.map

# Find terminal-related code
grep -r "terminal" src/
```

## Contact & History

This project has been developed across multiple Claude Code sessions. Each session builds on previous work:

### Session History (Reverse Chronological)
1. **Latest** - Disabled display text, removed memory/NPC/furniture systems, renamed to tty-space-station
2. Previous - Added display wall terminals, improved terminal input (F1 exit, Ctrl keys)
3. Earlier - Added sky texture, 3D cabinet boxes, display stacking
4. Earlier - Reorganized monolithic poom.c into modular architecture
5. Original - Memory palace concept with text plaques

---

**For Future Claude Sessions**: Read this document first! It will save you time understanding what's already been done and what the current issues are. Good luck! 🚀
