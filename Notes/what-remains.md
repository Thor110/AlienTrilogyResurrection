# What remains (after zip 84)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play

Checked by Edward after zip 79: doors, pickups, the window glass and the cryopods are right; so is everything on
the older list - 3.5.1's walk-through walls, see-through fill and fog, the resupply countdown, the levels that
lost duplicate models, Save Anywhere, the assessment screen, music per level and the vent jets. The thrown
seismic charge was confirmed after zip 83.

Changed in zips 83-84, to check:
- **Rest pitch (zip 83):** the camera's roll of 0x800 (DAT_000b0a54) means the original's rest pitch 0x30 looks
  slightly down; the port rested slightly up. Now -0x30 in the port's direction, and play starts there. Check
  the view with Free Look off, and that the ramps still tilt it the right way.
- **Explosions (zip 84):** every explosion now uses the effect, position, sound and size the original's code
  gives it (details in section 2's audit). They are about three times the size they were, as the original
  draws them; this is the one most worth looking at against the DOS version.
- **Objects caught in a blast or shot to pieces (zip 84)** now go a few ticks later, each on its own random
  delay, so a row of barrels goes off one after another.
- **Blood (zip 84):** small tinted sprites from the effect sheet - red for people, green for aliens, the light's
  colour for type 10 - in place of white streaks.
- **Shell casings (zip 84):** start below the eye and pop up and out, tumble on two axes, fall at 6 a tick; the
  shotgun's at full size, the pistol's at half.
- **A handler's grenade (zip 84):** the pulse rifle grenade model at full size and its true height.

Still open:
- **CD music from a real drive on Linux and macOS:** untested.
- **Creature scripting** (the egg, queen, colonist and the ceiling creatures letting go) still feels a little
  off. The likely places are in the audit below (frame durations and the script stand-ins in `Enemies.h`).

## 2. Audit against the original (zip 84)

Method: every routine the port cites was compared with the decompile where it touches what is seen or played,
and every game-side routine the port never cites (about 240 of the 1,242 Ghidra found; the rest are the C
library, the sound and video drivers and the DOS extender) was looked at to see what it does. The explosion,
blood, casing and grenade work above came out of this.

### Fixed in this pass
- The grenade, the seismic charge and a handler's grenade went off as the **barrel** fireball, 0x100 up, with
  the barrel's sound. The original plays its general blast (FUN_0002aaf0: table 0xad008, sound 0x29) at the
  explosive's own position. A vent bursting is the same call, and it does no blast damage.
- A barrel's fireball goes off 0x30 under its floor (0x37d71), not 0x100 above it, and its frames hang from
  their own anchors.
- **Every effect frame** is placed by its anchor (each table's per-frame x and y at +0x68/+0x88) instead of
  centred, and sized at scale / 0x100 world units a pixel (the focal length), not the guessed 5 / 0x1000.
- An explosive hitting the ceiling only shows the effect; that path never reaches the blast (FUN_000368c8).
- The blast's per-cell barrel sound is gone: the sound belongs to whatever goes off.
- **Object fuses**: shot to pieces, (random & 7) + 1 ticks (FUN_00036e9c); in a blast, (random & 7) + 4 in the
  inner ring and (random & 0xf) + 8 in the outer (FUN_000368c8); counted down by the objects' handlers
  (0x37c4e, 0x37d37, 0x38004). A save keeps the count.
- **Blood** (types 8-10, FUN_0002d2bc), **casings** (FUN_0002b37c, the arm at 0x2e3df, FUN_0002cf14) and the
  **handler's grenade** (FUN_0002b240): as in section 1. The casing models (26 and 27) and the grenade's (12)
  are confirmed from the pickup model table at 0x2480f4.

### Known to be missing (the original's code does it; the port doesn't)
1. **Multiplayer.** FUN_00054a60 and everything behind `DAT_000ae0a4`: the other players' shots and throws
   (FUN_0002b858, FUN_0002beb4, FUN_0002c630, FUN_0002c734), random pickup placement in a network game
   (FUN_0003b5a0), and the network branches inside the blast, the doors, the lifts and the pickups.
2. **Blood from explosives.** A grenade or charge that hits a creature sprays 0x40 drops at four times the size
   (FUN_0002ada4 with an explosive as the source; the +0x1d flag). The port sprays none.
3. **What an explosive does to the object it hits.** FUN_0002a2d8 takes 0x140 off that object (FUN_00036e9c)
   and has separate rules for cell attribute 0x14, the switch-like types 0x15/0x1c/0x1d and objects within
   0x200 (0x400 for type 0x19) above the floor. The port sets any object or blocking cell off the same way.
4. **Explosives riding lifts.** The arm calls FUN_000414f0 each tick, which moves a particle with the lift
   under it. The port's explosives ignore lifts.
5. **Effect blending and fog.** Effects are drawn through the fill at 0x4b5cc, at full brightness under 0x1000
   away and faded by FUN_0004ecdc past it. The port draws them as plain cut-outs with no fade. The same fill is
   unsettled for the vent jets.
6. **Model and door lighting.** The entity light path isn't traced; models are drawn at full brightness.
7. **The motion tracker and the map.** The HUD routines FUN_000432fc and FUN_00043604-FUN_0004462c (called
   from the frame's HUD pass, FUN_00045090) are never cited; the port's tracker and map are its own
   (`Minimap.h` says as much). These are the most likely place for the original's.
8. **The draw limit.** The original draws only the cells its ring walk reaches (`Game/ViewRings.h` already has
   the walk); the port draws the whole level.

### Inferred to be different (marked in the code as a guess or as the port's own)
1. **Creature animation timing and scripts:** frame hold times (`Enemies.h`: "the real duration is entry 0 of
   the..."), the fields read from +0x48 on, and a script stand-in marked GUESS. The likeliest cause of the
   creature scripting feeling off.
2. **Enemy sprite scale** (`EnemySprites.h`): a guess. The explosion work gives the rule - a pixel is
   scale / 0x100 world units at the focal length 0x100 - so the creature draw's scale can be read the same way.
3. **Tracers and fragments:** the original draws a tracer as a screen line and a fragment as a 2x2 pixel quad
   (FUN_0002d3e4, FUN_0002d1b4), sizes that don't change with distance; the port's world sizes are guesses.
4. **Weapon view:** placement is the port's, and some weapons have no frame table (`WeaponView`).
5. **HUD:** some icon artwork and the message presentation are the port's; which action prints which message
   is inferred from the text; a one-shot gate's reset isn't traced.
6. **Neutral light value** 128 versus 255 (`LightTable.h`).
7. **A creature's dropped item** starts at the creature's height plus the type's offset; the original starts it
   0x200 above the floor (`FUN_0003bc9c`).
8. **Creature sound cues** are cut past 0x1400 or out of sight; the original only attenuates them.
9. **A creature let out of a sealed wall vent** comes out alerted (the port's rule, so L111's huggers leave the
   duct).
10. **Run mode** is a second hold key; the original's is probably a toggle.
11. **Casings** vanish when they reach the floor; the original's fall through it out of sight. The same to look
    at.
12. **Vent jets'** sound plays once instead of looping (the mixer can't hold a loop).
13. **The assessment bars'** colours (palette bit depth), **routine 14** on level faces, **L371's lifts 6-9**
    (lift model 5 doesn't exist), the pickup counters 0x07/0x08/0x0f/0x19, the weapon noise value, the
    shatter launch speed, mouse sensitivity and two settings defaults.
14. **Open question A:** pistol rounds against a standing hugger.

### Looked at and matching
Compared line by line: the throw and its arc, the blast rings and their damage, the projectile spawn and its
side offset under the camera's roll, the flame stream's spawn, the vent jets, and the shatter pieces (types 6
and 0x0f, FUN_0002d054).

Present in the port under its own names, not compared line by line this pass: the cell actions the player
steps on (FUN_00040ff0, `Triggers::OnEnterCell`), the texture animation programs (FUN_00042ed0 and
FUN_00042d7c, `TextureAnimator`), and weapon cycling (FUN_00038d24, Next Weapon). Doors, pickups, windows
and cryopods were compared in earlier zips and confirmed in play.

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
