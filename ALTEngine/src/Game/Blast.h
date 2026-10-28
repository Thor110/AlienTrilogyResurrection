#pragma once

#include "../Formats/LevelLoader.h"

#include <vector>

namespace ALTEngine::Game::Blast
{
    // The cells a barrel blast centred on `originCell` reaches, gated the way
    // FUN_000368c8 gates it: two rings out from the origin, a cell is reached
    // only if its attribute is not solid and its floor is within a step of the
    // origin's, and each outer-ring cell is culled with the inner-ring cell it
    // sits behind. Ring tables and limits are in Screens/DamageSystem.h.
    // The origin itself is not included.
    std::vector<int> ReachedCells(const Formats::LevelGeometry& level, int originCell);

    // The same cells split by ring, because what the blast does to whatever is
    // standing in them depends on which ring it is (FUN_000368c8): the player
    // takes 0x32 in the inner ring and 0x19 in the outer, creatures other than
    // type 7 are killed outright in either, and objects not flagged
    // indestructible are destroyed.
    struct Rings
    {
        std::vector<int> inner;
        std::vector<int> outer;
    };
    Rings ReachedRings(const Formats::LevelGeometry& level, int originCell);

    inline constexpr int PLAYER_DAMAGE_INNER = 0x32;
    inline constexpr int PLAYER_DAMAGE_OUTER = 0x19;
    inline constexpr int DAMAGE_CAUSE = 5;
    inline constexpr int IMMUNE_CREATURE_TYPE = 7;
}
