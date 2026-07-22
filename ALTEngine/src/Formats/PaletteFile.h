#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ALTEngine::Formats
{
    // Loads an external CD/PALS/*.PAL file into a full 768-byte (256 x
    // RGB, 6-bit-per-channel 0-63) palette buffer. Port of
    // PaletteEditor.cs's LoadPalette - three cases depending on on-disk
    // file length:
    //
    //  - `trimmed` (BONESHIP/COLONY/PRISHOLD): the file is missing its
    //    first 96 bytes (32 unused colours) entirely. Loaded data is
    //    placed starting at offset 96 in the output buffer; indices 0-31
    //    stay zeroed.
    //  - not trimmed, length < 768 (e.g. LOGOSGFX, 576 bytes): loaded
    //    data is placed starting at offset 0; the remainder stays zeroed.
    //  - length == 768: used as-is.
    //
    // LEGAL and LOGOSGFX are NOT in the trimmed set (only BONESHIP,
    // COLONY, PRISHOLD are) - pass `trimmed = false` for those.
    class PaletteFile
    {
    public:
        static std::vector<uint8_t> Load(const std::filesystem::path& path, bool trimmed);
    };
}
