#pragma once

#include <filesystem>
#include <vector>

#include "PatchSet.h"

namespace ALTEngine::Formats
{
    enum class PatchOutcome
    {
        Applied,        // edits were written
        AlreadyApplied, // first edit's bytes already matched - skipped
        Failed,         // target file missing, too small, or a write error
    };

    struct PatchResult
    {
        PatchOperation const* operation = nullptr;
        PatchOutcome outcome = PatchOutcome::Failed;
        std::string error; // set when outcome == Failed
    };

    // Applies a list of PatchOperations against a game root directory.
    // Idempotent: before writing, checks whether the file's bytes at the
    // first edit's offset already equal that edit's target bytes, and
    // skips the whole operation if so. Since every edit in this patch set
    // is a same-length in-place overwrite, re-applying an already-patched
    // file would be harmless anyway - this check is about avoiding
    // unnecessary disk writes and giving an honest "already applied"
    // report, not correctness.
    class PatchRunner
    {
    public:
        static std::vector<PatchResult> ApplyAll(
            const std::filesystem::path& gameRoot,
            const std::vector<PatchOperation>& operations);
    };
}
