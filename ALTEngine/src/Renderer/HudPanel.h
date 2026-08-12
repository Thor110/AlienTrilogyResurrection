#pragma once

// HUD PANEL GRAPHICS - decoded, not yet implemented.
//
// WHAT THE FILES ARE. PNL0GFXU.16 and PNL1GFXU.16 are ordinary BND texture
// sets, readable with BndTextureLoader as-is: INFO / TP00 / CL00 / BX00, ONE
// 256x256 page, 343 descriptors each. Both files have the same descriptor
// layout - the same rects in the same order - and differ only in the artwork
// underneath and by one pixel in each rect's stored width and height (PNL0's
// read back 16x16 where PNL1's read back 15x15, on identical positions).
//
// Dumped and inspected, the 343 descriptors cover:
//   - the HUD frame pieces: bars, grilles, indicator lamps (red/amber/green),
//     a power icon, hazard striping
//   - TWO fonts, a large and a small set, each with digits, upper and lower
//     case and punctuation
//   - pre-rendered word labels baked into the sheet: "Charge", "Armor",
//     "Hyper", "Grenade", "Boots"
//   - the motion tracker: concentric arc segments, assembled into the ring
//   - solid fill blocks, including a red one - the health/ammo bar fills
//   - one large non-HUD texture block
//
// PNL0 is industrial/human; PNL1 is organic/alien, and its non-HUD block is
// the egg husk texture (confirmed by Edward). So these sheets carry a little
// world art as well as the HUD, which is why the egg husk appears to come from
// a panel file.
//
// HOW ONE IS CHOSEN - and there are THREE, not two.
// Ghidra, in the level-entry path around 0x0001b0xx:
//
//     if      (levelId < 0x0c) { resource = 0x90; panelIndex = 0; }
//     else if (levelId < 0x16) { resource = 0x91; panelIndex = 1; }
//     else                     { resource = 0x92; panelIndex = 2; }
//     FUN_00013c04(resource, 7);      // load it
//     FUN_0004f6d4(panelIndex);       // install it
//
// The thresholds land exactly on the chapter boundaries of the level order in
// LevelManifest.json: ids 0-11 are chapter 1's twelve levels, 12-21 are chapter
// 2's ten, 22+ are chapter 3's fourteen. So it is one panel set per chapter -
// which independently corroborates that manifest order IS the internal level id
// order, the same assumption the music track table rests on.
//
// WHICH DESCRIPTOR SET THE PANEL SHEET ACTUALLY IS - now established, and it is
// NOT resource 0x90/0x91/0x92.
//
// FUN_00039068 builds the font's advance-width table by walking 0x5b (91) glyph
// descriptors starting at DAT_00240a30, taking each width as rect u1 - u0. That
// address is DAT_002408c0 + 0x170, i.e. descriptor index 23 of the set based at
// DAT_002408c0 - and descriptor 23 is exactly where the 9-pixel-tall font rects
// begin in the dumped sheets, with 0..22 being the frame pieces. So:
//
//     the panel sheet IS the descriptor set at DAT_002408c0,
//     loaded from resource 0x7f / 0x83 / 0x84 / 0x85,
//     chosen by a switch on DAT_000ae10c - which is the LANGUAGE.
//
// That finally makes sense of everything: the sheet has baked-in word labels
// ("Charge", "Armor", "Hyper", "Grenade", "Boots") and two fonts, so of course it
// is language-selected. The "U" in PNL0GFXU / PNL1GFXU is very likely the
// language code, and the leading digit is the chapter variant.
//
// TWO EARLIER CLAIMS OF MINE WERE WRONG, and both are worth stating plainly:
//   1. DAT_002408c0 is NOT episode-dependent enemy graphics. It is the
//      language-selected panel sheet. The crate-scale argument that rested on
//      that reading is void - see the note at the object placement site in
//      GameplayScreen.cpp.
//   2. Resources 0x90/0x91/0x92, picked by level-id thresholds into descriptor
//      slot 7, are therefore NOT the panel sheets. What they are is unknown.
//      Their thresholds still land on chapter boundaries, so they are plausibly
//      per-chapter world art - which fits Edward's disc having no third panel
//      file at all.
//
// Chapter-to-sheet selection is Edward's, from what is actually on the disc:
// chapters 1 and 2 share PNL0GFXU, chapter 3 uses PNL1GFXU, corroborated by
// chapter 2 having no unique switch models and reusing chapter 1's.
//
// ============================================================================
// THE TWO FONTS - both decoded and confirmed against the sheets.
//
// The sheet has a small font and a large one, and the HUD uses one for each row.
// Their glyph tables sit at different bases in the same descriptor set:
//
//   FONT A - the ammo row, FUN_000393a0, glyph base DAT_00240a30
//     = DAT_002408c0 + 0x170  ->  descriptor 23
//     91 glyphs (0x5b), descriptors 23..113, 9 pixels tall
//     digits '0'-'9' at glyph 0x10..0x19  ->  descriptors 39..48
//
//   FONT B - the health row, FUN_000395d0, glyph base DAT_00240fe0
//     = DAT_002408c0 + 0x720  ->  descriptor 114, immediately after font A
//     digits '0'-'9' at glyph 0x10..0x19  ->  descriptors 130..139
//
// CONFIRMED, not inferred: every one of font A's ten digit rects is exactly 9x9
// and every one of font B's is exactly 12x10. Ten uniform rects in a row at each
// computed offset is not a coincidence, and it lands font A's glyph 0 exactly
// where the 9-tall rects begin in the dumped sheet.
//
// Both routines take the value in BX clamped to 0..999, emit one quad per digit
// most significant first via repeated /100 and %100, and advance x by each
// glyph's own width. Advance widths come from a table built by FUN_00039068 as
// each rect's u1 - u0.
//
// ============================================================================
// PLAYER STATE BLOCK - FOUND. All of this is read out of FUN_0003e4a8 (damage)
// and FUN_0003aac8 (the HUD's own ammo display), so it is decompiled rather than
// inferred.
//
//   0x000b0ab8   int16  HEALTH.   Damage subtracts from it, clamped at 0. On
//                               reaching <1 the code sets DAT_000b0cc0 |= 0x20
//                               (death flag) and calls FUN_000350e4.
//                               Appears as DAT_000b0ab6._2_2_ in the dump.
//   0x000b0aba   int16  ARMOUR.   Takes damage FIRST - health is only touched
//                               when armour is 0. Clamped at 0.
//   0x000b0b30   int16  damage cooldown, set to 0x1e (30) ticks after a hit, or
//                               8 when DAT_000ae0a4 is set. While non-zero the
//                               whole damage function is skipped, so this is the
//                               invulnerability window.
//   0x000b0cc0   flags, bit 0x20 = dead.
//   0x000b0aae   int16  CURRENT WEAPON, 0-4 (DAT_000b0aac._2_2_).
//
// AMMO, and note it is NOT one counter per weapon - two of them are stored as a
// unit count plus a remainder, and the HUD adds them up (FUN_0003aac8's switch):
//
//   weapon 0:  0x000b0ac0 * 15  + 0x000b0abe      (clips of 15, plus loose)
//   weapon 1:  0x000b0ac2
//   weapon 2:  0x000b0ac6 * 100 + 0x000b0ac4
//   weapon 3:  0x000b0ace * 100 + 0x000b0acc
//   weapon 4:  0x000b0aca
//
// Pickups add to the unit counters (FUN at 27918-27949 and its twin at
// 28024-28058); firing decrements the remainder.
//
// ============================================================================
// HUD LAYOUT - the ammo row, transcribed from FUN_0003aac8.
//
// The coordinate space is 320x240, not 320x200: the ammo row sits at y 0xd9-0xe0
// (217-224), which only fits a 240-tall screen.
//
//   ammo number   text drawn at (0x12, 0xd9) = (18, 217)
//   bar fill      x 0x30 (48) to 0x30 + min(ammo, 0x16) * 4, y 217..224
//                 so 22 segments maximum, 4 pixels each, 8 pixels tall
//   bar frame     x 0x30..0x88 (48..136), y 217..224
//
// DISPLAY LIST ENTRY LAYOUT - now established. Entries are pushed at
// DAT_0023e2e0 and come in TWO sizes, one per primitive:
//
//   FLAT QUAD, LAB_000498dc, 0x1c bytes:
//     +0x04  primitive routine pointer
//     +0x08..0x0a  three bytes, see the caution below
//     +0x0b  1
//     +0x0c..0x1b  four corners, x and y as shorts (4 x 4 bytes = 0x10)
//
//   TEXTURED QUAD, LAB_0004b5cc, 0x2c bytes:
//     +0x04  primitive routine pointer
//     +0x08..0x0a  three bytes
//     +0x0b / +0x22  1
//     then four (corner, UV) groups of 8 bytes:
//       +0x0c x0, +0x0e y0, +0x10 UV dword
//       +0x14 x1, +0x16 y1, +0x18 UV dword
//       +0x1c x2, +0x1e y2, +0x20 UV dword
//       +0x24 x3, +0x26 y3, +0x28 UV dword
//
// The sizes are confirmed by the advance after each push: the flat health bar
// does DAT_0023e2e0 = iVar3 + 0x1c, the textured weapon icon does + 0x2c, and
// the multi-quad builders around line 13337 place successive entries at +4,
// +0x30, +0x5c, +0x88 - a 0x2c stride.
//
// THE +0x08 FIELD - RESOLVED, and it means different things per primitive.
// Both routines are now lifted (thanks to Edward re-exporting with them forced
// into functions), so this is read rather than guessed:
//
//   FUN_000498dc (flat quad) reads ONE byte from +0x08 and nothing from +0x09
//   or +0x0a - those are part of the coordinate block. That byte is a CLUT
//   INDEX, not a colour. Checking it against the sheet's own 258-entry CL00
//   palette settles it:
//     index 5    -> rgb(176, 216, 176)  pale green   - the bar FILL
//     index 0x5f -> rgb( 24,  24,  16)  near black   - the bar FRAME
//   which is exactly a green fill in a dark trough, and matches the screenshot.
//   So my "5 and 0x5f are descriptor indices" was wrong, and so was the
//   correction that called them RGB. They are palette indices.
//
//   FUN_0004b5cc (textured quad) reads THREE bytes from +0x08/+0x09/+0x0a, each
//   shifted right by 2, into three consecutive colour registers. So for textured
//   entries it really is RGB, at 6 bits per channel. Note the bytes land in
//   DESCENDING register order (+0x08 -> 0x4002cb, +0x09 -> 0x4002ca,
//   +0x0a -> 0x4002c9), so the channel order may be reversed relative to the
//   entry - worth checking on screen before trusting it.
//
// +0x0b IS A SCALE FLAG, not a constant 1. When zero, FUN_000498dc rescales the
// coordinates from the 320x240 virtual space to the real output size:
//     x = x * outputWidth  / 0x140   (320)
//     y = y * outputHeight / 0xf0    (240)
// When one, the coordinates are used raw. Both HUD bars set it to 1. This is
// independent confirmation that the HUD's virtual space is 320x240.
//
// AND IT SETTLES THE LIGHTING QUESTION. The textured primitive takes an 8-bit
// channel and shifts it right by 2 into a 6-bit register, so 0x80 = 128 maps to
// 32 - the middle of 0..63. 128 is the neutral value for this engine's vertex
// colour, not 255. That is direct evidence that LIGHT_COLOUR_NEUTRAL in
// LightTable.h should be 128.0f rather than the 255.0f it currently holds.
// STILL NOT CHANGED: the level lighting looks right to Edward as shipped, and
// changing it halves every surface's brightness. Worth trying deliberately as a
// one-line experiment rather than folded in silently.
//
// Layout, as far as it is read:
//   +0x04        pointer to the primitive's draw routine. The ammo bar uses
//                LAB_000498dc, the glyph path uses LAB_0004b5cc - so flat quads
//                and textured quads are different primitive types.
//   +0x08..0x0a  RGB. The glyph path writes 0x80,0x80,0x80.
//   +0x0c..      corner coordinates, two shorts each.
//
// CORRECTION: I previously called the ammo row's 5 and 0x5f "descriptor
// indices". They are not - they are the RED component at +0x08, with green and
// blue left at 0. The bar is a flat coloured quad, not a textured one. What
// those two dark reds are for is not yet explained.
//
// WORTH NOTING FOR LIGHTING: the glyph path's neutral colour is 0x80 = 128, not
// 255. That is real evidence that this engine's vertex-colour modulate treats
// 128 as "leave it alone", which bears directly on LIGHT_COLOUR_NEUTRAL in
// LightTable.h - currently 255 with a documented caveat. Not changed here: the
// level lighting looks right to Edward as it stands, and one HUD constant is not
// enough to overturn something that has been eyeballed against the real game.
//
// ============================================================================
// HUD LAYOUT - the health row, transcribed from the draw around 29750-29840.
//
//   health number   FUN_000395d0(0xdd, y, buffer) - x = 221. Note this is a
//                   DIFFERENT text routine from the ammo row's FUN_000393a0.
//   bar frame       x 0xe1..0x12f (225..303), y 0x16..0x30 (22..48)
//   bar fill        x 0xe1 (225) to 0xe1 + width, same rows, where
//                     width = health < 100 ? (health * 0x50) / 100 : 0x4e
//                   i.e. health x 80 / 100, clamped to 78.
//
// The frame is 303 - 225 = 78 wide, exactly the fill's clamp, so the bar fills
// its frame at 100 health. But the scale factor is 80, not 78, and the < 100
// branch is not clamped - so the original OVERSHOOTS its own frame near the top
// of the range:
//
//     health  97 -> 77 px   (1 short)
//     health  99 -> 79 px   (1 PAST the 78-wide frame)
//     health 100 -> 78 px   (clamped, back inside)
//
// One pixel of the fill pokes out of the right of the frame at 99 health, and
// only at 99. That is the original's arithmetic, so it is reproduced exactly
// rather than clamped tidy - if it ever needs hiding, clamp at the draw site so
// this stays a faithful transcription of the formula.
//
// ALTERNATE HUD LAYOUT. When DAT_000ae0a4 and DAT_000ae0a5 are both set, the
// health row moves from y 22..48 to y 0xc6..0xe0 (198..224) - top of screen
// versus bottom. So the original has two HUD arrangements; what selects them is
// not yet known. DAT_000ae0a4 also shortens the damage cooldown from 30 ticks to
// 8 in FUN_0003e4a8, so it is some sort of mode rather than a display option.
//
// COLOUR BANDS. The health bar computes a colour by band - health < 0x1f (31)
// flashes, gated on bit 3 of DAT_000b0bb8 (a running tick counter) and only when
// the damage cooldown is 0; health < 0x4c (76) is a middle band; above that a
// third. The exact colours are NOT readable: the decompilation then writes
// +0x08 = 5, +0x09 = 0, +0x0a = 0 unconditionally, discarding everything the
// band logic just computed. The ammo bar writes the same 5. Two dark-red bars
// and a discarded computation is not a believable reading, so treat the
// +0x08..0x0a field as UNIDENTIFIED - it is probably not plain RGB, and Ghidra
// may be mis-ordering aliased writes here (it warns about overlapping globals in
// this file). The band THRESHOLDS are solid; the colours are not.
//
// ============================================================================
// MOTION TRACKER - FOUND. Two functions, and it is always drawn: it is part of
// the HUD, not an inventory item (the Auto Mapper only reveals the level map).
//
//   FUN_0003a1d0   the dish, and the ping. Runs only when DAT_000ae0a4 and
//                  DAT_000ae0a5 are both zero, i.e. the normal HUD layout.
//   FUN_0003a008   the blips. Called by the above, returns the contact count.
//
// THE BLIPS (FUN_0003a008):
//   - walks the entity list at DAT_00247a10, skipping by type on bytes +0x3a
//     and +0x6f
//   - delta = player - entity, from DAT_000b0a46 and DAT_000b0a4a (both the
//     high half of a 16.16 fixed-point pair)
//   - RANGE GATE: |dx| and |dz| must be < 0x3001. 0x3000 is 12288 world units,
//     i.e. 24 cells - so the tracker sees 24 cells on each axis.
//   - the pair is scaled by >> 9, which is the world-to-CELL shift, then run
//     through a rotation built from `-DAT_000b0ba2 & 0xfff` - the player's yaw
//     negated in the 4096-step turn - so contacts rotate with the player and
//     the display is always player-forward.
//   - each blip is a 2x2 point (entry +0x10 and +0x12 both 2), primitive
//     LAB_0004c904, colour bytes 0xff, 0x7f, 0x00, and the entry stride here is
//     0x14 - a THIRD entry size, smaller than the 0x1c flat and 0x2c textured
//     ones.
//   - the viewport is set to (0x108, 0xc0) = (264, 192) for the pass and put
//     back to (0xa0, 0x78) = screen centre afterwards, so blip coordinates are
//     relative to the dish's centre at 264,192.
//
// THE PING (FUN_0003a1d0): when DAT_00247f6c is set it plays sound 0x15 if the
// contact count is < 1, or 0x16 if something is in range - so the tracker has
// two ping sounds and swaps them on contact.
//
// THE DISH: assembled from several quads at x 0xdf..0x131 (223..305). Two of
// them are thin 5-pixel strips at y 0x9b..0xa0 (155..160) and y 0xe0..0xe5
// (224..229) - the top and bottom edges - drawn from descriptor 203 with its UVs
// swapped vertically, which is how one strip serves both edges. Colour 0x80
// neutral, textured primitive.
//
// THE QUADRANTS AND THE SWEEP - read, and the dish now assembles correctly.
//
// The 40x32 tile is the dish's TOP-LEFT quarter, with the circle's centre at the
// tile's own bottom-right corner. The other three quarters are that tile
// mirrored horizontally, vertically, and both.
//
// One quadrant quad is at x 0x108..0x130 (264..304), y 0xa0..0xc0 (160..192) -
// 40x32, exactly a tile, and the top-right quarter of the dish whose centre is
// (264,192). The other three are the same tile with its UVs swapped to mirror
// it, so the whole dish is 80x64 spanning x 224..304, y 160..224, with the two
// 5-pixel edge strips just outside at y 155..160 and 224..229.
//
// The tile is chosen as `&UNK_00241590 + (DAT_000acb94 >> 0x10) * 0x10`, i.e.
// descriptor 205 + frame, so DAT_000acb94's high word is the sweep frame.
//
// THE ANIMATION, which is the bit worth matching:
//   - a counter DAT_00247f70 ticks down every frame
//   - the sweep only advances when (DAT_00247f70 & 1) != 0, so EVERY OTHER TICK
//   - frame = (frame + 1) & 7, so it cycles through all 8 tiles
//   - when it wraps back to 0 it sets DAT_000acb9a = 0x20, which gates further
//     advances - a PAUSE OF 32 before the sweep starts over
// So: 8 frames at one per two ticks, then a 32-tick rest, repeating.
// (DAT_000acb98 is a one-shot gate within that; its exact reset was not traced,
// and it does not change the overall cadence.)

