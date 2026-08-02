#include "RenderMesh.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        constexpr float TEX_SIZE = 256.0f; // matches ModelRenderer.cs's texSize constant

        using Uv = std::pair<float, float>;

        std::array<Uv, 4> ComputeQuadUvs(const ModelQuad& q, const std::vector<BxRectangle>& uvRects)
        {
            // Out-of-range texIndex is a real, expected case in the
            // original data (confirmed against OPTOBJ.BND/OPTGFX.BND -
            // some quads legitimately reference an index past the end of
            // their BX rect list) - ModelRenderer.cs falls back to UV
            // (1,1) for every corner rather than treating it as an error.
            if (q.texIndex >= uvRects.size())
            {
                return { Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f} };
            }

            const BxRectangle& rect = uvRects[q.texIndex];
            float x0 = rect.x / TEX_SIZE;
            float y0 = rect.y / TEX_SIZE;
            float x1 = (rect.x + rect.width) / TEX_SIZE;
            float y1 = (rect.y + rect.height) / TEX_SIZE;

            std::array<Uv, 4> baseUvs = { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} }; // A, B, C, D

            std::array<Uv, 4> uvs;
            // Matches ExportModel's own switch exactly (Edward's
            // ModelRenderer.cs, 2026) - models use case 2/130 for
            // special-triangle and case 11/139 for flip-180. This is a
            // genuinely different mapping from levels (see
            // ComputeLevelQuadUvs below) - confirmed directly from the
            // authoritative source, not inferred from a bit-mask that
            // happened to produce the same result for 128/130 by
            // coincidence.
            switch (q.flags)
            {
            case 2:
            case 130:
                uvs = { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                break;
            case 11:
            case 139:
                uvs = { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
                break;
            default:
                uvs = baseUvs;
                break;
            }

            // NOTE: ModelRenderer.cs applies a "1 - v" flip here, but
            // only at OBJ-text-export time - every instance of it in the
            // source is explicitly commented "Flip Y for OBJ", meaning
            // it's a quirk of the .obj file format's V-axis convention,
            // not a property of the "correct" UV values themselves.
            // D3D/Vulkan/SDL_GPU already treat V=0 as the texture's top
            // row, which is exactly what baseUvs already encodes
            // (rect.y/TEX_SIZE = top of the rect in image space) - no
            // flip needed here. Applying the OBJ-specific flip anyway
            // was the bug behind the upside-down/scrambled texture
            // mapping (Edward, 2026).
            return uvs;
        }

        // Resolves a level quad's global texIndex to its UV rect,
        // directly - shared by ComputeLevelQuadUvs and the per-group
        // mesh builder. Returns rect=nullptr if texIndex is out of range
        // (the confirmed out-of-range fallback case).
        //
        // Confirmed via Ghidra decompilation (Edward, 2026 - full
        // deep-dive into the actual executable): texIndex is a direct,
        // flat, global index into ONE descriptor array built by
        // appending every BX chunk's rects in file order - there is NO
        // per-group/cumulative-subtraction resolution at all. Each
        // descriptor's own TPage field (stamped at load time from its
        // BX chunk's own tag digits, e.g. "BX00" -> page 0) is what
        // actually selects which texture it belongs to, not its position
        // in the array. The earlier version of this function walked 5
        // separate groups, subtracting each group's own rect count until
        // the index fit inside one - an accidental equivalent that only
        // produces the right page while BX chunks happen to appear in
        // ascending tag order with exactly one chunk per page. That was
        // the confirmed root cause of "many faces throughout the level
        // are mapped to the wrong texture" (Edward, 2026).
        struct ResolvedLevelTexture { int groupIndex = -1; const BxRectangle* rect = nullptr; };

        ResolvedLevelTexture ResolveLevelTexture(uint16_t texIndex, const std::vector<BxRectangle>& uvRects)
        {
            if (static_cast<size_t>(texIndex) >= uvRects.size()) { return {}; }
            const BxRectangle& rect = uvRects[texIndex];
            return { rect.page, &rect };
        }

        // Level geometry's UV resolution - the texIndex resolution is
        // genuinely different from ComputeQuadUvs above (a level's
        // texIndex is a direct, flat, global index into one descriptor
        // array spanning all BX chunks - see ResolveLevelTexture above),
        // but the FLAGS mapping is NOT different - confirmed against
        // AlienTrilogyMapLoader.cs's BuildMapGeometry (Edward, 2026):
        // levels use the identical flags==2/flags==11 mapping models do.
        // An earlier version of this function used a different mapping
        // (flags 1/5/13 special, flags==2 flip) based on a less careful
        // reading of ModelRenderer.cs - that was wrong, and was the bug
        // behind "many faces are flipped".
        std::array<Uv, 4> ComputeLevelQuadUvs(const ModelQuad& q, const std::vector<BxRectangle>& uvRects)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvRects);
            const BxRectangle* rect = resolved.rect;

            if (!rect)
            {
                // Confirmed real case per ExportLevel's own comment -
                // "L905LEV:6358 // this only pops for one face in one
                // level" - texIndex 0xFFFF with no matching rect.
                return { Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f} };
            }

            float x0 = rect->x / TEX_SIZE;
            float y0 = rect->y / TEX_SIZE;
            float x1 = (rect->x + rect->width) / TEX_SIZE;
            float y1 = (rect->y + rect->height) / TEX_SIZE;

            std::array<Uv, 4> baseUvs = { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} }; // A, B, C, D

            // NO per-flags UV modification (Edward, 2026 - Ghidra
            // deep-dive, full statistical verification against real
            // L111LEV data): the byte at quad offset 0x12 (`q.flags`) is
            // NOT a UV-orientation flag. Four independent tests ruled
            // this out: no correlation with quad winding, IDENTICAL
            // height distribution between flags==0 and flags==2, and -
            // the decisive one - 82 of 201 textures in this level appear
            // paired with more than one flags value, meaning it varies
            // independently of which texture/orientation a quad actually
            // has. It's confirmed instead to be the rasterizer's RGBC
            // selector (vertex colour + PSX semi-transparency/blend bit),
            // baked from a small lookup table, not read from the
            // descriptor at all.
            //
            // TWO earlier schemes were tried here based on that same
            // mistaken "this byte is orientation" premise - the C#
            // reference's own flags==2/11 mapping, and a later
            // flags==1/5/13/2 revision built from comparing against an
            // OBJ export. Both are removed; this was confirmed as
            // exactly the reported bug's signature ("tonnes of ceiling
            // tiles... some other tiles here and there") - flags==2
            // alone affected 1,902 quads (17% of the level), 1,462 of
            // them horizontal (ceiling/floor-facing).
            //
            // Real UV orientation almost certainly comes from vertex
            // winding order (which of a quad's 4 vertex indices maps to
            // which screen corner) rather than any flag byte - not yet
            // confirmed, still open. If ceilings are now merely
            // mis-oriented rather than showing a wrong image entirely,
            // that confirms this diagnosis and narrows the remaining
            // work to vertex order, not texture/UV selection.
            return baseUvs;
        }

        // Shared vertex-emission + triangulation - identical between
        // model and level geometry, only the UV computation differs.
        void EmitQuad(RenderMesh& result, const std::vector<ModelVertex>& vertices, const ModelQuad& q, const std::array<Uv, 4>& uvs)
        {
            auto inBounds = [&](int32_t vertexIndex) {
                return vertexIndex >= 0 && static_cast<size_t>(vertexIndex) < vertices.size();
            };

            // Any of a/b/c/d out of range (not just d==-1, the intentional
            // triangle marker) is handled the same way
            // AlienTrilogyMapLoader.cs's own "issueFound" does: fall back
            // to a degenerate triangle (d=a) with the special-triangle UV
            // pattern, rather than throwing - confirmed directly from the
            // authoritative source (Edward, 2026). This is a genuinely
            // different situation from the normal d==-1 case (an
            // intentional triangle marker, not an error).
            int32_t a = q.a, b = q.b, c = q.c, d = q.d;
            bool isTriangle = (d == -1);
            std::array<Uv, 4> effectiveUvs = uvs;
            if (!inBounds(a) || !inBounds(b) || !inBounds(c) || (!isTriangle && !inBounds(d)))
            {
                d = a;
                isTriangle = false; // still emits as a (degenerate) quad, matching the reference
                effectiveUvs = { uvs[0], uvs[2], uvs[3], uvs[3] };
                if (!inBounds(a) || !inBounds(b) || !inBounds(c))
                {
                    return; // a/b/c themselves invalid - nothing sane to emit at all
                }
            }

            auto emitVertex = [&](int32_t vertexIndex, Uv uv) -> uint32_t {
                const ModelVertex& v = vertices[static_cast<size_t>(vertexIndex)];
                result.vertices.push_back({
                    static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z),
                    uv.first, uv.second
                });
                return static_cast<uint32_t>(result.vertices.size() - 1);
            };

            uint32_t ia = emitVertex(a, effectiveUvs[0]);
            uint32_t ib = emitVertex(b, effectiveUvs[1]);
            uint32_t ic = emitVertex(c, effectiveUvs[2]);

            if (isTriangle)
            {
                result.indices.push_back(ia);
                result.indices.push_back(ib);
                result.indices.push_back(ic);
            }
            else
            {
                uint32_t id = emitVertex(d, effectiveUvs[3]);
                // Fan triangulation (A,B,C) + (A,C,D) - the one thing
                // here with no OBJ-format precedent to follow exactly,
                // since OBJ keeps quads as quads. Standard split, but
                // not verified against the original renderer's own
                // choice.
                result.indices.push_back(ia);
                result.indices.push_back(ib);
                result.indices.push_back(ic);
                result.indices.push_back(ia);
                result.indices.push_back(ic);
                result.indices.push_back(id);
            }
        }
    }

    RenderMesh BuildRenderMesh(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects)
    {
        RenderMesh result;
        result.vertices.reserve(mesh.quads.size() * 4);
        result.indices.reserve(mesh.quads.size() * 6);

        for (const auto& q : mesh.quads)
        {
            EmitQuad(result, mesh.vertices, q, ComputeQuadUvs(q, uvRects));
        }
        return result;
    }

    RenderMesh BuildLevelRenderMesh(const LevelGeometry& level, const std::vector<BxRectangle>& uvRects)
    {
        RenderMesh result;
        result.vertices.reserve(level.quads.size() * 4);
        result.indices.reserve(level.quads.size() * 6);

        for (const auto& q : level.quads)
        {
            EmitQuad(result, level.vertices, q, ComputeLevelQuadUvs(q, uvRects));
        }
        return result;
    }

    std::array<RenderMesh, 5> BuildLevelRenderMeshPerGroup(const LevelGeometry& level, const std::vector<BxRectangle>& uvRects)
    {
        std::array<RenderMesh, 5> result;

        for (const auto& q : level.quads)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvRects);
            // Each descriptor's own page field (0-4, stamped at load time
            // from its BX chunk's own tag digits) selects the output
            // group directly - not a computed/cumulative index. Fallback
            // (out-of-range texIndex) -> group 0, see header comment.
            int group = (resolved.groupIndex >= 0 && resolved.groupIndex < 5) ? resolved.groupIndex : 0;
            EmitQuad(result[static_cast<size_t>(group)], level.vertices, q, ComputeLevelQuadUvs(q, uvRects));
        }
        return result;
    }
}
