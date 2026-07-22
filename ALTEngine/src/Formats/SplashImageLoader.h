#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ALTEngine::Formats
{
    struct SplashImage
    {
        std::vector<uint8_t> rgba; // width x height, RGBA8888
        int width = 0;
        int height = 0;
    };

    // Loads one of the five external-palette, two-frame splash/briefing
    // images: LEGAL, LOGOSGFX, COLONY, PRISHOLD, BONESHIP - the only 5
    // files in the game using this scheme; all are menu backgrounds.
    //
    // Confirmed against the real LEGAL.BND + its BX metadata: each TP
    // frame is a raw 256x256 8bpp buffer with no sub-header. Frame A
    // contributes 240x240, frame B contributes only 80x240 - NOT a
    // symmetric 240+240. Final image is 320x240 (native PS1 output
    // resolution). Crop dimensions are read from each file's own BX
    // metadata rather than hardcoded, since this asymmetry means there's
    // no safe universal constant.
    //
    // LOGOSGFX specifically contains 2 separate images across 4 TP
    // frames (not 1 image across 2 frames like the other four) - pass
    // `imageIndex` to select which pair (0 = frames 0+1, 1 = frames 2+3).
    // This pairing (TP frames and BX sections both consumed 2-at-a-time,
    // positionally) is inferred from the file structure, not yet verified
    // against the actual LOGOSGFX.BND - flag if it turns out wrong.
    class SplashImageLoader
    {
    public:
        static SplashImage Load(
            const std::filesystem::path& bndPath,
            const std::filesystem::path& palPath,
            bool paletteTrimmed,
            int imageIndex = 0);
    };
}
