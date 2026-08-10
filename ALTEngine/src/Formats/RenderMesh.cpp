#include "RenderMesh.h"

#include <algorithm>

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
        // True when a patch entry names the same four (or three) vertices as
        // this face, in any order. Triangles carry -1 in the fourth slot on
        // both sides, so they compare correctly without a special case.
        bool SameVertexSet(const std::array<int32_t, 4>& patch, const ModelQuad& q)
        {
            std::array<int32_t, 4> a = patch;
            std::array<int32_t, 4> b = { q.a, q.b, q.c, q.d };
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            return a == b;
        }

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

            // TRIANGLES. Draw routines 1 and 3 are the triangle rasterizers -
            // in L111 they are the ONLY flag values that ever produce a
            // three-cornered face, and 0/2/4/8 are exclusively quads:
            //
            //   flag |  d == -1  |  quads
            //      0 |      0    |   8755
            //      1 |    285    |      0
            //      2 |      0    |   1902
            //      3 |      1    |      0
            //      4 |      0    |    171
            //      8 |      0    |    150
            //
            // A triangle needs three of the descriptor's four UV corners, and
            // WHICH three is not a free choice. The runtime descriptor built
            // by FUN_00018bcc lays its four corners out in Z order, not
            // winding order:
            //
            //   corner 0 = (x,     y    )   top-left
            //   corner 1 = (x + w, y    )   top-right
            //   corner 2 = (x,     y + h)   bottom-left
            //   corner 3 = (x + w, y + h)   bottom-right
            //
            // so the triangle routine's three corners are top-left,
            // top-right, bottom-left.
            //
            // What was here before rotated the WINDING order by one step and
            // came out with top-right, bottom-left, bottom-right - the wrong
            // three corners entirely, including bottom-right and omitting
            // top-left. That is why corner faces at hallway intersections
            // showed the wrong part of their tile (Edward, 2026). The old
            // rotation came from ModelRenderer.cs's OBJ exporter, which is
            // Edward's own tool rather than the game, and OBJ export never had
            // to agree with the original rasterizer about corner ordering.
            if (q.d == -1)
            {
                // Fourth entry repeats the third; nothing reads it for a
                // triangle, but leaving it defined keeps the array honest.
                return { Uv{x0, y0}, Uv{x1, y0}, Uv{x0, y1}, Uv{x0, y1} };
            }

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
        // Four corner colours, 0-1. All four are equal except for light
        // mode 4 (gouraud). WHITE is the neutral value - a face with no
        // light record resolved renders exactly as it did before colour
        // existed, so models and doors are unaffected.
        struct QuadColours { float r[4]{1,1,1,1}, g[4]{1,1,1,1}, b[4]{1,1,1,1}; };

        QuadColours WhiteColours() { return QuadColours{}; }

        // A flag-8 face's texIndex is an ANIMATOR ORDINAL, not a descriptor
        // index - see TextureAnimation.h for the evidence. Resolve it to the
        // descriptor the animator is currently outputting. Anything else
        // passes through untouched.
        uint16_t ResolveAnimatedTexIndex(const ModelQuad& q, const TextureAnimator* animator)
        {
            if (!animator || q.flags != DRAW_ROUTINE_ANIMATED) { return q.texIndex; }
            uint16_t resolved = 0;
            if (!animator->CurrentTexture(static_cast<int>(q.texIndex), resolved)) { return q.texIndex; }
            return resolved;
        }

        QuadColours LevelQuadColours(const ModelQuad& q, const LightTable* lights)
        {
            QuadColours out;
            if (!lights) { return out; }

            // q.reserved is the on-disk +0x13 byte - the light id. NOT
            // q.flags. See the long note in LightTable.h.
            const LightTable::Entry& e = lights->ColourFor(q.reserved);
            for (int i = 0; i < 4; ++i)
            {
                out.r[i] = static_cast<float>(e.corner[i].r) / LIGHT_COLOUR_NEUTRAL;
                out.g[i] = static_cast<float>(e.corner[i].g) / LIGHT_COLOUR_NEUTRAL;
                out.b[i] = static_cast<float>(e.corner[i].b) / LIGHT_COLOUR_NEUTRAL;
            }
            return out;
        }

        // `uvsAlreadyOrdered` means uvs[0..2] are already the correct
        // per-vertex UVs for a triangle and must not be permuted again. The
        // level path sets it (ComputeLevelQuadUvs now handles triangles
        // itself); the model path leaves it false so its own long-standing
        // flag-1 rotation is untouched - model and door geometry renders
        // correctly today and this change is not about it.
        void EmitQuad(RenderMesh& result, const std::vector<ModelVertex>& vertices, const ModelQuad& q, const std::array<Uv, 4>& uvs,
                      const QuadColours& colours, bool uvsAlreadyOrdered = false)
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

            auto emitVertex = [&](int32_t vertexIndex, Uv uv, int corner) -> uint32_t {
                const ModelVertex& v = vertices[static_cast<size_t>(vertexIndex)];
                result.vertices.push_back({
                    static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z),
                    uv.first, uv.second,
                    colours.r[corner], colours.g[corner], colours.b[corner]
                });
                return static_cast<uint32_t>(result.vertices.size() - 1);
            };

            // MODEL PATH ONLY. Model flag-1 triangles keep the one-step
            // rotation (ABC -> CAB) they have always had; models and doors
            // render correctly and nothing here changes that. LEVEL faces
            // arrive with uvsAlreadyOrdered set, because ComputeLevelQuadUvs
            // now picks the descriptor's real triangle corners (top-left,
            // top-right, bottom-left) itself - see the long note there.
            // Excludes the degenerate-quad fallback above, which already
            // forces isTriangle = false.
            bool rotateTriangleUvs = isTriangle && (q.flags == 1) && !uvsAlreadyOrdered;
            Uv uvA = rotateTriangleUvs ? effectiveUvs[2] : effectiveUvs[0];
            Uv uvB = rotateTriangleUvs ? effectiveUvs[0] : effectiveUvs[1];
            Uv uvC = rotateTriangleUvs ? effectiveUvs[1] : effectiveUvs[2];

            uint32_t ia = emitVertex(a, uvA, 0);
            uint32_t ib = emitVertex(b, uvB, 1);
            uint32_t ic = emitVertex(c, uvC, 2);

            if (isTriangle)
            {
                result.indices.push_back(ia);
                result.indices.push_back(ib);
                result.indices.push_back(ic);
            }
            else
            {
                uint32_t id = emitVertex(d, effectiveUvs[3], 3);
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
            EmitQuad(result, mesh.vertices, q, ComputeQuadUvs(q, uvRects), WhiteColours());
        }
        return result;
    }

    RenderMesh BuildLevelRenderMesh(const LevelGeometry& level, const std::vector<BxRectangle>& uvRects,
                                    const LightTable* lights)
    {
        RenderMesh result;
        result.vertices.reserve(level.quads.size() * 4);
        result.indices.reserve(level.quads.size() * 6);

        for (const auto& q : level.quads)
        {
            EmitQuad(result, level.vertices, q, ComputeLevelQuadUvs(q, uvRects), LevelQuadColours(q, lights), true);
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
            EmitQuad(result[static_cast<size_t>(group)], mesh.vertices, q, ComputeQuadUvs(q, uvRects), WhiteColours());
        }
        return result;
    }

    std::array<RenderMesh, 5> BuildLevelRenderMeshPerGroup(const LevelGeometry& level,
                                                            const std::vector<BxRectangle>& uvRects,
                                                            const std::vector<FaceUvRotation>& uvRotations,
                                                            const LightTable* lights,
                                                            const TextureAnimator* animator)
    {
        std::array<RenderMesh, 5> result;

        for (const auto& original : level.quads)
        {
            // Animated faces are redirected to the animator's current output
            // before anything else looks at texIndex, so texture page
            // selection and UV computation both see the real descriptor.
            ModelQuad q = original;
            q.texIndex = ResolveAnimatedTexIndex(original, animator);

            ResolvedLevelTexture resolved = ResolveLevelTexture(q.texIndex, uvRects);
            // Each descriptor's own page field (0-4, stamped at load time
            // from its BX chunk's own tag digits) selects the output
            // group directly - not a computed/cumulative index. Fallback
            // (out-of-range texIndex) -> group 0, see header comment.
            int group = (resolved.groupIndex >= 0 && resolved.groupIndex < 5) ? resolved.groupIndex : 0;

            std::array<Uv, 4> uvs = ComputeLevelQuadUvs(q, uvRects);

            // Per-face UV override, matched on the vertex indices. A
            // triangle rotates within its first three corners; a quad
            // rotates all four.
            for (const auto& rot : uvRotations)
            {
                // Match on the vertex SET, not the exact a/b/c/d order.
                //
                // The manifest's whole premise is that a face's vertex list
                // is its stable identity, but requiring the same starting
                // corner quietly broke that: the same face read out of a
                // viewer can list its corners from a different start point
                // or winding than the .MAP stores them, and the entry then
                // matched nothing and did nothing, silently. Face 10207 of
                // L111 is on disk as (10759,10760,10756,10755) and was
                // reported as (10756,10755,10760,10759) - same face, same
                // four vertices, no match under an ordered compare.
                if (!SameVertexSet(rot.vertices, q)) { continue; }

                int corners = (q.d == -1) ? 3 : 4;

                // Flip first, then rotate - see FaceUvRotation's comment.
                // The swap pairs are corners 0<->1 and 2<->3, matching what
                // draw routine 2 does relative to routine 0. On a triangle
                // only the first pair exists to swap.
                if (rot.flip)
                {
                    std::swap(uvs[0], uvs[1]);
                    if (corners == 4) { std::swap(uvs[2], uvs[3]); }
                }

                int steps = ((rot.steps % corners) + corners) % corners;
                if (steps != 0)
                {
                    std::array<Uv, 4> rotated = uvs;
                    for (int i = 0; i < corners; ++i)
                    {
                        rotated[static_cast<size_t>(i)] = uvs[static_cast<size_t>((i - steps + corners) % corners)];
                    }
                    uvs = rotated;
                }
                break;
            }

            EmitQuad(result[static_cast<size_t>(group)], level.vertices, q, uvs, LevelQuadColours(q, lights), true);
        }
        return result;
    }
}
