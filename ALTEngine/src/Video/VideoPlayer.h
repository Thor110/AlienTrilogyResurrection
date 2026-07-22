#pragma once

#include <filesystem>

namespace ALTEngine::Video
{
    class VideoPlayer
    {
    public:
        // Plays `path` fullscreen (audio + video, synced to the
        // container's own packet timestamps - correct regardless of the
        // exact nominal frame rate) until it finishes naturally, the user
        // skips (any key or mouse click), or the window is closed.
        //
        // Returns false only if the window was closed outright (treated
        // as an abort, same as DirectoryBrowser/ImageDisplay); true for
        // both "finished" and "skipped", since both mean "continue
        // booting".
        static bool Play(const std::filesystem::path& path);
    };
}
