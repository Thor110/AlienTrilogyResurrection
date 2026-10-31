# What remains (after zip 94)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play

Confirmed by Edward: doors, pickups, the window glass, the cryopods, the older list (3.5.1, the resupply
countdown, Save Anywhere, the assessment, music per level, the vent jets), the thrown seismic charge (zip 83)
and its explosion (zip 84).

Changed in zip 94, to check: **multiplayer, players' looks, starts, shots and chat.**
- **The players are Synthetics in the original.** Every multiplayer map (L900-L909) has exactly four
  creature records, all type 0x0d, the Synthetic; FUN_0002e638's case 0xd spawns them with a task of their
  own, and in a multiplayer game types 0xb and 0x0a are left out of cell occupancy. They are the four
  player starts.
- **Player starts:** player n starts (and comes back after dying) at the nth of those records, facing its
  rotation; the records are no longer spawned as creatures. They are, cell (x,y): L900 (55,2-8),
  L901 (21-24,41), L902 (32-37,112), L903 (59,1-7), L904 (44-50,5), L905 (44-50,48), L906 (43-53,3-4),
  L907 (67,46-51), L908 (3,2-5), L909 (1-7,2). All face the same way (rotation 0).
- **Player look:** Multiplayer Options has a new last row, PLAYER: < SYNTHETIC >, changed with Left/Right
  or Enter: Synthetic (the original's), Soldier, Guard or Handler. The chosen creature turns on the right
  of the screen, a view every quarter second. It is saved (MultiplayerSkin) along with your name
  (MultiplayerName), and sent when joining; the protocol is now version 2. Room is left for more looks.
- **Other players** are drawn as that creature, with its walking animation while they move, from the view
  that matches the way they face, as a creature is (FUN_000314b8); they were a coloured column.
- **Their shots** are seen: each round fired is sent and flies on the other machines, striking the level
  (walls, glass, crates) as it does on theirs; only the machine that fired it decides whether it hit a
  player.
- **Chat:** F2-F9 send the messages set in Multiplayer Options; everyone sees "NAME: message".

Changed in zip 93, to check: **multiplayer, first version.**
- **Networking** (`src/Net`): the original's IPX packet loop (FUN_00054a60, types 4-0x16, four 0x3c-byte
  player records at 0x402b50) can't run on today's networks, so the port does the same job over UDP: a
  host (player 1) and up to three more, the original's four. Port 27960. Tested on this machine: joining,
  each player's position passed on to the others, hits, frags, a fifth player turned away, a silent player
  dropped (after 30 seconds, so the pause menu - which stops the game loop - doesn't drop anyone), leaving.
- **Start Multiplayer Game** hosts: "Start at level" 1-10 is L900-L909, "Name Of Game" is what others see.
  **Join Multiplayer Game** lists the games on the local network in the original's five slots as they
  answer, plus an ADDRESS line (Enter to type, e.g. 192.168.1.20 or a host name, :port optional) for one
  further away; Enter joins, and the host's level loads.
- **In play:** other players are drawn and can be shot; a hit is sent to the player hit, who takes a
  quarter of it through the armour, as the original's type 8 does (FUN_0003e5c4, cause 5). A death is
  scored to whoever last hit you (anything else counts against you) and you start again at the level's
  start with 100 health instead of the game ending. Joins, leaves, frags and losing the host show as HUD
  messages; losing the host ends the level. Your name is the one in Multiplayer Options.

Changed in zip 92, to check:
- **Held sounds.** A vent's jet holds its sound while it jets: steam and flame are looping samples, started
  through FUN_00040c38's mode 2 (half volume at the vent, fading to almost nothing 0x1e00 away) and stopped by
  the vent's task when the jet stops, goes out of view or range, is switched off or is knocked back to dormant
  (the original loses the sound in that last case). They play on voices of their own, at most six, which other
  sounds never cut off. They used to play once as the jet started.
- **The light's sound 0x18** (1202-nit) is not a hum: the original plays it once, on the player's channel, as
  the lamp or goggles reach full strength 32 ticks on (0x29e37), and cuts it off if the light runs out first.
  The port never played it.
- **Lift sounds** come from the lift, by distance (FUN_00040c38's mode 0, or 3 when the player's trigger set it
  off), instead of at full volume from anywhere. The original never plays the lift-running loop (3611lrun).
- **Sounds on Linux and macOS:** the sound bank's names are mixed case and the disc's upper case, so on a
  case-sensitive file system most samples were not found; the SFX folder is now searched without regard to
  case.
- **Explosions' colour** (FUN_000429f0): within 0x1000 they are drawn in (0x80, light 0's green, light 0's
  blue) - a little cool, more so in a flash - and past it faded by distance as the original does (FUN_0004ecdc
  at d * 4 - 0x1200, from about 0x11c8 to the 0x1e00 cut) instead of by the level's fog. The colour the
  original fades is the explosion task's +0xa8, which nothing sets (task blocks are reused uncleared), so the
  port fades the near colour.
