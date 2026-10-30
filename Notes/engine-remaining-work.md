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
- **Modern "Faster Xenomorphs"** (in the Modernised preset): the aliens (types 1-9) also take every other one of the ticks the original skips, so they run at 1.5 times its rate (every tick, double speed, was too fast in play; `FASTER_XENO_OFF_PASS_EVERY` in `GameplayScreen.cpp`). The colonist, guard, soldier, synth, handler and the vents stay on the alternate tick, as do particles and firing.
- **Changes-16 was wrong:** it ran the player at 34.87 Hz and creatures at half that. That kept the 2:1 ratio but halved every creature's speed: slow huggers and drones. The creatures are at 34.87 Hz in every mode of the original (with `-so`, n is 1 and pass 1 is odd).
- **The player's walk** is 0x78 a tick (the move vector is rotated at full scale, `FUN_00026a20` / `FUN_00047e84`), so 8370 units a second at 69.74 Hz. Changes-15 to 18 had it at half that.
- **Player Jumping** is scaled to keep its height and timing: lift 0x24 and gravity 3 a tick.
- **The default, the original's own `-so` switch:** one pass per two interrupts, so every tick is 34.87 Hz and is also a creature tick. The player moves at half the compensated pace (the pace of changes-15 to 18) while creatures keep theirs. Jumping doubles its lift and quadruples its gravity to stay the same hop. Faster Xenomorphs only changes anything with Speed Compensation on, since creatures already run on every tick without it.
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
- **No Hit Immunity:** every hit lands, through a creature's flinch and the 0x10 ticks after it, from rounds and from blasts alike; a creature already dying still takes nothing more.
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
- **Fog:** 3.5.1's fog is routine-30 faces on one animator (descriptors 176-183, a frame every 5 ticks), drawn with the see-through fill, which reads the texel's own colour and never the light, so no distance fade (`0x5db67` takes table `0x2273c8`; the fade, `FUN_0004ecdc`, works on the light colour). Its CLUT is the face's own (descriptor +0x12, patched into the span at `0x5dbd4`). The original's fog looks green and without visible texture; why is still open. Checked further: routine 30 goes through routine 12's handler to span mode 4 (`0x4b5cc`, table `0xad554`), which reads the face's own CLUT (+0x12 << 0xb) through the split table at `0x2273c8`. That table is a plain copy of the CLUT after the BGR-to-RGB swap (`FUN_00049178`), the same colours the lit tables use. The fog's five colours are all dark yellow (5-bit 8,8,0 with the 0x8000 bit set), so by the code the fog is dark yellow at 50%, as the port draws it. The video (level 27, `screenshots/351-fog-original-vs-port.jpg`) shows it green, which a video colour shift doesn't explain. Span mode 6 (`0x5eceb`) is a flat see-through fill (colour index 2 of CLUT slot 0) but nothing routes routine 30 there.
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
  - **Open question:** if pistol rounds hit standing huggers in the original, something here is still missing. Checked again: the rest pitch 0x30 (`0x406ec`) tilts the view up (the camera's forward axis is (0, sin p, cos p) with the original's own matrix code, `FUN_00026d1c`, and floor heights grow upward, `FUN_00027e28`), rounds leave along the same axis (`FUN_0002b534`: -pitch, then the view's yaw and roll), the round handler never steers, and the hit test (`FUN_0002a628`) is a plain box. The per-weapon spawns (`FUN_0002bb74`, `FUN_0002bbc0`) don't aim. Nothing found that pivots toward a target; the smartgun is the only weapon that aims.
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

## Fixed after play-testing 0.9.0

- **Wall blocks:** a window, breakable wall or curtain (0x15, 0x1c, 0x1d) stamps a block of cells, record byte 8 along x and byte 9 along z from its own (`FUN_00037084`), and its destruction clears the block (`FUN_000370d4`). The port stamped one cell, so L231's grenade wall (a 0x15, 2 x 5) could be walked through. A blast reaches the object through any cell of its block.
- **Objects stop creatures:** the creature marks now start from the objects' stamps, so crates, barrels and wall blocks stop creatures and their lines as they do in the original (`FUN_000315f0`, `FUN_0003231c` refuse any non-zero +0x0c).
- **Raw sprite frames:** a creature frame that compression wouldn't shrink is stored as it is, width x height bytes running exactly to the next frame or the section's end. The burster's two death animations end on such a frame (its body), so they never decoded and it died in its walk pose. Its animations 14-18 still don't decode: their records give other animations' offsets with the wrong sizes, and nothing is known to play them.
- **A game sound never overlaps itself** (`FUN_0005aed9` won't start a sample still playing on any channel); the menus' ticks are exempt.
- **Animated faces changing texture page** rebuild that page's buffers instead of being skipped (L212).

## Zip 34

- **Rate of fire is the same at any frame rate.** The original fires (`FUN_0003d5b0`) and steps the weapon's animation (`FUN_0003e93c`) only on a frame's last pass (`0x6164 == 1`). Its speed compensation (`FUN_00012590`: n = c/2 + 1 passes for c interrupts, c rounded up to even by `FUN_0004d2c8`) gives the player and creatures extra passes on a slow frame but not the weapon, so the original's rate of fire followed the machine's frame rate; the gameplay frame has no other limit (`0x3602d4`'s 3-interrupt wait is the text screens'). The port now runs firing, weapon switching and the weapon's animation on their own clock, one pass per `WeaponFrameInterrupts` timer interrupts (config key, 1-8, default 3, about 23 fps; 2 is the 35 fps design point the port used before). The shotgun's shot is 16 such passes (7 frames of 2, the end, one at rest), so at the default about 1.45 shots a second, against 2.2 before.
- **Texture scan reviewed:** every visible candidate in LevelUvScan.json rendered before and after (`screenshots/`, see its README). The original mapping looks right in nearly all of them; no patches added. 909-7 and 909-9 to 909-16 are worth a look in game.
- **Tidy-up:** removed the one-shot light scanner (`Tools/LevelLightScanner`, `data/ScanLights.flag`), the unused `LevelTransparency` files, `ComputeVertexBounds` and `IsWallLegacy`; no compiler warnings left on GCC or Clang; vcpkg.json's version follows VERSION (0.9.0).

## Zip 35

- **Texture scan fixes kept** (`data/Patches.json`, notes "texture scan <level>-<fix>"): the 70 groups picked from the before/after review, plus three settled by hand: 154-1's pipe bend takes a quarter turn and a flip (the scan's three-quarter turn left a seam; the eight orientations were rendered side by side and only this one runs on), 905-1's neighbour one panel across and one down (face 1667, the only mirrored face in its column) is flipped like 905-1, and 311-9's floor panel, which shows in 311-61's view, is kept in place of 311-61's light strip. 311-103 changes only a light strip and is left out. 311-1 and 908-1 (the same faces; 908 reuses 311's geometry) keep only their floor panel: the light strip next to it in the same group is left as it is. `screenshots/kept` and `screenshots/not-kept` hold the renders; the kept ones are re-rendered with exactly these patches.
- **The 3.5.1 fog's palette, traced to the end.** The level's five CLUTs go to slots 0-4 of the pool at VRAM x 0, y 480-511 (`FUN_00011fc0` with the record at `0x4d68`; the slot is y - 0x1e0, `FUN_0004e324`, the same formula the copy uses, `FUN_0004e368`), and each descriptor's CLUT is the slot of its page (`0x18e29`). The only other writer to that pool is the map screen, slot 31 (`0x430a5`). `FUN_00048fe8` swaps every CLUT from BGR to RGB and builds the lit tables and the split table from the same words. So page 2's fog reads CL02: indices 147-177, all 5-bit (8,8,0) or within one step of it, with the semi-transparency bit set - dark yellow, half blended, as the port draws it. Nothing in the code gives it green, and the user's 351GFX.B16 matches the levels.zip copy byte for byte. **Settled:** checked in DOSBox, the PC version's fog looks as the port draws it; the green was the video's low colour depth.

## Zip 37

- **The texture scan fixes are now file patches**, like 3.5.1's resin wall and 3.9.1's egg sack: 92 faces in 20 levels, one byte patch per level in `data/Patches.json`, the same edits in ALTViewer's fixer, and each listed in `Patch/LxxxLEV.txt` (face, offset, scan code, the change, the vertex lists before and after). A turn rotates the face's vertex list (W[k] = V[(k + steps) mod n], winding kept); a mirror swaps the flag byte for its mirrored routine (0/2, 4/6, 12/34, 8/10, 26/28, 30/32). They left `faceUvRotations`, so nothing is turned twice. Its last two entries, L111's triangle at 0x46DB4 and its starting lift panel (face 10208, a mirror), became byte patches for L111 and L900 alike. The triangle's quarter-turn was dropped in zip 46 and put back in zip 50 (see "Triangles, traced" under zip 50). The list is empty; the loader and ALTLevelScan's suggestions still use its form.
- **ALTViewer's export-time UV fixes are now file patches.** ALTViewer's OBJ export used to correct 69 faces on its own (L111/L900 crates, L141/L903 crates, L161's secretion wall, L906's bridge), so neither the game nor the engine ever saw them. Each is now a byte patch in `data/Patches.json` and ALTViewer's fixer: a texture index, a mirror (the flag's partner) and/or a vertex-list rotation, found by search so that the engine's UVs equal what the export wrote, vertex for vertex. 84 face fixes over six levels (L900 and L903 reuse L111's and L141's faces at the same offsets); L141/L903 face 4449 was already right. Checked by running the engine's removal and patch runners on the real files: every face matches the old export, no other face changes, and a second run reports them applied. The export code is gone, so ALTViewer now exports exactly what is in the file. Each face, with its old export remap, is listed at the end of `Patch/LxxxLEV.txt`. Checked by rendering the byte-patched maps without the rotations: 68 of 72 views match exactly, the other 4 differ by a few hundred pixels at most where a quarter-turn moves the quad's split diagonal, which is what the original draws from the file.
- **Patches added to an already-patched file were skipped.** PatchLoader merges a file's patches into one operation and PatchRunner tested only its first edit, so an install made with an earlier Patches.json counted as fully patched. It now checks every edit against the bytes the file should end up with (later edits winning, as L906LEV 0x22BF9 is written twice). Tests: `tests/PatchRunnerTests.cpp`.

## Zip 39

- **Duplicate and unused door/lift models are removed** (Notes/duplicate-level-models.md): 38,316 bytes from 21 levels, by a new guarded patch kind, `modelRemovals`, run before the byte patches in the install step and in ALTViewer's fixer. L162's D001 fix moved to 0x3205C. ALTViewer's fix button now also shows for installs patched by an older ALTViewer.
- `LevelGeometry` records where the door and lift records start (`doorRecordsPos`, `liftRecordsPos`).

## Zip 40

- **903's walk-through pillars.** 903 is built on 141 (the same 105 x 81 grid at the same place, over 5,000 identical cells, the same geometry). Four of its cells lost their attribute byte on the way: the pillars at (11,43) and (11,48), with walls on all four sides, and (12,42) and (49,59), walled on one side. 141 has them as 0x14/0x15, which block. Restored by a byte patch (Patches.json, ALTViewer, `Patch/L903LEV.txt`). A scan of all 45 levels for walkable cells walled in on four sides finds no others.
- **The resupply levels' countdown** (`Game/ResupplyTimer.h`). `FUN_0003b518` gives the eight bonus levels a task holding the seconds left: 30 for 155, 45 for 352 and 353, 60 for 113, 114, 115, 242 and 243. A second is 0x46 timer interrupts (the real-time word `0x1a7f68`). The task prints the seconds in the HUD font at (16, 19) (`FUN_00043720`, font A), steady above 17 and blinking on bit 3 of its pass count from 17 down. It sounds cues on the seconds that change: 20 and 17 the level's own (0x64 "emergency" on 115, 155, 242 and 243; 0x65 "attention" on 113 and 114; 0x62 klaxon on 352 and 353), 14 0x63 "all personnel" (0x62 past index 21), and 11, 8, 5 and 2 0x62 past index 21. At zero it sets `0x10cc0 |= 4`, the level-done bit, which goes to the assessment like the end-of-level trigger. The colours are inferred: the text colour comes from a run-time table at `0x1a58dc` (bytes 0 and 1, as (0, b1, 0) steady and (b0, b1 or 0, 0) blinking), drawn here green, then red/yellow.
- **The assessment screen's graphics** (`Screens/MissionAssessmentScreen.cpp`, `FUN_00012abc` each frame). Everything sits on a 320 x 240 screen, scaled up as the menus are.
  - **Background:** LOGOSGFX's second picture (TP02 + TP03, pieces 7 and 8, `FUN_00013dd0`).
  - **Text:** FONT1GFX's 16-pixel face (`FUN_000187f4`). A glyph is descriptor (char - 0x20) and is drawn u1 - u0 wide and tall, so a rect's last row and column are not shown. The next glyph starts that far on; a space is 8.
  - **Layout:** the title (string 106) is centred at y 24. Rows are Aliens, Secrets and Mission (107-109). Each label is at x 26 (16 in Spanish, language 3, `SPAN.BIN`) and y 70/121/172. The figure, "N%", is at x 110 and y 62/113/164. The bar is at x 100 and y 58/109/160. "Press any key" (110) is centred at y 210 once counting ends.
  - **Colours:** each row's label and figure are coloured by its current value from the `FUN_00014ec0` table (0x80 = full brightness). The title and prompt are drawn untinted.
  - **Bars** (`FUN_00012858`):
    - Up to ten flat segments, 34 tall, starting at x 105. Widths are 18, 18, 17, 17, 16, 16, 16, 16, 16 and 15 (`0xa655c`). The last segment is cut to the remainder.
    - Each column takes working-palette entry 0xc2 + 3 per segment + column / 5.
    - The frame, FONT1GFX descriptor 118, is drawn over the segments. Its palette-0 pixels are see-through, which is how the segments show as rising columns.
  - **Counting:** one step a frame (34.87 Hz), rows in turn. Sound 0x27 plays every eighth step. Keys are ignored until the counting is done.
  - **Open: the segment colours.** The working palette (`0x361cf0`) holds the level's colours here. `FUN_0004f2dc` copies the first 0x1aa bytes of `PALS\LEVnnn.PAL` (via the table at `0xd846`) into entries 0x72-0xff. The repo only has empty placeholders for those files, so their bit depth is unconfirmed. The screen accepts either 6-bit or 8-bit values, and uses a stand-in red-to-green ramp when the file is missing.

## Reported from play, zip 46

- ~~**Triangles are pinched quads.**~~ WRONG, superseded by zip 50's "Triangles, traced": the pinch was a guess from voting on the levels' seams, and it put the wrong corners on L371's door frames (Edward, 2026).
- **L112's corner under the hanging sign** (the triangle in the report's screenshot) was not a mapping fault: the corner is covered twice, by triangle 5843 and by faces 5849 (texture 0) and 5850 over exactly the same area, and they fought for the pixels. A byte patch makes 5849 and 5850 zero-area (`Patch/L112LEV.txt`, README L112LEV Fix 1). A scan of every level for faces that lie in the same plane, face the same way and cover the same area (a probe, not in the repo) finds 34 pairs. Most have the same texture on both, which looks the same either way. The ones with different textures, still to be looked at in play: L161 faces 6081/6082, L162 2351/2354, L212 6425/6452 (a sliver), L322-L324 4251/4253 and 4425/4428 (L325 the second), and L905 6343/6357 (6357 has no texture).
- **A ceiling of 0 is floor + 0x30** (0x279a4, at level load). Read as a height it refused every step: 31 levels have such cells round the start (L113's start room entirely, the way out of others, such as L909's), the ducts behind the wall vents creatures wait in (32 creatures on L111, L122, L131, L222, L232 and L331), and the open ground of L113-L115. A creature released into a duct, a cell with no zones, comes out alerted (NOT TRACED: it could never see the player from there).
- **Vents:** the jet runs along the record's heading. The spawn turns the vent half round (+0x800), but FUN_0002b0e4 rotates a velocity along +z, the reverse of the port's heading, so the two cancel; turned round, 45 of the game's 59 horizontal vents jetted into the wall behind them. Steam is drawn from a grey copy of the explosion frames, not the frames' fire colours.
- **Seismic charges** (DAT_000b0ad0, pickup 6, which went to the pulse grenades before): Fire 2 with any weapon but the pulse rifle throws one (FUN_0003d794 -> FUN_0002c830, particle 0x13): from 0x100 below the eye, 0x18 down and the player's speed + 0x40 (at most 0x80) along the view, life 0x40, bouncing from 8. The pulse grenade (FUN_0002c4e0, particle 0xc) now flies too: 0x80 below the eye, speed + 0x100 (at most 0x100), life 0x30, bouncing from 0x10. Both by the arm at 0x2db80: gravity 1, a bounce 2 lower each time and then a slide halving, and they go off when their life runs out, on an object or creature (simplified, as the handler's grenade), or dropping more than 0x40 into a hole. The HUD shows CHRG with any other weapon while there are any (0x3ac97).
- **Bodies:** a frame hangs from the creature's anchor by its own offset (bottom at offsetY + height below it), stopped at the floor while on the ground. Drawing every frame's bottom at the anchor, 0x20 over the floor, left bodies floating.
- **Faster Xenomorphs** runs the aliens at 1.5 times the original's rate (every other skipped tick), not double.
- **The assessment screen:** a key or click while counting jumps every row to its final figure; the next leaves.
- **Still open from the same report:** egg husks shattering into pieces, the size of explosions, the flamethrower's flames, L381's ramps to the transparent-floor lifts, L391's ramps not putting the player off at the bottom, the queen not backing off when the player goes up a side hallway, the music fading only on a key press at the assessment, and rooms visible far off on L906/L907 (fog or draw distance).

