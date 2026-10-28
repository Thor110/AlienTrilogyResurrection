#include "Blast.h"

#include "../Screens/DamageSystem.h"

namespace ALTEngine::Game::Blast
{
    std::vector<int> ReachedCells(const Formats::LevelGeometry& level, int originCell)
    {
        Rings rings = ReachedRings(level, originCell);
        rings.inner.insert(rings.inner.end(), rings.outer.begin(), rings.outer.end());
        return rings.inner;
    }

    Rings ReachedRings(const Formats::LevelGeometry& level, int originCell)
    {
        namespace D = ALTEngine::Screens::Damage;
        Rings rings;
        if (originCell < 0 || originCell >= static_cast<int>(level.collisionGrid.size())) { return rings; }

        const int width = level.header.mapLength;
        const int ox = originCell % width;
        const int oz = originCell / width;
        const auto& origin = level.collisionGrid[static_cast<size_t>(originCell)];

        auto reachable = [&](int dx, int dz) -> int {
            const int index = Formats::CellIndex(level, ox + dx, oz + dz);
            if (index < 0) { return -1; }
            const auto& cell = level.collisionGrid[static_cast<size_t>(index)];
            if (cell.attribute > D::BLAST_MAX_ATTRIBUTE) { return -1; }
            const int step = static_cast<int>(cell.floorHeight) - static_cast<int>(origin.floorHeight);
            if (step > D::BLAST_MAX_HEIGHT_STEP || step < -D::BLAST_MAX_HEIGHT_STEP) { return -1; }
            return index;
        };

        bool ringOneOpen[8] = {};
        for (int i = 0; i < 8; ++i)
        {
            const int cell = reachable(D::BLAST_RING1[i].dx, D::BLAST_RING1[i].dz);
            if (cell >= 0) { ringOneOpen[i] = true; rings.inner.push_back(cell); }
        }
        for (const auto& outer : D::BLAST_RING2)
        {
            if (!ringOneOpen[outer.parent]) { continue; }
            const int cell = reachable(outer.offset.dx, outer.offset.dz);
            if (cell >= 0) { rings.outer.push_back(cell); }
        }
        return rings;
    }
}
