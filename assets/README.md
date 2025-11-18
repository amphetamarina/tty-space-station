# Assets

Drop optional BMP textures into this directory to override tty-space-station’s procedural
materials. Supported filenames (64×64 recommended):

- `wall0.bmp`, `wall1.bmp`, `wall2.bmp`
- `wall3.bmp` (window walls)
- `floor0.bmp`, `floor1.bmp`, `floor2.bmp`
- `ceiling0.bmp`, `ceiling1.bmp`
- `door.bmp`
- `table_square.bmp`, `table_round.bmp`
- `bed.bmp`, `sofa.bmp`, `wardrobe.bmp`

Images are automatically scaled to the internal resolution. If a file is
missing, tty-space-station falls back to the built-in generator.

Map characters map as follows:

- Floor `.` → `floor0`, `,` → `floor1`, `;` → `floor2`
- Wall `1/2/3` → `wall0/1/2`, window wall `4` → `wall3`
- Door `D` → `door.bmp`
- Furniture: `T/t` → `table_square`, `R` → `table_round`, `B` → `bed`,
  `S` → `sofa`, `W` → `wardrobe`

### Finding free textures

- [Kenney Assets](https://www.kenney.nl/assets) – permissively licensed packs
  with walls, floors, and UI icons.
- [OpenGameArt.org](https://opengameart.org/) – curated community textures and
  sprites; filter by CC0/CC-BY or GPL as needed.
- [Freedoom](https://freedoom.github.io/) – open-source Doom-compatible art
  useful for retro wall/floor sets.

Convert PNG/JPG assets to BMP (e.g., `convert file.png file.bmp`) before placing
them here so SDL can load them without extra libraries.
