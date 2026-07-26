#include "SaveSlotScreen.h"
#include "MenuBackground.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Menu/MenuTree.h" // for ModelIndex::HarddriveLeft/HarddriveRight
#include "../Renderer/ModelRenderer.h"
#include "../Renderer/ModelPreview.h"

#include <SDL3/SDL.h>
#include <algorithm>

namespace ALTEngine::Screens
{
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;

    namespace
    {
        constexpr Color COLOR_BRIGHT{ 51, 255, 102, 255 };
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };
    }

    std::array<SaveSlotInfo, 10> StubSaveSlots()
    {
        std::array<SaveSlotInfo, 10> slots{};
        slots[0] = { "TWENTYTHIRD", true }; // matches the reference screenshots exactly
        return slots;
    }

    SaveSlotResult SaveSlotScreen::Run(
        const std::filesystem::path& cdDirectory,
        SaveSlotMode mode,
        const std::array<SaveSlotInfo, 10>& slots)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, std::nullopt };
        }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        // HarddriveRight ("Hard Drive Loading ->") for Load, HarddriveLeft
        // ("Hard Drive Saving <-") for Save - both confirmed OPTOBJ
        // indices.
        int harddriveModelIndex = (mode == SaveSlotMode::Load) ? ALTEngine::Menu::ModelIndex::HarddriveRight : ALTEngine::Menu::ModelIndex::HarddriveLeft;
        ALTEngine::Renderer::ModelPreviewSource modelSource = ALTEngine::Renderer::ModelPreviewSource::ForOptobj(cdDirectory, harddriveModelIndex);

        int cursor = 0;
        SaveSlotResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    switch (event.key.key)
                    {
                    case SDLK_UP:
                        cursor = (cursor + 9) % 10;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        cursor = (cursor + 1) % 10;
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        // Load only makes sense on a used slot - Save can
                        // target any slot (including overwriting a used
                        // one, or writing a fresh unused one).
                        if (mode == SaveSlotMode::Save || slots[static_cast<size_t>(cursor)].used)
                        {
                            result.selectedSlot = cursor + 1;
                            running = false;
                            SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        }
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            DrawMenuBackground(renderer, background, bgW, bgH);

            int scale = 3;
            int rowHeight = TextHeight(scale) + scale * 4;
            int margin = scale * 8;

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            // Model viewport fills the screen, drawn as a background
            // layer before any text - fixed position/size, not tied to
            // cursor position or any other dynamic value (Edward, 2026:
            // models should fill "the entire background or thereabouts",
            // and shouldn't jump around while navigating).
            float rotationAngle = static_cast<float>(SDL_GetTicks()) / 1000.0f;
            ALTEngine::Renderer::DrawModelPreview(renderer, modelSource, 0, 0, windowW, windowH, rotationAngle);

            std::string title = (mode == SaveSlotMode::Load) ? "Load Game" : "Save Game";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, margin, scale, COLOR_BRIGHT);

            for (int i = 0; i < 10; ++i)
            {
                std::string line = std::to_string(i + 1) + ": " + (slots[static_cast<size_t>(i)].used ? slots[static_cast<size_t>(i)].name : "-UNUSED-");
                Color color = (i == cursor) ? COLOR_BRIGHT : COLOR_DIM;
                DrawBitmapText(renderer, line, margin, margin + rowHeight * 3 + i * rowHeight, scale, color);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
