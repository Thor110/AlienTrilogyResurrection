# Level Cleanup: what each level gets

Generated from the zip 104 cleanup run on every level (Options > Modern > Testing > Level Cleanup, then
restart). "First cell" is the cell whose faces are put back - always on the map's top row (z = 0), as the
original lists its cells from that corner. "Trimmed" faces are ones the original can never draw, so there should
be nothing to see where they were.

| Level | Trimmed faces | Unused vertices | First cell restored | Where to look | Bytes saved |
|---|---|---|---|---|---|
| L111 | 7 | 20 | - | four faces just behind the start (cells (36,98)-(36,101)) and three on the top edge (44-46,0) | 300 |
| L112 | - | 2 | yes (1) | first cell (4,0): 1 face | 16 |
| L113 | - | 1 | - | nothing visible changes (only vertices no face used) | 8 |
| L114 | - | 1 | - | nothing visible changes (only vertices no face used) | 8 |
| L115 | - | 1 | - | nothing visible changes (only vertices no face used) | 8 |
| L122 | - | - | yes (5) | first cell (0,0): 5 faces | 0 |
| L131 | - | - | yes (7) | first cell (26,0): 7 faces | 0 |
| L141 | - | - | yes (2) | first cell (26,0): 2 faces | 0 |
| L154 | - | - | yes (4) | first cell (0,0): 4 faces | 0 |
| L155 | - | 1 | - | nothing visible changes (only vertices no face used) | 8 |
| L161 | - | - | yes (1) | first cell (8,0): 1 face | 0 |
| L162 | - | 4 | - | nothing visible changes (only vertices no face used) | 32 |
| L211 | - | - | yes (7) | first cell (49,0): 7 faces | 0 |
| L212 | - | - | yes (7) | first cell (16,0): 7 faces | 0 |
| L213 | 2 | 8 | yes (7) | 2 faces out of every view; first cell (34,0): 7 faces | 104 |
| L222 | - | - | yes (7) | first cell (23,0): 7 faces | 0 |
| L231 | - | - | yes (1) | first cell (17,0): 1 face | 0 |
| L232 | - | - | yes (5) | first cell (58,0): 5 faces | 0 |
| L242 | - | - | yes (7) | first cell (68,0): 7 faces | 0 |
| L243 | - | - | yes (7) | first cell (68,0): 7 faces | 0 |
| L262 | - | - | yes (1) | first cell (18,0): 1 face | 0 |
| L263 | - | - | yes (8) | first cell (7,0): 8 faces | 0 |
| L311 | - | 4 | - | nothing visible changes (only vertices no face used) | 32 |
| L321 | 2684 | 2499 | - | the walled-off central core, and the other versions' elevators | 73672 |
| L322 | 1266 | 1096 | - | the core and other elevators | 34088 |
| L323 | 2709 | 2518 | - | the core and other elevators | 74324 |
| L324 | 2709 | 2518 | - | the core and other elevators | 74324 |
| L325 | 672 | 564 | yes (3) | parts outside the walls; first cell (31,0): 3 faces | 17952 |
| L331 | - | 2 | - | nothing visible changes (only vertices no face used) | 16 |
| L351 | - | - | yes (2) | first cell (39,0): 2 faces | 0 |
| L352 | - | - | yes (4) | first cell (26,0): 4 faces | 0 |
| L353 | - | - | yes (4) | first cell (26,0): 4 faces | 0 |
| L371 | 1 | - | yes (3) | 1 face out of every view; first cell (26,0): 3 faces | 20 |
| L381 | - | - | yes (3) | first cell (67,0): 3 faces | 0 |
| L391 | - | - | yes (4) | first cell (5,0): 4 faces | 0 |
| L900 | 7 | 20 | - | as L111 | 300 |
| L901 | - | - | yes (5) | first cell (0,0): 5 faces | 0 |
| L902 | 1209 | 1384 | - | the rings outside the arena wall (north) | 35252 |
| L903 | - | - | yes (2) | first cell (26,0): 2 faces | 0 |
| L904 | - | - | yes (7) | first cell (73,0): 7 faces | 0 |
| L905 | - | - | yes (5) | first cell (0,0): 5 faces | 0 |
| L906 | 28 | 45 | yes (7) | 28 faces out of every view; first cell (23,0): 7 faces | 920 |
| L907 | - | - | yes (1) | first cell (17,0): 1 face | 0 |
| L908 | - | 4 | - | nothing visible changes (only vertices no face used) | 32 |
| L909 | - | - | yes (5) | first cell (26,0): 5 faces | 0 |

**Most worth testing:**

- Trimmed only: L111, L321, L322, L323, L324, L900, L902. Walk the level, and look from every window and door for holes.
- Trimmed and first cell restored: L213, L325, L371, L906.
- First cell restored only: L112, L122, L131, L141, L154, L161, L211, L212, L222, L231, L232, L242, L243, L262, L263, L351, L352, L353, L381, L391, L901, L903, L904, L905, L907, L909. Look at the map's top edge near the cell named.
- No visible change (a few unused vertices dropped): L113, L114, L115, L155, L162, L311, L331, L908.

The 10 levels whose first cell has no faces in the file are L111, L113-L115, L155, L162, L311, L331, L900 and L908;
theirs still need inserting.
