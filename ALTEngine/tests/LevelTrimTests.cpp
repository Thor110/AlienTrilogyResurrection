#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Formats/LevelTrim.h"

#include <fstream>
#include <iterator>

namespace
{
    std::vector<uint8_t> ReadDemoLevel(const char* relative)
    {
        std::ifstream in(AltTest::RequireDemoFile(relative), std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}

TEST_CASE("Padding trim: a level trims, expands back and loads the same")
{
    namespace LT = ALTEngine::Formats::LevelTrim;
    for (const char* name : { "SECT11/L111LEV.MAP", "SECT11/L112LEV.MAP" })
    {
        const auto shipped = ReadDemoLevel(name);
        std::vector<uint8_t> trimmed, back, normalized;
        std::string error;
        REQUIRE(!LT::IsTrimmed(shipped));
        REQUIRE(LT::Trim(shipped, trimmed, error));
        CHECK(LT::IsTrimmed(trimmed));
        CHECK(trimmed.size() < shipped.size() - shipped.size() / 20);   // over 5% smaller
        REQUIRE(LT::Expand(trimmed, back, error));
        REQUIRE(LT::Normalized(shipped, normalized, error));
        CHECK(back == normalized);                                       // the fixed bytes put back exactly
        CHECK(back.size() == shipped.size());

        // What the game reads is the same either way.
        const auto a = ALTEngine::Formats::LevelLoader::Parse(shipped, "shipped");
        const auto b = ALTEngine::Formats::LevelLoader::Parse(trimmed, "trimmed");
        CHECK_EQ(a.vertices.size(), b.vertices.size());
        CHECK_EQ(a.quads.size(), b.quads.size());
        CHECK_EQ(a.faceRecordsPos, b.faceRecordsPos);                    // offsets in the untrimmed layout
        CHECK_EQ(a.collisionGrid.size(), b.collisionGrid.size());
        bool same = true;
        for (size_t i = 0; i < a.collisionGrid.size(); ++i)
        {
            const auto& x = a.collisionGrid[i];
            const auto& y = b.collisionGrid[i];
            same = same && x.floorHeight == y.floorHeight && x.ceilingHeight == y.ceilingHeight && x.attribute == y.attribute
                   && x.runtimeFlags == y.runtimeFlags && x.scriptAction == y.scriptAction && x.faceRun0 == y.faceRun0;
        }
        CHECK(same);
        for (size_t i = 0; i < a.crates.size() && i < b.crates.size(); ++i) { CHECK_EQ(int(a.crates[i].unusedByte5), int(b.crates[i].unusedByte5)); }
        for (size_t i = 0; i < a.lifts.size() && i < b.lifts.size(); ++i) { CHECK_EQ(int(a.lifts[i].unusedByte6), int(b.lifts[i].unusedByte6)); }
        CHECK(a.textureAnimators == b.textureAnimators);
        CHECK(a.animationFrames == b.animationFrames);

        // Trimmed once only; a file that is not trimmed expands to itself.
        std::vector<uint8_t> twice, same2;
        CHECK(!LT::Trim(trimmed, twice, error));
        REQUIRE(LT::Expand(shipped, same2, error));
        CHECK(same2 == shipped);
    }
}

TEST_CASE("Padding trim: a level whose fixed bytes are not their usual values is left alone")
{
    namespace LT = ALTEngine::Formats::LevelTrim;
    auto bytes = ReadDemoLevel("SECT11/L111LEV.MAP");
    // The first vertex's +6, always 0: MAP0's data starts at 20, the header is 36 bytes.
    bytes[20 + 36 + 6] = 1;
    std::vector<uint8_t> trimmed;
    std::string error;
    CHECK(!LT::Trim(bytes, trimmed, error));
    CHECK(!error.empty());
}
