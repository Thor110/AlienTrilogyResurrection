#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    struct SpriteFrameInfo
    {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
    };

    // Loads one decompressed, dimensioned, palette-applied frame from an
    // enemy/weapon sprite .B16 file (cd\NME\*.B16, or the weapon
    // view-model .B16 files - both use the same F0## compressed-section
    // format, confirmed against a real file). Ties together:
    //   - BndParser (finds the F0## sections - already format-agnostic,
    //     no changes needed)
    //   - SpriteFrameDecompressor (the actual LZSS decompression)
    //   - SpriteFrameDimensions (the hardcoded width/height lookup -
    //     these dimensions aren't stored in the file itself)
    //   - The palette-index-0-is-transparent rule (confirmed universal
    //     for these files specifically, unlike level textures' more
    //     complex per-level-ID rule - see LevelTransparency.h)
    //   - The override system, using Edward's own export naming
    //     convention: "{spriteName}_F{section:03d}_FRAME{frame:02d}"
    //     (e.g. "EGGS_F000_FRAME00"), checked before decoding the real
    //     data - same override-first pattern as BndTextureLoader.
    class SpriteFrameLoader
    {
    public:
        // `spriteName` is the sprite's base filename with no extension
        // (e.g. "EGGS", matching both the .B16 filename and the
        // dimension table's own keys). `palette` is 256 x RGB triplets,
        // 6-bit per channel - same format PaletteFile::Load and
        // RawImageRenderer expect. `section`/`frame` are 0-based.
        //
        // Returns std::nullopt if the section/frame doesn't exist, the
        // dimensions aren't in the lookup table, or the file can't be
        // read - logs why in each case.
        static std::optional<SpriteFrameInfo> LoadFrame(
            const std::filesystem::path& b16Path,
            const std::string& spriteName,
            int section, int frame,
            const std::vector<uint8_t>& palette);
    };
}
