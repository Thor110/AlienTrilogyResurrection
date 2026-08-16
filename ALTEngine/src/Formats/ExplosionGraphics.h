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

        // Page 1 is a clean 3x3 grid of nine 84x84 frames at (0,0) to (168,168).
        // Uniform size, uniform spacing, filling the page exactly - that is an
        // animation strip and nothing else, and at 84x84 it is the largest
        // effect in the file. This is the barrel blast.
        inline constexpr int BARREL_PAGE = 1;
        inline constexpr int BARREL_FRAME_COUNT = 9;
        inline constexpr int BARREL_FRAME_SIZE = 84;

        // Page 0's 36 rectangles are several sequences packed together. The
        // grouping below is READ FROM THE DIMENSIONS, not from anything that
        // labels them - consecutive runs that grow and then shrink are
        // animations, and the boundaries fall where the size pattern restarts.
        // The frame counts are solid; WHICH effect each run is has not been
        // confirmed against the game.
        struct Sequence
        {
            int first;
            int count;
            const char* note;
        };

        inline constexpr std::array<Sequence, 6> PAGE0_SEQUENCES{ {
            // 40x36 -> 60x48 -> 38x28: swells then collapses. A small blast.
            { 0,  9, "small blast, grows to 60x48 then fades" },
            // 40x8 and 38x18 - two odd strips, not obviously part of a run.
            { 9,  2, "two strips, unclassified" },
            // 70x76 -> 76x88 -> 68x58: the second-largest effect here.
            { 11, 7, "large blast" },
            // 8x4 up to 10x7: tiny. Sparks, or the bullet impact flecks.
            { 18, 6, "tiny sparks" },
            // 12x12 -> 26x12: a widening smear.
            { 24, 6, "widening smear" },
            // 24x10 -> 35x35 -> 16x8: grows then cuts off.
            { 30, 6, "medium puff" },
        } };

        // Timing is NOT in this file. The original drives these through the same
        // animation VM everything else uses (Formats/SpriteAnimator.h), and the
        // sequence that does it lives in code data, not here. The duration used
        // when playing these is a guess until that sequence is found.
        inline constexpr uint16_t FRAME_DURATION_GUESS = 2;
    }
}
