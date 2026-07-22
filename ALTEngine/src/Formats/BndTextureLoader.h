#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "BxParser.h"

namespace ALTEngine::Formats
{
    struct BndTexture
    {
        std::string index;        // e.g. "00", "01" - the numeric suffix shared by TP{index}/CL{index}
        std::vector<uint8_t> rgba; // 256x256 RGBA8888, full - not cropped (unlike SplashImageLoader's five)
        int width = 256;
        int height = 256;
    };

    struct BndTextureSet
    {
        std::vector<BndTexture> textures;               // one per matched TP{NN}/CL{NN} pair, in file order
        std::vector<std::vector<BxRectangle>> uvSections; // one entry per BX{NN} section, in file order
    };

    // Loads the general case of a .BND/.B16 file: level textures, menus,
    // etc - each texture has its own embedded CL{NN} palette (not an
    // external .PAL like SplashImageLoader's five), and is used at its
    // full 256x256 size (no crop/padding trick).
    //
    // CL chunk data (via BndParser) is 516 bytes: a 4-byte sub-header
    // (unknown purpose, always observed as 01 00 00 00) followed by 512
    // bytes (256 x 16-bit RGB555) of actual palette - confirmed against
    // real CL00 data by inspecting the decoded values. The 4-byte skip
    // corresponds to the gap between the old ALTViewer.cs scanning code's
    // skipHeader=12 (from the section-name match) and BndParser's clean
    // skipHeader=8 (name+size) - i.e. 12-8=4.
    class BndTextureLoader
    {
    public:
        static BndTextureSet Load(const std::filesystem::path& bndPath);
    };
}
