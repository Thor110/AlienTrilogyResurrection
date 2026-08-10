#include <iostream>
#include <optional>

#include "Tools/LevelLightScanner.h"
#include "Bootstrap/AppWindow.h"
#include "Bootstrap/Config.h"
#include "Bootstrap/DiscLocator.h"
#include "Bootstrap/GameLocator.h"
#include "Bootstrap/GameplaySettings.h"
#include "Bootstrap/AudioSettings.h"
#include "Bootstrap/KeyBindings.h"
#include "Bootstrap/Strings.h"
#include "Bootstrap/ImageDisplay.h"
#include "Bootstrap/InstallPipeline.h"
#include "Bootstrap/Localization.h"
#include "Bootstrap/PlatformPaths.h"
#include "Bootstrap/RenderSettings.h"
#include "Bootstrap/ResolutionSettings.h"
#include "Formats/SplashImageLoader.h"
#include "Menu/MenuController.h"
#include "Renderer/ModelPreview.h"
#include "Renderer/ModelRenderer.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MissionBriefingScreen.h"
#include "Screens/MultiplayerScreens.h"
#include "Screens/SaveSlotScreen.h"
#include "Video/OverrideVideo.h"
#include "Video/VideoPlayer.h"

using namespace ALTEngine::Bootstrap;
using namespace ALTEngine::Formats;
using namespace ALTEngine::Video;
using namespace ALTEngine::Menu;
using namespace ALTEngine::Screens;

namespace
{
    // A console app launched by double-clicking in Explorer closes its
    // window the instant main() returns, which hides any diagnostic
    // output entirely - looks exactly like "nothing happened". Pause
    // before any early-exit path so that's not a dead end while debugging.
    void PauseBeforeExit()
    {
        std::cout << "\nPress Enter to close...";
        std::cin.get();
    }

    // GraphicsViewer.cs tries these extensions in this order, since which
    // one's actually present varies by file/edition. Same here.
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

    // Shows CD/GFX/LEGAL - the first thing in the documented boot order
    // (data/BootSequence.json). Returns false only if the user closed the
    // window outright (treated the same as DirectoryBrowser's abort).
    bool ShowLegalSplash(const std::filesystem::path& cdDirectory)
    {
        std::filesystem::path gfxDir = cdDirectory / "GFX";
        std::filesystem::path palPath = cdDirectory / "PALS" / "LEGAL.PAL";

        auto bndPath = ResolveGfxFile(gfxDir, "LEGAL");
        if (!bndPath.has_value())
        {
            std::cout << "Could not find LEGAL graphics file under " << gfxDir.string() << " - skipping splash.\n";
            return true;
        }
        std::error_code ec;
        if (!std::filesystem::exists(palPath, ec))
        {
            std::cout << "Could not find " << palPath.string() << " - skipping splash.\n";
            return true;
        }

        try
        {
            SplashImage image = SplashImageLoader::Load(*bndPath, palPath, /*paletteTrimmed*/ false);
            std::cout << "Showing LEGAL splash (" << image.width << "x" << image.height << ")...\n";
            return ImageDisplay::Show(image.rgba, image.width, image.height, /*maxDurationMs*/ 5000);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load/show LEGAL splash: " << e.what() << "\n";
            return true; // don't abort the whole boot over a missing splash
        }
    }

    // Plays the four intro videos in the documented order (see
    // data/BootSequence.json), resolved for `language` via
    // LocalizedBaseName. A missing file is skipped, same philosophy as
    // ShowLegalSplash - only an actual window-close aborts the boot.
    // TODO: drive this list from BootSequence.json directly rather than
    // hardcoding it here, once there's a general boot-step runner.
    bool PlayIntroVideos(const std::filesystem::path& cdDirectory, Language language)
    {
        std::filesystem::path aviDir = cdDirectory / "AVI";
        const char* baseNames[] = { "FOXDKAUD", "ALOGODUK", "PRBLOGO", "INTRO" };

        for (const char* baseName : baseNames)
        {
            if (auto overridePath = FindOverrideVideo(cdDirectory, baseName))
            {
                std::cout << "Playing override " << overridePath->string() << "...\n";
                if (!VideoPlayer::Play(*overridePath))
                {
                    return false; // window closed - abort boot
                }
                continue;
            }

            std::string localizedName = LocalizedBaseName(baseName, language);
            std::filesystem::path path = aviDir / (localizedName + ".AVI");
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                std::cout << "Could not find " << path.string() << " - skipping.\n";
                continue;
            }

            std::cout << "Playing " << localizedName << ".AVI...\n";
            if (!VideoPlayer::Play(path))
            {
                return false; // window closed - abort boot
            }
        }
        return true;
    }
}

