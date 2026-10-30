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

            // TRIANGLES, TRACED. Draw routine 1 (0x1b5c8), the one level
            // triangles use, projects them with FUN_00048128(c, b, a) into
            // screen slots 0, 1 and 2 and copies the texture descriptor's
            // first three corners into those slots in order. The quad routine
            // (0x19af0) projects with FUN_00048490(d, c, b, a) into slots 0, 1,
            // 3, 2 and copies all four corners in order, which with the quad
            // mapping below puts the descriptor's corners at x0y0, x1y0, x0y1,
            // x1y1. So a triangle (a, b, c) takes x0y1, x1y0, x0y0 - the same
            // corners ALTViewer's export and the models' triangle case use,
            // and what the original shows (Edward, 2026: L371's door frames,
            // DOS against the port). The "pinched quad" of zip 46 and the
            // (x0y0, x1y0, x0y1) before it were both guesses, and both wrong.
            if (q.d == -1)
            {
                return { Uv{x0, y1}, Uv{x1, y0}, Uv{x0, y0}, Uv{x0, y0} };
            }

            // Routines 2, 6, 34 and their animated forms (10, 28, 32) swap
            // the corners as 2 does; everything else maps as 0. (11 draws
            // nothing in the original.)
            if (ClassifyDrawRoutine(q.flags).mirrored)
            {
                return { Uv{x1, y1}, Uv{x0, y1}, Uv{x0, y0}, Uv{x1, y0} };
            }
            return { Uv{x0, y1}, Uv{x1, y1}, Uv{x1, y0}, Uv{x0, y0} };
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
            if (!animator || !ClassifyDrawRoutine(q.flags).animated) { return q.texIndex; }
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
                      const QuadColours& colours, bool uvsAlreadyOrdered = false, bool forceDoubleSided = false)
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
            // now picks the descriptor's real triangle corners itself - see
            // the note there.
            // Excludes the degenerate-quad fallback above, which already
            // forces isTriangle = false.
            bool rotateTriangleUvs = isTriangle && (q.flags == 1) && !uvsAlreadyOrdered;
            Uv uvA = rotateTriangleUvs ? effectiveUvs[2] : effectiveUvs[0];
            Uv uvB = rotateTriangleUvs ? effectiveUvs[0] : effectiveUvs[1];
            Uv uvC = rotateTriangleUvs ? effectiveUvs[1] : effectiveUvs[2];

            uint32_t ia = emitVertex(a, uvA, 0);
            uint32_t ib = emitVertex(b, uvB, 1);
            uint32_t ic = emitVertex(c, uvC, 2);

            // The routines that don't cull (4, 5, 6, 12, 13, 14, 34 and the
            // animated 26-32 - ClassifyDrawRoutine) are DOUBLE-SIDED - visible
            // from behind as well as in front; with BACK culling on they vanish
            // from one side (Edward, 2026, for routine 4).
            //
            // Handled by emitting the reverse winding as extra triangles rather
            // than by routing these faces through the existing
            // doubleSidedPipeline. The level mesh is partitioned by texture page,
            // so a second pipeline would mean a second partition and a second set
            // of buffers per page; L111 has 171 flag-4 quads, so the duplicate
            // triangles cost a few hundred indices against that.
            //
            // Level faces only: `uvsAlreadyOrdered` marks the level path, and
            // model flag values mean something different (see ModelQuad).
            bool doubleSided = forceDoubleSided || (uvsAlreadyOrdered && !ClassifyDrawRoutine(q.flags).culled);

            if (isTriangle)
            {
                result.indices.push_back(ia);
                result.indices.push_back(ib);
                result.indices.push_back(ic);
                if (doubleSided)
                {
                    result.indices.push_back(ic);
                    result.indices.push_back(ib);
                    result.indices.push_back(ia);
                }
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

                if (doubleSided)
                {
                    // Same two triangles wound the other way.
                    result.indices.push_back(ic);
                    result.indices.push_back(ib);
                    result.indices.push_back(ia);
                    result.indices.push_back(id);
                    result.indices.push_back(ic);
                    result.indices.push_back(ia);
                }
            }
        }
    }

    std::array<std::pair<float, float>, 4> LevelFaceUvs(const ModelQuad& q, uint16_t texIndex,
                                                        const std::vector<BxRectangle>& uvRects)
    {
        ModelQuad resolved = q;
        resolved.texIndex = texIndex;
        return ComputeLevelQuadUvs(resolved, uvRects);
    }

    DrawRoutine ClassifyDrawRoutine(uint8_t routine)
    {
        DrawRoutine r;
        auto set = [&](FaceFill fill, bool culled, bool mirrored, bool animated) {
            r.fill = fill; r.culled = culled; r.mirrored = mirrored; r.animated = animated;
        };
        switch (routine)
        {
        case 0:  set(FaceFill::Opaque, true, false, false); break;
        case 1:  set(FaceFill::Opaque, true, false, false); break;
        case 2:  set(FaceFill::Opaque, true, true, false); break;
        case 4:  set(FaceFill::Cutout, false, false, false); break;
        case 5:  set(FaceFill::Cutout, false, false, false); break;
        case 6:  set(FaceFill::Cutout, false, true, false); break;
        case 8:  set(FaceFill::Opaque, true, false, true); break;
        case 10: set(FaceFill::Opaque, true, true, true); break;
        case 12: set(FaceFill::Translucent, false, false, false); break;
        case 13: set(FaceFill::Translucent, false, false, false); break;
        case 14: set(FaceFill::Opaque, false, false, false); r.flat = true; break;
        case 26: set(FaceFill::Cutout, false, false, true); break;
        case 28: set(FaceFill::Cutout, false, true, true); break;
        case 30: set(FaceFill::Translucent, false, false, true); break;
        case 32: set(FaceFill::Translucent, false, true, true); break;
        case 34: set(FaceFill::Translucent, false, true, false); break;
        default: break;   // nothing drawn
        }
        return r;
    }

    RenderMesh BuildRenderMesh(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects, bool flatSeeThrough,
                               std::array<float, 3> flatColour)
    {
        RenderMesh result;
        result.vertices.reserve(mesh.quads.size() * 4);
        result.indices.reserve(mesh.quads.size() * 6);

        // A model loaded as flat see-through (see the declaration) is all
        // routine 14: drawn black, half over what is behind, from both sides -
        // the shader multiplies the texel by the vertex colour, so the
        // texture never shows.
        if (flatSeeThrough)
        {
            QuadColours fill;
            for (int c = 0; c < 4; ++c) { fill.r[c] = flatColour[0]; fill.g[c] = flatColour[1]; fill.b[c] = flatColour[2]; }
            for (const auto& q : mesh.quads)
            {
                EmitQuad(result, mesh.vertices, q, ComputeQuadUvs(q, uvRects), fill, false, true);
            }
            result.translucentStart = result.indices.empty() ? SIZE_MAX : 0;
            return result;
        }

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

    namespace
    {
        // The drawn fills' slots in BuildLevelRenderMeshAllFills; -1 for none.
        int FillSlot(FaceFill fill)
        {
            switch (fill)
            {
            case FaceFill::Opaque: return 0;
            case FaceFill::Cutout: return 1;
            case FaceFill::Translucent: return 2;
            default: return -1;
            }
        }

        // Builds the faces of the fills `wanted` marks (by slot) into `result`.
        void BuildLevelFills(const LevelGeometry& level, const std::vector<BxRectangle>& uvRects,
                             const std::vector<FaceUvRotation>& uvRotations, const LightTable* lights,
                             const TextureAnimator* animator, const std::array<bool, 3>& wanted,
                             std::array<std::array<RenderMesh, 5>, 3>& result);
    }

    std::array<RenderMesh, 5> BuildLevelRenderMeshPerGroup(const LevelGeometry& level,
                                                            const std::vector<BxRectangle>& uvRects,
                                                            const std::vector<FaceUvRotation>& uvRotations,
                                                            const LightTable* lights,
                                                            const TextureAnimator* animator,
                                                            FaceFill fill)
    {
        const int slot = FillSlot(fill);
        if (slot < 0) { return {}; }
        std::array<bool, 3> wanted{};
        wanted[static_cast<size_t>(slot)] = true;
        std::array<std::array<RenderMesh, 5>, 3> all;
        BuildLevelFills(level, uvRects, uvRotations, lights, animator, wanted, all);
        return std::move(all[static_cast<size_t>(slot)]);
    }

    std::array<std::array<RenderMesh, 5>, 3> BuildLevelRenderMeshAllFills(const LevelGeometry& level,
                                                                         const std::vector<BxRectangle>& uvRects,
                                                                         const std::vector<FaceUvRotation>& uvRotations,
                                                                         const LightTable* lights,
                                                                         const TextureAnimator* animator)
    {
        std::array<std::array<RenderMesh, 5>, 3> all;
        BuildLevelFills(level, uvRects, uvRotations, lights, animator, { true, true, true }, all);
        return all;
    }

    namespace
    {
    void BuildLevelFills(const LevelGeometry& level, const std::vector<BxRectangle>& uvRects,
                         const std::vector<FaceUvRotation>& uvRotations, const LightTable* lights,
                         const TextureAnimator* animator, const std::array<bool, 3>& wanted,
                         std::array<std::array<RenderMesh, 5>, 3>& all)
    {
        for (size_t faceIndex = 0; faceIndex < level.quads.size(); ++faceIndex)
        {
            const ModelQuad& original = level.quads[faceIndex];
            const DrawRoutine routine = ClassifyDrawRoutine(original.flags);
            const int slot = FillSlot(routine.fill);
            if (slot < 0 || !wanted[static_cast<size_t>(slot)]) { continue; }
            const FaceFill fill = routine.fill;
            std::array<RenderMesh, 5>& result = all[static_cast<size_t>(slot)];

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

            // The see-through fill is unlit: it reads the texel's own colour
            // (the table at 0x2273c8), not the lit one.
            const QuadColours colours = fill == FaceFill::Translucent ? WhiteColours() : LevelQuadColours(q, lights);
            RenderMesh& out = result[static_cast<size_t>(group)];
            const size_t before = out.indices.size();
            EmitQuad(out, level.vertices, q, uvs, colours, true);
            for (size_t t = before; t + 2 < out.indices.size(); t += 3)
            {
                out.triangleFace.push_back(static_cast<uint32_t>(faceIndex));
            }
        }
    }
    }

    std::vector<MeshFaceExtent> ExtractMeshFaces(const RenderMesh& mesh, const ModelMesh& source)
    {
        std::vector<MeshFaceExtent> faces;
        auto lowest = [&](size_t i) { return std::min({ mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2] }); };
        std::vector<std::pair<size_t, size_t>> runs; // [first index, end index) of each face
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            if (!runs.empty() && lowest(runs.back().first) == lowest(i)) { runs.back().second = i + 3; }
            else { runs.push_back({ i, i + 3 }); }
        }
        const bool gate = runs.size() == source.quads.size();

        for (size_t r = 0; r < runs.size(); ++r)
        {
            if (gate)
            {
                // FUN_00046ffc: every other face, never the last.
                if ((r & 1) != 0 || r + 1 == runs.size()) { continue; }
                if (source.quads[r].flags != 0 && source.quads[r].flags != 2) { continue; }
            }
            float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f, minZ = 1e30f, maxZ = -1e30f;
            float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
            bool ok = true;
            for (size_t k = runs[r].first; k < runs[r].second; ++k)
            {
                const uint32_t vi = mesh.indices[k];
                if (vi >= mesh.vertices.size()) { ok = false; break; }
                const auto& v = mesh.vertices[vi];
                minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
                minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
                minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
                minU = std::min(minU, v.u); maxU = std::max(maxU, v.u);
                minV = std::min(minV, v.v); maxV = std::max(maxV, v.v);
            }
            if (!ok) { continue; }

            MeshFaceExtent face;
            face.cx = (minX + maxX) * 0.5f;
            face.cy = (minY + maxY) * 0.5f;
            face.cz = (minZ + maxZ) * 0.5f;
            // A face can be flat on any axis, so its size is the larger two sides.
            float e[3] = { maxX - minX, maxY - minY, maxZ - minZ };
            std::sort(e, e + 3);
            face.halfWidth = std::max(e[2], 1.0f) * 0.5f;
            face.halfHeight = std::max(e[1], 1.0f) * 0.5f;
            face.u0 = minU; face.v0 = minV; face.u1 = maxU; face.v1 = maxV;
            // The face's own corners: its run of vertices, in emitted order.
            uint32_t first = mesh.indices[runs[r].first], last = first;
            for (size_t k = runs[r].first; k < runs[r].second; ++k)
            {
                first = std::min(first, mesh.indices[k]);
                last = std::max(last, mesh.indices[k]);
            }
            for (int c = 0; c < 4; ++c)
            {
                const uint32_t vi = std::min(first + static_cast<uint32_t>(c), last);
                const auto& v = mesh.vertices[vi];
                face.px[c] = v.x; face.py[c] = v.y; face.pz[c] = v.z;
                face.pu[c] = v.u; face.pv[c] = v.v;
            }
            faces.push_back(face);
        }
        return faces;
    }
}
