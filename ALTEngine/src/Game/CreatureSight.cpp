#include "CreatureSight.h"

#include "OriginalMath.h"
#include "ProjectileCollision.h"

#include <cmath>

namespace ALTEngine::Game::CreatureSight
{
    bool LineOfFire(const Formats::LevelGeometry& level, const Query& q)
    {
        if (q.distance > MAX_DISTANCE) { return false; }

        // Integer positions, as the original keeps them, and its own bearing
        // and rotation, so a probe lands on the same side of a cell edge.
        const int cx = static_cast<int>(std::lround(q.x));
        const int cz = static_cast<int>(std::lround(q.z));
        const int bearing = OriginalMath::Bearing(cx - static_cast<int>(std::lround(q.targetX)),
                                                  cz - static_cast<int>(std::lround(q.targetZ)));
        int stepX, stepZ, sideX, sideZ, otherX, otherZ;
        OriginalMath::RotateYaw(static_cast<int>(PROBE_STEP), bearing + 0x800, stepX, stepZ);
        // ORDER MATTERS: the running height carries from one probe to the
        // next, and the original walks the 0x400 side first.
        OriginalMath::RotateYaw(static_cast<int>(PROBE_SIDE), bearing + 0x400, sideX, sideZ);
        OriginalMath::RotateYaw(static_cast<int>(PROBE_SIDE), bearing + 0xc00, otherX, otherZ);
        int ax = cx + sideX, az = cz + sideZ;
        int bx = cx + otherX, bz = cz + otherZ;

        const int rise = q.ceilingCrawler ? 0x200 : 0x100;
        const int drop = q.ceilingCrawler ? 0x800 : 0x400;
        int y = static_cast<int>(std::lround(q.y));

        int steps = q.distance >> 8;
        if (steps < 2) { steps = 2; }

        auto probe = [&](int gx, int gz) -> bool {
            if (gx < 0 || gz < 0) { return false; }
            const int cellX = gx >> 9, cellZ = gz >> 9;
            const auto* cell = Formats::CellAt(level, cellX, cellZ);
            if (!cell) { return false; }
            if (cell->attribute > 0x13 && cell->attribute < 0x2d) { return false; }
            // The creature lifts its own cell's stamp before probing - which,
            // when it shares a cell with the player, lifts the player's too.
            if (q.occupancy)
            {
                const int index = Formats::CellIndex(level, cellX, cellZ);
                if (index >= 0 && static_cast<size_t>(index) < q.occupancy->size()
                    && (*q.occupancy)[static_cast<size_t>(index)] != 0) { return false; }
            }
            else if (cellX == q.occupiedCellX && cellZ == q.occupiedCellZ
                     && !(cellX == (cx >> 9) && cellZ == (cz >> 9))) { return false; }

            const int ceiling = static_cast<int>(cell->ceilingHeight) * Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
            const int floor = static_cast<int>(Formats::FindFloorHeightGridSpace(level, gx, gz));
            if (cell->attribute < 0x2d)
            {
                int surface = ceiling;
                if (!q.ceilingCrawler)
                {
                    if (ceiling < q.halfHeight + y) { return false; }
                    if (q.tallType && ceiling < y + 0xc00) { return false; }
                    surface = floor;
                }
                if (y < surface && surface - y > rise) { return false; }
                if (surface < y && y - surface > drop) { return false; }
                y = surface;
            }
            else
            {
                y = floor;
                if (q.tallType && ceiling < y + 0xc00) { return false; }
            }
            return true;
        };

        for (int i = 0; i < steps; ++i)
        {
            if (!probe(ax, az)) { return false; }
            if (!probe(bx, bz)) { return false; }
            ax += stepX; az += stepZ;
            bx += stepX; bz += stepZ;
        }
        return true;
    }

    bool ZoneVisible(const Formats::LevelGeometry& level, int creatureCell, int playerCell)
    {
        return ProjectileCollision::ShareZone(level, creatureCell, playerCell);
    }
}