int main(int, char**)
{
    std::cout << "ALTEngine boot starting...\n";

    Config config;
    std::cout << "Config file: " << config.FilePath().string() << "\n";

    GameLocator locator(config);
    std::cout << "Locating install directory...\n";

    if (!locator.TryAutoLocate().has_value())
    {
        DiscLocateResult disc = DiscLocator::FindDisc();
        if (disc.found)
        {
            std::filesystem::path destination = ExecutableDirectory() / "GameData";
            std::cout << "Found disc at " << disc.discRoot.string() << "\n";
            if (InstallFromDisc(disc.discRoot, destination))
            {
                config.Set("GameDirectory", destination.string());
            }
        }
    }

    LocateResult result = locator.Locate();
    if (!result.success)
    {
        std::cout << "No install directory selected. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    std::cout << "Installation directory: " << result.gameDirectory.string() << "\n";

    // Patching happens as part of installation itself (InstallFromDisc)
    // - no separate re-check needed here every boot.
    std::filesystem::path cdDirectory = result.gameDirectory / "CD";

    // Must happen before anything below could possibly call Tr() -
    // LoadedLanguagePacks() only ever initializes its cache once, on
    // first use, so setting this any later would have no effect
    // (Edward, 2026: "move the LANGUAGE folder to GameData\CD\LANGUAGE
    // so that it sits alongside the original language files").
    ALTEngine::Bootstrap::SetLanguagePackDirectory(cdDirectory);

    // ---- ONE-SHOT LIGHT/ANIMATION SCAN - delete this whole block when done --
    //
    // Runs only when data/ScanLights.flag exists, and deletes the flag itself
    // afterwards, so it happens exactly once and cannot slow down or surprise a
    // normal launch. Writes data/LightManifest.json - a catalogue of every face
    // in every level (campaign and multiplayer) that carries a light or an
    // animation, which is what the OBJ replacement system's alt_light/alt_anim
    // tags are authored against.
    //
    // TO REMOVE: delete this block, src/Tools/LevelLightScanner.{h,cpp}, the
    // CMakeLists entry for it, and the include below. Nothing else uses it.
    {
        std::filesystem::path flag = "data/ScanLights.flag";
        std::error_code flagEc;
        if (std::filesystem::exists(flag, flagEc))
        {
            ALTEngine::Tools::ScanResult scan = ALTEngine::Tools::ScanAllLevelsForLights(
                cdDirectory, "data/LevelManifest.json", "data/LightManifest.json");
            SDL_Log("LevelLightScanner: %s%s", scan.ok ? "" : "FAILED - ", scan.message.c_str());
            // Remove the flag whether or not it worked, so a failure does not
            // re-run on every launch. Re-create the file to scan again.
            std::filesystem::remove(flag, flagEc);
        }
    }
    // ---- end one-shot scan -------------------------------------------------

    if (!ShowLegalSplash(cdDirectory))
    {
        std::cout << "Boot window closed. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    // Language now actually persists (Edward, 2026) - LanguageSettings
    // needs `config`, which already exists by this point, so it's
    // constructed here rather than alongside renderSettings/
    // resolutionSettings below.
    LanguageSettings languageSettings(config);
    Language language = languageSettings.Get();

    if (!PlayIntroVideos(cdDirectory, language))
    {
        std::cout << "Boot window closed. Aborting.\n";
        ALTEngine::Renderer::ModelRenderer::Shutdown();
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    RenderSettings renderSettings(config);
    ResolutionSettings resolutionSettings(config);
    DifficultySettings difficultySettings(config);
    CameraSwaySettings cameraSwaySettings(config);
    KeyBindings keyBindings(config);
    AudioSettings audioSettings(config);

    // Applied once, here, rather than than every time MenuController::Run
    // is re-entered (which would flicker the window every time the
    // player returns to the menu from gameplay) - EnsureCreated always
    // creates the window fullscreen regardless of what was last saved,
    // so a persisted Windowed/Borderless preference needs reapplying
    // explicitly on boot, or it's silently lost every restart.
    AppWindow::Instance().SetVSync(renderSettings.VSync());
    {
        auto savedResolution = resolutionSettings.Get();
        DisplayMode displayMode = renderSettings.GetDisplayMode();
        if (savedResolution.has_value())
        {
            if (displayMode == DisplayMode::Fullscreen)
            {
                // SetDisplayMode alone only switches which kind of
                // fullscreen is active - the exclusive resolution
                // itself needs setting separately, same as the menu's
                // own Resolution handling does.
                AppWindow::Instance().ApplyFullscreenResolution(savedResolution->first, savedResolution->second);
            }
            AppWindow::Instance().SetDisplayMode(displayMode, savedResolution->first, savedResolution->second);
        }
        else
        {
            AppWindow::Instance().SetDisplayMode(displayMode);
        }
    }

    std::vector<int> mainPath = { 0 };

    while (true)
    {
        MenuResult menuResult = MenuController::Run(cdDirectory, renderSettings, resolutionSettings, difficultySettings,
                                                      cameraSwaySettings, languageSettings, keyBindings, audioSettings, language, mainPath);
        if (menuResult.windowClosed)
        {
            std::cout << "Boot window closed. Aborting.\n";
            ALTEngine::Renderer::ModelRenderer::Shutdown();
            AppWindow::Instance().Shutdown();
            PauseBeforeExit();
            return 1;
        }
        std::cout << "Menu selection: " << menuResult.action << "\n";

        if (menuResult.action == "Exit")
        {
            // Escape at the main menu root - previously fell through
            // unrecognized here, so the loop just called
            // MenuController::Run() again, which explains both observed
            // symptoms exactly: never actually quit, and restarted the
            // music (MusicPlayer::PlayLooped runs again at the top of
            // Run()).
            break;
        }

        if (menuResult.action == "Multiplayer")
        {
            MultiplayerSettings mpSettings;
            int multiplayerCursor = 0;
            bool inMultiplayerMenu = true;
            bool windowClosed = false;
            while (inMultiplayerMenu)
            {
                MultiplayerMainResult mainResult = MultiplayerMainScreen::Run(cdDirectory, multiplayerCursor);
                if (mainResult.windowClosed) { windowClosed = true; break; }

                switch (mainResult.choice)
                {
                case MultiplayerMainChoice::StartGame:
                {
                    MultiplayerStartResult r = MultiplayerStartScreen::Run(cdDirectory, mpSettings);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; }
                    // r.startedGame has nowhere to hand off to yet - no
                    // real networking exists. Falls back to the
                    // multiplayer menu.
                    break;
                }
                case MultiplayerMainChoice::JoinGame:
                {
                    MultiplayerSubResult r = MultiplayerJoinScreen::Run(cdDirectory);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; }
                    break;
                }
                case MultiplayerMainChoice::Options:
                {
                    MultiplayerSubResult r = MultiplayerOptionsScreen::Run(cdDirectory, mpSettings);
                    if (r.windowClosed) { windowClosed = true; inMultiplayerMenu = false; }
                    break;
                }
                case MultiplayerMainChoice::Back:
                default:
                    inMultiplayerMenu = false;
                    break;
                }
            }
            if (windowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            continue; // Escape/Back - return to the main menu, not exit
        }

        if (menuResult.action == "Load Game")
        {
            SaveSlotResult loadResult = SaveSlotScreen::Run(cdDirectory, SaveSlotMode::Load, StubSaveSlots());
            if (loadResult.windowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            // NEXT: actually load the chosen slot's save data and resume
            // gameplay from it, once a real save system exists. For now,
            // selecting a slot (or backing out) has nowhere to go but
            // back to the main menu.
            continue;
        }

        // TEMPORARY level select - jump straight into any level for testing.
        // To remove: delete this block, the levelSelectLabels plumbing in
        // MenuController, the list in MenuTree, and MenuResult::levelCode.
        if (menuResult.action == "Level Select" && !menuResult.levelCode.empty()
            && menuResult.levelCode.find_first_of("0123456789") != std::string::npos)
        {
            std::cout << "Level Select: " << menuResult.levelCode << "\n";

            // Run the briefing first. It is not just presentation - it is what
            // preloads PICKMOD and OBJ3D, so skipping it left every crate,
            // barrel, switch and pickup with no model to draw. That is why
            // objects only appeared when starting the first level the normal way
            // (Edward, 2026).
            //
            // Multiplayer levels have no briefing text, so they get the same
            // preload without the screen. Anything whose code starts with 9 is
            // multiplayer; they are in the list for testing only.
            bool multiplayerLevel = (menuResult.levelCode[0] == '9');
            if (!multiplayerLevel)
            {
                MissionBriefingResult briefingResult = MissionBriefingScreen::Run(cdDirectory, language, menuResult.levelCode);
                if (briefingResult.windowClosed)
                {
                    std::cout << "Boot window closed. Aborting.\n";
                    ALTEngine::Renderer::ModelRenderer::Shutdown();
                    AppWindow::Instance().Shutdown();
                    PauseBeforeExit();
                    return 1;
                }
            }
            else
            {
                MissionBriefingScreen::PreloadObjectModels(cdDirectory, language);
            }

            GameplayResult gameplayResult = GameplayScreen::Run(cdDirectory, language, menuResult.levelCode, keyBindings, audioSettings,
                                                                  renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings);
            if (gameplayResult.outcome == GameplayOutcome::WindowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            continue;
        }

        if (menuResult.action == "Start Game")
        {
            // Hardcoded to the first level for now - no level-select flow
            // exists (there never was one in the original game either -
            // levels just progress, with save/load presumably selecting a
            // level once that exists). Wire this up to real progression
            // once there's somewhere for it to come from.
            MissionBriefingResult briefingResult = MissionBriefingScreen::Run(cdDirectory, language, "1.1.1");
            if (briefingResult.windowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }

            GameplayResult gameplayResult = GameplayScreen::Run(cdDirectory, language, "1.1.1", keyBindings, audioSettings,
                                                                  renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings);
            if (gameplayResult.outcome == GameplayOutcome::WindowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                ALTEngine::Renderer::ModelRenderer::Shutdown();
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            continue; // Exit Game (from the pause menu) - return to the main menu
        }

        // Any other selection (e.g. Options, handled entirely within
        // MenuController itself) - just re-show the main menu.
    }

    ALTEngine::Renderer::ModelRenderer::Shutdown();
    AppWindow::Instance().Shutdown();
    PauseBeforeExit();
    return 0;
}
