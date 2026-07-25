#include "SpriteFrameDecompressor.h"

namespace ALTEngine::Formats
{
    std::pair<std::vector<uint8_t>, size_t> SpriteFrameDecompressor::DecompressSingleFrame(
        const std::vector<uint8_t>& input, size_t startOffset)
    {
        std::vector<uint8_t> output;
        int32_t i = 0;
        size_t ptr = startOffset;

        while (true)
        {
            while (true)
            {
                i >>= 1;
                if ((i & 0xFF00) == 0)
                {
                    if (ptr >= input.size()) { return { output, ptr - startOffset }; }
                    i = 0xFF00 | input[ptr++];
                }
                if ((i & 1) == 1) { break; }
                if (ptr >= input.size()) { return { output, ptr - startOffset }; }
                output.push_back(input[ptr++]);
            }

            if (ptr >= input.size()) { return { output, ptr - startOffset }; }

            int32_t offs, size;
            if (input[ptr] >= 96)
            {
                offs = static_cast<int32_t>(input[ptr++]) - 256;
                size = 3;
            }
            else
            {
                size = (input[ptr] & 0xF0) >> 4;
                offs = (input[ptr] & 0x0F) << 8;
                if (++ptr >= input.size()) { return { output, ptr - startOffset }; }
                offs |= input[ptr++];
                if (offs == 0) { break; } // terminator
                offs = -offs;
                if (size == 5)
                {
                    if (ptr >= input.size()) { return { output, ptr - startOffset }; }
                    size = input[ptr++] + 9;
                }
                else
                {
                    size += 4;
                }
            }

            for (int32_t j = 0; j < size - 1; ++j)
            {
                int64_t src = static_cast<int64_t>(output.size()) + offs;
                if (src < 0 || src >= static_cast<int64_t>(output.size())) { break; }
                output.push_back(output[static_cast<size_t>(src)]);
            }
        }

        return { output, ptr - startOffset };
    }

    std::vector<std::vector<uint8_t>> SpriteFrameDecompressor::DecompressAllFramesInSection(const std::vector<uint8_t>& data)
    {
        std::vector<std::vector<uint8_t>> frames;
        size_t offset = 0;

        while (offset < data.size())
        {
            auto [frame, bytesConsumed] = DecompressSingleFrame(data, offset);
            if (frame.size() < 8) { break; } // heuristic: too small = likely invalid, matches TileRenderer.cs
            frames.push_back(std::move(frame));
            offset += bytesConsumed;

            while (offset < data.size() && data[offset] == 0) { ++offset; } // skip padding
        }

        return frames;
    }
}
