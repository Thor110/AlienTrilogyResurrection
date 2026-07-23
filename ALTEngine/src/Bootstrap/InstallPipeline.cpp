#include "InstallPipeline.h"
#include "PlatformPaths.h"

#include "../Formats/CddaRipper.h"
#include "../Formats/DiscManifest.h"
#include "../Formats/Installer.h"
#include "../Formats/PatchLoader.h"
#include "../Formats/PatchRunner.h"

#include <cstdio>
#include <iostream>

namespace ALTEngine::Bootstrap
{
    using ALTEngine::Formats::CddaRipper;
    using ALTEngine::Formats::CddaTrack;
    using ALTEngine::Formats::DiscManifest;
    using ALTEngine::Formats::DiscManifestLoader;
    using ALTEngine::Formats::InstallProgress;
    using ALTEngine::Formats::Installer;
    using ALTEngine::Formats::InstallResult;
    using ALTEngine::Formats::PatchLoader;
    using ALTEngine::Formats::PatchOperation;
    using ALTEngine::Formats::PatchOutcome;
    using ALTEngine::Formats::PatchResult;
    using ALTEngine::Formats::PatchRunner;

    void RunPatches(const std::filesystem::path& cdDirectory)
    {
        std::filesystem::path patchesJson = ExecutableDirectory() / "data" / "Patches.json";

        std::vector<PatchOperation> operations;
        try
        {
            operations = PatchLoader::Load(patchesJson);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << patchesJson.string() << ": " << e.what() << "\n";
            return;
        }

        std::cout << "Applying " << operations.size() << " patch operations...\n";
        std::vector<PatchResult> results = PatchRunner::ApplyAll(cdDirectory, operations);

        size_t applied = 0, alreadyApplied = 0, failed = 0;
        for (const auto& result : results)
        {
            switch (result.outcome)
            {
            case PatchOutcome::Applied:
                ++applied;
                std::cout << "  [applied]  " << result.operation->targetFile;
                if (!result.operation->note.empty()) { std::cout << "  (" << result.operation->note << ")"; }
                std::cout << "\n";
                break;
            case PatchOutcome::AlreadyApplied:
                ++alreadyApplied;
                break;
            case PatchOutcome::Failed:
                ++failed;
                std::cout << "  [FAILED]   " << result.operation->targetFile << " - " << result.error << "\n";
                break;
            }
        }

        std::cout << "Patches: " << applied << " applied, " << alreadyApplied
                   << " already applied, " << failed << " failed.\n";
    }

    bool InstallFromDisc(const std::filesystem::path& discRoot, const std::filesystem::path& destination)
    {
        std::cout << "Installing from disc (" << discRoot.string() << ") to " << destination.string() << "...\n";

        std::filesystem::path manifestPath = ExecutableDirectory() / "data" / "DiscFileManifest.json";
        DiscManifest manifest;
        try
        {
            manifest = DiscManifestLoader::Load(manifestPath);
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load " << manifestPath.string() << ": " << e.what() << "\n";
            return false;
        }

        InstallResult copyResult = Installer::CopyFiles(discRoot, destination, manifest,
            [](const InstallProgress& p) {
                if (p.filesCompleted % 25 == 0 || p.filesCompleted == p.filesTotal)
                {
                    std::cout << "  [" << p.filesCompleted << "/" << p.filesTotal << "] " << p.currentFile << "\n";
                }
            });

        if (!copyResult.success)
        {
            std::cout << "Install incomplete - " << copyResult.failedFiles.size() << " file(s) failed:\n";
            for (const auto& f : copyResult.failedFiles) { std::cout << "  " << f << "\n"; }
            return false;
        }
        std::cout << "File copy complete.\n";

        // Patch immediately, as part of installation itself - done and
        // dusted from the moment of installation, not deferred to
        // whatever happens to run next.
        RunPatches(destination / "CD");

        // CDDA rip - drive letter only (e.g. "D:"), not the trailing
        // backslash discRoot carries.
        std::string driveLetter = discRoot.string().substr(0, 2);
        std::filesystem::path musicDir = destination / "CD" / "MUSIC";
        std::error_code ec;
        std::filesystem::create_directories(musicDir, ec);

        try
        {
            std::vector<CddaTrack> tracks = CddaRipper::ReadToc(driveLetter);
            int ripped = 0, failed = 0;
            for (const auto& track : tracks)
            {
                if (!track.isAudio) { continue; } // track 1 is the data track
                try
                {
                    char nameBuf[32];
                    std::snprintf(nameBuf, sizeof(nameBuf), "track%02d.wav", track.trackNumber);
                    std::cout << "  Ripping " << nameBuf << "...\n";
                    CddaRipper::RipTrackToWav(driveLetter, track, musicDir / nameBuf);
                    ++ripped;
                }
                catch (const std::exception& e)
                {
                    std::cout << "  Failed to rip track " << track.trackNumber << ": " << e.what() << "\n";
                    ++failed;
                }
            }
            std::cout << "CDDA rip: " << ripped << " tracks ripped, " << failed << " failed.\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "CDDA rip failed entirely (TOC read): " << e.what() << "\n";
            std::cout << "Continuing without music - this can be retried later.\n";
        }

        return true;
    }
}
