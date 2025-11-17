# TTY Space Station - TODO List

Priority-ordered list of improvements and features for future development.

## Critical Issues (Fix ASAP)

### 1. Fix Display Terminal Text Rendering
**Priority**: 🔴 CRITICAL
**Status**: Currently disabled due to crashes

**Problem**: When activating a wall display terminal and then exiting with F1, the game crashes with "instrucao ilegal" (illegal instruction).

**What's been tried**:
- Frame skip mechanism (3 frames after exit)
- Only updating active terminal vs all display terminals
- Extensive null/bounds checking
- Verifying PTY file descriptor status
- Clamping array indices

**Next steps to try**:
- [ ] Double-buffer terminal cell data before rendering
- [ ] Copy terminal cells to separate render buffer
- [ ] Use mutex/locks for terminal cell access
- [ ] Render displays only when cells haven't changed
- [ ] Investigate if issue is in ANSI parser or rendering
- [ ] Add extensive logging around crash point
- [ ] Test with simpler terminal (just 'echo' commands, no vim)

**Location**: `src/renderer.c` around line 867 (currently disabled)

### 2. Fix Terminal `clear` Command
**Priority**: 🟠 HIGH
**Status**: ✅ FIXED

**Problem**: The `clear` command doesn't properly clear the screen. Some terminal apps don't work correctly.

**Solution**: Implemented all three modes of CSI J (Erase in Display):
- [x] Mode 0: Clear from cursor to end of screen (default for many apps)
- [x] Mode 1: Clear from cursor to beginning of screen
- [x] Mode 2: Clear entire screen
- [x] Added ESC c (Reset to Initial State) support
- [x] Added CSI ?25h/l (cursor visibility control)
- [x] Added CSI s/u (save/restore cursor position)

**Location**: `src/terminal.c` - ANSI parser state machine

## High Priority Features

### 3. Improve Terminal Performance
**Priority**: 🟠 HIGH
**Why**: Terminals can lag with heavy output (e.g., `cat large_file.txt`)

**Ideas**:
- [ ] Add scrollback buffer (currently loses history)
- [ ] Throttle rendering (only update display 30fps, not 60fps)
- [ ] Buffer PTY reads (larger chunks)
- [ ] Optimize ANSI parser (reduce string operations)
- [ ] Profile with `perf` or `gprof`
- [ ] Consider using VT100 library instead of custom parser

**Files**: `src/terminal.c`, `src/renderer.c`

### 4. Enhance Map Editor
**Priority**: 🟡 MEDIUM
**Status**: Basic but functional

**Missing features**:
- [ ] Undo/redo functionality
- [ ] Copy/paste regions
- [ ] Fill tool (flood fill)
- [ ] Grid size adjustment
- [ ] Tile rotation for cabinets/displays
- [ ] Preview mode (3D view of section)
- [ ] Snap to grid toggle
- [ ] Multi-select
- [ ] Keyboard shortcuts (Ctrl+S, Ctrl+Z, etc.)
- [ ] Layer system (walls, objects, special)
- [ ] Tile search/filter

**Files**: `tools/mapeditor.c`, `tools/README.md`

## Medium Priority Improvements

### 5. Better Cabinet/Display Highlighting
**Priority**: 🟡 MEDIUM
**Status**: ✅ IMPROVED

**Completed**:
- [x] Show "Press U to activate" hints when facing cabinets
- [x] Show "Press E to use" hints when facing displays
- [x] Show "Press F to open/close door" hints when facing doors
- [x] Fixed missing F key binding for door toggle

**Future improvements**:
- [ ] Different colors for active vs inactive terminals
- [ ] Show terminal status (running command, idle, etc.)
- [ ] Glow effect for interactive objects
- [ ] Visual highlighting of objects in 3D view

**Files**: `src/main.c`, `src/renderer.c`, `src/ui.c`

### 6. Add Sound Effects
**Priority**: 🟡 MEDIUM
**Status**: No audio at all

**Sounds needed**:
- [ ] Footsteps (different for floor types)
- [ ] Door open/close
- [ ] Cabinet activation
- [ ] Terminal keyboard sounds (optional, could be annoying)
- [ ] Ambient space station sounds
- [ ] Terminal beep (`\a` bell character)

**Libraries**: SDL_mixer or custom WAV playback

**Files**: New `src/audio.c`, `src/audio.h`

### 7. Optimize Rendering Performance
**Priority**: 🟡 MEDIUM
**Why**: Game runs well but could be better on low-end hardware

**Optimizations**:
- [ ] Multithreaded raycasting (split screen into columns)
- [ ] Dirty rectangle tracking (only redraw changed areas)
- [ ] Texture atlas (reduce cache misses)
- [ ] LOD system for distant walls
- [ ] Frustum culling for cabinets
- [ ] Profile with `gprof` or `perf`
- [ ] Consider GPU rendering (OpenGL/Vulkan) - major change

**Files**: `src/renderer.c`, Makefile

### 8. Improve Terminal Display Size
**Priority**: 🟡 MEDIUM
**Status**: Fixed at 80x24

**Ideas**:
- [ ] Allow terminals to use more screen space
- [ ] Dynamic terminal size based on screen resolution
- [ ] Zoom in/out for terminals
- [ ] Different terminal sizes for cabinets vs displays
- [ ] Font size options

