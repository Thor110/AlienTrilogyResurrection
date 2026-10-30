#include "TestFramework.h"

#include "Formats/ObjectShatter.h"

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
