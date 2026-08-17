#pragma once

#include <cstdint>

namespace ALTEngine::Formats
{
    // Enemy frames and animations. FOUND, not guessed.
    //
    // FUN_0002e638 assigns each spawning entity an animation table by writing a
    // fixed base into entity +0x7c, one per creature, from a switch on its type.
    // Fifteen sites, matching the fifteen NME files:
    //
    //     0x000ac3a0  0x000ac3d0  0x000ac460  0x000ac4f0  0x000ac548
    //     0x000ac5c8  0x000ac640  0x000ac6d8  0x000ac768  0x000ac7d8
    //     0x000ac848  0x000ac8e8  0x000ac958  0x000ac9e8
    //
    // (0x000ac848 is used twice - two creatures share one animation set.)
    //
    // EACH TABLE IS PAIRS OF POINTERS: {frame table, sequence}, exactly like the
    // weapons at 0x000ace28. The gaps between consecutive bases give the count:
    // the first creature has 0x30 bytes = 6 animations, the next 0x90 = 18.
    // Animation sets are not a fixed size.
    //
    // A FRAME RECORD IS THE SAME 12 BYTES AS A WEAPON'S:
    //     int16 offsetX, int16 offsetY, int16 width, int16 height, int32 dataOffset
    //
    // Verified side by side - the first creature's resting frames read
    //     (-20,-47) 48x60  data 0
    //     (-20,-47) 48x60  data 1884
    //     (-20,-49) 48x60  data 3752
    //     (-20,-50) 48x64  data 5728
    //     (-20,-49) 48x60  data 7732
    // against the pistol's idle frame (-7,-64) 40x68 data 0. Identical layout,
    // and the dataOffset climbs by roughly width*height per frame, so it is a
    // byte offset into that section's decoded pixels.
    //
    // SEQUENCES ARE THE SAME VM. {duration, frame count, program}, opcodes with
    // bit 0x8000 set. The first creature's six:
    //
    //   0  dur 4  8 frames  SET_FLAG1, 0..7, END_ALT
    //   1  dur 4  5 frames  EVENT(0x2f), SET_FLAG1, 0 1 2 3, SET_FLAG1, 4, END_ALT
    //   2  dur 4  8 frames  EVENT(0x2d), SET_FLAG1, 0..7, END_ALT
    //   3  dur 4  3 frames  EVENT(0x2e), SET_FLAG1, 0 1 2, END
    //   4  dur 4  3 frames  EVENT(0x2e), SET_FLAG1, 0 1 2, END
    //   5  dur 4  6 frames  SET_FLAG1, 0..5, END_ALT
    //
    // and later ones run EVENT(0x33) then 0..3 on OP_LOOP(0) - a held, looping
    // cycle with a sound every pass, which is what a walk looks like.
    //
    // THE EVENT OPERANDS ARE SOUND SLOTS, and they land in the enemy block:
    // 0x30-0x35 on level 1 are 1601lnch, 1602flor, 1603face, 1604skit, 1605dies,
    // 1606inja. So 0x33 is the skitter, which is why the looping cycles carry it.
    // 0x2d, 0x2e and 0x2f sit just below that block and are blank on level 1,
    // which means this particular creature's set is not one L111 uses - a level
    // only fills the slots for the creatures it ships.
    //
    // MARKED: which animation index is which pose (idle, walk, attack, death) is
    // not traced. The shapes are suggestive - a 3-frame one-shot ending on
    // OP_END reads as a death, a 4-frame OP_LOOP(0) as a walk - but that is
    // inference, not a read.
    struct EnemyFrameRecord
    {
        int16_t offsetX;
        int16_t offsetY;
        int16_t width;
        int16_t height;
        int32_t dataOffset;
    };
    static_assert(sizeof(EnemyFrameRecord) == 12, "must match the original's 12-byte record");

    // The animation table bases, in the order FUN_0002e638's switch reaches
    // them. The mapping onto the NME file list is NOT yet confirmed - the switch
    // cases would settle it.
    inline constexpr uint32_t ENEMY_ANIM_BASES[] = {
        0x000ac3a0, 0x000ac3d0, 0x000ac640, 0x000ac6d8, 0x000ac8e8,
        0x000ac548, 0x000ac4f0, 0x000ac460, 0x000ac958, 0x000ac9e8,
        0x000ac7d8, 0x000ac768, 0x000ac848, 0x000ac848, 0x000ac5c8,
    };
    inline constexpr int ENEMY_ANIM_BASE_COUNT = 15;
}