- **The draw limit for models and creatures:** doors, lifts, crates, switches, pickups and creatures are drawn
  only when their cell's zones are among those the view walk took in, as every one of the original's draws
  tests (FUN_00028f00, FUN_00041ba4, FUN_0003765c and its siblings, FUN_0003c6f4, FUN_00030304). Particles
  and explosions have no such test. Off with Render Distance on, like the level's.

Changed in zip 91 (review and optimisation pass), to check:
- **Creatures' distances and bearings** are over a hundred times cheaper: the square root behind them
  (FUN_0004de04's 256-entry table) counted its root up one step at a time, up to 16,000 steps a call, for every
  creature every tick. The table is now built once; every entry is checked against the old count, and the
  emulator-checked values still match.
- **A light change** (a flicker, the lamp or goggles coming up, a flash) recolours the level's vertices from the
  light table instead of building the level again: about 0.35 ms where it was 1.3 (and 2.6 before zip 86). An
  animated texture still rebuilds. Renders after flicker, the goggles and a flash are byte for byte the same as
  before.
- A weapon whose graphic is missing is no longer looked for, and logged, every frame; the tick's cell
  occupancy reuses its buffers instead of allocating two map-sized arrays a tick.
- clang-tidy (bugprone, performance and the static analyser, headers included) over all 98 files found
  nothing else that needed changing.

Changed in zip 90, to check:
- **Modern > Level Timer** (requested, for speedrunners): the mission assessment shows "Time" (the level, from
  its first tick to the End Level trigger) and "Run" (every level played to its assessment since the game was
  started or a save loaded, failed missions included) on one line under the bars, with "press any key" moved
  to the bottom. Game time: one tick's length for each logic tick the player ran, so pauses, loading and the
  screens between levels never count. Off by default and in no preset. The times also go to the console.
  Loading a save starts a new run total; a broken run is added up by hand.

Changed in zip 89, to check:
- **Bullets** point the way they fly. They were strips centred on the round and turned a quarter-turn out, so a
  shot fired ahead lay across the screen. Now each is the original's line (FUN_0002d3e4): from the round to
  position + velocity (+ velocity/2 for the pistol), one pixel wide, coloured from the level's lights 0 and 1:
  the pistol yellow to white, the pulse rifle teal to white, the smartgun cyan to white, a creature's dark red
  to white (they were all white). The shotgun's pellets are not drawn, as in the original (FUN_0002d6ec).
  The original leaves rounds out at its lower detail settings (DAT_000a63fc: the player's at 2, creatures' at
  1 and 2, 1 being its default); the port has no such setting and draws them all, as its highest does.

Changed in zips 87-88, to check:
- **Creatures attack up close again** (zip 87): every creature but the gunners had its line to the player
  cleared each tick (the gunners' ray test's `else` sat after the ground test), so huggers never pounced and
  no close attacker reached its attack. Confirmed by Edward for the huggers.
- **Damaged level files** (zip 88): a player reported `BndParser: unexpected end of data reading tag` on L141,
  which only an empty or nearly empty L141LEV.MAP or 141GFX.B16 gives. The cause was theirs: a "play from the
  CD" install, whose level files never reached the hard drive; the archive.org copy works. Made sturdier anyway:
  the byte patcher writes beside the file and moves the result over it (writing in place emptied the file
  first, so an interrupted write could leave it empty), the installer counts a copy whose size differs from
  the disc's as failed, and a level too short to read says which file and asks for it to be copied from the
  disc again.

Changed in zip 86, to check:
- **The draw limit.** The level draws only the cells the original's view walk takes in from the camera's cell
  (0x2745b), out to its last ring (14 cells): about 4% of a level's faces in a view, against all of them before.
  Each cell's faces are its run from bytes 0-3 to the face whose +0x13 has bit 0x80 (an exact split of every
  level's faces). The cones widen to the port's wider screen, the first two rings are taken whole, and each
  ring adds a cell's width, so nothing at the screen's edges drops out; looking at a straight corridor at 45
  degrees, its far end now stops where the original's does, before the fog has hidden it. With Render Distance
  on (no fog) the whole level is drawn as before.
- **Loading:** the "loading" screen holds for one second (two before), and each model file is parsed once for
  all the models in it rather than once per model.
- **Speed:** a light or texture change rebuilds the level in one walk over the faces (three before) and sends it
  in one upload (up to fifteen); the frame, sprite and culling uploads reuse their buffers.

Changed in zip 85, to check:
- **Models lit by their cell.** Crates, switches, pickups, doors, lifts and anything thrown take the light of
  the cell they stand in (its byte 0x0e; FUN_000269b8 and its siblings), as do the creatures' sprites
  (FUN_0002fb84). They were all at full brightness. The model shader has a new tint uniform: the Vulkan and
  Metal shaders are rebuilt here; the Direct3D one is compiled from the HLSL by dxc when building on Windows.
- **The map** shows what the view walk has seen (every cell it takes in, not a 3x3 patch round the player), in
  the areas the player has stood in; the Auto Mapper shows the whole of each discovered area rather than the
  whole level (0x2745b, FUN_00043078). Every cell of L111 is in area 0, the one counted from the start, so
  there the Auto Mapper shows the whole level, as in the original. Most other levels have 1 to 21 small extra
  areas (tens to hundreds of cells each; 21 on L331), which look like the secret areas.
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
1. **Multiplayer: a working version is in (zips 93-94); see "Multiplayer" below for what is still to do.**

### Scratched (Edward): known differences, not to be done
- **The detail setting** (DAT_000a63fc, 0-2, the original's default 1, saved in its config): at 1 it leaves
  out the creatures' rounds and at 2 the player's too (FUN_0002d6ec); 2 also sets DAT_000b0cc1 bit 0x80. The
  port has no such setting and draws everything, as the original's highest setting does.
- **The pause menu's zoomed map** (FUN_00044b48's entry 0): a 28-cell window round the player. The port's
  pause map shows the whole level.

### Inferred, or the port's own on purpose
1. **HUD:** some icon artwork and the message presentation are the port's; which action prints which message
   is inferred from the text; a one-shot gate's reset isn't traced.
2. **Creature sound cues** are cut past 0x1400 or out of sight (the original only quietens them), and a
   creature let out of a sealed wall vent comes out alerted - both asked for in play.
3. **Casings** vanish at the floor; the original's fall through it out of sight.
4. The assessment bars' colours, routine 14 on level faces, L371's lifts 6-9, the weapon noise value, the
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

Done in zip 86: the draw limit (about 4% of the faces in a view), one walk and one upload per light change
(half the time it took), and the frame, sprite and culling uploads keep their buffers. Shutdown now also
frees the sprite buffers and sheets, which a later Initialize would otherwise have drawn with.

1. Texture animation: a frame change still rebuilds the level (about 1.3 ms in a release build). Light changes
   only recolour it (zip 91); moving the light colours into a small GPU table would save that copy too.
2. The frame's GPU -> CPU -> GPU copy: draw straight to the window (needs real hardware; the HUD is drawn with
   SDL's 2D renderer over the frame, so it means moving the HUD too).
3. Sprites: the render pass is still closed and reopened for their upload.

## 4b. Multiplayer: what is still to do

The networking, menus, joining, positions, hits and frags are in (zip 93); the players' looks, the
original's four starts, seen shots and F2-F9 chat in zip 94. Still to come, roughly in order:
1. **The score table and the end of a game:** "Minimum game length", the frag table, and what ends a round.
2. **Pickups, doors and lifts kept the same on every machine** - each machine runs its own level for now
   (seen shots do break the same glass everywhere).
3. **L909's sealed start room.** The traced starts are at (1-7,2), well away from it, so the room is not
   used in a multiplayer game and the temporary patch (Patch/L909LEV.txt) can now be undone. Not done yet:
   installs that already have it would need the four bytes set back, so it is Edward's call.
4. **Over the internet:** works by address if the host's port 27960 (UDP) is open; there is no NAT help.
5. **The rest of the original's packet types** (FUN_00054a60), traced one by one.
6. **More looks**, such as a Doom Guy: a new `Net::Skin` value and its sprites.

## 5. Larger features

- **Multiplayer:** see 4b.
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
