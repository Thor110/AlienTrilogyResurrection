#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // Removing a level's duplicate object record (crate, barrel and the rest:
    // the 16-byte records in MAP0 after the pickups). Stage 1, with the
    // duplicate model removals, and global: ALTViewer's patch button makes the
    // same file (BinaryUtility.RemoveMapObject).
    //
    // The original reads MAP0 by its header's counts, so a record can go if
    // the object count (+0x12) goes down by one and nothing names a later
    // object by number (only the logic groups name objects; checked per level
    // before an entry is written). Everything after the record moves up 16
    // bytes, the MAP0 and FORM sizes shrink by 16, and the byte patches are
    // written against the file without it.
    //
    // Guarded like a model removal: applied only if the count is `countBefore`
    // and the record at `index` is `record`; already applied if the count is
    // one less and the record at `index` is `next` (the one that followed it;
    // empty if it was the last). Anything else fails without touching the file.
    struct ObjectRemoval
    {
        std::string targetFile;
        std::string note;
        int index = -1;
        int countBefore = 0;
        std::array<uint8_t, 16> record{};
        bool hasNext = false;
        std::array<uint8_t, 16> next{};
    };

    enum class ObjectRemovalOutcome { Applied, AlreadyApplied, Failed };

    struct ObjectRemovalResult
    {
        ObjectRemoval const* removal = nullptr;
        ObjectRemovalOutcome outcome = ObjectRemovalOutcome::Failed;
        std::string error;
    };

    // Offset of object record `index` in an untrimmed level file, or -1.
    int64_t MapObjectOffset(const std::vector<uint8_t>& bytes, int index, int* objectCount = nullptr);

    ObjectRemovalOutcome ApplyObjectRemoval(std::vector<uint8_t>& bytes, const ObjectRemoval& removal, std::string& error);

    class ObjectRemovalLoader
    {
    public:
        // The "objectRemovals" array of a Patches.json; empty if it has none.
        static std::vector<ObjectRemoval> Load(const std::filesystem::path& jsonPath);
    };

    class ObjectRemovalRunner
    {
    public:
        static std::vector<ObjectRemovalResult> ApplyAll(const std::filesystem::path& gameRoot,
                                                         const std::vector<ObjectRemoval>& removals);
    };
}
