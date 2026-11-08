# What remains (after zip 120)

Everything still to do, gathered from `engine-remaining-work.md`, `review-and-plan.md`, `uv-review-log.md` and
what came up in zips 71-79. The last section is the final step: the missing faces and a triple check of the
textures, done once everything else is in.

## 1. Check in play

Confirmed by Edward: doors, pickups, the window glass, the cryopods, the older list (3.5.1, the resupply
countdown, Save Anywhere, the assessment, music per level, the vent jets), the thrown seismic charge (zip 83)
and its explosion (zip 84).

Noted in play on L321 (Edward, after zip 107), face numbers as the Face Index Display shows them with Level
Cleanup off (the vertex lists were taken with it on or off as marked; matched to the fixed file):
- **Taken out by hand** (data/LevelCleanupExtra.json): the lift faces a sight line still reached in the two inner
  lifts and at (6,15) - 1052, 1053, 1319, 1320, 1322-1332, 3169, 3170, 3408, 3646, 3876, 3878, 4165, 4166. Of the
  27 the list named, 2961, 2962, 4163 and 4164 were not among them: still to look at.
- **Texture to check, fix unsure** (vertices taken with the cleanup off): 6080 and 6081 (cell 36,52) and 6061 and
  6062 (cell 31,52), triangles with texture 126, routine 1.
- **L322 and L323** have the same badly textured faces (Edward). L323: the lift behind the bottom door (cells
  31-36,64), faces 7109, 7112, 7118, 7123, 7128 and 7135, taken out by hand too. L322 (Edward's FaceLog, zip
  110): 57, 83, 211, 3062-3069, 3316-3319, 3556-3558, 3785-3787, 4059-4061, 4063, 4286-4289, 5775-5788, 6592,
  6717, 7109 and 7135 by hand, and the central room behind the gate at (31-36, 50) (gate tiles 5864-5866,
  5870-5872, 5876-5878, 5883-5885, z = 7936) as a `rooms` entry: every face crawled from the gate tile 5864
  goes, about 1117 of them. Checked by dense sight lines from every reachable cell: none of them can be seen.
- **L323-L325** (Edward's second FaceLog, zip 111), all by hand in data/LevelCleanupExtra.json, matched to the
  fixed files by corners and texture: 40 more on L323 (46 in all: the lifts at (31-36, 64) and round (-6400,
  -7000) and (-1000, -8700), and the gate's doorway at z 6912-7936), 50 on L324 and 38 on L325 (the same
  lifts, the side lifts at x = +-15616 and the corner lifts). 14346 faces in all; the view walk is unchanged in
  every level. Next: Edward flies round every level for strays.
- **Strays** (Edward's third FaceLog, zip 112), by hand: the black (texture 0) strips at the map's far ends on
  L112 (4), L113, L114 and L115 (7 each: 0-2 and 8829-8832), L122 (4) and L131 (4), and 24 more on L323 and 2
  on L325 (the lifts at x = +-15616, round (-11800, 13800), (5600, 5600) and the top lift). 14405 faces in all;
  the view walk is unchanged in every level. Edward thinks that is all of them.
- **L361 is off the level list** (zip 112): the unused, unfinished copy of another level, which does not load. Its
  map was never copied at install, but data/LevelManifest.json still listed it, so Level Select showed it. It is
  not in the original's campaign (Progression's CAMPAIGN_ORDER), so nothing else changes; 361GFX.B16 is no
  longer copied either (DiscFileManifest.json). MusicPlayer's ORDER keeps "361", as that is the original's table.
- **L325's twin ceiling face** (zips 114-115): face 4253 (texture 103, at y 5630 over (1280-1792, 768-1280),
  routine 0) is wound like a floor, facing up; face 4251 lies on the same four corners wound the other way, as
  every ceiling face round it is. Zip 114 took out 4251, which left only the wrong-facing one (Edward: "facing
  the wrong way"); zip 115 takes out 4253 instead, which leaves one ceiling face facing down.
- **The first cell's first face** (zip 116; Edward, L325: a face missing between 0, 2, 3 and 23). Record 0 is
  not a blank in any level: it is the first face of the first cell's run, which the cell's start of 0 hides
  with the rest. Giving the cell back from record 1 left it out, so every given-back first cell lacked one face
  (L325's diagonal wall at (-1792..-1280, 1536..2048, -17152..-17664), texture 307). Apply now puts a copy of
  record 0 at the head of the run (record 0 stays, as the start of the cells left empty, its corners now kept
  valid too). The list does not change; installs remake their cleaned copies (the state file's "apply", now 2).
  Header counts (vertices, faces), MAP0 and FORM sizes are written for every cleaned file, as before.
  The 10 levels said below to lack their first cell's faces (L111, L113-L115, L155, L162, L311, L331, L900,
  L908) do not: in each, record 0 lies in the first cell and ends its run (0x80 set) - the cell's one face, a
  texture 0 strip on the map's first row (texture 280 on L155). Giving them back is the same copy, with a
  judgement of whether it is ever seen; to do with the face additions, after the removals.
- **L141's end-room floor** (zip 119; Edward's FaceLog, then: "compare with the floor of L111's spawn"). The
  strip is one 128 x 32 picture in four slices - end, middle, middle, end - which L111 uses whole (299-302).
  L141's sheet had only the first two (221, 222), so no layout of them could be seamless. The two missing
  slices go into 141GFX/903GFX's unused slots 223 and 246, made of L141's own colours (each texel the L141
  texel where L111's first two slices match its surroundings; averages within half a step of 221/222's), and
  the first four rows become 221, 222, 223, 246 across the room, unturned like L111's; the last row is 73.
  L903 (made from L141) has the same room and sheet page and gets the same. The first fix that patches a
  texture sheet. See Patch/L141LEV.txt. To check in play.
- **L371's lift at the start** (zip 117; Edward: a lift visible from the start, overlapping faces, that the
  original never shows). Lift 6, at (97, 72-75) beside the start, is used: trigger action 15, on the cells at
  x = 90 (71-76) the way out, runs logic 19 (op 7, add 1) once, and with threshold 1 the lift rises its 31
  units from floor 224 to the ceiling (255), sealing the start room behind the player. It names model 5, but
  L371 has five lift models (L000-L004): the original fills its 32-entry model table (0x248354) from the L0
  sections in order and leaves the rest 0, and FUN_00041ba4 draws nothing for a lift whose entry is 0. The port
  fell back to model 0 (a 4 x 4 platform). Now such a lift draws nothing, as in the original; it still moves
  its cells' floors. Only L371 has any: lifts 6-26, 21 of its 27. Nothing in the file changed.
- **L909's holding rooms taken out by hand** (Edward's FaceLog, zip 115): 163 faces, the floors, walls,
  ceilings and lift-door dressing of both rooms behind the corridor's doorways (the doorway faces themselves,
  textures 26 and 0-5, stay as the corridor's wall). 14581 faces in all.
