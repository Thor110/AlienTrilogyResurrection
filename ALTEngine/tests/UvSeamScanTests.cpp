#include "TestFramework.h"

#include "Formats/RenderMesh.h"
#include "Formats/UvSeamScan.h"

using namespace ALTEngine::Formats;

namespace
{
    // Three quads in a row along x on the floor, showing a picture cut in
    // three: descriptors 0, 1 and 2 side by side on page 0.
    struct Strip
    {
        LevelGeometry level;
        BndTextureSet textures;
        Strip()
        {
            for (int i = 0; i <= 3; ++i)
            {
                level.vertices.push_back({ static_cast<int16_t>(i * 512), 0, 0, 0 });
                level.vertices.push_back({ static_cast<int16_t>(i * 512), 0, 512, 0 });
            }
            for (int i = 0; i < 3; ++i)
            {
                ModelQuad q{};
                // Laid so that routine 0's corners (BL, BR, TR, TL) run the
                // picture on from one face to the next.
                q.a = i * 2 + 1; q.b = i * 2 + 3; q.c = i * 2 + 2; q.d = i * 2;
                q.texIndex = static_cast<uint16_t>(i);
                q.flags = 0;
                level.quads.push_back(q);
                BxRectangle r{};
                r.x = i * 32; r.y = 0; r.width = 32; r.height = 32; r.page = 0;
                textures.uvRects.push_back(r);
            }
            BndTexture page;
            page.index = "00";
            page.rgba.assign(256 * 256 * 4, 255);
            textures.textures.push_back(page);
        }
    };
}

TEST_CASE("UV scan: a picture whose pieces run on has nothing to report")
{
    Strip s;
    // Check the layout first: the shared vertices land on the same texels.
    const auto left = LevelFaceUvs(s.level.quads[0], 0, s.textures.uvRects);
    const auto mid = LevelFaceUvs(s.level.quads[1], 1, s.textures.uvRects);
    REQUIRE(left[1] == mid[0]);   // vertex 3
    REQUIRE(left[2] == mid[3]);   // vertex 2
    UvSeamOptions o;
    o.minEdges = 2;
    CHECK(ScanLevelUvSeams(s.level, s.textures, o).empty());
}

TEST_CASE("UV scan: a piece turned the wrong way is named, with the turn that puts it right")
{
    Strip s;
    // Turn the middle face's corners round by two: on disk that is the
    // vertex list started from its third corner.
    ModelQuad& q = s.level.quads[1];
    const ModelQuad was = q;
    q.a = was.c; q.b = was.d; q.c = was.a; q.d = was.b;
    const auto found = ScanLevelUvSeams(s.level, s.textures, {});
    REQUIRE(found.size() == 1);
    CHECK_EQ(found[0].face, 1);
    CHECK(!found[0].flip);
    CHECK_EQ(found[0].steps, 2);
    CHECK(found[0].bestCost < 0.01f);
    CHECK(found[0].currentCost >= 4.0f);

    // With that correction already in the patch list it is quiet.
    UvSeamOptions patched;
    FaceUvRotation r;
    r.vertices = { q.a, q.b, q.c, q.d };
    r.steps = 2;
    patched.applied.push_back(r);
    CHECK(ScanLevelUvSeams(s.level, s.textures, patched).empty());
}
