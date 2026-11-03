# Level Cleanup: what each level gets

From data/LevelCleanupList.json (zip 107), made by ALTLevelCleanupList from the level files as the fixes leave
them; the install only applies it (Options > Modern > Testing > Level Cleanup, then restart). Faces go for one
of four reasons, none of which should leave anything to see where they were:

- **Out of view:** in a cell the view walk never takes in from anywhere the player can be.
- **Facing away:** a one-sided face whose front no place the player can be looks at (the back of a wall).
- **Sealed lifts:** L321-L325's lifts that the level does not use, crawled whole from behind their solid doors.
- **Hidden:** an opaque face other opaque faces always stand in front of.

"First cell" is a level whose first cell gets its faces back (its cell index, row-major from the map's top-left).

| Level | Out of view | Facing away | Sealed lifts | Hidden | First cell | Where to look | Bytes saved |
|---|---|---|---|---|---|---|---|
| L111 | 7 | - | - | - | - | four faces just behind the start (cells (36,98)-(36,101)) and three on the top edge (44-46,0) | 300 |
| L112 | - | - | - | - | 4 | first cell only | 16 |
| L113 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 8 |
| L114 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 8 |
| L115 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 8 |
| L122 | - | - | - | - | 0 | first cell only | 0 |
| L131 | - | - | - | 8 | 26 | 8 behind walls | 184 |
| L141 | - | - | - | 2 | 26 | 2 behind walls | 40 |
| L154 | - | 6 | - | 2 | 0 | 6 wall backs, 2 behind walls | 160 |
| L155 | - | 46 | - | 1 | - | 46 wall backs, 1 behind walls | 1004 |
| L161 | - | - | - | 3 | 8 | 3 behind walls | 92 |
| L162 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 32 |
| L211 | - | - | - | 1 | 49 | 1 behind walls | 20 |
| L212 | - | 1 | - | 2 | 16 | 1 wall backs, 2 behind walls | 60 |
| L213 | 2 | - | - | - | 34 | 2 out of every view | 104 |
| L222 | - | 2 | - | - | 23 | 2 wall backs | 40 |
| L231 | - | - | - | - | 17 | first cell only | 0 |
| L232 | - | - | - | 5 | 58 | 5 behind walls | 116 |
| L242 | - | - | - | - | 68 | first cell only | 0 |
| L243 | - | - | - | - | 68 | first cell only | 0 |
| L262 | - | - | - | - | 18 | first cell only | 0 |
| L263 | - | - | - | 68 | 7 | 68 behind walls | 1880 |
| L311 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 32 |
| L321 | 2684 | 24 | 217 | - | 31 | the walled-off core; the other versions' lifts behind every sealed door (see level-cleanup-lift-remainder.md for what stays) | 80172 |
| L322 | 1266 | 172 | 186 | 636 | 31 | the core; the sealed lifts; the central room's walls that never show through its gate | 60480 |
| L323 | 2709 | - | 80 | 81 | 31 | the core; the sealed lifts | 78528 |
| L324 | 2709 | - | 78 | 98 | 31 | as L323 | 78956 |
| L325 | 672 | - | 77 | 76 | 31 | parts outside the walls; the sealed lifts | 21980 |
| L331 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 16 |
| L351 | - | - | - | - | 39 | first cell only | 0 |
| L352 | - | 13 | - | 150 | 26 | 13 wall backs, 150 behind walls | 4132 |
| L353 | - | - | - | - | 26 | first cell only | 0 |
| L371 | 1 | 12 | - | 5 | 26 | 1 out of every view, 12 wall backs, 5 behind walls | 416 |
| L381 | - | - | - | - | 67 | first cell only | 0 |
| L391 | - | - | - | - | 5 | first cell only | 0 |
| L900 | 7 | - | - | - | - | as L111 | 300 |
| L901 | - | - | - | - | 0 | first cell only | 0 |
| L902 | 1209 | - | - | - | 17 | the rings outside the arena wall (north) | 35252 |
| L903 | - | - | - | 2 | 26 | 2 behind walls | 40 |
| L904 | - | - | - | 44 | 73 | 44 behind walls | 1080 |
| L905 | - | - | - | 9 | 0 | 9 behind walls | 212 |
| L906 | 28 | 44 | - | 3 | 23 | 28 out of every view, 44 wall backs, 3 behind walls | 2196 |
| L907 | - | - | - | - | 17 | first cell only | 0 |
| L908 | - | - | - | - | - | nothing visible changes (only vertices no face used) | 32 |
| L909 | - | - | - | - | 26 | first cell only | 0 |
| **All** | 11294 | 320 | 638 | 1196 | | | 367896 |

**Most worth testing** (faces facing away, in lifts or hidden), most first: L322, L321, L324, L352, L323, L325, L263, L155, L906, L904, L371, L905, L131, L154, L232, L161, L212, L141, L222, L903, L211. Walk the level and
look from every window, door and gate for holes; on L321-L325, from in front of every closed lift door.

The 10 levels whose first cell has no faces in the file are L111, L113-L115, L155, L162, L311, L331, L900 and L908;
theirs still need inserting.
