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
            case 2: // triangle special order
                uvs = { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                break;
            case 11: // flip 180
                uvs = { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
                break;
            default:
                uvs = baseUvs;
                break;
            }

            // ExportModel flips V at OBJ-write time ("1 - uv.Item2");
            // doing it here instead since these ARE the final GPU-ready
            // values, not intermediates something downstream will flip.
            for (auto& uv : uvs) { uv.second = 1.0f - uv.second; }
            return uvs;
        }
    }

    RenderMesh BuildRenderMesh(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects)
    {
        RenderMesh result;
        result.vertices.reserve(mesh.quads.size() * 4);
        result.indices.reserve(mesh.quads.size() * 6);

        auto emitVertex = [&](int32_t vertexIndex, Uv uv) -> uint32_t {
            if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= mesh.vertices.size())
            {
                throw std::runtime_error("BuildRenderMesh: vertex index " + std::to_string(vertexIndex) + " out of range");
            }
            const ModelVertex& v = mesh.vertices[static_cast<size_t>(vertexIndex)];
            result.vertices.push_back({
                static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z),
                uv.first, uv.second
            });
            return static_cast<uint32_t>(result.vertices.size() - 1);
        };

        for (const auto& q : mesh.quads)
        {
            std::array<Uv, 4> uvs = ComputeQuadUvs(q, uvRects);
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

        return result;
    }
}
