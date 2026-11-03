#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/CreatureSight.h"
#include "Game/LevelCleanup.h"
#include "Game/LevelCleanupPlan.h"
#include "Game/LevelFileLayout.h"

#include <fstream>
#include <iterator>
#include <map>
#include <tuple>

using namespace ALTEngine;

namespace
{
    std::vector<uint8_t> ReadDemo(const char* relative)
    {
        std::ifstream in(AltTest::RequireDemoFile(relative), std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // Works out the plan and applies it, as ALTLevelCleanupList and the
    // install do between them (no textures: no cut-out counts as solid).
    bool Clean(std::vector<uint8_t>& bytes, bool multiplayer, Game::LevelCleanup::Report& report, std::string& error,
               Game::LevelCleanup::Plan* planOut = nullptr)
    {
        Game::LevelCleanupPlan::Options options;
        options.multiplayer = multiplayer;
        Game::LevelCleanup::Plan plan;
        if (!Game::LevelCleanupPlan::Make(bytes, options, plan, error)) { return false; }
        if (planOut) { *planOut = plan; }
        return Game::LevelCleanup::Apply(bytes, plan, report, error);
    }

    // Every cell's faces as drawn: texture, routine, light and the corners' coordinates.
    using Face = std::tuple<int, int, int, int, int, int, int, int, int, int, int, int>;
    std::map<int, std::vector<Face>> CellFaces(const Formats::LevelGeometry& level)
    {
        std::map<int, std::vector<Face>> out;
        const auto owner = Game::CreatureSight::FaceCells(level);
        for (size_t q = 0; q < level.quads.size(); ++q)
        {
            if (owner[q] < 0) { continue; }
            const auto& f = level.quads[q];
            auto at = [&](int32_t v, int axis) -> int {
                if (v < 0) { return -1; }
                const auto& p = level.vertices[static_cast<size_t>(v)];
                return axis == 0 ? p.x : axis == 1 ? p.y : p.z;
            };
            out[owner[q]].push_back(Face{ f.texIndex, f.flags, f.reserved, at(f.a, 0), at(f.a, 1), at(f.a, 2),
                                          at(f.b, 0), at(f.b, 2), at(f.c, 0), at(f.c, 2), at(f.d, 0), at(f.d, 2) });
        }
        return out;
    }
}

TEST_CASE("Level cleanup: L111 loses the faces it never draws and nothing else")
{
    const auto shipped = ReadDemo("SECT11/L111LEV.MAP");
    auto bytes = shipped;
    Game::LevelCleanup::Report report;
    std::string error;
    REQUIRE(Clean(bytes, false, report, error));
    CHECK_EQ(report.facesRemoved, 7);         // four behind the start, three on the map's top edge
    CHECK(report.verticesRemoved > 0);
    CHECK(!report.firstCellRestored);         // L111's first cell has no faces in the file
    CHECK_EQ(report.bytesAfter, bytes.size());
    CHECK(bytes.size() < shipped.size());

    const auto before = Formats::LevelLoader::Parse(shipped, "before");
    const auto after = Formats::LevelLoader::Parse(bytes, "after");
    const auto was = CellFaces(before), now = CellFaces(after);
    CHECK_EQ(was.size() - now.size(), size_t(7));
    for (const auto& [cell, faces] : now)
    {
        REQUIRE(was.count(cell));
        CHECK(was.at(cell) == faces);          // every drawn cell exactly as it was
    }
    CHECK_EQ(after.collisionGrid.size(), before.collisionGrid.size());
    CHECK_EQ(after.monsters.size(), before.monsters.size());
    CHECK_EQ(after.doors.size(), before.doors.size());

    // Planned again from the cleaned file: nothing more to do.
    auto again = bytes;
    Game::LevelCleanup::Report second;
    REQUIRE(Clean(again, false, second, error));
    CHECK(!second.Changed());
    CHECK(again == bytes);
}

TEST_CASE("Level cleanup: L112's first cell gets its own faces back, record 0's among them")
{
    auto bytes = ReadDemo("SECT11/L112LEV.MAP");
    const auto shipped = bytes;   // record 0 as the file has it
    const auto before = Formats::LevelLoader::Parse(bytes, "before");
    Game::LevelCleanup::Report report;
    std::string error;
    REQUIRE(Clean(bytes, false, report, error));
    CHECK(report.firstCellRestored);
    // Its run begins at record 0 (the face the cell's start of 0 hid along
    // with record 1): a copy of it heads the run, then record 1.
    CHECK_EQ(report.firstCellFaces, 2);
    const auto after = Formats::LevelLoader::Parse(bytes, "after");
    const auto ownerBefore = Game::CreatureSight::FaceCells(before);
    const auto ownerAfter = Game::CreatureSight::FaceCells(after);
    CHECK_EQ(ownerBefore[0], -1);              // listed by no cell as shipped
    CHECK(ownerAfter[0] >= 0);                 // record 0's copy, now its cell's run
    CHECK(ownerAfter[1] >= 0);                 // and record 1
    CHECK_EQ(ownerAfter[0], ownerAfter[1]);
    // The copy is record 0: the same texture and the same corners.
    Game::LevelFileLayout::Layout lay;
    REQUIRE(Game::LevelFileLayout::Read(shipped, lay, error));
    REQUIRE(!after.quads.empty());
    const auto& copy = after.quads[0];
    CHECK_EQ(int(copy.texIndex), int(lay.Texture(shipped, 0)));
    const int corners[4] = { copy.a, copy.b, copy.c, copy.d };
    for (int k = 0; k < 4; ++k)
    {
        const int32_t was = lay.Corner(shipped, 0, k);
        if (was < 0) { CHECK(corners[k] < 0); continue; }
        REQUIRE(corners[k] >= 0 && size_t(corners[k]) < after.vertices.size());
        const auto& v = after.vertices[size_t(corners[k])];
        CHECK_EQ(int(v.x), int(lay.Coordinate(shipped, size_t(was), 0)));
        CHECK_EQ(int(v.y), int(lay.Coordinate(shipped, size_t(was), 1)));
        CHECK_EQ(int(v.z), int(lay.Coordinate(shipped, size_t(was), 2)));
    }
}

TEST_CASE("Level cleanup: the view walk takes in the same cells after it")
{
    // The walk spreads through a cell's zones only while the cell has a run
    // start, so a drawn cell must keep one even when it loses every face.
    for (const char* name : { "SECT11/L111LEV.MAP", "SECT11/L112LEV.MAP" })
    {
        const auto shipped = ReadDemo(name);
        auto bytes = shipped;
        Game::LevelCleanup::Report report;
        std::string error;
        REQUIRE(Clean(bytes, false, report, error));
        const auto before = Formats::LevelLoader::Parse(shipped, "before");
        const auto after = Formats::LevelLoader::Parse(bytes, "after");
        const int length = before.header.mapLength;
        int compared = 0;
        for (size_t cell = 0; cell < before.collisionGrid.size(); cell += 37)
        {
            const int x = int(cell % size_t(length)), z = int(cell / size_t(length));
            for (int yaw = 0; yaw < 4096; yaw += 512)
            {
                for (int width : { 0, 0x2a0 })
                {
                    CHECK(Game::CreatureSight::DrawnCells(before, x, z, yaw, width) == Game::CreatureSight::DrawnCells(after, x, z, yaw, width));
                    ++compared;
                }
            }
        }
        CHECK(compared > 0);
    }
}

TEST_CASE("Level cleanup: plans name faces the way the list file writes them")
{
    std::vector<uint32_t> values;
    CHECK(Game::LevelCleanup::EncodeList({ 9, 1, 2, 3, 4, 5, 12, 13, 40, 9 }) == "1-5,9,12-13,40");
    REQUIRE(Game::LevelCleanup::DecodeList("1-5,9,12-13,40", values));
    CHECK(values == std::vector<uint32_t>({ 1, 2, 3, 4, 5, 9, 12, 13, 40 }));
    REQUIRE(Game::LevelCleanup::DecodeList("", values));
    CHECK(values.empty());
    CHECK(!Game::LevelCleanup::DecodeList("1-", values));
    CHECK(!Game::LevelCleanup::DecodeList("5-2", values));
    CHECK(!Game::LevelCleanup::DecodeList("1,,2", values));
    CHECK(!Game::LevelCleanup::DecodeList("x", values));
}

TEST_CASE("Level cleanup: a plan naming faces the file does not have is refused")
{
    auto bytes = ReadDemo("SECT11/L111LEV.MAP");
    const auto copy = bytes;
    Game::LevelCleanup::Plan plan;
    plan.remove = { 60000 };
    Game::LevelCleanup::Report report;
    std::string error;
    CHECK(!Game::LevelCleanup::Apply(bytes, plan, report, error));
    CHECK(!error.empty());
    CHECK(bytes == copy);
}

TEST_CASE("Level cleanup: anything not a level is refused untouched")
{
    std::vector<uint8_t> junk{ 'F', 'O', 'R', 'M', 0, 0, 0, 4, 'J', 'U', 'N', 'K' };
    const auto copy = junk;
    Game::LevelCleanup::Report report;
    std::string error;
    CHECK(!Clean(junk, false, report, error));
    CHECK(!error.empty());
    CHECK(junk == copy);
}
