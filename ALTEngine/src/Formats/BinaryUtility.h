#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ALTEngine::Formats
{
    struct BinaryEdit
    {
        uint64_t offset;
        std::vector<uint8_t> bytes;
    };

    // Fixed-length in-place byte patching. Port of the ALTViewer.cs
    // BinaryUtility methods actually needed for the boot-time patch
    // routine (ReplaceBytesWithResize / BND frame alignment aren't ported
    // yet - those belong to the frame-replace feature, not this).
    class BinaryUtility
    {
    public:
        static uint8_t ReadByteAtOffset(const std::filesystem::path& path, uint64_t offset);

        // Reads `length` bytes starting at `offset` - used for idempotency
        // checks (has this edit already been applied?).
        static std::vector<uint8_t> ReadBytesAtOffset(const std::filesystem::path& path, uint64_t offset, size_t length);

        static void ReplaceByte(const std::filesystem::path& path, uint64_t offset, uint8_t value);

        // Overwrites byte ranges in place, in the given order - NOT sorted
        // by offset. Where two edits' offsets overlap, the later one in
        // `edits` wins, matching BinaryUtility.cs's Replace() (which
        // applies replacements via a plain foreach over the original
        // list, not a sorted one). File length never changes.
        static void ReplaceBytes(const std::filesystem::path& path, const std::vector<BinaryEdit>& edits);
    };
}