## Zip 48: the rest of the same report, and a pass against the original

- **The flamethrower's flames** (particle type 3) were drawn as tracers. FUN_0002d6ec sends type 3 to its own draw, FUN_0002c94c: a square sprite from one of the effect records 0-7, picked at random each frame, half-size k / distance with k = (0x10 - life) * 0x600 (0xa00 under 4), so a flame starts as a point and swells as it flies, shade 0xfa, drawn from 0x10 away rather than 0x200 (`ExplosionGraphics::FlameSprites`).
- **Shards were built from the wrong faces.** The renderer cut a model's faces six indices at a time, which only holds for single-sided quads; a triangle put the rest out of step and glued pieces of neighbouring faces into huge shards (barrels and the episode 3 egg husks, object 0x14). A face is now a run of triangles with the same lowest index (`Formats::ExtractMeshFaces`, tested), and only draw routines 0 and 2 shatter, as FUN_000464ec has it.
- **Shards behave as the type 6 arm does** (0x2e21e): life (random & 0xf) + 0x20, which is also the size (drawn at life << 6 / 0x1000 of its quarter of the face, so it shrinks away); every particle moves by its velocity and a shard moves vertically a second time; gravity 4 (exactly 4 becomes -2); at floor + 0x40 it stops moving sideways and bounces at its bounce speed (launch + random & 0xf), 8 less each landing, staying down under 4, and a shard that has stopped is gone. Their life (60), gravity (6) and size were guesses before.
- **Explosion size:** FUN_000429f0 draws a frame (width + 1) * scale / depth pixels wide, which is (width + 1) * scale / 0x100 world units at the focal length 0x100: about three times the size the port draws them (`WORLD_UNITS_PER_SCALED_PIXEL`, still a guess). Left as it is, since the report was that they looked too big; the giant shards above were probably most of that.
- **A diagonal wall cell kept x on the wrong side.** FUN_0002800c's 0x1a holds x below z within the cell and 0x1b above it; the port had the two the other way round (the z clamp, FUN_000280dc, was right).
- **The fade, traced.** The level face walker (0x25880) averages a face's four view depths, shifts the result left by 2 and hands it to FUN_0004ecdc, so the fade runs from a view depth of 3400 (about 6.6 cells) to black at 8500 (about 16.6). The divisor had been matched to a screenshot (black at about 8 cells). With the Modern Render Distance option on there is no fade at all, which will be why rooms showed far off on L906/L907. NOT DONE: the original also draws only the cells its ring walk reaches, out to 15 rings and through the zones the player can see into (0x2745b, `CreatureSight::DrawnZoneMask`); the port draws the whole level.
- **The port's yaw is the original's turned half round.** FUN_00026d1c / FUN_00047e84 turn (0, 0, s) by a yaw into x = -s sin, z = s cos, so the original's yaw 0 looks toward +z; the port's looks toward -z. Where the port hands its own yaw to something of the original's it has to add 0x800:
  - the renderer's zone walk, which creatures test the player against (FUN_00033a1c), was being given the port's yaw and walked the cells behind the player. Fixed.
  - the ramp pitch table is the original's read against the port's yaw, and the original's pitch runs opposite to the port's; the two cancel, so ramps tip the view up a slope as before. The note there no longer calls it a guess. The original tests its look-up key first, so up now takes priority when both are held.
  - knockback, the creatures' own movement (FUN_00032f78 turns by facing + 0x800), the smartgun's cone, the minimap and the tracker were checked and are consistent.