// ============================================================================
// TEXT IS ASCII - confirmed from a second direction.
//
// FUN_0003ab5c (the general character draw) takes a character code and computes
// its glyph as `code - 0x20`, so the font is ASCII from space onward. Space is
// glyph 0, and '0' is 0x30 - 0x20 = 0x10 - which is exactly the digit offset the
// number routines use. Two independent derivations of the same mapping.
//
// It also reveals a THIRD glyph table: codes >= 0x80 use `code - 0x80` against
// advances at DAT_00247d28 and UVs at panel offset 0x1570 (descriptor 343),
// beyond this sheet's 343 descriptors. Probably an accented or second-language
// set. Not needed for the HUD.
//
// ============================================================================
// OVER-100 HEALTH (derm patches) - FULLY DECODED from FUN_0003a674.
//
//   if (health <= 100) nothing is drawn
//   value = min(health, 200) - 100
//   bars  = value / 10, and if that rounds to 0 it is forced to 1
//   for i in 0..bars-1:
//       x    = 0x109 + i*4  .. 0x10b + i*4      (265 + i*4, three wide)
//       y    = 0x2f - heightTable[i] .. 0x2f    (bottom fixed at 47)
//   flat quad, CLUT index 0x68, +0x0b = 1
//
// So they are up to TEN three-pixel-wide bars on a 4-pixel pitch, hanging from a
// fixed bottom at y 47, growing UPWARD by a per-bar amount from a height table
// at DAT_000acba4. That table is data, not code, so the ten heights are not in
// the decompilation - they need dumping from 0x000acba4 (ten int16s).
//
// The alternate HUD layout (DAT_000ae0a4 && DAT_000ae0a5) moves the row's bottom
// from 0x2f to 0xdf, matching how it moves the health frame.
//
// HOW HEALTH OVER 100 IS ACTUALLY REACHED - traced. There are THREE separate
// health pickup types in the collection handler (the switch around line 28745):
//
//   type 0x11  raises health UP TO 100 if below it, never above, and also sets
//              DAT_000b0ae4 = 900 - some 900-tick effect
//   type 0x14  health += amount, then CLAMPED AT 100
//   type 0x15  health += amount, with NO CLAMP AT ALL
//   type 0x17  health = 200 outright
//
// So 0x14 is the ordinary medikit and 0x15 is the one that can exceed 100 - the
// derm patch. Because it is unclamped, the amount comes from the pickup record's
// own field (`param_2`), not from a fixed +1, and the reachable total is bounded
// only by how many are placed in a level. Type 0x17 sets 200 flat, which is
// exactly the value FUN_0003a674 clamps to and the point at which all ten bars
// show.
//
// That resolves the earlier worry: the ramp is NOT unreachable. With 0x15
// granting more than 1, or 0x17 existing at all, the full 1,3,5,7,8... curve is
// reachable in normal play, and the ten-bar display makes sense.
//
// NOT YET ESTABLISHED: which pickup ids in PICKMOD map to 0x14 / 0x15 / 0x17, and
// what the 900-tick DAT_000b0ae4 effect on 0x11 is. Both are in the pickup tables
// rather than this switch.
//
// THE TEN HEIGHTS, dumped from 0x000acba4 by Edward as int16s:
//
//     1, 3, 5, 7, 8, 8, 8, 8, 8, 8
//
// So the bars ramp 1,3,5,7 then plateau at 8 - a short curve rising to a flat
// top, read left to right. That plateau is why the row looks like a small block
// once several patches are held rather than a continuous slope.
//
// STILL MISSING: the colour. CLUT index 0x68 is grey-green in PNL0 and brown in
// PNL1, and Edward has confirmed it is in neither PANEL.PAL nor NEWFONT.PAL, nor
// any other palette the game actually needs. So the blue is presumably a literal
// in the executable - the same situation as the weapon frame sizes. Until it
// turns up, HUD_DERM_BLUE below is sampled from a screenshot and marked as such.

