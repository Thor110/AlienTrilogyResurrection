#include "ImageDisplay.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace ALTEngine::Bootstrap
{
    bool ImageDisplay::Show(const std::vector<uint8_t>& rgba, int width, int height, uint32_t maxDurationMs)
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            SDL_Log("ImageDisplay: SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        SDL_Window* window = SDL_CreateWindow("ALTEngine", width, height, SDL_WINDOW_FULLSCREEN);
        if (!window)
        {
            SDL_Log("ImageDisplay: SDL_CreateWindow failed: %s", SDL_GetError());
            SDL_Quit();
            return false;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer)
        {
            SDL_Log("ImageDisplay: SDL_CreateRenderer failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        SDL_SetRenderVSync(renderer, 1);

        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
        if (!texture)
        {
            SDL_Log("ImageDisplay: SDL_CreateTexture failed: %s", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
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
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return !closedByUser;
    }
}
