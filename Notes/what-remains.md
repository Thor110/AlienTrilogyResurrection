# What remains (after zip 82)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play

Checked by Edward after zip 79: doors, pickups, the window glass and the cryopods are right; so is everything on
the older list - 3.5.1's walk-through walls, see-through fill and fog, the resupply countdown, the levels that
lost duplicate models, Save Anywhere, the assessment screen, music per level and the vent jets.

Still open:
- **CD music from a real drive on Linux and macOS:** untested.
- **Creature scripting** (the egg, queen, colonist and the ceiling creatures letting go) still feels a little
  off. Needs a closer comparison against the original in play to say where.
- **Thrown seismic charge (zip 82):** traced again end to end (`FUN_0002c830`, the arm at 0x2db80,
  `FUN_0002a2d8`, `FUN_0002cf14`). The launch already matched: 0x100 below the eye, (0, -0x18, speed + 0x40 up to
  0x80) turned by the pitch then the yaw, gravity one a tick, bounces of 8, 6, 4. The -0x18 is a downward push, so
  the original's throw only rises when looking up while moving; the arc is the fall. What differed: the original
  doesn't draw it until it is 0x200 ahead of the view (drawn from the start it seemed to rise out of the floor),
  it tumbles 0x20 a tick, it goes off when it passes the ceiling, and the start x carries the walk's dip. All four
  are in now. Check in play.

## 2. Still a guess or not traced

**Visible in play**
- **The draw limit:** the original draws only the cells its ring walk reaches; the port draws the whole level.
  Worth doing later if the information is easy to use: the ring walk and zone masks are already traced and in
  the port (`Game/ViewRings.h`, `CreatureSight::DrawnZoneMask`), so it may be close.
- **Model and door lighting:** drawn at full brightness; the entity light path is not traced.
- **Neutral light value:** 128 versus 255 (`LightTable.h`).
- **Weapon frame tables** for weapons without one (`WeaponView`).
- **Sizes and timings:** enemy sprite scale, explosion scale and duration, the barrel fireball's offset,
  particle size.
- **Shell casings:** their spin and gravity, and model 26 for the shotgun shell (inferred).
- **Vent jets:** look right in play. Whether a burst pipe does blast damage is unchecked, and the sound plays once
  instead of looping.
- **The assessment screen's bar colours** (palette bit depth).
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

Done in zip 81:
1. **The old creature movement path is gone** (`Enemies.h`): the turn-then-walk movement, its melee leap and
   ranged attack, `TURN_RATE`, `ALIGN_BEFORE_MOVING`, `SPEED_SCALE`, and the fields only it used. Every monster
   type in the data (1-14, 0x10-0x13) has its own task, so `Tick` now requires the AI world. The tests that drove
   the old path run the real tasks on an open-floor world (`tests/TestWorld.h`); one that described the old
   path's rule (a burster's damage only on landing) is dropped, since the common task lands the hit when the
   attack's script says. `BARREL_HEALTH_UNKNOWN`, `REST_SECTION` and `FRAME_DURATION_GUESS` are gone too
   (`NON_LEAP_ACT_TICKS` stays: the common task uses it).
2. **One tick loop:** the world's tick now runs exactly the player's passes, each with that pass's own alternate
   flag, instead of its own accumulator and counter.
3. **`GameplayScreen.cpp`, first step:** the object rules (each type's model, the switch families, the windows,
   the cryopods, the glass texture) are in `Screens/GameplayObjects.h`; the glass sheet and the cryopod cover are
   built in `Screens/GameplayLevelBuild.h`; both tested. `Run` itself is still one function of about 4,300 lines
   with its state in local variables; the next step is to gather that state into a struct so the level load,
   the tick, the draw and the HUD can become functions of their own.
4. **Tests added:** the light table (flat, flash, gouraud, blink), texture animation (speed, loop, gate, stop),
   the BND and BX parsers (synthetic and the demo's 111GFX), the sprite RLE and the LZSS frame decoder, the boot
   sequence, and the object rules.
5. **The TODOs:** the pause menu's briefing wraps to the window's width; the intro videos come from
   `BootSequence.json` (the original's four if it is missing); the speaker-texture note was already handled
   (`ModelPreviewSource::ForOptobj`) and its dead code is gone.
6. **Files looked for in the wrong place:** none left - only the level's MAP and GFX are looked for in the SECT
   folders, which is where they are.

Still open:
- **Splitting `Run`** further (above).
- Optional: rename the settings classes' `config` parameter so `-Wshadow` can be turned on.

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
