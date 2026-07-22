#include "MenuController.h"
#include "MenuTree.h"
#include "MenuNavigation.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/SplashImageLoader.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Menu
{
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::Language;
    using ALTEngine::Bootstrap::RenderFidelity;
    using ALTEngine::Bootstrap::RenderSettings;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;
    using ALTEngine::Formats::SplashImage;
    using ALTEngine::Formats::SplashImageLoader;

    namespace
    {
        constexpr Color COLOR_BG{ 0, 0, 0, 255 };
        constexpr Color COLOR_GREEN{ 51, 255, 102, 255 };
        constexpr Color COLOR_GREEN_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_HIGHLIGHT_BG{ 0, 40, 15, 255 };

        std::optional<std::filesystem::path> ResolveGfxFile(const std::filesystem::path& gfxDir, const std::string& baseName)
        {
            for (const char* ext : { ".BND", ".B16", ".16" })
            {
                std::filesystem::path candidate = gfxDir / (baseName + ext);
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec)) { return candidate; }
            }
            return std::nullopt;
        }

        // Loads one of LOGOSGFX's 2 images (imageIndex 0 = main menu, 1 =
        // options/multiplayer/settings/credits) as an SDL texture.
        // Returns nullptr on any failure - callers render a plain black
        // background instead rather than crashing over a missing asset.
        SDL_Texture* LoadBackgroundTexture(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer, int imageIndex, int& outW, int& outH)
        {
            auto bndPath = ResolveGfxFile(cdDirectory / "GFX", "LOGOSGFX");
            std::filesystem::path palPath = cdDirectory / "PALS" / "LOGOSGFX.PAL";
            if (!bndPath.has_value())
            {
                SDL_Log("Menu: could not find LOGOSGFX graphics file");
                return nullptr;
            }
            std::error_code ec;
            if (!std::filesystem::exists(palPath, ec))
            {
                SDL_Log("Menu: could not find %s", palPath.string().c_str());
                return nullptr;
            }

            try
            {
                SplashImage image = SplashImageLoader::Load(*bndPath, palPath, /*paletteTrimmed*/ false, imageIndex);
                SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
                if (!texture) { return nullptr; }
                SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                outW = image.width;
                outH = image.height;
                return texture;
            }
            catch (const std::exception& e)
            {
                SDL_Log("Menu: failed to load LOGOSGFX image %d: %s", imageIndex, e.what());
                return nullptr;
            }
        }

        void DrawBackground(SDL_Renderer* renderer, SDL_Texture* texture, int texW, int texH)
        {
            SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
            SDL_RenderClear(renderer);
            if (!texture) { return; }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
            float scale = std::min(static_cast<float>(windowW) / texW, static_cast<float>(windowH) / texH);
            float destW = texW * scale;
            float destH = texH * scale;
            SDL_FRect dest{ (windowW - destW) / 2.0f, (windowH - destH) / 2.0f, destW, destH };
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
        }

        // Placeholder for the eventual real 3D model render - a labeled
        // box showing the model index. See MenuNode.h.
        void DrawModelPlaceholder(SDL_Renderer* renderer, int modelIndex, int x, int y, int w, int h, int scale)
        {
            if (modelIndex < 0) { return; }

            SDL_SetRenderDrawColor(renderer, COLOR_GREEN_DIM.r, COLOR_GREEN_DIM.g, COLOR_GREEN_DIM.b, 255);
            SDL_FRect box{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h) };
            SDL_RenderRect(renderer, &box);

            std::string label = "[MODEL #" + std::to_string(modelIndex) + "]";
            int textX = x + (w - TextWidth(label, scale)) / 2;
            int textY = y + h / 2 - TextHeight(scale) / 2;
            DrawBitmapText(renderer, label, textX, textY, scale, COLOR_GREEN_DIM);
        }

        void DrawColumn(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int selectedIndex, bool isActiveColumn,
                         int x, int y, int rowHeight, int scale)
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                Color textColor = isSelected ? COLOR_GREEN : COLOR_GREEN_DIM;

                if (isSelected)
                {
                    SDL_SetRenderDrawColor(renderer, COLOR_HIGHLIGHT_BG.r, COLOR_HIGHLIGHT_BG.g, COLOR_HIGHLIGHT_BG.b, 255);
                    SDL_FRect bar{ static_cast<float>(x), static_cast<float>(rowY), 220.0f, static_cast<float>(rowHeight - 4) };
                    SDL_RenderFillRect(renderer, &bar);
                    if (isActiveColumn)
                    {
                        SDL_SetRenderDrawColor(renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255);
                        SDL_RenderRect(renderer, &bar);
                    }
                }

                DrawBitmapText(renderer, items[i].label, x + scale * 4, rowY + (rowHeight - TextHeight(scale)) / 2, scale, textColor);
            }
        }

        void DrawSlider(SDL_Renderer* renderer, const std::string& label, int value, int x, int y, int scale)
        {
            DrawBitmapText(renderer, label, x, y, scale, COLOR_GREEN);
            int barX = x + TextWidth(label, scale) + scale * 12;
            int cellSize = scale * 4;
            for (int i = 0; i < 10; ++i)
            {
                Color c = (i < value) ? COLOR_GREEN : COLOR_GREEN_DIM;
                SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                SDL_FRect cell{ static_cast<float>(barX + i * (cellSize + scale)), static_cast<float>(y), static_cast<float>(cellSize), static_cast<float>(cellSize) };
                SDL_RenderFillRect(renderer, &cell);
            }
        }

        // Applies the meaning of an Action leaf once "Toggled" - only
        // Render Quality and Language actually have somewhere to persist
        // to right now (RenderSettings/Language). Camera Sway/Difficulty
        // selections are visually confirmed (highlighted) but not backed
        // by a real setting yet - no gameplay system exists to apply them
        // to.
        void ApplyLeafAction(const std::string& parentLabel, const std::string& leafLabel, RenderSettings& renderSettings, Language& language)
        {
            if (parentLabel == "Render Quality")
            {
                renderSettings.Set(leafLabel == "Smoothed" ? RenderFidelity::Smoothed : RenderFidelity::Original);
            }
            else if (parentLabel == "Language")
            {
                if (leafLabel == "Français") { language = Language::French; }
                else if (leafLabel == "Italiano") { language = Language::Italian; }
                else if (leafLabel == "Español") { language = Language::Spanish; }
                else { language = Language::English; }
            }
        }
    }

    MenuResult MenuController::Run(
        const std::filesystem::path& cdDirectory,
        RenderSettings& renderSettings,
        Language& language)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, "" };
        }
        SDL_Renderer* renderer = app.Renderer();

        int mainBgW = 0, mainBgH = 0, optionsBgW = 0, optionsBgH = 0;
        SDL_Texture* mainBg = LoadBackgroundTexture(cdDirectory, renderer, 0, mainBgW, mainBgH);
        SDL_Texture* optionsBg = LoadBackgroundTexture(cdDirectory, renderer, 1, optionsBgW, optionsBgH);

        MenuNode root = BuildMainMenuTree();
        const MenuNode& optionsRoot = root.children[3]; // "Options"

        enum class Screen { MainMenu, Options, Credits };
        Screen screen = Screen::MainMenu;

        std::vector<int> mainPath = { 0 };
        std::vector<int> optionsPath = { 0 };

        MenuResult result;
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
                        if (screen == Screen::MainMenu) { MoveSelection(root, mainPath, -1); }
                        else if (screen == Screen::Options) { MoveSelection(optionsRoot, optionsPath, -1); }
                        break;
                    case SDLK_DOWN:
                        if (screen == Screen::MainMenu) { MoveSelection(root, mainPath, 1); }
                        else if (screen == Screen::Options) { MoveSelection(optionsRoot, optionsPath, 1); }
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (screen == Screen::MainMenu)
                        {
                            const MenuNode& chosen = WalkPath(root, mainPath);
                            if (chosen.label == "Options")
                            {
                                screen = Screen::Options;
                                optionsPath = { 0 };
                            }
                            else
                            {
                                result.action = chosen.label;
                                running = false;
                            }
                        }
                        else if (screen == Screen::Options)
                        {
                            std::vector<int> parentPath(optionsPath.begin(), optionsPath.end() - 1);
                            std::string parentLabel = parentPath.empty() ? optionsRoot.label : WalkPath(optionsRoot, parentPath).label;
                            std::string leafLabel = WalkPath(optionsRoot, optionsPath).label;

                            EnterResult r = Enter(optionsRoot, optionsPath);
                            if (r == EnterResult::EnteredCredits) { screen = Screen::Credits; }
                            else if (r == EnterResult::Toggled) { ApplyLeafAction(parentLabel, leafLabel, renderSettings, language); }
                        }
                        break;
                    case SDLK_ESCAPE:
                        if (screen == Screen::Credits) { screen = Screen::Options; }
                        else if (screen == Screen::Options)
                        {
                            if (!Back(optionsPath)) { screen = Screen::MainMenu; }
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            int scale = 3;
            int rowHeight = TextHeight(scale) + scale * 6;

            if (screen == Screen::MainMenu)
            {
                DrawBackground(renderer, mainBg, mainBgW, mainBgH);
                int windowW = 0, windowH = 0;
                SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
                DrawColumn(renderer, root.children, mainPath[0], true, windowW / 2 - 100, windowH * 2 / 3, rowHeight, scale);
            }
            else if (screen == Screen::Options)
            {
                DrawBackground(renderer, optionsBg, optionsBgW, optionsBgH);

                int columnX = scale * 8;
                const MenuNode* node = &optionsRoot;
                for (size_t depth = 0; depth <= optionsPath.size(); ++depth)
                {
                    if (node->children.empty()) { break; }

                    int selectedHere = (depth < optionsPath.size()) ? optionsPath[depth] : 0;
                    bool isActive = (depth == optionsPath.size() - 1);
                    bool isPreview = (depth == optionsPath.size());

                    const MenuNode& highlighted = node->children[static_cast<size_t>(std::clamp(
                        selectedHere, 0, static_cast<int>(node->children.size()) - 1))];

                    if (highlighted.kind == MenuNodeKind::Slider && isPreview)
                    {
                        DrawSlider(renderer, "Music", 8, columnX, scale * 8, scale);
                        DrawSlider(renderer, "SFX", 8, columnX, scale * 8 + rowHeight, scale);
                        break;
                    }

                    DrawColumn(renderer, node->children, selectedHere, isActive, columnX, scale * 8, rowHeight, scale);
                    columnX += 220 + scale * 4;

                    if (depth >= optionsPath.size()) { break; }
                    node = &node->children[static_cast<size_t>(optionsPath[depth])];
                    if (node->kind != MenuNodeKind::List) { break; } // leaf - nothing further to preview as a column
                }

                int modelIndex = EffectiveModelIndex(optionsRoot, optionsPath);
                int windowW = 0, windowH = 0;
                SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
                DrawModelPlaceholder(renderer, modelIndex, columnX, scale * 8, windowW - columnX - scale * 8, 300, scale);

                DrawBitmapText(renderer, "PRESS ESC TO GO BACK", scale * 8, windowH - rowHeight * 2, scale, COLOR_GREEN);
                DrawBitmapText(renderer, "PRESS ENTER TO SELECT", scale * 8, windowH - rowHeight, scale, COLOR_GREEN);
            }
            else // Credits
            {
                DrawBackground(renderer, optionsBg, optionsBgW, optionsBgH);
                // Placeholder - real credits scroll (parsing CD/GFX/CREDITS.TXT
                // and animating it) isn't implemented yet.
                DrawBitmapText(renderer, "CREDITS", scale * 8, scale * 8, scale, COLOR_GREEN);
                DrawBitmapText(renderer, "(scroll not yet implemented - see CD/GFX/CREDITS.TXT)",
                                scale * 8, scale * 8 + rowHeight, scale, COLOR_GREEN_DIM);
                DrawBitmapText(renderer, "PRESS ESC TO GO BACK", scale * 8, scale * 8 + rowHeight * 3, scale, COLOR_GREEN);
            }

            SDL_RenderPresent(renderer);
        }

        if (mainBg) { SDL_DestroyTexture(mainBg); }
        if (optionsBg) { SDL_DestroyTexture(optionsBg); }

        return result;
    }
}
