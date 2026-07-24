#pragma once

#include <filesystem>
#include <optional>

#include "../Bootstrap/Localization.h"

namespace ALTEngine::Formats
{
    // OBJ3D.BND models don't all share one texture the way PICKMOD's do -
    // confirmed against ModelRenderer.cs (Edward, 2026), lines around
    // its ExportModel "special" (OBJ3D) branch:
    //   - mesh numbers 3-18, or 35 (lockers & coil obstacle)  -> PNL0GFX
    //   - mesh numbers 19-34, or 41 (boneship switches & egg husk) -> PNL1GFX
    //   - everything else (0-2, 36-40 - pylon and computer)  -> PICKGFX
    //
    // PNL0GFX/PNL1GFX live in CD/LANGUAGE (not CD/GFX like every other
    // texture file so far) and use the same U/E/F/I/S suffix convention
    // as MISSION#.TXT - confirmed against the real PNL0GFXE.16/
    // PNL1GFXE.16 (Edward, 2026: "note there are U/E/F/I/S versions
    // across different versions of the game"). PICKGFX has no language
    // variant at all, and lives in CD/GFX as already established.
    //
    // Structurally, all three (PICKGFX/PNL0GFX/PNL1GFX) are the same
    // "single shared texture" shape ModelRenderer already auto-detects
    // (exactly one BX section) - confirmed against the real
    // PNL0GFXE.16/PNL1GFXE.16 (453 rects each, matching PICKGFX's shape).
    //
    // This is foundation work - nothing currently loads OBJ3D models
    // (the pause menu only uses PICKMOD), so this is unverified against
    // an actual rendered OBJ3D model yet, only against the raw texture
    // files' own structure.
    std::optional<std::filesystem::path> ResolveObj3DTextureFile(
        const std::filesystem::path& cdDirectory, int meshNumber, ALTEngine::Bootstrap::Language language);
}
