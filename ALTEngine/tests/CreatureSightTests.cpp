#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/CreatureSight.h"

using namespace ALTEngine::Formats;
namespace CS = ALTEngine::Game::CreatureSight;

namespace
{
    // A 12 x 5 room of open floor, 0x30 ceilings, one zone.
    LevelGeometry Room()
    {
        LevelGeometry level;
        level.header.mapLength = 12;
        level.header.mapWidth = 5;
        level.collisionGrid.resize(12 * 5);
        for (auto& cell : level.collisionGrid) { cell.ceilingHeight = 0x30; cell.ceilingFog = 1; }
        return level;
    }
    CollisionNode& At(LevelGeometry& level, int x, int z) { return level.collisionGrid[static_cast<size_t>(z * 12 + x)]; }

    // A creature at the centre of cell (1, 2) and the player 4000 units along
    // the row, in cell (9, 2). The probes stop distance >> 8 steps out, so the
    // last one falls in cell 8 and never touches the player's occupied cell.
    CS::Query Across()
    {
        CS::Query q;
        q.x = 1 * 512 + 256; q.z = 2 * 512 + 256; q.y = 0x20;
        q.targetX = q.x + 4000; q.targetZ = q.z;
        q.distance = 4000;
        q.occupiedCellX = 9; q.occupiedCellZ = 2;
        return q;
    }
}

TEST_CASE("CreatureSight: a clear room gives a line of fire")
{
    LevelGeometry level = Room();
    CHECK(CS::LineOfFire(level, Across()));
}

TEST_CASE("CreatureSight: a wall in between blocks it, and so does one either side of the line")
{
    LevelGeometry level = Room();
    At(level, 5, 2).attribute = 0x14;
    CHECK(!CS::LineOfFire(level, Across()));

    // The probes run 200 units either side, so a wall in the neighbouring row
    // still cuts the corridor off.
    LevelGeometry side = Room();
    At(side, 5, 1).attribute = 0x14;
    At(side, 5, 3).attribute = 0x14;
    CS::Query q = Across();
    q.z = 2 * 512 + 100; q.targetZ = 2 * 512 + 100;   // nearer row 1 than row 3
    CHECK(!CS::LineOfFire(side, q));
}

TEST_CASE("CreatureSight: a step up of more than 0x100 or a low ceiling blocks it")
{
    LevelGeometry step = Room();
    for (int z = 0; z < 5; ++z) { At(step, 5, z).floorHeight = 0x10; }   // 0x200 up
    CHECK(!CS::LineOfFire(step, Across()));

    LevelGeometry low = Room();
    for (int z = 0; z < 5; ++z) { At(low, 5, z).ceilingHeight = 0x08; }  // 0x100: below 0x20 + 0x200
    CHECK(!CS::LineOfFire(low, Across()));
}

TEST_CASE("CreatureSight: a probe that reaches the player's own cell is refused")
{
    // As in the original: the player's cell is stamped occupied, and at a
    // distance that is an exact multiple of 0x100 from its centre the last
    // probe lands on its edge. (Checked against FUN_0003231c run in an
    // emulator, which refuses this case too.)
    LevelGeometry level = Room();
    CS::Query q = Across();
    q.targetX = 9 * 512 + 256;
    q.distance = static_cast<int>(q.targetX - q.x);
    CHECK(!CS::LineOfFire(level, q));
}

TEST_CASE("CreatureSight: no line beyond 0x2000")
{
    LevelGeometry level = Room();
    CS::Query q = Across();
    q.distance = 0x2001;
    CHECK(!CS::LineOfFire(level, q));
}

TEST_CASE("CreatureSight: zones")
{
    LevelGeometry level = Room();
    At(level, 9, 2).ceilingFog = 2;
    CHECK(!CS::ZoneVisible(level, 2 * 12 + 1, 2 * 12 + 9));
    CHECK(CS::ZoneVisible(level, 2 * 12 + 1, 2 * 12 + 2));
}

#include "Game/OriginalMath.h"

TEST_CASE("OriginalMath: bearings and roots match the original's routines")
{
    namespace OM = ALTEngine::Game::OriginalMath;
    // FUN_00026b64(creature, player), captured from the original in an
    // emulator: (creature x, z, player x, z) -> bearing.
    CHECK_EQ(OM::Bearing(1171 - 3637, 846 - 2452), 1401);
    CHECK_EQ(OM::Bearing(7442 - 3635, 3367 - 635), 3480);
    CHECK_EQ(OM::Bearing(1684 - 3992, 1255 - 6248), 1765);
    CHECK_EQ(OM::Bearing(6635 - 2329, 5737 - 7998), 2757);
    CHECK_EQ(OM::Bearing(3072 - 7177, 6602 - 5195), 808);
    // FUN_0004de04 runs low for large arguments.
    CHECK_EQ(OM::Sqrt1024(274281998), 16906240);
    CHECK_EQ(OM::Sqrt1024(31144123), 5694976);
    // FUN_00026a20 turning (0, 0, 256) a quarter turn: (-256, 0).
    int x = 0, z = 0;
    OM::RotateYaw(256, 0x400, x, z);
    CHECK_EQ(x, -256);
    CHECK_EQ(z, 0);
}

TEST_CASE("CreatureSight: a creature sharing the player's cell is not blocked by it")
{
    // The original lifts the creature's own cell stamp before probing, and in
    // a shared cell that is the player's stamp too (seen in the emulator).
    LevelGeometry level = Room();
    CS::Query q;
    q.x = 1783 % 512 + 3 * 512; q.z = 2 * 512 + 400; q.y = 0x20;
    q.targetX = q.x - 206; q.targetZ = q.z - 345;
    q.distance = 401;
    q.occupiedCellX = static_cast<int>(q.targetX) >> 9; q.occupiedCellZ = static_cast<int>(q.targetZ) >> 9;
    REQUIRE((static_cast<int>(q.x) >> 9) == q.occupiedCellX);
    CHECK(CS::LineOfFire(level, q));
}

TEST_CASE("CreatureSight: another creature in the way blocks the line")
{
    LevelGeometry level = Room();
    std::vector<uint8_t> occupancy(level.collisionGrid.size(), 0);
    CS::Query q = Across();
    q.occupancy = &occupancy;
    CHECK(CS::LineOfFire(level, q));
    occupancy[static_cast<size_t>(2 * 12 + 5)] = 1;   // a creature halfway along
    CHECK(!CS::LineOfFire(level, q));
}