- **Slippery ramps** (0x35-0x38) push downhill, as the port had them: worked through FUN_0003dff0's yaws (0x800, -0x400, 0x400, 0) with the matrix above. There is no momentum - the push is the tick's own - so leaving the foot of one is the player's own walk, as in the original. L381's walkways reach the main floor only by running up its slippery ramps (rows 42-45); its cross-shaped ramps top out at 0xbff, one short of the 0xd00 floor beside them, in the original as well.
- **L909's start room was sealed** by four attribute 1 cells past its doorway with nothing drawn there; a byte patch opens them (`Patch/L909LEV.txt`). TEMPORARY, for testing: the room is most likely the multiplayer players' holding room (its lift has developer graffiti, "RIPLEY WOZ ERE" and the like, in textures that match nothing in the level), sealed on purpose. Undo the patch once multiplayer spawns are in. Every other level's start was walked with the port's mover: the small ones (L391, L371) are closed by a door or reached by lifts.
- **The assessment music:** FUN_00014f44 begins with FUN_000425fc, which only marks the current track as none (0xffff); nothing fades or stops the CD at the level's end (FUN_00040a98 and FUN_000408d0(-1) stop the sound effects and their loops). The level's music plays on through the assessment and changes when the next level starts its own, which is what the port does.
- **The queen:** her task (0x35bec) was compared state by state with the port and matches, including her waypoint pick (FUN_00032fcc, 0x400-0x5ff from the player) and the stop at the far band. Not found: why she does not back off from a side hallway. The zone walk above was wrong for every creature, her included, and may have been part of it.
- **The egg husks** are level objects (0x14 in episode 3, model EggHusk) and come apart through the shatter above; a creature egg's death is only its animation (FUN_00030b04, animation 1).

