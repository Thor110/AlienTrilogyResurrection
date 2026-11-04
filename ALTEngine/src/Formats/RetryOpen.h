#pragma once

#include <chrono>
#include <filesystem>
#include <thread>

namespace ALTEngine::Formats
{
    // Retries `tryOpen` (should attempt to open a stream and return
    // whether it succeeded) a few times with a short delay between
    // attempts. Files that were *just* created or written (exactly the
    // case right after Installer::CopyFiles, right before PatchRunner
    // reopens them) are commonly held briefly by OneDrive sync, antivirus
    // real-time scanning, or the Windows Search indexer - none of which
    // is a real permission problem, just a transient lock that clears
    // within a few hundred milliseconds. 5 attempts / 150ms apart = up to
    // ~600ms of extra latency, only ever paid on the failure path.
    template <typename TryOpenFunc>
    bool RetryOnTransientLock(TryOpenFunc&& tryOpen, int maxAttempts = 5, std::chrono::milliseconds delay = std::chrono::milliseconds(150))
    {
        for (int attempt = 0; attempt < maxAttempts; ++attempt)
        {
            if (tryOpen()) { return true; }
            if (attempt + 1 < maxAttempts) { std::this_thread::sleep_for(delay); }
        }
        return false;
    }

    // Disc-sourced files (a physical CD or a mounted image) are read-only
    // media, and a copy can carry that attribute over - a permanent file
    // attribute, not a transient lock, so no retry ever clears it. Cleared
    // after every install copy and before every patch write. Not checked: if
    // it fails, the write that follows reports the error clearly.
    inline void ClearReadOnly(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write,
            std::filesystem::perm_options::add, ec);
    }
}
