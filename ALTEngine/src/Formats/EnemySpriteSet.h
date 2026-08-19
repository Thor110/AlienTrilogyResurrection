#pragma once

#include "BndParser.h"
#include "SpriteFrameDecompressor.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // Turning an enemy animation frame into pixels. This closes the last gap.
    //
    // WHAT THE FRAME RECORD'S THIRD DWORD IS: a byte offset into the COMPRESSED
    // payload of one F0## section of the creature's NME file. Confirmed by
    // decompressing at those offsets and checking the pixel count against the
    // record's own width and height - on EGGS, all 33 frames of all 6 animations
    // come out exactly width*height.
    //
    // THREE WRONG READINGS PRECEDED IT, and the sequence is worth keeping:
    //   1. "a data offset" - right in kind, wrong about what it was relative to.
    //   2. "an opaque key into a sprite table" - wrong; the tables it indexes are
    //      a runtime CACHE, and the number is the cache tag only incidentally.
    //   3. "an offset into an RLE stream" - the RLE (FUN_00028350) is real, but
    //      running it at these offsets yields nothing. The compression here is the
    //      LZSS the port already had.
    //
    // ANIMATION INDEX TO SECTION IS A PERMUTATION, and it is not stored in the
    // frame record. EGGS maps its six animations to sections 5, 2, 4, 0, 1, 3 -
    // shuffled, not identity, so it cannot be assumed. It CAN be derived, and
    // deterministically: for a given animation, the correct section is the one
    // where every one of its frame offsets decompresses to exactly the size its
    // record states. Wrong sections fail almost immediately, because an LZSS
    // stream read at an arbitrary offset does not produce a plausible length.
    //
    // That derivation is what this does at load. It is not a guess - it is a
    // search with a check, and the check is the record's own dimensions.
    class EnemySpriteSet
    {
    public:
        // A frame record as it appears in the image, plus room for the decoded
        // pixels. The 12-byte record is
        //     int16 offsetX, offsetY, width, height, int32 compressedOffset
        struct Frame
        {
            int offsetX = 0;      // draw offset relative to the anchor
            int offsetY = 0;
            int width = 0;
            int height = 0;
            int compressedOffset = 0;
            std::vector<uint8_t> pixels;   // palette indices, width * height
        };

        // Loads the file and splits out its F0## payloads. The C000 palette is
        // read too - these files carry their own, so no external one is needed.
        bool Load(const std::filesystem::path& b16Path)
        {
            std::ifstream file(b16Path, std::ios::binary);
            if (!file) { return false; }
            const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                            std::istreambuf_iterator<char>());
            if (data.size() < 12) { return false; }

            sections.clear();
            palette.clear();

            size_t offset = 12;
            while (offset + 8 <= data.size())
            {
                char tag[5] = { 0 };
                std::memcpy(tag, &data[offset], 4);
                bool printable = true;
                for (int i = 0; i < 4; ++i)
                {
                    if (tag[i] < 32 || tag[i] > 126) { printable = false; }
                }
                if (!printable) { break; }

                const uint32_t length = (static_cast<uint32_t>(data[offset + 4]) << 24)
                                      | (static_cast<uint32_t>(data[offset + 5]) << 16)
                                      | (static_cast<uint32_t>(data[offset + 6]) << 8)
                                      | static_cast<uint32_t>(data[offset + 7]);
                if (offset + 8 + length > data.size()) { break; }

                if (std::strncmp(tag, "F0", 2) == 0)
                {
                    sections.emplace_back(data.begin() + offset + 8,
                                          data.begin() + offset + 8 + length);
                }
                else if (std::strncmp(tag, "C0", 2) == 0)
                {
                    palette.assign(data.begin() + offset + 8, data.begin() + offset + 8 + length);
                }

                offset += 8 + length + (length & 1);
            }
            return !sections.empty();
        }

        // Decodes one animation, given its frame table read out of the image.
        //
        // `records` is the animation's frame records in order - width, height and
        // the compressed offset. Returns the frames with pixels, or an empty
        // vector if no section satisfies every record, which is the signal that
        // the frame table or the file do not belong together.
        std::vector<Frame> DecodeAnimation(const std::vector<Frame>& records) const
        {
            if (records.empty()) { return {}; }

            for (size_t section = 0; section < sections.size(); ++section)
            {
                std::vector<Frame> decoded;
                decoded.reserve(records.size());
                bool allMatched = true;

                for (const Frame& record : records)
                {
                    if (record.width <= 0 || record.height <= 0) { allMatched = false; break; }
                    const size_t want = static_cast<size_t>(record.width)
                                      * static_cast<size_t>(record.height);
                    if (record.compressedOffset < 0
                        || static_cast<size_t>(record.compressedOffset) >= sections[section].size())
                    {
                        allMatched = false;
                        break;
                    }

                    auto result = SpriteFrameDecompressor::DecompressSingleFrame(
                        sections[section], static_cast<size_t>(record.compressedOffset));
                    if (result.first.size() != want) { allMatched = false; break; }

                    Frame frame = record;
                    frame.pixels = std::move(result.first);
                    decoded.push_back(std::move(frame));
                }

                if (allMatched && decoded.size() == records.size()) { return decoded; }
            }
            return {};
        }

        size_t SectionCount() const { return sections.size(); }
        const std::vector<uint8_t>& Palette() const { return palette; }

    private:
        std::vector<std::vector<uint8_t>> sections;
        std::vector<uint8_t> palette;
    };
}
