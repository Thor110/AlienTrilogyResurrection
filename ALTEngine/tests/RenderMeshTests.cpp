#include "TestFramework.h"

#include "Formats/RenderMesh.h"

using namespace ALTEngine::Formats;

// The draw-routine table at 0x000a7098, as traced (RenderMesh.h).
TEST_CASE("Draw routines: fill, culling, mirroring and the animated forms")
{
    auto fill = [](int r) { return ClassifyDrawRoutine(static_cast<uint8_t>(r)).fill; };
    CHECK(fill(0) == FaceFill::Opaque);
    CHECK(fill(1) == FaceFill::Opaque);
    CHECK(fill(2) == FaceFill::Opaque);
    CHECK(fill(4) == FaceFill::Cutout);
    CHECK(fill(6) == FaceFill::Cutout);
    CHECK(fill(12) == FaceFill::Translucent);
    CHECK(fill(30) == FaceFill::Translucent);   // breakable glass
    CHECK(fill(34) == FaceFill::Translucent);
    for (int r : { 3, 7, 9, 11, 15, 20, 25, 27, 29, 31, 33, 35, 255 })
    {
        CHECK(fill(r) == FaceFill::Hidden);
    }

    // The animated forms take their base routine's way of drawing.
    const std::pair<int, int> animated[] = { { 8, 0 }, { 10, 2 }, { 26, 4 }, { 28, 6 }, { 30, 12 }, { 32, 34 } };
    for (const auto& [a, base] : animated)
    {
        const DrawRoutine ra = ClassifyDrawRoutine(static_cast<uint8_t>(a));
        const DrawRoutine rb = ClassifyDrawRoutine(static_cast<uint8_t>(base));
        CHECK(ra.animated);
        CHECK(!rb.animated);
        CHECK(ra.fill == rb.fill);
        CHECK(ra.culled == rb.culled);
        CHECK(ra.mirrored == rb.mirrored);
    }

    CHECK(ClassifyDrawRoutine(0).culled);
    CHECK(ClassifyDrawRoutine(2).culled);
    CHECK(!ClassifyDrawRoutine(4).culled);
    CHECK(!ClassifyDrawRoutine(12).culled);
    CHECK(ClassifyDrawRoutine(2).mirrored);
    CHECK(ClassifyDrawRoutine(6).mirrored);
    CHECK(ClassifyDrawRoutine(34).mirrored);
    CHECK(!ClassifyDrawRoutine(12).mirrored);
}

namespace
{
    LevelGeometry FourFaces()
    {
        LevelGeometry level;
        for (int i = 0; i < 4; ++i) { level.vertices.push_back({ static_cast<int16_t>(i * 10), 0, static_cast<int16_t>(i & 1), 0 }); }
        auto face = [&](uint8_t routine) {
            ModelQuad q{};
            q.a = 0; q.b = 1; q.c = 2; q.d = 3;
            q.texIndex = 0;
            q.flags = routine;
            q.reserved = 0;
            level.quads.push_back(q);
        };
        face(0);    // opaque, one-sided
        face(4);    // cut-out, two-sided
        face(12);   // see-through, two-sided
        face(7);    // nothing
        return level;
    }
}

TEST_CASE("Draw routines: the level is built once per fill, hidden faces nowhere")
{
    const LevelGeometry level = FourFaces();
    const std::vector<BxRectangle> rects = { BxRectangle{ 0, 0, 64, 64, 0, 0 } };
    auto count = [&](FaceFill fill) {
        const auto meshes = BuildLevelRenderMeshPerGroup(level, rects, {}, nullptr, nullptr, fill);
        return meshes[0].indices.size();
    };
    CHECK_EQ(count(FaceFill::Opaque), size_t(6));        // one quad, one side
    CHECK_EQ(count(FaceFill::Cutout), size_t(12));       // both sides
    CHECK_EQ(count(FaceFill::Translucent), size_t(12));
    CHECK_EQ(count(FaceFill::Hidden), size_t(0));
}

TEST_CASE("Destroyed objects: the texture animators they step on")
{
    Crate c;
    c.unknown5 = 0x12; c.unknown6 = 0x01;   // first 0x112
    c.unknown7 = 3; c.unknown8 = 0;          // three of them
    CHECK_EQ(TextureStepFirst(c), 0x112);
    CHECK_EQ(TextureStepCount(c), 3);
}
