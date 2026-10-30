# What remains (after zip 85)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play

Confirmed by Edward: doors, pickups, the window glass, the cryopods, the older list (3.5.1, the resupply
countdown, Save Anywhere, the assessment, music per level, the vent jets), the thrown seismic charge (zip 83)
and its explosion (zip 84).

Changed in zip 85, to check:
- **Models lit by their cell.** Crates, switches, pickups, doors, lifts and anything thrown take the light of
  the cell they stand in (its byte 0x0e; FUN_000269b8 and its siblings), as do the creatures' sprites
  (FUN_0002fb84). They were all at full brightness. The model shader has a new tint uniform: the Vulkan and
  Metal shaders are rebuilt here; the Direct3D one is compiled from the HLSL by dxc when building on Windows.
- **The map** shows what the view walk has seen (every cell it takes in, not a 3x3 patch round the player), in
  the areas the player has stood in; the Auto Mapper shows the whole of each discovered area rather than the
  whole level (0x2745b, FUN_00043078).
- **The shoulder lamp and night-vision goggles** can be used: Enter on the pause menu's Shoulder Lamp entry
  lights a lamp, or goggles if there is no lamp (FUN_00044b48). The lamp is a warm light, the goggles a
  green-blue one; they come up over 32 ticks, last 0x708 and 0xa8c ticks, and fade over 64 (FUN_00029d94).
  Each pickup adds one.
- **Explosives** follow the original's rules for what they hit (FUN_0002a2d8, FUN_0002a628): any creature but
  the queen is killed outright and sprays 0x40 big drops of blood; an object they strike loses 0x140; walls
  and windows (0x15, 0x1c, 0x1d) and objects within 0x200 of the floor set them off; they set off the cell
  triggers a bullet would, and a handler's grenade its own (flag 8). Ordinary wall cells don't stop them, in
  the original either.
- **Creature sprites** are drawn at each type's own scale (+0xb4, 7.6 to 11 world units a pixel; 8 before)
  and placed across the view by each frame's own x offset.
- **Sparks and tracers** keep the original's on-screen size (a 2x2 pixel spark, a one-pixel line).
- **A creature's dropped item** starts 0x200 above the floor.
- **Run Mode** is a toggle, as Caps Lock is in the original.

Still open:
- **CD music from a real drive on Linux and macOS:** untested.
- **Creature scripting** still feels a little off to Edward. The timing is the original's own: every creature
  type runs its own animation scripts (EnemyAnimSequences.h) and its thresholds come from its level record,
  so a note of what looks wrong, and where, is what's needed to go further.

## 2. Audit against the original (zips 84-85)

Method: every routine the port cites was compared with the decompile where it touches what is seen or played,
and every game-side routine the port never cites (about 240 of the 1,242 Ghidra found; the rest are the C
library, the sound and video drivers and the DOS extender) was looked at to see what it does.

### Known to be missing
1. **Multiplayer.** FUN_00054a60 and everything behind `DAT_000ae0a4`.
2. **The draw limit.** The original draws only the cells its view walk reaches; the port draws the whole level
   (with depth testing, so only what's in sight shows). The walk is in the port (`CreatureSight`), and the
   seen cells now come from it; limiting the draw to them needs the level mesh split by cell.
3. **Effects past 0x1000.** FUN_000429f0 draws a near effect in colour 0x80 and a far one in the table's own
   colour (+0xa8) faded by distance; those table colours don't read as plain colours (ff,ff,ff and 00,ff,00), so
   the far case isn't copied. Near ones, the usual case, are right.
4. **The lamp's hum** (sound 0x18 while lit) - the mixer can't hold a loop, as with the vent jets.
5. **The pause menu's zoomed map** (entry 0 used: a 28-cell window round the player, FUN_00044b48).

### Inferred, or the port's own on purpose
1. **HUD:** some icon artwork and the message presentation are the port's; which action prints which message
   is inferred from the text; a one-shot gate's reset isn't traced.
2. **Creature sound cues** are cut past 0x1400 or out of sight (the original only quietens them), and a
   creature let out of a sealed wall vent comes out alerted - both asked for in play.
3. **Casings** vanish at the floor; the original's fall through it out of sight.
4. **Vent jets'** sound plays once instead of looping.
5. The assessment bars' colours, routine 14 on level faces, L371's lifts 6-9, the weapon noise value, the
   shatter launch speed, mouse sensitivity and two settings defaults; open question A (pistol rounds against a
   standing hugger).

### Settled in this pass
- The neutral light value is 0x80 (an effect near the player is drawn in 0x80; every level's light 0 is
  128,128,128) - the port already used it.
- Pickup 0x0f (ID badges, 0xae2) is counted and copied once into a value nothing reads (0x14c78): it does
  nothing in the original either. Pickup 7 is the batteries, already in.
- The weapon view's frame offsets, scripts and anchor are all the original's (`WeaponData.h`); its comments
  were out of date.
- Creature animation timing and attack timing come from the scripts for every creature type; the fixed values
  in `Enemies.h` are fallbacks nothing reaches.

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