## Zip 49

- **The smartgun's sound** (and every weapon's): the animations' cues (FUN_0003e93c) go to FUN_00040b2c with EBX -1, which plays them through FUN_0005b21b on the player's own channel, cutting off whatever was there; only the flamethrower's loop (internal sample 0xc, slot 0x0b) is left running if it already is. The port played them through the shared rule that a sample never starts over itself (FUN_0005aed9's, which is the positioned path), so a held smartgun, whose cue comes round before its report has finished, dropped most of its shots. Voice 0 is now the player's channel (`AppWindow::PlayerSfxVoice`, `SfxPlayer::PlayPlayerSlot`).
- **Fewer, truer shards.** FUN_00046ffc steps two faces on after each one unless the next is the model's last, and stops on the last without shattering it: only faces 0, 2, 4 ... of a model break up, about half as many pieces as the port made. Each piece is now its real quarter of the face - corner, edge midpoint, the jittered centre, the other midpoint - with that quarter of the texture, in the model's space and scale, turned with the object as it was drawn and spinning about the model's Z axis, as FUN_0002d054 draws it. A piece keeps its face's plane, so pieces from the front spin like plates and those from the top tumble; none faces the player. Before, every piece was an upright square, and the object's turn was read from its compass value as if it were an angle (so pieces never turned with it).

