#include "TestFramework.h"

#include "Game/SaveGame.h"

#include <fstream>

using namespace ALTEngine::Game;

namespace
{
    // A fresh, empty directory per test, under the system temp folder.
    std::filesystem::path TempSaveDir(const std::string& name)
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / ("altengine-tests-" + name);
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
        return dir;
    }
}

TEST_CASE("SaveGame: a written slot reads back identically")
{
    const auto dir = TempSaveDir("roundtrip");
    SaveData data;
    data.name = SaveGame::DefaultName("112");
    data.levelDigits = "112";
    data.difficulty = 2;
    data.inventory.shotgun.available = true;
    data.inventory.shotgun.rounds = 37;
    data.inventory.pulseRifle.available = true;
    data.inventory.pulseRifle.rounds = 60;
    data.inventory.pulseRifle.units = 3;
    data.inventory.pulseRifle.grenades = 4;
    data.inventory.batteries = 2;
    data.inventory.hasAutoMapper = true;

    REQUIRE(SaveGame::Write(dir, 3, data));
    auto read = SaveGame::Read(dir, 3);
    REQUIRE(read.has_value());
    CHECK(read->name == "MISSION 1.1.2");
    CHECK(read->levelDigits == "112");
    CHECK_EQ(read->difficulty, 2);
    CHECK_EQ(read->inventory.AmmoTotal(1), 37);
    CHECK_EQ(read->inventory.AmmoTotal(2), 360);
    CHECK_EQ(read->inventory.pulseRifle.grenades, 4);
    CHECK_EQ(read->inventory.AmmoTotal(0), 45);
    CHECK(read->inventory.pistol.equipped);
    CHECK_EQ(read->inventory.batteries, 2);
    CHECK(read->inventory.hasAutoMapper);
    CHECK(!read->inventory.hasShoulderLamp);
}

TEST_CASE("SaveGame: overwriting a slot replaces it")
{
    const auto dir = TempSaveDir("overwrite");
    SaveData a; a.name = "A"; a.levelDigits = "111";
    SaveData b; b.name = "B"; b.levelDigits = "121";
    REQUIRE(SaveGame::Write(dir, 1, a));
    REQUIRE(SaveGame::Write(dir, 1, b));
    auto read = SaveGame::Read(dir, 1);
    REQUIRE(read.has_value());
    CHECK(read->name == "B");
    CHECK(!std::filesystem::exists(SaveGame::SlotPath(dir, 1).string() + ".tmp"));
}

TEST_CASE("SaveGame: the slot list shows used slots only")
{
    const auto dir = TempSaveDir("list");
    SaveData a; a.name = "FIRST"; a.levelDigits = "111";
    SaveGame::Write(dir, 2, a);
    SaveGame::Write(dir, 10, a);
    const auto names = SaveGame::ListNames(dir);
    CHECK(!names[0].has_value());
    CHECK(names[1].has_value() && *names[1] == "FIRST");
    CHECK(names[9].has_value());
}

TEST_CASE("SaveGame: bad slots and broken files are rejected, not crashed on")
{
    const auto dir = TempSaveDir("broken");
    SaveData a; a.levelDigits = "111";
    CHECK(!SaveGame::Write(dir, 0, a));
    CHECK(!SaveGame::Write(dir, 11, a));
    CHECK(!SaveGame::Read(dir, 4).has_value());
    {
        std::ofstream out(SaveGame::SlotPath(dir, 4));
        out << "{ not json";
    }
    CHECK(!SaveGame::Read(dir, 4).has_value());
    {
        std::ofstream out(SaveGame::SlotPath(dir, 5));
        out << R"({"version": 999, "level": "111", "inventory": {"weapons": []}})";
    }
    CHECK(!SaveGame::Read(dir, 5).has_value());
}
