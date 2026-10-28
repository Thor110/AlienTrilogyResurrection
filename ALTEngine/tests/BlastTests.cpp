#include "TestFramework.h"

#include "Game/Blast.h"
#include "Screens/DamageSystem.h"

#include <algorithm>

using namespace ALTEngine::Formats;
using namespace ALTEngine::Game;

namespace
{
    LevelGeometry OpenFloor(int size)
    {
        LevelGeometry level;
        level.header.mapLength = static_cast<uint16_t>(size);
        level.header.mapWidth = static_cast<uint16_t>(size);
        level.collisionGrid.resize(static_cast<size_t>(size * size));
        return level;
    }

    bool Contains(const std::vector<int>& cells, int cell)
    {
        return std::find(cells.begin(), cells.end(), cell) != cells.end();
    }
}

TEST_CASE("Blast: on open floor it reaches both rings, 24 cells")
{
    LevelGeometry level = OpenFloor(9);
    const auto cells = Blast::ReachedCells(level, 4 * 9 + 4);
    CHECK_EQ(cells.size(), size_t(24));
    CHECK(!Contains(cells, 4 * 9 + 4));
}

TEST_CASE("Blast: a wall stops the blast and shadows the cells behind it")
{
    LevelGeometry level = OpenFloor(9);
    const int origin = 4 * 9 + 4;
    const int north = 3 * 9 + 4;   // ring one, straight up
    level.collisionGrid[static_cast<size_t>(north)].attribute = ALTEngine::Screens::Damage::BLAST_MAX_ATTRIBUTE + 1;
    const auto cells = Blast::ReachedCells(level, origin);
    CHECK(!Contains(cells, north));
    CHECK(!Contains(cells, 2 * 9 + 4));   // the ring-two cell behind it
    CHECK(Contains(cells, 2 * 9 + 2));    // a corner cell, behind an open corner
}

TEST_CASE("Blast: a big height step blocks the blast")
{
    LevelGeometry level = OpenFloor(9);
    const int east = 4 * 9 + 5;
    level.collisionGrid[static_cast<size_t>(east)].floorHeight = ALTEngine::Screens::Damage::BLAST_MAX_HEIGHT_STEP + 1;
    CHECK(!Contains(Blast::ReachedCells(level, 4 * 9 + 4), east));
}

TEST_CASE("Blast: cells off the map are never reached")
{
    LevelGeometry level = OpenFloor(9);
    const auto cells = Blast::ReachedCells(level, 0);   // a corner
    for (int cell : cells) { CHECK(cell >= 0 && cell < 81); }
    CHECK_EQ(cells.size(), size_t(8));                // 3 ring one + 5 ring two
}

TEST_CASE("Blast: the rings split 8 inner cells from 16 outer ones")
{
    LevelGeometry level = OpenFloor(9);
    const auto rings = Blast::ReachedRings(level, 4 * 9 + 4);
    CHECK_EQ(rings.inner.size(), size_t(8));
    CHECK_EQ(rings.outer.size(), size_t(16));
    CHECK(Contains(rings.inner, 3 * 9 + 4));
    CHECK(Contains(rings.outer, 2 * 9 + 4));
    CHECK_EQ(Blast::PLAYER_DAMAGE_INNER, 0x32);
    CHECK_EQ(Blast::PLAYER_DAMAGE_OUTER, 0x19);
}
