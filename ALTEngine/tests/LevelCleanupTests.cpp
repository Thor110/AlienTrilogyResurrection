#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/CreatureSight.h"
#include "Game/LevelCleanup.h"

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
    REQUIRE(Game::LevelCleanup::Clean(bytes, false, report, error));
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

    // Again: nothing more to do.
    auto again = bytes;
    Game::LevelCleanup::Report second;
    REQUIRE(Game::LevelCleanup::Clean(again, false, second, error));
    CHECK(!second.Changed());
    CHECK(again == bytes);
}

TEST_CASE("Level cleanup: L112's first cell gets its own face back")
{
    auto bytes = ReadDemo("SECT11/L112LEV.MAP");
    const auto before = Formats::LevelLoader::Parse(bytes, "before");
    Game::LevelCleanup::Report report;
    std::string error;
    REQUIRE(Game::LevelCleanup::Clean(bytes, false, report, error));
    CHECK(report.firstCellRestored);
    CHECK_EQ(report.firstCellFaces, 1);
    const auto after = Formats::LevelLoader::Parse(bytes, "after");
    const auto ownerBefore = Game::CreatureSight::FaceCells(before);
    const auto ownerAfter = Game::CreatureSight::FaceCells(after);
    CHECK_EQ(ownerBefore[0], -1);              // listed by no cell as shipped
    CHECK(ownerAfter[0] >= 0);                 // now its cell's run
}

TEST_CASE("Level cleanup: anything not a level is refused untouched")
{
    std::vector<uint8_t> junk{ 'F', 'O', 'R', 'M', 0, 0, 0, 4, 'J', 'U', 'N', 'K' };
    const auto copy = junk;
    Game::LevelCleanup::Report report;
    std::string error;
    CHECK(!Game::LevelCleanup::Clean(junk, false, report, error));
    CHECK(!error.empty());
    CHECK(junk == copy);
}
