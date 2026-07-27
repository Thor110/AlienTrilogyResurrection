#pragma once

#include <SDL3/SDL.h>

namespace ALTEngine::Bootstrap
{
    // Edward, 2026: "Display Mode / Windowed / Fullscreen / Borderless
    // options in the graphics options list."
    enum class DisplayMode
    {
        Windowed,
        Fullscreen,  // exclusive fullscreen at a specific resolution - see ApplyFullscreenResolution
        Borderless,  // fullscreen "desktop" mode - matches the current desktop resolution, no exclusive mode switch
    };

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

        // Lazily opens a single persistent audio stream matching the
        // game's SFX format (8-bit unsigned PCM, mono, 11025Hz - per
        // SoundEffects.cs's CreateWavHeader defaults and ReplaceWAV's
        // format validation), reused across every SfxPlayer::Play call
        // rather than opening/closing a device per sound. Returns
        // nullptr if no window exists yet or the device couldn't open.
        SDL_AudioStream* SfxAudioStream();

        // Lazily opens (or reopens, if `spec` differs from whatever's
        // currently open) a persistent audio stream for music playback.
        // Separate from SfxAudioStream since music format isn't fixed
        // the way SFX is - ripped CDDA tracks are 16-bit/44100Hz/stereo,
        // but this just uses whatever spec the caller determined from
        // the actual loaded file (see MusicPlayer). Returns nullptr if
        // no window exists yet or the device couldn't open.
        SDL_AudioStream* MusicAudioStream(const SDL_AudioSpec& spec);

        // Finds the closest available fullscreen display mode to
        // (width, height) on the window's current display and applies
        // it. Returns false if no window exists yet or no matching mode
        // was found. Safe to call repeatedly (e.g. every time the
        // Resolution menu selection changes).
        bool ApplyFullscreenResolution(int width, int height);

        // Toggles vsync on the renderer - on by default (see
        // EnsureCreated). Safe to call repeatedly (Edward, 2026: "VSync
        // On / Off options in the Graphics options list").
        void SetVSync(bool enabled);

        // Switches between windowed/fullscreen/borderless. windowedWidth/
        // windowedHeight are used for the Windowed case - should be the
        // persisted Resolution setting, not a hardcoded default, so
        // switching to Windowed doesn't silently ignore whatever
        // resolution was actually selected.
        void SetDisplayMode(DisplayMode mode, int windowedWidth = 1280, int windowedHeight = 720);

        // Resizes the actual window directly - the missing piece for
        // resolution changes to take effect while in Windowed mode.
        // ApplyFullscreenResolution only sets the exclusive-fullscreen
        // display mode (SDL_SetWindowFullscreenMode), which has no
        // effect on window size at all while not actually in that mode
        // (Edward, 2026: "resolutions don't apply when in windowed
        // mode... it just seems to get stuck at whatever resolution is
        // set scaled down to windowed"). Safe to call regardless of
        // current display mode - only visually matters while Windowed.
        void SetWindowedSize(int width, int height);

        // Call once, at actual program exit - not between boot stages.
        void Shutdown();

        AppWindow(const AppWindow&) = delete;
        AppWindow& operator=(const AppWindow&) = delete;

    private:
        AppWindow() = default;

        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_AudioStream* sfxStream = nullptr;
        SDL_AudioStream* musicStream = nullptr;
        SDL_AudioSpec musicSpec{};
        bool sdlInitialized = false;
    };
}
