#include "TestFramework.h"

#include "Formats/BndParser.h"
#include "Formats/BootSequence.h"
#include "Formats/BndTextureLoader.h"
#include "Formats/BxParser.h"
#include "Formats/ExplosionGraphics.h"
#include "Formats/LevelLoader.h"
#include "Formats/LightTable.h"
#include "Formats/SpriteFrameDecompressor.h"
#include "Formats/SpriteRle.h"
#include "Formats/TextureAnimation.h"

#include <cmath>
#include <fstream>
#include <iterator>

using namespace ALTEngine::Formats;

namespace
{
    std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    }

    LightRecord Light(uint8_t mode, uint8_t litGrey, uint8_t unlitGrey)
    {
        LightRecord r;
        r.mode = mode;
        for (int c = 0; c < 3; ++c) { r.lit[c] = litGrey; r.unlit[c] = unlitGrey; }
        return r;
    }
}

// ---- the light table (FUN_00029be0 and the per-tick updater) ----------------

TEST_CASE("Lights: a flat record shows its lit colour, scaled by the global multiplier")
{
    LightTable table;
    LightRecord flat = Light(0, 96, 0);
    flat.on = 1;
    table.Reset({ flat });
    REQUIRE(table.Entries().size() == 1);
    // 96 * 3840 / 0xc00 = 120, on all four corners.
    for (int c = 0; c < 4; ++c) { CHECK_EQ(static_cast<int>(table.ColourFor(0).corner[c].r), 120); }
    // Bit 0x80 is the end-of-run marker, not part of the index.
    CHECK(table.ColourFor(0x80) == table.ColourFor(0));
    // Past the table: full brightness.
    CHECK_EQ(static_cast<int>(table.ColourFor(5).corner[0].r), 255);
}

TEST_CASE("Lights: a flash brightens one tick only, clamped at 255")
{
    LightTable table;
    LightRecord flat = Light(0, 96, 0);
    flat.on = 1;
    table.Reset({ flat });
    table.RequestFlash();
    table.Tick(0);
    CHECK_EQ(static_cast<int>(table.ColourFor(0).corner[0].r), 255);   // 96 * 8192 / 0xc00 = 256
    table.Tick(0);
    CHECK_EQ(static_cast<int>(table.ColourFor(0).corner[0].r), 120);
}

TEST_CASE("Lights: a gouraud record takes lit on its first corner and unlit on its second")
{
    LightTable table;
    LightRecord g = Light(4, 48, 24);
    for (int c = 0; c < 3; ++c) { g.corner2[c] = 12; g.corner3[c] = 0; }
    table.Reset({ g });
    const auto& e = table.ColourFor(0);
    CHECK_EQ(static_cast<int>(e.corner[0].r), 60);
    CHECK_EQ(static_cast<int>(e.corner[1].r), 30);
    CHECK_EQ(static_cast<int>(e.corner[2].r), 15);
    CHECK_EQ(static_cast<int>(e.corner[3].r), 0);
}

TEST_CASE("Lights: a blink record waits for its switch, then blinks with its durations")
{
    LightTable table;
    LightRecord blink = Light(1, 96, 0);
    blink.variantMax = 1;
    blink.onDuration = 3;
    blink.offDuration = 2;
    blink.blinkRepeats = 5;
    table.Reset({ blink });
    for (int t = 0; t < 10; ++t) { table.Tick(0); }
    CHECK_EQ(static_cast<int>(table.Records()[0].on), 0);          // not armed
    table.ToggleLight(0, 5);
    CHECK_EQ(static_cast<int>(table.Records()[0].variant), 1);     // clamped to its maximum
    table.Tick(0);                                                  // jitter (0 & 3) + 1
    CHECK_EQ(static_cast<int>(table.Records()[0].on), 1);
    CHECK_EQ(static_cast<int>(table.Records()[0].blinkCountdown), 1 + 3);
    CHECK_EQ(static_cast<int>(table.ColourFor(0).corner[0].r), 120);
    for (int t = 0; t < 5; ++t) { table.Tick(0); }
    CHECK_EQ(static_cast<int>(table.Records()[0].on), 0);
    CHECK_EQ(static_cast<int>(table.ColourFor(0).corner[0].r), 0);
}

// ---- texture animators (the frame programs) ---------------------------------

