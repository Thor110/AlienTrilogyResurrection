#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
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
        // `transparentRgb`, if set, is applied to every texture decoded
        // in this call (see RawImageRenderer::RenderRGBA) - opt-in per
        // call rather than per-texture, since a caller only ever passes
        // this when they already know the specific file needs it (e.g.
        // OPTGFX.BND for the Music/SFX speaker and Multitap models).
        //
        // `perTextureTransparentIndices`, if non-empty, gives PALETTE
        // INDEX transparency per texture position (i.e. [i] applies to
        // the i-th TP section decoded, matching group order) - needed
        // for level textures, where transparency is index-based (not
        // colour-based) and genuinely different per texture group and
        // per level ID (confirmed against Edward's AlienTrilogyMapLoader.cs
        // GetTransparencyValues, 2026). Out-of-range or empty entries
        // mean "no index-based transparency for that texture".
        static BndTextureSet Load(const std::filesystem::path& bndPath,
                                   std::optional<std::array<uint8_t, 3>> transparentRgb = std::nullopt,
                                   const std::vector<std::vector<int>>& perTextureTransparentIndices = {});
    };
}
