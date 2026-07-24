#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Renderer/ModelRenderer.h"

#include <SDL3/SDL.h>

namespace ALTEngine::Screens
{
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;

    GameplayResult GameplayScreen::Run(
        const std::filesystem::path& cdDirectory,
        Bootstrap::Language language,
        const std::string& missionLevelCode)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { GameplayOutcome::WindowClosed };
        }
        SDL_Renderer* renderer = app.Renderer();

        // Placeholder inventory - see PlayerInventoryState.h. Swap for
        // real player state once one exists.
        PlayerInventoryState inventory;

        GameplayResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.outcome = GameplayOutcome::WindowClosed; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                {
                    PauseMenuResult pauseResult = PauseMenuScreen::Run(cdDirectory, language, missionLevelCode, inventory);
                    if (pauseResult.outcome == PauseMenuOutcome::WindowClosed)
                    {
                        result.outcome = GameplayOutcome::WindowClosed;
                        running = false;
                    }
                    else if (pauseResult.outcome == PauseMenuOutcome::ExitGame)
                    {
                        result.outcome = GameplayOutcome::ExitGame;
                        running = false;
                    }
                    // else Resumed - just keep going
                }
            }
            if (!running) { break; }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            DrawBitmapText(renderer, "(gameplay placeholder - press Escape for the pause menu)", 24, 24, 2, Color{ 24, 130, 52, 255 });
            SDL_RenderPresent(renderer);
        }

        ALTEngine::Renderer::ModelRenderer::Shutdown();
        return result;
    }
}
