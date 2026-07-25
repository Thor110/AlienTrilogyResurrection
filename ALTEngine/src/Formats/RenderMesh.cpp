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
            switch (q.flags)
            {
            case 2: // special triangle order - REVERTED (Edward, 2026):
                    // directly verified against his own toolkit's OBJ
                    // export of OPTOBJ model 9 (152 verts/118 quads,
                    // matched vertex-for-vertex and quad-for-quad). Every
                    // flags==11 quad in that model (11 of them) matched
                    // this pattern, not the "flip 180" pattern an earlier
                    // session swapped this to based on a misreading of
                    // AlienTrilogyMapLoader.cs - that swap was wrong, this
                    // is the original, correct mapping.
                uvs = { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                break;
            case 11: // flip 180 - see above; this pattern is what the
                     // 11 flags==11 quads in the reference model actually
                     // need, confirmed via 1-V-flip-adjusted comparison
                     // against the exported OBJ's own vt coordinates.
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

        // Resolves a level quad's global texIndex to (which of the 5 BX
        // groups, the rect within it) - shared by ComputeLevelQuadUvs and
        // the per-group mesh builder. Returns groupIndex=-1, rect=nullptr
        // if texIndex doesn't fall within any group (the confirmed
        // out-of-range fallback case).
        struct ResolvedLevelTexture { int groupIndex = -1; const BxRectangle* rect = nullptr; };

        ResolvedLevelTexture ResolveLevelTexture(uint16_t texIndex, const std::array<std::vector<BxRectangle>, 5>& uvGroups)
        {
            int localIndex = texIndex;
            for (size_t g = 0; g < uvGroups.size(); ++g)
            {
                if (static_cast<size_t>(localIndex) < uvGroups[g].size())
                {
                    return { static_cast<int>(g), &uvGroups[g][static_cast<size_t>(localIndex)] };
                }
                localIndex -= static_cast<int>(uvGroups[g].size());
            }
            return {};
        }

        // Level geometry's UV resolution - the texIndex resolution is
        // genuinely different from ComputeQuadUvs above (a level's
        // texIndex is a GLOBAL index across all 5 BX00-BX04 groups
        // concatenated, resolved via cumulative offset - subtract each
        // group's rect count until the index fits within a group), but
        // the FLAGS mapping is NOT different - confirmed against
        // AlienTrilogyMapLoader.cs's BuildMapGeometry (Edward, 2026):
        // levels use the identical flags==2/flags==11 mapping models do.
        // An earlier version of this function used a different mapping
        // (flags 1/5/13 special, flags==2 flip) based on a less careful
        // reading of ModelRenderer.cs - that was wrong, and was the bug
        // behind "many faces are flipped".
        std::array<Uv, 4> ComputeLevelQuadUvs(const ModelQuad& q, const std::array<std::vector<BxRectangle>, 5>& uvGroups)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvGroups);
            const BxRectangle* rect = resolved.rect;

            if (!rect)
            {
                // Confirmed real case per ExportLevel's own comment -
                // "L905LEV:6358 // this only pops for one face in one
                // level" - texIndex 0xFFFF with no matching group.
                return { Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f}, Uv{1.0f, 1.0f} };
            }

            float x0 = rect->x / TEX_SIZE;
            float y0 = rect->y / TEX_SIZE;
            float x1 = (rect->x + rect->width) / TEX_SIZE;
            float y1 = (rect->y + rect->height) / TEX_SIZE;

            std::array<Uv, 4> baseUvs = { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} }; // A, B, C, D

            switch (q.flags)
            {
            case 2: // special triangle order - REVERTED (Edward, 2026):
                    // see ComputeQuadUvs above for the full explanation -
                    // directly verified against a real OBJ export that
                    // the earlier case2/11 swap was wrong. Levels share
                    // this same convention as models (same EmitQuad code
                    // path builds both), so this reverts alongside that
                    // fix rather than staying out of sync with it.
                return { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
            case 11: // flip 180
                return { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
            default:
                return baseUvs;
            }
        }

        // Shared vertex-emission + triangulation - identical between
        // model and level geometry, only the UV computation differs.
        void EmitQuad(RenderMesh& result, const std::vector<ModelVertex>& vertices, const ModelQuad& q, const std::array<Uv, 4>& uvs)
        {
            auto emitVertex = [&](int32_t vertexIndex, Uv uv) -> uint32_t {
                if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertices.size())
                {
                    throw std::runtime_error("EmitQuad: vertex index " + std::to_string(vertexIndex) + " out of range");
                }
                const ModelVertex& v = vertices[static_cast<size_t>(vertexIndex)];
                result.vertices.push_back({
                    static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z),
                    uv.first, uv.second
                });
                return static_cast<uint32_t>(result.vertices.size() - 1);
            };

            bool isTriangle = (q.d == -1);
            uint32_t ia = emitVertex(q.a, uvs[0]);
            uint32_t ib = emitVertex(q.b, uvs[1]);
            uint32_t ic = emitVertex(q.c, uvs[2]);

            if (isTriangle)
            {
                result.indices.push_back(ia);
                result.indices.push_back(ib);
                result.indices.push_back(ic);
            }
            else
            {
                uint32_t id = emitVertex(q.d, uvs[3]);
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

    RenderMesh BuildLevelRenderMesh(const LevelGeometry& level, const std::array<std::vector<BxRectangle>, 5>& uvGroups)
    {
        RenderMesh result;
        result.vertices.reserve(level.quads.size() * 4);
        result.indices.reserve(level.quads.size() * 6);

        for (const auto& q : level.quads)
        {
            EmitQuad(result, level.vertices, q, ComputeLevelQuadUvs(q, uvGroups));
        }
        return result;
    }

    std::array<RenderMesh, 5> BuildLevelRenderMeshPerGroup(const LevelGeometry& level, const std::array<std::vector<BxRectangle>, 5>& uvGroups)
    {
        std::array<RenderMesh, 5> result;

        for (const auto& q : level.quads)
        {
            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvGroups);
            int group = (resolved.groupIndex >= 0) ? resolved.groupIndex : 0; // fallback case -> group 0, see header comment
            EmitQuad(result[static_cast<size_t>(group)], level.vertices, q, ComputeLevelQuadUvs(q, uvGroups));
        }
        return result;
    }
}
