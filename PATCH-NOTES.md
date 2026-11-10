# ALTEngine patch notes

What changed in each version of ALTEngine, newest first. The version number lives in `ALTEngine/VERSION`;
it goes up with each release, and each release gets a section here.

The aim stays the same: with every Modern option off, the game plays as the 1996 original does. The Modern
options (Options > Modern) are the deliberate departures, each one off until you turn it on.

---

## 0.9.8

Everything since 0.9.7.

### Fixes

- **Cutscenes in other languages** (reported by Pox83). With French, Italian or Spanish chosen, the game looked
  only for that language's videos, and skipped every one the disc or installation didn't have. It now plays
  the one that is there: the chosen language's, else the English one, else another language's. The same goes
  for the mission briefing text and the panel graphics, which were left blank.

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
