#include "SaveSlotScreen.h"

#include <utility>
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
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;

    namespace
    {
        constexpr Color COLOR_BRIGHT{ 51, 255, 102, 255 };
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };
    }

    namespace
    {
        std::filesystem::path& SaveDirectoryStorage()
        {
            static std::filesystem::path directory = "Saves";
            return directory;
        }

        std::optional<ALTEngine::Game::SaveData>& CheckpointStorage()
        {
            static std::optional<ALTEngine::Game::SaveData> checkpoint;
            return checkpoint;
        }

        std::pair<std::string, std::string>& PendingRestoreStorage()
        {
            static std::pair<std::string, std::string> pending;   // level, snapshot
            return pending;
        }
    }

    void SetSaveDirectory(const std::filesystem::path& directory) { SaveDirectoryStorage() = directory; }
    const std::filesystem::path& SaveDirectory() { return SaveDirectoryStorage(); }

    std::array<SaveSlotInfo, 10> CurrentSaveSlots()
    {
        std::array<SaveSlotInfo, 10> slots{};
        const auto names = ALTEngine::Game::SaveGame::ListNames(SaveDirectory());
        for (size_t i = 0; i < slots.size(); ++i)
        {
            if (names[i]) { slots[i] = { *names[i], true }; }
        }
        return slots;
    }

    void SetCheckpoint(const ALTEngine::Game::SaveData& checkpoint) { CheckpointStorage() = checkpoint; }
    void ClearCheckpoint() { CheckpointStorage().reset(); }
    bool HasCheckpoint() { return CheckpointStorage().has_value(); }
    std::optional<ALTEngine::Game::SaveData> CurrentCheckpoint() { return CheckpointStorage(); }

    void SetPendingRestore(const std::string& levelDigits, const std::string& snapshot)
    {
        PendingRestoreStorage() = { levelDigits, snapshot };
    }

    std::string TakePendingRestore(const std::string& levelDigits)
    {
        auto& pending = PendingRestoreStorage();
        std::string snapshot = pending.first == levelDigits ? pending.second : std::string{};
        pending = {};
        return snapshot;
    }

    bool SaveCheckpointToSlot(int slot)
    {
        if (!CheckpointStorage()) { return false; }
        const bool ok = ALTEngine::Game::SaveGame::Write(SaveDirectory(), slot, *CheckpointStorage());
        SDL_Log("SaveSlots: %s %s to slot %d", ok ? "saved" : "FAILED to save",
                CheckpointStorage()->name.c_str(), slot);
        return ok;
    }

    SaveSlotResult SaveSlotScreen::Run(
        const std::filesystem::path& cdDirectory,
        SaveSlotMode mode,
        const std::array<SaveSlotInfo, 10>& slots,
        bool showBackground)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, std::nullopt };
        }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = showBackground ? LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH) : nullptr;

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

            int scale = ComputeMenuScale(renderer);
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
