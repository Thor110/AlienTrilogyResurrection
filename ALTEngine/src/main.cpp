#include <iostream>
#include <optional>

#include "Bootstrap/AppWindow.h"
#include "Bootstrap/Config.h"
#include "Bootstrap/DiscLocator.h"
#include "Bootstrap/GameLocator.h"
#include "Bootstrap/ImageDisplay.h"
#include "Bootstrap/InstallPipeline.h"
#include "Bootstrap/Localization.h"
#include "Bootstrap/PlatformPaths.h"
#include "Bootstrap/RenderSettings.h"
#include "Bootstrap/ResolutionSettings.h"
#include "Formats/SplashImageLoader.h"
#include "Menu/MenuController.h"
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
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    std::cout << "Installation directory: " << result.gameDirectory.string() << "\n";

    // Patching happens as part of installation itself (InstallFromDisc)
    // - no separate re-check needed here every boot.
    std::filesystem::path cdDirectory = result.gameDirectory / "CD";

    if (!ShowLegalSplash(cdDirectory))
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    // No settings/persistence system yet - defaults to English until
    // there's somewhere for a language choice to actually live.
    Language language = Language::English;

    if (!PlayIntroVideos(cdDirectory, language))
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }

    RenderSettings renderSettings(config);
    ResolutionSettings resolutionSettings(config);

    while (true)
    {
        MenuResult menuResult = MenuController::Run(cdDirectory, renderSettings, resolutionSettings, language);
        if (menuResult.windowClosed)
        {
            std::cout << "Boot window closed. Aborting.\n";
            AppWindow::Instance().Shutdown();
            PauseBeforeExit();
            return 1;
        }
        std::cout << "Menu selection: " << menuResult.action << "\n";

        if (menuResult.action == "Multiplayer")
        {
            MultiplayerSettings mpSettings;
            bool inMultiplayerMenu = true;
            bool windowClosed = false;
            while (inMultiplayerMenu)
            {
                MultiplayerMainResult mainResult = MultiplayerMainScreen::Run(cdDirectory);
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
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }

            GameplayResult gameplayResult = GameplayScreen::Run(cdDirectory, language, "1.1.1");
            if (gameplayResult.outcome == GameplayOutcome::WindowClosed)
            {
                std::cout << "Boot window closed. Aborting.\n";
                AppWindow::Instance().Shutdown();
                PauseBeforeExit();
                return 1;
            }
            continue; // Exit Game (from the pause menu) - return to the main menu
        }

        // Any other selection (e.g. Options, handled entirely within
        // MenuController itself) - just re-show the main menu.
    }
}
