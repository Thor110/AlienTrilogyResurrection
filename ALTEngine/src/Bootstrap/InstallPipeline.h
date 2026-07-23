#pragma once

#include <filesystem>

namespace ALTEngine::Bootstrap
{
    // Applies data/Patches.json to `cdDirectory`. Idempotent (see
    // PatchRunner) - safe to call every boot regardless of install
    // source. Also called as the final data step of InstallFromDisc
    // below, so a fresh install is patched immediately as part of
    // installing, not just "eventually, on some later boot".
    void RunPatches(const std::filesystem::path& cdDirectory);

    // Runs a fresh install from a located disc: copies the file
    // manifest, applies patches, then rips the 16 CDDA music tracks -
    // patches specifically happen right after the file copy and before
    // CDDA, so the critical data-patching step completes even if CDDA
    // ripping has trouble. CDDA ripping (Windows raw CD-ROM IOCTLs) is
    // UNTESTED - see CddaRipper.h; a CDDA failure is logged but doesn't
    // fail the whole install, since the game is playable without music,
    // just quieter.
    bool InstallFromDisc(const std::filesystem::path& discRoot, const std::filesystem::path& destination);
}
