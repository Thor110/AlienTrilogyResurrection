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
