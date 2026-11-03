# The level patching routine, in order

Everything ALTEngine does to the game's level files and level texture sheets, step by step, with the tool or
data file behind each step and how to run it forwards (remake it) or backwards (undo it). Each step reads the
files exactly as the step before leaves them, so the whole routine can be replayed or unwound one step at a time.

| # | Step | Code | Data | Undo |
|---|---|---|---|---|
| 1 | Copy from the disc | Formats/Installer | `data/DiscFileManifest.json` | copy again |
| 2 | Put the cleaned files back | Bootstrap/InstallPipeline (`SetCleanedAside`, `SetBlankedAside`) | `<file>.untrimmed`, `<sheet>.unblanked` | - |
| 3 | Duplicate and unused door/lift models out | Formats/ModelRemoval | `data/Patches.json` `modelRemovals` | copy again |
| 4 | The fixes | Formats/PatchRunner | `data/Patches.json` `patches`, notes in `Patch/LxxxLEV.txt` | copy again, or an entry that writes the old bytes back |
| 5 | Level Cleanup: faces out, first cell back, faces added | Game/LevelCleanup (`Apply`) | `data/LevelCleanupList.json` | Testing > Level Cleanup off |
| 6 | Texture blanking | Game/TextureBlank (`Apply`) | `data/TextureBlankList.json` | Testing > Level Cleanup off |

Steps 2 to 6 run at every start (`RunPatches`). Steps 5 and 6 follow the Testing > Level Cleanup option (off by
default); with it off the files stay as step 4 leaves them.

## The steps

1. **Copy from the disc.** The install copies the files `data/DiscFileManifest.json` lists from the game's CD into
   `CD/`. Only these copies are ever changed; the disc never is. L361 (an unused, unfinished copy of another level)
   and its sheet are not copied.
2. **Put the cleaned files back.** A level the Level Cleanup changed has its fixed-but-uncleaned copy beside it as
   `<file>.untrimmed`, and a blanked sheet its unblanked copy as `<sheet>.unblanked`. These go back first, so the
   fixes always apply to the files they were written against.
3. **Model removals** (`modelRemovals`): door and lift models that are exact duplicates of another, or that no door
   or lift record uses, are cut out of the level file, and the records pointing past them are renumbered
   (`Notes/duplicate-level-models.md`, `Patch/DUPLICATE-MODELS.txt`). First, because every byte offset in step 4 is
   written against the files without them. An entry already in place is skipped.
4. **The fixes** (`patches`): byte edits at file offsets, in the order listed - textures, UVs, faces, cells,
   triggers, lifts, and since zip 119 texture sheets (141GFX and 903GFX's two new floor slices). Each one's reason is
   its `note` and the level's `Patch/LxxxLEV.txt`. An entry whose bytes are already in place is skipped, so the step
   can run any number of times. A fix taken back is written as an entry that sets the shipped bytes back (L909's
   start room, L161's face 3758), which also mends an install the old fix changed.
5. **Level Cleanup** (`data/LevelCleanupList.json`, worked out by `ALTLevelCleanupList`): per level, against the
   CRC32 of the file step 4 leaves (a level with another CRC is left alone and reported `[skipped]`):
   1. the faces no player can see go - out of every view walk, facing away from everywhere the eye can be, always
      behind other faces, the sealed lifts of L321-L325, and those named by hand;
   2. the first cell's run is given back from record 0 (its first face, which the shipped files hide), as a copy
      at the head of the run where record 0 is ever seen;
   3. then the missing faces are added (`add`), by corners, at the head of their cell's run.
   Every face and vertex number, run start, header count and the MAP0 and FORM sizes follow.
6. **Texture blanking** (`data/TextureBlankList.json`, worked out by `ALTTextureBlankList`): per sheet, against the
   CRC32 of the sheet step 4 leaves, every texel no texture the level draws covers takes its page's background -
   the most common colour of the space blanked, never placeholder magenta (`Notes/texture-use.md`). Nothing drawn
   changes.

## Working forwards: remaking the lists after a change

A change to a fix (step 4) changes the files the lists were made from, and the install then reports those levels
or sheets `[skipped]`. To remake:

1. Edit `data/Patches.json` (and the level's `Patch/LxxxLEV.txt`, and ALTViewer's fixer).
2. Start ALTEngine with Testing > Level Cleanup **off**: `CD/` then holds every file as step 4 leaves it.
3. `ALTLevelCleanupList <CD folder>` (from `ALTEngine/`): writes `data/LevelCleanupList.json` and
   `Notes/level-cleanup-lift-remainder.md`. It reads the hand list, `data/LevelCleanupExtra.json`.
4. `ALTTextureBlankList <CD folder>` (from `ALTEngine/`, after step 3, whose added faces it counts): writes
   `data/TextureBlankList.json` and `Notes/texture-use.md`.
5. Turn Testing > Level Cleanup on and start again: the install applies both lists.

## Working backwards

- Steps 5 and 6: turn Testing > Level Cleanup off; the next start puts every `.untrimmed` and `.unblanked` file
  back and removes the cleaned copies.
- One level's cleanup or one sheet's blanking: take its entry out of the list file (or make it name nothing).
- A fix: copy the shipped file from the disc again (delete it from `CD/` and reinstall it), or add an entry that
  writes the shipped bytes back. Every entry's offsets and bytes are in `data/Patches.json`, and the level's
  `Patch/LxxxLEV.txt` gives the bytes it replaced.
- The removed duplicate models: copy the shipped file again.

## Picking faces by hand

With Testing > Face Index Display on, the face under the crosshair shows its number, and each Fire 1 press appends
it to `FaceLog.txt` beside the executable: the level, the face's vertices as the display shows them, then its
number, texture, routine and corners. The corners place a face whether the level was cleaned or not. Faces to take
out go into `data/LevelCleanupExtra.json` (`faces`, by face number with the cleanup off, or `points`, by corners);
`rooms` names a door tile whose sealed room goes whole; `recordZero: false` keeps a level's record 0 out; `add`
lists faces to put in, by corners, texture, routine, light and cell.

## Checks

- `ALTEngineTests` (`ctest`): the cleanup on the demo disc's L111 and L112 (nothing drawn changes, the view walk
  takes in the same cells, plans refused when they do not fit, faces added at the head of their run) and the
  texture scan and blanking on L112's sheet (no used texel changes, blanking twice changes nothing).
- `ALTTextureBlankList` applies every plan to a copy of its sheet and refuses one that would change a texel any used
  texture covers.
- `ALTLevelCleanupList` applies every plan to a copy of its level as the install will.
