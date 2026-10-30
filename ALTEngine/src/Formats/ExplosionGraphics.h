#pragma once

#include <array>
#include <cstdint>

namespace ALTEngine::Formats
{
    // EXPLGFX.B16 - the explosion and impact effect frames.
    //
    // NOT the F0## sprite format the enemy and weapon .B16 files use. This is
    // the SAME texture-page format as the level graphics (111GFX.B16), so the
    // port already has everything needed to read it: BndParser finds the
    // chunks, BxParser reads the rectangles, and the CL chunks are the palettes.
    // No new format work, and no entry needed in SpriteFrameDimensions - the
    // dimensions are in the file.
    //
    // Container, verified against the real file:
    //   FORM/PSXT
    //     INFO  16 bytes  = 256, 256, 4, 45, 2, 2, 6, 1   (u16 LE)
    //     TP00  65536     256x256 8-bit page 0
    //     CL00  516       palette for page 0
    //     BX00  220       36 rectangles
    //     TP01  65536     256x256 8-bit page 1
    //     CL01  516       palette for page 1
    //     BX01  56        9 rectangles
    //
    // INFO's fourth value is 45, which is exactly 36 + 9. The file declares its
    // own total rectangle count, so nothing here is inferred from sizes.
    namespace ExplosionGraphics
    {
        inline constexpr int PAGE_COUNT = 2;
        inline constexpr int PAGE_WIDTH = 256;
        inline constexpr int PAGE_HEIGHT = 256;
        inline constexpr int TOTAL_RECTANGLES = 45;

        // EVERY EFFECT TABLE, read out of the image. Each is
        //     +0x00 dword X scale, +0x04 dword Y scale,
        //     then one dword per frame - an index into the FLAT, global uv rect
        //     list - terminated by -1.
        //
        // The uv indices are global across the file: page 0 contributes 0-35 and
        // page 1 contributes 36-44, which is exactly how BndTextureSet already
        // builds uvRects.
        //
        //   0x000acf5c  uv  0..8    scale 0x0c00  9 frames   (no direct xref)
        //   0x000ad008  uv 36..44   scale 0x0c00  9 frames   CRATE
        //   0x000ad0b4  uv  9..17   scale 0x0f3c x 0x125c    BARREL
        //   0x000ad160  uv 18..23   scale 0x0a00  6 frames   (no direct xref)
        //   0x000ad20c  uv 25..31   scale 0x0800  7 frames   particle type 9
        //   0x000ad2b8  uv 25..31   scale 0x0800  7 frames   particle type 8
        //   0x000ad364  uv 25..31   scale 0x0800  7 frames   any other particle
        //
        // The last three are chosen by FUN_0002ad48 on the particle's type byte
        // and share their frames, differing only in the per-frame offsets at
        // +0x66/+0x86. So a bullet impact, whatever hit, runs uv 25..31.
        //
        // NOTE THE CRATE/BARREL PAIRING. FUN_0002aaf0 (sound 0x29 4101crat, the
        // crate) uses the 84x84 grid on page 1; FUN_0002ab54 (sound 0x21
        // 5001barr, the barrel) uses uv 9..17 on page 0 and is markedly taller
        // than wide. That is the reverse of what was assumed here at first.
        //
        // This also corrects the rectangle grouping guessed earlier from the
        // dimensions alone: 9..17 is ONE nine-frame run, not a 2 + 7 split, and
        // the last group is 25..31, not 24..29 plus 30..35.
        struct EffectTable
        {
            int firstRecord;
            int frameCount;
            int scaleX;
            int scaleY;
        };

        inline constexpr EffectTable EFFECT_UNKNOWN_A{  0,  9, 0x0c00, 0x0c00 };
        inline constexpr EffectTable EFFECT_CRATE     { 36, 9, 0x0c00, 0x0c00 };
        inline constexpr EffectTable EFFECT_BARREL    {  9, 9, 0x0f3c, 0x125c };
        inline constexpr EffectTable EFFECT_UNKNOWN_B { 18, 6, 0x0a00, 0x0a00 };
        inline constexpr EffectTable EFFECT_IMPACT    { 25, 7, 0x0800, 0x0800 };

