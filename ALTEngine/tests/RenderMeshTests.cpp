#include "TestFramework.h"

#include "Formats/BndTextureLoader.h"
#include "Formats/LevelLoader.h"
#include "Formats/RenderMesh.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

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

TEST_CASE("Shatter faces: every other face, never the last, routines 0 and 2 only")
{
    // FUN_00046ffc takes faces 0, 2, 4 ... and never the last; FUN_000464ec
    // only routines 0 and 2. The old six-indices-at-a-time walk also lost step
    // after a triangle, so one sits at face 1 here.
    ModelMesh mesh;
    mesh.vertices = {
        { 0, 0, 0, 0 }, { 100, 0, 0, 0 }, { 100, 100, 0, 0 }, { 0, 100, 0, 0 },      // quad in z = 0
        { 500, 0, 0, 0 }, { 540, 0, 0, 0 }, { 500, 0, 60, 0 },                       // triangle in y = 0
        { 0, 0, 1000, 0 }, { 0, 200, 1000, 0 }, { 0, 200, 1300, 0 }, { 0, 0, 1300, 0 }, // quad in x = 0
    };
    auto quad = [](int a, int b, int c, int d, uint8_t flags) { ModelQuad q{}; q.a = a; q.b = b; q.c = c; q.d = d; q.flags = flags; return q; };
    mesh.quads = { quad(0, 1, 2, 3, 0), quad(4, 5, 6, -1, 2), quad(7, 8, 9, 10, 0), quad(0, 1, 2, 3, 0),
                   quad(7, 8, 9, 10, 4), quad(0, 1, 2, 3, 0), quad(4, 5, 6, -1, 0) };

    const RenderMesh render = BuildRenderMesh(mesh, {});
    const auto faces = ExtractMeshFaces(render, mesh);
    // Faces 0 and 2 shatter; 4 is routine 4; 6 is the last; the odd ones are stepped over.
    REQUIRE(faces.size() == size_t(2));

    CHECK(std::fabs(faces[0].cx - 50.0f) < 0.01f);
    CHECK(std::fabs(faces[0].halfWidth - 50.0f) < 0.01f);
    CHECK(std::fabs(faces[1].cz - 1150.0f) < 0.01f);
    CHECK(std::fabs(faces[1].halfWidth - 150.0f) < 0.01f);
    CHECK(std::fabs(faces[1].halfHeight - 100.0f) < 0.01f);

    // The real corners, in order.
    CHECK(std::fabs(faces[0].px[1] - 100.0f) < 0.01f);
    CHECK(std::fabs(faces[0].py[2] - 100.0f) < 0.01f);
    CHECK(std::fabs(faces[1].pz[3] - 1300.0f) < 0.01f);
    CHECK(std::fabs(faces[1].py[1] - 200.0f) < 0.01f);
}

TEST_CASE("Level triangles: a, b, c take the texture's x0y1, x1y0, x0y0")
{
    // Draw routine 1 (0x1b5c8) and the quad routine (0x19af0), traced: the
    // descriptor's corners run x0y0, x1y0, x0y1, x1y1, and a triangle puts the
    // first three on c, b and a.
    LevelGeometry level;
    level.vertices = { { 0, 0, 0, 0 }, { 512, 0, 0, 0 }, { 0, 512, 0, 0 } };
    ModelQuad q{}; q.a = 0; q.b = 1; q.c = 2; q.d = -1; q.texIndex = 0; q.flags = 1;
    level.quads = { q };
    const std::vector<BxRectangle> rects = { BxRectangle{ 64, 32, 64, 32, 0, 0 } };
    const RenderMesh mesh = BuildLevelRenderMesh(level, rects);
    REQUIRE(mesh.vertices.size() >= size_t(3));
    const float x0 = 64 / 256.0f, x1 = 128 / 256.0f, y0 = 32 / 256.0f, y1 = 64 / 256.0f;
    CHECK(std::fabs(mesh.vertices[0].u - x0) < 1e-5f); CHECK(std::fabs(mesh.vertices[0].v - y1) < 1e-5f);
    CHECK(std::fabs(mesh.vertices[1].u - x1) < 1e-5f); CHECK(std::fabs(mesh.vertices[1].v - y0) < 1e-5f);
    CHECK(std::fabs(mesh.vertices[2].u - x0) < 1e-5f); CHECK(std::fabs(mesh.vertices[2].v - y0) < 1e-5f);
}

