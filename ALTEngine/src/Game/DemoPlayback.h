#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace ALTEngine::Game
{
    // THE ATTRACT DEMOS - GFX/DEMO111.BIN, DEMO211.BIN, DEMO311.BIN.
    //
    // Each file is 900 little-endian words: the player's input word (the one
    // FUN_0004d438 builds from the keys, DAT_000b0cc8), one a pass of the
    // player's task. The task takes the next word in place of the keys
    // (0x3eff4 -> FUN_00010928) and, past the 900th, reads 0xffff, which ends
    // the demo (cc0 | 8).
    //
    // HOW ONE STARTS. Left alone on the main menu for 0x6d6 frames (0x1376c),
    // the menu steps through a cycle of six (the words at 0x62c4): -1, level 0,
    // -1, level 0xc, -1, level 0x16. -1 is the title sequence again (the logos
    // and the intro film); a level number is that level's demo, played at the
    // lowest difficulty (0x1097e), with the default input bits (0x10998), and
    // the menu fades out first (0x13802). The level picks its file at 0x105ab:
    // level 0 -> DEMO111, 0xc -> DEMO211, 0x16 -> DEMO311. A key ends it.
    //
    // THE BITS (the defaults the demo installs, 0x4d40-0x4d58, and the player
    // task's tests):
    //   0x1000 forward          0x4000 back
    //   0x8000 left (turn)      0x2000 right (turn)
    //   0x0004 strafe left      0x0008 strafe right
    //   0x0080 fire (FUN_0003d5b0)   0x0020 second fire (FUN_0003d794)
    //   0x0010 use (0x3e3bb), or with 0x0001 held, look up (0x3f23c)
    //   0x0040 with 0x0001 held, look down
    // The three files use only forward, back, the two turns, use and fire.
    namespace DemoPlayback
    {
        inline constexpr int WORDS = 900;                 // 0x384
        inline constexpr uint16_t END = 0xffff;
        // Menu frames left alone before the attract cycle moves on (0x6d6),
        // at the original's 34.87 frames a second: about fifty seconds.
        inline constexpr int MENU_IDLE_FRAMES = 0x6d6;

        // The cycle at 0x62c4: -1 the title sequence, else the level's digits.
        inline constexpr std::array<const char*, 6> CYCLE = { nullptr, "111", nullptr, "211", nullptr, "311" };

        inline constexpr uint16_t FORWARD = 0x1000, BACK = 0x4000, LEFT = 0x8000, RIGHT = 0x2000;
        inline constexpr uint16_t STRAFE_LEFT = 0x0004, STRAFE_RIGHT = 0x0008;
        inline constexpr uint16_t FIRE = 0x0080, FIRE2 = 0x0020, USE = 0x0010, LOOK_MOD = 0x0001, LOOK_DOWN = 0x0040;

        // What one word holds, as the port's actions.
        struct Keys
        {
            bool forward = false, backward = false, turnLeft = false, turnRight = false;
            bool strafeLeft = false, strafeRight = false;
            bool fire = false, fire2 = false, use = false, lookUp = false, lookDown = false;
        };

        inline Keys Decode(uint16_t w)
        {
            Keys k;
            if (w == END) { return k; }
            k.forward = (w & FORWARD) != 0;
            k.backward = (w & BACK) != 0;
            k.turnLeft = (w & LEFT) != 0;
            k.turnRight = (w & RIGHT) != 0;
            k.strafeLeft = (w & STRAFE_LEFT) != 0;
            k.strafeRight = (w & STRAFE_RIGHT) != 0;
            k.fire = (w & FIRE) != 0;
            k.fire2 = (w & FIRE2) != 0;
            const bool look = (w & LOOK_MOD) != 0;
            k.lookUp = look && (w & USE) != 0;
            k.use = !look && (w & USE) != 0;
            k.lookDown = look && (w & LOOK_DOWN) != 0;
            return k;
        }

        // The words, or nothing if the file is missing or short.
        inline std::vector<uint16_t> Load(const std::filesystem::path& file)
        {
            std::ifstream in(file, std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (bytes.size() < static_cast<size_t>(WORDS) * 2) { return {}; }
            std::vector<uint16_t> words(WORDS);
            for (int i = 0; i < WORDS; ++i)
            {
                words[static_cast<size_t>(i)] = static_cast<uint16_t>(bytes[size_t(i) * 2] | (bytes[size_t(i) * 2 + 1] << 8));
            }
            return words;
        }

        // FUN_00010928: the next word, or END once all 900 are read.
        struct Reader
        {
            std::vector<uint16_t> words;
            int index = 0;   // DAT_000a4d38
            uint16_t Next()
            {
                if (index >= static_cast<int>(words.size()) || index >= WORDS) { return END; }
                return words[static_cast<size_t>(index++)];
            }
        };

        inline std::string FileName(const std::string& digits) { return "DEMO" + digits + ".BIN"; }
    }
}