// STILL NEEDED BEFORE THIS CAN BE BUILT:
//   - which descriptor index is which element. The layout is regular (16x16 and
//     16x24 runs, then 9-pixel-tall font glyphs from index 23 on) so this is a
//     matter of matching indices to the dumped sheet, not guesswork.
//   - what the display-list +0x08..0x0a field actually is, since both bars set
//     it to 5. Reading LAB_000498dc (the flat-quad primitive) would settle it.
//   - the motion tracker's assembly: which arc descriptors, at what positions,
//     and how the sweep is animated.
//   - nothing about the display list. Both primitives are lifted and the entry
//     layout is fully accounted for.
//   - nothing from Edward. Everything needed is in the dump and the sheets.

namespace ALTEngine::Renderer
{
    // ---- font -------------------------------------------------------------
    // Descriptor index of glyph 0 in the panel set (FUN_00039068 walks from
    // DAT_002408c0 + 0x170, which is index 23).
    // Font A - the small font, used by the ammo row.
    inline constexpr int HUD_FONT_FIRST_DESCRIPTOR = 23;
    inline constexpr int HUD_FONT_GLYPH_COUNT = 0x5b; // 91
    inline constexpr int HUD_FONT_GLYPH_HEIGHT = 9;

    // Height of the quad both number routines emit: FUN_000393a0 and
    // FUN_000395d0 both write y and y + 9, a literal, regardless of the glyph's
    // descriptor height. The WIDTH comes from the advance table, not the rect.
    inline constexpr int HUD_FONT_QUAD_HEIGHT = 9;

