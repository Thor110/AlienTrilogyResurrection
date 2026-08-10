#pragma once

#include "RenderMesh.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // Wavefront OBJ/MTL loader, for geometry that is NOT in the original
    // files: the -GAP fill faces and, later, higher-resolution replacement
    // level models.
    //
    // Deliberately a small subset. This reads the OBJ features Blender emits
    // for hand-made level geometry and nothing more:
    //   v / vt          positions and texture coordinates
    //   f               faces, `v`, `v/vt` and `v//vn` forms, any vertex count
    //                   (fan-triangulated), negative indices supported
    //   usemtl / mtllib material assignment, so faces can be grouped by page
    //   o / g           ignored (all objects merge into one mesh)
    //   vn              parsed but unused - the level renderer has no normals
    //
    // Plus two CUSTOM statements, for reattaching the original's lighting and
    // animation to replacement geometry. Both set state that applies to every
    // face declared after them, like usemtl does, and -1 clears:
    //
    //   alt_light <id>   the level light record this face is lit by, 0..127.
    //                    Exactly the on-disk +0x13 low 7 bits, so the ids in
    //                    the scanner's manifest can be copied straight across.
    //   alt_anim <n>     make this face animated, driven by animator ordinal n.
    //                    The equivalent of draw routine 8, where the original
    //                    stored the ordinal in texIndex.
    //
    // A replacement may use MORE faces than the original did - nothing is
    // bounded by the original counts. Several faces may share a light id or an
    // animator, exactly as the original's do.
    //
    // Unknown statements are ignored, so these are backward compatible: a file
    // carrying them still loads in any other OBJ tool.
    //
    // Anything else on a line is skipped rather than treated as an error, so
    // an export with extra features still loads.
    //
    // COORDINATE SPACE. The OBJ is expected to already be in the game's world
    // units and orientation - the same space `LevelGeometry::vertices` uses
    // after loading. This is what Edward's own OBJ exporter produces, so a
    // face can be exported, edited and brought back without a transform. No
    // scaling or axis flip is applied here; if a hand-made file looks
    // mirrored or inside out, the fix belongs in the exporter, not here.
    struct ObjMaterial
    {
        std::string name;
        std::string diffuseTexture; // map_Kd, as written in the .mtl
    };

    // Faces are grouped by everything that has to be uniform for one draw:
    // the material, and the light and animator they are bound to.
    struct ObjFaceGroup
    {
        std::string materialName; // empty when the file used no usemtl
        int lightId = -1;         // -1 = unlit, render at full brightness
        int animatorOrdinal = -1; // -1 = static texture
        std::vector<RenderVertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct ObjModel
    {
        std::vector<ObjFaceGroup> groups;
        std::vector<ObjMaterial> materials;

        // Human-readable account of what was skipped or went wrong. Always
        // worth logging: a silently-empty override is very hard to diagnose
        // from the outside.
        std::vector<std::string> warnings;

        size_t TotalTriangles() const
        {
            size_t n = 0;
            for (const ObjFaceGroup& g : groups) { n += g.indices.size() / 3; }
            return n;
        }

        bool Empty() const { return TotalTriangles() == 0; }
    };

    class ObjLoader
    {
    public:
        // Loads an OBJ. A missing file is NOT an error - it returns an empty
        // model with no warnings, because every override in this project is
        // optional and absence is the normal case.
        static ObjModel Load(const std::filesystem::path& objPath);

        // True when the path exists and is a regular file.
        static bool Exists(const std::filesystem::path& objPath);
    };
}
