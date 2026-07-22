#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Bootstrap
{
    class ImageDisplay
    {
    public:
        // Shows an RGBA8888 image (width x height) fullscreen, scaled to
        // fit preserving aspect ratio (letterboxed), until a key is
        // pressed, the mouse is clicked, or `maxDurationMs` elapses
        // (0 = wait indefinitely). Returns false if the window was closed
        // via the OS close control - caller should treat that as "abort
        // boot", same as DirectoryBrowser's std::nullopt.
        static bool Show(const std::vector<uint8_t>& rgba, int width, int height, uint32_t maxDurationMs = 0);
    };
}