## Zip 50

- **Triangles, traced.** Draw routine 1 (0x1b5c8), which nearly every level triangle uses, projects a triangle with FUN_00048128(c, b, a) into screen slots 0, 1 and 2 and copies the texture descriptor's first three corners into those slots in order; the quad routine (0x19af0) projects with FUN_00048490(d, c, b, a) into slots 0, 1, 3, 2 and copies all four. With the quads' mapping (a, b, c, d take x0y1, x1y1, x1y0, x0y0) that puts the descriptor's corners at x0y0, x1y0, x0y1, x1y1, so a triangle (a, b, c) takes **x0y1, x1y0, x0y0** - ALTViewer's export mapping and the models' triangle case. L371's door frames now match DOS and the old Blender exports; L907's stripes still line up. The pinch (zip 46) and the mapping before it (x0y0, x1y0, x0y1: a and c swapped) were both wrong. ALTLevelScan looks at triangles again.
- **L111/L900's triangle at 0x46DB4** takes its quarter-turn again (8049, 8113, 8052): of its three rotations under the traced mapping it is the one whose texture runs with its neighbours'. No other patch rewrote a triangle's corners until zip 51's L371 recesses.
- **The smartgun's sound.** The cue stays up from the animation step that raises it to the next, and the port read it every player tick - two or three times a step at the default weapon pacing. It is now read on the weapon's own pass, as FUN_0003e93c reads it. Each cue restarting the player's channel is the original's (FUN_0005b21b); 0902smar is 1.37 s of unbroken fire, so held, the restarts every burst run together into one continuous sound, where the port's old rule (never start a sample over itself) let each run out and gave separate bursts.

