#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // Resolves a logical, extension-less asset path (e.g. "GFX/LEGAL",
    // "SECT11/111GFX") against an Override tree first, falling back to the
    // original game data when no override exists.
    //
    // Override assets are expected to mirror the CD folder structure under
    // a separate root, e.g.:
    //   Override/GFX/LEGAL.png
    //   Override/SECT11/111GFX/<frame>.png
    // ...so an artist/preservationist can drop in higher-quality PNGs
    // without touching the original BND/B16 files at all. ALTViewer is the
    // intended tool for producing the originals to start from.
    //
    // This class only resolves paths - it doesn't know how to decode
    // either format. Callers (the eventual texture/graphics loader in
    // ALTFormats) decide what to do with whichever path comes back.
    class AssetResolver
    {
    public:
        AssetResolver(std::filesystem::path cdRoot, std::filesystem::path overrideRoot)
            : cdRoot(std::move(cdRoot)), overrideRoot(std::move(overrideRoot))
        {
        }

        // Returns the override file for `relativePath` if one exists under
        // the override root with any of `extensions`, else std::nullopt -
        // meaning the caller should fall back to the original data.
        std::optional<std::filesystem::path> FindOverride(
            const std::filesystem::path& relativePath,
            const std::vector<std::string>& extensions = { ".png", ".dds", ".tga" }) const
        {
            if (overrideRoot.empty()) { return std::nullopt; }

            for (const auto& ext : extensions)
            {
                std::filesystem::path candidate = overrideRoot / relativePath;
                candidate += ext;

                std::error_code ec;
                if (std::filesystem::exists(candidate, ec)) { return candidate; }
            }
            return std::nullopt;
        }

        // The path to the original data for `relativePath`, relative to
        // the CD root - no existence check, since the original archive
        // format (BND/B16/etc) is opened and parsed, not checked as a
        // plain file.
        std::filesystem::path Original(const std::filesystem::path& relativePath) const
        {
            return cdRoot / relativePath;
        }

        bool HasOverrideRoot() const { return !overrideRoot.empty(); }

    private:
        std::filesystem::path cdRoot;
        std::filesystem::path overrideRoot;
    };
}
