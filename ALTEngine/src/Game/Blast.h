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
}
