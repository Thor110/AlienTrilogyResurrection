#pragma once

#include "SfxPlayer.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace ALTEngine::Audio
{
    // Ripley's voice: the remarks the original makes as things happen -
    // hurt, low on health, picking a weapon, the queen, a battery switch.
    //
    // THE BANK. SFX/SPCH.TXT, loaded after the level's own list (FUN_0005a674
    // from the pointer at 0x4de8, just before the per-level table), one line a
    // sample in the level lists' own format. Its samples are numbered on from
    // the level's (DAT_00402d98), so a speech id is a line in this file.
    //
    // THE CALL (FUN_000572e4): not in a network game (DAT_000ae0a4), not
    // without the bank, and not while the last line's cooldown (DAT_00402d9d)
    // runs; otherwise it plays line ID_TO_LINE[id] at volume 100 and starts
    // the cooldown at 0x8c. The bank needed spare memory in the original
    // (DAT_0040415c); the port always has it.
    //
    // THE TASK (FUN_00057338, every pass): counts the cooldown down; once it
    // is out, a second cooldown (DAT_00402d9c, FUN_00057298) does the same,
    // and once that too is out, while a creature holds the pounce lock (bit 0
    // of DAT_000b0ab5) there is a 4-in-8 chance of line 0x28, which restarts
    // that second cooldown at 0x8c.
    namespace Speech
    {
        // The ids the game calls, with what calls them.
        inline constexpr int QUEEN_HIT = 0x04;         // FUN_0002af24: queen hit, 1 in 8
        inline constexpr int HURT_RARE_A = 0x05;       // FUN_0003e618: hurt, health 16 on
        inline constexpr int WEAPON_FOUR_B = 0x06;     // FUN_0004d438: weapon key 4
        inline constexpr int HURT_LOW = 0x09;          // FUN_0003e618: hurt, health under 16
        inline constexpr int HURT = 0x0c;              // FUN_0003e618: hurt, health 16 on
        inline constexpr int WEAPON_THREE = 0x0d;      // FUN_0004d438: weapon key 3
        inline constexpr int BATTERY_REQUIRED = 0x0e;  // FUN_00036f4c: a switch without a battery
        inline constexpr int DYING = 0x17;             // FUN_0003e618: health gone
        inline constexpr int HUGGER_KILLED = 0x1e;     // FUN_000313b0: a face hugger killed
        inline constexpr int WEAPON_FOUR_A = 0x1f;     // FUN_0004d438: weapon key 4
        inline constexpr int QUEEN_WEAK = 0x20;        // FUN_000313b0: queen under 0x1a health
        inline constexpr int HURT_RARE_B = 0x24;       // FUN_0003e618: hurt, health 16 on
        inline constexpr int FLAMING_A = 0x26;         // FUN_0002c0a0: the flamethrower
        inline constexpr int FLAMING_B = 0x27;         // FUN_0002c0a0: the flamethrower
        inline constexpr int UNDER_ATTACK = 0x28;      // FUN_00057338: a creature pouncing
        inline constexpr int QUEEN_KILLED = 0x2a;      // FUN_00030b04: the queen dead

        inline constexpr int COOLDOWN = 0x8c;
        // FUN_0005b368's volume, out of the driver's 0..0x63 range: full.
        inline constexpr float VOLUME = 1.0f;

        // The id-to-line table at DAT_000ae0ac (16-bit entries): 0-19 straight
        // through, 24 to line 20, 29-42 to lines 21-34; the gaps are line 0.
        inline int LineForId(int id)
        {
            if (id >= 0 && id <= 19) { return id; }
            if (id == 24) { return 20; }
            if (id >= 29 && id <= 42) { return id - 8; }
            return 0;
        }

        // The sample names in a bank list: "X:cd\sfx\vc01c.raw 0" -> "vc01c".
        inline std::vector<std::string> ReadBankList(const std::filesystem::path& listFile)
        {
            std::vector<std::string> stems;
            std::ifstream in(listFile);
            std::string line;
            while (std::getline(in, line))
            {
                const size_t start = line.find_last_of("\\/:") == std::string::npos ? 0 : line.find_last_of("\\/:") + 1;
                size_t end = line.find_first_of(". \t\r", start);
                if (end == std::string::npos) { end = line.size(); }
                if (end <= start) { continue; }
                std::string stem = line.substr(start, end - start);
                for (char& c : stem) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
                stems.push_back(std::move(stem));
            }
            return stems;
        }

        class Voice
        {
        public:
            // Reads SFX/SPCH.TXT (any case). No file, no speech.
            void Load(const std::filesystem::path& cdDirectory)
            {
                cd = cdDirectory;
                lines.clear();
                std::error_code ec;
                const std::filesystem::path dir = cdDirectory / "SFX";
                for (const char* name : { "SPCH.TXT", "spch.txt", "Spch.txt" })
                {
                    if (std::filesystem::exists(dir / name, ec)) { lines = ReadBankList(dir / name); break; }
                }
                cooldown = 0;
                chatterCooldown = 0;
            }

            // FUN_000572e4. `networkGame` is the original's DAT_000ae0a4: any
            // multiplayer game, co-operative included.
            void Say(int id, bool networkGame)
            {
                if (networkGame || lines.empty() || cooldown != 0) { return; }
                const size_t line = static_cast<size_t>(LineForId(id));
                if (line >= lines.size()) { return; }
                SfxPlayer::PlayStem(lines[line], VOLUME, cd);
                cooldown = COOLDOWN;
            }

            // FUN_00057338, once a pass of the task list.
            void Tick(bool pounceLockHeld, bool networkGame)
            {
                if (networkGame || lines.empty()) { return; }
                if (cooldown != 0) { cooldown--; return; }
                if (chatterCooldown != 0) { chatterCooldown--; return; }
                if (pounceLockHeld && (Rand(8)) <= 3)
                {
                    chatterCooldown = COOLDOWN;
                    Say(UNDER_ATTACK, networkGame);
                }
            }

            // FUN_0003e618, after a hit that came off health (not armour).
            void OnHurt(int health, bool networkGame)
            {
                int id;
                if (health < 1)
                {
                    if (Rand(4) == 0) { return; }
                    id = DYING;
                }
                else if (health < 0x10) { id = HURT_LOW; }
                else if (Rand(16) < 2) { id = Rand(2) == 0 ? HURT_RARE_B : HURT_RARE_A; }
                else
                {
                    if (Rand(2) == 0) { return; }
                    id = HURT;
                }
                Say(id, networkGame);
            }

            bool Loaded() const { return !lines.empty(); }

        private:
            // The original's FUN_000100ec draws, masked to a power of two.
            int Rand(int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); }

            std::minstd_rand rng{ std::random_device{}() };
            std::filesystem::path cd;
            std::vector<std::string> lines;
            int cooldown = 0;          // DAT_00402d9d
            int chatterCooldown = 0;   // DAT_00402d9c
        };
    }
}