    // Font B - the large font, used by the health row. Starts immediately after
    // font A. Its glyph count is not established, only its base and digits.
    inline constexpr int HUD_FONT_B_FIRST_DESCRIPTOR = 114;
    inline constexpr int HUD_FONT_B_GLYPH_HEIGHT = 10;

    // Glyph index of '0'. FUN_000393a0 computes its digit index as
    // value/100 + 0x10, so the digits start at glyph 0x10.
    inline constexpr int HUD_FONT_DIGIT_GLYPH = 0x10;

    // Descriptor index for a glyph, or -1 if out of range.
    inline int HudGlyphDescriptor(int glyphIndex)
    {
        if (glyphIndex < 0 || glyphIndex >= HUD_FONT_GLYPH_COUNT) { return -1; }
        return HUD_FONT_FIRST_DESCRIPTOR + glyphIndex;
    }

    // Descriptor index for a decimal digit 0-9 in font A (descriptors 39-48).
    inline int HudDigitDescriptor(int digit)
    {
        if (digit < 0 || digit > 9) { return -1; }
        return HudGlyphDescriptor(HUD_FONT_DIGIT_GLYPH + digit);
    }

    // Same for font B (descriptors 130-139).
    inline int HudDigitDescriptorFontB(int digit)
    {
        if (digit < 0 || digit > 9) { return -1; }
        return HUD_FONT_B_FIRST_DESCRIPTOR + HUD_FONT_DIGIT_GLYPH + digit;
    }

