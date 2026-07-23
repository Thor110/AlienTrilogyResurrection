#pragma once

#include <chrono>
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
}
