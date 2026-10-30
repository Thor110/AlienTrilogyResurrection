#include "TestFramework.h"

#include "Formats/ObjectShatter.h"
#include "Formats/RenderMesh.h"
#include "Formats/BndTextureLoader.h"

using namespace ALTEngine::Formats;

// FUN_00045808: a pane breaks into four quads around a jittered centre, each
// with its quarter of the texture and the texture's middle on the inner corner.
TEST_CASE("Glass: a pane breaks into four quarters of its texture")
{
    const auto pieces = GlassPieces(10.0f, -20.0f, 30.0f, /*facesZ*/ true);
    int centres = 0, textureCorners = 0;
    for (const auto& p : pieces)
    {
        for (int c = 0; c < 4; ++c)
        {
            if (p.u[c] == 0.5f && p.v[c] == 0.5f)
            {
                ++centres;
                CHECK(p.x[c] == 10.0f && p.y[c] == -20.0f && p.z[c] == 30.0f);
            }
            else
            {
                CHECK(p.z[c] == 0.0f);   // the grid lies in x/y for a pane facing z
                // The texture runs left to right with x and top to bottom as y falls.
                CHECK(p.u[c] == (p.x[c] + 512.0f) / 1024.0f);
                CHECK(p.v[c] == (512.0f - p.y[c]) / 1024.0f);
            }
            if ((p.u[c] == 0.0f || p.u[c] == 1.0f) && (p.v[c] == 0.0f || p.v[c] == 1.0f)) { ++textureCorners; }
        }
    }
    CHECK_EQ(centres, 4);          // every quarter shares the middle
    CHECK_EQ(textureCorners, 4);   // and owns one corner of the texture

    // A pane facing x lies in z/y.
    for (const auto& p : GlassPieces(0.0f, 0.0f, 0.0f, false))
    {
        for (int c = 0; c < 4; ++c) { CHECK(p.x[c] == 0.0f); }
    }
}

// FUN_000367b4 / FUN_00036700: a model face is only ever routine 0, 1 or 2,
// except on OBJ3D model 0x28, the cryopod cover, where every face is routine
// 14 - a flat see-through fill, no texture. Drawn black, half over the
// background, from both sides.
TEST_CASE("Model mesh: only the cryopod cover is drawn flat see-through")
{
    CHECK(Obj3DIsFlatSeeThrough(40));
    CHECK(!Obj3DIsFlatSeeThrough(39));
    CHECK(!Obj3DIsFlatSeeThrough(0));

    ModelMesh mesh;
    mesh.vertices = { { 0, 0, 0, 0 }, { 100, 0, 0, 0 }, { 100, 100, 0, 0 }, { 0, 100, 0, 0 } };
    ModelQuad quad{};
    quad.a = 0; quad.b = 1; quad.c = 2; quad.d = 3; quad.flags = 13;
    mesh.quads = { quad };
    std::vector<BxRectangle> rects = { { 0, 0, 16, 16, 0, 0 } };

    // Any other model: drawn as it always was, whatever its face byte.
    const RenderMesh plain = BuildRenderMesh(mesh, rects);
    CHECK(plain.translucentStart == SIZE_MAX);
    CHECK_EQ(static_cast<int>(plain.indices.size()), 6);

    // The cover: all see-through, both windings, black.
    const RenderMesh cover = BuildRenderMesh(mesh, rects, /*flatSeeThrough*/ true);
    CHECK_EQ(static_cast<int>(cover.translucentStart), 0);
    CHECK_EQ(static_cast<int>(cover.indices.size()), 12);
    for (const auto& v : cover.vertices) { CHECK(v.r == 0.0f && v.g == 0.0f && v.b == 0.0f); }
}

// The fill's colour is entry 2 of CLUT slot 0 - the level's first palette.
TEST_CASE("Model mesh: the flat see-through fill takes the colour it is given")
{
    ModelMesh mesh;
    mesh.vertices = { { 0, 0, 0, 0 }, { 100, 0, 0, 0 }, { 100, 100, 0, 0 } };
    ModelQuad tri{};
    tri.a = 0; tri.b = 1; tri.c = 2; tri.d = -1;
    mesh.quads = { tri };
    std::vector<BxRectangle> rects = { { 0, 0, 1, 1, 0, 0 } };
    const RenderMesh cover = BuildRenderMesh(mesh, rects, true, { 0.25f, 0.5f, 0.75f });
    REQUIRE(!cover.vertices.empty());
    for (const auto& v : cover.vertices) { CHECK(v.r == 0.25f && v.g == 0.5f && v.b == 0.75f); }
}

TEST_CASE("Level textures: the first palette is kept for the routine 14 fill")
{
    const auto set = BndTextureLoader::Load(AltTest::RequireDemoFile("SECT11/111GFX.BIN"));
    CHECK_EQ(static_cast<int>(set.firstPaletteRgb.size()), 256 * 3);
}
