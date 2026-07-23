#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    struct OverrideImage
    {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
    };

    // Checks `overrideRoot / key`.png (e.g. overrideRoot/"GFX/LEGAL_TP00_TP01.png")
    // and decodes it if present, via stb_image - forced to RGBA8 regardless
    // of the source PNG's actual channel count. Returns std::nullopt if the
    // file doesn't exist (not an error - this is the expected "no override,
    // use the original" case) or std::nullopt with a logged reason if it
    // exists but fails to decode.
    //
    // The override is expected to be the *final assembled* image (e.g.
    // 320x240 for LEGAL) - not per-tile. Whatever resolution the PNG
    // actually is gets used directly; no scaling/cropping is applied here.
    std::optional<OverrideImage> TryLoadOverrideImage(const std::filesystem::path& overrideRoot, const std::string& key);
}