    // ---- display list ------------------------------------------------------
    // CLUT indices the HUD bars use, read from the flat-quad primitive.
    inline constexpr int HUD_CLUT_BAR_FILL = 0x05;   // pale green
    inline constexpr int HUD_CLUT_BAR_FRAME = 0x5f;  // near black

    // Neutral value for a textured quad's colour bytes. The primitive shifts
    // each channel right by 2 into a 6-bit register, so 128 sits mid-range.
    inline constexpr int HUD_COLOUR_NEUTRAL = 0x80;

    // ---- health row, from the draw around 29750-29840 ----------------------
    inline constexpr int HUD_HEALTH_TEXT_X = 0xdd;      // 221
    // POSSIBLE DISCREPANCY. 0xe1 is what the draw code uses, but measuring
    // Edward's screenshot of the original puts the green bar's left edge nearer
    // x 257 in 320-space, with the "100" occupying roughly 224..253 - i.e. the
    // bar starts AFTER the number rather than underneath it. At 225 the three
    // 12-pixel font-B digits (221..257) overlap the bar's first few pixels.
    // Left at the decompiled value; if it looks wrong side by side, this is the
    // constant to nudge.
    inline constexpr int HUD_HEALTH_BAR_LEFT = 0xe1;    // 225
    inline constexpr int HUD_HEALTH_BAR_RIGHT = 0x12f;  // 303 = 225 + 19*4 + 2, i.e. 20 slots
    inline constexpr int HUD_HEALTH_ROW_TOP = 0x16;     // 22
    inline constexpr int HUD_HEALTH_ROW_BOTTOM = 0x30;  // 48

