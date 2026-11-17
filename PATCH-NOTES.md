# ALTEngine patch notes

What changed in each version of ALTEngine, newest first. The version number lives in `ALTEngine/VERSION`;
it goes up with each release, and each release gets a section here.

The aim stays the same: with every Modern option off, the game plays as the 1996 original does. The Modern
options (Options > Modern) are the deliberate departures, each one off until you turn it on.

---

## 0.9.9

Everything since 0.9.8.

A third review of the original's code against ALTEngine, this time looking for what makes play feel slightly
off: rates, timing, rounding and the order things happen in.

### Closer to the original

**The player**
- **Falling and climbing.** Walking off a ledge, you now drop to the floor at the original's speed (0x100 a
  step) instead of landing at once, and climb up the same way. A long fall that lands exactly on the floor
  hurts, as in the original (it skips armour); landing in water or slime makes its sound.
- The killing hit's knockback no longer slides your body two cells while you die.
- The view tips on ramps the way the original's does when facing along them (one direction was the wrong way
  round).
- Mouse turning is smooth at high frame rates. The view used to run ahead and jump back every tick, a fine
  judder while turning.

**Weapons**
- **The smart gun's firing sound no longer stops** (bug report 6). The real cause hit every sound: once a sound
  had finished, the audio system still counted it as playing, so the same sound was refused from then on, and
  sounds went missing at random once a dozen had played. Fixed for every sound.
- The smart gun, shotgun, pulse rifle and flamethrower now play the original's own animations, with their own
  sounds at the right frame: the shotgun's pump, the smart gun's reload. Their rates of fire and reload times
  are the original's (the smart gun was firing 60% too fast).
- **The pulse rifle shows its own firing art.** It was drawing its grenade art when firing, its firing art when
  reloading, and its reload art for the grenade.
- Switching weapons runs at the original's speed: down for 9 steps, up for 8. You can't fire or throw while
  the weapon is lowered.
- The lamp's sound no longer cuts off your gunfire.
- **Weapons fire and animate at the original's speed against the world.** The weapon pacing's default is now 2
  (it was 3): with ALTEngine's default timing the original steps the weapon every game tick, and 3 ran it on
  only two ticks of three, so every gun fired and moved at two thirds of its speed. A `WeaponFrameInterrupts`
  value already in your config file is kept; remove it (or set 2) to get the new default.

**Creatures**
- **Warriors walking backwards and sideways:** a creature now picks the view it is drawn in after it has
  turned, as the original does. It used to show the way it faced a moment before, so on setting off each new
  leg it walked a step in a view up to 45 degrees off its path.
- A creature near a path node no longer heads for a random point around it, often beside or behind it.
- Creatures climb and duck the original's heights. They step up a little more (0x120), aren't let into
  passages too low for them, and walk along a drop's edge as the original's do.
- Their timers keep counting while they flinch, as in the original.
- The queen notices you only in her sight cone.
- **On Hard, the dead come back.** As in the original: a body left lying while you're far away gets up again.

**The world**
- **Doors make their sounds** (opening, closing, the thud). A door opening also makes the noise that wakes
  creatures nearby, as a shot does.
