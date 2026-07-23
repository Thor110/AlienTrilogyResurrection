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
#include "Video/VideoPlayer.h"

using namespace ALTEngine::Bootstrap;
using namespace ALTEngine::Formats;
using namespace ALTEngine::Video;
using namespace ALTEngine::Menu;

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
    MenuResult menuResult = MenuController::Run(cdDirectory, renderSettings, resolutionSettings, language);
    if (menuResult.windowClosed)
    {
        std::cout << "Boot window closed. Aborting.\n";
        AppWindow::Instance().Shutdown();
        PauseBeforeExit();
        return 1;
    }
    std::cout << "Menu selection: " << menuResult.action << "\n";

    // NEXT: actually act on menuResult.action (Start Game / Multiplayer /
    // Load Game), and the real 3D model renderer (SDL GPU API) to replace
    // the menu's placeholder boxes.

    AppWindow::Instance().Shutdown();
    PauseBeforeExit();
    return 0;
}
