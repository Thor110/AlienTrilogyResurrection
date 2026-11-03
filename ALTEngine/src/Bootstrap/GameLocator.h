#pragma once

#include <filesystem>
#include <optional>

#include "Config.h"

namespace ALTEngine::Bootstrap
{
    struct LocateResult
    {
        bool success = false;
        std::filesystem::path gameDirectory;
    };

    // Finds the Alien Trilogy install directory: checks the saved config
    // first, then the executable's own directory, then falls back to the
    // full-screen DirectoryBrowser (MU/TH/UR 9000 style folder picker).
    //
    // A valid install directory contains TRILOGY.EXE or RUN.EXE, and a
    // CD\GFX subdirectory (game files live under FOLDER\CD\GFX, not
    // directly under FOLDER\GFX).
    class GameLocator
    {
    public:
        explicit GameLocator(Config& config);

        LocateResult Locate();

        // Just the config/exe-directory checks, no manual browser fallback.
        // Used by main.cpp to decide whether disc detection/install is
        // worth attempting before falling back to Locate()'s full flow.
        std::optional<std::filesystem::path> TryAutoLocate() const;

        bool Validate(const std::filesystem::path& directory) const;

        // The folder an existing installation's game files can be installed
        // from - one holding CD\GFX\LEGAL.BND, the layout of the disc itself -
        // when `chosen` is that folder, its CD folder, or a folder beside its
        // CD folder (an install's HDD folder). Empty if it is none of these.
        std::optional<std::filesystem::path> SourceRoot(const std::filesystem::path& chosen) const;

        // An install ALTEngine made (and patches): its CD folder carries the
        // patcher's LevelCleanup.json, written every start. Anything else -
        // the original game installed, a copy of the disc - is copied into
        // ALTEngine's own GameData rather than patched where it is.
        bool IsEngineInstall(const std::filesystem::path& directory) const;

    private:
        Config& config;

        std::optional<std::filesystem::path> TryConfigPath() const;
        std::optional<std::filesystem::path> TryExecutableDirectory() const;

        // Case-insensitive check for an entry (file or directory) directly
        // inside `directory`, e.g. HasEntry(dir, "GFX").
        bool HasEntry(const std::filesystem::path& directory, const std::string& name) const;

        // Case-insensitive lookup of an entry's actual on-disk path, or
        // std::nullopt if not present.
        std::optional<std::filesystem::path> FindEntry(const std::filesystem::path& directory, const std::string& name) const;
    };
}
