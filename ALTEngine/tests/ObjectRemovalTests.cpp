#include "TestFramework.h"

#include "Formats/ObjectRemoval.h"

#include <string>
#include <vector>

using namespace ALTEngine::Formats;

namespace
{
    void Be32(std::vector<uint8_t>& b, size_t at, uint32_t v)
    {
        b[at] = uint8_t(v >> 24); b[at + 1] = uint8_t(v >> 16); b[at + 2] = uint8_t(v >> 8); b[at + 3] = uint8_t(v);
    }

    // FORM + MAP0 with one vertex, one face, a 1 x 1 map, no paths, monsters
    // or pickups, three objects (10, 11, 12 in their first byte), then 8 more
    // bytes; then an L000 chunk.
    std::vector<uint8_t> MakeLevel()
    {
        std::vector<uint8_t> map(36, 0);
        map[0] = 1; map[2] = 1; map[4] = 1; map[6] = 1; map[18] = 3;
        map.resize(36 + 8 + 20 + 16, 0);
        for (uint8_t k = 0; k < 3; ++k) { std::vector<uint8_t> r(16, uint8_t(0x40 + k)); r[0] = uint8_t(10 + k); map.insert(map.end(), r.begin(), r.end()); }
        map.insert(map.end(), 8, 0x77);
        std::vector<uint8_t> file{ 'F', 'O', 'R', 'M', 0, 0, 0, 0, '0', '0', '0', '0', 'M', 'A', 'P', '0', 0, 0, 0, 0 };
        Be32(file, 16, uint32_t(map.size()));
        file.insert(file.end(), map.begin(), map.end());
        const std::vector<uint8_t> l{ 'L', '0', '0', '0', 0, 0, 0, 4, 1, 2, 3, 4 };
        file.insert(file.end(), l.begin(), l.end());
        Be32(file, 4, uint32_t(file.size() - 8));
        return file;
    }

    ObjectRemoval RemoveMiddle()
    {
        ObjectRemoval r;
        r.targetFile = "x";
        r.index = 1;
        r.countBefore = 3;
        r.record.fill(0x41); r.record[0] = 11;
        r.hasNext = true;
        r.next.fill(0x42); r.next[0] = 12;
        return r;
    }
}

TEST_CASE("ObjectRemoval: finds the object records after the header's sections")
{
    const auto file = MakeLevel();
    int count = 0;
    const int64_t at = MapObjectOffset(file, 0, &count);
    CHECK(count == 3);
    CHECK(at > 0);
    CHECK(file[size_t(at)] == 10);
    CHECK(file[size_t(at) + 32] == 12);
}

TEST_CASE("ObjectRemoval: takes the record out, fixes the count and both sizes, then reads as applied")
{
    auto file = MakeLevel();
    const size_t before = file.size();
    std::string error;
    CHECK(ApplyObjectRemoval(file, RemoveMiddle(), error) == ObjectRemovalOutcome::Applied);
    CHECK(file.size() == before - 16);
    int count = 0;
    const int64_t at = MapObjectOffset(file, 1, &count);
    CHECK(count == 2);
    CHECK(file[size_t(at)] == 12);
    CHECK(((uint32_t(file[4]) << 24) | (uint32_t(file[5]) << 16) | (uint32_t(file[6]) << 8) | file[7]) == file.size() - 8);
    const uint32_t mapSize = (uint32_t(file[16]) << 24) | (uint32_t(file[17]) << 16) | (uint32_t(file[18]) << 8) | file[19];
    CHECK(file[20 + mapSize] == 'L');
    auto again = file;
    CHECK(ApplyObjectRemoval(again, RemoveMiddle(), error) == ObjectRemovalOutcome::AlreadyApplied);
    CHECK(again == file);
}

TEST_CASE("ObjectRemoval: leaves a file that is not the expected one alone")
{
    auto file = MakeLevel();
    const auto shipped = file;
    auto wrong = RemoveMiddle();
    wrong.record[5] = 0;
    std::string error;
    CHECK(ApplyObjectRemoval(file, wrong, error) == ObjectRemovalOutcome::Failed);
    CHECK(file == shipped);
    auto count = RemoveMiddle();
    count.countBefore = 4;
    CHECK(ApplyObjectRemoval(file, count, error) == ObjectRemovalOutcome::Failed);
    CHECK(file == shipped);
}
