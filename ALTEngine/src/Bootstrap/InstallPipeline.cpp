#include "InstallPipeline.h"
#include "Config.h"
#include "PlatformPaths.h"
#include "TestingSettings.h"

#include "../Game/LevelCleanup.h"

#include "../Formats/CddaRipper.h"
#include "../Formats/DiscManifest.h"
#include "../Formats/Installer.h"
#include "../Formats/ModelRemoval.h"
#include "../Formats/PatchLoader.h"
#include "../Formats/PatchRunner.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>

#include <nlohmann/json.hpp>

namespace ALTEngine::Bootstrap
{
    using ALTEngine::Formats::CddaRipper;
    using ALTEngine::Formats::CddaRipSummary;
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

    namespace
    {
        // LEVEL CLEANUP's files (Testing > Level Cleanup; Game::LevelCleanup).
        // Beside each cleaned level: <file>.untrimmed, the level as the fixes
        // left it. The order every start: put the untrimmed file back (the
        // cleaned one set aside as <file>.cleaned), run the fixes against it
        // as they were written, then - with the option on - put the cleaned
        // copy back if it was made from exactly this file (its CRC is kept in
        // LevelCleanup.json), or clean it again; with it off, the untrimmed
        // file stays and the cleaned copy goes. Only renames, unless a fix
        // changed the level.
        std::filesystem::path With(const std::filesystem::path& p, const char* suffix)
        {
            auto out = p;
            out += suffix;
            return out;
        }

        bool ReadAll(const std::filesystem::path& p, std::vector<uint8_t>& out)
        {
            std::ifstream in(p, std::ios::binary);
            if (!in) { return false; }
            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }

        bool WriteAll(const std::filesystem::path& p, const std::vector<uint8_t>& bytes)
        {
            const auto temp = With(p, ".tmp");
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (!out) { return false; }
            }
            std::error_code ec;
            std::filesystem::rename(temp, p, ec);
            if (ec) { std::filesystem::remove(temp, ec); return false; }
            return true;
        }

