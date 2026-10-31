#include "CreatureSight.h"

#include "OriginalMath.h"
#include "ProjectileCollision.h"
#include "ViewRings.h"

#include <algorithm>
#include <cmath>

namespace ALTEngine::Game::CreatureSight
{
    bool LineOfFire3D(const Formats::LevelGeometry& level, const Query3D& q)
    {
        if (q.distance > MAX_DISTANCE_3D) { return false; }

        int x = static_cast<int>(std::lround(q.x));
        int z = static_cast<int>(std::lround(q.z));
        int y = static_cast<int>(std::lround(q.y)) + RAY_START_HEIGHT;
        const int eye = static_cast<int>(std::lround(q.targetFloorY)) + RAY_TARGET_HEIGHT;
        const int across = OriginalMath::Distance(static_cast<int>(std::lround(q.targetX)) - x,
                                                  static_cast<int>(std::lround(q.targetZ)) - z);

        // The step: 0x100 along the pitch toward the eye, then turned onto the
        // heading. (The original turns (0, 0, 0x100) by the pitch from its own
        // angle routine, then by the heading; the same vector to within a
        // unit.)
        const double pitch = std::atan2(static_cast<double>(eye - y), static_cast<double>(across));
        const int forward = static_cast<int>(std::lround(PROBE_STEP * std::cos(pitch)));
        const int stepY = static_cast<int>(std::lround(PROBE_STEP * std::sin(pitch)));
        int stepX, stepZ;
        OriginalMath::RotateYaw(forward, q.heading + 0x800, stepX, stepZ);

        int steps = q.distance >> 8;
        if (steps < 2) { steps = 2; }
        for (int i = 0; i < steps; ++i)
        {
            if (x < 0 || z < 0) { return false; }
            const int cellX = x >> 9, cellZ = z >> 9;
            const auto* cell = Formats::CellAt(level, cellX, cellZ);
            if (!cell) { return false; }
            if (cell->attribute > 0x13 && cell->attribute < 0x2d) { return false; }

            int floor = static_cast<int>(Formats::FindFloorHeightGridSpace(level, x, z));
            const int index = Formats::CellIndex(level, cellX, cellZ);
            uint8_t occupant = cell->unknown13;
            if (q.occupancy && index >= 0 && static_cast<size_t>(index) < q.occupancy->size()
                && (*q.occupancy)[static_cast<size_t>(index)] != 0)
            {
                occupant = (*q.occupancy)[static_cast<size_t>(index)];
            }
            if (occupant != 0)
            {
                if (occupant == 0x14 || occupant == 0x17) { floor += 0x200; }
                else if (occupant == 0x19) { floor += 0x400; }
                else { return false; }
            }
            if (y < floor) { return false; }
            const int ceiling = static_cast<int>(cell->ceilingHeight) * Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
            if (ceiling < y) { return false; }

            x += stepX; y += stepY; z += stepZ;
        }
        return true;
    }

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

    namespace
    {
        // The view walk (0x2745b): calls `take(cellIndex)` for every cell it
        // takes in and returns the zone mask it builds.
        template <typename Take>
        uint16_t WalkView(const Formats::LevelGeometry& level, int playerCellX, int playerCellZ, int viewYaw, Take&& take,
                          int minHalfWidth = 0)
        {
            const int length = level.header.mapLength;
            const int width = level.header.mapWidth;
            if (playerCellX < 0 || playerCellZ < 0 || playerCellX >= length || playerCellZ >= width) { return 0; }
            if (level.collisionGrid.size() < static_cast<size_t>(length) * static_cast<size_t>(width)) { return 0; }
            auto index = [&](int x, int z) { return z * length + x; };
            auto at = [&](int x, int z) -> const Formats::CollisionNode& {
                return level.collisionGrid[static_cast<size_t>(index(x, z))];
            };
            auto zones = [](const Formats::CollisionNode& c) { return static_cast<uint16_t>(c.ceilingFog | (c.floorFog << 8)); };
            // Bytes 0-3 are the cell's first face, 0xffffffff for none (the
            // runtime turns it into the list the renderer draws).
            auto hasFaces = [](const Formats::CollisionNode& c) {
                return (c.unknown1 & c.unknown2 & c.unknown3 & c.unknown4) != 0xff;
            };

            uint16_t mask = zones(at(playerCellX, playerCellZ));
            take(index(playerCellX, playerCellZ));
            struct Pass { int firstRing, lastRing, halfWidth; };
            static constexpr Pass PASSES[] = { { 0, 0, 0x600 }, { 1, 1, 0x384 }, { 2, 7, 0x280 }, { 8, 14, 0x1b8 } };
            const int yaw = viewYaw & 0xfff;
            for (const Pass& pass : PASSES)
            {
                for (int ring = pass.firstRing; ring <= pass.lastRing; ++ring)
                {
                    // The port's wider screens (minHalfWidth > 0) test each
                    // ring's cells by their centres, so the cone also widens
                    // by how wide a cell looks from anywhere in the player's
                    // cell (1.5 cells over the ring's distance); the first
                    // two rings are taken whole.
                    int halfWidth = pass.halfWidth;
                    if (minHalfWidth > 0)
                    {
                        const int margin = ring <= 1 ? 0x800
                            : static_cast<int>(std::asin(std::min(1.0, 1.5 / ring)) * 4096.0 / 6.283185307179586);
                        halfWidth = std::max(halfWidth, minHalfWidth + margin);
                    }
                    halfWidth = std::min(halfWidth, 0x800);
                    int lo = yaw - halfWidth;
                    int hi = yaw + halfWidth;
                    if (lo < 0) { lo += 0x1000; hi += 0x1000; }
                    int i = ViewRings::RING_BUCKETS[ring][lo >> 7];
                    while (ViewRings::RING_CELLS[i].angle < lo) { ++i; }
                    for (; i < ViewRings::CELL_COUNT && ViewRings::RING_CELLS[i].angle <= hi; ++i)
                    {
                        const int x = playerCellX - ViewRings::RING_CELLS[i].dx;
                        const int z = playerCellZ + ViewRings::RING_CELLS[i].dz;
                        if (x < 0 || z < 0 || x >= length || z >= width) { continue; }
                        const auto& cell = at(x, z);
                        const uint16_t cellZones = zones(cell);
                        if ((cellZones & mask) == 0) { continue; }
                        take(index(x, z));
                        if (hasFaces(cell)) { mask = static_cast<uint16_t>(mask | cellZones); }
                    }
                }
            }
            return mask;
        }
    }

