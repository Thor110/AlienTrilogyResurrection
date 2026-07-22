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

    void AppWindow::Shutdown()
    {
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        if (sdlInitialized) { SDL_Quit(); sdlInitialized = false; }
    }
}
