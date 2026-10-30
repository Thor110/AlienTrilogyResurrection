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
- **Mirrored, other half:** a triangle cannot show the half of the tile cut along x0y0-x1y1. These become a
  quad (a, b, c, c) with routine 2 (mirrored quad, corners x1y1, x0y1, x0y0, x1y0): the triangle (a, b, c) is the
  face, and (a, c, d) has no area. The port draws quads as (a, b, c) + (a, c, d), so this is exact there. No
  shipped level uses a quad like this, and how the original rasterises one is not traced, so in DOS it is
  unverified.

## L371LEV

Recesses: each wall recess frames a band (texture 35) with four corner triangles (texture 35, or 27 in some
recesses). The target is a symmetric frame: every corner shows the band mirrored across the seam it shares with
the band.

| Face | Offset | Added | Change | Was | Routine | Now | Routine |
|---|---|---|---|---|---|---|---|
| 2581 | 0x1FF08 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 8264, 8265, 8266, -1 | 1 | 8266, 8265, 8264, -1 | 5 |
| 2582 | 0x1FF1C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 8270, 8271, 8272, -1 | 1 | 8272, 8271, 8270, -1 | 5 |
| 4516 | 0x29634 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4402, 4390, 4389, -1 | 1 | 4389, 4390, 4402, -1 | 5 |
| 4521 | 0x29698 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4397, 4382, 4381, -1 | 1 | 4381, 4382, 4397, -1 | 5 |
| 4902 | 0x2B45C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2690, 2675, 2674, -1 | 1 | 2674, 2675, 2690, -1 | 5 |
| 4949 | 0x2B808 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2726, 2714, 2713, -1 | 1 | 2713, 2714, 2726, -1 | 5 |
| 5002 | 0x2BC2C | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4480, 4479, 4492, -1 | 1 | 4479, 4492, 4480, 4480 | 2 |
| 5005 | 0x2BC68 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4496, 4486, 4485, -1 | 1 | 4485, 4486, 4496, -1 | 5 |
| 5010 | 0x2BCCC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4491, 4478, 4477, -1 | 1 | 4477, 4478, 4491, -1 | 5 |
| 5014 | 0x2BD1C | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4488, 4487, 4495, -1 | 1 | 4487, 4495, 4488, 4488 | 2 |
| 5178 | 0x2C9EC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2695, 2683, 2682, -1 | 1 | 2682, 2683, 2695, -1 | 5 |
| 5219 | 0x2CD20 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2721, 2706, 2705, -1 | 1 | 2705, 2706, 2721, -1 | 5 |
| 5306 | 0x2D3EC | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5715, 5704, 5705, -1 | 1 | 5705, 5704, 5715, -1 | 5 |
| 5317 | 0x2D4C8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5710, 5696, 5697, -1 | 1 | 5697, 5696, 5710, -1 | 5 |
| 5628 | 0x2ED14 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 5807, 4408, 4418, -1 | 1 | 4408, 4418, 5807, 5807 | 2 |
| 5631 | 0x2ED50 | round 1 | TL/BR corner, mirrored (reversed to c, b, a) | 4421, 4412, 5809, -1 | 1 | 5809, 4412, 4421, -1 | 5 |
| 5636 | 0x2EDB4 | round 1 | TL/BR corner, mirrored (reversed to c, b, a) | 4417, 4407, 4406, -1 | 1 | 4406, 4407, 4417, -1 | 5 |
| 5640 | 0x2EE04 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4414, 4413, 4420, -1 | 1 | 4413, 4420, 4414, 4414 | 2 |
| 5890 | 0x3018C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5756, 5742, 5743, -1 | 1 | 5743, 5742, 5756, -1 | 5 |
| 6291 | 0x320E0 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5761, 5750, 5751, -1 | 1 | 5751, 5750, 5761, -1 | 5 |
| 6812 | 0x34994 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5820, 5813, 5814, -1 | 1 | 5814, 5813, 5820, -1 | 5 |
| 6815 | 0x349D0 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 5817, 5818, 5821, -1 | 1 | 5818, 5821, 5817, 5817 | 2 |
| 6820 | 0x34A34 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4425, 4426, 4433, -1 | 1 | 4426, 4433, 4425, 4425 | 2 |
| 6824 | 0x34A84 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4436, 4429, 4430, -1 | 1 | 4430, 4429, 4436, -1 | 5 |
| 7578 | 0x3856C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4329, 4315, 4314, -1 | 1 | 4314, 4315, 4329, -1 | 5 |
| 7591 | 0x38670 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4334, 4324, 4323, -1 | 1 | 4323, 4324, 4334, -1 | 5 |
| 7600 | 0x38724 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4363, 4346, 4345, -1 | 1 | 4345, 4346, 4363, -1 | 5 |
| 7603 | 0x38760 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4359, 4358, 4367, -1 | 1 | 4358, 4367, 4359, 4359 | 2 |
| 7609 | 0x387D8 | round 1 | TR/BL corner, mirrored (quad, last corner repeated) | 4348, 4347, 4364, -1 | 1 | 4347, 4364, 4348, 4348 | 2 |
| 7613 | 0x38828 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4368, 4357, 4356, -1 | 1 | 4356, 4357, 4368, -1 | 5 |
| 9273 | 0x409D8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4474, 4464, 4465, -1 | 1 | 4465, 4464, 4474, -1 | 5 |
| 9532 | 0x41E14 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4471, 4456, 4457, -1 | 1 | 4457, 4456, 4471, -1 | 5 |
| 9610 | 0x4242C | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5735, 5724, 5725, -1 | 1 | 5725, 5724, 5735, -1 | 5 |
| 9873 | 0x438B8 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 5730, 5716, 5717, -1 | 1 | 5717, 5716, 5730, -1 | 5 |
| 9881 | 0x43958 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2768, 2754, 2755, -1 | 1 | 2755, 2754, 2768, -1 | 5 |
| 9893 | 0x43A48 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 2771, 2762, 2763, -1 | 1 | 2763, 2762, 2771, -1 | 5 |
| 9903 | 0x43B10 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4920, 4907, 4908, -1 | 1 | 4908, 4907, 4920, -1 | 5 |
| 9915 | 0x43C00 | zip 51 | TL/BR corner, mirrored (reversed to c, b, a) | 4923, 4915, 4916, -1 | 1 | 4916, 4915, 4923, -1 | 5 |
