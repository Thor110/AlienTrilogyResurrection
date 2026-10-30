# Repository review and plan (October 2026)

A pass over the whole repository after the texture correction pass (`5e6d6d5`): the engine, ALTViewer, the patch data, the notes and the build. The first part says what was checked and found. The second part is the plan: everything still to do, in the order I would take it.

## What was checked

| Check | Result |
|---|---|
| CI (Windows, Linux, macOS) | Green on every recent commit to main. |
| GCC with `-Wall -Wextra -Wpedantic` and a dozen more warnings | No warnings. With `-Wshadow`, 204, all harmless: 199 are a settings class's `config` parameter named like its member, 5 are inner variables that reuse an outer name and are only used locally. |
| AddressSanitizer + UndefinedBehaviorSanitizer, all 179 tests | Clean. |
| All 45 levels under the sanitizers: loader, 600 ticks of lights and texture animation, mesh builds for the three fills, every light toggled | Clean. |
| clang-tidy (`bugprone-*`, `performance-*`) over every engine source file | No real bugs. The bugprone hits are all deliberate (signed-byte casts of the original's signed fields, pixel centring by integer division, guarded decrements, optionals tested before use). The performance hits are small (string concatenation in the installer, vectors filled without `reserve`). Most of the raw count is in the bundled `stb_image.h`. |
| Save loading | Sound. Every read sits in a try block and each part of a level snapshot is checked against the level before it is restored. One hardening gap, fixed below. |
| Frame timing | Fixed-tick throughout; nothing in gameplay runs off the frame time. One structural point, item 4.2. |
| Creature turning | Matches the original: `FUN_0002e638` gives every creature type +0x46 = 0x200 and `FUN_00032ec4` turns by it each tick, as `CreatureAI.h` does. The 0x60 in `Enemies.h` belongs to the old movement path, which gameplay no longer reaches (item 4.1). |

## Fixed with this review

- **ALTViewer crashed on start** if the game folder was not found: it showed the message, called `Close()` and carried on reading level files from a relative path. It now returns.
- **ALTViewer also crashed on start** if any of the five files its patch check reads was missing or short (a partial install, or the demo). The check now reads them with `BinaryUtility.TryReadByteAtOffset`, which gives -1 instead of throwing; with a file missing the patch button stays hidden, since patching would fail on the same file.
- **Saves:** a save's level must now be three digits. It becomes part of the level's file path, so a hand-edited or damaged save could have pointed outside the game folder. Tested.
- **README:** the road map's patch list points at the full list (it stopped at L371 Fix 3); two of its lines had lost their indent; the to-do list no longer asks for the egg sack UVs (fixed, L391 Fix 1) and now names the texture scan candidates left to review; a typo.

## The plan

### 1. Play-testing (needs the game running, not code)

These are done in code from the binary but have not been seen in play. They are the cheapest way to find what is still wrong.

1. **The texture fixes:** L906's 53 bridge faces especially, then L141/L903's crates, L111/L900's crates and lift panel, L161's wall, and the 92 texture scan faces.
2. **3.5.1:** the walk-through walls and floors, the see-through fill and the fog.
3. **The resupply levels' countdown** (113, 114, 115, 155, 242, 243, 352, 353), and its colours.
4. **The levels that lost models** (21 levels): every door and lift should still look and move the same.
5. **Breakable windows** (glass) and the **cryopod cover**.
6. **Save Anywhere:** save and load mid-level, with creatures, doors and lifts part-way.
7. **The per-type creature tasks** (egg, queen, colonist, vents) and the ceiling creatures letting go.
8. **The mission assessment screen** on a real disc, with the real `LOGOSGFX.PAL` and a level palette.
9. **CD music** from a real drive on Linux and macOS.

### 2. Performance (in order of payoff)

1. **Light and texture animation updates.** `ModelRenderer::TickLevelLights` rebuilds the whole level mesh on the CPU (three passes over every face) and re-uploads every group, with new transfer buffers, whenever any light flickers or any animated texture steps, which can be most creature ticks. Better: upload only the faces whose light or animator changed (keep a face-to-vertex-range table from the build), or move the light colour into a small GPU table the shader reads by light id, so a flicker is a 128-entry upload.
2. **The frame's GPU → CPU → GPU copy** (`RenderLevelToRgba`): the frame waits on a fence, copies the image into a new vector (about 8 MB at 1080p, allocated every frame) and uploads it again as an SDL texture. Short term: keep one buffer and reuse it. Long term: draw straight to the window (item 7e in the engine notes), which needs testing on real hardware.
3. **Sprites:** a new upload buffer is created every frame, and the render pass is closed and reopened to upload them. A persistent, reused transfer buffer and one upload before the pass would remove both.
4. Small: `reserve` before the loops clang-tidy names (menu tree, HUD messages, explosions).

### 3. Matching the original: what is still a guess

Each is marked GUESS or NOT TRACED in the code. Grouped by how visible it is.

**Visible in play**
- **Weapon frame tables:** a weapon without one is placed bottom-centre (`WeaponView.cpp`); the sequence data itself is not traced (`WeaponView.h`).
- **Fog colour:** the code gives dark yellow at 50% and the video shows green (engine notes, section 8). Its distance is traced now (zip 48): 3400 to 8500 of view depth.
- **The draw limit:** the original draws only the cells its ring walk reaches (15 rings, through the zones the player can see into); the port draws the whole level.
- **Neutral light value:** 128 versus 255, the one remaining guess in the lighting path (`LightTable.h`).
- **Model and door lighting:** they draw at full brightness; the entity light path is not traced (`RenderMesh.h`).
- **Enemy sprite scale** (`EnemySprites.h`), **explosion scale and duration** (`ExplosionGraphics.h`), **the barrel fireball's offset and the particle size** (`GameplayScreen.cpp`).
- **Shell casings:** spin and gravity (`ParticleSystem.h`), and model 26 for the shotgun shell (inferred).
- ~~**Ramp pitch sign**~~: worked out from the original (zip 48): the port's yaw is the original's turned half round and its pitch runs the other way, and the two cancel.
- **Music per level** (`MusicPlayer.h`).
- **Vent jets:** the blend of their two fills, and whether a burst pipe does blast damage.
- **Routine 14 flat faces:** which palette their colour comes from (unused by the levels checked).
- **The assessment screen's bar colours:** from the level's palette file, bit depth unconfirmed.

**Less visible**
- Which action prints which HUD message (`HudMessages.h`), the pickup counters (`PickupSystem.h`), the weapon's noise value (`WeaponSystem.h`), the shatter launch speed (`ShatterEffects.h`), the minimap and motion tracker (`Minimap.h`), a one-shot HUD gate's reset (`HudPanel.h`).
- Particle types 0x0b, 0x0c and 0x13, which take a body away at once; the port fires none of them.
- The visited-cell bit (+7 bit 1) the view walk sets; unused by the port.
- **Open question A:** pistol rounds against a standing hugger.
- Defaults that are guesses: mouse sensitivity, two settings defaults (`GameplaySettings.h`).

### 4. Code health

1. **Remove the old creature movement path in `Enemies.h`.** Gameplay always passes the AI world, so every creature runs the original-based tasks; the old turn-then-walk code (`TURN_RATE` 0x60, `ALIGN_BEFORE_MOVING`, `SPEED_SCALE`, `NON_LEAP_ACT_TICKS`) only runs without one. Check the tests that use it first. Unused constants to drop too: `BARREL_HEALTH_UNKNOWN`, `REST_SECTION`, `FRAME_DURATION_GUESS`.
2. **One tick loop.** The player and the world each have their own accumulator fed from the same frame time, so they can fall a tick out of step. The original runs everything from one pass counter (player every pass, creatures on odd passes); one accumulator and one counter would match it exactly.
3. **Split `GameplayScreen.cpp`** (4,143 lines): the level load, the tick, the draw and the HUD are natural pieces.
4. **Tests for what has none:** the light table, texture animation, sprite decompression and RLE, the BND and BX parsers. The demo files in `Notes/repack-disc-comparison` are enough data.
5. **The TODOs:** the pause menu's fixed 500-pixel panel, driving the boot list from `BootSequence.json`, and black as transparent on the menu's speaker textures.
6. Optional: rename the settings classes' `config` parameter so `-Wshadow` can be turned on.

### 5. Patches

1. **The texture scan's remaining candidates:** L263 (5), L351 (82) and L371 (51) are not yet reviewed.
2. **Doors and lifts:** the scan only looks at level geometry; door and lift models could have the same kind of faults.
3. **ALTViewer's patcher** assumes every file is there; with a file missing it stops part-way. It could check all of them first and say which are missing.

### 6. Larger features

- **Multiplayer:** the menus exist; there is no networking.
- **Direct-to-window rendering** (2.2 above).
- **ALTViewer** (from the README's to-do list): replacing compressed images needs a matching compressor; level object export (enemies, crates, pickups, switches, doors); the text editor is effectively dropped.
