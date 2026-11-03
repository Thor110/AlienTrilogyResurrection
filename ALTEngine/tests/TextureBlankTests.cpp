#include "TestFramework.h"

#include "Formats/BndTextureLoader.h"
#include "Formats/LevelLoader.h"
#include "Formats/ModelLoader.h"
#include "Formats/RenderMesh.h"
#include "Game/TextureBlank.h"
#include "Game/TextureUse.h"

#include <fstream>
#include <iterator>

using namespace ALTEngine;

namespace
{
    std::vector<uint8_t> ReadDemo(const char* relative)
    {
        std::ifstream in(AltTest::RequireDemoFile(relative), std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}

TEST_CASE("Texture use: L112's faces, animated faces' frames and nothing past its sheet")
{
    const auto level = ReadDemo("SECT11/L112LEV.MAP");
    const auto set = Formats::BndTextureLoader::LoadLevelPages(AltTest::RequireDemoFile("SECT11/112GFX.BIN"));
    Game::TextureUse::Result use;
    std::string error;
    REQUIRE(Game::TextureUse::Scan(level, {}, set.uvRects.size(), use, error));
    REQUIRE(use.use.size() == set.uvRects.size());
    // Every face that is not animated names a used texture.
    const auto geometry = Formats::LevelLoader::Parse(level, "L112");
    int animated = 0;
    for (const auto& q : geometry.quads)
    {
        if (Formats::ClassifyDrawRoutine(q.flags).animated) { ++animated; continue; }
        if (q.texIndex < use.use.size()) { CHECK((use.use[q.texIndex] & Game::TextureUse::FACE) != 0); }
    }
    // Every frame an animator's program shows is used.
    for (uint32_t word : geometry.unknownListB)
    {
        const uint16_t entry = uint16_t(word >> 16);
        if ((entry & 0x8000) == 0 && entry < use.use.size()) { CHECK((use.use[entry] & Game::TextureUse::FRAME) != 0); }
    }
    CHECK(animated > 0);
    size_t unused = 0;
    for (uint8_t u : use.use) { unused += u == 0; }
    CHECK(unused > 0);
    CHECK(unused < use.use.size());
}

TEST_CASE("Texture blanking: used textures keep every texel, the rest take the page's background")
{
    const auto level = ReadDemo("SECT11/L112LEV.MAP");
    const auto sheet = ReadDemo("SECT11/112GFX.BIN");
    const auto set = Formats::BndTextureLoader::LoadLevelPages(AltTest::RequireDemoFile("SECT11/112GFX.BIN"));
    Game::TextureUse::Result use;
    std::string error;
    REQUIRE(Game::TextureUse::Scan(level, {}, set.uvRects.size(), use, error));

    Game::TextureBlank::Plan plan;
    for (size_t t = 0; t < use.use.size(); ++t) { if (!use.use[t]) { plan.unused.push_back(uint32_t(t)); } }
    int pages = 0;
    for (const auto& r : set.uvRects) { pages = std::max(pages, r.page + 1); }
    for (int p = 0; p < pages; ++p) { plan.background[p] = 7; }
    auto bytes = sheet;
    Game::TextureBlank::Report report;
    REQUIRE(Game::TextureBlank::Apply(bytes, plan, report, error));
    CHECK(report.texelsChanged > 0);
    CHECK_EQ(bytes.size(), sheet.size());

    // Decode both through the loader's own rectangles: a used texture's RGBA is the same texel for texel.
    std::ofstream(std::filesystem::temp_directory_path() / "alt_blank_test.bin", std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    const auto blanked = Formats::BndTextureLoader::LoadLevelPages(std::filesystem::temp_directory_path() / "alt_blank_test.bin");
    REQUIRE(blanked.textures.size() == set.textures.size());
    size_t differing = 0, checked = 0;
    for (size_t t = 0; t < set.uvRects.size(); ++t)
    {
        if (!use.use[t]) { continue; }
        const auto& r = set.uvRects[t];
        for (const auto& page : set.textures)
        {
            if (std::atoi(page.index.c_str()) != r.page) { continue; }
            const auto* other = &blanked.textures[size_t(&page - &set.textures[0])];
            for (int y = r.y; y < r.y + r.height && y < 256; ++y)
            {
                for (int x = r.x; x < r.x + r.width && x < 256; ++x)
                {
                    const size_t o = size_t(y * 256 + x) * 4;
                    ++checked;
                    differing += page.rgba[o] != other->rgba[o] || page.rgba[o + 1] != other->rgba[o + 1] || page.rgba[o + 2] != other->rgba[o + 2];
                }
            }
        }
    }
    CHECK(checked > 0);
    CHECK_EQ(differing, size_t(0));
    std::error_code ec;
    std::filesystem::remove(std::filesystem::temp_directory_path() / "alt_blank_test.bin", ec);

    // Blanking again changes nothing.
    auto again = bytes;
    Game::TextureBlank::Report second;
    REQUIRE(Game::TextureBlank::Apply(again, plan, second, error));
    CHECK_EQ(second.texelsChanged, size_t(0));
    CHECK(again == bytes);
}

TEST_CASE("Texture blanking: a plan naming a texture or page the sheet lacks is refused untouched")
{
    const auto sheet = ReadDemo("SECT11/112GFX.BIN");
    std::string error;
    Game::TextureBlank::Report report;
    Game::TextureBlank::Plan bad;
    bad.unused = { 99999 };
    auto bytes = sheet;
    CHECK(!Game::TextureBlank::Apply(bytes, bad, report, error));
    CHECK(bytes == sheet);
    Game::TextureBlank::Plan badPage;
    badPage.background[42] = 1;
    CHECK(!Game::TextureBlank::Apply(bytes, badPage, report, error));
    CHECK(bytes == sheet);
    std::vector<uint8_t> notASheet(64, 0);
    CHECK(!Game::TextureBlank::Apply(notASheet, Game::TextureBlank::Plan{}, report, error));
}
