#pragma once

#include "BndTextureLoader.h"
#include "FaceUvPatches.h"
#include "LevelLoader.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // A texture-orientation check for level faces, for ALTLevelScan.
    //
    // Many surfaces are one picture cut into pieces - a window, a sign, the
    // queen's egg sack - each face showing the descriptor for its piece, and
    // the descriptors lying side by side on the texture page. Where two such
    // faces share an edge, both should put each of its two vertices on the
    // same texel of the page. A face that is rotated or mirrored wrongly puts
    // them elsewhere, and one of its other orientations (seven for a quad,
    // five for a triangle) brings them back - that is what is reported, as
    // the faceUvRotations entry that would apply it.
    //
    // Only edges between faces on the same page, showing different
    // descriptors that overlap or border each other there, are compared, and
    // only where the faces lie in roughly the same plane. Repeated tiles,
    // which the levels turn freely on purpose, are never compared. It can't
    // see a picture whose pieces are scattered over the page, and a face is
    // only reported on two or more such edges - on one, unrelated
    // neighbours on the page match by chance often enough to swamp the real
    // finds. Each suggestion still wants a look in a viewer.
    //
    // Checked against L391's inner egg sack: on the original file it names
    // 162 of the 246 faces fixed by hand, each with the same correction, and
    // none of them on the fixed file.
    struct UvSeamOptions
    {
        float minMismatch = 4.0f;  // texels apart the vertices must be now
        float maxRatio = 0.25f;    // and the best orientation at most this fraction of it
        int minEdges = 2;          // compared edges a face needs (one is mostly chance)
        float minCosine = 0.7f;    // neighbours whose normals differ more are not compared
        // Existing faceUvRotations for this level: a face they name is
        // judged as patched, and a suggestion replaces its entry.
        std::vector<FaceUvRotation> applied;
    };

    struct UvSeamSuspect
    {
        int face = -1;                       // index in LevelGeometry::quads
        std::array<int32_t, 4> vertices{};   // a, b, c, d (d -1 for a triangle)
        uint16_t texIndex = 0;               // the descriptor it shows
        uint8_t drawRoutine = 0;
        int edges = 0;                       // edges compared
        float currentCost = 0.0f;            // mean texels apart per edge, as it is now
        float bestCost = 0.0f;               // the same, as suggested
        bool flip = false;                   // the suggestion, in FaceUvRotation terms
        int steps = 0;
        bool alreadyPatched = false;         // a faceUvRotations entry names it
    };

    // Applies a FaceUvRotation's flip and steps to a face's corner UVs, as
    // the level mesh does.
    std::array<std::pair<float, float>, 4> OrientUvs(std::array<std::pair<float, float>, 4> uvs,
                                                    bool triangle, bool flip, int steps);

    // Strongest suspects first.
    std::vector<UvSeamSuspect> ScanLevelUvSeams(const LevelGeometry& level, const BndTextureSet& textures,
                                                const UvSeamOptions& options = {});
}
