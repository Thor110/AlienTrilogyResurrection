# ALTEngine: what is left to match the original

Checked against `TRILOGY.EXE` (DOS/4GW LE: code object at `0x10000`, data object at `0xa0000`; operand values in the raw code are offsets into the data object). Most items name the original function to work from. Addresses written as `LAB_` or bare hex are timer-task handlers that Ghidra never turned into functions. Disassemble those directly.

Ordered by how much they change play.

## Checklist

Details for each item are in the numbered sections below.

| # | Item | Can be done from the binary alone | Status |
|---|---|---|---|
| 1 | One logic tick rate (find the game's clock) | yes | done: 34.87 Hz for everything (`-so`); Speed Compensation gives 69.74 Hz for the player, doors and lifts |
| 2 | Player movement exactly (`FUN_0003d85c` / `FUN_0003dc44`) | yes | done |
| 3 | Lifts (trigger commands 5 and 7) | yes | done (tested on 3.5.1: heights and platform position) |
| 4a | Creature line of fire and visibility before attacking (`FUN_0003231c`, `FUN_00033a1c`) | yes | done (zone test approximated) |
| 4b | Creature chase via the path network (state 2/3) | yes | done for the common task (see section 4 for what's approximated) |
| 4c | Creatures blocking each other (cell occupancy) | yes | done (barrel blasts still find creatures by position) |
| 4d | Hit, death and leap-end animations | yes: the scripts are in the image | done: bodies stay down on the death animation's last frame; egg, colonist and queen scripted too |
| 4e | Per-type tasks for types 1, 7, 10, 0x10-0x13 | yes | done, on their animation scripts (untested in play) |
| 5 | Flame pre-steps damaging objects | yes | done |
| 6 | Player Jumping option | modern | done (a hop; doesn't reach higher ledges) |
| 7a | Secrets counter on the assessment screen | yes | done, with the mission figure, the grade and the original's campaign order |
| 7b | Mid-level saves | modern | done: Modern "Save Anywhere" (untested in play) |
| 7c | Death clip choice | yes | done |
| 7d | Field renames in `LevelLoader.h` | tidy-up | done for lifts, pickups, crates and monsters |
| 7e | Direct-to-window rendering | needs real hardware | open |
| 7f | Multiplayer | large | open |
| A | Open question: pistol rounds against a standing hugger | needs a check in the original | open |
| 8a | Walk-through walls and floors: texture mapping (e.g. 3.5.1) | yes | done from the draw-routine table (untested on 3.5.1) |
| 8b | Breakable windows | yes | done (the break checked on the demo's L112; glass untested in play) |
| 8c | Cryopod cover (textured like the windows, not breakable) | yes | done as the see-through fill (untested in play) |

## 1. One logic tick rate

The port runs two clocks:
- 30 Hz: the player, weapons, enemies and particles. `PlayerCamera::TICK_HZ` is a guess.
- 60 Hz: doors, lights, HUD timers (including the 0x1e damage cooldown) and the motion tracker.

**How the original does it (found):**
- **The clock:** the game installs its own INT 8 handler (`0x5a87a`, or `0x5a9e2` in the second set-up path). It programs the PIT with divisor 0x42d5 / 0x42d3 (`0x5ab87`, `0x5b5ed`): 1193182 / 17109 ≈ **69.74 Hz**, the VGA refresh rate. Each interrupt:
  - increments `DAT_000b0bb8` (the game clock, used for blinking);
  - increments the frame counter `0x36415f`;
  - counts down a few real-time timers (`0x1a8178`, `0x362d3c`, `0x1a7f68`, `0xe0a6`, `0x364156`);
  - chains the BIOS clock.
- **The frame:** `FUN_00012590` (from the gameplay loop at `0x10b03`) runs the whole task list `n` times in a row, counting `0x6164` down from n to 1. The frame task (`LAB_00010230`, priority 200) only draws on the last pass (`0x6164 == 1`). There it calls `FUN_0004d2c8`, which waits while the frame counter is odd, stores counter + 1 in `0xd5d0` and clears the counter. So a frame lasts an even number of interrupts, unless it takes under one, in which case it doesn't wait at all.
- **Speed compensation** (on unless the `-so` switch is given, "Speed compensation overridden"): n = (`0xd5d0` + 1) >> 1, at least 1, so a frame of c interrupts runs c/2 + 1 passes. With `-so`, n is always 1.
- **Which systems run on which passes:**
  - **Every pass:** the player's state and movement (`0x3ed70` / `0x40040`, no gate), lifts and the other tasks.
  - **Odd passes only** (`test byte [0x6164], 1`): the common creature task (`0x342c1`), the particle pass (`0x2d929`) and part of the player code (`0x3c265`).
  - **The last pass only** (`cmp byte [0x6164], 1`): firing (`FUN_0003d5b0`), the HUD, drawing, and most sounds.
- **So at the design point** (a frame drawn within two interrupts, 35 fps): two passes a frame. The player, doors and lifts tick at ~70 Hz, while creatures, particles and firing tick at ~35 Hz. On slower frames the compensation falls short: c = 4 gives 52 Hz and 35 Hz; c = 6 gives 46 Hz and 23 Hz. On a machine that draws faster than 70 fps nothing waits, so everything speeds up with the frame rate.

**What the port does now:** by default, the original's `-so` timing: one 34.87 Hz tick for everything, creatures included (Edward, 2026: with speed compensation as the default, "everything is insanely fast"). The Modern option **Speed Compensation** gives the original's default instead, described next: the design point, fixed. A base tick of 69.74 Hz (`PlayerCamera::TICK_HZ`), with drawing free of it and the camera smoothed between ticks:
- **Every tick (69.74 Hz):** the player's movement and state, doors and lifts.
- **Every other tick (34.87 Hz, `CREATURE_TICK_HZ`):** creatures, particles, explosions and debris, the handlers' grenades, firing, the weapon's animation and its swap, thrown pickups, and the cosmetic cadences: lights, HUD timers, messages, the tracker, the face-hug overlay, the pickups' spin and the switch lights. A fire-on-press weapon's press is held over to the next such tick.
- **Modern "Faster Enemies"** (in the Modernised preset): creatures run on every tick. Particles and firing stay on the alternate tick.
- **Changes-16 was wrong:** it ran the player at 34.87 Hz and creatures at half that. That kept the 2:1 ratio but halved every creature's speed: slow huggers and drones. The creatures are at 34.87 Hz in every mode of the original (with `-so`, n is 1 and pass 1 is odd).
- **The player's walk** is 0x78 a tick (the move vector is rotated at full scale, `FUN_00026a20` / `FUN_00047e84`), so 8370 units a second at 69.74 Hz. Changes-15 to 18 had it at half that.
- **Player Jumping** is scaled to keep its height and timing: lift 0x24 and gravity 3 a tick.
- **The default, the original's own `-so` switch:** one pass per two interrupts, so every tick is 34.87 Hz and is also a creature tick. The player moves at half the compensated pace (the pace of changes-15 to 18) while creatures keep theirs. Jumping doubles its lift and quadruples its gravity to stay the same hop. Faster Enemies only changes anything with Speed Compensation on, since creatures already run on every tick without it.
- **Not reproduced:** the frame-rate dependence itself, which would only hurt on modern hardware.

## 2. Player movement, exactly

Done as `Game::PlayerMover`, used for the player in place of the old five-point test:
- **Order:** X then Z at the new X (`FUN_0003d85c` / `FUN_0003dc44`).
- **Probes:** each axis probes 200 past the target, on the player's line and 200 either side.
- **Walls:** cell +7 bit 0x80, set at load for attribute 1 and 0x14-0x18 (`0x282ed`), plus creature cells. The player's own cell is exempt.
- **Heights,** for attributes below 0x14 or 0x19-0x2c: a ceiling under Y + 0x300 or a rise over 0x100 stops the move. A stopped move slides to 200 short of the blocked cell (`FUN_0003d804`).
- **Diagonals:** walls 0x19-0x1c clamp to their own side (`FUN_0002800c` / `FUN_000280dc`).
- **Drops:** a drop over 0x100 puts the player 0xc9 / 0x138 into the low cell, clear of the lip.
- **Pushes `FUN_0003dff0` adds:** slippery ramps 0x35-0x38 push downhill, by the player's speed (at least 0x20) or 0x80 standing still. Knockback: 0x80 along the attacker's facing from a directed hit that takes health, 0x40 back from a shotgun shot, 0x20 from a grenade, fading 8 a tick.
- **Checked on L111 and L112:** every cell the old test treated as a wall but the attribute doesn't (5578 and 2446) has its ceiling at its floor, so the height tests still stop the player there.
- **Running on a slippery ramp** (`0x10ab4` bit 2, the run flag) pushes by the speed less 0x20: done.

## 3. Lifts

Done as `Game::Lifts` (see section "Settled"). Still to check against a retail map that has lifts:
- that record byte 12 indexes the `L0` meshes in file order;
- sound slots `0x5e`/`0x5f` are blank in the demo's name table;
- a fast lift (2 units a tick) with an odd travel is clamped at the ends. The original only stops on an exact match, so it would overshoot.
- **Fixed:** the record's low and high are not absolute. `FUN_00041b30` runs as the level loads: it sets low to the floor of the lift's first cell and adds that floor to high. Checked against 3.5.1's 17 lifts, where floor + travel meets the landing (64 + 32 = 96, 15/16 + 48 = 63/64). The game logs each lift's record with its own and surrounding floors.

## 4. Enemy behaviour per type

`FUN_0002e638` spawns every creature and registers a per-type task:

| Creature type | Task |
|---|---|
| Most | `LAB_000342b4` |
| 1 egg | `LAB_000356a0` |
| 7 queen | `LAB_00035bec` |
| 10 colonist | `LAB_000364d4` |
| 0x10 / 0x11 horizontal steam / flame vent | `LAB_00035aa0` / `LAB_00035ae8` |
| 0x12 / 0x13 vertical steam / flame vent | `LAB_00035b68` / `LAB_00035b28` |

Status in the port:
- **The common task** (`LAB_000342b4`, used by types 2-6, 8, 9 and 0x0b-0x0e) now runs state by state in `Screens/CreatureAI.h`: idle, chase, move, attack and act, with the flow field (`Game/FlowField.h`). What is still approximated:
  - **Animation scripts:** done (`Formats/EnemyAnimSequences.h`, generated from the pairs at `0xac3a0`: frame table, then sequence, both offsets into the `0x90000` object). For the common task's creatures the script now times the frames, cues the creature's sounds (`OP_EVENT`, `FUN_00040d44`, at `FUN_00040c38`'s distance volume), fires a gunner's rounds (`OP_SET_FLAG8`, `0x34881`) and ends the attack and the flinch (`FLAG_ENDED`, `0x348b8` / `0x34ba4`). The script only steps past the chase (`+0x6f` > 2, `0x34f8b`).
  - **Walk by view** (`FUN_000314b8`): animation = (facing octant - octant seen from) & 7, views 5-7 mirrored for all but types 1, 10 and 13. The port had played each creature's attack animation as its walk.
  - **The attack's animation** (`FUN_00033cbc`'s tables, near `0x33c04` and middle `0x33c60`): hugger 10, burster 12 (leaping at 0x40, not 0x60), bambi 12, dog 10 / 11, warrior 11 / 12, ceiling warrior 9, ceiling dog 8, guard and soldier 9, synth 16, handler 11, or its grenade throw 10 (middle band, five times in eight). A ceiling creature in the middle band doesn't attack: it idles 2 ticks (`0x33ea5`).
  - **Rounds per attack**, from the scripts: a guard's burst loops, 4 rounds; the soldier, synth and handler fire 1. A round is 1 damage, then 0x1e ticks of immunity (`FUN_0003e4a8`), so gunners are weak in the original too.
  - **The handler's grenade:** done (`FUN_0002b240`, arm `0x2dd0c`). From 0x180 up, 0x80 along the heading (0x5a when the player is higher or lower), vy 0x20; gravity 2; on the floor it bounces at 0x10, 2 less each time, then slides, halving. It goes off after 0x40 ticks (`0x2da19`), on a drop of more than 0x40 below the floor, on an attribute 0x14 cell, or on the player (0x19, cause 5, and the blast). Simplified: any object in its cell sets it off (`FUN_0002a2d8` tests heights and only blasts on a barrel). Drawn with the grenade pickup model.
  - **Flinch:** done (`FUN_000310ac`). 0x10 ticks of immunity (the queen 0x12, 0x40 while she rises), counted down at the top of each task (`0x342ce`). A hugger and gunners 0x0b, 0x0c and 0x0e flinch on 8; the warriors, burster, bambi, dog and synth on 8 seen from behind (view 7, 0 or 1) and 9 otherwise; the egg on 3 closed and 4 checking. Types 2-6 and 0x0b-0x0e are pushed away from the player at speed / 4 (`FUN_00032f78` along the bearing turned half round), and a rise is cut short. The ceiling creatures (8, 9) don't react at all. When the flinch ends (`0x34ba4`) the push stops and the creature chooses afresh: a burster backs off; anything else attacks at once at contact range or with a line, or side-steps with the attack timer at 0x3c, or idles 8 ticks.
  - **Death:** `FUN_00030b04` doesn't remove a creature. It sets state 7 and starts the type's death animation (1 egg, 9 hugger, 11 burster and bambi, 13 dog, 10 warrior, 15 queen, 8 ceiling warrior (which becomes a warrior, type 6), 10 ceiling dog (becomes a dog, 5), 25 colonist, 10 guard and soldier, 11 synth, 9 handler). The common task's state 7 (`0x34d99`) waits for the script's end (+0xb6 bit 4, `OP_END_ALT` on most) and goes to state 8; state 8 (`0x34eb3`) is the body, left on its last frame. The port now draws it so.
    - **Taken away** only in a cell of attribute 6 with under 4 units of headroom (`0x34eb6`), or at once when the killing round was particle type 0x0b, 0x0c or 0x13 (`FUN_000313b0` sets +0x6c bit 0x20, which state 7 tests first). The port fires none of those types, so the second never happens.
    - **Acid:** above Easy, a dead type 2-6 (the ceiling ones included) turns bare floor (attribute 0) under it to 0x0c, the acid floor that burns the player 1 (`0x34f60`).
    - **The death push:** done. Types 2-7 and 0x0b-0x0e fall pushed away from the player at speed / 4 (`FUN_00030b04`; BX is `+0x5e >> 2` there, not 0 as noted before), except on a ramp cell (0x2d up), until the death animation ends. A body that comes to rest on a ramp cell (0x2d-0x34) is carried down it at full speed (tables `0x34294`, `0x35bcc`: 0x2d/0x31 -z, 0x2e/0x32 +x, 0x2f/0x33 -x, 0x30/0x34 +z) and stops on the first cell that isn't one. The queen leaves acid too (`0x36455`).
  - **Types 0x0b-0x0e** use `FUN_00032aec`, the 3D ray, for their line: done (`Game::CreatureSight::LineOfFire3D`).
  - **Speeds and timers** are the original's per-tick values. How fast they look depends on the tick rate (item 1).
- **The other tasks** are now in `Screens/CreatureTasks.h` (egg, colonist, vents) and, for the queen, as a variant of the common task in `Screens/CreatureAI.h`. See "Settled" for what each does. What is still approximated:
  - **Animation events:** done. The egg, colonist and queen step their scripts in every state (`0x3583d`, `0x366b4`, `0x3646c`); the egg waits on either end bit (its idle and opening animations end on `OP_END_ALT`), the colonist and queen on bit 2. The colonist has 35 animations, not 18 (the table runs to `0xacb00`), and 25 of the port's frame records had wrong values; both are now read from the image. The queen's attack choice and hit animations are the original's (`0x33c50` / `0x33cac`; `FUN_000310ac`).
  - **The egg's line** uses `FUN_00032aec`: done.
  - **Vent jets** are drawn with their own frames (EXPLGFX records 2-8 by life plus a random 0-3, tables `0xac20c` / `0xac354`) and sizes (`0xac10c`, `0xac18c`, `0xac254`, `0xac2d4`, at the focal length 0x100), grey 0x41 + life for steam and 0x69 for flame (`FUN_0002caa8`, `FUN_0002ccfc`). They are drawn as cut-outs; the blend of the two fills they use (`0x4b400`, `0x4b5cc`) isn't settled. The vent's sound plays once as the jet starts, since the port's mixer can't hold a loop. A burst pipe shows the barrel explosion; whether `FUN_0002aaf0` also does blast damage is unchecked.
  - **The queen's death** slide and pit, and the colonist's body (animation 0x1b + view once its death has played, following the player round, `0x3667f`): done.
- **Visibility mask:** done. `FUN_00033a1c` tests the creature's cell against the zones the renderer drew (`DAT_000b0cf2`): the player's cell's zones, then the cells round the player walked ring by ring to 15 out (tables `0xa9804`, `0xabc5e`; `Game/ViewRings.h`), within 0x600 of the view yaw for ring 1, 0x384 for ring 2, 0x280 to ring 8 and 0x1b8 beyond, each cell sharing a zone with the mask adding its own if it has faces (`Game::CreatureSight::DrawnZoneMask`). The same walk marks cells visited (+7 bit 1), which the port doesn't use.
- **Leap end:** done for the common task. The leap ends when its script ends, sliding on after landing until then, as in the original.
- **Walking off a ledge:** done. A step dropping more than 0x100 below the floor the move began on (0x200 for a ceiling crawler) gives `vy = 0x3c` if it is not already moving vertically, and carries the creature at least 200 into the lower cell. A refused step slides up to 201 short of what refused it (`FUN_000315b4` / `FUN_00031ab8`) instead of stopping where it was.
- **Creature steps** now refuse a rise over 0x100, uneven ground (probes more than 0x100 apart) and a ceiling below the creature's half height (`FUN_000315f0`).
- **Hit and death animations:** `FUN_0002f4b0` starts an animation and `FUN_00030b04` is death.
- **Cell occupancy:** rebuilt every tick in the original's pattern: the player's cell is marked `0xff`, and each live creature lifts its own mark before it looks or moves, then marks where it ends up. A hugger on the player's face marks nothing: the pounce clears its cell (`0x33d3e`) and its handler never marks one. Movement and line of fire both refuse marked cells. Barrel blasts go by the marks (`FUN_000368c8`): each reached cell's occupant only - the player, or the one creature its mark names (killed unless it is the queen, and set off in a crate burst, `FUN_0002aaf0`, either way).

## 5. Weapons still approximated

- **Flame pre-steps:** done. `FUN_0002bcac` runs each of a flame's 1-2 spawn steps through the full collision test (`FUN_0002a448`), so objects, creatures and shot triggers in those cells are hit, and a blocked step leaves the flame burning there. The port now does the same with `Particle::preSteps`.
- **Shotgun shell model:** model 26 is used for the ejected shell. This is inferred, not traced.

## 6. Modern options

- **Player Jumping:** done. A Jump action (default J, in the Controls list, translated in all six string tables) gives a hop with the original's gravity (0xc a creature tick, so 3 a player tick), lift 0x24: about 216 units high. It's held under the ceiling. Movement's step-up rules are unchanged, so a jump doesn't reach a ledge more than the usual 256 up.
- **No Hit Immunity** (was "Stunned Enemies", same saved setting): every hit lands, through a creature's flinch and the 0x10 ticks after it, from rounds and from blasts alike; a creature already dying still takes nothing more.
- **All other options** are now read by gameplay or the menu.

## 7. Smaller items

- **Assessment screen:** done (`FUN_00014c68`, `Game/Progression.h`).
  - **Secrets:** a cell's byte 5 is its secret area. Each frame marks the camera's cell's area as found (`0x274e1`, table `0x245bb4`); the figure is areas 1-255 found over the level's highest area number (level record +0x76). 100 when a level has none.
  - **Aliens:** kills of types 1-14 over their totals. **Mission:** the same, over the level's objective types (5 per level, table `0xa62e9`). Totals count creature records (crate contents included) and objects below type 0x25; kills count deaths, vents switched off, switches thrown and objects destroyed.
  - **Grading** (`FUN_00014f44`): the mission figure must pass 15 (levels 0-11), 25 (12-21) or 45 (after). Failing replays the level; 85 or more before a bonus level opens it, otherwise the bonus level is skipped.
  - **The campaign order** is the table at `0xa4dec`, not the manifest's: 111, 112, 113, 122, 131, 114, 141, 115, 154, 155, 161, 162, 211, 212, 213, 222, 242, 231, 232, 243, 262, 263, 311, 321, 331, 322, 351, 352, 323, 371, 353, 324, 381, 325, 391. The bonus levels are 113, 114, 115, 155, 242, 243, 352 and 353.
- **Saves:** done as the Modern option **Save Anywhere** (`Screens/LevelSnapshot.h`). Off, Save Game checkpoints the start of the level, as before. On, opening the pause menu captures the level into the checkpoint's `snapshot`, which loading puts back over the freshly built level:
  - **Kept:** the whole cell grid (16 bytes a cell, so doors, lifts, marks and anything a trigger rewrote), each door's and lift's progress, every trigger's uses, every pickup (taken, still in its crate, waiting on a trigger, where it lies), every crate and barrel (health, broken), the switches, the minimap, the player (position, facing, look, health, armour, items, weapon) and every creature (position, facing, health, dead or alive, alerted, an egg's, vent's or colonist's own state, its animation, a body taken away), and the texture and light changes switches and breakages made.
  - **Not kept:** rounds and other particles in flight, explosions, debris, sounds, a creature's attack, leap, flinch or face hug in progress (it comes back idle and decides again), and the level's light flicker.
  - Each piece is only restored when it matches the level as built (the same number of doors, creatures and so on), so a save from an edited level can't put anything out of range.
- **Death clip choice:** done. The causes are the creature attack's, by type (`FUN_00033ff8`, tables `0x33f60` / `0x33fac` by type - 1): huggers and bursters 4 (DEATH1), bambi and dog 1 (DEATH5), warrior drone 0 (DEATH2), queen 2 (DEATH3). Cause 3 is a creature's projectile (particle type 5), and 5 a blast or a vent. The port passed 4 for every creature. Types 8 and up have no arm there at all.
- **Field names:** renamed. Pickups: `triggerCount` / `triggerThreshold`. Crates: `flags` / `healthDiv16`. Lifts: `width`, `depth`, `low`, `travel`, `flags`, `autoDwell`, `dwell`, `threshold`, `counter`, `model`. Monsters: `coneHalfWidth`, `wakeRange`, `speedHigh`. A monster's byte 6 keeps the name `drop`, with a note that its meaning depends on the type.
- **Rendering:** each frame is still copied GPU → CPU → GPU (`ModelRenderer::RenderLevelToRgba`). Drawing straight to the window would remove a full-frame stall, and needs testing on real hardware.
- **Multiplayer:** the menus exist, but there is no networking.

- **CD music on Linux and macOS:** done (`Formats/CddaRipper.cpp`). Linux reads the drive the disc is mounted from (`/proc/mounts`) with `CDROMREADTOCHDR` / `CDROMREADTOCENTRY` / `CDROMREADAUDIO`. macOS converts the tracks its own `cddafs` mounts; failing that it reads `/dev/rdiskN` with `DKIOCCDREADTOC` / `DKIOCCDREAD`, which normally only root can open. Checked against Apple's cddafs source: it mounts any disc with an audio track (this disc included), names each file after its real track number (`2 Audio Track.aiff`), and serves AIFF-C `sowt`, already little-endian. The drive code is untested on a real drive on all three systems; macOS's compiles in CI.

## 8. Reported from play, after the rest of the list

All three came down to the level faces' draw routines, which the port mostly ignored. A face's +0x12 byte picks one of 35 routines (table `0xa7098`, `Formats::ClassifyDrawRoutine`):

| Routine | Shape | Culled | Mirrored | Fill |
|---|---|---|---|---|
| 0 / 1 / 2 | quad / tri / quad | yes | 2 only | opaque, lit |
| 4 / 5 / 6 | quad / tri / quad | no | 6 only | cut-out: key texels not drawn, lit |
| 12 / 13 / 34 | quad / tri / quad | no | 34 only | see-through: key texels not drawn, the rest half and half with what's behind, unlit |
| 14 | | no | | flat, no texture (drawn textured in the port). Its primitive (`0x49dd0`, span mode 6) fills with CLUT index 0x64 of the screen palette at `0x361cf0`, not the face's colour; which palette that is in play isn't traced. None of 263, 351, 371, 391, 111 or 112 uses it. |

- **Animated forms:** 8, 10, 26, 28, 30 and 32 are 0, 2, 4, 6, 12 and 34 with the texture taken from the texture animator (the face's texIndex is an animator number). The port only did this for 8, so the others showed whatever descriptor had that number. **Every other value draws nothing** (3, 7, 9, 11, 15-25, 27, 29, 31, 33); the port drew them.
- **Transparency is the face's, not the page's.** The cut-out and see-through spans skip a texel whose CLUT word is 0 (`0x5d77e`, `0x5db67`); opaque faces draw every texel. In the demo's pages the key in practice is magenta (index 0, 0x7c1f): L111's cut-out faces are half index 0 and hold no word-0 texel. The port now keys on both, and opaque faces ignore it. The old per-level index table (`LevelTransparency.h`, now unused) applied to every face of a page, which punched holes in opaque faces (index 255 is 0x0001, a near-black).
- **Breaking:** object types 0x15, 0x1c and 0x1d have no model (`FUN_00037184`). They stand in the window or panel, block its cell and take the shots. Destroying one steps each of its texture animators by one (`FUN_00037ff8`; first and count are the record's u16s at +10 and +12), which shows the broken frames. Checked on the demo's L112: crate 13 (type 0x15) steps animators 21-28, the eight routine-26/28 louvre faces next to it, from intact to broken. The port had drawn these objects as crates and never stepped the textures.
- **Saves** now keep the switches' and breakages' texture and light changes (replayed in order).
- **Untested:** 3.5.1 itself, and the see-through fill: the demo levels have no routine 12, 13, 30, 32 or 34 face.
- **Fog:** 3.5.1's fog is routine-30 faces on one animator (descriptors 176-183, a frame every 5 ticks), drawn with the see-through fill, which reads the texel's own colour and never the light, so no distance fade (`0x5db67` takes table `0x2273c8`; the fade, `FUN_0004ecdc`, works on the light colour). Its CLUT is the face's own (descriptor +0x12, patched into the span at `0x5dbd4`). The original's fog looks green and without visible texture; why is still open.
- **Level data errors fixed** (`data/Patches.json`, `Patch/`, ALTViewer's fixer): 3.5.1's resin wall top row (two corner faces a quarter-turn out, two centre faces mirrored the wrong way round), and 3.9.1's egg sack inner layer (246 faces a quarter-turn out: in 2.6.3 each strip runs into the next at its bottom edge, in 3.9.1 they ran around the sack). Found by checking which way the texture runs against the faces next to it; the same check could look for others.

**Object models** (`FUN_00037184`): the model pointers at `DAT_00247b84` are OBJ3D's models in order. Type 0x14 is the crate, or on episode 3 (campaign index 0x16-0x22, and 908/909 by the table at `0xa706b`) the egg husk (41). The switches: 0x16 small battery switch (11), 0x18 small switch (3), 0x1a large battery switch (15), 0x1b large switch (7), and on episode 3 the boneship switches (27, 19, 31, 23). 0x17 barrel (0), 0x19 double crate (2), 0x20 stasis pod cover (40), 0x21-0x25 models 35-39, anything else the crate.

## Settled this round (see the code comments for the evidence)

- **-GAP and level override geometry never loaded:** both looked for the level's page textures before they had been uploaded. The pages now upload first.

- **Creatures could only be hurt once:** a hit set the creature's hit-reaction cooldown and nothing counted it down. Anything that survived its first hit (a warrior drone has 30 << 4 = 480 health against a pistol round's 32) was then immune. It now counts down each tick.
- **When a creature's hit lands** (`FUN_00033ff8`): the player must be in a zone it sees and inside its cone (`FUN_00033a1c`). At contact range that's enough; in the near and middle bands it also needs a clear line, and does the close or middle damage. There's no vertical test. The port had needed contact range, a clear line and a vertical limit all at once, so warriors rarely hit.
- **Gunners** (types 0x0b-0x0e) have no arm in that routine. Their harm is a round (`FUN_0002af8c`, particle type 5): from 0x2c0 up for a guard and 0x240 for the others, aimed 0x280 above the player's feet, 0x80 a tick, 0x40 ticks of life, 1 damage, cause 3. It's stopped by walls (`0x2de7e`).

- **The egg** (`LAB_000356a0`): its own thresholds (0x800 near, 0xc00 middle) replace the record's, and its record's byte 6 is the monster it holds. Closed, it rechecks every 0x40-0xbf ticks. With the player in the middle band it opens (animation 2), and once open, with the player in the near band and a clear line, it lets the hugger out and dies. The hugger (`FUN_00035530`) appears at the egg, alerted, facing the player and already leaping: 0x40 along, 0x60 up. A hit opens a closed egg.
- **The colonist** (`LAB_000364d4`): a captive in the wall, pushed 0x40 out along its facing and 0x40 up. It turns its head to whichever eighth the player is in (animation 8 + view) and cries out (sound 0x6e) in the middle band. Close and straight in front (view 4, near band) it plays 0x1a and the chestburster its speed word names comes out (`FUN_00035624`: 0x60 along, 0x78 up, animation 0xd). Its byte 6 pickup is thrown out (`FUN_0003bc9c`).
- **Vents** (`FUN_000358a4` and the jet arms of the particle pass):
  - **Timing:** a cycle of the record's bytes 16/17. Each cycle the jet is off for the speed word's length, starting at bytes 18/19. A jet goes out every 3 (or 4) ticks: steam 0x0e / 0x12, flame 0x10 / 0x11, at 0x3c / 0x78 / 0x5a / 0x41.
  - **Damage:** a jet touching the player (0-0x400 above the feet, 0x100 across) does 5, or 0xa for horizontal flame, with cause 5.
  - **Off:** a vent only runs while its zone is in view and the player is within 0x1e00. A negative spawn-monster trigger takes it below its threshold and switches it off for good, with "Steam valve closed" / "Flame jet shut down".
  - **Pipes:** a vent with health is a pipe that bursts and starts jetting when shot. One with none can't be hit, since it is never marked in its cell.
  - **Placement:** vents face away from their record's heading, and byte 6 sets their height (from the ceiling when negative, jetting down).
- **Monster triggers** (`FUN_0002f224`): the amount is a signed byte and moves the count of an active monster too. A count at its threshold takes no positive amount.
- **The queen** (`LAB_00035bec`): the common task with an intro in IDLE (animations 9, then 10 or 8), a chase that only alerts, attacks without the vertical reach test, and her own hit reaction. Her death runs the command chain her record's byte 6 names (`FUN_00040e2c`, now `Triggers::System::RunCommands`).

- **Lifts** (`FUN_000423e8` setup, handlers `0x42238` idle, `0x420ec` ascend, `0x41fa0` descend, `0x41ed0` wait; now `Game::Lifts`):
  - **Record:** x, z, width, depth, low, high, flags, auto-dwell, dwell, threshold, counter, model. On disc low is 0 and high is the travel; at load `FUN_00041b30` adds the first cell's floor to both.
  - **Floor:** every cell of the lift's w×h area gets its floor byte (+0xb) set to the height each tick (`FUN_00041e80`), so the player rides it with no extra code.
  - **Starting it:** command 7 (`FUN_00041dd8`) adds to the counter; command 5 (`FUN_00041e20`) adds only as the final increment. Both refuse while the lift moves or once the counter has reached the threshold. At the threshold the idle lift sets off (sound `0x5e`) and stops with `0x5f`.
  - **First run:** sets flag 0x80, adds one to `DAT_000b0b2a` and shows the lift notice. No other direct reference to that counter was found.
  - **Cycling:** a non-zero auto-dwell makes the lift wait and go back by itself.
  - **Returning:** without that, a lift with a dwell waits at the far end, goes back to where it started, and is idle there. Arriving at the top checks flag 0x20 the opposite way to arriving at the bottom.
  - **Drawing:** `FUN_00041ba4` only draws the platform, unrotated, at the centre of the lift's first cell (x·512 + 0x100, height·32 + 0x10, z·512 + 0x100: record words 0, 2 and 4). The block's centre (x·512 + w·256, words 0x1c/0x20) and half the mesh's height (word 0x1e) are only for its visibility test.

- **Creature AI, common task** (`LAB_000342b4`):
  - **Waking:** creatures idle for 0x20 ticks, then chase.
  - **Noticing you:** within half their wake range (the record's bytes 12/13) while in view, or within the whole of it while your weapon is loud (`DAT_000b0adc`). Being seen inside their cone with a clear line also alerts them.
  - **Alerted:** they head for a random point 0x400-0x5ff from you with a clear line to it (`FUN_00032fcc`). Failing that, they take the next path node toward you, then a side-step.
  - **Moving:** they turn 0x200 a tick and advance only when facing the waypoint, at the record's speed word (+0x14 for huggers).
  - **Attacking:** they attack whenever you're in reach with a clear line. A hugger retreats down the flow field after its leap; other types attack again while they can.
  - **The flow field** is a breadth-first fill from your path node (`FUN_000281cc`). Each cell's byte 6 names its node, and node byte 3 holds two half-cell offset flags, not a state.
- **The 0x1e00 "do nothing" cutoff** belongs to other tasks. The common task keeps running and only stands still beyond 0x3fff.

- **Creatures need a clear line to attack** (`FUN_0003231c`, now `Game::CreatureSight`):
  - **Probes:** two, 200 either side of the line to the player, in 0x100 steps, distance >> 8 of them. The 0x400 side goes first.
  - **Refused by:** walls, occupied cells (the creature's own cell, and so a shared one, excepted), low ceilings, rises over 0x100 and drops over 0x400.
  - **Checked:** against the original routine run in an emulator on about 8,000 random rooms, with no differences.
- **The original's integer maths** (`Game::OriginalMath`):
  - **Bearing:** `FUN_00026b64`, an arcsine table, up to 4 units off a true atan2.
  - **Square root:** `FUN_0004de04`, a table plus recursion, which runs low for large values.
  - **Distance:** `FUN_000320f8`.
  - **Yaw rotation:** the one `FUN_00026a20` performs.
  - **Use:** creature distance and bearing now use them.
  - **Sine table:** the port's own is now the original's (data `0x7804`), dumped into `Formats/OriginalTables.h` with the arcsine table. A rounded sine differs from it in 41 entries.
- **The player's cell really is marked every tick:** the mover clears it at the start (`0x3e008`) and sets it at the end (`0x3e3d6`). A creature in the same cell is not held by it, because the creature lifts its own cell's mark first.

- **Weapon slots 3 and 4 were crossed.** The original's internal slot (`DAT_000b0aae`) is 0 pistol, 1 shotgun, 2 pulse rifle, **3 smartgun, 4 flamethrower**:
  - **Graphics:** `FUN_000401d0`'s jump table at `0x401bc` sends slot 3 to the table at `0x0ace88` (smartgun sounds) and slot 4 to `0x0ace70` (flamethrower sounds). An earlier reading took the cases in address order.
  - **Firing:** `FUN_0003d5b0` fires slot 3 through `FUN_0002c1d4`, using `acc`, which pickup 4, "Smart Gun", fills. Slot 4 fires through `FUN_0002c0a0`, using `aca`, which pickup 3, "Flamethrower", fills.
  - **Keys:** the select keys (pistol, shotgun, pulse rifle, flamethrower, smartgun) map to slots 0, 1, 2, 4, 3. The restore code converts with the same 3/4 swap (`0x17bd5`).
  - **The port's mix-up:** firing and the weapon table were per slot, but graphics, names and pickups were in key order. So each of the two weapons fired the other's rounds, and slot 2 drew the pulse rifle's animation over `FLAME.B16`.
- **Weapon switching** (`FUN_0003efcc` at `0x3f0bc`):
  - **Lower and raise:** state 4 lowers the weapon (`DAT_000b0b57` + 0x10 a tick to 0x80), the swap happens at the bottom, and state 5 raises it again. Eight ticks each way.
  - **Running dry:** the weapon empties and switches to the highest other slot that still has ammo (`FUN_00038e64`). Only when there is none does the pistol get its single-round reload (`FUN_00038c78`).
  - **Selecting:** choosing a weapon or next-weapon skips one that is held but empty (`FUN_00039004`, `FUN_00038d26`).
- **Flamethrower particles** stop where a wall or creature blocks them and burn on there. That is the type 3 arm of `0x2da89`, and `FUN_0002bcac` makes the same test at each pre-step.
- **Casings:** only the pistol (0x15) and the shotgun (0x14) eject one (`FUN_0002b37c`).
- **The smartgun aims itself** (`FUN_0002c12c` / `FUN_0002c1d4`):
  - **Targets:** up to three creatures within 0x1c00, inside ±0x80 of the heading, in a visible zone and in state ≤ 5, nearest first.
  - **Aimed rounds:** each starts 0x100 below the eye and 0x190 ahead, and flies at its target's Y + half its box's half height.
  - **Leftover shots:** go straight ahead as type 4.

- **Melee creatures leap** (`FUN_00033cbc`, called from creature-task state 4 at `0x3476e`):
  - **Entering the attack state:** the chase (state 2) enters state 4 only at mode 0 and within 0x600 vertically. State 4 returns to the chase at mode 3.
  - **When it attacks:** in state 4, once the creature faces the player exactly, is in a mode below 3, is within 0x400 vertically, and its attack timer has run out (reloaded to 0x3c).
  - **Face hug:** a face hugger in mode 0, within 0x600 vertically, with the lock free, face-hugs.
  - **Leap:** otherwise it leaps, with horizontal 0x60 along its heading (`FUN_00032f78`), vertical 0x80 and gravity 0xc (`FUN_00031f3c`). Type 3 always leaps.
  - **Landing:** the hit is the ordinary attack check (`FUN_00033ff8`), run when the leap ends. A hugger then goes back to the chase; type 3 stays in state 4.
- **Spawn clamps** (`FUN_0002e638`, every type except 0x10-0x13): the mode-1 threshold is at least 0x600. The attack cone is the record's bytes 10/11, clamped to 0x100..0x800, with anything wider set to 0x700.
- **Rounds converge** (`FUN_0002b534`): each round gets a yaw of its own before the view's, turning it back across the muzzle offset. Pistol and shotgun 0x28, pulse rifle 0x30, the smartgun's unaimed type 4 none.
- **Level fire passes over a standing hugger.** This is checked, not assumed:
  - **Rounds:** they start 0x60 below the eye (floor + 0x2a0) and are pitched by the view. Running the original's rotate routine (`FUN_00026a20`) in an emulator at the rest pitch (0x30) gives vertical velocity +18 per 256 forward, about 4° up. Its camera (`FUN_00026ab0` and `FUN_0004884c`) looks up by the same amount, so rounds fly to the screen centre.
  - **Hugger:** a standing hugger's box is floor + 0x20 ± 0x200, so its top is floor + 0x220. Nothing else lowers a round: no gravity in the batch mover (`FUN_00047524`), and no steering in the round handler.
  - **Shotgun:** it doubles the box, which covers a standing hugger out to about 5000 units.
  - **Open question:** if pistol rounds hit standing huggers in the original, something here is still missing.
- **Creatures can't step into the player's cell:** the player stamps `0xff` into its cell, and `FUN_000315f0` refuses any occupied cell. So a leap stops at the player instead of sailing past.
- **Creature mode thresholds:** the creature task reads them as shorts at `+0x4a` (mode 1) and `+0x48` (mode 2). The perception routine is `FUN_00032154`, and `+0x14` is the vertical separation.

- **Eye height and field of view:**
  - The standing eye is floor + `0x300`, set by player setup at `0x4078f`; the player's Y is the floor itself (`FUN_0003dff0`, `FUN_0003d00c`).
  - `0x180` is where the death sequence (`FUN_0003ee4c`) leaves the camera, not the standing height.
  - Look Up / Look Down are the original's own keys (`FUN_0003efcc`, ±`0x80`). They're needed to shoot anything on the floor without Free Look.
  - The view is 320×240 with focal length `0x100`, so the vertical field of view is ≈ 50.2° (`FUN_0004884c`, `FUN_00010e38`).
- **Rounds:**
  - Nothing steers a round after launch (particle task `0x2d91c`; type 0–4 handler `0x2da89`).
  - Each tick: cell test `FUN_0002a448`, then creature test `FUN_0002a628`, then shot triggers `FUN_000414f0`.
- **Creature hits:**
  - Hit boxes are per type and centred on the creature (`FUN_0002e638`). The shotgun doubles them.
  - Creatures are hittable only while in state < 6, and only when the round's cell and the creature's cell share a zone bit.
  - A hit sprays blood (`FUN_0002af24` / `FUN_0002ada4`). Acid blood burns the player (`FUN_0002a8f8`).
- **Objects:**
  - Health is record byte 4 × 16, and the indestructible flag is bit 0 of byte 3 (`FUN_00037184`).
  - Damage to objects isn't doubled.
  - The pistol can't hurt barrels.
- **Player damage:**
  - All damage goes through `FUN_0003e4a8`: 0x1e cooldown, terrain shield, and armour takes the whole hit.
  - Pits use `FUN_0003e3e4`, which skips armour.
  - Harmful floors: attribute 7 = 5, 8 = 10, `0xc` = 1, 10 = all health.
  - Barrel blasts deal 50 in the inner ring and 25 in the outer, and kill creatures in either ring except type 7 (`FUN_000368c8`).
- **Trigger commands:**
  - Command 2 spawns a pickup (`FUN_0003bb14`).
  - Command 9 is an add-and-clamp on a 16-byte animator record (`FUN_00042fcc`).
  - Shot-activated triggers have flag `0x02`.
- **Weapons:**
  - The flamethrower puts 2 particles out per pull, launched from eye − `0x80` + jitter, `0x38` to the side (`FUN_0002c0a0`, `FUN_0002bcac`).
  - Each pulse round costs one (`FUN_0002bc18`).

## Still open (creatures)

- **The ceiling creatures:** done. Types 8 and 9 spawn with `+0x6c` bit 8 (`0x2eb43`, `0x2ebab`) and hang 0x20 under the ceiling (`FUN_00031f3c`); their mover measures the ceiling, refusing a fall of it over 0x200 or probes more than 0x200 apart, with no headroom test (`FUN_000315f0`), and their line follows the ceiling (`FUN_0003231c`). They let go only by attacking from the contact or near band (`FUN_00033cbc` -> `FUN_0002f1b0`: the ceiling warrior becomes a warrior, 6, playing its animation 9, the dog a dog, 5, playing 8; from the middle band they idle 2 ticks instead) or by dying (`FUN_00030b04` clears the bit; they keep their own death animation). Nothing in the original climbs walls: the mover knows only the floor and the ceiling. If they seemed never to come down in the original, it is that they wait until the player is within the near band (about 0x600).

## The level UV scan

`ALTLevelScan <CD folder>` (built with the engine, `src/Tools/LevelScanMain.cpp`, `Formats/UvSeamScan.h`) looks through every level for faces textured the wrong way round. Where neighbouring faces show touching pieces of one picture on the same page, both should put each shared vertex on the same texel; a face that doesn't, and that one of its other orientations would fix, is reported, strongest first, with a `faceUvRotations` entry in Patches.json's form. Faces Patches.json already turns are judged as turned. On L391 it names 162 of the 246 inner egg sack faces fixed by hand, each with the same correction, and 7 faces on the fixed file; L263 shows 5, L351 82 and L371 51, not yet looked at.

