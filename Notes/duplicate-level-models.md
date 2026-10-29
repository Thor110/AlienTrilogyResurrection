# Duplicate and unused door and lift models

Whether the level files' repeated door ("D") and lift ("L") model chunks can be stripped, with the doors' and lifts' model numbers updated to match. Measured on the patched levels from levels.zip.

**Done (zip 39):** every duplicate and unused model below is removed by the `modelRemovals` in `data/Patches.json`, the same removals in ALTViewer's fixer (`BinaryUtility.RemoveFormChunks`), and `Patch/DUPLICATE-MODELS.txt`. See "How it is done" at the end.

## Can it be done?

Yes. The models don't need to be stored once per door or lift:

- **The loader** (`0x27b30`) keeps a pointer to each "D" chunk in a table at `0x1a5ce8` and each "L" chunk in `0x1a8354`, in file order. It looks only at the tag's first letter, so the numbers in the tags don't matter. Limits: 8 door models, 32 lift models.
- **A door** is drawn as `table[its model number]` (`FUN_00028f00`, object +0x1e, from record byte 7), placed by the door object's own position and turn. Opening a door moves the object, not the mesh. **A lift** is drawn the same way (`FUN_00041ba4`).
- **Each door model is set up once per table entry** (`FUN_00029b24`), not once per door, so a model shared by several doors isn't processed twice.
- **The shipped levels already share models:** five of 211's six doors use model 0, and four of 141's lifts use model 4.

So a duplicate can go as long as every door or lift record pointing at it, or at a later model, is renumbered.

## What would go

"Duplicate" means byte-identical to an earlier chunk of the same kind. "Unused" means no door or lift record in the level points at it, or at any copy of it. The unused ones are less certain: if anything other than the records (a script, say) picked a model by number, removing it would break that. Nothing like that has been found, but it hasn't been ruled out either, so they need a play-test.

| Level | Kind | Models now -> after | Duplicates removed | Unused removed | Records renumbered (old -> new) | Bytes saved |
|---|---|---|---|---|---|---|
| 122 | door | 2 -> 1 | D001 | - | 1->0 | 668 (668 dup + 0 unused) |
| 131 | door | 3 -> 1 | D001 | D002 | 1->0 | 1,188 (668 dup + 520 unused) |
| 141 | door | 2 -> 1 | D001 | - | 1->0 | 908 (908 dup + 0 unused) |
| 154 | door | 2 -> 1 | D001 | - | 1->0 |  |
| 154 | lift | 9 -> 7 | - | L001, L008 | 2->1, 3->2, 4->3, 5->4, 6->5, 7->6 | 3,720 (908 dup + 2,812 unused) |
| 155 | door | 2 -> 1 | - | D000 | 1->0 | 908 (0 dup + 908 unused) |
| 162 | door | 2 -> 1 | - | D000 | 1->0 | 908 (0 dup + 908 unused) |
| 211 | door | 4 -> 1 | D001, D002, D003 | - | 1->0 | 2,724 (2,724 dup + 0 unused) |
| 212 | door | 5 -> 4 | D001 | - | 1->0, 2->1, 3->2, 4->3 | 908 (908 dup + 0 unused) |
| 213 | lift | 5 -> 4 | - | L000 | 1->0, 2->1, 3->2, 4->3 | 2,468 (0 dup + 2,468 unused) |
| 222 | door | 2 -> 1 | D001 | - | 1->0 | 908 (908 dup + 0 unused) |
| 231 | door | 2 -> 1 | D001 | - | 1->0 |  |
| 231 | lift | 4 -> 2 | L001, L003 | - | 1->0, 2->1, 3->0 | 5,860 (5,860 dup + 0 unused) |
| 232 | door | 3 -> 1 | D001 | D002 | 1->0 | 1,088 (908 dup + 180 unused) |
| 391 | lift | 1 -> 0 | - | L000 | none | 1,660 (0 dup + 1,660 unused) |
| 901 | door | 2 -> 1 | D001 | - | 1->0 | 668 (668 dup + 0 unused) |
| 902 | door | 2 -> 1 | D001 | - | none | 668 (668 dup + 0 unused) |
| 903 | door | 2 -> 1 | D001 | - | 1->0 | 908 (908 dup + 0 unused) |
| 904 | door | 2 -> 1 | D001 | - | none | 908 (908 dup + 0 unused) |
| 905 | door | 2 -> 1 | D001 | - | none | 908 (908 dup + 0 unused) |
| 906 | door | 2 -> 1 | - | D001 | none | 908 (0 dup + 908 unused) |
| 907 | door | 2 -> 1 | D001 | - | 1->0 |  |
| 907 | lift | 4 -> 1 | L001, L003 | L002 | 1->0, 3->0 | 7,376 (5,860 dup + 1,516 unused) |
| 909 | lift | 8 -> 6 | - | L000, L002 | 1->0, 3->1, 4->2, 5->3, 6->4, 7->5 | 2,056 (0 dup + 2,056 unused) |

