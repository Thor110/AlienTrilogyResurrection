#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Bootstrap
{
    // Full-screen SDL2 folder browser styled after the MU/TH/UR 9000
    // interface: green-on-black, bordered panels, bitmap font. Used to
    // locate the Alien Trilogy install directory when it can't be found
    // automatically.
    //
    // Deliberately knows nothing about what makes a directory "valid" -
    // that's supplied by the caller via `validate`, so this stays reusable
    // for any future "point me at a folder" prompts the engine needs.
    class DirectoryBrowser
    {
    public:
        // Runs the full-screen browser loop. Returns the chosen directory
        // once the user opens a directory that passes `validate` and
        // presses SELECT, or std::nullopt if the window is closed first.
        //
        // `headerText` is the line shown top-left (e.g. "SEARCHING FOR
        // MU/TH/UR 9000 DIRECTORY"). `panelTitle` is shown in the panel's
        // title bar (e.g. "SELECT ALIEN TRILOGY INSTALL LOCATION").
        std::optional<std::filesystem::path> Run(
            const std::string& headerText,
            const std::string& panelTitle,
            const std::function<bool(const std::filesystem::path&)>& validate);

    private:
        struct Entry
        {
            std::string displayName;
            std::filesystem::path fullPath; // empty => "back to drive list"
            bool isParent = false;
        };

        // Empty path = drive list ("This PC" level).
        std::vector<Entry> BuildEntries(const std::filesystem::path& currentPath) const;
    };
}
