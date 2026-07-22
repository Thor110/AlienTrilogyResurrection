#include "PaletteFile.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace ALTEngine::Formats
{
    std::vector<uint8_t> PaletteFile::Load(const std::filesystem::path& path, bool trimmed)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in.is_open())
        {
            throw std::runtime_error("PaletteFile::Load: could not open " + path.string());
        }
        auto length = static_cast<size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        if (length > 768)
        {
            throw std::runtime_error("PaletteFile::Load: " + path.string() + " is larger than 768 bytes");
        }

        std::vector<uint8_t> loaded(length);
        in.read(reinterpret_cast<char*>(loaded.data()), static_cast<std::streamsize>(length));
        if (!in)
        {
            throw std::runtime_error("PaletteFile::Load: failed reading " + path.string());
        }

        std::vector<uint8_t> palette(768, 0);

        if (trimmed)
        {
            // On-disk data is missing its first 96 bytes (32 colours) -
            // place what we have starting at offset 96.
            if (length > 768 - 96)
            {
                throw std::runtime_error("PaletteFile::Load: trimmed palette " + path.string() + " is too large");
            }
            std::copy(loaded.begin(), loaded.end(), palette.begin() + 96);
        }
        else if (length == 768)
        {
            palette = std::move(loaded);
        }
        else
        {
            std::copy(loaded.begin(), loaded.end(), palette.begin());
        }

        return palette;
    }
}
