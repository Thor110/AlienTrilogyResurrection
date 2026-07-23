#pragma once

#include <SDL3/SDL.h>

namespace ALTEngine::Bootstrap
{
    // Single persistent SDL window/renderer for the whole boot sequence
    // (and eventually the game itself). DirectoryBrowser, ImageDisplay,
    // VideoPlayer etc all use this instead of each creating/destroying
    // their own SDL context - that per-component teardown is what caused
    // the visible minimize/restore flash between each intro video.
    //
    // Not thread-safe; assumed used from the main thread only, which
    // matches everything else in Bootstrap/.
    class AppWindow
    {
    public:
        static AppWindow& Instance();

        // Creates the window/renderer on first call (initializing SDL
        // video+audio); subsequent calls are no-ops. Returns false if
        // SDL/window/renderer creation failed.
        bool EnsureCreated();

        SDL_Window* Window() const { return window; }
        SDL_Renderer* Renderer() const { return renderer; }

        // Finds the closest available fullscreen display mode to
        // (width, height) on the window's current display and applies
        // it. Returns false if no window exists yet or no matching mode
        // was found. Safe to call repeatedly (e.g. every time the
        // Resolution menu selection changes).
        bool ApplyFullscreenResolution(int width, int height);

        // Call once, at actual program exit - not between boot stages.
        void Shutdown();

        AppWindow(const AppWindow&) = delete;
        AppWindow& operator=(const AppWindow&) = delete;

    private:
        AppWindow() = default;

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool sdlInitialized = false;
    };
}
