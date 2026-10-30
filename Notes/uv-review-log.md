# UV review log

Every level face fixed during the face-by-face UV review (Edward with the Face Index Display, 2026), so that
ALTViewer's fixer, the Patch/LxxxLEV.txt lists and the README can be brought up to date in one pass once the
review is done.

Status of each part:

- `ALTEngine/data/Patches.json`: **up to date** - every row below is in it.
- `Patch/LxxxLEV.txt` and the README: only the zip 51 rows so far (L371LEV Fix 8).
- ALTViewer's fixer: **none yet**.

Patches.json is applied when the game is installed (InstallPipeline's RunPatches), so a fix only shows up in
game after a reinstall or after the patch is applied to the installed files.

## How the fixes work

A face record is 20 bytes at the offset shown: four vertex indices (d = -1 for a triangle), the texture index,
the draw routine at +0x12, and the light id at +0x13.

- A triangle (a, b, c) always takes the texture's x0y1, x1y0 and x0y0 corners, so it shows the half of its tile
  cut along the x0y1-x1y0 diagonal, with the corner of the tile on c.
- **Turned:** rotate the vertex order. The face keeps its winding and routine.
- **Mirrored triangle:** reverse the vertex order and move routine 1 to routine 5, the same triangle draw
  without the back-face test (0x1ef20), so the reversed face still shows.
- **Unpinched:** a triangle whose right angle is not on c shears its texture; turning it so the right angle is on c.
- **Facing turned:** a quad reversed to (b, a, d, c) with routine 0 <-> 2 shows from the other side with every corner keeping its texture corner.
- **Spare record reused:** a missing face takes over a record that drew nothing new (an exact duplicate), since the file cannot grow.
- **Texture changed:** the 16-bit texture index at +0x10 set to the right one.
- **Mirrored, other half:** a triangle cannot show the half of the tile cut along x0y0-x1y1. These become a
  quad (a, b, c, c) with routine 2 (mirrored quad, corners x1y1, x0y1, x0y0, x1y0): the triangle (a, b, c) is the
  face, and (a, c, d) has no area. The port draws quads as (a, b, c) + (a, c, d), so this is exact there. No
  shipped level uses a quad like this, and how the original rasterises one is not traced, so in DOS it is
  unverified.

## L111LEV and L900LEV

The two share this face at the same offset; the patch entries for both are updated in place (they already wrote it).

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 9618 | 0x46DB4 | round 4 | turned once more counter-clockwise; also texture 0x38 at +0x10 (earlier fix) | 8049, 8113, 8052, -1 (original 8052, 8049, 8113) | 1 | 8113, 8052, 8049, -1 | 1 |

L111LEV: Edward reports the level complete after this fix.

## L131LEV

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 6754 | 0x3BFE4 | round 5 | texture 154 -> 183, matching the pillar | 8431, 8425, 8724, 9112 | 0 | 8431, 8425, 8724, 9112 | 0 |
| 6752 | 0x3BFBC | round 5 | turned one step, (b, c, d, a), to match 7497 | 8431, 9112, 9164, 8432 | 0 | 9112, 9164, 8432, 8431 | 0 |

## L141LEV and L903LEV

The same faces at the same offsets in both; one row each.

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 2735 | 0x28B70 | round 5 | texture 223 -> 217, the crate's own | 2248, 2538, 2551, 2249 | 0 | 2248, 2538, 2551, 2249 | 0 |
| 2736 | 0x28B84 | round 5 | flipped horizontally: the texture lies on its side, so (c, d, a, b) with routine 0 -> 2 (mirrored) | 2552, 2560, 2530, 2285 | 0 | 2530, 2285, 2552, 2560 | 2 |
| 2196 | 0x26154 | round 5 | texture 0 -> 225 | 2556, 2562, 10307, 10306 | 0 | 2556, 2562, 10307, 10306 | 0 |

## L161LEV

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 3758 | 0x29E1C | round 5 | was an exact duplicate of 3757; given over to the missing face between faces 0, 29, 36 and 40 - texture 1 -> 109, routine 1 -> 0, light 0x08 -> 0x0D (13, as its neighbours; no end-of-run bit either way) | 8645, 8646, 8655, -1 | 1 | 4178, 4177, 4148, 4149 | 0 |

A face cannot be added without moving every byte after it, so a spare record is reused. L161 has three: exact duplicates 3758, 3760 and 3797. In the port the new face is exact. The original draws faces in per-cell runs (bit 0x80 of the light byte ends one), and 3758's run belongs to another part of the level, so in DOS the face may only show when that part is drawn - untraced.

## L371LEV

**Round 4 needs revisiting** - Edward reports a number of its fixes are wrong; he will come back to L371 after the other levels.

Recesses: each wall recess frames a band (texture 35) with four corner triangles (texture 35, or 27 in some
recesses). The target is a symmetric frame: every corner shows the band mirrored across the seam it shares with
the band.

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 1456 | 0x1A724 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8581, 8842, 8582, -1 | 1 | 8842, 8582, 8581, -1 | 1 |
| 1457 | 0x1A738 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8843, 8582, 8842, -1 | 1 | 8582, 8842, 8843, -1 | 1 |
| 1458 | 0x1A74C | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8846, 8847, 8581, -1 | 1 | 8847, 8581, 8846, -1 | 1 |
| 1459 | 0x1A760 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8579, 8850, 8580, -1 | 1 | 8850, 8580, 8579, -1 | 1 |
| 1460 | 0x1A774 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8849, 8580, 8850, -1 | 1 | 8580, 8850, 8849, -1 | 1 |
| 1461 | 0x1A788 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8578, 8584, 8579, -1 | 1 | 8584, 8579, 8578, -1 | 1 |
| 1636 | 0x1B534 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8842, 8581, 8847, -1 | 1 | 8581, 8847, 8842, -1 | 1 |
| 1637 | 0x1B548 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8850, 8579, 8584, -1 | 1 | 8579, 8584, 8850, -1 | 1 |
| 2077 | 0x1D7A8 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 7584, 7583, 7582, -1 | 1 | 7583, 7582, 7584, -1 | 1 |
| 2078 | 0x1D7BC | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8262, 7583, 7585, -1 | 1 | 7583, 7585, 8262, -1 | 1 |
| 2241 | 0x1E478 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 7581, 7583, 7601, -1 | 1 | 7583, 7601, 7581, -1 | 1 |
| 2244 | 0x1E4B4 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8261, 7583, 8263, -1 | 1 | 7583, 8263, 8261, -1 | 1 |
| 2580 | 0x1FEF4 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8267, 8266, 8265, -1 | 1 | 8266, 8265, 8267, -1 | 1 |
| 2581 | 0x1FF08 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 8264, 8265, 8266, -1 | 1 | 8266, 8265, 8264, -1 | 5 |
| 2582 | 0x1FF1C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 8270, 8271, 8272, -1 | 1 | 8272, 8271, 8270, -1 | 5 |
| 2583 | 0x1FF30 | round 4 | right angle on a: turned to (b, c, a) so it is on c, where the texture's own right angle is | 8273, 8272, 8271, -1 | 1 | 8272, 8271, 8273, -1 | 1 |
| 4513 | 0x295F8 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4384, 4383, 4398, -1 | 1 | 4383, 4398, 4384, 4384 | 2 |
| 4516 | 0x29634 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4402, 4390, 4389, -1 | 1 | 4389, 4390, 4402, -1 | 5 |
| 4521 | 0x29698 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4397, 4382, 4381, -1 | 1 | 4381, 4382, 4397, -1 | 5 |
| 4525 | 0x296E8 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4392, 4391, 4401, -1 | 1 | 4391, 4401, 4392, 4392 | 2 |
| 4902 | 0x2B45C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2690, 2675, 2674, -1 | 1 | 2674, 2675, 2690, -1 | 5 |
| 4905 | 0x2B498 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2685, 2684, 2694, -1 | 1 | 2684, 2694, 2685, 2685 | 2 |
| 4946 | 0x2B7CC | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2708, 2707, 2722, -1 | 1 | 2707, 2722, 2708, 2708 | 2 |
| 4949 | 0x2B808 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2726, 2714, 2713, -1 | 1 | 2713, 2714, 2726, -1 | 5 |
| 5002 | 0x2BC2C | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4480, 4479, 4492, -1 | 1 | 4479, 4492, 4480, 4480 | 2 |
| 5005 | 0x2BC68 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4496, 4486, 4485, -1 | 1 | 4485, 4486, 4496, -1 | 5 |
| 5010 | 0x2BCCC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4491, 4478, 4477, -1 | 1 | 4477, 4478, 4491, -1 | 5 |
| 5014 | 0x2BD1C | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4488, 4487, 4495, -1 | 1 | 4487, 4495, 4488, 4488 | 2 |
| 5175 | 0x2C9B0 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2677, 2676, 2691, -1 | 1 | 2676, 2691, 2677, 2677 | 2 |
| 5178 | 0x2C9EC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2695, 2683, 2682, -1 | 1 | 2682, 2683, 2695, -1 | 5 |
| 5219 | 0x2CD20 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2721, 2706, 2705, -1 | 1 | 2705, 2706, 2721, -1 | 5 |
| 5222 | 0x2CD5C | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2716, 2715, 2725, -1 | 1 | 2715, 2725, 2716, 2716 | 2 |
| 5303 | 0x2D3B0 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5699, 5698, 5711, -1 | 1 | 5698, 5711, 5699, 5699 | 2 |
| 5306 | 0x2D3EC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5715, 5704, 5705, -1 | 1 | 5705, 5704, 5715, -1 | 5 |
| 5317 | 0x2D4C8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5710, 5696, 5697, -1 | 1 | 5697, 5696, 5710, -1 | 5 |
| 5320 | 0x2D504 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5707, 5706, 5714, -1 | 1 | 5706, 5714, 5707, 5707 | 2 |
| 5628 | 0x2ED14 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 5807, 4408, 4418, -1 | 1 | 4408, 4418, 5807, 5807 | 2 |
| 5628 | 0x2ED14 | round 2 | texture 27 -> 35 (+0x10), the band's own | | | | |
| 5631 | 0x2ED50 | round 1 | TL/BR corner, mirrored (reversed to c, b, a) | 4421, 4412, 5809, -1 | 1 | 5809, 4412, 4421, -1 | 5 |
| 5636 | 0x2EDB4 | round 1 | TL/BR corner, mirrored (reversed to c, b, a) | 4417, 4407, 4406, -1 | 1 | 4406, 4407, 4417, -1 | 5 |
| 5640 | 0x2EE04 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4414, 4413, 4420, -1 | 1 | 4413, 4420, 4414, 4414 | 2 |
| 5640 | 0x2EE04 | round 3 | texture 27 -> 35 (+0x10), the band's own | | | | |
| 5792 | 0x2F9E4 | round 4 | texture 86 -> 94 | 5955, 5954, 5953, -1 | 1 | 5955, 5954, 5953, -1 | 1 |
| 5794 | 0x2FA0C | round 4 | texture 86 -> 94 | 5955, 5953, 5958, -1 | 1 | 5955, 5953, 5958, -1 | 1 |
| 5890 | 0x3018C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5756, 5742, 5743, -1 | 1 | 5743, 5742, 5756, -1 | 5 |
| 5893 | 0x301C8 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5753, 5752, 5760, -1 | 1 | 5752, 5760, 5753, 5753 | 2 |
| 6062 | 0x30EFC | round 4 | circle panel's bottom left corner: the inner half of tile 17, a quad with its first corner repeated, routine 6 (reversed, so double-sided) | 2788, 2779, 2778, -1 | 1 | 2778, 2778, 2779, 2788 | 6 |
| 6064 | 0x30F24 | round 4 | circle panel's top left corner: texture 17 -> 1 (the circle's top left) and the inner half of it, a quad with its last corner repeated, routine 0 | 2790, 2784, 2783, -1 | 1 | 2783, 2790, 2784, 2784 | 0 |
| 6069 | 0x30F88 | round 4 | circle panel's top middle (texture 2) was drawn mirrored, routine 2 -> 0 like the rest of the panel | 2790, 2789, 2785, 2784 | 2 | 2790, 2789, 2785, 2784 | 0 |
| 6075 | 0x31000 | round 4 | circle panel's top right corner: texture 19 -> 3 (the circle's top right) and the inner half of it, a quad with its last corner repeated, routine 6 (reversed, so double-sided) | 2786, 2785, 2789, -1 | 1 | 2786, 2789, 2785, 2785 | 6 |
| 6288 | 0x320A4 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5745, 5744, 5757, -1 | 1 | 5744, 5757, 5745, 5745 | 2 |
| 6291 | 0x320E0 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5761, 5750, 5751, -1 | 1 | 5751, 5750, 5761, -1 | 5 |
| 6453 | 0x32D88 | round 4 | texture 86 -> 94 | 5952, 5855, 5870, -1 | 1 | 5952, 5855, 5870, -1 | 1 |
| 6455 | 0x32DB0 | round 4 | texture 86 -> 94 | 5952, 5920, 5855, -1 | 1 | 5952, 5920, 5855, -1 | 1 |
| 6812 | 0x34994 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5820, 5813, 5814, -1 | 1 | 5814, 5813, 5820, -1 | 5 |
| 6815 | 0x349D0 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 5817, 5818, 5821, -1 | 1 | 5818, 5821, 5817, 5817 | 2 |
| 6815 | 0x349D0 | round 3 | texture 27 -> 35 (+0x10), the band's own | | | | |
| 6820 | 0x34A34 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4425, 4426, 4433, -1 | 1 | 4426, 4433, 4425, 4425 | 2 |
| 6820 | 0x34A34 | round 2 | texture 27 -> 35 (+0x10), the band's own | | | | |
| 6824 | 0x34A84 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4436, 4429, 4430, -1 | 1 | 4430, 4429, 4436, -1 | 5 |
| 7382 | 0x3761C | round 4 | faced away from the room: reversed to (b, a, d, c) and routine 0 -> 2, so every corner keeps its texture corner | 3654, 3541, 3619, 3645 | 0 | 3541, 3654, 3645, 3619 | 2 |
| 7578 | 0x3856C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4329, 4315, 4314, -1 | 1 | 4314, 4315, 4329, -1 | 5 |
| 7581 | 0x385A8 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4326, 4325, 4333, -1 | 1 | 4325, 4333, 4326, 4326 | 2 |
| 7587 | 0x38620 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4317, 4316, 4330, -1 | 1 | 4316, 4330, 4317, 4317 | 2 |
| 7591 | 0x38670 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4334, 4324, 4323, -1 | 1 | 4323, 4324, 4334, -1 | 5 |
| 7600 | 0x38724 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4363, 4346, 4345, -1 | 1 | 4345, 4346, 4363, -1 | 5 |
| 7603 | 0x38760 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4359, 4358, 4367, -1 | 1 | 4358, 4367, 4359, 4359 | 2 |
| 7609 | 0x387D8 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4348, 4347, 4364, -1 | 1 | 4347, 4364, 4348, 4348 | 2 |
| 7613 | 0x38828 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4368, 4357, 4356, -1 | 1 | 4356, 4357, 4368, -1 | 5 |
| 9270 | 0x4099C | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4454, 4455, 4470, -1 | 1 | 4455, 4470, 4454, 4454 | 2 |
| 9273 | 0x409D8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4474, 4464, 4465, -1 | 1 | 4465, 4464, 4474, -1 | 5 |
| 9532 | 0x41E14 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4471, 4456, 4457, -1 | 1 | 4457, 4456, 4471, -1 | 5 |
| 9535 | 0x41E50 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4462, 4463, 4475, -1 | 1 | 4463, 4475, 4462, 4462 | 2 |
| 9607 | 0x423F0 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5719, 5718, 5731, -1 | 1 | 5718, 5731, 5719, 5719 | 2 |
| 9610 | 0x4242C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5735, 5724, 5725, -1 | 1 | 5725, 5724, 5735, -1 | 5 |
| 9873 | 0x438B8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5730, 5716, 5717, -1 | 1 | 5717, 5716, 5730, -1 | 5 |
| 9876 | 0x438F4 | round 3 | TR/BL corner, mirrored (quad, last corner repeated) | 5727, 5726, 5734, -1 | 1 | 5726, 5734, 5727, 5727 | 2 |
| 9881 | 0x43958 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2768, 2754, 2755, -1 | 1 | 2755, 2754, 2768, -1 | 5 |
| 9884 | 0x43994 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2760, 2761, 2772, -1 | 1 | 2761, 2772, 2760, 2760 | 2 |
| 9889 | 0x439F8 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 2752, 2753, 2767, -1 | 1 | 2753, 2767, 2752, 2752 | 2 |
| 9893 | 0x43A48 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2771, 2762, 2763, -1 | 1 | 2763, 2762, 2771, -1 | 5 |
| 9903 | 0x43B10 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4920, 4907, 4908, -1 | 1 | 4908, 4907, 4920, -1 | 5 |
| 9906 | 0x43B4C | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4913, 4914, 4924, -1 | 1 | 4914, 4924, 4913, 4913 | 2 |
| 9911 | 0x43BB0 | round 2 | TR/BL corner, mirrored (quad, last corner repeated) | 4440, 4906, 4919, -1 | 1 | 4906, 4919, 4440, 4440 | 2 |
| 9915 | 0x43C00 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4923, 4915, 4916, -1 | 1 | 4916, 4915, 4923, -1 | 5 |

## Left as they are

- **L113 and L114, the strip above the breakable walls** (type 0x15 objects; on L113 faces such as 3127, 3132 and 3137, texture 255). It is the same art as L112's strip, texture 259, but each level's palette colours it: flat dark red on L112, flat dark teal on L113 and L114, which reads as black. Neither level has a dark red version of the strip to swap to; L113's nearest flat brown (texture 42, 40 24 16) is lighter and browner than L112's (8 0 0). Left as shipped (Edward, 2026).
