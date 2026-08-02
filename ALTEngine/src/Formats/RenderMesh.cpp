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

            // Models use case 2/130 and 11/139 - a different mapping
            // from level geometry (see ComputeLevelQuadUvs below).
            //
            // No "1 - v" flip here: ModelRenderer.cs applies one, but
            // only at OBJ-export time ("Flip Y for OBJ"), a quirk of the
            // .obj V-axis convention. SDL_GPU already treats v=0 as the
            // texture's top row, which is what these values encode.
            switch (q.flags)
            {
            case 2:
            case 130:
                return { Uv{x0, y1}, Uv{x1, y0}, Uv{x0, y0}, Uv{x0, y0} };
            case 11:
            case 139:
                return { Uv{x1, y1}, Uv{x0, y1}, Uv{x0, y0}, Uv{x1, y0} };
            default:
                return { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} };
            }
        }

        // Resolves a level quad's texIndex to its UV rect. texIndex is a
        // direct, flat, global index into one descriptor array built by
        // appending every BX chunk's rects in file order (Ghidra) - each
        // descriptor carries its own page, stamped at load time from its
        // BX chunk's tag digits. rect=nullptr if texIndex is out of range.
        struct ResolvedLevelTexture { int groupIndex = -1; const BxRectangle* rect = nullptr; };

        ResolvedLevelTexture ResolveLevelTexture(uint16_t texIndex, const std::vector<BxRectangle>& uvRects)
        {
            if (static_cast<size_t>(texIndex) >= uvRects.size()) { return {}; }
            const BxRectangle& rect = uvRects[texIndex];
            return { rect.page, &rect };
        }

        // Level geometry's UV resolution. texIndex resolution differs
        // from ComputeQuadUvs above (flat global index - see
        // ResolveLevelTexture); the flags mapping differs too.
        //
        // Corner values are written out literally per case rather than
        // permuting a shared base array. Edward established these
        // against his own tool with the level's faces colour-coded by
        // flag in Blender - every face except lights (flag 8) and
        // breakable glass now matches.
        std::array<Uv, 4> ComputeLevelQuadUvs(const ModelQuad& q, const std::vector<BxRectangle>& uvRects)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvRects);
            const BxRectangle* rect = resolved.rect;

            if (!rect)
            {
                // Real case per ExportLevel's own comment -
                // "L905LEV:6358 // this only pops for one face in one
                // level" - texIndex 0xFFFF with no matching rect.
                return { Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f} };
            }

            float x0 = rect->x / TEX_SIZE;
            float y0 = rect->y / TEX_SIZE;
            float x1 = (rect->x + rect->width) / TEX_SIZE;
            float y1 = (rect->y + rect->height) / TEX_SIZE;

            switch (q.flags)
            {
            case 11: // triangle: fourth corner repeats the third
                return { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x1, y0} };
            case 2:  // mirrored horizontally, right way up
                return { Uv{x1, y1}, Uv{x0, y1}, Uv{x0, y0}, Uv{x1, y0} };
            default:
                return { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} };
            }
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

            // Genuine flag-1 triangles need their UVs rotated one step
            // clockwise (ABC -> CAB). Excludes the degenerate-quad
            // fallback above, which already forces isTriangle = false.
            bool rotateTriangleUvs = isTriangle && (q.flags == 1);
            Uv uvA = rotateTriangleUvs ? effectiveUvs[2] : effectiveUvs[0];
            Uv uvB = rotateTriangleUvs ? effectiveUvs[0] : effectiveUvs[1];
            Uv uvC = rotateTriangleUvs ? effectiveUvs[1] : effectiveUvs[2];

            uint32_t ia = emitVertex(a, uvA);
            uint32_t ib = emitVertex(b, uvB);
            uint32_t ic = emitVertex(c, uvC);

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

    std::array<RenderMesh, 5> BuildRenderMeshPerGroup(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects)
    {
        std::array<RenderMesh, 5> result;

        for (const auto& q : mesh.quads)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvRects);
            int group = (resolved.groupIndex >= 0 && resolved.groupIndex < 5) ? resolved.groupIndex : 0;
            EmitQuad(result[static_cast<size_t>(group)], mesh.vertices, q, ComputeQuadUvs(q, uvRects));
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
