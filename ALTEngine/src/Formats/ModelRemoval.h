#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "BinaryUtility.h"

namespace ALTEngine::Formats
{
    // Removing a level's duplicate and unused door ("D") and lift ("L") model
    // chunks (Notes/duplicate-level-models.md).
    //
    // The original keeps a pointer to each such chunk in a table, in file
    // order (0x27b30: D to 0x1a5ce8, L to 0x1a8354), and a door or lift draws
    // table[its record's model number] at its own position (FUN_00028f00,
    // FUN_00041ba4). So a chunk can go once every record that points at it, or
    // at a later chunk of its kind, is renumbered. The tags of the chunks that
    // stay are left as they are; only their first letter is ever read.
    //
    // Unlike a byte patch this shortens the file, so it is guarded: it is
    // applied only if every chunk it removes is there with the expected size
    // and CRC-32, and counts as already applied if none is there and the
    // record bytes already hold their new values. Anything in between fails
    // without touching the file. Run it before the byte patches, which are
    // written against the shortened file.
    struct RemovedChunk
    {
        std::string tag;     // e.g. "D001"
        uint32_t size = 0;   // the chunk's own size field (payload bytes)
        uint32_t crc32 = 0;  // of the payload
    };

    struct ModelRemoval
    {
        std::string targetFile;              // relative to the game root
        std::string note;
        std::vector<RemovedChunk> chunks;
        std::vector<BinaryEdit> recordEdits; // door byte 7 / lift byte 12, inside MAP0
    };

    enum class ModelRemovalOutcome { Applied, AlreadyApplied, Failed };

    struct ModelRemovalResult
    {
        ModelRemoval const* removal = nullptr;
        ModelRemovalOutcome outcome = ModelRemovalOutcome::Failed;
        std::string error;
    };

    // The standard CRC-32 (reflected, polynomial 0xEDB88320).
    uint32_t Crc32(const uint8_t* data, size_t size);

    // Removes the chunks from a whole file image and applies the record
    // edits. Returns the outcome; `bytes` is changed only on Applied.
    ModelRemovalOutcome ApplyModelRemoval(std::vector<uint8_t>& bytes, const ModelRemoval& removal, std::string& error);

    class ModelRemovalLoader
    {
    public:
        // The "modelRemovals" array of a Patches.json; empty if it has none.
        static std::vector<ModelRemoval> Load(const std::filesystem::path& jsonPath);
    };

    class ModelRemovalRunner
    {
    public:
        static std::vector<ModelRemovalResult> ApplyAll(const std::filesystem::path& gameRoot,
                                                        const std::vector<ModelRemoval>& removals);
    };
}
