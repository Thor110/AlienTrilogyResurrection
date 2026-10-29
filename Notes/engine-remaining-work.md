# ALTEngine: what is left to match the original

Checked against `TRILOGY.EXE` (DOS/4GW LE: code object at `0x10000`, data object at `0xa0000`; operand values in the raw code are offsets into the data object). Most items name the original function to work from. Addresses written as `LAB_` or bare hex are timer-task handlers that Ghidra never turned into functions. Disassemble those directly.

Ordered by how much they change play.

## Checklist

Details for each item are in the numbered sections below.

| # | Item | Can be done from the binary alone | Status |
|---|---|---|---|
| 1 | One logic tick rate (find the game's clock) | yes | done: one fixed 34.87 Hz tick |
| 2 | Player movement exactly (`FUN_0003d85c` / `FUN_0003dc44`) | yes | open |
| 3 | Lifts (trigger commands 5 and 7) | yes | done (tested on 3.5.1: heights and platform position) |
| 4a | Creature line of fire and visibility before attacking (`FUN_0003231c`, `FUN_00033a1c`) | yes | done (zone test approximated) |
| 4b | Creature chase via the path network (state 2/3) | yes | done for the common task (see section 4 for what's approximated) |
| 4c | Creatures blocking each other (cell occupancy) | yes | done (barrel blasts still find creatures by position) |
| 4d | Hit, death and leap-end animations | needs the animation scripts | open |
| 4e | Per-type tasks for types 1, 7, 10, 0x10-0x13 | yes | done (animation events approximated; untested in play) |
| 5 | Flame pre-steps damaging objects | yes | done |
| 6 | Player Jumping option | modern | done (a hop; doesn't reach higher ledges) |
| 7a | Secrets counter on the assessment screen | yes | open |
| 7b | Mid-level saves | modern | open |
| 7c | Death clip choice | yes | done |
| 7d | Field renames in `LevelLoader.h` | tidy-up | done for lifts, pickups, crates and monsters |
| 7e | Direct-to-window rendering | needs real hardware | open |
| 7f | Multiplayer | large | open |
| A | Open question: pistol rounds against a standing hugger | needs a check in the original | open |
| 8a | Walk-through walls and floors: texture mapping (e.g. 3.5.1) | yes | open |
| 8b | Breakable windows | yes | open |
| 8c | Cryopod cover (textured like the windows, not breakable) | yes | open |

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

**What the port does now:** one fixed tick of 34.87 Hz (69.74 / 2, `PlayerCamera::TICK_HZ`) for everything: player, weapons, creatures, particles, doors, lifts, lights, HUD and tracker. Drawing runs at whatever rate the display allows, with the camera smoothed between ticks.
- **Why that rate:** it's the original's own target, one pass per two interrupts. That is what `-so` gives on a machine that keeps up, and what the compensation is meant to hold: its formula runs one pass more than it needs (c/2 + 1), so a fast machine actually ran the player at ~70 Hz.
- **It matches the old hand-tuned walk:** 0x3c a tick at 34.87 Hz is 2092 units a second, against about 2000.
- **Not reproduced:** the frame-rate dependence itself (faster on a fast machine, slower on a slow one), which would only hurt on modern hardware.

## 2. Player movement, exactly

`FUN_0003d85c` (X axis) and `FUN_0003dc44` (Z axis), called from `FUN_0003dff0`:
- Three probes across the leading edge: ±200 along the direction of travel, at z, z−200 and z+200.
- **Blocked** when cell byte `+7` has bit `0x80` or the occupancy byte `+0xc` is non-zero, unless the noclip cheat flag `DAT_000a4d64 & 1` is set.
- **Height checks** apply only to attributes `< 0x14` or `0x19–0x2c`:
  - blocked when the ceiling is below the player's Y + `0x300`;
  - blocked when the rise is more than `0x100`.
- **Walls and corners** are clamped by `FUN_0002800c`, with sliding in `FUN_0003d804`.

The port samples five points and treats bytes `+2`/`+3` as walls. The clearance (now 24 units) and step-up (256) already match.

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
  - **Act length:** a non-leap act lasts a fixed 0x18 ticks. The original waits for the attack animation's script to set `+0xb6` bit 2.
  - **Gunners' rounds:** done for `FUN_0002af8c` (types 0x0b-0x0e). They fire half-way through the act, standing in for the `+0xb6` bit 3 event. The grenade (`FUN_0002b240`, particle 0x0d, 25 damage, used when `+0x6c` bit 0 is set) isn't done, since nothing traced sets that bit.
  - **Types 0x0b-0x0e** use `FUN_00032aec` instead of `FUN_0003231c` in places. The port uses `FUN_0003231c` for them.
  - **Speeds and timers** are the original's per-tick values. How fast they look depends on the tick rate (item 1).
- **The other tasks** are now in `Screens/CreatureTasks.h` (egg, colonist, vents) and, for the queen, as a variant of the common task in `Screens/CreatureAI.h`. See "Settled" for what each does. What is still approximated:
  - **Animation events:** the egg, colonist and queen wait on their animations' script bits (+0xb6 bits 1 and 2). The port has no script VM, so an animation playing through stands in, with a frame count from the sprite set (4 when it has none).
  - **The egg's line** uses `FUN_0003231c`; the original uses the variant `FUN_00032aec`.
  - **Vent jets** are drawn as plain puffs; the original's per-type sprites (the table at `0x2d6a0`) aren't read. The vent's sound plays once as the jet starts, since the port's mixer can't hold a loop. A burst pipe shows the barrel explosion; whether `FUN_0002aaf0` also does blast damage is unchecked.
  - **The queen's death** slides her down a slope and can sink her into a pit (cell attribute 6, under 4 units of headroom); neither is done. The colonist's view-dependent death animations (0x1b + view) aren't either.
- **Visibility mask:** `FUN_00033a1c`'s zone test uses the zones of the player's own cell. The original's mask is every zone the renderer drew that frame (`0x274c2` onward).
- **Leap end:** the original ends a leap when the animation script sets `+0xb6` bit 2. The port ends it on landing.
- **Walking off a ledge** gives a creature a small hop (`vy = 0x3c`, `FUN_000315f0`). Not implemented.
- **Creature steps** now refuse a rise over 0x100, uneven ground (probes more than 0x100 apart) and a ceiling below the creature's half height (`FUN_000315f0`).
- **Hit and death animations:** `FUN_0002f4b0` starts an animation and `FUN_00030b04` is death.
- **Cell occupancy:** rebuilt every tick in the original's pattern: the player's cell is marked `0xff`, and each live creature lifts its own mark before it looks or moves, then marks where it ends up. Movement and line of fire both refuse marked cells. Barrel blasts still find creatures by position instead of by the mark's type and index.

## 5. Weapons still approximated

- **Flame pre-steps:** done. `FUN_0002bcac` runs each of a flame's 1-2 spawn steps through the full collision test (`FUN_0002a448`), so objects, creatures and shot triggers in those cells are hit, and a blocked step leaves the flame burning there. The port now does the same with `Particle::preSteps`.
- **Shotgun shell model:** model 26 is used for the ejected shell. This is inferred, not traced.

## 6. Modern options

- **Player Jumping:** done. A Jump action (default J, in the Controls list, translated in all six string tables) gives a hop with the original's gravity (0xc a tick, a creature's), lift 0x48: about 216 units high. It's held under the ceiling. Movement's step-up rules are unchanged, so a jump doesn't reach a ledge more than the usual 256 up.
- **All other options** are now read by gameplay or the menu.

## 7. Smaller items

- **Assessment screen:** the "secrets" figure is always 0, because nothing tracks secrets.
  - **Found so far:** two level counters that only ever count up. `DAT_000b0b28` counts doors activated for the first time (`FUN_00029560` / `FUN_00029610`, door flag 0x80), and `DAT_000b0b2a` counts lifts run for the first time (lift flag 0x80). Neither has a direct reader, so the assessment screen probably reads them as a block. Its code is the next thing to read.
- **Saves:** they checkpoint the start of a level only. Mid-level state isn't serialised.
- **Death clip choice:** done. The causes are the creature attack's, by type (`FUN_00033ff8`, tables `0x33f60` / `0x33fac` by type - 1): huggers and bursters 4 (DEATH1), bambi and dog 1 (DEATH5), warrior drone 0 (DEATH2), queen 2 (DEATH3). Cause 3 is a creature's projectile (particle type 5), and 5 a blast or a vent. The port passed 4 for every creature. Types 8 and up have no arm there at all.
- **Field names:** renamed. Pickups: `triggerCount` / `triggerThreshold`. Crates: `flags` / `healthDiv16`. Lifts: `width`, `depth`, `low`, `travel`, `flags`, `autoDwell`, `dwell`, `threshold`, `counter`, `model`. Monsters: `coneHalfWidth`, `wakeRange`, `speedHigh`. A monster's byte 6 keeps the name `drop`, with a note that its meaning depends on the type.
- **Rendering:** each frame is still copied GPU → CPU → GPU (`ModelRenderer::RenderLevelToRgba`). Drawing straight to the window would remove a full-frame stall, and needs testing on real hardware.
- **Multiplayer:** the menus exist, but there is no networking.

## 8. Reported from play, after the rest of the list

- **Walk-through walls and floors:** some walls (and floors) can be walked through, in the original too, where it isn't obvious either. In the port their textures are not mapped properly. Seen on 3.5.1 and throughout the game.
- **Breakable windows:** some levels have windows that can be shot out.
- **Cryopod cover:** looks to take its texture the same way as the windows, but can't be broken.

## Settled this round (see the code comments for the evidence)

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
