#pragma once

#include <cstdint>
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

        // Byte offset of the chunk header (start of its 4-byte name) for
        // section "F0{index:D2}" - used by the frame-replace feature, not
        // by read-only decoding.
        static int64_t FindFormSectionOffset(const std::vector<uint8_t>& bnd, int index);
    };
}