        // THE VENT JETS (FUN_0002caa8 steam, FUN_0002ccfc flame, from the
        // particle draw's table at 0x2d6a0). Each jet is a billboard of one of
        // records 2-8, chosen afresh every frame by its life (+0x1c, clamped
        // to 0-0x1f) plus a random 0-3: frame = table[0x20 - life + r]. Its
        // half size on screen is a per-life value times a constant over the
        // depth, which at the focal length 0x100 is that value times
        // constant / 0x100 in world units. A steam jet is drawn at grey
        // 0x41 + life, a flame at 0x69 (0x80 is white).
        //   steam  frames 0xac20c, widths 0xac10c, heights 0xac18c, x 15000;
        //          a vertical jet's width and height are swapped.
        //   flame  frames 0xac354, lengths 0xac254 (x 16000), breadths
        //          0xac2d4 (x 8000 across a horizontal jet, 12000 a vertical);
        //          horizontal: length across, breadth up; vertical: the other way.
        // The flame draws through LAB_0004b400 while life is 0x1c or more and
        // LAB_0004b5cc after; the steam always through LAB_0004b5cc. Both are
        // drawn here as cut-out billboards (the blend of those two fills is not
        // settled).
        namespace VentSprites
        {
            inline constexpr int16_t STEAM_FRAMES[36] = {
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6,
                7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8 };
            inline constexpr int16_t FLAME_FRAMES[36] = {
                2, 2, 2, 2, 4, 3, 4, 3, 3, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 5, 4, 4, 5, 4,
                4, 4, 5, 5, 5, 6, 5, 6, 6, 7, 7, 7 };
            inline constexpr int8_t STEAM_WIDTH[32] = {
                10, 10, 8, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 8, 8, 8, 8 };
            inline constexpr int8_t STEAM_HEIGHT[32] = {
                5, 5, 5, 5, 5, 6, 6, 6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2 };
            inline constexpr int8_t FLAME_LENGTH[32] = {
                4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8 };
            inline constexpr int8_t FLAME_BREADTH[32] = {
                8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3 };
            inline constexpr float FOCAL = 0x100;
            inline constexpr int STEAM_GREY_BASE = 0x41;
            inline constexpr int FLAME_GREY = 0x69;
            inline constexpr float WHITE = 0x80;

            struct Look { int record; float halfWidth, halfHeight, grey; };

            // `steam` and `vertical` from the jet's type; `random` any value.
            inline Look For(bool steam, bool vertical, int life, int random)
            {
                const int l = life < 0 ? 0 : (life > 0x1f ? 0x1f : life);
                const int index = 0x20 - l + (random & 3);
                Look look{};
                if (steam)
                {
                    look.record = STEAM_FRAMES[index];
                    const float w = STEAM_WIDTH[l] * 15000.0f / FOCAL;
                    const float h = STEAM_HEIGHT[l] * 15000.0f / FOCAL;
                    look.halfWidth = vertical ? h : w;
                    look.halfHeight = vertical ? w : h;
                    look.grey = static_cast<float>(STEAM_GREY_BASE + l) / WHITE;
                }
                else
                {
                    look.record = FLAME_FRAMES[index];
                    const float length = FLAME_LENGTH[l] * 16000.0f / FOCAL;
                    if (vertical)
                    {
                        look.halfHeight = length;
                        look.halfWidth = FLAME_BREADTH[l] * 12000.0f / FOCAL;
                    }
                    else
                    {
                        look.halfWidth = length;
                        look.halfHeight = FLAME_BREADTH[l] * 8000.0f / FOCAL;
                    }
                    look.grey = static_cast<float>(FLAME_GREY) / WHITE;
                }
                return look;
            }
        }

        // THE FLAMETHROWER'S FLAMES (particle type 3), drawn by FUN_0002c94c, not
        // as a tracer. Each flame is a square sprite from one of the effect
        // records 0-7, picked at random every frame (FUN_000100ec & 7, into the
        // same rect table at 0x2405a0 the vents use), so it flickers. Its
        // half-size on screen is k / distance with k = (0x10 - life) * 0x600,
        // or * 0xa00 once life is under 4, so a flame starts as a point at the
        // muzzle and swells as it flies, fastest in its last ticks. Its shade
        // byte is 0xfa (over the 0x80 that leaves a texel as it is).
        // The world size is k / FOCAL, the same conversion as the vents.
        // It is drawn from 0x10 away, not the 0x200 the rounds keep.
        namespace FlameSprites
        {
            inline constexpr int NEAR_CUTOFF = 0x10;
            inline constexpr float SHADE = static_cast<float>(0xfa) / VentSprites::WHITE;

