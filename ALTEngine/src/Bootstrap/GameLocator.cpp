#include "GameLocator.h"
#include "DirectoryBrowser.h"
#include "PlatformPaths.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ALTEngine::Bootstrap
{
    namespace
    {
        std::string ToLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        constexpr const char* CONFIG_KEY_GAME_DIR = "GameDirectory";
    }

    GameLocator::GameLocator(Config& config)
        : config(config)
    {
    }

    std::optional<std::filesystem::path> GameLocator::FindEntry(const std::filesystem::path& directory, const std::string& name) const
    {
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec))
        {
            return std::nullopt;
        }

        std::string target = ToLower(name);
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
        {
            if (ToLower(entry.path().filename().string()) == target)
            {
                return entry.path();
            }
        }
        return std::nullopt;
    }

    bool GameLocator::HasEntry(const std::filesystem::path& directory, const std::string& name) const
    {
        return FindEntry(directory, name).has_value();
    }

    bool GameLocator::Validate(const std::filesystem::path& directory) const
    {
        bool hasExecutable = HasEntry(directory, "TRILOGY.EXE") || HasEntry(directory, "RUN.EXE");
        if (!hasExecutable) { return false; }

        auto cdDir = FindEntry(directory, "CD");
        if (!cdDir.has_value()) { return false; }

        return HasEntry(*cdDir, "GFX");
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
