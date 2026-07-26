#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    struct BndSection
    {
        std::string name;
        std::vector<uint8_t> data;
    };

    // .BND / .B16 file format (WIP, per TileRenderer.cs):
    //   4 bytes  = "FORM"
    //   4 bytes  = big-endian Int32 form size
    //   4 bytes  = platform tag, e.g. "PSXT"
    //   then a sequence of IFF-style chunks:
    //     4 bytes = chunk name (e.g. "TP00", "CL00", "BX00", "F001")
    //     4 bytes = big-endian Int32 chunk size
    //     N bytes = chunk data, padded to 2-byte alignment
    class BndParser
    {
    public:
        // Returns every chunk whose name starts with `sectionPrefix`
        // (e.g. "TP", "CL", "BX", "F0"), in file order.
        static std::vector<BndSection> ParseFormSections(const std::vector<uint8_t>& bnd, const std::string& sectionPrefix);

        // Single sequential pass returning EVERY chunk regardless of
        // prefix, in file order - use this (then filter the result by
        // prefix in memory) instead of calling ParseFormSections
        // separately per prefix, which re-scans the whole file from the
        // start each time (Edward, 2026: this was the source of
        // noticeable lag navigating the options menu - BndTextureLoader
        // was calling ParseFormSections three separate times per model
        // load, once each for "TP"/"CL"/"BX", on top of a full file read
        // from disk with no caching at all).
        static std::vector<BndSection> ParseAllFormSections(const std::vector<uint8_t>& bnd);

        // Reads and parses a .BND/.B16 file, returning ALL of its
        // sections (any prefix). Cached by absolute path - repeated
        // calls for the same file (e.g. navigating between menu items
        // that share the same OPTOBJ.BND/OPTGFX.BND) reuse the already-
        // parsed result instead of re-reading from disk and re-scanning.
        // Callers filter the returned sections by prefix themselves
        // (cheap - just a string comparison over an in-memory list, no
        // I/O or re-parsing).
        static const std::vector<BndSection>& LoadCached(const std::filesystem::path& path);

        // Byte offset of the chunk header (start of its 4-byte name) for
        // section "F0{index:D2}" - used by the frame-replace feature, not
        // by read-only decoding.
        static int64_t FindFormSectionOffset(const std::vector<uint8_t>& bnd, int index);
    };
}
