#include "TestFramework.h"

#include "Formats/EnemySpriteSet.h"

#include <filesystem>
#include <fstream>

using ALTEngine::Formats::EnemySpriteSet;

TEST_CASE("Enemy sprites: a frame stored uncompressed decodes as it is")
{
    // A file with one F000 section holding a 2x2 frame's four bytes raw, as
    // the burster's death animations store their last frame (the body). Its
    // bytes run exactly to the section's end, which is what marks it raw.
    const auto path = std::filesystem::temp_directory_path() / "altengine_raw_frame.b16";
    {
        std::ofstream out(path, std::ios::binary);
        const unsigned char header[12] = { 'F','O','R','M', 0,0,0,0x10, 'B','1','6',' ' };
        out.write(reinterpret_cast<const char*>(header), 12);
        const unsigned char chunk[12] = { 'F','0','0','0', 0,0,0,4, 1,2,3,4 };
        out.write(reinterpret_cast<const char*>(chunk), 12);
    }
    EnemySpriteSet set;
    REQUIRE(set.Load(path));
    EnemySpriteSet::Frame record;
    record.width = 2;
    record.height = 2;
    record.compressedOffset = 0;
    const auto frames = set.DecodeAnimation({ record });
    REQUIRE(frames.size() == 1);
    CHECK(frames[0].pixels == std::vector<uint8_t>({ 1, 2, 3, 4 }));

    // A size the bytes don't account for is still refused.
    record.width = 3;
    CHECK(set.DecodeAnimation({ record }).empty());
    std::filesystem::remove(path);
}