Total: 21 levels, **38,316 bytes** (24,380 from duplicates, 13,936 from unused models) out of 18,835,536 bytes across the 45 maps (0.2%). Each chunk also carries an 8-byte header and a pad byte when its size is odd; both are included.

## What it saves compressed

The duplicates sit next to the copy they repeat, a few hundred bytes apart, so any compressor already stores the second copy almost free. On the 21 affected files:

| | Before | After | Saved |
|---|---|---|---|
| Raw | 9,345,164 | 9,306,848 | 38,316 |
| zlib -9 | 2,821,881 | 2,818,293 | 3,588 |
| xz -9 | 1,749,844 | 1,748,740 | 1,104 |

## What doing it would take

- **A new kind of patch.** Every current patch overwrites bytes in place (`BinaryUtility.ReplaceBytes`, `PatchRunner`); none removes bytes or shortens a file. This needs one that drops chunks, rewrites the FORM size at offset 4, and changes the door record byte 7 / lift record byte 12 values listed above. ALTViewer's fixer and the engine's install step would both need it, run after the existing patches.
- **One existing patch moves.** L162's "D001 - incorrect texture index" edit (0x323E8) sits after D000, which would go, so it shifts 908 bytes earlier. L906's patches are in D000, which stays, and nothing is removed in L161. Every other patch is inside MAP0, which comes first and doesn't move.
- **The port** already draws doors and lifts by record number (`GameplayScreen`, `ModelLoader::Load(map, "D")` / `"L0"`), so it needs no change.

## Episode 3

Levels 311-391 and 908/909 have door records but no "D" chunks at all, so there are no door models there to share.

## How it is done

- **A new kind of patch, `modelRemovals`** (`Formats/ModelRemoval.h`). Each entry names the chunks by tag with their size and the CRC-32 of their payload, plus the record bytes to renumber. It is made only if every chunk is there and matches, counts as already done if none is there and the records hold their new values, and otherwise fails without touching the file. The file is written beside the level and moved over it.
- **It runs before the byte patches**, in the engine's install step (`RunPatches`) and at the top of ALTViewer's fixer, so the byte patches are written against the shortened files. L162's D001 texture fix moved from 0x323E8 to 0x3205C; nothing else moves.
- **ALTViewer's fix button** now also shows while L909 hasn't been shortened or the last texture fix isn't in place, so an install patched by an older ALTViewer can take the new patches.
- **Checked** on copies of all 45 levels: everything applies once, and a second run of both the removals and the byte patches changes nothing. MAP0 changes only at the renumbered record bytes. Every door and lift resolves to byte-identical model data. The other chunks are kept, in order. The FORM size is right, and every level loads. ALTViewer's C# (compiled here with Mono's compiler, the closest available) gives byte-identical files to the engine. Tests: `tests/ModelRemovalTests.cpp`.
- **Still to do:** a play-test of the levels that lose unused models (131, 154, 155, 162, 213, 232, 391, 906, 907, 909).
