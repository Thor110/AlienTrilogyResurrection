#include "GameLocator.h"
#include "DirectoryBrowser.h"
#include "PlatformPaths.h"
#include "FsUtil.h"
#include "InstallPipeline.h"

#include <iostream>

#include <string>

namespace ALTEngine::Bootstrap
{
    namespace
    {
        constexpr const char* CONFIG_KEY_GAME_DIR = "GameDirectory";
    }

    GameLocator::GameLocator(Config& config)
        : config(config)
    {
    }

    std::optional<std::filesystem::path> GameLocator::FindEntry(const std::filesystem::path& directory, const std::string& name) const
    {
        return FindEntryCaseInsensitive(directory, name);
    }

    bool GameLocator::HasEntry(const std::filesystem::path& directory, const std::string& name) const
    {
        return HasEntryCaseInsensitive(directory, name);
    }

    bool GameLocator::Validate(const std::filesystem::path& directory) const
    {
        auto cdDir = FindEntry(directory, "CD");
        if (!cdDir.has_value()) { return false; }

        auto gfxDir = FindEntry(*cdDir, "GFX");
        if (!gfxDir.has_value()) { return false; }

        // Specifically LEGAL.BND, not the .B16/.16 fallback other GFX
        // lookups use - not every file has a compressed B16 counterpart
        // (some do, lower quality matching the PS1/Saturn versions - e.g.
        // enemy sprites - but that's inconsistent per-file, not something
        // to rely on here), and LEGAL.BND specifically is confirmed
        // present on every release regardless of executable name (e.g.
        // the German release ships WTRILOGY.EXE, not TRILOGY.EXE).
        //
        // Used to also check for TRILOGY.ICO at the install root, since
        // it was another release-independent marker - removed (Edward,
        // 2026) once TRILOGY.ICO stopped being copied during install
        // (the low-quality disc icon was replaced by a higher-quality
        // one embedded directly into the executable at build time
        // instead), which would otherwise fail validation for every
        // fresh install going forward. LEGAL.BND alone was already
        // sufficient on its own.
        return HasEntry(*gfxDir, "LEGAL.BND");
    }

    std::optional<std::filesystem::path> GameLocator::SourceRoot(const std::filesystem::path& chosen) const
    {
        if (chosen.empty()) { return std::nullopt; }
        if (Validate(chosen)) { return chosen; }
        const std::filesystem::path parent = chosen.parent_path();
        if (parent.empty() || parent == chosen) { return std::nullopt; }
        // Its CD folder (GFX directly inside), or a folder beside it (HDD).
        if (auto gfx = FindEntry(chosen, "GFX"); gfx && HasEntry(*gfx, "LEGAL.BND") && Validate(parent)) { return parent; }
        if (Validate(parent)) { return parent; }
        return std::nullopt;
    }

    bool GameLocator::IsEngineInstall(const std::filesystem::path& directory) const
    {
        auto cdDir = FindEntry(directory, "CD");
        return cdDir.has_value() && HasEntry(*cdDir, "LevelCleanup.json");
    }

    std::optional<std::filesystem::path> GameLocator::TryConfigPath() const
    {
        auto saved = config.Get(CONFIG_KEY_GAME_DIR);
        if (!saved.has_value()) { return std::nullopt; }

        const std::filesystem::path path = PathFromText(*saved);
        if (Validate(path)) { return path; }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> GameLocator::TryExecutableDirectory() const
    {
        std::filesystem::path here = ExecutableDirectory();
        if (Validate(here)) { return here; }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> GameLocator::TryAutoLocate() const
    {
        if (auto fromConfig = TryConfigPath()) { return fromConfig; }
        if (auto fromExeDir = TryExecutableDirectory()) { return fromExeDir; }
        return std::nullopt;
    }

    LocateResult GameLocator::Locate()
    {
        if (auto fromConfig = TryConfigPath())
        {
            return { true, *fromConfig };
        }

        if (auto fromExeDir = TryExecutableDirectory())
        {
            config.Set(CONFIG_KEY_GAME_DIR, PathText(*fromExeDir));
            return { true, *fromExeDir };
        }

        DirectoryBrowser browser;
        auto located = browser.Run(
            "SEARCHING FOR MU/TH/UR 9000 DIRECTORY",
            "SELECT ALIEN TRILOGY INSTALL LOCATION",
            [this](const std::filesystem::path& path) { return SourceRoot(path).has_value(); });

        if (!located.has_value())
        {
            return { false, {} };
        }
        located = SourceRoot(*located);
        if (!located.has_value()) { return { false, {} }; }

        // An existing installation of the original game (or a copy of the
        // disc) is installed from, into ALTEngine's own GameData, as a disc
        // is: the patches change the files, which must never happen to the
        // original game's own install - and it may sit somewhere unwritable,
        // such as Program Files.
        if (!IsEngineInstall(*located))
        {
            const std::filesystem::path destination = ExecutableDirectory() / "GameData";
            std::cout << "Installing from the existing installation at " << PathText(*located) << "\n";
            if (!InstallFromDisc(*located, destination) || !Validate(destination))
            {
                // Most likely a repackaged release (the Collection Chamber one,
                // for instance) that keeps the game in its own layout and has
                // left some of the disc's files out: not supported - ALTEngine
                // installs from the disc, or a full copy of its files.
                std::cout << "The install from " << PathText(*located) << " did not complete: the files listed above are\n"
                          << "missing there. ALTEngine installs from the original disc (or a mounted image of it), or from\n"
                          << "a complete copy of the disc's files; repackaged releases such as Collection Chamber's keep the\n"
                          << "game in their own layout and are not supported.\n";
                return { false, {} };
            }
            located = destination;
        }

        config.Set(CONFIG_KEY_GAME_DIR, PathText(*located));
        return { true, *located };
    }
}