TEST_CASE("Animated faces: rebuilding just their runs gives the whole level's build, frame after frame")
{
    const auto bytes = [] {
        std::ifstream in(AltTest::RequireDemoFile("SECT11/L112LEV.MAP"), std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }();
    const auto level = ALTEngine::Formats::LevelLoader::Parse(bytes, "L112");
    const auto set = ALTEngine::Formats::BndTextureLoader::LoadLevelPages(AltTest::RequireDemoFile("SECT11/112GFX.BIN"));
    ALTEngine::Formats::LightTable lights;
    lights.Reset(level.lights);
    ALTEngine::Formats::TextureAnimator animator;
    animator.Reset(level);
    REQUIRE(animator.Count() > 0);

    auto kept = BuildLevelRenderMeshAllFills(level, set.uvRects, {}, &lights, &animator);
    size_t runs = 0;
    for (const auto& fill : kept) { for (const auto& page : fill) { runs += page.animatedFaces.size(); } }
    REQUIRE(runs > 0);

    int patched = 0;
    for (int tick = 0; tick < 400; ++tick)
    {
        animator.Tick(tick * 7919);
        lights.Tick(tick * 104729);
        const auto full = BuildLevelRenderMeshAllFills(level, set.uvRects, {}, &lights, &animator);
        bool rebuild = false;   // what the renderer does when a run cannot be patched
        for (size_t p = 0; p < 3; ++p)
        {
            for (size_t g = 0; g < 5; ++g)
            {
                auto& mesh = kept[p][g];
                // Lights: the kept colours follow the table, as the renderer's recolour does.
                for (size_t v = 0; v < mesh.vertices.size(); ++v)
                {
                    if (mesh.vertexLight[v] == RenderMesh::NO_LIGHT) { continue; }
                    const auto& c = lights.ColourFor(mesh.vertexLight[v]).corner[mesh.vertexCorner[v] & 3];
                    mesh.vertices[v].r = static_cast<float>(c.r) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                    mesh.vertices[v].g = static_cast<float>(c.g) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                    mesh.vertices[v].b = static_cast<float>(c.b) / ALTEngine::Formats::LIGHT_COLOUR_NEUTRAL;
                }
                for (const auto& run : mesh.animatedFaces)
                {
                    LevelFaceVertices face;
                    REQUIRE(BuildLevelFaceVertices(level, set.uvRects, {}, &lights, &animator, run.face, face));
                    if (face.slot != int(p) || face.page != int(g)) { rebuild = true; continue; }
                    REQUIRE(face.mesh.vertices.size() == run.count);
                    std::copy(face.mesh.vertices.begin(), face.mesh.vertices.end(), mesh.vertices.begin() + run.first);
                    ++patched;
                }
                if (full[p][g].vertices.size() != mesh.vertices.size()) { rebuild = true; continue; }
                bool same = true;
                for (size_t v = 0; v < mesh.vertices.size() && same; ++v)
                {
                    const auto& a = mesh.vertices[v];
                    const auto& b = full[p][g].vertices[v];
                    same = a.x == b.x && a.y == b.y && a.z == b.z && a.u == b.u && a.v == b.v && a.r == b.r && a.g == b.g && a.b == b.b;
                }
                if (!rebuild) { CHECK(same); }
            }
        }
        if (rebuild) { kept = full; }
    }
    CHECK(patched > 0);
}
