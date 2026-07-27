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
#include "../Screens/MenuBackground.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
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
    using ALTEngine::Bootstrap::AudioSettings;
    using ALTEngine::Bootstrap::CameraSwaySettings;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::Difficulty;
    using ALTEngine::Bootstrap::DifficultySettings;
    using ALTEngine::Bootstrap::DisplayMode;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::DrawRoundedRect;
    using ALTEngine::Bootstrap::Language;
    using ALTEngine::Bootstrap::LanguageSettings;
    using ALTEngine::Bootstrap::KeyBindings;
    using ALTEngine::Bootstrap::LerpColor;
    using ALTEngine::Bootstrap::PulsePhase;
    using ALTEngine::Bootstrap::RenderFidelity;
    using ALTEngine::Bootstrap::RenderSettings;
    using ALTEngine::Bootstrap::ResolutionSettings;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;
    using ALTEngine::Formats::SplashImage;
    using ALTEngine::Formats::SplashImageLoader;
    using ALTEngine::Renderer::ModelRenderer;
    using ALTEngine::Screens::DrawMenuBackground;

    namespace
    {
        constexpr Color COLOR_GREEN{ 51, 255, 102, 255 };
        constexpr Color COLOR_GREEN_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_HIGHLIGHT_BG{ 0, 40, 15, 255 };        // very dark green - every row's default box now (Edward, 2026), not just the selected one
        constexpr Color COLOR_HIGHLIGHT_BG_LIGHT{ 20, 130, 60, 255 }; // light green - pulse target for the current cursor row
        constexpr Color COLOR_DISABLED_TEXT{ 12, 65, 26, 255 };       // dark green text for disabled items (Controls hardware not yet tested) - darker than COLOR_GREEN_DIM, stays this dark even when the cursor is on it
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
        //
        // selectedIndex = -1 means no highlight at all - used for the
        // one-ahead preview column on pure navigation lists (Volume,
        // Controls), where no child represents a real "current value"
        // to indicate (Edward, 2026: "Volume's list shouldn't have a
        // highlight until entered as neither Music or SFX are 'active'
        // or selected"). pulseSelected controls whether the selected
        // row's box animates between dark/light green (true - the real
        // cursor column) or stays a static light green (false - a
        // settings-list preview column showing its current value one
        // column ahead of actually entering it).
        int DrawColumn(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int selectedIndex, bool pulseSelected,
                        int x, int y, int rowHeight, int scale, Language language)
        {
            // Music/SFX volume and Mouse Sensitivity (Edward, 2026:
            // "keep the design of the slider which now only resides in
            // the pause menu... revert to that so that we can match the
            // original aesthetic") - same inputActionIndex sentinel
            // range AdjustNumericSettingIfOnEntry already uses.
            auto isSliderEntry = [](const MenuNode& node) { return node.inputActionIndex >= -5 && node.inputActionIndex <= -3; };

            // Box width is label text only - the slider sits outside/to
            // the right of the box, not stretching it, matching the
            // pause menu exactly (Edward, 2026: "The slider should not
            // be within the button, it should be to the right of the
            // button like it was before and still is in the pause
            // menu").
            int width = 0;
            for (const auto& item : items) { width = std::max(width, TextWidth(DisplayLabel(item, language), scale)); }
            width += scale * 8; // padding

            // Fixed barX (not relative to each label's own width) so
            // Music/SFX/Mouse Sensitivity's bars all align in a column,
            // same as the pause menu's own "fixed X so SFX/Music's bars
            // align regardless of their different label widths".
            int barX = x + width + scale * 8;

            float pulse = pulseSelected ? PulsePhase() : 1.0f;

            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                bool enabled = items[i].enabled;

                // Every row gets a box now (Edward, 2026: "a disabled
                // version of the current highlight around all options
                // and pause menu items") - very dark green by default,
                // brighter and pulsing for the selected row regardless
                // of whether that item is enabled (Edward, 2026: "make
                // it so that disabled menu items in the controls
                // section still have a highlight to indicate the
                // currently selected item... a matching pulsing
                // highlight, just leaving the darker text in place") -
                // only the text brightens to full green, and only when
                // the item is actually enabled.
                Color boxColor = COLOR_HIGHLIGHT_BG;
                Color textColor = enabled ? COLOR_GREEN_DIM : COLOR_DISABLED_TEXT;

                if (isSelected)
                {
                    boxColor = LerpColor(COLOR_HIGHLIGHT_BG, COLOR_HIGHLIGHT_BG_LIGHT, pulse);
                    if (enabled) { textColor = COLOR_GREEN; }
                }

                int boxHeight = rowHeight - scale * 2;
                SDL_FRect bar{ static_cast<float>(x), static_cast<float>(rowY), static_cast<float>(width), static_cast<float>(boxHeight) };
                DrawRoundedRect(renderer, bar, static_cast<float>(scale * 2), boxColor);

                std::string label = DisplayLabel(items[i], language);
                if (isSliderEntry(items[i]))
                {
                    // rowY/boxHeight passed directly (not a pre-computed
                    // text Y) so the label and the slider cells - which
                    // have different heights - each get centered
                    // correctly within the same box (Edward, 2026: "the
                    // squares of the sliders are not aligned with the
                    // buttons themselves").
                    DrawSlider(renderer, label, items[i].sliderValue, x + scale * 4, barX, rowY, boxHeight, scale, textColor, COLOR_GREEN, COLOR_HIGHLIGHT_BG);
                }
                else
                {
                    int textY = rowY + (rowHeight - TextHeight(scale)) / 2;
                    DrawBitmapText(renderer, label, x + scale * 4, textY, scale, textColor);
                }
            }

            return width;
        }

        // Main menu specifically: no highlight box at all - just text,
        // green by default, white when selected. Distinct from DrawColumn
        // (used for Options, which keeps its filled-bar highlight).
        void DrawMainMenuList(SDL_Renderer* renderer, const std::vector<MenuNode>& items, int selectedIndex,
            int windowW, int y, int rowHeight, int scale, Language language)
        {
            float pulse = PulsePhase();
            for (size_t i = 0; i < items.size(); ++i)
            {
                int rowY = y + static_cast<int>(i) * rowHeight;
                bool isSelected = (static_cast<int>(i) == selectedIndex);
                Color textColor = isSelected ? LerpColor(COLOR_GREEN, COLOR_WHITE, pulse) : COLOR_GREEN;

                std::string text = DisplayLabel(items[i], language);
                int textW = TextWidth(text, scale);
                int textX = (windowW - textW) / 2;

                DrawBitmapText(renderer, text, textX, rowY + (rowHeight - TextHeight(scale)) / 2, scale, textColor);
            }
        }

        // Applies the meaning of an Action leaf once "Toggled" - only
        // Quality, Resolution, and Language actually have somewhere to
        // persist to right now. Camera Sway/Difficulty selections are
        // visually confirmed (highlighted) but not backed by a real
        // setting yet - no gameplay system exists to apply them to.
        void ApplyLeafAction(const std::string& parentLabel, const std::string& leafLabel,
                              RenderSettings& renderSettings, ResolutionSettings& resolutionSettings,
                              DifficultySettings& difficultySettings, CameraSwaySettings& cameraSwaySettings,
                              LanguageSettings& languageSettings, Language& language)
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
                    // Always set the exclusive-fullscreen preference
                    // (harmless if not currently in that mode - it just
                    // takes effect whenever Fullscreen is next entered).
                    bool ok = AppWindow::Instance().ApplyFullscreenResolution(width, height);

                    // But that call alone does nothing visible while in
                    // Windowed mode (Edward, 2026: "resolutions don't
                    // apply when in windowed mode, at all") - resize the
                    // actual window directly for that case.
                    if (renderSettings.GetDisplayMode() == DisplayMode::Windowed)
                    {
                        AppWindow::Instance().SetWindowedSize(width, height);
                        ok = true;
                    }

                    if (ok) { resolutionSettings.Set(width, height); }
                }
            }
            else if (parentLabel == "VSync")
            {
                bool enabled = (leafLabel == "On");
                renderSettings.SetVSync(enabled);
                AppWindow::Instance().SetVSync(enabled);
            }
            else if (parentLabel == "Display Mode")
            {
                DisplayMode mode = leafLabel == "Windowed" ? DisplayMode::Windowed
                                  : leafLabel == "Borderless" ? DisplayMode::Borderless
                                  : DisplayMode::Fullscreen;
                renderSettings.SetDisplayMode(mode);

                // Windowed needs an actual size - use whatever
                // Resolution is currently set to, rather than a
                // hardcoded default that would silently ignore it
                // (Edward, 2026).
                auto savedResolution = resolutionSettings.Get();
                if (savedResolution.has_value())
                {
                    AppWindow::Instance().SetDisplayMode(mode, savedResolution->first, savedResolution->second);
                }
                else
                {
                    AppWindow::Instance().SetDisplayMode(mode);
                }
            }
            else if (parentLabel == "Language")
            {
                if (leafLabel == "Français") { language = Language::French; }
                else if (leafLabel == "Italiano") { language = Language::Italian; }
                else if (leafLabel == "Español") { language = Language::Spanish; }
                else { language = Language::English; }
                languageSettings.Set(language);
            }
            // Difficulty and Camera Sway - Edward, 2026: previously
            // completely unwired, selecting either did nothing at all.
            else if (parentLabel == "Difficulty")
            {
                Difficulty difficulty = leafLabel == "Raging Terror" ? Difficulty::RagingTerror
                                       : leafLabel == "Xenomania" ? Difficulty::Xenomania
                                       : Difficulty::AcidReign;
                difficultySettings.Set(difficulty);
            }
            else if (parentLabel == "Camera Sway")
            {
                cameraSwaySettings.Set(leafLabel == "On");
            }
        }
    }

    MenuResult MenuController::Run(
        const std::filesystem::path& cdDirectory,
        RenderSettings& renderSettings,
        ResolutionSettings& resolutionSettings,
        DifficultySettings& difficultySettings,
        CameraSwaySettings& cameraSwaySettings,
        LanguageSettings& languageSettings,
        KeyBindings& keyBindings,
        AudioSettings& audioSettings,
        Language& language,
        std::vector<int>& mainPath,
        bool startInOptionsOnly)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true, "" };
        }
        SDL_Renderer* renderer = app.Renderer();

        // Apply a previously-saved resolution, if any - otherwise leave
        // the desktop's current mode alone.
        std::string currentResolutionLabel;
        if (auto saved = resolutionSettings.Get())
        {
            app.ApplyFullscreenResolution(saved->first, saved->second);
            currentResolutionLabel = std::to_string(saved->first) + "x" + std::to_string(saved->second);
        }

        std::vector<std::string> resolutionLabels = QueryResolutionLabels(app.Window());

        int mainBgW = 0, mainBgH = 0, optionsBgW = 0, optionsBgH = 0;
        SDL_Texture* mainBg = startInOptionsOnly ? nullptr : LoadBackgroundTexture(cdDirectory, renderer, 0, mainBgW, mainBgH);
        // nullptr when opened from the pause menu (Edward, 2026: "the
        // background menu is still showing when accessing the options
        // menu from the pause menu") - DrawMenuBackground is nullptr-
        // safe (clears to black), same pattern SaveSlotScreen's own
        // showBackground=false already uses.
        SDL_Texture* optionsBg = startInOptionsOnly ? nullptr : LoadBackgroundTexture(cdDirectory, renderer, 1, optionsBgW, optionsBgH);

        // Current values of every persisted setting the Options tree
        // needs, so each settings list starts on whichever child
        // actually matches what's saved, rather than always resetting
        // to the first option (Edward, 2026: "so that the correct entry
        // is selected when you enter each list").
        MenuSettingsSnapshot settingsSnapshot;
        settingsSnapshot.quality = renderSettings.Get();
        settingsSnapshot.resolutionLabel = currentResolutionLabel;
        settingsSnapshot.vsync = renderSettings.VSync();
        settingsSnapshot.displayMode = renderSettings.GetDisplayMode();
        settingsSnapshot.difficulty = difficultySettings.Get();
        settingsSnapshot.cameraSwayOn = cameraSwaySettings.Get();
        settingsSnapshot.language = language;

        MenuNode root = BuildMainMenuTree(resolutionLabels, settingsSnapshot, keyBindings, audioSettings, /*includeCredits=*/!startInOptionsOnly);
        MenuNode& optionsRoot = root.children[3]; // "Options"

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
        // Skipped when opened from the pause menu - starting the boot
        // menu's own music mid-gameplay would be a jarring, unwanted
        // side effect (Edward, 2026: "so that it can render without a
        // menu").
        if (!startInOptionsOnly)
        {
            MusicPlayer::PlayLooped(cdDirectory / "MUSIC" / "track02.wav");
            MusicPlayer::SetVolume(audioSettings.MusicVolume());
        }

        enum class Screen { MainMenu, Options, Credits };
        Screen screen = startInOptionsOnly ? Screen::Options : Screen::MainMenu;

        std::vector<int> optionsPath = { 0 };

        // Redefine controls (Edward, 2026) - set while waiting for the
        // next key/mouse-button press to capture as a new binding.
        // Cleared by the capture itself, or by Escape (cancels without
        // changing anything - standard "press a key..." convention).
        bool awaitingRebind = false;
        int rebindActionIndex = -1;
        ALTEngine::Bootstrap::DeviceKind rebindDevice = ALTEngine::Bootstrap::DeviceKind::Keyboard;
        std::string rebindActionLabel;

        // Music/SFX volume and Mouse Sensitivity (Edward, 2026:
        // "Separate the buttons from the sliders, so that you have to
        // press the button to access the slider. That way you can still
        // back out using left, only requiring escape if you have
        // selected the music or sfx slider button.") - false while just
        // navigating (left/right behave like every other entry: back/
        // enter), true only once the button's been pressed, at which
        // point left/right adjust the value and only Escape exits back
        // to normal navigation.
        bool adjustingSlider = false;

        MenuResult result;
        bool running = true;

        while (running)
        {
            MusicPlayer::Update();

            // Shared Enter/Escape logic, callable from both their own
            // dedicated keys and from left/right (Edward, 2026: "right
            // on the keyboard to enter a menu as well as enter and left
            // ... to escape a menu as well as escape" - matching the
            // original game, which used the arrow keys this way too).
            auto doEnter = [&]() {
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
                    MenuNode& parent = parentPath.empty() ? optionsRoot : WalkPath(optionsRoot, parentPath);
                    std::string parentLabel = parent.label;
                    MenuNode& leaf = WalkPath(optionsRoot, optionsPath);

                    // Redefine controls (Edward, 2026) - a leaf with a
                    // real inputActionIndex means "capture the next key/
                    // mouse-button press as this action's new binding",
                    // not the normal Enter/Toggled flow.
                    if (leaf.inputActionIndex >= 0)
                    {
                        awaitingRebind = true;
                        rebindActionIndex = leaf.inputActionIndex;
                        rebindDevice = static_cast<ALTEngine::Bootstrap::DeviceKind>(leaf.deviceIndex);
                        rebindActionLabel = ALTEngine::Bootstrap::ActionLabel(static_cast<ALTEngine::Bootstrap::InputAction>(leaf.inputActionIndex), language);
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        return;
                    }

                    // "Restore Defaults" declined (Edward, 2026) - "No"
                    // just backs out, matching Escape, rather than
                    // falling through to the normal Enter/Toggled flow
                    // (which would do nothing at all, since there's no
                    // ApplyLeafAction case for "No").
                    if (parentLabel == "Restore Defaults" && leaf.label == "No")
                    {
                        Back(optionsPath);
                        SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        return;
                    }

                    // "Restore Defaults" confirmed (Edward, 2026) - Yes
                    // is tagged inputActionIndex=-2. optionsPath here is
                    // [..., device, "Restore Defaults", "Yes"] - the
                    // device's own node is two levels up; its first
                    // child is always the Redefine list built alongside
                    // it (see Controls() in MenuTree.cpp).
                    if (leaf.inputActionIndex == -2)
                    {
                        auto device = static_cast<ALTEngine::Bootstrap::DeviceKind>(leaf.deviceIndex);
                        keyBindings.ResetToDefaults(device);

                        std::vector<int> devicePath(optionsPath.begin(), optionsPath.end() - 2);
                        MenuNode& redefineList = WalkPath(optionsRoot, devicePath).children[0];
                        for (auto& child : redefineList.children)
                        {
                            auto action = static_cast<ALTEngine::Bootstrap::InputAction>(child.inputActionIndex);
                            child.label = keyBindings.FormatBinding(device, action, language);
                        }
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        return;
                    }

                    std::string leafLabel = leaf.label;

                    EnterResult r = Enter(optionsRoot, optionsPath);
                    if (r == EnterResult::EnteredCredits) { screen = Screen::Credits; }
                    else if (r == EnterResult::Toggled)
                    {
                        ApplyLeafAction(parentLabel, leafLabel, renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings, language);

                        // Edward, 2026: "when I press escape it reverts
                        // to selecting the resolution in the config file
                        // rather than the newly selected option" - same
                        // for Camera Sway and Difficulty. initialSelectedChild
                        // was only ever computed once, at boot, from
                        // whatever was in Config then - it never updated
                        // when a setting changed live during the
                        // session, so the one-ahead preview column (and
                        // a later re-entry via Enter()) fell back to the
                        // stale value. optionsPath.back() is exactly the
                        // index just picked, since Enter() doesn't touch
                        // path for a leaf Action.
                        if (parent.isSettingsList) { parent.initialSelectedChild = optionsPath.back(); }
                    }
                    if (r != EnterResult::NoOp) { SfxPlayer::Play(SfxId::MenuSelect, cdDirectory); }
                }
            };

            auto doEscape = [&]() {
                if (screen == Screen::Credits) { screen = Screen::Options; }
                else if (screen == Screen::Options)
                {
                    if (!Back(optionsPath))
                    {
                        // Started directly in Options from the pause
                        // menu (Edward, 2026) - escaping out of it
                        // entirely means returning control to the
                        // caller, not falling back to a main menu that
                        // was never shown in the first place.
                        if (startInOptionsOnly) { running = false; }
                        else { screen = Screen::MainMenu; }
                    }
                }
                else if (screen == Screen::MainMenu)
                {
                    result.action = "Exit";
                    running = false;
                }
                SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
            };

            // Music/SFX volume and Mouse Sensitivity (Edward, 2026:
            // "Functional volume sliders effecting the music which is
            // all we have to test against currently", "Mouse
            // sensitivity slider in the Controls / Mouse list") -
            // identified by the same inputActionIndex sentinel
            // mechanism Redefine/Restore Defaults already use. Returns
            // false (no-op) if the cursor isn't on one of these three
            // specific entries, so the caller falls back to the normal
            // enter/escape behaviour.
            auto AdjustNumericSettingIfOnEntry = [&](int delta) {
                if (screen != Screen::Options || optionsPath.empty()) { return false; }
                MenuNode& leaf = WalkPath(optionsRoot, optionsPath);
                if (leaf.inputActionIndex < -5 || leaf.inputActionIndex > -3) { return false; }

                if (leaf.inputActionIndex == -5) // Mouse Sensitivity
                {
                    int updated = std::clamp(keyBindings.MouseSensitivity() + delta, 1, 10);
                    keyBindings.SetMouseSensitivity(updated);
                    // No immediate "apply" call needed - GameplayScreen
                    // reads this live from keyBindings every frame,
                    // unlike music volume which needs pushing into
                    // MusicPlayer's own separate, already-playing stream.
                    leaf.sliderValue = updated;
                    SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                    return true;
                }

                bool isMusic = (leaf.inputActionIndex == -3);
                int current = isMusic ? audioSettings.MusicVolume() : audioSettings.SfxVolume();
                int updated = std::clamp(current + delta, 0, 10);
                if (isMusic)
                {
                    audioSettings.SetMusicVolume(updated);
                    MusicPlayer::SetVolume(updated); // applied immediately - this is the one actually playing right now
                }
                else
                {
                    audioSettings.SetSfxVolume(updated);
                }

                leaf.sliderValue = updated;
                SfxPlayer::Play(SfxId::MenuMove, cdDirectory);
                return true;
            };

            // The "press the button" half of the same feature (Edward,
            // 2026) - checked before the normal doEnter() flow on Enter/
            // Right. Same range check as AdjustNumericSettingIfOnEntry
            // above; returns false if the cursor isn't on one of the
            // three slider entries, so the caller falls back to normal
            // enter behaviour.
            auto TryEnterSliderIfOnEntry = [&]() {
                if (screen != Screen::Options || optionsPath.empty()) { return false; }
                MenuNode& leaf = WalkPath(optionsRoot, optionsPath);
                if (leaf.inputActionIndex < -5 || leaf.inputActionIndex > -3) { return false; }
                adjustingSlider = true;
                SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                return true;
            };

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; continue; }

                if (awaitingRebind)
                {
                    using ALTEngine::Bootstrap::DeviceKind;
                    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                    {
                        awaitingRebind = false; // cancel, no change - standard "press a key..." convention
                    }
                    else if (rebindDevice == DeviceKind::Keyboard && event.type == SDL_EVENT_KEY_DOWN)
                    {
                        auto action = static_cast<ALTEngine::Bootstrap::InputAction>(rebindActionIndex);
                        keyBindings.SetKey(action, event.key.scancode);
                        WalkPath(optionsRoot, optionsPath).label = keyBindings.FormatBinding(rebindDevice, action, language);
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        awaitingRebind = false;
                    }
                    else if (rebindDevice == DeviceKind::Mouse && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    {
                        auto action = static_cast<ALTEngine::Bootstrap::InputAction>(rebindActionIndex);
                        keyBindings.SetMouseButton(action, event.button.button);
                        WalkPath(optionsRoot, optionsPath).label = keyBindings.FormatBinding(rebindDevice, action, language);
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        awaitingRebind = false;
                    }
                    else if (rebindDevice == DeviceKind::Mouse && event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.y != 0)
                    {
                        auto action = static_cast<ALTEngine::Bootstrap::InputAction>(rebindActionIndex);
                        keyBindings.SetMouseWheel(action, event.wheel.y > 0); // y>0 = scrolled away from the user (up), per SDL3's own docs
                        WalkPath(optionsRoot, optionsPath).label = keyBindings.FormatBinding(rebindDevice, action, language);
                        SfxPlayer::Play(SfxId::MenuSelect, cdDirectory);
                        awaitingRebind = false;
                    }
                    continue; // no other input processed while awaiting a rebind
                }

                // Music/SFX volume and Mouse Sensitivity, once entered
                // (Edward, 2026) - only left/right (adjust) and Escape
                // (exit back to normal navigation, cursor still on the
                // button) are processed; up/down/enter are ignored while
                // adjusting, same as the rebind-capture block above.
                if (adjustingSlider)
                {
                    if (event.type == SDL_EVENT_KEY_DOWN)
                    {
                        if (event.key.key == SDLK_ESCAPE)
                        {
                            adjustingSlider = false;
                            SfxPlayer::Play(SfxId::MenuBack, cdDirectory);
                        }
                        else if (event.key.key == SDLK_RIGHT) { AdjustNumericSettingIfOnEntry(1); }
                        else if (event.key.key == SDLK_LEFT) { AdjustNumericSettingIfOnEntry(-1); }
                    }
                    continue;
                }

                if (event.type == SDL_EVENT_KEY_DOWN)
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
                        if (!TryEnterSliderIfOnEntry()) { doEnter(); }
                        break;
                    case SDLK_ESCAPE:
                        doEscape();
                        break;
                    // Edward, 2026: left/right moving the cursor was only
                    // ever meant for the pause menu's Exit Game Yes/No
                    // (matching the original), not any settings list
                    // here - Difficulty/Camera Sway/Language/Quality/
                    // Resolution all just use enter/escape like every
                    // other list. The Main Menu itself doesn't respond
                    // to left/right at all - only Enter and Escape.
                    //
                    // Music/SFX volume and Mouse Sensitivity (Edward,
                    // 2026: "Separate the buttons from the sliders, so
                    // that you have to press the button to access the
                    // slider... you can still back out using left, only
                    // requiring escape if you have selected the music or
                    // sfx slider button") - Right on the button enters
                    // slider-adjust mode (handled above via
                    // TryEnterSliderIfOnEntry); once entered, left/right
                    // adjust and only Escape exits (handled by the
                    // adjustingSlider block earlier in this loop). Until
                    // then, left/right behave like every other entry.
                    case SDLK_RIGHT:
                        if (!TryEnterSliderIfOnEntry()) { if (screen != Screen::MainMenu) { doEnter(); } }
                        break;
                    case SDLK_LEFT:
                        if (screen != Screen::MainMenu) { doEscape(); }
                        break;
                    default:
                        break;
                    }
                }
            }
            if (!running) { break; }

            int scale = ComputeMenuScale(renderer);
            int rowHeight = TextHeight(scale) + scale * 6;

            if (screen == Screen::MainMenu)
            {
                DrawMenuBackground(renderer, mainBg, mainBgW, mainBgH);
                int windowW = 0, windowH = 0;
                SDL_GetRenderOutputSize(renderer, &windowW, &windowH);
                DrawMainMenuList(renderer, root.children, mainPath[0], windowW, windowH * 2 / 3, rowHeight, scale, language);
            }
            else if (screen == Screen::Options)
            {
                DrawMenuBackground(renderer, optionsBg, optionsBgW, optionsBgH);

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

                std::string title = ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::OptionsTitle, language);
                DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, scale * 6, scale, COLOR_GREEN);

                int columnTop = scale * 20;
                int columnX = scale * 6;
                const MenuNode* node = &optionsRoot;
                for (size_t depth = 0; depth <= optionsPath.size(); ++depth)
                {
                    if (node->children.empty()) { break; }

                    bool isPreview = (depth == optionsPath.size());
                    int selectedHere;
                    bool pulseHere;
                    if (!isPreview)
                    {
                        selectedHere = optionsPath[depth];
                        pulseHere = true; // this is where the cursor actually is
                    }
                    else
                    {
                        // One-ahead preview column, before the user has
                        // actually pressed Enter - only show a highlight
                        // if this list has a real "current value" to
                        // indicate (Difficulty, Camera Sway, Language,
                        // Quality, Resolution). Pure navigation lists
                        // (Volume, Controls) show no highlight at all
                        // here (Edward, 2026), and never pulse even when
                        // they do, since the cursor isn't really there
                        // yet.
                        selectedHere = node->isSettingsList ? node->initialSelectedChild : -1;
                        pulseHere = false;
                    }

                    int columnWidth = DrawColumn(renderer, node->children, selectedHere, pulseHere, columnX, columnTop, rowHeight, scale, language);
                    columnX += columnWidth + scale * 4; // Edward, 2026: needs a visible gap between adjacent columns too, not just between stacked rows within one

                    if (depth >= optionsPath.size()) { break; }
                    node = &node->children[static_cast<size_t>(optionsPath[depth])];
                    if (node->kind != MenuNodeKind::List) { break; } // leaf - nothing further to preview as a column
                }

                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::PressEscToGoBack, language),
                                (windowW - TextWidth(ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::PressEscToGoBack, language), scale)) / 2, windowH - rowHeight * 2, scale, COLOR_GREEN);
                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::PressEnterToSelect, language),
                                (windowW - TextWidth(ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::PressEnterToSelect, language), scale)) / 2, windowH - rowHeight, scale, COLOR_GREEN);

                // Redefine controls (Edward, 2026) - a prominent, centred
                // prompt while waiting for the next key/mouse-button
                // press, since all other input is ignored in this state
                // and the player needs a clear signal.
                if (awaitingRebind)
                {
                    bool isMouse = (rebindDevice == ALTEngine::Bootstrap::DeviceKind::Mouse);
                    std::string promptLine1 = ALTEngine::Bootstrap::Tr(isMouse ? ALTEngine::Bootstrap::StringId::PressAMouseButtonOrWheelToBind
                                                                                : ALTEngine::Bootstrap::StringId::PressAKeyToBind, language);
                    std::string promptLine2 = "\"" + rebindActionLabel + "\"";
                    std::string promptLine3 = ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::OrEscToCancel, language);
                    int promptWidth = std::max({ TextWidth(promptLine1, scale), TextWidth(promptLine2, scale), TextWidth(promptLine3, scale) }) + scale * 16;
                    int promptHeight = rowHeight * 3 + scale * 8;
                    SDL_FRect promptBox{ static_cast<float>((windowW - promptWidth) / 2), static_cast<float>((windowH - promptHeight) / 2),
                                         static_cast<float>(promptWidth), static_cast<float>(promptHeight) };
                    DrawRoundedRect(renderer, promptBox, static_cast<float>(scale * 3), COLOR_HIGHLIGHT_BG);

                    int promptTextY = (windowH - promptHeight) / 2 + scale * 4;
                    DrawBitmapText(renderer, promptLine1, (windowW - TextWidth(promptLine1, scale)) / 2, promptTextY, scale, COLOR_WHITE);
                    DrawBitmapText(renderer, promptLine2, (windowW - TextWidth(promptLine2, scale)) / 2, promptTextY + rowHeight, scale, COLOR_GREEN);
                    DrawBitmapText(renderer, promptLine3, (windowW - TextWidth(promptLine3, scale)) / 2, promptTextY + rowHeight * 2, scale, COLOR_GREEN_DIM);
                }
            }
            else // Credits
            {
                DrawMenuBackground(renderer, optionsBg, optionsBgW, optionsBgH);
                // Placeholder - real credits scroll (parsing CD/GFX/CREDITS.TXT
                // and animating it) isn't implemented yet.
                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::CreditsTitle, language), scale * 8, scale * 8, scale, COLOR_GREEN);
                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::CreditsPlaceholder, language),
                                scale * 8, scale * 8 + rowHeight, scale, COLOR_GREEN_DIM);
                DrawBitmapText(renderer, ALTEngine::Bootstrap::Tr(ALTEngine::Bootstrap::StringId::PressEscToGoBack, language), scale * 8, scale * 8 + rowHeight * 3, scale, COLOR_GREEN);
            }

            SDL_RenderPresent(renderer);
        }

        MusicPlayer::Stop();

        if (mainBg) { SDL_DestroyTexture(mainBg); }
        if (optionsBg) { SDL_DestroyTexture(optionsBg); }

        return result;
    }
}