**Files**: `src/types.h` (TERM_COLS/TERM_ROWS), `src/renderer.c`

## Low Priority / Nice to Have

### 9. Add More Terminal Features
**Priority**: 🟢 LOW
**Features**:
- [ ] Copy/paste support (Ctrl+Shift+C/V)
- [ ] Mouse support in terminal (for apps like `mc`)
- [ ] Unicode/UTF-8 support (currently ASCII only)
- [ ] 256-color support (currently 16 colors)
- [ ] True color support
- [ ] Bold/italic/underline rendering
- [ ] Scrollback with PageUp/PageDown
- [ ] Search in terminal output

**Files**: `src/terminal.c`, `src/renderer.c`

### 10. Advanced Map Features
**Priority**: 🟢 LOW
**Ideas**:
- [ ] Elevators/stairs (multi-level maps)
- [ ] Moving platforms
- [ ] Scripted events (triggers)
- [ ] Locked doors (requires key/password)
- [ ] Secret areas
- [ ] Teleporters
- [ ] Colored lighting
- [ ] Animated textures
- [ ] Weather effects in "outdoor" areas

**Files**: `src/map.c`, `src/game.c`, `src/renderer.c`

### 11. Networking / Multiplayer
**Priority**: 🟢 LOW
**Status**: Was removed in previous session

**Notes**: Original POOM had basic networking. Could re-implement with:
- [ ] Shared terminal sessions (multiple players in same terminal)
- [ ] Chat system using terminals
- [ ] Shared map exploration
- [ ] Player avatars visible to each other
- [ ] Collaborative coding environment

**Complexity**: HIGH - requires significant work

**Files**: Would need new `src/network.c`

### 12. Configuration System
**Priority**: 🟢 LOW
**Status**: Everything is hardcoded

**Config file ideas** (`~/.config/tty-space-station/config.ini`):
- [ ] Key bindings
- [ ] Graphics settings (resolution, FOV, render distance)
- [ ] Audio settings
- [ ] Terminal settings (font, colors, size)
- [ ] Performance options (multithreading, vsync)
- [ ] Default map path

**Files**: New `src/config.c`, `src/config.h`

### 13. Better Error Handling
**Priority**: 🟢 LOW
**Current**: Many errors just printf to stderr

**Improvements**:
- [ ] In-game error messages (not just console)
- [ ] Graceful degradation (missing textures, etc.)
- [ ] Better PTY error handling
- [ ] Log file for debugging
- [ ] Crash dump/backtrace

**Files**: All files, especially `src/main.c`

### 14. Documentation
**Priority**: 🟢 LOW
**Status**: README is good, code comments are sparse

**Needed**:
- [ ] Code documentation (Doxygen style comments)
- [ ] Architecture diagram
- [ ] Tutorial for new users
- [ ] Video demo/trailer
- [ ] Development guide (beyond CLAUDE.md)
- [ ] Performance profiling results
- [ ] Memory usage analysis

**Files**: All source files, new docs/ directory

## Ideas for Future (No Priority)

- [ ] VR support (very complex!)
- [ ] Mobile port (touch controls?)
- [ ] Web version (Emscripten compile)
- [ ] Mac app bundle
- [ ] Flatpak/Snap packages
- [ ] Save game system (remember terminal sessions)
- [ ] Achievement system (via terminal commands)
- [ ] Easter eggs
- [ ] Modding support (Lua scripting?)
- [ ] AI assistant in-game (chatbot in terminal?)
- [ ] Integration with actual SSH (connect to remote servers)

## Recently Completed ✅

### Latest Session (Terminal Improvements & UX Enhancements)
- [x] **Fix terminal `clear` command** - Implemented all CSI J modes (0, 1, 2)
- [x] **Add ESC c support** - Terminal reset functionality
- [x] **Add cursor visibility control** - CSI ?25h/l sequences
- [x] **Add save/restore cursor** - CSI s/u commands for better vim/app support
- [x] **Add interaction hints** - Show "Press U/E/F" when facing objects
- [x] **Fix missing F key binding** - Door toggle now works properly

### Previous Sessions
- [x] Disable display text rendering (temporary fix for crashes)
- [x] Remove memory palace system
- [x] Remove NPC system (dogs, ghosts)
- [x] Remove furniture system (beds, sofas, tables)
- [x] Rename project from "poom" to "tty-space-station"
- [x] Update README with alpha warning
- [x] Fix terminal font rendering (proper scaling)
- [x] Add Ctrl+key support for terminals
- [x] Change terminal exit from ESC to F1
- [x] Add display stacking (DDD creates larger displays)
- [x] Add sky texture rendering
- [x] Change cabinets from sprites to 3D boxes
- [x] Add 4 cabinet texture variations
- [x] Increase terminal screen size (50% bigger)
- [x] Reorganize monolithic poom.c into modules

---

## How to Contribute

If you're a future Claude session or external contributor:

1. Check CLAUDE.md for project context
2. Pick an item from this TODO
3. Create a branch (if using git)
4. Make changes and test thoroughly (see Testing Checklist in CLAUDE.md)
5. Update this TODO with progress
6. Commit with clear message

**For Critical Issues**: Fix these first! Display text rendering is the biggest problem right now.

**For New Features**: Make sure they don't break existing functionality. Test terminals especially carefully.

**Current Focus**: Fix display text rendering, then improve map editor.