    // The alternate arrangement, used when DAT_000ae0a4 && DAT_000ae0a5.
    inline constexpr int HUD_HEALTH_ROW_TOP_ALT = 0xc6;    // 198
    inline constexpr int HUD_HEALTH_ROW_BOTTOM_ALT = 0xe0; // 224

    // Colour band thresholds. Below LOW the bar flashes on bit 3 of a tick
    // counter, and only while the damage cooldown is zero.
    inline constexpr int HUD_HEALTH_BAND_LOW = 0x1f;  // 31
    inline constexpr int HUD_HEALTH_BAND_MID = 0x4c;  // 76

    // Fill width for a health value, exactly as the original computes it.
    // Deliberately NOT clamped to the frame: at health 99 this returns 79 for a
    // 78-wide frame, which is what the original does. See the note above.
    inline int HudHealthBarFillWidth(int health)
    {
        if (health <= 0) { return 0; }
        if (health < 100) { return (health * 0x50) / 100; }
        // 0x4e (78) after all. 20 slots of 2px at a 4px pitch starting at 225 end
        // at 303, which is 78 wide - so the original's clamp was right and the
        // earlier 19-segment count came from drawing 3px-wide segments from the
        // bar's left edge instead of into the artwork's slots.
        return 0x4e;
    }

    // ---- motion tracker ----------------------------------------------------
    // Eight sweep frames, one quadrant each. See the note above; the draw
    // position and the frame timing are both still unknown.
    inline constexpr int HUD_TRACKER_FIRST_DESCRIPTOR = 205;
    inline constexpr int HUD_TRACKER_FRAME_COUNT = 8;
    inline constexpr int HUD_TRACKER_QUADRANT_W = 40;
    inline constexpr int HUD_TRACKER_QUADRANT_H = 32;

    // Dish centre, from the viewport FUN_0003a008 sets for the blip pass.
    inline constexpr int HUD_TRACKER_CENTRE_X = 0x108;  // 264
    inline constexpr int HUD_TRACKER_CENTRE_Y = 0xc0;   // 192

    // Dish extent and its two edge strips.
    inline constexpr int HUD_TRACKER_LEFT = 0xdf;        // 223
    inline constexpr int HUD_TRACKER_RIGHT = 0x131;      // 305
    inline constexpr int HUD_TRACKER_TOP_STRIP_Y = 0x9b; // 155
    inline constexpr int HUD_TRACKER_BOTTOM_STRIP_Y = 0xe0; // 224
    inline constexpr int HUD_TRACKER_STRIP_DESCRIPTOR = 203;

    // Contacts are gated on |dx| and |dz| < 0x3001 world units, then shifted
    // right by 9 into cells.
    inline constexpr int HUD_TRACKER_RANGE = 0x3000;     // 12288 = 24 cells
    inline constexpr int HUD_TRACKER_BLIP_SIZE = 2;

    // Ping sounds: index 0 when nothing is in range, 1 on contact.
    inline constexpr int HUD_TRACKER_SOUND_CLEAR = 0x15;
    inline constexpr int HUD_TRACKER_SOUND_CONTACT = 0x16;

    // Dish geometry: four 40x32 quadrants around the centre.
    inline constexpr int HUD_TRACKER_DISH_LEFT = HUD_TRACKER_CENTRE_X - HUD_TRACKER_QUADRANT_W; // 224
    inline constexpr int HUD_TRACKER_DISH_TOP = HUD_TRACKER_CENTRE_Y - HUD_TRACKER_QUADRANT_H;  // 160

    // Sweep timing, from the counter above.
    inline constexpr int HUD_TRACKER_TICKS_PER_FRAME = 2;
    inline constexpr int HUD_TRACKER_PAUSE_TICKS = 0x20; // 32, after the cycle wraps

    // Advances the sweep. `frame` cycles 0..7; `timer` is caller-owned state.
    // Returns the frame to draw.
    inline int HudTrackerAdvance(int& frame, int& timer, int& pause)
    {
        if (pause > 0) { pause--; return frame; }
        if (++timer < HUD_TRACKER_TICKS_PER_FRAME) { return frame; }
        timer = 0;
        frame = (frame + 1) & 7;
        if (frame == 0) { pause = HUD_TRACKER_PAUSE_TICKS; }
        return frame;
    }

