#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // Little-endian reads - this data is PS1-originated (little-endian),
    // and the C# reference tooling's BinaryReader is little-endian by
    // default on .NET too (matching x86/x64), so no byte-order surprises
    // to reconcile between the two.
    inline int32_t ReadInt32LE(const std::vector<uint8_t>& data, size_t offset)
    {
        return static_cast<int32_t>(
            static_cast<uint32_t>(data[offset]) |
            (static_cast<uint32_t>(data[offset + 1]) << 8) |
            (static_cast<uint32_t>(data[offset + 2]) << 16) |
            (static_cast<uint32_t>(data[offset + 3]) << 24));
    }

    inline uint16_t ReadUInt16LE(const std::vector<uint8_t>& data, size_t offset)
    {
        return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
    }

    inline int16_t ReadInt16LE(const std::vector<uint8_t>& data, size_t offset)
    {
        return static_cast<int16_t>(ReadUInt16LE(data, offset));
    }
}
