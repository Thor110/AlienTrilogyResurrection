#include "AppWindow.h"

namespace ALTEngine::Bootstrap
{
    AppWindow& AppWindow::Instance()
    {
        static AppWindow instance;
        return instance;
    }

    bool AppWindow::EnsureCreated()
    {
        if (window && renderer) { return true; }

        if (!sdlInitialized)
        {
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
            {
                SDL_Log("AppWindow: SDL_Init failed: %s", SDL_GetError());
                return false;
            }
            sdlInitialized = true;
        }

        window = SDL_CreateWindow("ALTEngine", 1280, 720, SDL_WINDOW_FULLSCREEN);
        if (!window)
        {
            SDL_Log("AppWindow: SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }

        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer)
        {
            SDL_Log("AppWindow: SDL_CreateRenderer failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            window = nullptr;
            return false;
        }
        SDL_SetRenderVSync(renderer, 1);

        return true;
    }

    bool AppWindow::ApplyFullscreenResolution(int width, int height)
    {
        if (!window) { return false; }

        SDL_DisplayID display = SDL_GetDisplayForWindow(window);
        if (display == 0) { return false; }

        SDL_DisplayMode closest{};
        if (!SDL_GetClosestFullscreenDisplayMode(display, width, height, 0.0f, false, &closest))
        {
            SDL_Log("AppWindow::ApplyFullscreenResolution: no matching mode for %dx%d", width, height);
            return false;
        }

        if (!SDL_SetWindowFullscreenMode(window, &closest))
        {
            SDL_Log("AppWindow::ApplyFullscreenResolution: SDL_SetWindowFullscreenMode failed: %s", SDL_GetError());
            return false;
        }
        return true;
    }

    void AppWindow::SetVSync(bool enabled)
    {
        if (!renderer) { return; }
        SDL_SetRenderVSync(renderer, enabled ? 1 : 0);
    }

    void AppWindow::SetDisplayMode(DisplayMode mode, int windowedWidth, int windowedHeight)
    {
        if (!window) { return; }

        switch (mode)
        {
        case DisplayMode::Windowed:
            SDL_SetWindowFullscreen(window, false);
            SDL_SetWindowSize(window, windowedWidth, windowedHeight);
            break;
        case DisplayMode::Fullscreen:
            SDL_SetWindowFullscreen(window, true);
            // Leaves whatever exclusive mode ApplyFullscreenResolution
            // last set (or SDL's own default if that was never called)
            // rather than picking a resolution here - Resolution is a
            // separate menu entry, this only switches which kind of
            // fullscreen is active.
            break;
        case DisplayMode::Borderless:
            SDL_SetWindowFullscreen(window, true);
            SDL_SetWindowFullscreenMode(window, nullptr); // nullptr = match the desktop's own current mode, not a specific exclusive one
            break;
        }
    }

    void AppWindow::SetWindowedSize(int width, int height)
    {
        if (!window) { return; }
        SDL_SetWindowSize(window, width, height);
    }

    SDL_AudioStream* AppWindow::SfxAudioStream()
    {
        if (sfxStream) { return sfxStream; }
        if (!window) { return nullptr; }

        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_U8;
        spec.channels = 1;
        spec.freq = 11025;

        sfxStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!sfxStream)
        {
            SDL_Log("AppWindow::SfxAudioStream: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
            return nullptr;
        }
        SDL_ResumeAudioStreamDevice(sfxStream);
        return sfxStream;
    }

    SDL_AudioStream* AppWindow::MusicAudioStream(const SDL_AudioSpec& spec)
    {
        if (!window) { return nullptr; }

        bool specMatches = musicStream &&
            musicSpec.format == spec.format && musicSpec.channels == spec.channels && musicSpec.freq == spec.freq;
        if (specMatches) { return musicStream; }

        if (musicStream) { SDL_DestroyAudioStream(musicStream); musicStream = nullptr; }

        musicStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!musicStream)
        {
            SDL_Log("AppWindow::MusicAudioStream: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
            return nullptr;
        }
        musicSpec = spec;
        SDL_ResumeAudioStreamDevice(musicStream);
        return musicStream;
    }

    void AppWindow::Shutdown()
    {
        if (sfxStream) { SDL_DestroyAudioStream(sfxStream); sfxStream = nullptr; }
        if (musicStream) { SDL_DestroyAudioStream(musicStream); musicStream = nullptr; }
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        if (sdlInitialized) { SDL_Quit(); sdlInitialized = false; }
    }
}