    // ---- live minimap (Modern option) ---------------------------------------
    // Not part of the original HUD - the original only showed a map in the pause
    // menu - so this is placement rather than transcription. Lined up with the
    // rest of the HUD so it does not look bolted on:
    //   x = 14, the ammo frame's own left gap from the screen edge
    //   y = 19 and 34 tall, exactly the health frame's rows, so the two boxes
    //       sit on the same line across the top of the screen
    //   width = 98, the health frame's width, so left and right balance
    // Edge strips sit just outside it, the way the tracker's do.
    // Doubled from the health frame's 98x34 (Edward, 2026). 14 + 196 = 210,
    // which still clears the health frame's left edge at 215.
    // Edge strip thickness in HUD units, matching the tracker's own strips
    // (its quads are y 0x9b..0xa0, five rows).
    inline constexpr int HUD_MINIMAP_STRIP_H = 5;

    // The tracker dish, its edge strips and the minimap's border composite at
    // FIFTY PERCENT.
    //
    // Not a value picked by eye. The panel artwork is 15-bit BGR555 with bit 15
    // as the semi-transparency flag (visible in the CLUT dumps as `stp`), and
    // semi-transparency in that format means an even blend of source and
    // destination - the PlayStation's default and what this port of it inherits.
    // So 128 rather than the 200 I had guessed at, which is why ours read as far
    // more solid than the original (Edward, 2026).
    inline constexpr int HUD_TRACKER_ALPHA = 128;

    inline constexpr int HUD_MINIMAP_SCALE = 2;

    // The PAUSE menu's map, in that screen's own pixel space rather than the
    // HUD's 320x240. Bigger and further right than the old 520x400 at panelX,
    // matching how much of the screen the original's gives it.
    //
    // NOT LOCATED IN THE DECOMPILATION. Several functions touch the collision
    // grid base and none is obviously the pause map renderer; the original's own
    // geometry and colours are still unfound, so these are layout values.
    inline constexpr int PAUSE_MAP_X_OFFSET = 60;   // extra, beyond panelX
    inline constexpr int PAUSE_MAP_WIDTH = 660;
    inline constexpr int PAUSE_MAP_HEIGHT = 500;
    inline constexpr int HUD_MINIMAP_X = 14;
    inline constexpr int HUD_MINIMAP_Y = 19;   // == HUD_OVERLAY_HEALTH_Y, declared below
    inline constexpr int HUD_MINIMAP_W = 98 * HUD_MINIMAP_SCALE;
    inline constexpr int HUD_MINIMAP_H = 34 * HUD_MINIMAP_SCALE;

    // ---- over-100 health (derm patches) -------------------------------------
    // Geometry transcribed from FUN_0003a674; see the long note above.
    inline constexpr int HUD_DERM_X = 0x109;          // 265, first bar's left edge
    inline constexpr int HUD_DERM_BAR_WIDTH = 3;      // 0x109..0x10b inclusive
    inline constexpr int HUD_DERM_PITCH = 4;
    inline constexpr int HUD_DERM_BOTTOM = 0x2f;      // 47, fixed
    inline constexpr int HUD_DERM_BOTTOM_ALT = 0xdf;  // 223, alternate HUD layout
    inline constexpr int HUD_DERM_MAX_BARS = 10;

    // Per-bar heights at DAT_000acba4 (Edward, 2026).
    inline constexpr int HUD_DERM_HEIGHTS[HUD_DERM_MAX_BARS] = { 1, 3, 5, 7, 8, 8, 8, 8, 8, 8 };

    // SAMPLED FROM A SCREENSHOT by Edward, not from a palette - see the note
    // above. The one value in the derm patch path that is not derived.
    //
    // A desaturated teal rather than a blue: RGB 90/154/156, which converts to
    // HSL 121/64/116 on the 0-240 scale and matches the figure Edward measured.
    // The 64/112/216 previously here was MY invention, not a sample - I described
    // it as sampled when it was not, which was wrong of me.
    inline constexpr int HUD_DERM_BLUE_R = 90;
    inline constexpr int HUD_DERM_BLUE_G = 154;
    inline constexpr int HUD_DERM_BLUE_B = 156;

    // How many bars to draw, exactly as FUN_0003a674 computes it: nothing at or
    // below 100, otherwise (min(health,200) - 100) / 10, forced to at least 1.
    inline int HudDermBarCount(int health)
    {
        if (health <= 100) { return 0; }
        int value = (health > 200 ? 200 : health) - 100;
        int bars = value / 10;
        if (bars == 0) { bars = 1; }
        return bars > HUD_DERM_MAX_BARS ? HUD_DERM_MAX_BARS : bars;
    }

    // ---- overlay artwork ---------------------------------------------------
    // Both frames are ordinary descriptors drawn at positions the decompilation
    // gives, so nothing here is measured off a screenshot any more:
    //
    //   health  FUN_000398a4  descriptor 174, quad x 0xd7..0x139 (215..313),
    //                         y 0x13..0x35 (19..53)          = 98 x 34
    //   ammo    same family   descriptor 173, quad x 0xe..0x90 (14..144),
    //                         y 0xd3..0xe5 (211..229)        = 130 x 18
    //
    // Descriptor 174's rect is (0,115) 98x34 and 173's is (0,97) 130x18, matching
    // the sizes Edward measured on the sheet - 174 exactly, 173 a slightly wider
    // crop than his 126x15.
    //
    // AND IT ALL RECONCILES. 215 + the health slots' +10 inset = 225 = 0xe1, the
    // health bar left read out of the draw code. 14 + the ammo slots' +34 = 48 =
    // 0x30, likewise. Two independent numbers landing on the decompiled constants
    // is what says the slot measurements and the frame positions are both right.
    inline constexpr int HUD_OVERLAY_HEALTH_DESCRIPTOR = 174;
    inline constexpr int HUD_OVERLAY_HEALTH_X = 215;
    inline constexpr int HUD_OVERLAY_HEALTH_Y = 19;
    inline constexpr int HUD_OVERLAY_HEALTH_Y_ALT = 195; // DAT_000ae0a4 && a5 layout

