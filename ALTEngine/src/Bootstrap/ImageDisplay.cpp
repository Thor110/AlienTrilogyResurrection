#include "ImageDisplay.h"
#include "AppWindow.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace ALTEngine::Bootstrap
{
    bool ImageDisplay::Show(const std::vector<uint8_t>& rgba, int width, int height, uint32_t maxDurationMs)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return false;
        }
        SDL_Window* window = app.Window();
        SDL_Renderer* renderer = app.Renderer();
        (void)window; // fullscreen render target - no direct window manipulation needed here

        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
        if (!texture)
        {
            SDL_Log("ImageDisplay: SDL_CreateTexture failed: %s", SDL_GetError());
            return false;
        }
        SDL_UpdateTexture(texture, nullptr, rgba.data(), width * 4);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST); // preserve the original's blocky look, no smoothing

        bool closedByUser = false;
        bool running = true;
        Uint64 startTicks = SDL_GetTicks();

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { closedByUser = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN) { running = false; }
                else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { running = false; }
            }
            if (maxDurationMs > 0 && SDL_GetTicks() - startTicks >= maxDurationMs) { running = false; }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            // Letterbox: scale to fit while preserving aspect ratio.
            float scale = std::min(static_cast<float>(windowW) / width, static_cast<float>(windowH) / height);
            float destW = width * scale;
            float destH = height * scale;
            SDL_FRect dest{ (windowW - destW) / 2.0f, (windowH - destH) / 2.0f, destW, destH };

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
            SDL_RenderPresent(renderer);
        }

        SDL_DestroyTexture(texture);

        return !closedByUser;
    }
}
