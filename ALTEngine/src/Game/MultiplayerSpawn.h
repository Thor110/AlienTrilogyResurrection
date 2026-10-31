#pragma once

#include <functional>

namespace ALTEngine::Game::MultiplayerSpawn
{
    // WHERE A PLAYER STARTS IN A MULTIPLAYER GAME - the original's spawn at
    // 0x402fc, with the node picked at 0x108b6 (level start) and 0x1034e
    // (after a death).
    //
    // Not the level's start (that is single player only), and not the four
    // Synthetic records either: those are parked outside the map, the bodies
    // the other players are drawn with. A player goes to a PATH NODE picked
    // at random: (random + seed) & 0x7f at the level's start, & 0x3f after a
    // death, picked again until it is below the level's node count. On the
    // tenth multiplayer map (the original's level 0x2c, L909) node 0 is
    // taken as node 1. The player stands at the node's own cell corner
    // (node x << 9, node z << 9 in grid space), with 100 health.
    //
    // `random` returns the original's FUN_000100ec-like value; anything
    // uniform will do.
    inline int PickNode(int nodeCount, bool levelStart, bool isL909, const std::function<int()>& random)
    {
        if (nodeCount <= 0) { return -1; }
        const int mask = levelStart ? 0x7f : 0x3f;
        // A level with fewer nodes than the mask's lowest value would never
        // end; the original's have 33 to 94 nodes. Guard anyway.
        int node = 0;
        for (int tries = 0; tries < 4096; ++tries)
        {
            node = random() & mask;
            if (node < nodeCount) { break; }
            node = -1;
        }
        if (node < 0) { node = 0; }
        if (node == 0 && isL909 && nodeCount > 1) { node = 1; }
        return node;
    }
}
