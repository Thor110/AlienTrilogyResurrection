#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // A render-time UV fix for one mis-authored face.
    //
    // Deliberately separate from the byte-patch manifest: this is not a
    // file edit, it is an override applied while building the mesh. That
    // means it needs no re-application if original files are restored,
    // and it can express things a byte patch cannot - UVs come from the
    // face's texture rect, assigned to its vertex slots, so "rotate the
    // texture on this one face" is a permutation of that assignment and
    // has no representation in the file at all.
    //
    // Faces are identified by their vertex indices rather than by a face
    // number. Face numbering depends on export order and triangulation
    // and is not stable; the vertex triple/quad is unambiguous and
    // survives byte patches to the same record.
    struct FaceUvRotation
    {
        std::string targetFile;            // e.g. "SECT11/L111LEV.MAP"
        std::array<int32_t, 4> vertices{}; // a, b, c, d - d is -1 for triangles
        int steps = 0;                     // rotate the UV assignment right by this many corners

        // Mirror the face horizontally - swaps corners 0<->1 and 2<->3,
        // which is exactly what draw routine 2 does relative to routine 0.
        // Applied BEFORE `steps`, so a face needing both reads as
        // "flip it, then rotate".
        //
        // This is the involutive counterpart of the existing byte patches
        // that write 0x00/0x02 into a face's draw-routine byte: it toggles
        // the mirror rather than setting it, so it corrects a face that is
        // wrong in either direction without needing to know which value is
        // currently on disk.
        bool flip = false;
    };

    class FaceUvPatchLoader
    {
    public:
        // Reads the "faceUvRotations" array from the patch manifest.
        // Missing file or missing section yields an empty list rather
        // than an error - the manifest is optional.
        static std::vector<FaceUvRotation> Load(const std::filesystem::path& manifestPath);

        // Those entries whose targetFile ends with the given level file
        // name, e.g. "L111LEV.MAP".
        static std::vector<FaceUvRotation> ForLevelFile(const std::vector<FaceUvRotation>& all,
                                                        const std::string& levelFileName);
    };
}
