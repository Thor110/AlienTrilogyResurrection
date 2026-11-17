The Patch folder - what is patched, in what order, and why

Every fix to the game's own files is listed here: one file per level or texture sheet (LxxxLEV.txt, 141GFX.txt
and so on), each ending with every patch to that file in bytes (shipped -> patched). Both ALTEngine (at install)
and ALTViewer (its "Patch Game" button) apply them, and for the global stages below they make the same files,
byte for byte. The screenshots are in the repository's README.md (Before And After Screenshots).


THE STAGES

The fixes run as stages, always in this order. Each stage works on the files exactly as the stage before leaves
them, so each one is written against a known file.

  1. Duplicate door and lift models taken out                      global  (ALTEngine and ALTViewer)
  2. Level fixes                                                   global
  3. Texture fixes                                                 global
  4. Missing faces                                                 global
  5. The Level Cleanup, the texture blanking, the file fixes,
     then the padding trim                                         ALTEngine only

Global fixes correct mistakes in the shipped files and suit the original game as well as ALTEngine: ALTViewer's
patch button puts all four into an installed copy of the original. ALTEngine-only fixes are made for ALTEngine
and are never offered for the original game.


1. Duplicate door and lift models (DUPLICATE-MODELS.txt)

   21 levels carry door or lift models that are exact copies of another one, or that nothing uses. They are taken
   out, and the doors and lifts that named a copy name the original. Nothing in the game looks different; the
   level files are smaller. First, because every byte patch after it is written against the files without them.

2. Level fixes ("stage": "level" in ALTEngine/data/Patches.json)

   Mistakes in a level's shape or in what it does, not in how it is painted:
   - L906: a misplaced triangle (ALTViewer's fix 1);
   - L901: the player store placed inside the level;
   - L371: the inaccessible secret, its lift's threshold set so it can be taken down again (found by bambamalicious);
   - L111 and L900: a vertex of the first lift with the wrong Z coordinate (found by Edward);
   - L112: a corner under the hanging sign covered by two sets of faces;
   - L909: the multiplayer holding rooms kept sealed - the start room as shipped (setting back an earlier test
     patch that opened it), and its mirror at the corridor's east end, which could be walked into through its
     painted lift door;
   - L903: four cells that lost their wall attribute when the level was made from L141 (the walk-through pillars);
   - L161: face 3758 put back as shipped (the hole it was standing in for is now one of the missing faces).

3. Texture fixes ("stage": "texture")

   How the level is painted: a face's texture number, its turn or flip, its draw routine (see-through, mirrored),
   and the texture sheets themselves (141GFX, 371GFX, 903GFX). Most come from three places:
   - the original fixes from ALTViewer (L906's railings and bridge, L905's flipped textures, crates on L111, L141,
     L900 and L903, L161's and L162's doors, L371's two faces and its texture's stray tiles);
   - the texture scan (ALTLevelScan), which looks for faces whose texture does not run on from the faces around
     them; each was rendered before and after and kept only after review (the "texture scan 909-14" style
     numbers in the README);
   - the UV review: Edward going through each level with the Face Index Display (Notes/uv-review-log.md).
   No texture fix touches a byte a level fix does, so stages 2 and 3 never undo each other.

4. Missing faces (ALTEngine/data/MissingFaces.json, ALTViewer/MissingFaces.cs)

   In every level, the first open cell of the map (always in its top row) points at the blank record before the
   level's faces instead of at its own faces, which come first in the file. So the original never draws them. It
   looks like an off-by-one in the tool that made the levels. On most levels that cell is out of sight, but on some
   it leaves a hole: the floors on L212, L242 and L243, a wall on L161, the boiler room wall by the grate on L232.
   On 36 levels the cell is given its faces back. Where the first of them (record 0) is seen, a copy of it is put
   in at the head of the cell's faces; L232's copy gets texture 193, flipped, as Edward picked. And L211's floor,
   which was never in the file at all, is put in by its corners.

   Putting a face in moves every later face up one: on 31 levels the face numbers from the first cell's faces on
   are one higher than in the notes written before (Notes/uv-review-log.md says which). That is why this stage
   comes after every byte patch: the byte patches name offsets in the file without the new faces.

   Each level is checked before (the file must be exactly the one the byte patches leave) and after (the result
   must be exactly the expected file). Pressing ALTViewer's button again, or installing ALTEngine from a game
   ALTViewer patched, first takes the faces back out, exactly, then checks the byte patches, then puts them back.

5. ALTEngine only: the Level Cleanup, the texture blanking and ALTEngine's file fixes

   The Level Cleanup (Options > Testing > Level Cleanup, on by default) takes out the faces no one can ever see
   (behind walls, outside the map, under floors) and the cells never drawn: ALTEngine/data/LevelCleanupList.json,
   made by ALTLevelCleanupList, with Edward's hand-picked additions in LevelCleanupExtra.json. Turned off, the
   files go back to how stages 1-4 left them. The texture blanking (data/TextureBlankList.json) clears the texture
   slots no level uses. Both keep a copy of the file before them (.untrimmed, .unblanked).

   ALTEngine's file fixes (data/EngineFilePatches.json) change files for ALTEngine alone, each checked by its
   CRC before and after. The first is the face hugger's art, FINGERS.B16, filled to the bottom of the screen
   (FINGERS.txt).

   The padding trim, truly last (Options > Testing > Padding Trim, on by default): each level file loses the
   bytes the game never reads, or reads as one fixed value, or clears when the level starts - 1,930,078 bytes
   across the 45 levels, about 10% of them (Notes/unread-bytes-notes.txt says what every byte is). The level's
   MAP0 chunk becomes MAPT, with shorter records; every other chunk stays as it was:

     record        bytes   taken out
     header        36->32  +28,+29 (always 0x40 0x40); +34,+35 (vent types, never read)
     vertex         8->6   +6,+7 (always 0)
     cell          16->13  +4, +7 (always 0, rebuilt at level start); +12 (cleared at level start)
     path node      8->7   +2 (always 0)
     monster       20->19  +9 (never read)
     pickup         8->6   +5 (always 0); +7 (never read)
     object        16->12  +5 (10 on the crate types, else 0); +11, +13, +15 (always 0)
     door           8->7   +5 (always 0)
     lift          16->10  +4, +11 (always 0); +6 (always 1); +13..15 (never read)
     action group   4->3   +3 (always 0)
     light         28->24  the pad byte after each colour (always 0)

   It comes last because every offset in this folder, and every offset the stages before it use, is in the
   untrimmed layout. The untrimmed level is kept beside each (.padded); turning the option off puts it back at
   the next start. ALTEngine reads either; ALTViewer reads either too (LevelTrim.cs), and refuses to patch a
   trimmed install, whose patches are all in already. Planned for this stage: the gaps above the doors
   (Notes/what-remains.md, section 7b).


THE OTHER FILES HERE

  FINGERS.txt                                    the face hugger's art (ALTEngine only, stage 5)
  UNPATCHED-NOTES.txt, UNPATCHED-FIX-LIFTS.txt   problems found and not patched (yet), with why
  another-fix.txt, new-fix.txt                   ALTViewer's earliest fixes, as first written up
  LxxxLEV-Dnnn.txt, L906LEV-D000.txt             a level's door model looked at on its own