## Zip 51

- **L371's recess corners.** The level's 14 wall recesses each frame a texture 35 band with four corner triangles. With triangles traced (zip 50: (a, b, c) take x0y1, x1y0, x0y0), a right-angled corner needs its right angle on c. The top right and bottom left have that; the top left and bottom right have it on a, so the texture runs diagonally across them (reported by Edward). They are mirror images of the good corners, so a turn to put the right angle on c still leaves the texture transposed: Edward spotted the orange marks running across the band instead of along it. Routine 1 can only turn a triangle's texture (routine 3, the slot a mirrored one would take, is an empty stub at 0x1d644), so the fix reverses the order to (c, b, a), which mirrors it, and moves the face to routine 5 (0x1ef20). Routine 5 projects and copies the corners as routine 1 does but never tests the winding, so the reversed face still draws. Its backs are against solid wall. 28 faces, one byte patch in `data/Patches.json`, listed in `Patch/L371LEV.txt`, with before and after shots of all 14 checked. Recess 1 is a bent band: only its two flat corner triangles change, and its slanted pieces are sheared on purpose. ALTViewer's fixer does not have it yet (after review).
- **The same pattern elsewhere, not yet reviewed.** Grouping coplanar triangles of one texture in fours, with right angles on a in some and on c in others, also flags L232 (texture 306, 12 groups with 2 such corners each), L231/L907 (texture 310, 8 groups) and L311/L908 (texture 174, 2 groups), along with groups on L122/L901, L141/L903, L154, L211, L212, L904 and L905 where all four corners match (probably meant). Each needs rendering before anything is patched. Other L371 texture 35 triangles with the right angle on a (the hourglass bands near faces 2073-2244, the panels near 1456-1637, and 191, 2842, 2916, 3383, 3436, 4694 and 4726) looked right or were not clearly wrong, so they are left alone.

## Zip 52: Testing options