- **Flickering, strobing and pulsing lights** (three light modes the port didn't have) now work, on most
  levels. L111's red alarm lights pulse for good instead of dying out after 18 seconds.
- The gun is no longer blown out white by its own muzzle flash.
- "Lift activated" shows every time a lift is called, not only the first.
- Motion tracker blips move a cell at a time, as the original's do.
- Flames and steam jets flicker at the original's speed at any frame rate.
- Triggers and pickups are checked once per game tick, not once per frame (at high frame rates some trigger
  commands ran several times over). No pickups are collected while you're dying.

**Multiplayer**
- Bots move, turn and fire at the players' own rate (they were twice as fast).

### The level fixes, in stages

Every fix to the game's files now runs in a fixed order, each stage working on the files as the stage before
leaves them, the same in ALTEngine and in ALTViewer's patch button (`Patch/README.txt` explains each one):

1. the duplicate door and lift models taken out;
2. the level fixes (geometry, objects, lifts, doors);
3. the texture fixes (texture numbers, turns, flips, the texture sheets);
4. **the missing faces** - new in the global fixes;
5. ALTEngine only: the Level Cleanup, the texture blanking, ALTEngine's own file fixes, and last the padding trim.

- **The missing faces are now fixed for the original game too.** On 36 levels the first cell of the map never
  had its faces drawn (an off-by-one in the original's level files); some of those faces fill real holes, like
  the floors on L212, L242 and L243 and the wall on L161. ALTEngine's Level Cleanup used to put them back, so
  with the cleanup off they were missing. They are now a stage of their own, with L211's missing floor, applied
  whether the cleanup is on or off, and ALTViewer's patch button puts them in for the original game.
- **L232: the missing wall in the boiler room, by the grate** (reported on Discord), is in, with the texture
  Edward picked (193, flipped).
- **The padding trim** (ALTEngine only, Options > Testing > Padding Trim, on by default): the level files lose
  the bytes the game never reads, about 10% of them (1.9 MB across the 45 levels). The levels play exactly the
  same. Turned off, the untrimmed files come back at the next start. ALTViewer reads the trimmed levels too.
- **The face hugger's art is fixed in ALTEngine's copy of FINGERS.B16** (ALTEngine only). The two rows under
  the creature, filled since 0.9.6 so it reaches the bottom of the screen, were made in code every time the art
  loaded; they are now in the file itself, the same pictures (`Patch/FINGERS.txt`).
- With the faces in, face numbers on most levels are one higher than before from the first cell's faces on
  (the Face Index Display shows the new numbers).
- ALTViewer's patch button can be pressed again over a patched game: it takes the missing faces out, checks the
  byte patches, and puts them back. ALTEngine does the same when it is installed from a game ALTViewer patched.
- **ALTViewer reads ALTEngine's files**: the trimmed levels (Level Viewer) and the face hugger's taller frames
  (Graphics Viewer). Its patch button stays hidden on an ALTEngine install, whose patches are already in, and
  refuses one if pressed: its byte patches are for the original game's files.

### New

- **Options > Graphics > Renderer**: Auto (the default, SDL's own choice), or Vulkan, Direct3D 12 or Metal -
  whichever this machine can run, so only those are listed. It takes effect the next time ALTEngine starts; one
  that will not start there falls back to Auto. The log says which one is drawing. (There is no OpenGL: the
  game's 3D is drawn through SDL's GPU API, which has none.)
- **ALTViewer beside ALTEngine** finds the game files ALTEngine installed (its `altengine.cfg`, else
  `GameData\CD`), and if ALTEngine has not been run yet, says so and closes: ALTEngine installs the files on its
  first start.

### Changed

- **No command window on Windows.** The window that opened behind the game is gone; what it showed now goes to
  `altengine.log` beside `ALTEngine.exe`, made afresh at every start - the file to send with a bug report. (A
  build configured with `-DALTENGINE_CONSOLE=ON` brings the window back, for testing.)
- **Picking a weapon from the pause menu is always on**, as in the original: Enter (or Right) on a weapon in the
  list switches to it. It was the Modern option Pause Weapon Select, which is gone.

---

## 0.9.8

Everything since 0.9.7.

### Fixes

- **Cutscenes in other languages** (reported by Pox83). With French, Italian or Spanish chosen, the game looked
  only for that language's videos, and skipped every one the disc or installation didn't have. It now plays
  the one that is there: the chosen language's, else the English one, else another language's. The same goes
  for the mission briefing text and the panel graphics, which were left blank.
- **Lift and bridge switches can be used again**, as in the original (reported on Discord). A switch thrown by a
  trigger with uses left comes back about a second later, ready to be thrown again; only a switch on a trigger's
  last use stays thrown. Before, every switch stayed thrown for good, so a lift called once could never be
  called back, and on L232 a player could be left stuck. 35 switches on 11 levels work this way (L131, L141,
  L213, L231, L232, L262, L311, L331, L371, L905, L908).
- **Cheats in the pause menu** stay as you set them. Turning one on and backing out showed it off again (it was
  still on underneath until the menu closed), and closing the menu could switch off a cheat that hadn't been
  touched that time. Now the menu shows what you picked, and the game takes every cheat's setting as the menu
  closes.

---

## 0.9.7

Everything since 0.9.6.

### Closer to the original

**Creatures** (a second audit of the creature routines against the original's code)
- Warriors walk backwards and sideways much less: a creature standing idle keeps turning its walk to face you
  each moment, as in the original, instead of keeping the view it had when it stopped.
- Creatures no longer stop a step short of a wall and turn: they look ahead from where this step takes them.
- A creature that stops while rising falls back down.
- Eggs, face huggers, the queen, colonists and vents run at the original's rate; before, some ran half as often.
- **Face huggers:**
  - one from an egg leaps out of it, on its leap animation;
  - one from a crate leaps out too, instead of walking out;
  - one on your face holds on for the original's time, taking 1 health each moment it holds, then lets go and
    falls away;
  - a waiting face hugger (or any creature) wakes when it sees you either way the original checks, not just
    one.
- A creature carrying a pickup throws it out when it dies, as in the original.

### Fixes

- A weapon switched to after another ran out of ammunition fires again. Running the shotgun dry and going back
  to the pistol left the pistol unable to fire.

---

## 0.9.6

Everything since 0.9.5.

### New

- **Invert Look** (Options > Controls > Mouse, under Mouse Sensitivity): moving the mouse forward looks down.
  It applies with Free Look on, the only time the mouse tips the view.
- **Mouse Sensitivity applies to both axes**: turning, Free Look's up and down, and (without Free Look) the
  mouse's forward and back walking, which it didn't scale before. 5 is the default and unchanged.
- **HUD Size** (Options > Graphics, a slider): the in-game HUD drawn smaller, down to 55% of the original's
  size, each part shrinking into its own corner - the map, items and messages top left, health top right,
  ammunition bottom left, the motion tracker bottom right. Full, the default, is the original's size, which
  already takes up a lot of the screen. The weapon in hand and a face hugger on your face keep their size, as
  they're part of the view.
- **Pause Weapon Select** (Modern option). Pick the weapon in hand from the pause menu's list: Enter (or Right)
  on a weapon you carry, with ammunition, switches to it, and the menu stays open. The original game has this;
  here it can be turned off. On in the Convenience and Modernised presets.
- **Multiplayer plays by the host's rules.** In deathmatch and co-operative games, every option that changes
  how the game plays is the host's choice, for every player:
  - **The host's choice for everyone:** Player Jumping, No Hit Immunity, Free Look, Render Distance,
    Live Minimap, Faster Xenomorphs, Speed Compensation, Pause Weapon Select, Enable Cheats, and the weapon
    pacing. With the host's cheats on, each player can turn their own on from the pause menu.
  - **Off for everyone:** Save Anywhere, and the testing aids Disable Enemy Spawn, Ghost Mode and Disable
    Resupply Timer.
  - **Each player's own:** Level Timer, Level Select, Skip End Level Screen, and everything else (sound, video,
    controls).

  Your own settings are left as they are and come back when the game ends.
- **Version check when joining.** Every player in a network game must run the same version of ALTEngine. A
  player on another version is turned away with a message saying which version the host runs and which they
  run. The same screen explains a full game, or a host that never answered (with a reminder about the UDP
  port). From 0.9.6 on, any two versions can tell each other this; a version before 0.9.6 can't read the
  message and still just times out.
- **Co-operative mode.** Play the story missions together over the network. Every player must reach the exit
  before the level ends, and the doors, lifts, switches, pickups and creatures are shared between machines.
  A guest joining a level already in progress is caught up behind a short loading screen.
- **Ripley speaks.** The original's speech lines play: when you're hurt (depending on how much health you have
  left), when you're killed, on some weapon changes and flamethrower bursts, when the queen is hit or killed,
  when a face hugger dies, at a battery switch with no battery, and while a face hugger has hold of you. Not in
  network games, as in the original. (Needs the SPCH files from the disc.)
- **The MU/TH/UR boot screen.** Everything the start-up does (finding the game, installing, ripping the music,
  patching, cleaning the levels) is now typed out on the game window in the MU/TH/UR style. Any key skips the
  typing.
- **A music playlist** (`extra/AlienTrilogyPlaylist.m3u8`) ships with releases, for playing the soundtrack
  outside the game.

### Closer to the original

**Pickups and weapons**
- **What carries into the next level is now the original's**, checked in its code:
  - **kept:** the weapons, their ammunition and spare magazines, grenades, seismic charges, the weapon in hand,
    and your health and armour (health and armour used to go back to 100 and 0 every level);
  - **taken away at each level's start:** batteries, shoulder lamps and night-vision goggles (and one still
    lit), and the Auto Mapper - the port used to keep these;
  - **a failed mission** is played again with what you had when you started it, not what you had left;
  - a level started from the main menu, or dying and going back to the menu, starts afresh: the pistol, full
    health, no armour.
  The Modern option "Keep Items" is gone: it was added on the mistaken belief that the original took the
  weapons away each level.
  Saves keep health and armour too.
- Pickups give what the level says: the amount into the loaded rounds and the spare magazines separately, each
  capped. They had been giving far too much (a box of 10 shotgun shells gave 110, a fuel can 530).
- Picking up magazines for the pistol, pulse rifle or smart gun in hand starts a reload.
- Firing and exploding barrels light up the level for a moment, as in the original.
- The weapon in your hand is drawn in the light of where you stand.
- The flamethrower's flames converge, as in the original.

**Creatures**
- A creature coming into play can't be hurt for a moment, as in the original. This is why the face hugger in
  the first level's middle row of barrels now leaps out instead of dying in the blast. Face huggers from eggs
  are the exception: they can be shot at once.
- Handlers walk at the original's speed (they were twice as fast).
- 29 vents on L122, L222 and L232 now start working at the beginning of the level; before, they waited for a
  trigger that never came.
- A colonist whose chestburster has come out can no longer be hurt.
- Creatures seen mirrored stay mirrored when they flinch or die.
- Face huggers no longer show through wall vents.

**The player**
- Dying: the view sinks to the floor before the death video, instead of cutting straight to it.
- In a network game, the pause between hits you can take is the original's shorter one.

**Sound**
- Taking a hit makes a sound again (a metallic clank when armour takes it).
- Blasts, barrels and breaking crates are quieter the further away they are.
- The pulse grenade's throw has its sound.
- The flamethrower's sound stops when you let go of the trigger.
- Picking things up makes a sound (each weapon and its ammunition its own).
- The burning and acid footstep plays only while the floor is actually hurting you.
- The motion tracker sounds as it should: clean blips at the original's sweep rate, not a crunchy doubled tick.
- The shotgun's sound plays once per shot.

**Levels and graphics**
- Each level plays its own music track. 27 levels had the wrong track; level 909 now has music.
- Doors are lit as in the original, not by the room they're in (some inner doors were much too dark).
- Animated textures run at the original's speed.
- 31 objects turned at an angle (L211's double crates, L141's steel coils, crates on L311 and L381) now face
  the right way.
- Switches are drawn at their original size.
- Flames and steam are drawn half see-through, as in the original.

**HUD and messages**
- Messages type out one at a time, a letter at a time, with a blinking caret, as in the original.
- "Battery required" shows at a battery switch when you have none.
- The speed and shield boxes count down in the original's units, not raw ticks.
- The item boxes are wide enough that a number never runs into its label.
- Messages start at the original's place when the Live Minimap is off.

### Fixes

- The face hugger on your face now reaches the bottom of the screen in every frame; a two-pixel strip of the
  world used to show under it. As each frame loads, the two empty rows under its drawn art are filled with
  copies of the art's last two rows (ALTEngine only; the game's files are unchanged).
- **Choosing the game folder:** no crash on a folder name with unusual characters; the SELECT button works with
  Windows display scaling; the path box can be clicked or typed into (Tab, Ctrl+V, Enter).
- **Installing from an existing installation** (not just the disc) now works, into ALTEngine's own folder; the
  original game's files are never changed. Repackaged releases missing some of the disc's files are reported
  clearly rather than half-installed.
- The level override geometry loads again; the old door-gap overlay (-GAP files) has been removed.

### Under the hood

- Every routine of the original game was compared with the port, one by one; this release carries the first
  batch of fixes from that audit. The rest are listed in `Notes/what-remains.md`.
- Every byte of the level files is now accounted for.
- Code clean-ups: duplicate code merged, stricter compiler warnings, and more tests (283 now).

---

## 0.9.5 and earlier

Not recorded here. See the repository's history and `Notes/` for what came before.
