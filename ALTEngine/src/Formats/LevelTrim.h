#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Formats::LevelTrim
{
    // THE PADDING TRIM - ALTENGINE ONLY, the last step of the install's last
    // stage (Patch/README.txt, stage 5; Notes/unread-bytes-notes.txt says what
    // every byte is). ALTViewer's patch button never makes it: its files stay
    // ones the original game runs. ALTViewer reads trimmed levels, though.
    //
    // A level's MAP0 chunk holds bytes the game never reads, or reads as one
    // fixed value, or clears when the level starts. The trim takes them out:
    // the chunk becomes MAPT ("MAP0, trimmed") with shorter records, every
    // other chunk as it was. The tag is the marker - nothing else changes.
    //
    //   record        bytes   dropped
    //   header        36->32  +28,+29 (always 0x40 0x40); +34,+35 (vent types, never read)
    //   vertex         8->6   +6,+7 (always 0)
    //   face          20      -
    //   cell          16->13  +4, +7 (always 0, rebuilt at level start); +12 (occupancy, cleared at level start)
    //   path node      8->7   +2 (always 0)
    //   monster       20->19  +9 (never read)
    //   pickup         8->6   +5 (always 0); +7 (never read)
    //   object        16->12  +5 (10 on crate types 20, 25, 29, 32, else 0); +11, +13, +15 (always 0)
    //   door           8->7   +5 (always 0)
    //   lift          16->10  +4, +11 (always 0); +6 (always 1); +13..15 (never read)
    //   action group   4->3   +3 (always 0)
    //   logic group    4      -
    //   light         28->24  +3, +7, +11, +15 (the pad byte after each colour, always 0)
    //   texture animators, animation frames: 4 each, unchanged
    //
    // On the 45 shipped levels that is 1,930,104 bytes, about 10% of them.
    //
    // Expand puts every dropped byte back: the fixed ones with their value,
    // the never-read and level-start ones as 0. So Expand(Trim(x)) is x
    // exactly but for those last (Normalized(x) is x with them at 0), and the
    // game reads the same level from either. Trim refuses a level whose
    // "fixed" bytes are not what they always are, or whose MAP0 does not end
    // where its counts say: such a file is left as it is.
    //
    // Every offset a tool or a patch names is in the untrimmed layout, so the
    // trim runs truly last, and the install keeps the untrimmed file beside
    // the trimmed one (<file>.padded; Testing > Padding Trim off restores it).
    inline constexpr const char* TRIMMED_TAG = "MAPT";
    inline constexpr const char* UNTRIMMED_TAG = "MAP0";

    // True when the file's level chunk is MAPT.
    bool IsTrimmed(const std::vector<uint8_t>& file);

    // A level file (MAP0) to its trimmed form (MAPT).
    bool Trim(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error);

    // A trimmed level file (MAPT) back to MAP0. A file that is not trimmed is
    // copied as it is.
    bool Expand(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error);

    // The level as Expand(Trim(x)) gives it back: the never-read and
    // level-start bytes at 0.
    bool Normalized(const std::vector<uint8_t>& file, std::vector<uint8_t>& out, std::string& error);
}