- **L909's holding rooms** (zip 114), dressed as lifts that do not work: the start room (1-3, 37-40) and its
  mirror at the corridor's east end (82-84, 37-40). The start room is sealed again (the testing patch that
  opened (5,37)-(5,40) is undone: Patches.json now sets them back to 01, which also puts back an install the
  old patch changed). The east room was open: (80,37)-(80,40), in front of its painted lift door, had
  attribute 0 where the start room's have 1, so a player could walk through the paint, open door 0 at (81,38)
  and walk in. They are set to 1 too. Edward picks the rooms' faces by hand.
- **Level Select on a multiplayer map** starts at a random path node as a multiplayer game does
  (GameplayScreen, MultiplayerSpawn; node 0 skipped on L909 by its digits, not the session's). The cleanup
  tool's reachable cells on a multiplayer map now start from those path nodes only, not the header's start.
- **Next, from Edward:** textures that do not match their neighbours, faces drawn two-sided that need not be.
- **The eye keeps clear of walls** (zip 110): the mover holds the player's centre PROBE_LEAD (200) short of a
  wall cell, ahead and to each side, so the sight-line test now takes the eye no nearer than 190 to a side of
  its cell that borders a wall cell (8 elsewhere). Before, an eye 49 units from the wall beside L322's gate saw
  into the room through the gap at its left. It also lets a few more lift faces go on L322-L325 (14218 faces in
  all, from 14158).
- **Face logger (kept, Edward):** with Testing > Face Index Display on, every Fire 1 press appends the face under
  the crosshair to `FaceLog.txt` beside the executable: the level, the face's vertices as the display shows them,
  then its number, texture, routine and corners (GameplayScreen's LogShotFace). The corners place a face whether
  the level was cleaned or not. It stays in, with the index display, for finding a face quickly.
- **Faces with no texture**, to look at once the other levels are checked: the triangles with texture 183 at the
  lift doors - 2949 (14,31), 2954 (15,31), 4151 (14,36), 4156 (15,36), 955 (31,14), 1118 (31,15), 976 (36,14),
  1131 (36,15), 4302 (53,36), 4297 (52,36), 3079 (53,31), 3074 (52,31). Routine 1, except 4151 and 4156 (5).

Changed in zips 106-107, to check: **Level Cleanup is worked out once, not at install, and trims face by face**
(Edward: L321's trim left the faces behind the door walls; then "we shouldn't be putting all this work in client
side", and the lift crawl).
- **Worked out once:** `ALTLevelCleanupList <CD folder>` (src/Tools/LevelCleanupListMain.cpp,
  Game/LevelCleanupPlan) judges every level as the fixes leave it and writes `data/LevelCleanupList.json`: per
  level, the CRC32 of the file it judged, the face records to take out, the cells that become FF FF FF FF and the
  first cell to give back. The install only applies that (Game/LevelCleanup::Apply, a few milliseconds a level),
  and only to a file with that CRC; another file is reported `[skipped]` and left as the fixes leave it. **Remake
  the list whenever a Patches.json fix changes a level** (run the install with the option off, then the tool on
  its CD folder; about two minutes). A cleaned copy made from an older list is made again.
- **Facing away:** a one-sided face (routines 0, 1, 2, 8, 10) whose front no place the eye can be in a cell that
  draws it ever looks at goes. A quad is judged by all four of its triangles (bent quads show two sides).
- **Sealed lifts (L321-L325):** from each lift door (its six tiles, textures 254/255, 262/263, 270/271) the faces
  behind it are crawled corner to shared corner (within 3 units - the corners are often a unit or two apart),
  staying behind the door's plane. A crawl that stays lift-sized (at most 120 faces, 2600 units deep; a lift is 74
  and 2048), touches no cell the player can reach and sits behind a door with no clear texel is a sealed lift; its
  faces go where no sight line reaches them (sight lines alone, not the walk's cones - that takes the slanted faces
  just behind each door's corners that zip 106 could not). Taking them whole was tried first: on L321 it got all
  of them, but on L322-L325 it took 50 lift-floor faces just inside doors that a low eye (a dying player's) sees
  under the door's bottom edge. A lift a level uses is reached, and a door that opens onto more than a lift crawls
  far more than 120 faces: both are left.
- **What a sight line still reaches** is listed, lift by lift, in `Notes/level-cleanup-lift-remainder.md` (face
  numbers as the Face Index Display shows them with the cleanup off, and corners). Any that should go after all:
  name them in `data/LevelCleanupExtra.json` (by face number, or by corners, which do not move when the cleanup
  renumbers) and run ALTLevelCleanupList again.
- **Cut-outs that are solid:** every cut-out texture on L321-L325 (the lift doors drawn with routines 4 and 6
  among them) has no clear texel, so they hide what is behind them; they now count as opaque, from both sides,
  in the hidden-face test and for the lift doors (cut-outs elsewhere, the vents and grilles of L111 say, have
  hundreds of clear texels and still count as see-through).
- **Hidden:** an opaque face in a cell the player cannot walk into goes if, from every reachable cell whose walk
  draws it, sight lines from points over the whole cell (the walkable half of a diagonal cell; every eye height
  from dying to a Modern jump; slopes and lifts) to points over the face all hit opaque faces facing the eye
  (`Game/LevelOcclusion`, now only run by the tool). A blocker counts only when its cell is drawn at every facing
  and screen width (a 4:3 window up to all round, as Free Look's steep pitches give) at which the face's cell is
  drawn with that line on screen; a line must pass 8 units inside a blocker's open edges (wall tops, door sides);
  and a face with any see-through face between it and the eye's cell is kept. Doors, lifts, windows and crates are
  objects, never counted as blocking; nothing in a level file that blocks can move.
- **The walk is untouched:** a drawn cell left with no faces starts at 0 (the blank record, which draws nothing -
  the first cell's own trick) rather than FF FF FF FF, because the walk only spreads through a cell's zones while it
  has a start. Checked: from every reachable cell of all 45 levels, at 16 facings and four cone widths, the cleaned
  levels draw exactly the cells the fixed ones do.
- **Checked independently:** every face removed from a drawn cell was either confirmed facing away, or re-tested
  with about six times the sight lines against the cleaned level's own faces (solid cut-outs blocking). For zip
  107's list, two faces are reached that way, both on L323, just inside a lift door, from 13 cells away under
  the door's bottom edge; the port's walk never draws their cell with that line on screen, so it never shows them
  (the check ignores the walk's cones). Everything else removed is out of every sight line. (The check caught two
  faces on L113's top edge in zip 106, seen over a wall top only by a jumping player nine cells away - the 8-unit
  margin fixed it - and the 50 lift-floor faces of the lifts-whole try.)
- Per level: `Notes/level-cleanup-test-list.md`.

Changed in zip 104, to check: **Level Cleanup** (Options > Modern > Testing > Level Cleanup, off by default;
`Game/LevelCleanup`, run from `Bootstrap/InstallPipeline`). At the next start, in this order: Update Fixes ->
the option -> Trim -> Add Faces.
- **Update Fixes:** a level cleaned before is put back as the fixes left it first (`<file>.untrimmed`, the
  cleaned copy set aside as `<file>.cleaned`), so every model removal and byte patch runs against the layout it
  was written for.
- **Trim:** faces the original can never draw are taken out, with the vertices only they use (section 6b's
  survey, now in code): 311 KB over the 45 levels, most of it L321-L324 (the walled-off core) and L902 (the
  outer rings).
- **Add Faces:** the first cell's faces are put back where they are already in the file (31 levels; L321-L324
  and L902's first cells can never be seen, so theirs are trimmed instead). The 10 levels whose first cell has
  no faces in the file are left for the insertion step.
- Every face of every cell that can be drawn is checked identical before and after (texture, routine, light,
  corners); cleaning a cleaned file changes nothing; on -> on -> off leaves the files byte for byte as a run
  with the option off. A cleaned copy is reused while the level it was made from is unchanged (its CRC is kept
  in `CD/LevelCleanup.json`); a fix that changes a level makes it cleaned again.
- **Off** puts the files back as the fixes left them, so the original face and vertex numbers (the ones
  Patches.json, the review log and the Face Index Display use) are there to work with.
- ALTViewer's patch button does not know about it.
- **What to test, level by level:** `Notes/level-cleanup-test-list.md`.
- To trim out later (Edward): the .untrimmed backups and the menu option (a plain boolean was meant).
- **L321's elevator** (Edward asked about the red cell in it): the room at (18-23, 42-45) is L323's way out (its
  end-of-level trigger covers (18-23, 43-44)); in L321 it is walled in, (23,44) is one of its wall cells, and no
  cell the player can reach ever takes it in. A check of the method: the original's own cones, at 64 facings
  from every reachable cell, never draw a cell the full-circle walk does not, so the trim is if anything short.

Changed in zip 103: **a review of the network, bot and trigger code** (no change to play):
- Every message from another machine is checked before use: a player slot past the roster (a Welcome naming
  slot 200 would have indexed past it), a position or velocity that is not a finite number, or a shot of a
  kind no player fires (acid blood, vent jets) is refused (`Net::Protocol`, `Sane`).
- The Join list counts players only: a game whose places are bots no longer shows as full.
- Bots look for a target nearest first and stop at the first clear line (one ray most ticks, not one per
  player), and their path search reuses its buffers.
- Bot settings are reset with each hosted game; small tidy-ups.
- The Doom-style marine (green armour, helmet and visor) made from the Synthetic's frames: a separate
  download, not in the repo.

Changed in zip 147 (version 0.9.6), to check:
- **Pause Weapon Select** (Modern; on in the Convenience and Modernised presets): Enter or Right on a weapon in
  the pause menu's list switches to it, the menu staying open (FUN_00044b48 cases 2-6, the original's). Uses the
  same switch as the number keys.
- **The host's rules in a network game** (`Bootstrap/SessionRules.h`, sent in the Welcome, protocol 7): each
  Modern feature is Shared (the host's for everyone), Off, or Local (`ModernSettings::NetworkRuleFor`); the
  weapon pacing is shared; Disable Enemy Spawn, Ghost Mode and Disable Resupply Timer are off. In force for the
  whole of `playMultiplayer` (main.cpp), through `ModernSettings::IsActive`, `TestingSettings::IsOn` and
  `WeaponPacingSettings::Get`, so the pause menu and the level loop follow it too. The player's own settings
  are never written over (SetFeature reads `IsActiveHere`). To check: host with a feature on and join from a
  machine with it off.
- **PATCH-NOTES.md** in the repository's root: the changes since 0.9.5. VERSION is 0.9.6 (and vcpkg.json).

Changed in zip 148, to check:
- **Enable Cheats is the host's** (Shared, not Off): with it on, each player turns their own cheats on in the
  pause menu.
- **The version check.** Join and Reject are the handshake: read whatever the datagram's protocol byte, their
  layouts never to change (new fields on the end only). A Join carries the player's ALTEngine version
  (`Net::GameVersion`, from ALTEngine/VERSION, compiled into ALTGame); the host refuses a different version or
  protocol with Reject{Version, its own version}, and the player sees why on `MultiplayerNoticeScreen` (also
  for a full game and a host that never answered, `JoinRefusedText`). A version before 0.9.6 can't read it
  and times out as before. Not done: the Join list doesn't show games of another version (DiscoverReply is not
  part of the handshake); it could, with a version field read across versions the same way.
- **To do (Edward):** move **Disable Resupply Timer** from Testing to the Modern options. Which preset it
  belongs in is open, and the Modern list may in time want splitting into categories (play, display,
  convenience, multiplayer...) as it grows.

Changed in zip 146, to check (from the routine-by-routine audit: 15 agents over every function, their findings
checked against the disassembly before anything was changed):
- **Pickups give what the record says** (FUN_000387c0): byte 3 into the loaded counter, byte 4 into the spare
  magazines, each clamped to its own cap. The port multiplied the two and poured the total into both: a box of
  10 shotgun shells gave 110, a fuel can 530. An ammo pickup for the pistol, pulse rifle or smart gun in hand,
  idle, starts its reload (FUN_00040000).
- **The light flash**: firing and an exploding barrel light the level at 0x2000 for a tick (DAT_000b0cc1 bit 4);
  the port had the flash but never raised it. The weapon in hand is drawn in its cell's light (light 0 on a
  flash tick), as the original does (FUN_0003e93c); it only darkens here.
- **Sounds**: a hit plays 0202ripl (3), or 5003metl (0x24) for a cause-3 hit on armour (FUN_0003e4a8); a blast,
  barrel, crate or kill by a blast sounds by distance (FUN_00040c38 mode 1), not at full volume; the pulse
  grenade's throw plays sound 8; the flamethrower's sample stops when the trigger is let go (0x3fbe8).
- **Dying**: the eye sinks 0xc a pass from standing to 0x180, the pitch walking home and the controls dead,
  before the death video (FUN_0003ee4c); the port cut to it at once.
- **Creatures**: the handler walks at half speed (0x2efc0); a record whose bytes 7 and 8 are already equal starts
  in play (29 vents on L122, L222 and L232 waited for a trigger that could never start them); a colonist whose
  burster is out can't be hurt; the mirror bit is kept across a new animation (only the walk, an attack pick and
  the queen's hit change it), so a creature seen mirrored flinches and dies mirrored. The flamethrower's flames
  converge (0x38).
- **Level**: doors are lit by light 10, not their cell's; texture animations show a frame for `speed` ticks, not
  one more; 909 plays track 15 (the word after the music table is static data, not the current-track slot);
  objects with an odd rotation (31 of them: L211's double crates, L141's coils, crates on L311 and L381) turn
  the other way (0x800 + r * 0x200); switches are drawn at (0.75, 0.85, 0.75), the triple once wrongly applied to
  every object; the speed and shield boxes count in 0x1e-tick units; the burning/acid footstep plays only when
  the floor is actually hurting (not with the speed item or shield running).
- In a network game the hit cooldown is 8 ticks, not 0x1e.
- Left for later, checked but not done: the flame cue the original skips once after a release (0x5b25c);
  reusable switches (35 switches have 0xff uses; the thrown-switch task 0x380f4 counts down and goes back to
  0x3846c when +0x1a is 2).

Changed in zip 144, to check: **a creature coming into play can't be hurt for 0x40 ticks** (Edward: with the
Modern options on, the hugger in L111's middle row of barrels died in the chain instead of leaping out). The
original does give this grace: FUN_0002e638, which every creature coming into play goes through (the level's
start, a trigger, a crate or barrel, an egg), sets +0x4e to 0x40 (0x2e7dc), and FUN_000313b0 refuses damage while
it runs. The port never set it, and No Hit Immunity ignored that cooldown altogether. Now every creature starts
with it (`Enemies::BringIntoPlay`), and the option lifts only the cooldown a hit leaves, as its description says.
Zip 145: **not for a hugger from an egg** (or a colonist's burster). The original gives them the grace too
(FUN_00035530 goes through FUN_0002e638), but a hugger flying at the player that can't be shot for 1.8 seconds
feels wrong (Edward) - the port's own choice, in `Enemies::ReleaseFromEgg`.

Changed in zip 143, to check: **facehuggers no longer show through wall vents** (Edward: L111, and likely other
levels). The original draws a creature only when its cell's zones meet the zones the view walk took in
(FUN_00030304); a duct's cells - the cells with no face, where vent creatures wait - have none on L111 (all
5,774 of them, and all 11 of its duct huggers) and seldom share one with the room elsewhere. The port ran that
test only with the distance fade on, so with Render Distance (on in Modern On and both presets) the huggers
showed through the grilles. A creature in a duct cell now takes the test whatever the settings; creatures in
ordinary cells keep the old behaviour, so Render Distance still shows far-off creatures.

Changed in zip 142, to check:
- **Level music.** The music table is indexed by the original's level number, and that number is now traced:
  it is the index of the level file table at 0xa4dec (0x1069b reads it with the level number), whose 45 entries
  run 111, 112, 113, 122, 131, 114, 141, 115, ... (`Formats/LevelOrder.h`; the campaign order is its first 35).
  The port had guessed the manifest's order, which gave 27 levels the wrong track. Now the four levels on the
  main menu's track 2 are the bonus levels 113, 114, 115 and 155. 909 reads the word after the table, which
  is static data holding 15, so it plays track 15 (the current-track slot is the word after that).
- **Messages without the Live Minimap** start at the original's place, (0x10, 0x3a), still below the item
  boxes when there are any; with it they stay under the map.

Changed in zip 141, to check (from a second pass over the decompilation):
- **Ripley speaks** (new, `Audio/Speech.h`). The original has a speech bank, SFX/SPCH.TXT (the name is the
  pointer at 0x4de8, just before the per-level lists), played by FUN_000572e4: never in a network game,
  and not again for 0x8c passes after a line. The lines come on: being hurt off health (FUN_0003e618 - a
  remark by how much health is left, one for dying); weapon keys 3 and 4, half the time (FUN_0004d438);
  firing the flamethrower, 5 in 16 (FUN_0002c0a0); the queen hit (1 in 8), under 0x1a health, and killed
  (3 in 4); a face hugger killed (half the time); a battery switch with no battery (half the time); and,
  while a creature holds the pounce lock, a 4-in-8 chance each time a second cooldown runs out
  (FUN_00057338). Which line each id plays is the table at DAT_000ae0ac. NOT HEARD: the SPCH files are not
  in the test data here, so this wants a listen - if SPCH.TXT is missing, nothing plays.
- **Pickups make a sound** - the port had none. FUN_0003c6f4 plays 0x28 for most things and a weapon's
  own report for that weapon and its ammunition (`Pickups::SoundFor`).
- **"Battery required"** is printed when a battery switch is used without one (FUN_00036f4c); the port
  refused silently.
- **The message panel's timing is the original's** (the task at 0x3b0c8): one message at a time, a
  character a pass, a caret after it that blinks four on, four off once typed, held 0x28 passes; four
  slots, the newest into the highest free one and the lowest shown next, a fifth dropped. It used to show
  up to four lines at once at guessed speeds. Which action prints which notice is now traced (comment in
  `Screens/HudMessages.h`); the port's pairings were right.

Changed in zip 140, to check: **the motion tracker's sound** (Edward: crunchy, not blippy). The sweep is
stepped by the timer interrupt itself (FUN_00039f8c, called from the 69.74 Hz handler), so one sweep is
96 ticks, about 1.38 s; the port stepped it on the alternate world pass, which took twice that. The pulse
(FUN_0003a1d0 -> FUN_00040b2c with EBX set) halves the volume and goes through the general channels
(FUN_0005aed9), which won't start a sample still playing; it was on the player's channel and restarted every
sweep. The idle sample (1002moti) is four clicks 1.31 s apart over 5.2 s, so each restart landed on its third
click - a doubled tick. Now it plays through at half volume, and the contact tone (1003moti) likewise. The
dish's sweep animation runs twice as fast as before, which is the original's rate.

Changed in zip 102, to check: **no motion tracker in multiplayer**, as in the original (Edward). Its tracker
routine (FUN_0003a1d0) runs only with both network flags clear (DAT_000ae0a4 and DAT_000ae0a5), so neither
the dish nor its sweep is there in a network game. The original also moves the health bar to the bottom
right in multiplayer (FUN_0003aac8 / FUN_0003a7a4: 0xc6 in place of 0x16 when both flags are set) - left as
the single-player layout on purpose (Edward).

Changed in zip 101, to check: **bots a little slower** (Edward: they moved a little too fast). They walk
0x40-0x60 a tick by skill (was 0x50-0x78; the player's forward walk is 0x78), and sidestep at half that, as
the player's strafe is halved (it was their full speed, twice the player's).

Changed in zip 100, to check: **multiplayer starts (Edward: every map loaded into empty space).**
- **Players now start where the original puts them: a random path node.** The four Synthetic records the port
  used as starts (zip 94) are parked OUTSIDE each map, in cells with no faces - they are the bodies the
  original draws the other players with (their task, LAB_00035290, picks walk 0-7, hit 8/9, death 10/11 and
  firing 12-19 by view), not starts. Traced (the spawn at 0x402fc): in a multiplayer game the player goes to
  path node N, at the node's cell corner (node x, z << 9), with 100 health. N is random: masked to 0-127 at
  the level's start (0x108b6) and to 0-63 after a death (0x1034e), drawn again until it is below the level's
  node count; on L909 (the original's level 0x2c) node 0 is taken as node 1. Bots come back the same way.
  Every node of the ten maps was checked: all are on the map, in open cells, except L909's node 6, which is
  in a wall cell - OURS: such a node is drawn again. Tested: three bots on L900 all spawn on the floor
  (height 2048) and find each other.
- The single-player start is unchanged (the header's).

Changed in zip 98, to check: **doors that open as you walk up to them, and the player's trigger runner.**
- **Walk-up doors**, as in the original (Edward: match it). A trigger's "unlock door" (command 1) runs as
  the player walks onto its cells, no key needed; at the door's threshold it opens. Most doors in L113-L115,
  the multiplayer maps and the L3xx levels open this way, and on L111 door 5.
- **The player's trigger runner is now the original's** (FUN_00040ff0, `Triggers::OnPlayerTick`), run every
  tick with the use key's state instead of a walk-on run plus a separate use-key run:
  - on a new entry the effects, unlocks and lifts run; a switch (4) and a door's final opening (6) run only
    while the use key is held;
  - a chain that fails clears the latch, so it is tried again next tick - a door still moving takes the
    unlock as soon as it stops, and the use key works whenever it is pressed on the cell;
  - standing on, with the key held, only the switch and the opening are tried again.
- **L111's middle room** (Edward: the far door seemed to wait for the room to be cleared, then only opened on
  walking back towards the first door). Traced: there is no clear-the-room rule anywhere in the trigger
  commands. Door 4 (the way in, at 40,73) is opened with the use key from trigger 7's cells outside it; its
  open time is 40, so it holds 160 ticks and closes once you are more than 1024 away, and as its trigger
  cells are all outside, it can't be opened again from within. Door 5 (the way out, at 24,73, open time 0,
  stays open) is unlocked by trigger 13, the line of cells just inside door 4 (39,71-76), which also turns
  two wall panels (texture animators 5 and 6). In the original that happens the moment you walk in. The
  port made trigger doors wait for the use key, so door 5 only opened when you pressed it on that line -
  which is where you stand when going back to door 4.

Changed in zip 97, to check: **creatures coming out of wall vents, and the level scripts traced.**
- **Creatures in the ducts behind wall vents now come out**, as in the original - L111's huggers among them,
  and about 400 creatures across most levels (L112, L122, L131, L141, L154, L161-L162, L211-L213, L222,
  L231-L232, L262-L263, L331, L351, L381, L391). How the original does it, traced:
  - A duct is a cell with no face of its own: its first four bytes are FF FF FF FF on disk, and the level
    load writes 0 there in place of a face pointer (0x27983).
  - "Has the creature noticed the player" (FUN_00033a70) skips its view test for such a cell. A creature in a
    duct notices the player by distance alone - inside half its wake range, or all of it while the player's
    gun is still loud - and becomes alerted. (The cone test, FUN_00033a1c, has no such exception.)
  - It then walks to the player through the vent: a cell of attribute 1, which is a wall to the player but
    not to a creature (the creature mover, FUN_000315f0, refuses only 0x14-0x2c).
  - A trigger's "spawn monster" (FUN_0002f224) only adds to the creature's counter and spawns it at the
    threshold; it never alerts it. The port's guess (alert anything released into a duct) is gone.
  Tested on this machine: standing in L111's room at (19,82), the hugger in the duct at (15,82) noticed the
  player at 2048 units (half its wake range is 2433), came through the vent at (16,82) and leapt.
- **Creatures set off triggers** marked for them (flag 0x04, FUN_0004129c, run by the creature mover each
  time a creature crosses into another cell, unless dying). Only L231 uses it: its lifts 2 and 3 are called
  by a creature walking onto their cells.
- **A round, a grenade and a creature pass over a door's final opening (command 6) as well as a switch
  (4)**, and the chain's result carries over a pass-over, starting as a failure (see "Level scripts" below).
  Only two chains in the game are affected: L212's trigger 12 (shot-only, "open door 3") does nothing when
  shot, as in the original; L331's trigger 9 is a player trigger and is unchanged.

Changed in zip 96, to check: **bots, player limit, and the player look preview.**
- **The preview showed a hit frame for some views** (the Synthetic's 2, 3, 4 and 7, the Guard's 1, 2, 4, 6
  and 7). It decoded each view's first frame on its own, and a single frame can decode cleanly from the wrong
  section of the file; a whole animation only fits its own section, as in play. It now decodes each view's
  whole walk, walks on the spot and turns a view every half second, each frame stood on its own anchor.
  The layouts, from Edward's files: SYNTH.B16 has 20 sections, one per animation, all eight walking views
  its own; SOLDIER and GUARD have 8 and HANDLER 9 - five walking views (5-7 are 3-1 mirrored), then the
  hit and death animations. Their animations 11-13 (Handler 12-14) have no frames in these files at all.
- **Player limit** (Start Multiplayer Game): 4-8, bots included; the original's four is the default. (Starts: see zip 100 -
  a random path node, as in the original.) The protocol is now version 3 (eight places, and a bot flag in the roster).
- **Bots** (Start Multiplayer Game): NONE, or 3 up to the limit less one (always room for the host), at skill
  1-5. Saved with the limit (MultiplayerPlayerLimit, MultiplayerBots, MultiplayerBotSkill). The host runs them
  (`Game/Bots.h`); each takes a player place, so everyone sees, shoots and is shot by them as by a player, and
  their frags count in the roster. A player joining a full game takes a bot's place, and a bot comes back when
  they leave. Named BOT 1, BOT 2 and so on, in the four looks in turn.
  How they play - a variant of the Synthetic's task: follow the path network toward the nearest player
  (the flow field's walk, FUN_000281cc / FUN_00033114), and once there is a clear line (as FUN_0003231c, out
  to 0x4000) turn to them, wait their reaction time and fire a pulse rifle round from the hip (0x240, a
  gunner's height); close in they sidestep. They move with the player's own mover, take a quarter of a
  round's damage as a player does, and come back after about three seconds. Skill sets reaction time
  (1.0 s at 1 to 0.2 s at 5), aim error, rate of fire, turning and walking speed (up to the player's walk).
  Tested on this machine: five bots on L900 for 40 seconds found each other and the host and scored 16 frags.

Changed in zip 94, to check: **multiplayer, players' looks, starts, shots and chat.**
- **The players are Synthetics in the original.** Every multiplayer map (L900-L909) has exactly four
  creature records, all type 0x0d, the Synthetic; FUN_0002e638's case 0xd spawns them with a task of their
  own, and in a multiplayer game types 0xb and 0x0a are left out of cell occupancy. They are parked
  outside the map: the other players' bodies (corrected in zip 100; they are not starts).
- **Player starts:** WRONG in zip 94 (the records outside the map, so every map loaded into empty space);
  see zip 100. The records are not spawned as creatures.
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
2. **Creature sound cues** are cut past 0x1400 or out of sight (the original only quietens them) - asked for
   in play. (A creature in a wall vent's duct now notices the player as the original does, zip 97, instead of
   coming out alerted.)
3. **Casings** vanish at the floor; the original's fall through it out of sight.
4. **Spawn grace** (zip 145): a creature out of an egg or a colonist can be hurt at once; the original gives it
   the same 0x40 ticks as every other new creature.
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

Done in zip 139 (a review of zips 121-138, and a scan for repeated code - every run of 8 or more lines
appearing twice):
1. **One copy each now:** the Modern/Testing settings read (level start and after the pause menu -
   `readGameplaySettings`); the light tick's two GPU vertex uploads (`UploadVertexSpans`); a mesh's
   buffer-and-texture binding (`BindMesh`, five draws); the texture animator's program walk (`RunToFrame`,
   shared by Prime and the tick); the option descriptions' word wrap (`Bootstrap::WrapText`, boot and pause
   menus); the full-screen background loader (the boot menu and the briefing each had their own copy -
   now `LoadSplashBackground`, with `LoadMenuBackground` a one-line call); a model's centre and radius
   (`BoundsOf`, single and batch loads);
   `ClearReadOnly` (`RetryOpen.h`, for the installer and the patcher); the menu colours and the pulsing
   selection colour (`Screens/MenuStyle.h`, five screens); the round's particle looked up once.
2. **A crash path closed:** the level override lookup had its own case-insensitive file search, which could
   throw on a folder name the system code page cannot hold (the Windows crash fixed in the installer in zip
   128); it uses `FsUtil.h`'s now.
3. **The MU/TH/UR readout** wraps only the rows it shows, newest first, instead of all 400 kept lines each
   frame - which a typing headline redraws once a character.
Looked at and left: the three-item menus' Up/Down/Enter switches (a few lines each, the items differ); the
language tables' identical first lines (data).

Done in zip 141: the option descriptions' wrap-and-draw (boot and pause menus: `Bootstrap::DrawDescription`);
a render pass's cleared colour and depth targets (`ClearColourTarget`, `ClearDepthTarget`, two passes); the
menu path's parent, three copies (`Menu::ParentOf`, which also makes an empty path safe in the pause menu's
Enter); a dropped pickup's 0x18 -> 0x15 written out again beside `RemapDroppedType`. Removed as dead:
`HudRenderer::TrackerTick`, `PlayerInventoryState::NextAvailable` (NextSelectable is the one used), and the
256-unit hit box placeholder in DamageSystem.h (the real boxes are ProjectileCollision's).

Done in zip 142: `-Wshadow` is on for GCC and Clang, with every warning it gave fixed (the settings classes'
`config` parameters, and twelve inner variables reusing an outer name); the Windows header's set-up is one
guarded header (`Bootstrap/WindowsHeader.h`) instead of four copies, which also stops MinGW's NOMINMAX
redefinition warning; the level order is one list (`Formats/LevelOrder.h`) for the campaign and the music.

Still open:
- **Splitting `Run`** further (above).

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

The networking, menus, joining, positions, hits and frags are in (zip 93); the players' looks,
seen shots and F2-F9 chat in zip 94, the original's starts (a random path node) in zip 100. Still to come, roughly in order:
1. **The score table and the end of a game:** "Minimum game length", the frag table, and what ends a round.
2. **Pickups, doors and lifts kept the same on every machine** - each machine runs its own level for now
   (seen shots do break the same glass everywhere).
3. **L909's sealed start room.** Done in zip 114: the testing patch is undone and both holding rooms
   are sealed (see section 1).
4. **Over the internet:** works by address if the host's UDP port 27960 is open (firewall allowed, and
   forwarded on the router); there is no NAT help. The README has a boxed note on this ("Multiplayer: the
   port to open"), and the Start Multiplayer Game screen says it under its options.
5. **The rest of the original's packet types** (FUN_00054a60), traced one by one.
6. **More looks**, such as a Doom Guy: a new `Net::Skin` value and its sprites.
7. **Bots:** they don't use doors, lifts or pickups, and don't dodge. Rounds from a player on a client are
   checked against a bot where the client saw it, as for any player.

## 4c. Level scripts (trigger commands), as the original runs them

Traced from FUN_00040ff0, FUN_000414f0, FUN_00041784, FUN_0004129c and FUN_00040e2c, and their jump tables.

**Data.** A cell's byte 15 is its trigger id (0 none). 64 triggers of 4 bytes at 0x248248: flags, first
command, uses (0xff unlimited), 0. 64 commands of 4 bytes at DAT_0024834c: action, next (0xff ends), amount,
target. Flags: 0x01 the player, 0x02 a round, 0x04 a creature, 0x08 a handler's grenade.

**The five runners**, each with its own latch (the id last set off):

| Runner | Called by | Flag | Latch |
|---|---|---|---|
| FUN_00040ff0 | the player's mover, every tick, with the use key | 0x01 | DAT_000b0b56 |
| FUN_000414f0 | the particle task, each live round | 0x02 | particle +0x1f |
| FUN_00041784 | a handler's grenade | 0x08 | particle +0x1f |
| FUN_0004129c | the creature mover, on a change of cell | 0x04 | creature +0x71 |
| FUN_00040e2c | a dying queen (her record's byte 6) | none | none, no uses |

Each: a cell with no trigger clears the latch; otherwise the trigger must have its flag and uses left. "New"
means the latch differs from the cell's id.

**The commands.**

| Action | Does | Runs when |
|---|---|---|
| 0 light (FUN_00029d44), 2 pickup (FUN_0003bb14), 3 monster (FUN_0002f224), 9 texture (FUN_00042fcc) | effect; result = success | new |
| 1 unlock door (FUN_00029594), 5 lift final (FUN_00041e20), 7 lift add (FUN_00041dd8) | result = its answer | new |
| 4 switch (FUN_00036f4c) | result = its answer | the player's runner only, with the use key |
| 6 open door (FUN_00029610) | result = its answer | the player's runner only, with the use key |
| 8 end level | result = success | always |
| anything else (0xff an empty slot) | nothing | - |

The result starts as a failure (ECX cleared) and a command that doesn't run leaves it as it was; the loop
stops when it reads failure. At the end, success uses the trigger once (unless 0xff) and sets the latch;
failure on a new entry clears the latch, so the next tick tries again.

**The port** follows all five (zip 98: `Triggers::OnPlayerTick` for the player's, run each frame). The one
small difference: standing on a cell without the key, the original re-runs a chain that starts with the
level's end every tick, which changes nothing; the port doesn't.

## 5. Larger features

- **Multiplayer:** see 4b.
- **The Modern options, reorganised** (Edward, zip 148): move Disable Resupply Timer from Testing into Modern
  (which preset is open), and split the Modern list into categories once it grows further.
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

## 6b. Geometry that is never drawn (zip 103 survey)

Method (a probe, not in the repo): every cell the player can walk to, flooded from the level's start through
anything that is not a wall to the player (attribute 1, 0x14-0x18; every door counted open, heights ignored, so
it over-counts what can be reached); from each, the original's own view walk (CreatureSight::DrawnCells, the
ring walk of 0x2745b) in every direction at once; and every face whose cell's run is never taken in. On the
multiplayer maps every path node is a start too. Treating doors that no trigger opens as shut finds nothing
more: every unreachable area is walled off, not just locked.

| What | Faces | Bytes (faces 20, vertices 8) |
|---|---|---|
| L321-L325 (the chapter-3 level met several times; its central core is walled off in all but L325) | 672-2706 each, 10024 in all | about 274 KB |
| L902 (rings outside the arena's wall) | 1200 | 35 KB |
| L906 | 28 | 1.1 KB |
| L111 and L900: the four faces just behind the start (cells (36,98)-(36,101), start (39,100)) and three at the map's top edge | 7 each | 300 bytes each |
| L213 2, L371 1 | 3 | - |
| All 45 levels | 11273 faces, 10740 vertices freed | 314 KB of the 18.8 MB of level files (1.7%) |

The 153 faces no cell lists are NOT junk: they are each level's first cell's own faces (section 7, item 2), to
be restored, not trimmed.

Trimming them would mean, per level: drop the faces and the vertices only they use; renumber every cell's
run start (bytes 0-3) and every face's vertex indices; set the dropped cells' run start to FF FF FF FF; move
the header counts and the MAP0 and FORM sizes; and renumber every Patches.json entry and review-log note that
names a face of that level (the L321-L325 and L902 texture fixes do). Best done as one tool pass after the
section 7 insertions, since both renumber the same lists. NOT done: Edward's call.

## 6b2. Finishing the geometry cleanup (Edward's plan)

1. Finish the list: the faces judged by ALTLevelCleanupList plus those picked by hand in play
   (data/LevelCleanupExtra.json, logged with the temporary face logger).
2. When the list is complete, ship it through Patches.json like every other fix: the face (and vertex) removals,
   run starts and first-cell fixes written as Patches.json entries, applied with the fixes. Then
   data/LevelCleanupList.json, data/LevelCleanupExtra.json, Bootstrap's ApplyLevelCleanup, the .untrimmed/.cleaned
   swapping and the Testing > Level Cleanup option go.
3. Do away with the working-out code once the list is compiled: Game/LevelCleanupPlan, Game/LevelOcclusion,
   src/Tools/LevelCleanupListMain.cpp (ALTLevelCleanupList), CreatureSight::DrawnCellList if nothing else uses it,
   and GameplayScreen's LogShotFace. Keep the record of how the list was made in these notes.
4. Then bring the README.md, the patch folder's notes and ALTViewer up to date with it. Patches.json entries and
   review-log notes that name a face or vertex of a trimmed level by number must be renumbered to match.

## 6c. Texture blanking - done in zip 120 (Edward)

Every texel of a level's sheet that no texture the level draws covers takes its page's background: the texels of
the unused textures that no used one overlaps, and the texels no texture covers. Used: what a face names (an
animated face: its animator's frames), every animator program's frames, the door and lift models' faces, and the
faces the Level Cleanup adds (Game/TextureUse). The background is the most common colour of the space blanked,
placeholder magenta left out (Edward: most common, not averaged; it gives the 0xFA of his 371GFX fix). Worked out
by `ALTTextureBlankList` into `data/TextureBlankList.json` (per sheet: its CRC32, the unused textures, each page's
background); applied at install after the Level Cleanup, with the same option, keeping `<sheet>.unblanked`
(Game/TextureBlank). 45 sheets: 4088 of 15100 textures unused, 2,730,471 bytes changed; the sheets deflate 18.8%
smaller (LZMA 19.1%). Report: `Notes/texture-use.md`. The whole routine, in order: `Notes/cleanup-procedure.md`.
(141GFX and 903GFX's slots 223 and 246 hold the end-room floor's slices, zip 119: in use.)

## 7. The final step: missing faces, then a triple check of the textures

1. **Missing faces - done in zip 118**, as the Level Cleanup's last step, after the removals (so the work reads
   back in order: the fixes, then the removals, then the additions, each against the file the step before leaves):
   - **L212, L242, L243 and L161** were record 0 all along - the first face of the first cell's run, which the
     cell's start of 0 hides. Record 0's copy at the head of the run (zip 116) puts them in. L161's interim
     (face 3758 given over to the hole) is set back to its shipped triangle in Patches.json and taken out by the
     cleanup, as the same triangle 3757 draws (3758 differs only in its light, 0x08 against 0x0F).
   - **L211** is the one real insertion: the floor over face 990 (990 is the same quad wound the other way,
     facing down). `data/LevelCleanupExtra.json` names it by corners, texture 264, routine 0, light 0x0B, in
     cell (49, 19); the tool copies it into the list (`"add"`), and Apply puts it at the head of that cell's
     run: the cell keeps its start, every later run start moves up one, a corner with no vertex at its point
     gets a new vertex, and the header counts, MAP0 and FORM sizes follow.
2. **The level's first cell, every level - done (zips 116 and 118).** Its run is the file's first, from record
   0. On 35 levels it goes on into records 1 to n; on the other 10 (L111, L113-L115, L155, L162, L311, L331,
   L900, L908) record 0 is the cell's one face, its run bit set - nothing is missing from the file, only hidden.
   The plan now judges record 0 like every other face (in view, facing a place the eye can be) and gives it back
   as a copy only if it is seen. L111 and L900: out of view. L113-L115: record 0 is the fourth of the black
   (texture 0) strips at the map's end whose other three Edward took out by hand, so `"recordZero": false` keeps
   it out, as for the texture 0 faces of L162, L311, L331 and L908 (to look at in play: flip to true to give
   one back). L155's (texture 280) is given back.
3. **Renumber everything that names a face after an insertion:** later Patches.json entries for the same level,
   the review log, and ALTViewer's fixer.
4. **Triple check the textures:** every level once more with the Face Index Display after the insertions, the
   texture scan run again on the patched files, and the multiplayer copies checked against their campaign levels.
5. **Bring the rest up to date in one pass - done (zip 121).** Every `Patch/LxxxLEV.txt` (and new 141GFX, 371GFX,
   903GFX, L131LEV, L162LEV and L232LEV files) ends with every patch to that file in bytes, shipped -> patched, made
   from Patches.json. ALTViewer's fixer has patches 49-64 (the UV reviews, L161's face 3758 put back, L141/L903's end
   room floor and its sheet slices) and patch 13/14's second quarter-turn, and shows its button until L903's floor is
   in. The README has before and after screenshots of every patch, the cleanup and the blanking
   (`Images/<level>/`), made by ALTEngine's renderer from an untouched copy of the shipped levels. The byte counts
   (`Notes/patching-byte-counts.md`) were measured again from that copy: the first level set used had 13 files that
   already carried patches 0-19.
   Zip 122: the end room floor's far slice (texture 246, in 141GFX and 903GFX) given the strip's grey outline on its
   right edge - its last two columns are texture 221's first two, mirrored, as L111's 302 carries 299's (Edward,
   FaceLog); the blanking list's two CRCs follow. Testing > Level Cleanup is now on by default.
   Zip 123: the HUD's left side laid out as one column (Renderer/HudLayout.h) - with the Live Minimap on, the
   resupply countdown, the CHRG/GREN/ARMR/SPD/SHLD boxes and the typed messages start below the map, in that order,
   and the messages never start inside the item boxes; without it the countdown and boxes keep the original's places.
   Code review and optimisation pass: the texture blanking is cached like the cleanup (a second start renames files
   instead of rewriting 30 MB); a failed swap of a cached cleaned or blanked file puts the uncleaned one back rather
   than leaving the file missing; the live minimap tests each cell's blocking once instead of up to nine times and
   uploads its image only when it changed, its texture kept per renderer; the item boxes come from one list that the
   drawing and the layout share.
   Zip 124 (v0.9.5): an animated texture moving no longer builds the whole level again. Each animated face's vertices
   are one run in its page's mesh (RenderMesh::animatedFaces); a frame change builds just that face with the level
   build's own code (Formats::BuildLevelFaceVertices) over its run, and only the runs that changed go to the GPU. On
   L111 and L371: 1.9 ms of CPU and about 1.3 MB of upload a change become 0.05 ms and about 20 KB. L906's 23
   animators drive no face at all and used to rebuild the level 450 times a minute for nothing. A frame on another
   page (none found on L111, L161, L371 or L906) still builds the level again. Tested against the whole build, frame
   by frame (RenderMeshTests).

## Install size (v0.9.4, Edward's measurements)

The original installed: 129,751,918 bytes (its music streams off the disc). ALTEngine: 548,430,318, of which the ripped
music is 413,606,960 and the Level Cleanup's kept copies (`.untrimmed` levels, `.unblanked` sheets) 31,827,936. The
install itself is 102,995,422 bytes: **26,756,496 smaller than the original**, with no trimming needed.

If it is ever wanted smaller still: FFmpeg is about 13 MB, and the videos need only its TrueMotion 1 decoder and AVI
reader (Duck `DUCK` video, PCM audio), so a cut-down LGPL build would be under 1 MB; once the cleanup is always on,
the kept copies need not be kept; the music would be about half as FLAC.

## Towards the end of the project: trim the bytes nothing uses (ALTEngine only)

Edward's idea (zip 126). Every byte of MAP0 is now accounted for (Notes/unread-bytes-notes.txt), and some hold
nothing: data the editor left that the game never reads, and runtime space the level start rebuilds anyway. A last
patch layer could drop them, run after the Level Cleanup, for ALTEngine's copies only. **Not in ALTViewer**, so its
fixer keeps producing files the original game can run.

What it would save across the 45 levels, as cleaned (1,930,104 bytes, about 10% of the level files):

| Bytes | What |
|---|---|
| 946,018 | vertex +6..7, always 0 |
| 943,554 | cell +4, +7, +12: runtime, rebuilt at level start (FUN_000282c8) |
| 23,040 | the pad byte after each light colour (+3/+7/+11/+15) |
| 5,196 | object +5 (never read) and the always-0 high bytes +11/+13/+15 |
| 3,974 | pickup +5 (runtime counter) and +7 (never read) |
| 2,880 | action group +3 |
| 2,541 | path node +2 (runtime) |
| 1,554 | monster +9 (never read) |
| 966 | lift +4 and +11 (runtime), +6 and +13..15 (never read) |
| 201 | door +5 (runtime) |
| 180 | header +34 (vent types) and +28 (the fixed 64, 64) |

It changes every record's size, so it is a new, ALTEngine-only layout: the loader would read both (a marker in the
FORM header, or a chunk name other than MAP0), the runtime fields would be rebuilt at load as they already are
(LevelLoader zeroes +4 and +12 and rebuilds +7), and every tool that reads or writes offsets (the patches, the Level
Cleanup, the blanking scan, the Face Index Display's file offsets) would have to run on the original layout first,
with the trim truly last. Worth doing only once the patching is finished, since it moves every offset after it.

**ALTViewer must read the trimmed levels too (Edward, zip 137).** ALTViewer's own fixer keeps writing files the
original game can run, but the viewer itself has to open ALTEngine's trimmed copies (the levels an ALTEngine
install actually holds), not only the original layout: recognise the marker the trim leaves, read the shorter
records, and show the runtime fields as the loader rebuilds them. To be worked out with the trim itself.

## Later: patch in the gaps above the doors (and other added detail)

Edward (zip 137). With free look the slit above most door frames shows - the original never let the player look
up at it. The **-GAP overlay is gone** (zip 137): it loaded `CD/OVERRIDE/<section>/<level>-GAP.obj` and drew it
beside the level, but the faces had no part in the level's own lighting, texture animation or cell-by-cell
drawing, needed hand-named materials, and any vertex placed by hand could miss. The full-level OBJ override
(`CD/OVERRIDE/<section>/<level>.obj`) stays - that is expected, for replacing a level's look outright.

The fills are to be **patched into the level data instead**, as an ALTEngine-only patch layer in the same family
as the planned byte trim (not ALTViewer's fixer). Not only for the gaps: the same patches can add any detail.
What Edward's L111 attempt (`L111LEV-GAP.obj`, six door headers, 18 quads) showed:
- the positions were right with no axis change - 68 of its 72 vertices are existing L111 vertices exactly, so a
  header fill is a few new faces over vertices the level already has, plus a handful of new ones;
- its faces wind the same way as the level faces they meet (every shared edge runs the opposite way);
- four vertices sat at x 5312 where the level's are at 5314 - hand placement is the risk the patch removes, by
  naming the level's own vertices;
- what it did not carry, and a patch must: a texture descriptor (UV rectangle) per face, the face's light, and
  the cell the face is drawn with (so it is culled with its doorway).
Order: after the Level Cleanup's additions and before the trim (which moves every offset), like the other
geometry the cleanup adds (`data/LevelCleanupExtra.json` already inserts a face by its corners - the natural
place to start).

## The MU/TH/UR folder picker (zip 127)

Edward pointed it at an installed copy (C:\Program Files (x86)\Collection Chamber\Alien Trilogy), and at its HDD
and CD folders: it crashed, the SELECT button could not be clicked, and the path drew over the button.
- **Crash:** folder names went through path::string(), which on Windows throws on a name outside the code page, and
  the folder loops used range-for, which throws if a folder stops being readable part way. Both are now safe
  (FsUtil::PathText / PathFromText, iterators stepped with an error code) in the picker, the locator, the
  installer and main.cpp.
- **SELECT:** the layout is in the renderer's pixels but mouse events came in window units, which differ under
  Windows display scaling; events are converted (SDL_ConvertEventToRenderCoordinates).
- **The path box:** its own box left of SELECT, clipped (the end of a long path shows); click it or press Tab to
  type, Ctrl+V to paste, Enter to go there.
- **An existing installation** (its folder, its CD folder or its HDD folder) is now installed from, into
  ALTEngine's own GameData, as a disc is - never patched where it is, which would change the original game's files
  and fails in Program Files anyway. An ALTEngine install is told apart by the CD\LevelCleanup.json every start
  writes. A missing README.TXT no longer fails an install. Its music: only a disc's audio tracks can be ripped, so an
  installed copy without them installs silent (CD\MUSIC\trackNN.wav can be added by hand).
- **Repackaged releases are not supported** (Edward, 2026). Tested on the Collection Chamber release (zip 128): no
  crash, but its copy under HDD\TRILOGY lacks some of the disc's files (SFX/SYNHIT8.RAW and others), so the install
  stops. The message now says so and names what does work: the disc, a mounted image of it, or a full copy of its files.

## The boot on the MU/TH/UR screen (zip 129)

Everything the boot prints - the locator, the install from a disc or an installation, the music rip, the model
removals, the patches, the Level Cleanup and the texture blanking - is traced out on the game window in the MU/TH/UR
style (Bootstrap/MuthurConsole: std::cout teed, so the command window still gets every line, behind - to be hidden or
removed later). Headline lines type out a character at a time; the per-file lines (indented) show dim and at once,
so a full install is not slowed. Any key or click skips the typing. After an install it waits for a key ("PRESS ANY
KEY TO CONTINUE."); an ordinary start holds for under a second (about 2.3 s in all, the six summary lines typed).
Closing the window during it ends the boot.
- **Later: make the readout easier to read** (Edward, 2026). Its lines are the command window's, written for
  debugging and too verbose for players: e.g. "[cleaned] SECT90/L906LEV.MAP: 76 faces (28 out of view, 44 facing
  away, 0 in sealed lifts, 4 hidden) and 87 vertices taken out, first cell's 8 faces restored". Give the MU/TH/UR
  screen its own short wording (one line per step, the counts that matter), and keep the long form for the log.
  Rows wrap after the last comma that fits (zip 130), so a count stays with its word.
