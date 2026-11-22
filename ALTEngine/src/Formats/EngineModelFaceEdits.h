#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // ALTENGINE-ONLY MODEL FACE EDITS - NOT FOR THE ORIGINAL GAME.
    //
    // Part of stage 5 (data/EngineFilePatches.json, "modelFaces"; Patch/README.txt).
    // ALTViewer's patch button never applies these, and an installed copy of the
    // original must never be given them.
    //
    // A door or lift model ("D"/"L" section of a level .MAP) cannot hold some
    // fixes the original could draw: the original's loader for them
    // (FUN_00041a20) turns each face's +0x12 byte into draw routine 0, 1 or 2,
    // and all three skip a face seen from behind. A mirrored copy of a face
    // needs its corners reversed, so it faces away and the original hides it.
    // These edits are for those faces: they turn the face over and put
    // ENGINE_DOUBLE_SIDED in its +0x13 byte. The original never reads that byte
    // from the file (the loader writes the routine number into it), and no
    // shipped door or lift face has it set (all 4,430 are 0); ALTEngine draws
    // a door or lift face carrying it from both sides
    // (BuildRenderMeshPerGroup, honourEngineMarker).
    //
    // An edit names its face by section and face number, not by file offset,
    // so it holds whether the Level Cleanup (which changes the length of the
    // map before the models) is on or off. It is made after the Level Cleanup
    // and the texture blanking and taken back out at the start of every
    // install run, before any earlier stage's work is set aside, so every
    // global stage always meets the file without it.
    inline constexpr uint8_t ENGINE_DOUBLE_SIDED = 5;

    struct EngineModelFaceEdit
    {
        std::string target;               // e.g. "SECT32/L381LEV.MAP"
        std::string section;              // e.g. "L000"
        int face = -1;                    // face record number in the section
        std::array<uint8_t, 20> was{};    // the record as the global stages leave it
        std::array<uint8_t, 20> now{};    // the record after the edit
        std::string note;
    };

    enum class EngineModelFaceOutcome { Applied, AlreadyApplied, NotFound, Mismatch };

    namespace EngineModelFaceEdits
    {
        // Byte offset of face `face` of the OBJ1 model in FORM section
        // `section`, or -1 if there is no such section or face.
        int64_t FaceOffset(const std::vector<uint8_t>& file, const std::string& section, int face);

        // Makes the edit if the record is `was`; reports it already made if it
        // is `now`; anything else is left alone.
        EngineModelFaceOutcome Apply(std::vector<uint8_t>& file, const EngineModelFaceEdit& edit);

        // Puts `was` back if the record is `now`. True if it changed the file.
        bool Revert(std::vector<uint8_t>& file, const EngineModelFaceEdit& edit);

        // Reads the "modelFaces" array of EngineFilePatches.json (as text).
        // Entries that cannot be read are skipped and named in `errors`.
        std::vector<EngineModelFaceEdit> Parse(const std::string& jsonText, std::vector<std::string>& errors);
    }
}
