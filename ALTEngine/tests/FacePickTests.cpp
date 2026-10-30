#include "TestFramework.h"

#include "Formats/FacePick.h"

#include <cmath>
#include <utility>

using namespace ALTEngine::Formats;

namespace
{
    // A 200-unit square at z = `z`, wound so its front faces +z (towards a
    // camera at the origin looking down -z), or the other way if `flipped`.
    void AddSquare(LevelGeometry& level, int z, uint8_t routine, bool flipped = false)
    {
        const int32_t base = static_cast<int32_t>(level.vertices.size());
        const int16_t zz = static_cast<int16_t>(z);
        level.vertices.push_back({ -100, -100, zz, 0 });
        level.vertices.push_back({ 100, -100, zz, 0 });
        level.vertices.push_back({ 100, 100, zz, 0 });
        level.vertices.push_back({ -100, 100, zz, 0 });
        ModelQuad q;
        q.a = base; q.b = base + 1; q.c = base + 2; q.d = base + 3;
        if (flipped) { std::swap(q.b, q.d); }
        q.flags = routine;
        level.quads.push_back(q);
    }
}

TEST_CASE("Face pick: the nearest drawn face along the ray, by its index")
{
    LevelGeometry level;
    AddSquare(level, -2000, 0);
    AddSquare(level, -1000, 0);
    const auto hit = PickLevelFace(level, 0, 0, 0, 0, 0, -1);
    CHECK(hit.face == 1);
    CHECK(std::fabs(hit.distance - 1000.0f) < 0.5f);

    // Off to the side: nothing.
    CHECK(PickLevelFace(level, 500, 0, 0, 0, 0, -1).face == -1);
    // Behind the camera: nothing.
    CHECK(PickLevelFace(level, 0, 0, 0, 0, 0, 1).face == -1);
}

TEST_CASE("Face pick: sees through the back of a culled face, not a double-sided one")
{
    LevelGeometry level;
    AddSquare(level, -2000, 0);
    AddSquare(level, -1000, 1, /*flipped=*/true);   // routine 1 culls: its back faces us
    CHECK(PickLevelFace(level, 0, 0, 0, 0, 0, -1).face == 0);

    level.quads[1].flags = 5;                       // routine 5 does not cull
    CHECK(PickLevelFace(level, 0, 0, 0, 0, 0, -1).face == 1);

    level.quads[1].flags = 3;                       // routine 3 draws nothing
    CHECK(PickLevelFace(level, 0, 0, 0, 0, 0, -1).face == 0);
}

TEST_CASE("Face pick: triangles, and a face's file offset")
{
    LevelGeometry level;
    AddSquare(level, -1000, 1);
    level.quads[0].d = -1;                          // triangle a, b, c: the lower right half
    CHECK(PickLevelFace(level, 50, -50, 0, 0, 0, -1).face == 0);
    CHECK(PickLevelFace(level, -50, 50, 0, 0, 0, -1).face == -1);

    // Face n's record: 0x14 (MAP0's data) + the first record + 20 each.
    level.faceRecordsPos = 100;
    CHECK(LevelFaceFileOffset(level, 0) == 0x14 + 100);
    CHECK(LevelFaceFileOffset(level, 3) == 0x14 + 100 + 60);
}
