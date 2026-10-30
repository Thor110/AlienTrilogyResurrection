# What remains (after zip 79)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play (done in code, not yet seen running)

- **Doors** against the original's rules: open-time-0 doors staying open, timed doors holding then closing
  once you are away, the doorway test (zip 72), and L112's one-sided door.
- **Pickups:** reach (tables in the next cell), items left when you are full, resting height, the scale
  table for types 17-21.
- **Breakable windows:** the flying glass (zip 74), on L131, L211 and L212 (three rows on L212).
- **Cryopods on L211:** its cover colour comes out pale grey (the level's palette), not dark as on L131.
- **Thrown seismic charge and grenade** at full size.
- **Patches at every launch:** a fresh install and an old one both end up the same.
- From the older list: 3.5.1's walk-through walls, see-through fill and fog; the resupply levels' countdown;
  the 21 levels that lost duplicate door and lift models; Save Anywhere mid-level; the egg, queen, colonist,
  vents and ceiling creatures; the assessment screen on a real disc; CD music from a real drive on Linux and
  macOS.

## 2. Still a guess or not traced

**Visible in play**
- **Fog colour on 3.5.1:** the code gives dark yellow at 50%, the video shows green.
- **The draw limit:** the original draws only the cells its ring walk reaches; the port draws the whole level.
- **Model and door lighting:** drawn at full brightness; the entity light path is not traced.
- **Neutral light value:** 128 versus 255 (`LightTable.h`).
- **Weapon frame tables** for weapons without one (`WeaponView`).
- **Sizes and timings:** enemy sprite scale, explosion scale and duration, the barrel fireball's offset,
  particle size.
- **Shell casings:** their spin and gravity, and model 26 for the shotgun shell (inferred).
- **Vent jets:** the blend of their two fills; whether a burst pipe does blast damage. Their sound plays once
  instead of looping.
- **Music per level** (`MusicPlayer.h`).
- **The assessment screen's bar colours** (palette bit depth).
- **Thrown seismic charge:** the original spins it (+0x20 a frame on its first angle); the port doesn't.
- **A creature's dropped item** starts at the creature's height plus the type's offset; the original starts it
  0x200 above the floor (`FUN_0003bc9c`).
- **Routine 14's colour** is taken from the level's first palette (entry 2). That is traced for model faces;
  the levels don't use routine 14, and they still draw it textured.
- **L371's lifts 6-9** use lift model 5, which the file doesn't have; what the original draws for them is not
  traced.

**Less visible**
- Which action prints which HUD message, the pickup counters 0x07/0x08/0x0f/0x19, the weapon noise value, the
  shatter launch speed, the minimap and motion tracker, a one-shot HUD gate's reset.
- Particle types 0x0b, 0x0c and 0x13 (a body taken away at once): the port fires none of them.
- The visited-cell bit the view walk sets, unused by the port.
- **Open question A:** pistol rounds against a standing hugger.
- Defaults that are guesses: mouse sensitivity, two settings defaults.

## 3. Code health

1. Remove the old creature movement path in `Enemies.h`, and its unused constants.
2. One tick loop: the player and the world have separate accumulators and can drift a tick apart; the
   original runs both from one pass counter.
3. Split `GameplayScreen.cpp` (load, tick, draw, HUD).
4. Tests for what has none: the light table, texture animation, sprite decompression and RLE, the BND and BX
   parsers.
5. The TODOs: the pause menu's fixed 500-pixel panel, the boot list from `BootSequence.json`, black as
   transparent on the menu's speaker textures.
6. The general object preload looked for OBJ3D.BND in the SECT folders (fixed in zip 75); worth a look for any
   other file looked for in the wrong place.

## 4. Performance

1. Light and texture animation: a flicker rebuilds and re-uploads the whole level mesh; upload only what
   changed, or move light colours into a small GPU table.
2. The frame's GPU -> CPU -> GPU copy: reuse one buffer now; draw straight to the window later (needs real
   hardware).
3. Sprites: one persistent upload buffer instead of a new one each frame and a render pass reopened for it.
4. Small `reserve` calls.

## 5. Larger features

- **Multiplayer:** the menus exist, no networking.
- **Direct-to-window rendering.**
- **ALTViewer:** a compressor for replacing compressed images; level object export; its patcher checking every
  file before it starts.

## 6. UV review: levels still to go through

The log has fixes for L111/L900, L112-L114 (left as shipped), L131, L141/L903, L161, L211, L212, L232, L242,
L243, L262, L371, and L321-L325 (scratched for now). No entries yet for:

- **Campaign:** L115, L122, L154, L155, L162, L213, L222, L231, L263, L311, L331, L351, L352, L353, L381,
  L391.
- **Multiplayer:** L901, L902, L904-L909. Several share geometry with a campaign level (L900 is L111, L903 is
  L141), so their fixes may only need copying across.
- **The texture scan's candidates** not yet looked at: L263 (5), L351 (82), L371 (51; may already be covered by
  the L371 rounds).
- **Door and lift models:** the scan only covers level geometry.
- **To look at again:** L262 face 11915 (the animated cabinet), and L321-L325 once the doors across the versions
  are understood.
- **Optional:** L212's three unreachable pickups (7, 10, 18) can be made inert with a byte each.

## 7. The final step: missing faces, then a triple check of the textures

1. **Insert the missing faces** listed in `uv-review-log.md`: L161, L211, L212, L242, L243. Each goes into its
   cell's face run; the face count, later run starts, and the MAP0 and FORM sizes move up. L161's interim
   duplicate (3758) goes back to its original triangle.
2. **The level's first cell, every level:** its run start is 0 (the blank record) instead of its own run, so the
   original never draws that cell's faces, and its floor (often its ceiling too) is missing. Insert those faces
   at the start of the first run and set the run start to 1.
3. **Renumber everything that names a face after an insertion:** later Patches.json entries for the same level,
   the review log, and ALTViewer's fixer.
4. **Triple check the textures:** every level once more with the Face Index Display after the insertions, the
   texture scan run again on the patched files, and the multiplayer copies checked against their campaign levels.
5. **Bring the rest up to date in one pass:** `Patch/LxxxLEV.txt`, the README, and ALTViewer's fixer (none of the
   UV review's fixes are in it yet).
