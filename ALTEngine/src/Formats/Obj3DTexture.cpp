#include "Obj3DTexture.h"

namespace ALTEngine::Formats
{
    namespace
    {
        std::optional<std::filesystem::path> ResolveLanguageSuffixedFile(
            const std::filesystem::path& dir, const std::string& baseName, const std::string& ext, Bootstrap::Language language)
        {
            for (char suffix : Bootstrap::LanguageSuffixCandidates(language))
            {
                std::filesystem::path candidate = dir / (baseName + suffix + ext);
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec)) { return candidate; }
            }
            return std::nullopt;
        }
    }

    std::optional<std::filesystem::path> ResolveObj3DTextureFile(
        const std::filesystem::path& cdDirectory, int meshNumber, Bootstrap::Language language)
    {
        if ((meshNumber >= 3 && meshNumber <= 18) || meshNumber == 35)
        {
            return ResolveLanguageSuffixedFile(cdDirectory / "LANGUAGE", "PNL0GFX", ".16", language);
        }
        if ((meshNumber >= 19 && meshNumber <= 34) || meshNumber == 41)
        {
            return ResolveLanguageSuffixedFile(cdDirectory / "LANGUAGE", "PNL1GFX", ".16", language);
        }

        // 0-2 and 36-40 (pylon and computer) - "fine to default"/"uses
        // PICKGFX" per ModelRenderer.cs - no language variant at all.
        std::filesystem::path path = cdDirectory / "GFX" / "PICKGFX.BND";
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) { return path; }
        return std::nullopt;
    }
}
