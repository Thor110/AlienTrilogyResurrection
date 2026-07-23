#pragma once

#include <cstdint>
#include <vector>

#include "BxParser.h"
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
}
