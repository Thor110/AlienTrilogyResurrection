#include "RawImageRenderer.h"

#include <algorithm>
#include <stdexcept>

namespace ALTEngine::Formats
{
    std::vector<uint8_t> RawImageRenderer::RenderRGBA(
        const std::vector<uint8_t>& pixelData,
        const std::vector<uint8_t>& palette,
        int width, int height,
        const std::vector<int>& transparentIndices,
        std::optional<std::array<uint8_t, 3>> transparentRgb)
    {
        int colors = static_cast<int>(palette.size()) / 3;
        std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4, 0);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                size_t idx = static_cast<size_t>(y) * width + x;
                if (idx >= pixelData.size()) { continue; }

                uint8_t colorIndex = pixelData[idx];
                size_t outOffset = idx * 4;

                if (colorIndex < colors)
                {
                    out[outOffset + 0] = static_cast<uint8_t>(palette[colorIndex * 3 + 0] * 4);
                    out[outOffset + 1] = static_cast<uint8_t>(palette[colorIndex * 3 + 1] * 4);
                    out[outOffset + 2] = static_cast<uint8_t>(palette[colorIndex * 3 + 2] * 4);
                    out[outOffset + 3] = 255;
                }

                if (std::find(transparentIndices.begin(), transparentIndices.end(), static_cast<int>(colorIndex))
                    != transparentIndices.end())
                {
                    out[outOffset + 3] = 0;
                }

                // Checked against the DECODED RGB value, not the palette
                // index - the same colour (e.g. pure black) can sit at a
                // different index in every model's own 256-colour
                // palette, so an index-based check alone can't reliably
                // target "this specific colour" across different
                // textures. Confirmed needed for OPTGFX's Music/SFX
                // speaker models and the Multitap model, which use a
                // colour (not index 0) as their transparency key, unlike
                // most other models where plain black just happens to be
                // opaque material colour (Edward, 2026).
                if (transparentRgb.has_value() &&
                    out[outOffset + 0] == (*transparentRgb)[0] &&
                    out[outOffset + 1] == (*transparentRgb)[1] &&
                    out[outOffset + 2] == (*transparentRgb)[2])
                {
                    out[outOffset + 3] = 0;
                }
            }
        }

        return out;
    }

    std::vector<uint8_t> RawImageRenderer::Convert16BitPaletteToRGB(const std::vector<uint8_t>& rawPalette)
    {
        if (rawPalette.size() < 2)
        {
            throw std::runtime_error("RawImageRenderer::Convert16BitPaletteToRGB: palette data is missing or too short.");
        }

        size_t colorCount = rawPalette.size() / 2;
        std::vector<uint8_t> rgbPalette(256 * 3, 0);

        for (size_t i = 0; i < colorCount && i < 256; ++i)
        {
            uint16_t color = static_cast<uint16_t>((rawPalette[i * 2 + 1] << 8) | rawPalette[i * 2]);
            int r = (color & 0x1F) * 63 / 31;
            int g = ((color >> 5) & 0x1F) * 63 / 31;
            int b = ((color >> 10) & 0x1F) * 63 / 31;

            rgbPalette[i * 3 + 0] = static_cast<uint8_t>(r);
            rgbPalette[i * 3 + 1] = static_cast<uint8_t>(g);
            rgbPalette[i * 3 + 2] = static_cast<uint8_t>(b);
        }

        return rgbPalette;
    }
}