            struct Look { int record; float halfSize; };

            inline Look For(int life, int random)
            {
                const int l = life < 0 ? 0 : (life > 0x10 ? 0x10 : life);
                const int k = (0x10 - l) * (l < 4 ? 0xa00 : 0x600);
                return { random & 7, static_cast<float>(k) / VentSprites::FOCAL };
            }
        }

        // The scale is a 12.12 fixed-point multiplier the original divides by
        // distance to get screen size. A world-space billboard needs it as a
        // world size instead, so the rect's pixel size is multiplied by the
        // scale and then by this. GUESS - tuned so a crate's 84-pixel frame at
        // scale 0x0c00 comes out about a cell across.
        inline constexpr float WORLD_UNITS_PER_SCALED_PIXEL = 5.0f;
        inline constexpr int SCALE_ONE = 0x1000;

        // FRAME DURATION: 4 TICKS, CONFIRMED.
        //
        // These do not run on the sprite animation VM at all. FUN_0002aaf0
        // schedules an effect as a TASK - FUN_000124f4 pops an object off a free
        // list and stores a type id at +0 and an update handler at +4 - and the
        // handler for this one is at 0x00042c5c. It does:
        //
        //     frame = obj[+0x14] >> 2
        //     FUN_000429f0(x, y, z, frame, DAT_000ad008)
        //     if that returned non-zero  -> free the task, the effect is over
        //     else                       -> obj[+0x14]++
        //
        // A counter incremented once per tick and shifted right by 2 is a frame
        // every FOUR ticks. Nine frames therefore run 36 ticks - about 1.2
        // seconds at 30Hz - which is twice as long as the 2 that was guessed
        // here.
        //
        // The position lives at +0x06, +0x0a and +0x0e as 16.16 fixed, and
        // DAT_000ad008 is this effect's own frame descriptor table. There is a
        // second, identical handler at 0x00042cac using DAT_000ad0b4 instead, so
        // effect variants differ only by which table they point at.
        inline constexpr uint16_t FRAME_DURATION = 4;
        inline constexpr uint16_t FRAME_DURATION_GUESS = FRAME_DURATION;  // old name, kept for callers

        // The counter shift the handler applies.
        inline constexpr int FRAME_COUNTER_SHIFT = 2;

        // TWO EFFECT SPAWNERS, and they are the crate and the barrel:
        //
        //   FUN_0002aaf0  sound 0x29 (4101crat)  table DAT_000ad008
        //   FUN_0002ab54  sound 0x21 (5001barr)  table DAT_000ad0b4, and it also
        //                 raises bit 4 of DAT_000b0cc1
        //
        // Identical handlers otherwise. So a crate breaking and a barrel
        // exploding are the same code with a different frame table - which is
        // why page 0 of this file carries several sequences and page 1 only one.
        //
        // THE DESCRIPTOR TABLE, from FUN_000429f0:
        //   +0x00  dword  X scale for the whole effect
        //   +0x04  dword  Y scale
        //   +0x66  per-frame X offset, stride 2, read as a dword and >> 16
        //   +0x86  per-frame Y offset, same
        //   frame*4 + 8   index into a 16-byte UV record table at DAT_002405a0,
        //                 and -1 THERE IS THE END OF THE ANIMATION - the frame
        //                 count is not stored, it is walked until the sentinel
        //
        // The quad's size comes from the UV rect's own width and height, scaled
        // by the table's scale and divided by the distance, so an effect is a
        // perspective-scaled billboard. Drawing a world-space quad, as this port
        // does, gets the same result without the manual divide.
        //
        // CULLING is not the same as the particles': FUN_000429f0 keeps the
        // effect alive but skips the draw when distance*4 is >= 0x1e00 or
        // <= 0x10, and when it falls outside the screen bounds. Note it does NOT
        // end the effect - a blast that goes off out of sight still finishes in
        // its own time.
        inline constexpr int EFFECT_FAR_CUTOFF = 0x1e00;
        inline constexpr int EFFECT_NEAR_CUTOFF = 0x10;

        // Under this distance the effect draws at full brightness; past it the
        // draw-distance fade applies (FUN_0004ecdc).
        inline constexpr int EFFECT_FULL_BRIGHT_DISTANCE = 0x1000;
    }
}
