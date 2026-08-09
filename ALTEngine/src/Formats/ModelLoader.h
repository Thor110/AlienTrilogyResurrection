#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    struct ModelVertex
    {
        int16_t x, y, z;
        uint16_t marker; // 128 = first vertex, 0 = vertex, 127 = last vertex (per ModelRenderer.cs)
    };

    struct ModelQuad
    {
        int32_t a, b, c, d;  // vertex indices - d == -1 means triangle, not quad
        uint16_t texIndex;   // which TP frame (in the paired *GFX.B16/.BND) this face uses
        // NOTE ON THESE TWO BYTES. The names are historical and are kept
        // only because renaming them would touch the model paths too;
        // what they actually hold for LEVEL quads is:
        //
        //   flags    = on-disk +0x12. For levels this is the DRAW ROUTINE
        //              index - the original dispatches it through the
        //              35-entry table at 0x000a7098 (Ghidra:
        //              FUN_00025648, which rejects anything >= 0x23;
        //              L111's values are 0,1,2,3,4,8, all well inside
        //              that). The port approximates routines 0/1/2/11
        //              with UV permutations, which is empirically right
        //              for those, and falls back to identity UVs for 8.
        //   reserved = on-disk +0x13. The LIGHT ID ("light id x // Kaiser"
        //              in Edward's own ModelRenderer.cs). Low 7 bits index
        //              the level's 128-entry light table; bit 0x80 is the
        //              original's end-of-face-run marker (3885 of L111's
        //              11264 quads have it, partitioning the array
        //              exactly, with nothing left over).
        //
        // The runtime layout is this pair SWAPPED - the walker reads the
        // light id from runtime +0x12 and the draw routine from runtime
        // +0x13. That swap is why the light byte was misidentified for
        // six rounds. Do not "fix" one to match the other.
        uint8_t flags;       // level: draw routine index. model: UV variant (2 = triangle special order, 11 = flip 180, per ModelRenderer.cs)
        uint8_t reserved;    // level: light id | 0x80 end-of-run. See above.
    };

    struct ModelMesh
    {
        std::string sectionName;             // e.g. "M000"
        std::array<uint8_t, 4> identifier{};  // the 4 header bytes ModelRenderer.cs's big comment block maps to names (device type, etc)
        std::vector<ModelVertex> vertices;
        std::vector<ModelQuad> quads;
    };

    // Parses every M0 section in a model container (OBJ3D.BND,
    // PICKMOD.BND, or OPTOBJ.BND - all three share this format, per
    // ModelRenderer.cs's ExportModel). Each M0 section is:
    //   4 bytes  = "OBJ1" tag
    //   4 bytes  = padding (always zero, per the format notes)
    //   4 bytes  = identifier (maps to a device/object name - see
    //              Formats/ModelIndices.h for the three catalogs)
    //   4 bytes  = quadCount (Int32)
    //   4 bytes  = vertexCount (Int32)
    //   then quadCount x 20-byte quads (A,B,C,D Int32 vertex indices,
    //   texIndex UInt16, flags u8, reserved u8)
    //   then vertexCount x 8-byte vertices (X,Y,Z Int16, marker UInt16)
    //
    // CONFIRMED against the real OPTOBJ.BND (Edward, 2026): parses all
    // 14 sections without error, and every section's identifier bytes
    // match the confirmed reference list exactly (see ModelIndices.h) -
    // both the section-order assumption and the byte layout itself are
    // now verified against real data, not just internally self-
    // consistent against a synthetic file built to the same spec.
    //
    // OPTGFX.BND (the paired texture file) is also confirmed: exactly
    // 14 TP/CL/BX sections, matching OPTOBJ's 14 M0 sections 1:1 - see
    // RenderMesh.h for the UV/texture pairing this enables.
    //
    // Not yet verified against OBJ3D.BND or PICKMOD.BND specifically -
    // same format per ModelRenderer.cs, but no file to test against yet.
    class ModelLoader
    {
    public:
        // sectionPrefix selects which FORM sections to parse as meshes.
        // "M0" is the usual model naming; door and lift meshes live in
        // the level's own .MAP under "D" and "L" instead, alongside the
        // MAP0 chunk (which has no OBJ1 tag and must not be parsed).
        static std::vector<ModelMesh> Load(const std::filesystem::path& bndPath, const std::string& sectionPrefix = "M0");

        // Finds a mesh by its section NUMBER (e.g. 6 -> "M006"), not
        // array position - needed because PICKMOD.BND's section naming
        // has gaps (confirmed missing M005 and M024, still 26 sections
        // total matching the documented 0-25 catalog range minus those
        // two) - unlike OPTOBJ.BND, which was cleanly sequential and
        // safe to index positionally. Returns nullptr if no section with
        // that number exists.
        static const ModelMesh* FindByNumber(const std::vector<ModelMesh>& meshes, int number);
    };
}
