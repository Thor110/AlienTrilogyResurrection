#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "BxParser.h"
#include "FaceUvPatches.h"
#include "LevelLoader.h"
#include "LightTable.h"
#include "TextureAnimation.h"
#include "ModelLoader.h"

namespace ALTEngine::Formats
{
    // r/g/b are the quad's LIGHT COLOUR, 0-1, resolved from the level's
    // light table (see LightTable.h). Per-vertex rather than per-quad
    // because light mode 4 is gouraud: it gives each of the four corners
    // its own colour. Every other mode writes the same colour to all
    // four, which is why the original engine passes one 16-byte LUT
    // entry per face and lets the rasterizer decide how many of its
    // four triples to read (Ghidra: FUN_00025648 hands the entry to the
    // draw routine; FUN_00029be0 builds it).
    //
    // Models and doors currently emit 1,1,1 - they are lit from the
    // entity's own light lookup, not a level light record, and that path
    // is not traced yet.
    struct RenderVertex
    {
        float x, y, z;
        float u, v;
        float r, g, b;
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
    // function specifically because texIndex resolution genuinely
    // differs from ExportModel's:
    //   - CORRECTED (Edward, 2026 - full Ghidra decompilation of the
    //     real texture-selection code): texIndex is a DIRECT, FLAT index
    //     into one continuous descriptor array built by appending every
    //     BX00-BX04 section's rects in file order - there is no
    //     per-group resolution at render time at all. Each descriptor
    //     carries its own `page` field (which TP/texture it belongs to),
    //     stamped at load time from its BX chunk's own tag digits
    //     ("BX00" -> page 0, etc), not computed from its position in the
    //     flattened sequence.
    //   - An EARLIER version of this used cumulative-subtraction group
    //     resolution (matching AlienTrilogyMapLoader.cs's own approach)
    //     - confirmed WRONG via Ghidra: that C# logic is "an accidental
    //     equivalent" that only gives the right answer when BX chunks
    //     appear in ascending tag order with exactly one chunk per page,
    //     and was the actual cause of "many faces throughout the level
    //     are mapped to the wrong texture" (Edward, 2026) once real
    //     level rendering was visible.
    //   - The per-quad flags meaning is the SAME as ExportModel's
    //     (flags==2 = flip 180, flags==11 = special triangle order) -
    //     CORRECTED (Edward's AlienTrilogyMapLoader.cs, 2026). An
    //     earlier version of this claimed levels used a different
    //     mapping (flags 1/5/13 special, flags==2 flip); that was wrong,
    //     from a less careful reading of ModelRenderer.cs, and was the
    //     bug behind "many faces are flipped" once real level rendering
    //     was actually visible.
    //   - Same out-of-range fallback (UV (1,1)) and same "no extra V
    //     flip" correction as BuildRenderMesh.
    //
    // Structurally checked against real level data (L111LEV.MAP +
    // 111GFX.B16): index counts, bounds, and UV range were all clean
    // under the OLD group-resolution scheme too - that test caught
    // structural errors (out-of-bounds, wrong triangle counts) but,
    // same lesson as BuildRenderMesh's earlier V-flip bug, was never
    // capable of catching "resolves to a plausible-looking but wrong
    // page" on its own. Re-verification against the new flat-index
    // scheme is still pending actual visual confirmation.
    RenderMesh BuildLevelRenderMesh(const LevelGeometry& level, const std::vector<BxRectangle>& uvDescriptors,
                                    const LightTable* lights = nullptr);

    // Same UV/texture resolution as BuildLevelRenderMesh, but partitions
    // the output by which TEXTURE PAGE each quad's resolved descriptor
    // actually belongs to (its own `page` field), instead of merging
    // everything into one buffer. Needed for actually rendering a
    // level: unlike a single OPTOBJ/PICKMOD model (one texture, one draw
    // call), a level can use up to 5 different textures, and a GPU draw
    // call can only have one texture bound at a time - so the renderer
    // needs one vertex/index buffer + draw call per page actually used,
    // not one mixed buffer. Quads whose texIndex is out of range for the
    // descriptor array (the documented fallback case) are placed in
    // page 0 with the fallback UV (1,1) - they still need to go
    // somewhere, and page 0 is as good as any since the fallback isn't
    // texture-specific.
    // Per-texture-page split for a single model mesh, using the MODEL
    // UV convention (flags 2/130 = triangle order, 11/139 = flip 180).
    //
    // This is what door and lift meshes need: they live in the level's
    // own .MAP, are textured from the level's texture pages, and index
    // the level's flat descriptor array - but they follow the model
    // flags convention, not the level one. Pass the level's uvRects.
    // A door mesh can span more than one page (both L111 doors span
    // pages 2 and 4), which is why this splits rather than returning a
    // single mesh.
    std::array<RenderMesh, 5> BuildRenderMeshPerGroup(const ModelMesh& mesh, const std::vector<BxRectangle>& uvRects);

    // `lights` supplies each quad's colour. Passing nullptr emits white
    // for everything, i.e. the pre-lighting behaviour, which is what the
    // model-preview and any non-gameplay caller wants.
    std::array<RenderMesh, 5> BuildLevelRenderMeshPerGroup(const LevelGeometry& level,
                                                            const std::vector<BxRectangle>& uvDescriptors,
                                                            const std::vector<FaceUvRotation>& uvRotations = {},
                                                            const LightTable* lights = nullptr,
                                                            const TextureAnimator* animator = nullptr);
}

