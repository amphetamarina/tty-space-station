# tty-space-station Map Editor

A graphical point-and-click map editor for creating tty-space-station memory palace maps.

## Building

```bash
make mapeditor
# or
make all  # builds both tty-space-station and mapeditor
```

## Usage

```bash
# Edit an existing map
./mapeditor maps/palace.map

# Create a new map with custom dimensions
./mapeditor maps/newmap.map 60 40

# Quick launch with make
make editor
```

## Controls

### Mouse
- **Left Click**: Place selected tile on the map
- **Right Click + Drag**: Pan the view
- **Mouse Wheel**: Scroll vertically
- **Click Palette**: Select tile type from the palette on the right

### Keyboard
- **G**: Toggle grid display
- **H**: Toggle help overlay
- **Ctrl+S**: Save map to file
- **ESC**: Quit (prompts to save)

## Features

### Tile Palette

The editor includes all tty-space-station tile types:

**Floors:**
- `.` - Floor 1 (stone checker)
- `,` - Floor 2 (noise)
- `;` - Floor 3 (marble)

**Walls:**
- `1` - Wall 1 (checkered)
- `2` - Wall 2 (striped)
- `3` - Wall 3 (brick)
- `4` - Window wall (semi-transparent)
- `#` - Solid wall

**Interactive:**
- `D` - Door (toggleable with F key in game)
- `X` - Player spawn point

**Furniture:**
- `T` - Square table
- `R` - Round table
- `B` - Bed
- `S` - Sofa
- `W` - Wardrobe

**NPCs:**
- `P` - Puppy (friendly wandering NPC)
- `G` - Ghost (mysterious translucent NPC)

### Variable Map Sizes

The editor supports maps from 10x10 to 100x100 tiles. Simply specify dimensions when creating a new map, or load any existing map regardless of size.

### Visual Tile Colors

Each tile type is displayed with its approximate in-game color:
- Walls appear in their textured colors
- Floors show their surface appearance
- Special tiles (spawn, doors, NPCs) have distinctive colors
- Furniture uses its primary color

### Grid Overlay

Toggle the grid (G key) to see tile boundaries clearly, making precise editing easier.

## Tips

1. **Start with borders**: The editor automatically creates wall borders for new maps
2. **Place spawn first**: Use `X` to mark where players start
3. **Mix floor types**: Use `.`, `,`, and `;` for visual variety
4. **Test in tty-space-station**: Run `./tty-space-station` with `tty-space-station_MAP_FILE=maps/your_map.map`
5. **Save often**: Use Ctrl+S to save your work
6. **NPCs need space**: Place NPCs (`P`, `G`) in open floor areas so they can wander

## Example Workflow

```bash
# Create a new 60x40 map
./mapeditor maps/mypalace.map 60 40

# 1. The editor creates a bordered map with spawn point
# 2. Click palette to select wall type (e.g., '2' for striped walls)
# 3. Click and drag to draw rooms
# 4. Select doors ('D') and place them in walls
# 5. Select floors ('.', ',', ';') and fill in floor patterns
# 6. Add furniture ('T', 'B', 'S', etc.) for decoration
# 7. Place NPCs ('P', 'G') in open areas
# 8. Press Ctrl+S to save
# 9. Test in game:
tty-space-station_MAP_FILE=maps/mypalace.map ./tty-space-station
```

## Technical Details

- Maps are saved as plain text ASCII files
- Each line represents one row of tiles
- Compatible with all tty-space-station features
- Supports the full tile set including NPCs
- Dynamic memory allocation for any map size
- Efficient rendering with viewport culling

## Troubleshooting

**Editor won't start:**
- Ensure SDL2 is installed: `sudo apt-get install libsdl2-dev`
- Check that the map file path is valid

**Can't save map:**
- Verify write permissions in the maps/ directory
- Make sure the filename ends with `.map`

**Map not loading in tty-space-station:**
- Ensure map has at least one spawn point (`X`)
- Check that borders are properly walled
- Verify file path is correct

## Future Enhancements

Planned features:
- Texture preview (show actual game textures if available)
- Copy/paste regions
- Undo/redo
- Fill tool for large areas
- Room templates
- Zoom in/out
- Mini-preview of full map
