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
        uint8_t flags;       // UV winding/orientation variant (2 = triangle special order, 11 = flip 180, per ModelRenderer.cs)
        uint8_t reserved;
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
        static std::vector<ModelMesh> Load(const std::filesystem::path& bndPath);

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
