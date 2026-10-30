#pragma once

#include "Screens/CreatureAI.h"

#include <cstdint>

namespace AltTest
{
    // Open floor, every line clear, no path network: what the creature tasks
    // see on a flat, empty level. One world for every test; its random numbers
    // start again from the same seed each time it is asked for, so a test does
    // not depend on the ones run before it.
    inline ALTEngine::Screens::CreatureAI::World& OpenWorld()
    {
        static uint32_t seed = 3;
        static ALTEngine::Screens::CreatureAI::World world = [] {
            ALTEngine::Screens::CreatureAI::World w;
            w.cellOpenOrPlayer = [](int, int) { return true; };
            w.cellOpen = [](int, int) { return true; };
            w.lineTo = [](int, int, int) { return true; };
            w.nodeAt = [](int, int) { return -1; };
            w.nodeToward = [](int) { return -1; };
            w.nodeAway = [](int) { return -1; };
            w.nodePosition = [](int, int&, int&) { return false; };
            w.random = [] { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };
            return w;
        }();
        seed = 3;
        return world;
    }
}
