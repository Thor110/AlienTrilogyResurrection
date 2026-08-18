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
    // THE FIELD AT +8 IS A LOOKUP KEY, NOT A DATA OFFSET. This is the piece that
    // was missing, and it explains why picking frames by index gave the wrong
    // sprite.
    //
    // FUN_0002ff30 takes an entity and searches a table for the frame it should
    // show:
    //
    //     table = (subtype 1..3) ? DAT_000a4dcc : DAT_000a4d90
    //     count = (subtype 1..3) ? DAT_000a4dca : DAT_000a4d8e
    //     walk entries of 12 bytes until  entry[+8] == frameRecord[+8]
    //
    // So the frame record's third dword is a KEY that identifies a sprite, and
    // the table maps it to whatever slot that sprite was loaded into. The matched
    // entry then supplies:
    //     entry[+4]  a byte added into the drawn height
    //     entry[+2]  a byte written to entity +0x81 and +0x85 - the texture page
    //     entry[+0]  a short written to entity +0x86
    //
    // FUN_0002ffd8 is the fast path for the next frame: it checks whether the
    // entity's cached entry (+0xa4) still matches the new frame's key and only
    // re-searches when it does not.
    //
    // TWO TABLES, chosen by the entity's SUBTYPE at +0x74 - one for subtypes 1
    // to 3, another for everything else. That is worth knowing before wiring it:
    // the same key can mean different sprites for different creature classes.
    //
    // WHAT THIS MEANS FOR THE PORT. Frames cannot be addressed as "section N,
    // frame M" the way the weapons are. The animation's frame table names sprites
    // by key, and the key has to be resolved against a table built when the NME
    // file is loaded. Until that table's construction is traced, the port cannot
    // pick the right frame at all - which is why the resting pose came out as a
    // damage frame (Edward, 2026). Loading frame 0 of section 0 was never going
    // to be right; the first frame in the file simply is not the idle pose.
    struct EnemyFrameRecord
    {
        int16_t offsetX;
        int16_t offsetY;
        int16_t width;
        int16_t height;
        int32_t spriteKey;   // NOT an offset - see above
    };
    static_assert(sizeof(EnemyFrameRecord) == 12, "must match the original's 12-byte record");

    // WHAT THOSE TABLES ACTUALLY ARE: A SPRITE CACHE, not a key-to-frame map.
    //
    // Following the chain all the way settles it:
    //
    //   FUN_0002fe74   finds a FREE slot - one whose byte at +7 is zero - claims
    //                  it, calls FUN_000285b4 to decode the sprite into it, then
    //                  stamps the frame's identifier into the slot:
    //                      slot[+8] = frameRecord[+8]
    //   FUN_0002ff30   searches for a slot whose +8 ALREADY equals the wanted
    //                  frame's identifier - a cache hit, no decode needed
    //   FUN_0002ffd8   the fast path: is the entity's current slot still the
    //                  right one?
    //   FUN_000285b4   the decode itself, and it reads the sprite's source
    //                  address straight from frameRecord[+8]:
    //                      FUN_00028350(*(uint32*)(entity[+0x10] + 8), dest)
    //                  where entity+0x10 is the resolved frame record.
    //
    // So frameRecord[+8] IS the sprite's location - my first reading of it as a
    // data offset was right, and my second reading of it as an opaque key was
    // wrong. It is used as a cache tag as well, which is what made it look like a
    // key: the same number serves both as "where to read this sprite from" and as
    // "which sprite is in this slot".
    //
    // The counts are static in the image: 12 slots for ordinary entities and 8
    // for subtypes 1-3. The original only keeps that many decoded sprites resident
    // at once and swaps them as animations play. A port with memory to spare does
    // not need the cache at all - it can decode every frame up front - so THE
    // TABLES DO NOT NEED PORTING. What was needed was the knowledge that +8
    // addresses the sprite data, which is now established.
    //
    // WHAT REMAINS for correct frames: which animation index a resting enemy
    // should be playing. The frame within an animation is now addressable, but
    // FUN_0002e638 does not start one - it leaves the state byte at 0 - so
    // something else picks the first animation, and until that is found the port
    // has no principled starting frame. Loading "the first frame in the file" is
    // what produced the damage pose (Edward, 2026), and it is wrong for the same
    // reason picking frame 0 of section 0 was: the file's order is not the
    // animation's order.
    //
    // TWO DRAW PATHS, from FUN_000300a4, the draw dispatcher:
    //
    //     FUN_0002f53c   the sprite draw traced above - the near path
    //     FUN_0002fb84   a DIFFERENT draw, taken when:
    //                      - the state byte +0x6f is 8, or
    //                      - the subtype is 1, 8, 9 or 10, or
    //                      - the distance at +0xac is over 0x1000, or
    //                      - for subtype 7, over 0x1800 or a condition on +0xc0
    //
    // So distant enemies and several whole subtypes never go through the sprite
    // path at all. Subtype 1 is the egg, and 8/9/10 are the ceiling warrior,
    // ceiling dog and colonist - which is a suggestive set, and worth following
    // when the ceiling creatures are looked at, since they are drawn by a route
    // that has not been read.
    //
    // WHAT REMAINS for a correct resting frame: which animation index to start.
    // FUN_0002e638 leaves the state byte at 0 and starts no animation, so
    // something else picks the first one. That is the last link.

    // The sprite-slot tables the key resolves against.
    inline constexpr uint32_t SPRITE_TABLE_SUBTYPE_1_3 = 0x000a4dcc;
    inline constexpr uint32_t SPRITE_COUNT_SUBTYPE_1_3 = 0x000a4dca;
    inline constexpr uint32_t SPRITE_TABLE_DEFAULT = 0x000a4d90;
    inline constexpr uint32_t SPRITE_COUNT_DEFAULT = 0x000a4d8e;
    inline constexpr int SPRITE_TABLE_STRIDE = 12;

    // MIRRORING IS BIT 0x20 OF ENTITY +0xb6, found in the draw itself
    // (FUN_0002f53c): with it clear the quad's X runs from the record's offset
    // outward, and with it set both X terms are negated - a horizontal flip.
    //
    // That confirms the mirrored half of the eight-view scheme. FUN_0002f4b0
    // clears only the low nibble of +0xb6 when starting an animation, so the flag
    // survives an animation change, and FUN_00033cbc's subtype 7 arm sets it
    // explicitly.
    inline constexpr int ENTITY_MIRROR_FLAG = 0x20;
    inline constexpr int ENTITY_FLAGS_OFFSET = 0xb6;

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
