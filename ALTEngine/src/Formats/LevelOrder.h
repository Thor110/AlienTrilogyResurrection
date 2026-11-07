#pragma once

#include <string>

namespace ALTEngine::Formats
{
    // The original's level numbers (DAT_000b0ca8), the index of every
    // per-level table: the level files, the sound lists, the music. TRACED
    // from the file-name table at 0xa4dec, which 0x1069b indexes by that number
    // (`mov eax,[0x10ca8]; mov ebx,[eax*4+0x4dec]`): the 35 campaign levels in
    // the order they are played, then the multiplayer levels. There is no
    // level 361.
    inline constexpr int ORIGINAL_LEVEL_COUNT = 45;
    inline constexpr const char* ORIGINAL_LEVEL_ORDER[ORIGINAL_LEVEL_COUNT] = {
        "111", "112", "113", "122", "131", "114", "141", "115", "154", "155",
        "161", "162", "211", "212", "213", "222", "242", "231", "232", "243",
        "262", "263", "311", "321", "331", "322", "351", "352", "323", "371",
        "353", "324", "381", "325", "391",
        "900", "901", "902", "903", "904", "905", "906", "907", "908", "909",
    };

    // A level's number from its code ("111"), or -1.
    inline int OriginalLevelNumber(const std::string& code)
    {
        for (int i = 0; i < ORIGINAL_LEVEL_COUNT; ++i)
        {
            if (code == ORIGINAL_LEVEL_ORDER[i]) { return i; }
        }
        return -1;
    }
}