    uint16_t DrawnZoneMask(const Formats::LevelGeometry& level, int playerCellX, int playerCellZ, int viewYaw)
    {
        return WalkView(level, playerCellX, playerCellZ, viewYaw, [](int) {});
    }

    std::vector<uint8_t> DrawnCells(const Formats::LevelGeometry& level, int playerCellX, int playerCellZ,
                                    int viewYaw, int minHalfWidth, uint16_t* zoneMask)
    {
        const size_t cells = static_cast<size_t>(level.header.mapLength) * static_cast<size_t>(level.header.mapWidth);
        std::vector<uint8_t> drawn(cells, 0);
        const uint16_t mask = WalkView(level, playerCellX, playerCellZ, viewYaw, [&](int i) {
            if (i >= 0 && static_cast<size_t>(i) < cells) { drawn[static_cast<size_t>(i)] = 1; }
        }, minHalfWidth);
        if (zoneMask) { *zoneMask = mask; }
        return drawn;
    }

    std::vector<int32_t> FaceCells(const Formats::LevelGeometry& level)
    {
        std::vector<int32_t> owner(level.quads.size(), -1);
        for (size_t cell = 0; cell < level.collisionGrid.size(); ++cell)
        {
            const auto& n = level.collisionGrid[cell];
            const uint32_t start = static_cast<uint32_t>(n.unknown1) | (static_cast<uint32_t>(n.unknown2) << 8)
                                 | (static_cast<uint32_t>(n.unknown3) << 16) | (static_cast<uint32_t>(n.unknown4) << 24);
            if (start == 0xffffffffu || start == 0) { continue; }
            for (size_t q = start - 1; q < level.quads.size(); ++q)
            {
                owner[q] = static_cast<int32_t>(cell);
                if (level.quads[q].reserved & 0x80) { break; }
            }
        }
        return owner;
    }

    void MarkSeen(const Formats::LevelGeometry& level, std::vector<uint8_t>& seen, std::vector<uint8_t>& areas,
                  int playerCellX, int playerCellZ, int viewYaw)
    {
        const size_t cells = static_cast<size_t>(level.header.mapLength) * static_cast<size_t>(level.header.mapWidth);
        if (seen.size() != cells) { seen.assign(cells, 0); }
        if (areas.size() != 256) { areas.assign(256, 0); areas[0] = 1; }
        WalkView(level, playerCellX, playerCellZ, viewYaw, [&](int i) {
            if (i >= 0 && static_cast<size_t>(i) < cells) { seen[static_cast<size_t>(i)] = 1; }
        });
        const int playerIndex = playerCellZ * level.header.mapLength + playerCellX;
        if (playerCellX >= 0 && playerCellZ >= 0 && playerCellX < level.header.mapLength
            && playerCellZ < level.header.mapWidth && static_cast<size_t>(playerIndex) < level.collisionGrid.size())
        {
            areas[level.collisionGrid[static_cast<size_t>(playerIndex)].unknown6] = 1;
        }
    }

    std::vector<uint8_t> MapReveal(const Formats::LevelGeometry& level, const std::vector<uint8_t>& seen,
                                   const std::vector<uint8_t>& areas, bool autoMapper)
    {
        const size_t cells = static_cast<size_t>(level.header.mapLength) * static_cast<size_t>(level.header.mapWidth);
        std::vector<uint8_t> reveal(cells, 0);
        for (size_t i = 0; i < cells && i < level.collisionGrid.size(); ++i)
        {
            const uint8_t area = level.collisionGrid[i].unknown6;
            const bool discovered = area == 0 || (area < areas.size() && areas[area] != 0);
            const bool wasSeen = i < seen.size() && seen[i] != 0;
            reveal[i] = (discovered && (autoMapper || wasSeen)) ? 1 : 0;
        }
        return reveal;
    }

    bool ZoneVisible(const Formats::LevelGeometry& level, int creatureCell, uint16_t drawnZones)
    {
        return (ProjectileCollision::CellZones(level, creatureCell) & drawnZones) != 0;
    }

    bool CellHasNoFace(const Formats::LevelGeometry& level, int cell)
    {
        if (cell < 0 || static_cast<size_t>(cell) >= level.collisionGrid.size()) { return false; }
        return (level.collisionGrid[static_cast<size_t>(cell)].unknown4 & 0x80) != 0;
    }
}
