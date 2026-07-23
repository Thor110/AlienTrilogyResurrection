#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "DiscManifest.h"

namespace ALTEngine::Formats
{
    struct InstallProgress
    {
        std::string currentFile;
        int filesCompleted = 0;
        int filesTotal = 0;
    };

    struct InstallResult
    {
        bool success = true;
        std::vector<std::string> failedFiles; // relative paths that failed to copy
    };

    // Copies README.TXT and CD/* per `manifest` from discRoot to
    // destinationRoot, preserving structure. Per-file resilient - a
    // single failed file is recorded in InstallResult::failedFiles rather
    // than aborting the whole install. Does NOT rip CDDA audio - that's
    // CddaRipper's job, run separately.
    class Installer
    {
    public:
        static InstallResult CopyFiles(
            const std::filesystem::path& discRoot,
            const std::filesystem::path& destinationRoot,
            const DiscManifest& manifest,
            const std::function<void(const InstallProgress&)>& onProgress = nullptr);
    };
}
