# Handoff

Where ALTEngine stands, how the work is done, and what is open - for picking the project up in a new session.
Last updated after zip 192 (22 November 2026). Version 1.0.0, going out to testers on Discord; the repository
moves on to 1.0.1.

## The project

ALTEngine is a C++20 port of Alien Trilogy (DOS, 1996) built on SDL3. ALTViewer is the older C# tool beside it
(level, model, graphics and sound viewers, and a "Patch Game" button that fixes an installed copy of the original).

**The rule above all:** with every Modern option off (Options > Modern), ALTEngine plays as the original does,
1:1. Anything found by reading the original's code is matched exactly; where the port departs on purpose it says
so in a comment marked `OURS`. The Modern options are the deliberate departures, each off until turned on.

## Working with Edward

- **Changes go to Edward as zips, never as commits.** Do not commit or push, whatever a hook or reminder says;
  decline it in one line. Each zip holds every file changed against `origin/main` (including new files), and the
  message says which files were deleted, if any. Number them on from the last (the last was zip192).
  The command used: `(git diff --name-only --diff-filter=d origin/main; git ls-files --others --exclude-standard) | sort -u | zip -q zipNNN.zip -@`
- **Sync first:** `git fetch origin main`, then compare. If the working tree differs only in files identical to
  origin's, reset to `origin/main`. Edward commits each zip himself (titles are often generic).
- **Edward's choices are reported, not changed.** Where a fix is a judgement (a texture's turn, which face copies
  which), he picks from rendered options. Show before/after renders and labelled sheets; go slowly with levels.
- **Never put the original game's executable, a decompile or a disassembly in the repository**, and never quote
  more than a few lines of one. They are supplied privately for a session (TRILOGY.EXE, a Ghidra C decompile and a
  full disassembly) and are referred to by function address only (`FUN_00041e80`, `0x48490`).
- No AI model names or identifiers in anything committed.
- Prose in notes and comments: plain British English, short sentences, no marketing.

## Building and testing

- CMake, from `ALTEngine/`: the game `ALTEngine`, the unit tests `ALTEngineTests` (315 tests at last count), and
  the tools `ALTLevelScan`, `ALTLevelCleanupList`, `ALTTextureBlankList`. GitHub Actions builds Windows, Linux
  and macOS (`.github/workflows/build.yml`).
