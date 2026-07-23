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

    void AppWindow::Shutdown()
    {
        if (sfxStream) { SDL_DestroyAudioStream(sfxStream); sfxStream = nullptr; }
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        if (sdlInitialized) { SDL_Quit(); sdlInitialized = false; }
    }
}