TEST_CASE("Texture animation: a program holds each frame for its speed, then loops")
{
    LevelGeometry level;
    level.unknownListA = { 0x00000000u };            // animator 0: program 0, no gate
    // Program 0: speed op 3 (2 ticks), frames 100 and 101, loop.
    for (uint32_t payload : { 0x8003u, 100u, 101u, 0x8001u }) { level.unknownListB.push_back(payload << 16); }
    TextureAnimator anim;
    anim.Reset(level);
    uint16_t tex = 0;
    REQUIRE(anim.CurrentTexture(0, tex));
    CHECK_EQ(static_cast<int>(tex), 100);
    std::vector<int> seen;
    for (int t = 0; t < 9; ++t) { anim.Tick(0); anim.CurrentTexture(0, tex); seen.push_back(tex); }
    // Held for two more ticks after each change, then the next frame.
    CHECK(seen == std::vector<int>({ 100, 100, 101, 101, 101, 100, 100, 100, 101 }));
}

TEST_CASE("Texture animation: a gated program waits for ChangeTexture, and op 11 stops it")
{
    LevelGeometry level;
    level.unknownListA = { 0x01000000u };            // gate: variantMax 1
    for (uint32_t payload : { 200u, 201u, 0x800bu }) { level.unknownListB.push_back(payload << 16); }
    TextureAnimator anim;
    anim.Reset(level);
    uint16_t tex = 0;
    for (int t = 0; t < 5; ++t) { anim.Tick(0); }
    REQUIRE(anim.CurrentTexture(0, tex));
    CHECK_EQ(static_cast<int>(tex), 200);            // primed, but not running
    anim.ChangeTexture(0, 1);
    for (int t = 0; t < 10; ++t) { anim.Tick(0); }
    anim.CurrentTexture(0, tex);
    CHECK_EQ(static_cast<int>(tex), 201);            // ran to the stop and stayed
    CHECK(anim.Animators()[0].stopped);
    CHECK(!anim.CurrentTexture(5, tex));
}

// ---- BND and BX ------------------------------------------------------------

TEST_CASE("BND: chunks come back in file order, by prefix, with the data they hold")
{
    const std::vector<uint8_t> bnd = {
        'F','O','R','M', 0,0,0,0x1c, 'P','S','X','T',
        'T','P','0','0', 0,0,0,2, 0xaa,0xbb,
        'B','X','0','0', 0,0,0,2, 0x00,0x00,
    };
    const auto all = BndParser::ParseAllFormSections(bnd);
    REQUIRE(all.size() == 2);
    CHECK(all[0].name == "TP00");
    CHECK(all[0].data == std::vector<uint8_t>({ 0xaa, 0xbb }));
    CHECK(all[1].name == "BX00");
    CHECK(BndParser::ParseFormSections(bnd, "BX").size() == 1);
    CHECK(BndParser::ParseFormSections(bnd, "CL").empty());
}

TEST_CASE("BX: a rect's size is stored one short, and its page is the chunk's")
{
    const std::vector<uint8_t> bx = { 2, 0,  10, 20, 31, 15, 3, 0,  0, 0, 0, 0, 1, 1 };
    const auto rects = BxParser::ParseRectangles(bx, 4);
    REQUIRE(rects.size() == 2);
    CHECK_EQ(rects[0].x, 10);
    CHECK_EQ(rects[0].y, 20);
    CHECK_EQ(rects[0].width, 32);
    CHECK_EQ(rects[0].height, 16);
    CHECK_EQ(static_cast<int>(rects[0].textureIndex), 3);
    CHECK_EQ(rects[1].page, 4);
    CHECK_EQ(static_cast<int>(rects[1].textureIndex), 0x101);
}

TEST_CASE("BND: the demo's L111 texture file has its five pages, palettes and rect lists")
{
    const auto bnd = ReadAll(AltTest::RequireDemoFile("SECT11/111GFX.BIN"));
    const auto all = BndParser::ParseAllFormSections(bnd);
    int tp = 0, cl = 0, bx = 0;
    for (const auto& s : all)
    {
        if (s.name.rfind("TP", 0) == 0) { ++tp; CHECK_EQ(static_cast<int>(s.data.size()), 256 * 256); }
        if (s.name.rfind("CL", 0) == 0) { ++cl; CHECK_EQ(static_cast<int>(s.data.size()), 516); }
        if (s.name.rfind("BX", 0) == 0) { ++bx; }
    }
    CHECK_EQ(tp, 5);
    CHECK_EQ(cl, 5);
    CHECK_EQ(bx, 5);
    const auto set = BndTextureLoader::Load(AltTest::RequireDemoFile("SECT11/111GFX.BIN"));
    CHECK_EQ(static_cast<int>(set.textures.size()), 5);
    CHECK(set.uvRects.size() > 300);
}