        void SetCleanedAside(const std::filesystem::path& cdDirectory)
        {
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto untrimmed = With(file, ".untrimmed");
                if (!std::filesystem::exists(untrimmed, ec)) { continue; }
                const auto cleaned = With(file, ".cleaned");
                std::filesystem::remove(cleaned, ec);
                if (std::filesystem::exists(file, ec)) { std::filesystem::rename(file, cleaned, ec); }
                std::filesystem::rename(untrimmed, file, ec);
                if (ec) { std::cout << "Level cleanup: could not restore " << file.string() << ": " << ec.message() << "\n"; }
            }
        }

        void ApplyLevelCleanup(const std::filesystem::path& cdDirectory, bool enabled)
        {
            const auto statePath = cdDirectory / "LevelCleanup.json";
            nlohmann::json state = nlohmann::json::object();
            {
                std::ifstream in(statePath);
                if (in) { try { in >> state; } catch (...) { state = nlohmann::json::object(); } }
                if (!state.is_object()) { state = nlohmann::json::object(); }
            }
            int cleaned = 0, kept = 0, restored = 0;
            size_t saved = 0;
            std::error_code ec;
            for (const auto& relative : ALTEngine::Game::LevelCleanup::LevelFiles(cdDirectory))
            {
                const auto file = cdDirectory / relative;
                const auto cleanedCopy = With(file, ".cleaned");
                const std::string key = relative.generic_string();
                if (!enabled)
                {
                    if (std::filesystem::remove(cleanedCopy, ec)) { ++restored; }
                    state.erase(key);
                    continue;
                }
                std::vector<uint8_t> bytes;
                if (!ReadAll(file, bytes)) { continue; }
                const uint32_t source = ALTEngine::Formats::Crc32(bytes.data(), bytes.size());
                if (std::filesystem::exists(cleanedCopy, ec) && state.contains(key) && state[key].get<uint32_t>() == source)
                {
                    std::filesystem::rename(file, With(file, ".untrimmed"), ec);
                    if (!ec) { std::filesystem::rename(cleanedCopy, file, ec); }
                    if (ec) { std::cout << "Level cleanup: could not swap " << file.string() << ": " << ec.message() << "\n"; }
                    ++kept;
                    continue;
                }
                std::filesystem::remove(cleanedCopy, ec);
                ALTEngine::Game::LevelCleanup::Report report;
                std::string error;
                std::vector<uint8_t> original = bytes;
                if (!ALTEngine::Game::LevelCleanup::Clean(bytes, key.find("/L9") != std::string::npos, report, error))
                {
                    std::cout << "  [FAILED]   " << key << " - " << error << "\n";
                    continue;
                }
                if (!report.Changed()) { state.erase(key); continue; }
                if (!WriteAll(With(file, ".untrimmed"), original) || !WriteAll(file, bytes))
                {
                    std::cout << "  [FAILED]   " << key << " - could not write the cleaned level\n";
                    std::filesystem::remove(With(file, ".untrimmed"), ec);
                    continue;
                }
                state[key] = source;
                ++cleaned;
                saved += report.bytesBefore - report.bytesAfter;
                std::cout << "  [cleaned]  " << key << ": " << report.facesRemoved << " faces and "
                          << report.verticesRemoved << " vertices never drawn taken out"
                          << (report.firstCellRestored ? ", first cell's " + std::to_string(report.firstCellFaces) + " faces restored" : std::string())
                          << "\n";
            }
            std::ofstream out(statePath, std::ios::trunc);
            out << state.dump(1);
            if (enabled) { std::cout << "Level cleanup: " << cleaned << " cleaned (" << saved << " bytes), " << kept << " already clean.\n"; }
            else if (restored) { std::cout << "Level cleanup off: " << restored << " levels back as the fixes left them.\n"; }
        }
    }

    void RunPatches(const std::filesystem::path& cdDirectory)
    {
        // Update Fixes -> Boolean -> Trim -> Add Faces: the fixes always run
        // against the level files as shipped, so a cleaned level is put back
        // first (see ApplyLevelCleanup).
        SetCleanedAside(cdDirectory);

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

        // Duplicate and unused door/lift models go first: the byte patches
        // are written against the files without them (ModelRemoval.h).
        try
        {
            using namespace ALTEngine::Formats;
            const std::vector<ModelRemoval> removals = ModelRemovalLoader::Load(patchesJson);
            size_t removed = 0, already = 0, removalFailed = 0;
            for (const auto& result : ModelRemovalRunner::ApplyAll(cdDirectory, removals))
            {
                switch (result.outcome)
                {
                case ModelRemovalOutcome::Applied: ++removed; break;
                case ModelRemovalOutcome::AlreadyApplied: ++already; break;
                case ModelRemovalOutcome::Failed:
                    ++removalFailed;
                    std::cout << "  [FAILED]   " << result.removal->targetFile << " - " << result.error << "\n";
                    break;
                }
            }
            std::cout << "Model removals: " << removed << " applied, " << already << " already applied, "
                      << removalFailed << " failed.\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to load the model removals from " << patchesJson.string() << ": " << e.what() << "\n";
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

        Config config;
        ApplyLevelCleanup(cdDirectory, TestingSettings(config).IsOn(TestingOption::LevelCleanup));
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

        // The music: the disc's audio tracks as CD/MUSIC/trackNN.wav, by
        // whichever route this system has (CddaRipper::RipDisc).
        std::filesystem::path musicDir = destination / "CD" / "MUSIC";
        std::error_code ec;
        std::filesystem::create_directories(musicDir, ec);

        const CddaRipSummary rip = CddaRipper::RipDisc(discRoot, musicDir,
            [](const std::string& line) { std::cout << line << "\n"; });
        if (!rip.error.empty())
        {
            std::cout << "CDDA rip failed entirely: " << rip.error << "\n";
            std::cout << "Continuing without music - this can be retried later.\n";
        }
        else
        {
            std::cout << "CDDA rip: " << rip.ripped << " tracks ripped, " << rip.failed << " failed.\n";
        }

        return true;
    }
}