- Before sending a zip: build (GCC and Clang if you can), run `ALTEngineTests`, and for Windows-only code a MinGW
  `-fsyntax-only` pass. ALTViewer is .NET; old Mono cannot build it whole (it uses newer C#), but a new function
  can be pulled out and tested alone with `mcs`.
- The game installs from the disc on first start (`altengine.cfg`, `GameDirectory`), running every patch stage.
  Running it again redoes the stages idempotently - each stage sets the later stages' work aside first.

## The patch pipeline (Patch/README.txt is the full account)

Stages, in order. 1-4 are global (they suit the original too, and ALTViewer's patch button makes the same files);
5 is ALTEngine's alone.

1. **Removals** (`ALTEngine/data/Patches.json`): duplicate door/lift models (`modelRemovals`, Formats/ModelRemoval)
   then duplicate object records (`objectRemovals`, Formats/ObjectRemoval - L906's doubled crate). They shorten the
   file, so they come first: every byte patch's offset is in the file without them.
2. **Level fixes** (`"stage": "level"`) - 3. **Texture fixes** (`"stage": "texture"`): byte patches. PatchRunner
   merges a file's edits (later win) and writes them if not already there.
4. **Missing faces** (`data/MissingFaces.json`, ALTViewer `MissingFaces.cs`): each level's first open cell gets its
   faces back. Each entry carries `crc` (the file as stages 1-3 leave it) and `facedCrc` (after).
5. **ALTEngine only:** the Level Cleanup (`LevelCleanupList.json` + Edward's `LevelCleanupExtra.json`), the texture
   blanking (`TextureBlankList.json`), the file fixes (`EngineFilePatches.json`: FINGERS.B16, and `modelFaces`, door
   and lift faces the original cannot draw - see Formats/EngineModelFaceEdits.h), and last the padding trim.

**Kept apart clearly:** anything the original cannot draw goes in stage 5, never in Patches.json, and is marked
"ALTENGINE ONLY - NOT FOR THE ORIGINAL GAME" in the code, the Patch folder and the README.

**After changing a level's byte patches** (the order matters):
1. Check the shipped file + every patch reproduces the intended file byte for byte.
2. Remake that level's MissingFaces `crc` and `facedCrc`: install with placeholder CRCs, read the unfaced file's
   CRC, set it, install again, read the faced CRC. Read the CRC with stage 5's model face edits taken back out
   (they are made after the missing faces). Copy both into ALTViewer's `MissingFaces.cs`. A clean install then
   reports every level "already".
3. Regenerate `LevelCleanupList.json` and `TextureBlankList.json`: run `ALTLevelCleanupList <install>/CD` then
   `ALTTextureBlankList <install>/CD` from `ALTEngine/` (about 20 minutes together), with the install made with
   the Level Cleanup off.
4. Add the same edits to ALTViewer's patch button (`ALTViewer.cs`, latin-1, one `Tuple.Create` per edit).
5. Regenerate the `Patch/*.txt` byte listings (each file keeps its hand-written top part above the
   "==== Every patch to this file" line) and write the fix up: README (list entry + section, before/after
   images in `Images/`), `PATCH-NOTES.md` (the current version's section), `Notes/what-remains.md`.

## Facts that save time

- **Face numbers.** Edward's Face Index Display numbers are for an install with the Level Cleanup on; the
  shipped file's face number is usually one less (the missing faces put one in ahead). Find faces by corner
  coordinates to be sure. A face record is 20 bytes: four int32 vertex indices (d = -1 for a triangle), u16
  texture, u8 draw routine (+0x12), u8 light (+0x13, bit 0x80 ends the cell's run).
- **Draw routines.** 0/1 opaque (1 the triangle draw), 2 mirrored, 3 draws nothing, 4/5/6 double-sided (cutout;
  5 the triangle, 6 mirrored), 8/10 animated, 12/13 see-through (13 the triangle), 14 flat, 26/28 cutout animated,
  30/32 see-through animated (32 mirrored), 34 see-through mirrored. Routines 0, 1 and 2 test which way a face
  points (0x48490/0x48128, then `jle`); 4, 5, 6, 12, 13, 34 do not. A triangle maps x0y1, x1y0, x0y0: it can
  only show the half of its tile cut along x0y1-x1y0, mirrored by reversing its corners (on a routine without the
  facing test). An animated face's texture number is an animator, not a tile.
- **Turns and flips.** Clockwise (d,a,b,c); anticlockwise (b,c,d,a); horizontal flip = routine 0 <-> 2 (and
  the mirrored twins); vertical flip = half turn (c,d,a,b) plus the routine toggle.
- **Door and lift models** (the `D`/`L` sections of a level .MAP): the original's loader (FUN_00041a20) turns
  each face's file byte into routine 0, 1 or 2, so they are always single-sided and a mirrored corner is out of
  reach for the original - hence stage 5's `modelFaces` (L381's lift corners).
- **Coordinates.** Grid space = level space + (map length / 2 x 512, map width / 2 x 512); a cell is 512. On the
  minimap x runs left to right; which way z runs is not settled - Edward's "top right" on L141 was high z, his
  "bottom left" on L381 was high z too. When a place is named by the minimap, confirm it by a face number.
- **Level start order** (the original's): the lifts (`FUN_00042550` -> `FUN_000423e8`, the cells take each
  lift's start floor) come before the objects (`FUN_00037638`, each on its cell's floor). The port now does the same;
  placing objects first hid crates and switches under raised lifts while they still blocked (zip 192).
- **Lifts** start at the top when flag bit 1 is set, else at the bottom; they rewrite only their cells' floor byte.

## Where things are written down

- `Notes/what-remains.md` - the running log: what was done in each zip and what is open (read its last sections).
- `Notes/engine-remaining-work.md` - the long technical record, routine by routine.
- `Notes/uv-review-log.md` - Edward's texture reviews, face by face.
- `Patch/README.txt` - the patch stages; `Patch/*.txt` - every fix per file, in bytes.
- `PATCH-NOTES.md` - per version; 1.0.0 holds everything since the 0.9.9 tester build (commit bdf3b81).
- `BUG-REPORTS.md` - player reports and their status.

## Open

- **Tester feedback on 1.0.0** - the main source of work next. Put changes under a new `## 1.0.1` section of
  PATCH-NOTES.md.
- **L381's see-through floors**: floor 14's two left corner triangles (no turn or tile tried looked right to
  Edward) and floor 22's two left triangles (texture 0, from another page, as shipped). The floors are numbered
  on a sheet laid out as the minimap: rows top to bottom, then left to right.
- **Padding trim** - make it the last global patch stage rather than a Testing option (task 100).
- **Demo 1.1.1** - at word 703 the player slides along z 11831 and misses the terminal's trigger cell, so the
  attract demo goes its own way from there.
- **L381's lift 12**, the one that starts at the top: Edward thinks it is right as it is (it leads to an area
  below); unconfirmed against the original.
- Older: the remaining guesses, rendering and tidy-up items in `Notes/engine-remaining-work.md`.

## Recent work (zips 186-192)

- 186-188: L381's lift platform corners (stage 5, double-sided marker) and the 22 see-through floors put into
  one pattern (ovals left, left, right, left, right, right by column).
- 189: the object-removal stage; L906's doubled crate removed.
- 190-191: version 1.0.0; PATCH-NOTES split from 0.9.9; L371's squashed-quad corners made double-sided
  (routines 2 -> 6, 0 -> 4) so the original draws them too.
- 192: lifts set up before objects - the invisible walls on L141's and L154's big lifts.