- **Options > Modern > Testing**, two aids for going over every face of every level (Edward, 2026). Both are off by default, and kept apart from the Modern features: Enable All, Custom and the presets never touch them (`Bootstrap/TestingSettings.h`, config keys `TestingDisableEnemySpawn` and `TestingFaceIndexDisplay`). TEMPORARY, like Level Select: to remove, drop TestingSettings.h, the Testing list in MenuTree.cpp's Modern(), its branch in MenuController's ApplyLeafAction, and the `testing*` uses in GameplayScreen.cpp.
  - **Disable Enemy Spawn.** The level starts with an empty creature roster. Triggers, eggs, crates and vents all look their monster up by index and do nothing when it is not there, so nothing can appear. It takes effect at the next level start; the assessment then counts 0 aliens.
  - **Face Index Display.** A yellow dot at the centre of the view, the level face under it outlined in yellow, and at the top of the HUD its face number, texture, draw routine, light id, file offset and vertex list. The pick (`Formats/FacePick.h`) skips faces that draw nothing and the backs of culled faces, so it names the face you can see. Its number is the index the Patch/LxxxLEV.txt lists use; its offset is 0x14 + `LevelGeometry::faceRecordsPos` + 20 per face (checked against L371: 2581 at 0x1FF08, 4516 at 0x29634, 9915 at 0x43C00). Doors, lifts and other models are not level faces and are not picked. Re-read on leaving the pause menu, so it can be switched on mid-level.
- **L371's top right and bottom left recess corners**, reported as needing the same flip. They have the right angle on c already, so their only mirror is (b, a, c) with routine 5; rendered, it turns their orange marks across the band (recess 5 shown side by side). Not patched until checked in game with the face display.

## Zip 53: UV review, round 1

