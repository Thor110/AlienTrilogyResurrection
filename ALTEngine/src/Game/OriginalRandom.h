#pragma once

#include <array>
#include <cstdint>

namespace ALTEngine::Game
{
    // THE ORIGINAL'S RANDOM NUMBERS - FUN_000100ec, its one generator.
    //
    // Six 32-bit words at 0x109b0-0x109c4 (s0..s5). A draw:
    //     t  = s4 + s1, plus 1 if that carried out
    //     s0 = t + 1 + s5
    //     s1 = s0, s2 = s1, ... s5 = s4   (copied upward one at a time, so
    //                                       all six end up as the new s0)
    //     returns s0, all 32 bits; every caller masks what it needs.
    //
    // Seeded from the six words at 0xa4dd0 (FUN_00010088 copies them in):
    // at every level start (0x1072d, FUN_00010694) and at 0x113be. The table
    // is never written, so a level always starts the same stream - which is
    // what lets the attract demos replay. Everything draws from it in task
    // order: the particles, the weapons, the creatures, the pickups, the
    // shatter, the lights and texture animations, the speech.
    //
    // The port keeps one shared stream (Shared()) and reseeds it where the
    // original does. The order of the draws is the original's only where the
    // port runs the same steps in the same order; see Notes/what-remains.md.
    namespace OriginalRandom
    {
        inline constexpr std::array<uint32_t, 6> SEED = {
            0xabcd0123u, 0x00000010u, 0xf0ad0000u, 0x000da0e0u, 0x00000001u, 0x1aa01001u,
        };

        struct State
        {
            std::array<uint32_t, 6> s = SEED;
            uint64_t draws = 0;   // how many since the last seed, for tracing

            void Reseed() { s = SEED; draws = 0; }

            uint32_t Next()
            {
                uint32_t t = s[4] + s[1];
                if (t < s[4]) { ++t; }
                s[0] = t + 1 + s[5];
                for (int i = 0; i < 5; ++i) { s[static_cast<size_t>(i) + 1] = s[static_cast<size_t>(i)]; }
                ++draws;
                return s[0];
            }
        };

        // The game's one stream.
        inline State& Shared()
        {
            static State state;
            return state;
        }

        inline uint32_t Next() { return Shared().Next(); }
        inline void Reseed() { Shared().Reseed(); }
    }
}
