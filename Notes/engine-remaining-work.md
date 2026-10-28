# ALTEngine: what is left to match the original

Checked against `TRILOGY.EXE` (DOS/4GW LE: code object at `0x10000`, data object at `0xa0000`; operand values in the raw code are offsets into the data object). Most items name the original function to work from. Addresses written as `LAB_` or bare hex are timer-task handlers that Ghidra never turned into functions. Disassemble those directly.

Ordered by how much they change play.

## 1. One logic tick rate

The port runs two clocks:
- 30 Hz: the player, weapons, enemies and particles. `PlayerCamera::TICK_HZ` is a guess.
- 60 Hz: doors, lights, HUD timers (including the 0x1e damage cooldown) and the motion tracker.

The original has one. This matters more now: the creature leap is per tick (vertical 0x80, gravity 0xc, horizontal 0x60), so its height and length on screen depend on the rate. Leads:
- The game never calls the sound library's timer API itself. The only frequency set through it is 100 Hz (`0x64f68`), on the library's own internal callback (`0x54d2a`). The game's clock is somewhere else: the task list run by `FUN_000124f4`'s scheduler, or its own interrupt handler.
- The timer is a sound-library service. `0x72060` programs the PIT (mode `0x36`) from a divisor. `0x72090` takes a period in µs (divisor = µs × 10000 / 8380). `0x720c3` picks the fastest registered period. `0x72844` registers a slot, called through the wrapper at `0x65aef`.
- Game tasks are registered with `FUN_000124f4(priority, handler)` (see `FUN_00010694`). Find the event that drives them and its period.

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

- `level.lifts` is parsed but unused. The demo levels have none, so retail maps are needed to test them.
- Commands 5 and 7 are `FUN_00041e20` and `FUN_00041dd8`, with HUD notices `FUN_000537dc` / `FUN_000537bc`.
- The trigger system currently fails the chain on both.
- `Game::DoorState::isLift` and `DoorNotice::LiftActivated` are already there for them.

## 4. Enemy behaviour per type

`FUN_0002e638` spawns every creature and registers a per-type task:

| Creature type | Task |
|---|---|
| Most | `LAB_000342b4` |
| 1 | `LAB_000356a0` |
| 7 | `LAB_00035bec` |
| 10 | `LAB_000364d4` |
| 0x10 / 0x11 | `LAB_00035aa0` / `LAB_00035ae8` |
| 0x12 / 0x13 | `LAB_00035b68` / `LAB_00035b28` |

Status in the port:
- **The creature task's state machine** (`LAB_000342b4`, jump table at `0x34270`, states 0-8) isn't followed state by state. The port has its own ACTING / HIT / DYING states, with the original's attack logic (state 4 → `FUN_00033cbc` → state 5) mapped onto them.
- **Line of fire** (`FUN_0003231c`) and the visibility test in `FUN_00033a1c` aren't implemented, so a creature can attack through a wall.
- **Leap end:** the original ends a leap when the animation script sets `+0xb6` bit 2. The port ends it on landing.
- **Walking off a ledge** gives a creature a small hop (`vy = 0x3c`, `FUN_000315f0`). Not implemented.
- **Movement:** enemies still walk straight at the player.
- **Path network:** `Game::PathGraph` loads and routes it, tested, but nothing uses it yet.
- **Hit and death animations:** `FUN_0002f4b0` starts an animation and `FUN_00030b04` is death.
- **Cell occupancy:** a creature stamps its cell (`+0xc` = type, `+4` = index) at spawn and as it moves, and the step probe refuses any occupied cell. The port now blocks only the player's own cell (the player stamps `0xff`, `0x3e3d6`). Creatures still overlap each other, and barrel blasts find creatures by position instead.

## 5. Weapons still approximated

- **Flamethrower:** builds its particles inline in `FUN_0002c1d4`. The port uses the common spawn.
- **Smartgun:** pre-steps each round 1–2 steps with a wall test on each (`FUN_0002bcac`). The port pre-steps without the test. Its ammo use is a variable amount (`FUN_0002c0a0`); the port takes 1.
- **Auto weapon switch when empty:** `FUN_00038c78`. The port always takes the "cannot switch" path.

## 6. Modern options

- **Player Jumping:** in the menu but does nothing. It needs a Jump input action, a default key, a Controls menu entry and strings in all six `LANGUAGE` packs. Look Up / Look Down show how to add an action: append to `InputAction`, list it in `AllActions()`, give it a label and a default key.
- **All other options** are now read by gameplay or the menu.

## 7. Smaller items

- **Assessment screen:** the "secrets" figure is always 0, because nothing tracks secrets.
- **Saves:** they checkpoint the start of a level only. Mid-level state isn't serialised.
- **Death clip choice:** causes 0–2 are known to `GameOverSequence`, but their sources aren't identified.
- **Field names:** `Pickup::unknown1` / `Pickup::z` are the trigger counter / threshold, and `Crate::unknown1` / `Crate::drop` are health ÷ 16 / flags. They're used correctly now but still have their old names in `LevelLoader.h`.
- **Rendering:** each frame is still copied GPU → CPU → GPU (`ModelRenderer::RenderLevelToRgba`). Drawing straight to the window would remove a full-frame stall, and needs testing on real hardware.
- **Multiplayer:** the menus exist, but there is no networking.

## Settled this round (see the code comments for the evidence)

- **Melee creatures leap** (`FUN_00033cbc`, called from creature-task state 4 at `0x3476e`):
  - **When:** the creature faces the player exactly, is in a mode below 3, is within 0x400 vertically, and its attack timer has run out (reloaded to 0x3c).
  - **Face hug:** a face hugger in mode 0, within 0x600 vertically, with the lock free, face-hugs.
  - **Leap:** otherwise it leaps, with horizontal 0x60 along its heading (`FUN_00032f78`), vertical 0x80 and gravity 0xc (`FUN_00031f3c`). Type 3 always leaps.
  - **Landing:** the hit is the ordinary attack check (`FUN_00033ff8`), run when the leap ends.
  - **Effect:** a hugger rises through level fire mid-leap, so it can be shot without the look keys.
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
  - The smartgun fires 2 rounds per pull, launched from eye − `0x80` + jitter, `0x38` to the side (`FUN_0002c0a0`, `FUN_0002bcac`).
  - Each pulse round costs one (`FUN_0002bc18`).