- **The face-by-face UV review has its own log: `Notes/uv-review-log.md`.** Every face fixed goes in it (face, offset, old and new record), so ALTViewer's fixer, the Patch/LxxxLEV.txt lists and the README can be updated in one pass when the review is done. Patches.json is kept up to date as it goes.
- **Round 1, L371 (Edward's first two lists of eight).** The top left and bottom right corners are zip 51's fix, plus the recess at z 9984 (faces 5631 and 5636, texture 27). The top right and bottom left corners (5640, 5628, 6820, 6815, 7609, 7603, 5014, 5002) are mirrored too, so every corner shows the band mirrored across its seam and the frame is symmetric. These need the half of the tile cut along x0y0-x1y1, which no triangle can show (routine 1 and 5 always give x0y1, x1y0, x0y0). Each becomes a quad (a, b, c, c) with routine 2: the triangle (a, b, c) carries the face, and (a, c, d) has no area. That is exact in the port, which draws quads as (a, b, c) + (a, c, d). No shipped level has such a quad, and the original's quad rasteriser (FUN_0004abb4 and its spans) is not traced, so DOS may draw it differently.
- **The fixes were found by a solver, not by eye** (scratch probe `mirrorfix`). For each corner triangle it finds the same-texture band quad sharing an edge (texture 27 counted as 35), reflects the corner across that edge into the band to get the texture corner each vertex should show, and picks the simplest record that shows it: a turned triangle, a reversed one with routine 5, or a quad with its last corner repeated. For the top left and bottom right corners it gives exactly zip 51's records.
- **Patches only reach the game at install.** RunPatches is called only from the install pipeline, so a fix needs a reinstall (or the patch applied to the installed files) before it shows in game.

## Zip 54: UV review round 2, Ghost Mode

- **L371, round 2** (`Notes/uv-review-log.md`). The top right and bottom left corners of the other ten recesses (7587, 7581, 4525, 4513, 9906, 9911, 9889, 9884, 9535, 9270, 4905, 5175, 5222, 4946) are mirrored like round 1's, each a quad with its last corner repeated, routine 2. Face 9270's band corner is 2 units off the grid, so the solver now rounds within 0.02 of a tile corner (9532, solved again with it, comes out as its zip 51 record). Faces 6820 and 5628 (round 1's corners at z 13056 and z 9984) moved from texture 27 to 35, the band's own.
- **Ghost Mode** (Options > Modern > Testing). Movement skips PlayerMover's wall, object and creature checks, so the player goes straight through. The floor stops holding them: the feet keep their own height, starting where the player stood when it came on, and a step forward climbs or dives by the step along the view times tan(pitch). That works with the look keys or with free look. Ramp pushes and harmful floors are off while it is on. Triggers, pickups and doors still act on the cell the player is in. Turned off, the player drops back to the floor of whatever cell they are in, which may be inside a wall. Positions off the grid are safe: CellAt, FindFloorHeightGridSpace and the trigger cell all bounds-check.

## Zip 55: UV review round 3

- **L371, round 3** (`Notes/uv-review-log.md`): the top right and bottom left corners of recesses 6, 7 and 11 (5893, 6288, 5320, 5303, 9876, 9607) mirrored as in rounds 1 and 2, and faces 6815 and 5640 moved from texture 27 to 35. Every recess corner on L371 that has come up so far now shows the band mirrored across its seam.

## Zip 56: UV review round 4, door and lift meshes

- **L111 and L900, face 9618** (0x46DB4): turned once more counter-clockwise to (8113, 8052, 8049), the one rotation not tried. The original and zip 50's quarter-turn both streaked it. The two existing entries are updated in place. Edward reports L111 complete after this.
- **L371, round 4** (`Notes/uv-review-log.md`):
  - 6453, 6455, 5794 and 5792: texture 86 -> 94.
  - 7382 faced away from the room. It is reversed to (b, a, d, c) with routine 0 -> 2, so it shows from inside and every corner keeps its texture corner; its rotation is left for Edward to check.
  - **The circle panel at z 11520** is a 3 x 3 picture (textures 1-3, 9-11, 17-19) on a skewed grid, with its corners cut. Its top corners carried bottom-row textures (17 and 19); they are now 1 and 3. Each cut corner should show the inner half of its tile, and only the bottom right (6073) is a half a triangle can show. The other three are quads with a repeated corner: 6064 routine 0, and 6062 and 6075 routine 6, because their real triangle is wound the other way (routine 6 is the double-sided mirrored quad; these tiles have no transparent pixels, so its cut-out changes nothing). 6069, the top middle, was drawn mirrored (routine 2) and is now 0. The solver gained a "continue the picture" mode for this, the next tile over rather than a mirror; on 6073 it leaves the record as it is.
  - **14 pinched triangles** (2580, 2583, 2244, 2241, 2077, 2078, 1457, 1456, 1636, 1458, 1460, 1459, 1637, 1461) each had its right angle (or its largest angle) on a. Turned to (b, c, a), the angle sits on c, where the texture's own right angle is, so the texture no longer shears; any further turn or flip is for Edward to judge in game. The column hourglass near (-26624, 3328, -12032) has more triangles in the same state that are not flagged yet: the lower pinch (2073, 2074, 2237, 2239) and the rest of both fans.
- **Door and lift meshes leaked between levels.** They are cached under ModelCatalog::LevelDoor / LevelLift by slot (mesh * 8 + page), every level reuses the slots, RegisterMesh keeps whatever a slot already holds, and UnloadLevels freed only level geometry. So from the second level played on, a door or lift whose slot an earlier level had filled drew that level's mesh and textures. Edward's "door outside the level" on L371 is this: L371 has one door record, at cell (60, 4) inside solid wall, and no "D" sections at all, so it drew whichever door an earlier level left in slot 0. ModelRenderer::UnloadLevelModels now releases both catalogs; UnloadLevels calls it, and so does the level load before registering its own. The stray door now draws nothing.
- **L371's lifts 6-9 use lift model 5, and the file has only L000-L004.** With the fix above they draw nothing; before it, they may have drawn a stale mesh. What the original does with a model index past its table is not traced.

## Zip 57: two more testing options

- **Disable Resupply Timer** (Options > Modern > Testing): the resupply levels' countdown stops where it is and its HUD digits are hidden, so the level does not end on it. Turned back off mid-level, it carries on from where it stopped.
- **All Testing Options**, first in the Testing list, sets every testing option on or off at once. It reads On only when they all are, and the whole list is refreshed whenever any row in it changes.
- **L371 round 4 needs revisiting**: Edward reports a number of its fixes are wrong. He is going over the other levels first and will come back to L371.

## Zip 59: UV review round 5 (L131, L141/L903, L161)

- L131: 6754 texture 154 -> 183; 6752 turned to match 7497 (they face the same way, so matching on screen is matching in the world).
- L141, and L903 (the same geometry at the same offsets): 2735 texture 223 -> 217; 2736 flipped horizontally - its texture lies on its side, so that is a v flip, (c, d, a, b) with routine 2; 2196 texture 0 -> 225 (it sits behind a crate side, so it shows up close).
- L161: a missing face, the quad between faces 0, 29, 36 and 40. A face cannot be added without moving everything after it, so face 3758, an exact duplicate of 3757, is given over to it (texture 109, routine 0, light 13 like its neighbours). Exact in the port; in DOS its face run belongs elsewhere, so it may only show when that part of the level is drawn (untraced).
- New scratch tools for these rounds: faceedit (apply edits by face letter or vertex number, with a record of each), spare (a level's reusable face records), tilematch, tilecol and flat (compare texture tiles between levels' graphics).