    inline constexpr int HUD_OVERLAY_AMMO_DESCRIPTOR = 173;
    inline constexpr int HUD_OVERLAY_AMMO_X = 14;
    inline constexpr int HUD_OVERLAY_AMMO_Y = 211;

    // ---- bar slots, MEASURED FROM THE OVERLAY ARTWORK -----------------------
    // The frames have transparent slots the bar shows through, so their spacing
    // is not a free choice - it is in the art. Scanning the two overlay regions
    // for columns that are keyed transparent gives, exactly:
    //
    //   health: 20 slots, each 2px wide, at overlay-relative x 10,14,...,86
    //   ammo:   22 slots, each 2px wide, at overlay-relative x 34,38,...,118
    //
    // So the pitch really is 4, but the first slot is inset and each slot is 2
    // wide, not 3. Drawing from the bar's own left edge at 3px wide put every
    // segment half onto its slot and dropped the last one entirely
    // (Edward, 2026).
    //
    // Slot heights vary along the bar - that staircase is in the artwork - but
    // they do not need measuring: the fill is drawn full-height in each slot's
    // column and the opaque parts of the overlay, drawn on top, clip it.
    inline constexpr int HUD_BAR_SLOT_PITCH = 4;
    inline constexpr int HUD_BAR_SLOT_WIDTH = 2;

    inline constexpr int HUD_HEALTH_SLOT_FIRST = 10;  // relative to the overlay's x
    inline constexpr int HUD_HEALTH_SLOT_COUNT = 20;

    inline constexpr int HUD_AMMO_SLOT_FIRST = 34;
    inline constexpr int HUD_AMMO_SLOT_COUNT = 22;

    // How many slots are lit. Health is a percentage of the full bar; ammo is one
    // slot per round, which is what the original's `segments * 4` amounts to.
    inline int HudHealthLitSlots(int health)
    {
        if (health <= 0) { return 0; }
        int lit = (health * HUD_HEALTH_SLOT_COUNT) / 100;
        return lit > HUD_HEALTH_SLOT_COUNT ? HUD_HEALTH_SLOT_COUNT : lit;
    }

    inline int HudAmmoLitSlots(int ammoTotal)
    {
        if (ammoTotal <= 0) { return 0; }
        return ammoTotal > HUD_AMMO_SLOT_COUNT ? HUD_AMMO_SLOT_COUNT : ammoTotal;
    }

    // ---- ammo row, from FUN_0003aac8, in the original's 320x240 space -------
    inline constexpr int HUD_VIRTUAL_WIDTH = 320;
    inline constexpr int HUD_VIRTUAL_HEIGHT = 240;

    // Ammo number's left edge, as the decompilation has it.
    //
    // RESOLVED: this looked 3 pixels too far right until the digit ADVANCE was
    // fixed. FUN_00039068 builds advances as u1 - u0 from the raw descriptor,
    // and BxParser adds +1 to width on read - so font A's digits advance 8, not
    // the 9 the loader reports. At 8 the three digits span 18..42 and leave a
    // 2-pixel gap before the first bar slot at 44, which is right. The earlier
    // overlay-relative inset was papering over the advance bug and is gone.
    inline constexpr int HUD_AMMO_TEXT_X = 0x12;   // 18
    inline constexpr int HUD_AMMO_ROW_TOP = 0xd9;  // 217
    inline constexpr int HUD_AMMO_ROW_BOTTOM = 0xe0; // 224
    inline constexpr int HUD_AMMO_BAR_LEFT = 0x30; // 48
    inline constexpr int HUD_AMMO_BAR_RIGHT = 0x88; // 136
    inline constexpr int HUD_AMMO_BAR_MAX_SEGMENTS = 0x16; // 22
    inline constexpr int HUD_AMMO_BAR_SEGMENT_WIDTH = 4;

    // Right edge of the ammo bar's fill for a given total, matching the
    // original's clamp-then-scale.
    inline int HudAmmoBarFillRight(int ammoTotal)
    {
        int segments = ammoTotal;
        if (segments < 0) { segments = 0; }
        if (segments > HUD_AMMO_BAR_MAX_SEGMENTS) { segments = HUD_AMMO_BAR_MAX_SEGMENTS; }
        return HUD_AMMO_BAR_LEFT + segments * HUD_AMMO_BAR_SEGMENT_WIDTH;
    }

    // WHICH FILE TO LOAD - use this one. 0 = PNL0GFXU.16, 1 = PNL1GFXU.16.
    // Chapters 1 and 2 share the first sheet; chapter 3 uses the second.
    inline int HudPanelFileForLevelId(int levelId)
    {
        if (levelId < 0) { return 0; }
        return (levelId < 0x16) ? 0 : 1;
    }

    // The original's raw three-way bucket, kept only for reference while the
    // third one is unaccounted for. NOT a file index - see the note above.
    inline int HudPanelBucketForLevelId(int levelId)
    {
        if (levelId < 0) { return 0; }
        if (levelId < 0x0c) { return 0; }
        if (levelId < 0x16) { return 1; }
        return 2;
    }
}
