#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "BxParser.h"
#include "LevelLoader.h"
#include "ModelLoader.h"

namespace ALTEngine::Formats
{
    struct RenderVertex
    {
        float x, y, z;
        float u, v;
    };

    struct RenderMesh
    {
        std::vector<RenderVertex> vertices;
        std::vector<uint32_t> indices; // triangle list (3 per triangle)
    };

    // Converts a parsed ModelMesh + its paired BX UV rects (same section
    // index in the paired *GFX.BND, e.g. OPTOBJ's M007/Keyboard pairs
    // with OPTGFX's BX07) into a flat, GPU-ready vertex/index buffer.
    //
    // UV computation and edge cases follow ModelRenderer.cs's
    // ExportModel - texSize=256 normalization, the per-corner UV
    // mapping, the flags==2/11 special cases, and the out-of-range
    // texIndex fallback (UV (1,1) for all corners) - EXCEPT the "1 - v"
    // flip ExportModel applies at OBJ-text-export time, which is
    // specifically an .obj-format V-axis quirk (every instance in the
    // source is commented "Flip Y for OBJ"), not a property of the
    // "correct" UV values - applying it here caused upside-down/
    // scrambled texture mapping (Edward, 2026) since D3D/Vulkan/SDL_GPU
    // already treat V=0 as the texture's top row.
    //
    // The one genuinely new decision here (OBJ format has no
    // equivalent) is quad triangulation: A-B-C-D splits as triangles
    // (A,B,C) and (A,C,D) - the standard fan split, not verified against
    // how the original game itself rendered these, since OBJ export
    // never needed to make this choice.
    //
    // CONFIRMED against all 14 real OPTOBJ/OPTGFX model pairs: every
    // model converts without exceptions, every triangle index is
    // in-bounds, index counts exactly match expected triangulation, and
    // every UV lands within [0,1]. NOTE: an earlier check (100% of the
    // Keyboard's UV points landing on non-black texture content) did NOT
    // catch the V-flip bug above - a flipped V still lands on *some*
    // populated region of a densely-packed atlas, just not necessarily
    // the *correct* region for that face. That check is real evidence
    // against gross errors (huge offsets, wrong scale) but isn't
    // sufficient on its own to confirm per-face UV correctness - worth
    // remembering next time something "passes" that kind of test.
    RenderMesh BuildRenderMesh(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects);

    // Level geometry's equivalent of BuildRenderMesh above - separate
    // function specifically because ExportLevel's UV logic genuinely
    // differs from ExportModel's, confirmed by reading both in full
    // rather than assuming symmetry:
    //   - Levels resolve texIndex against FIVE separate BX00-BX04 groups
    //     via a cumulative global offset (subtract each group's rect
    //     count from the index until it fits within a group), not a
    //     single group like OPTOBJ models use.
    //   - The per-quad flags meaning is DIFFERENT: for levels, flags
    //     1/5/13 mean "triangle special order" and flags 2 means "flip
    //     180" - the inverse of ExportModel's mapping (flags 2 = triangle
    //     special, flags 11 = flip 180 there). Reusing BuildRenderMesh's
    //     logic here would have silently misinterpreted these.
    //   - Same out-of-range fallback (UV (1,1)) and same "no extra V
    //     flip" correction as BuildRenderMesh - both confirmed via the
    //     same "Flip Y for OBJ" comment pattern in ExportLevel.
    //
    // CONFIRMED against real level data (L111LEV.MAP + its paired
    // 111GFX.B16, Edward, 2026): 111GFX.B16 has exactly 5 TP/CL/BX
    // sections as expected, and running the full 22,242-triangle level
    // through this function produced index counts exactly matching
    // expected triangulation, zero out-of-bounds indices, zero UVs
    // outside [0,1], and - the precise test, not just "lands on
    // non-black content" - 44,770/44,770 (100%) vertex UVs land within
    // their quad's exact, correctly-resolved rect bounds, verified by
    // independently replicating the cumulative-offset group resolution
    // and checking each vertex against it. No code changes were needed
    // to pass this - unlike BuildRenderMesh's UV flip bug, which only
    // surfaced once real model data was available, this one was correct
    // on the first real-data test.
    RenderMesh BuildLevelRenderMesh(const LevelGeometry& level, const std::array<std::vector<BxRectangle>, 5>& uvGroups);
}

