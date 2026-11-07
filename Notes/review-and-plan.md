# The plan (November 2026)

Where the project stands after zip 142, and what comes next, in order. The detail behind each item is in
`what-remains.md` (its section numbers are given). This replaces the October review, whose list is done or
carried over below.

## 1. Play-testing (Edward)

Done in code from the original but not yet seen or heard in play - the cheapest way to find what is wrong:
- the motion tracker's sound and sweep speed (zip 140);
- Ripley's voice lines, the pickup sounds, "Battery required" and the message panel's timing (zip 141) - the
  speech needs SFX/SPCH.TXT and its VC samples, which are not in the test data;
- the level music (zip 142): 27 levels changed track;
- the co-operative mode's shared world: creatures, doors, pickups, grenades and charges (zips 134-136);
- the "to check" items in what-remains.md, section 1.

## 2. Matching the original: what is still open

- [x] Music per level (zip 142): the table is indexed by the original's level numbers, now traced.
- [x] The message panel's place without the minimap (zip 142): the original's (0x10, 0x3a).
- [ ] The credits screen: the original reads `GFX/CREDITS.TXT` through its file table (0x55dc); its format and
      how it scrolls need the file itself.
- [ ] The volume of the sounds played with EBX set (FUN_00040b2c / FUN_00040ba0 halve a value nothing else
      sets): needs the game running to see what it starts at. The tracker is at half for now.
- [ ] The smaller inferred items (what-remains.md section 2): shell casings falling through the floor, the
      shatter launch speed, the weapon noise value, open question A (pistol rounds against a standing hugger).
- [ ] Another pass over the original's front-end functions (menus, options, credits) - the area the last pass
      showed still hides gaps.

## 3. Level data, in Edward's order (what-remains.md 6, 6b2, 7 and the last two sections)

1. The UV review: 16 campaign levels, the multiplayer copies, the scan's L263, L351 and L371 candidates, the
   door and lift models.
2. The cleanup list finished (Edward's fly-round for strays), shipped as Patches.json entries; the working-out
   code removed.
3. The final texture check over every level.
4. The door-gap fills as an ALTEngine-only patch layer.
5. Last of all, the byte trim, and ALTViewer reading the trimmed layout.

Everything that renumbers faces comes before the trim, which moves every offset.

## 4. Multiplayer (what-remains.md 4b)

- The score table and what ends a deathmatch round ("Minimum game length").
- Doors, lifts and pickups the same on every machine in deathmatch too, with co-op's shared world.
- Bots that use doors, lifts and pickups.

## 5. Code health and performance (what-remains.md 3 and 4)

- [x] `-Wshadow` is on (zip 142), with every warning it gave fixed.
- [ ] Split `GameplayScreen.cpp` (about 6,000 lines, most of it one function): the level load, the tick, the
      draw and the HUD.
- [ ] Tests for the parsers that have none (BND, BX, sprite RLE, the light table).
- [ ] Draw straight to the window rather than copying each frame back through the CPU; one sprite upload
      without closing the render pass.

## 6. Release 1.0

Version, README, the release script and green CI on all three systems once 1 to 3 are done.