// ---- sprites ---------------------------------------------------------------

TEST_CASE("Sprite RLE (FUN_00028350): one to three pixels a byte, longer runs escaped")
{
    // 0x0f: pixel 3, two and one more. 0x10 0x05: pixel 4, a run of 5. 0x15:
    // pixel 5, one. Then the end (a run of zero).
    const std::vector<uint8_t> data = { 0x0f, 0x10, 0x05, 0x15, 0x00, 0x00 };
    std::vector<uint8_t> out;
    CHECK_EQ(static_cast<int>(SpriteRle::Decode(data, 0, out)), 3 + 5 + 1);
    CHECK(out == std::vector<uint8_t>({ 3, 3, 3, 4, 4, 4, 4, 4, 5 }));
}

TEST_CASE("Sprite frames (LZSS): literals, a short match, and the terminator")
{
    // Control 0xf0: four literals, then matches. 0xfc: a length-3 match four
    // back (copies two). 0x00 0x00: offset 0, the end of the frame.
    const std::vector<uint8_t> data = { 0xf0, 'A', 'B', 'C', 'D', 0xfc, 0x00, 0x00 };
    const auto [frame, consumed] = SpriteFrameDecompressor::DecompressSingleFrame(data, 0);
    CHECK(frame == std::vector<uint8_t>({ 'A', 'B', 'C', 'D', 'A', 'B' }));
    CHECK_EQ(static_cast<int>(consumed), 8);
}

TEST_CASE("Boot sequence: the intro videos come from BootSequence.json, in its order")
{
    using namespace ALTEngine::Formats::BootSequence;
    const std::string json = R"({ "steps": [
        { "order": 3, "type": "video", "path": "AVI/PRBLOGO.AVI" },
        { "order": 1, "type": "image", "path": "GFX/LEGAL" },
        { "order": 2, "type": "video", "path": "AVI/FOXDKAUD.AVI" } ] })";
    CHECK(VideosFromJson(json) == std::vector<std::string>({ "FOXDKAUD", "PRBLOGO" }));
    CHECK(VideosFromJson("not json").empty());
    CHECK(LoadVideos("/no/such/BootSequence.json") == DefaultVideos());
    // The shipped file plays the original's four, in the original's order.
    CHECK(LoadVideos(std::filesystem::path(ALT_TEST_DATA_ROOT) / "BootSequence.json") == DefaultVideos());
}

TEST_CASE("ExplosionGraphics: frames sit on their anchors at the original's size")
{
    namespace EG = ALTEngine::Formats::ExplosionGraphics;
    // The general blast (FUN_0002aaf0): an 84-pixel frame at 0xc00 over the
    // focal length 0x100 is 1008 units across, anchored at 43,43 - a pixel's
    // worth off its centre.
    const EG::FrameQuad blast = EG::QuadFor(EG::EFFECT_CRATE, 0, 84, 84);
    CHECK(std::fabs(blast.halfWidth - 504.0f) < 0.01f);
    CHECK(std::fabs(blast.centreRight - (-12.0f)) < 0.01f);
    CHECK(std::fabs(blast.centreUp - 12.0f) < 0.01f);

    // The barrel's later frames hang from an anchor near their foot, so the
    // fire stands up from where it goes off.
    const EG::FrameQuad fire = EG::QuadFor(EG::EFFECT_BARREL, 8, 80, 90);
    CHECK(fire.centreUp > 0.0f);
    CHECK(std::fabs(fire.centreUp - (81.0f - 45.0f) * (0x125c / 256.0f)) < 0.01f);

    // A frame past the end reads the last one's anchor.
    const EG::FrameQuad past = EG::QuadFor(EG::EFFECT_IMPACT, 20, 16, 16);
    CHECK(std::fabs(past.centreRight - (8.0f - 12.0f) * 8.0f) < 0.01f);
}
