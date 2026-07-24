#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ALTEngine::Formats
{
    class RawImageRenderer
    {
    public:
        // Renders 8bpp indexed pixel data to an RGBA8888 buffer (4 bytes
        // per pixel, row-major). `palette` is 256 x RGB triplets, 6-bit
        // per channel (0-63) - values are scaled *4, matching
        // RenderRaw8bppImage exactly (max 252, not 255 - this is
        // deliberately not rescaled to preserve the original look).
        //
        // Any pixel whose index appears in `transparentIndices` gets
        // alpha 0 instead of an opaque colour. Unlike the C# original
        // (which has a separate "magenta" mode for indexed-PNG export
        // tooling), this always produces real alpha - there's no
        // equivalent authoring-tool use case here.
        //
        // `transparentRgb`, if set, ALSO makes any pixel whose decoded
        // RGB matches it transparent - checked by colour, not palette
        // index, since the same colour can sit at a different index in
        // different models' palettes. Needed for OPTGFX's Music/SFX
        // speaker models and the Multitap model specifically, which use
        // a colour key rather than most models' plain "black = opaque
        // material" convention (Edward, 2026).
        static std::vector<uint8_t> RenderRGBA(
            const std::vector<uint8_t>& pixelData,
            const std::vector<uint8_t>& palette,
            int width, int height,
            const std::vector<int>& transparentIndices = {},
            std::optional<std::array<uint8_t, 3>> transparentRgb = std::nullopt);

        // Converts an embedded 16-bit-per-colour BND palette (CL
        // sections) to the same 768-byte, 6-bit-per-channel (0-63) RGB
        // triplet format PaletteFile::Load produces for external .PAL
        // files, so both feed RenderRGBA identically.
        static std::vector<uint8_t> Convert16BitPaletteToRGB(const std::vector<uint8_t>& rawPalette);
    };
}
