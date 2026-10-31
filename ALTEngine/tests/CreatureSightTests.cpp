#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/CreatureSight.h"
#include "Game/OriginalMath.h"

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
    CHECK(!CS::ZoneVisible(level, 2 * 12 + 9, 1));
    CHECK(CS::ZoneVisible(level, 2 * 12 + 2, 1));
}

TEST_CASE("CreatureSight: the drawn zones spread from the player's cell through what is in view")
{
    // The room is zone 1. A second zone (2) shares the cells of column 6 with
    // zone 1 (bits 1 and 2), and runs on through columns 7-11 on its own. A
    // third (4) is only in column 11. Every cell has faces.
    LevelGeometry level = Room();
    for (auto& cell : level.collisionGrid) { cell.unknown1 = cell.unknown2 = cell.unknown3 = cell.unknown4 = 0; }
    for (int z = 0; z < 5; ++z)
    {
        At(level, 6, z).ceilingFog = 3;
        for (int x = 7; x < 11; ++x) { At(level, x, z).ceilingFog = 2; }
        At(level, 11, z).ceilingFog = 4;
    }
    // From (2, 2) looking along +x (yaw 0xc00 has the rings' cells at x+1)
    // the walk reaches column 6, so zone 2 joins; zone 4 never touches it.
    const uint16_t ahead = CS::DrawnZoneMask(level, 2, 2, 0xc00);
    CHECK(ahead == 3);
    // Looking the other way, column 6 is behind and outside every window.
    CHECK(CS::DrawnZoneMask(level, 2, 2, 0x400) == 1);
    // A cell with no faces is drawn but adds nothing.
    for (int z = 0; z < 5; ++z) { auto& c = At(level, 6, z); c.unknown1 = c.unknown2 = c.unknown3 = c.unknown4 = 0xff; }
    CHECK(CS::DrawnZoneMask(level, 2, 2, 0xc00) == 1);
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

namespace
{
    // The same creature and player for FUN_00032aec's ray: 0x240 above the
    // creature to 0x400 above the player's floor.
    CS::Query3D Ray()
    {
        CS::Query3D q;
        q.x = 1 * 512 + 256; q.z = 2 * 512 + 256; q.y = 0x20;
        q.targetX = q.x + 4000; q.targetZ = q.z; q.targetFloorY = 0;
        q.distance = 4000;
        q.heading = ALTEngine::Game::OriginalMath::Bearing(static_cast<int>(q.x - q.targetX), static_cast<int>(q.z - q.targetZ));
        return q;
    }
}

TEST_CASE("CreatureSight: the gunners' ray passes over a crate, not a double crate, and not a wall")
{
    LevelGeometry level = Room();
    CHECK(CS::LineOfFire3D(level, Ray()));

    At(level, 5, 2).unknown13 = 0x14;   // a crate raises the floor by 0x200 - under the ray
    CHECK(CS::LineOfFire3D(level, Ray()));
    At(level, 5, 2).unknown13 = 0x19;   // a double crate by 0x400 - over it
    CHECK(!CS::LineOfFire3D(level, Ray()));
    At(level, 5, 2).unknown13 = 0x16;   // anything else in the cell blocks
    CHECK(!CS::LineOfFire3D(level, Ray()));

    LevelGeometry wall = Room();
    At(wall, 5, 2).attribute = 0x14;
    CHECK(!CS::LineOfFire3D(wall, Ray()));
}

TEST_CASE("CreatureSight: the gunners' ray is blocked by a creature, a low ceiling, and beyond 0x1555")
{
    LevelGeometry level = Room();
    std::vector<uint8_t> occupancy(level.collisionGrid.size(), 0);
    occupancy[2 * 12 + 5] = 1;
    CS::Query3D q = Ray(); q.occupancy = &occupancy;
    CHECK(!CS::LineOfFire3D(level, q));

    LevelGeometry low = Room();
    At(low, 5, 2).ceilingHeight = 0x10;   // 0x200, under the ray
    CHECK(!CS::LineOfFire3D(low, Ray()));

    CS::Query3D distant = Ray(); distant.distance = 0x1556;
    CHECK(!CS::LineOfFire3D(Room(), distant));
}

TEST_CASE("CreatureSight: the map shows what the view walk has seen, in discovered areas")
{
    namespace CS = ALTEngine::Game::CreatureSight;
    ALTEngine::Formats::LevelGeometry level = ALTEngine::Formats::LevelLoader::Load(
        AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    // A cell the player can stand in.
    int px = -1, pz = -1;
    for (int z = 0; z < level.header.mapWidth && px < 0; ++z)
    {
        for (int x = 0; x < level.header.mapLength; ++x)
        {
            if (!ALTEngine::Formats::IsCellBlocking(level, x << 9, z << 9)) { px = x; pz = z; break; }
        }
    }
    REQUIRE(px >= 0);

    std::vector<uint8_t> seen, areas;
    CS::MarkSeen(level, seen, areas, px, pz, 0);
    const size_t playerIndex = static_cast<size_t>(pz) * level.header.mapLength + static_cast<size_t>(px);
    CHECK_EQ(seen.size(), level.collisionGrid.size());
    CHECK_EQ(areas.size(), size_t(256));
    CHECK(seen[playerIndex] != 0);
    CHECK(areas[0] != 0);
    size_t seenCount = 0;
    for (uint8_t s : seen) { seenCount += s ? 1 : 0; }
    CHECK(seenCount > 1);
    CHECK(seenCount < seen.size());

    // Without the Auto Mapper only what was seen shows; with it, the whole of
    // every discovered area does.
    const auto plain = CS::MapReveal(level, seen, areas, false);
    const auto mapped = CS::MapReveal(level, seen, areas, true);
    size_t plainCount = 0, mappedCount = 0;
    for (size_t i = 0; i < plain.size(); ++i) { plainCount += plain[i] ? 1 : 0; mappedCount += mapped[i] ? 1 : 0; }
    CHECK_EQ(plainCount, seenCount);
    CHECK(mappedCount > plainCount);
}

TEST_CASE("CreatureSight: each cell's face run partitions the level's faces")
{
    namespace CS = ALTEngine::Game::CreatureSight;
    const ALTEngine::Formats::LevelGeometry level = ALTEngine::Formats::LevelLoader::Load(
        AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    const std::vector<int32_t> owner = CS::FaceCells(level);
    REQUIRE(owner.size() == level.quads.size());
    // Every run starts at its cell's first face and ends on the end bit, and
    // no face is claimed twice (the walk would overwrite, so check run by run).
    std::vector<int> claims(level.quads.size(), 0);
    size_t owned = 0;
    for (size_t cell = 0; cell < level.collisionGrid.size(); ++cell)
    {
        const auto& n = level.collisionGrid[cell];
        const uint32_t start = n.unknown1 | (n.unknown2 << 8) | (n.unknown3 << 16) | (static_cast<uint32_t>(n.unknown4) << 24);
        if (start == 0xffffffffu || start == 0) { continue; }
        for (size_t q = start - 1; q < level.quads.size(); ++q)
        {
            ++claims[q];
            CHECK_EQ(owner[q], static_cast<int32_t>(cell));
            if (level.quads[q].reserved & 0x80) { break; }
        }
    }
    for (size_t q = 0; q < claims.size(); ++q) { CHECK(claims[q] <= 1); owned += claims[q] ? 1 : 0; }
    CHECK(owned * 10 > level.quads.size() * 9);
}

TEST_CASE("CreatureSight: the drawn cells are the view walk's, widened only for a wider view")
{
    namespace CS = ALTEngine::Game::CreatureSight;
    LevelGeometry level = Room();
    for (auto& cell : level.collisionGrid) { cell.unknown1 = cell.unknown2 = cell.unknown3 = cell.unknown4 = 0; }
    // Looking along +x from (2, 2): the cell ahead is drawn, the one behind
    // (outside ring 1's 0x384 window) is not.
    const auto ahead = CS::DrawnCells(level, 2, 2, 0xc00);
    CHECK(ahead[2 * 12 + 2] == 1);
    CHECK(ahead[2 * 12 + 3] == 1);
    CHECK(ahead[2 * 12 + 1] == 0);
    CHECK(ahead[2 * 12 + 10] == 1);
    // A wider view takes the first two rings whole, so the cell behind is
    // drawn too, and never takes fewer cells.
    const auto wide = CS::DrawnCells(level, 2, 2, 0xc00, 0x300);
    CHECK(wide[2 * 12 + 1] == 1);
    for (size_t i = 0; i < ahead.size(); ++i) { CHECK(wide[i] >= ahead[i]); }
    // Outside the map nothing is drawn.
    const auto outside = CS::DrawnCells(level, -1, 2, 0xc00);
    for (uint8_t d : outside) { CHECK(d == 0); }
}

TEST_CASE("OriginalMath: the square root's table matches counting the root up, entry by entry")
{
    namespace OM = ALTEngine::Game::OriginalMath;
    for (int64_t n = 0; n < 256; ++n)
    {
        int64_t r = 0;
        while ((r + 1) * (r + 1) <= (n << 20)) { ++r; }
        CHECK_EQ(OM::Sqrt1024(n), r);
    }
    CHECK_EQ(OM::Sqrt1024(-5), int64_t(0));
    CHECK_EQ(OM::Sqrt1024(1024), 2 * OM::Sqrt1024(256));
}
