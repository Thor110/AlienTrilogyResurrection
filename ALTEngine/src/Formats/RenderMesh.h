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
    // UV computation and edge cases are replicated exactly from
    // ModelRenderer.cs's ExportModel - texSize=256 normalization, the
    // per-corner UV mapping, the flags==2/11 special cases, and the
    // out-of-range texIndex fallback (UV (1,1) for all corners) - not
    // reinvented. The one genuinely new decision here (OBJ format has no
    // equivalent) is quad triangulation: A-B-C-D splits as triangles
    // (A,B,C) and (A,C,D) - the standard fan split, not verified against
    // how the original game itself rendered these, since OBJ export
    // never needed to make this choice.
    // CONFIRMED against all 14 real OPTOBJ/OPTGFX model pairs (Edward,
    // 2026): every model converts without exceptions, every triangle
    // index is in-bounds, index counts exactly match expected
    // triangulation, and every UV lands within [0,1] - and, checked
    // against the Keyboard model's actual decoded texture specifically,
    // 100% of its UV points land on real texture content (0% on the
    // atlas's black/empty padding), which would be an unlikely outcome
    // if the UV math were wrong (flipped/offset/scaled incorrectly).
    RenderMesh BuildRenderMesh(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects);
}
