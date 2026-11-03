#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ALTEngine::Game::TextureBlank
{
    // TEXTURE BLANKING - the last step of the level cleanup, after the fixes
    // (Patches.json) and the Level Cleanup's removals and additions. Every
    // texel of a level's sheet (NNNGFX.B16) that no texture the level draws
    // covers is set to that page's background colour: the texels of the
    // texture rectangles nothing uses (Game/TextureUse.h) that no used
    // rectangle overlaps, and the texels no rectangle covers at all. Nothing
    // drawn changes; long runs of one byte compress to nearly nothing, and
    // nothing unrelated bleeds in at a face's edge.
    //
    // The work is done once, by ALTTextureBlankList, against the sheets as
    // the fixes leave them; its result ships as data/TextureBlankList.json:
    // per sheet, the CRC32 of the sheet it was worked out from, the unused
    // texture indices and each page's background index. The install only
    // applies it (Apply), and only to a sheet with that CRC, keeping the
    // sheet as it was beside it (<sheet>.unblanked) to put back.
    struct Plan
    {
        std::vector<uint32_t> unused;        // texture indices (the sheet's flat BX order) nothing draws
        std::map<int, int> background;       // page -> palette index
    };

    struct Report
    {
        size_t texelsChanged = 0;            // bytes of the sheet that differ afterwards
        size_t texelsFilled = 0;             // texels set to a background, changed or not
        int pages = 0;
    };

    // Applies `plan` to a sheet's whole image in place. False (and `error`)
    // if it is not a sheet this understands or the plan names a texture or
    // page it does not have; `bytes` is then untouched.
    bool Apply(std::vector<uint8_t>& bytes, const Plan& plan, Report& report, std::string& error);

    // The sheets the blanking applies to: each level's NNNGFX.B16, beside it
    // in its SECT folder, relative to the CD folder.
    std::vector<std::filesystem::path> SheetFiles(const std::filesystem::path& cdDirectory);
}
