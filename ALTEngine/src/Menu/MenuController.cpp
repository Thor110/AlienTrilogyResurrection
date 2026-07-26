#include "MenuController.h"
#include "MenuTree.h"
#include "MenuNavigation.h"
#include "../Audio/MusicPlayer.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Bootstrap/ResolutionSettings.h"
#include "../Formats/SplashImageLoader.h"
#include "../Renderer/ModelRenderer.h"
#include "../Renderer/ModelPreview.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Menu
{
    using ALTEngine::Audio::MusicPlayer;
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::Language;
    using ALTEngine::Bootstrap::RenderFidelity;
    using ALTEngine::Bootstrap::RenderSettings;
    using ALTEngine::Bootstrap::ResolutionSettings;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;
    using ALTEngine::Formats::SplashImage;
    using ALTEngine::Formats::SplashImageLoader;
    using ALTEngine::Renderer::ModelRenderer;

    namespace
    {
        constexpr Color COLOR_BG{ 0, 0, 0, 255 };
        constexpr Color COLOR_GREEN{ 51, 255, 102, 255 };
        constexpr Color COLOR_GREEN_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_HIGHLIGHT_BG{ 0, 40, 15, 255 };
        constexpr Color COLOR_WHITE{ 255, 255, 255, 255 };

        // Queries the real available fullscreen display modes for the
        // window's current display, deduped by resolution (ignoring
        // refresh rate - the Resolution menu picks a size, not a
        // specific refresh rate) and sorted widest-first. Falls back to
        // an empty list (Resolution submenu just has no options) if
        // nothing could be queried, rather than failing the whole menu.
        std::vector<std::string> QueryResolutionLabels(SDL_Window* window)
        {
            std::vector<std::string> labels;
            if (!window) { return labels; }

            SDL_DisplayID display = SDL_GetDisplayForWindow(window);
            if (display == 0) { return labels; }

            int count = 0;
            SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
            if (!modes) { return labels; }

            std::vector<std::pair<int, int>> seen;
            for (int i = 0; i < count; ++i)
            {
                int w = modes[i]->w, h = modes[i]->h;
                if (std::find(seen.begin(), seen.end(), std::make_pair(w, h)) == seen.end())
                {
                    seen.push_back({ w, h });
                }
            }
            SDL_free(modes);

            std::sort(seen.begin(), seen.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            for (const auto& [w, h] : seen) { labels.push_back(std::to_string(w) + "x" + std::to_string(h)); }
            return labels;
        }

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
        // box showing the model index. See MenuNode.h. Used as a
        // fallback by DrawModel below if GPU rendering isn't available.
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

        // Renders the real spinning 3D model via the shared
        // DrawModelPreview helper (src/Renderer/ModelPreview.h) -
        // falls back to DrawModelPlaceholder if the GPU pipeline isn't
        // available or this specific model fails to load, so the menu
        // stays fully usable either way.
        void DrawModel(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int modelIndex,
                        int x, int y, int w, int h, int scale, float rotationAngle)
        {
            if (modelIndex < 0) { return; }

            ALTEngine::Renderer::ModelPreviewSource source = ALTEngine::Renderer::ModelPreviewSource::ForOptobj(cdDirectory, modelIndex);

            if (!ALTEngine::Renderer::DrawModelPreview(renderer, source, x, y, w, h, rotationAngle))
            {
                DrawModelPlaceholder(renderer, modelIndex, x, y, w, h, scale);
            }
        }

        // Returns the column's width (fitted to its widest label + padding)
        // so callers can pack the next column tightly against it, matching
        // the reference layout where each column's width follows its own
        // content (the Redefine column is visibly narrower than the
        // device list, which is narrower than the main Options list).
        int DrawColumn(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int selectedIndex,
                        int x, int y, int rowHeight, int scale)
        {
            int width = 0;
            for (const auto& item : items) { width = std::max(width, TextWidth(item.label, scale)); }
            width += scale * 8; // padding

            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                Color textColor = isSelected ? COLOR_GREEN : COLOR_GREEN_DIM;

                if (isSelected)
                {
                    // Filled bar only - no border. The original UI marks
                    // the highlighted item with a solid background and
                    // brighter text, nothing else.
                    SDL_SetRenderDrawColor(renderer, COLOR_HIGHLIGHT_BG.r, COLOR_HIGHLIGHT_BG.g, COLOR_HIGHLIGHT_BG.b, 255);
                    SDL_FRect bar{ static_cast<float>(x), static_cast<float>(rowY), static_cast<float>(width), static_cast<float>(rowHeight - 4) };
                    SDL_RenderFillRect(renderer, &bar);
                }

                DrawBitmapText(renderer, items[i].label, x + scale * 4, rowY + (rowHeight - TextHeight(scale)) / 2, scale, textColor);
            }

            return width;
        }

        // Main menu specifically: no highlight box at all - just text,
        // green by default, white when selected. Distinct from DrawColumn
        // (used for Options, which keeps its filled-bar highlight).
        void DrawMainMenuList(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int selectedIndex,
            int windowW, int y, int rowHeight, int scale)
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                Color textColor = isSelected ? COLOR_WHITE : COLOR_GREEN;

                int textW = TextWidth(items[i].label, scale);
                int textX = (windowW - textW) / 2;

                DrawBitmapText(renderer, items[i].label, textX, rowY + (rowHeight - TextHeight(scale)) / 2, scale, textColor);
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
        // Quality, Resolution, and Language actually have somewhere to
        // persist to right now. Camera Sway/Difficulty selections are
        // visually confirmed (highlighted) but not backed by a real
        // setting yet - no gameplay system exists to apply them to.
        void ApplyLeafAction(const std::string& parentLabel, const std::string& leafLabel,
                              RenderSettings& renderSettings, ResolutionSettings& resolutionSettings, Language& language)
        {
            if (parentLabel == "Quality")
            {
                renderSettings.Set(leafLabel == "Smoothed" ? RenderFidelity::Smoothed : RenderFidelity::Original);
            }
            else if (parentLabel == "Resolution")
            {
                int width = 0, height = 0;
                if (std::sscanf(leafLabel.c_str(), "%dx%d", &width, &height) == 2 && width > 0 && height > 0)
                {
                    if (AppWindow::Instance().ApplyFullscreenResolution(width, height))
                    {
                        resolutionSettings.Set(width, height);
                    }
                }
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
        ResolutionSettings& resolutionSettings,
        Language& language)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, "" };
        }
        SDL_Renderer* renderer = app.Renderer();

        // Apply a previously-saved resolution, if any - otherwise leave
        // the desktop's current mode alone.
        if (auto saved = resolutionSettings.Get())
        {
            app.ApplyFullscreenResolution(saved->first, saved->second);
        }

        std::vector<std::string> resolutionLabels = QueryResolutionLabels(app.Window());

        int mainBgW = 0, mainBgH = 0, optionsBgW = 0, optionsBgH = 0;
        SDL_Texture* mainBg = LoadBackgroundTexture(cdDirectory, renderer, 0, mainBgW, mainBgH);
        SDL_Texture* optionsBg = LoadBackgroundTexture(cdDirectory, renderer, 1, optionsBgW, optionsBgH);

        MenuNode root = BuildMainMenuTree(resolutionLabels);
        const MenuNode& optionsRoot = root.children[3]; // "Options"

        // OPTOBJ preload queue - built here, drained in a blocking
        // loading phase below (not incrementally inside the main menu
        // loop). Edward, 2026: "don't pop up the main menu screen
        // unless it is loaded, that way the lingering black screen from
        // skipping naturally covers it" - and separately, this closes a
        // real gap: incremental loading inside the main loop meant a
        // fast player could reach "Start Game" before OPTOBJ finished
        // loading, and since the model cache now persists across screen
        // transitions (see ModelRenderer::Shutdown's own doc comment -
        // no longer destroyed when leaving the menu), the ONLY place
        // OPTOBJ is guaranteed to finish loading is here, before the
        // menu becomes interactive at all.
        //
        // Initialize() MUST be called before any of this - without it,
        // device is still null, so every LoadModel/RenderToRgba call
        // below silently returns false/empty immediately (see their own
        // "if (!device) return" early-outs). This was a real bug: the
        // whole blocking phase "completed" in a single frame having
        // preloaded nothing at all, which is exactly why it looked like
        // it "changed nothing" - Initialize() was still only ever
        // getting called for the first time inside DrawModelPreview,
        // i.e. on the user's actual first visit to Options, which is
        // where the full cold-start cost was still landing.
        ALTEngine::Renderer::ModelRenderer::Initialize();

        std::vector<ALTEngine::Renderer::PreloadRequest> modelPreloadQueue;
        for (int i = 0; i < 14; i++)
        {
            auto source = ALTEngine::Renderer::ModelPreviewSource::ForOptobj(cdDirectory, i);
            modelPreloadQueue.push_back({ source.CacheKey(), i, source.objBndPath, source.gfxBndPath,
                                           source.transparentRgb, source.baseRotationRadians });
        }
        size_t modelPreloadNext = 0;
        bool modelPreloadWarmedUp = false;

        // Blocking loading phase - same event-pump-and-present-every-
        // iteration approach already proven not to trigger Windows'
        // "not responding" cursor (see main.cpp's boot sequence), just
        // applied here instead of at boot. Presents a plain black frame
        // each iteration - deliberately no loading text or indicator,
        // matching "the lingering black screen from skipping naturally
        // covers it" (Edward, 2026).
        {
            bool loadingWindowClosed = false;
            while (!modelPreloadWarmedUp)
            {
                SDL_Event loadEvent;
                while (SDL_PollEvent(&loadEvent))
                {
                    if (loadEvent.type == SDL_EVENT_QUIT) { loadingWindowClosed = true; }
                }
                if (loadingWindowClosed) { break; }

                // Time-budgeted rather than a fixed count per frame -
                // self-adjusts to whatever the actual per-model cost is
                // on the machine it's running on, rather than a guessed
                // fixed number, while still never blocking longer than
                // the budget at a stretch.
                constexpr Uint64 PRELOAD_FRAME_BUDGET_MS = 6;
                Uint64 preloadFrameStart = SDL_GetTicks();
                while (modelPreloadNext < modelPreloadQueue.size() &&
                       (SDL_GetTicks() - preloadFrameStart) < PRELOAD_FRAME_BUDGET_MS)
                {
                    const auto& req = modelPreloadQueue[modelPreloadNext++];
                    ALTEngine::Renderer::ModelRenderer::LoadModel(req.cacheKey, req.meshNumber, req.objBndPath, req.gfxBndPath,
                                                                   req.transparentRgb, req.baseRotationRadians);
                }

                // One-time GPU warm-up, right after the model-data
                // preload queue finishes. Edward, 2026: model data being
                // preloaded (vertex/index/texture buffers uploaded)
                // turned out not to be the whole story - DrawModel only
                // ever gets called from the Options branch, never the
                // main menu, so the render target (EnsureRenderTarget)
                // and the normal render pipeline had never actually
                // been exercised by a real draw call before the user's
                // first visit to Options - and Multitap/SpeakerMusic/
                // SpeakerSfx use a SEPARATE doubleSidedPipeline object
                // that's equally never been used in a real draw before
                // the user first switches to one of those specific
                // models (e.g. Volume). GPU drivers often lazily compile
                // a pipeline's actual GPU-side shader code on its first
                // real draw call, not at creation time - confirmed ~80x
                // slower on the very first RenderToRgba call vs the
                // second, even on software rendering. Two throwaway
                // calls here (one per pipeline, discarding the result)
                // forces that cost to happen now, before the menu is
                // interactive, rather than on the user's first encounter
                // with either path.
                if (modelPreloadNext >= modelPreloadQueue.size() && !modelPreloadWarmedUp)
                {
                    int warmupW = 0, warmupH = 0;
                    SDL_GetRenderOutputSize(renderer, &warmupW, &warmupH);
                    int warmupSize = std::min(warmupW, warmupH);
                    if (warmupSize < 64) { warmupSize = 64; }
                    ALTEngine::Renderer::ModelRenderer::RenderToRgba({ ALTEngine::Renderer::ModelCatalog::Optobj, 0 }, 0.0f, warmupSize, warmupSize);
                    ALTEngine::Renderer::ModelRenderer::RenderToRgba({ ALTEngine::Renderer::ModelCatalog::Optobj, ModelIndex::Multitap }, 0.0f, warmupSize, warmupSize);
                    modelPreloadWarmedUp = true;
                }

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderPresent(renderer);
            }

            if (loadingWindowClosed)
            {
                if (mainBg) { SDL_DestroyTexture(mainBg); }
                if (optionsBg) { SDL_DestroyTexture(optionsBg); }
                return { true, "" };
            }
        }

        // track02.wav is confirmed as the main menu music (Edward).
        // Plays continuously across the whole menu (Main Menu, Options,
        // Credits) rather than stopping the moment you leave the Main
        // Menu screen specifically - matches how menu music behaves in
        // basically every game with a menu. Started here, after the
        // loading phase above, rather than before it - Edward, 2026:
        // "the music starts playing before the main menu appears" was
        // jarring, hearing menu music over a black loading screen for
        // several seconds before anything shows.
        MusicPlayer::PlayLooped(cdDirectory / "MUSIC" / "track02.wav");

        enum class Screen { MainMenu, Options, Credits };
        Screen screen = Screen::MainMenu;

        std::vector<int> mainPath = { 0 };
        std::vector<int> optionsPath = { 0 };

        MenuResult result;
        bool running = true;

        while (running)
        {
            MusicPlayer::Update();

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
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                        break;
                    case SDLK_DOWN:
                        if (screen == Screen::MainMenu) { MoveSelection(root, mainPath, 1); }
                        else if (screen == Screen::Options) { MoveSelection(optionsRoot, optionsPath, 1); }
                        SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
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
                            SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        }
                        else if (screen == Screen::Options)
                        {
                            std::vector<int> parentPath(optionsPath.begin(), optionsPath.end() - 1);
                            std::string parentLabel = parentPath.empty() ? optionsRoot.label : WalkPath(optionsRoot, parentPath).label;
                            std::string leafLabel = WalkPath(optionsRoot, optionsPath).label;

                            EnterResult r = Enter(optionsRoot, optionsPath);
                            if (r == EnterResult::EnteredCredits) { screen = Screen::Credits; }
                            else if (r == EnterResult::Toggled) { ApplyLeafAction(parentLabel, leafLabel, renderSettings, resolutionSettings, language); }
                            if (r != EnterResult::NoOp) { SfxPlayer::Play(SfxId::MenuSelect, cdDirectory); }
                        }
                        break;
                    case SDLK_ESCAPE:
                        if (screen == Screen::Credits) { screen = Screen::Options; }
                        else if (screen == Screen::Options)
                        {
                            if (!Back(optionsPath)) { screen = Screen::MainMenu; }
                        }
                        else if (screen == Screen::MainMenu)
                        {
                            result.action = "Exit";
                            running = false;
                        }
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
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
                DrawMainMenuList(renderer, root.children, mainPath[0], windowW, windowH * 2 / 3, rowHeight, scale);
            }
            else if (screen == Screen::Options)
            {
                DrawBackground(renderer, optionsBg, optionsBgW, optionsBgH);

                int windowW = 0, windowH = 0;
                SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

                // Model viewport fills the screen (roughly) and is drawn
                // BEFORE the text columns, as a background layer - fixed
                // position/size regardless of navigation depth. Previously
                // this was a small box anchored to columnX (which grows
                // as you navigate into nested menus), so the model itself
                // visibly jumped around and resized every time the column
                // count changed - Edward, 2026.
                int modelIndex = EffectiveModelIndex(optionsRoot, optionsPath);
                float rotationAngle = static_cast<float>(SDL_GetTicks()) / 1000.0f; // 1 radian/sec - a slow, steady spin
                DrawModel(renderer, cdDirectory, modelIndex, 0, 0, windowW, windowH, scale, rotationAngle);

                std::string title = "OPTIONS";
                DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, scale * 6, scale, COLOR_GREEN);

                int columnTop = scale * 20;
                int columnX = scale * 6;
                const MenuNode* node = &optionsRoot;
                for (size_t depth = 0; depth <= optionsPath.size(); ++depth)
                {
                    if (node->children.empty()) { break; }

                    int selectedHere = (depth < optionsPath.size()) ? optionsPath[depth] : 0;
                    bool isPreview = (depth == optionsPath.size());

                    const MenuNode& highlighted = node->children[static_cast<size_t>(std::clamp(
                        selectedHere, 0, static_cast<int>(node->children.size()) - 1))];

                    if (highlighted.kind == MenuNodeKind::Slider && isPreview)
                    {
                        DrawSlider(renderer, "Music", 8, columnX, columnTop, scale);
                        DrawSlider(renderer, "SFX", 8, columnX, columnTop + rowHeight, scale);
                        break;
                    }

                    int columnWidth = DrawColumn(renderer, node->children, selectedHere, columnX, columnTop, rowHeight, scale);
                    columnX += columnWidth; // packed tight against the next column, matching the reference

                    if (depth >= optionsPath.size()) { break; }
                    node = &node->children[static_cast<size_t>(optionsPath[depth])];
                    if (node->kind != MenuNodeKind::List) { break; } // leaf - nothing further to preview as a column
                }

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

        MusicPlayer::Stop();

        if (mainBg) { SDL_DestroyTexture(mainBg); }
        if (optionsBg) { SDL_DestroyTexture(optionsBg); }

        return result;
    }
}
