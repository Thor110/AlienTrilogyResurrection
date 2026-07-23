#include "GameLocator.h"
#include "DirectoryBrowser.h"
#include "PlatformPaths.h"
#include "FsUtil.h"

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
        // TRILOGY.ICO is present on every release; the executable name
        // isn't (e.g. the German release ships WTRILOGY.EXE, not
        // TRILOGY.EXE) - so check the icon, not the exe.
        if (!HasEntry(directory, "TRILOGY.ICO")) { return false; }

        auto cdDir = FindEntry(directory, "CD");
        if (!cdDir.has_value()) { return false; }

        auto gfxDir = FindEntry(*cdDir, "GFX");
        if (!gfxDir.has_value()) { return false; }

        // Specifically LEGAL.BND, not the .B16/.16 fallback other GFX
        // lookups use - not every file has a compressed B16 counterpart
        // (some do, lower quality matching the PS1/Saturn versions - e.g.
        // enemy sprites - but that's inconsistent per-file, not something
        // to rely on here), and LEGAL.BND specifically is confirmed
        // present on every release.
        return HasEntry(*gfxDir, "LEGAL.BND");
    }

    std::optional<std::filesystem::path> GameLocator::TryConfigPath() const
    {
        auto saved = config.Get(CONFIG_KEY_GAME_DIR);
        if (!saved.has_value()) { return std::nullopt; }

        std::filesystem::path path(*saved);
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
            config.Set(CONFIG_KEY_GAME_DIR, fromExeDir->string());
            return { true, *fromExeDir };
        }

        DirectoryBrowser browser;
        auto located = browser.Run(
            "SEARCHING FOR MU/TH/UR 9000 DIRECTORY",
            "SELECT ALIEN TRILOGY INSTALL LOCATION",
            [this](const std::filesystem::path& path) { return Validate(path); });

        if (!located.has_value())
        {
            return { false, {} };
        }

        config.Set(CONFIG_KEY_GAME_DIR, located->string());
        return { true, *located };
    }
}
