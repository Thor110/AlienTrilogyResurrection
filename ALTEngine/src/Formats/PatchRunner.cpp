#include "PatchRunner.h"
#include "BinaryUtility.h"

#include <map>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        // Every edit must already be in place. PatchLoader merges all of a
        // file's patches into one operation, so testing only the first edit
        // took a file patched by an earlier version as fully patched, and a
        // fix added to that file later was never written. Edits are compared
        // with the bytes the file ends up holding, later edits winning, since
        // one patch may rewrite a byte an earlier one set (L906LEV 0x22BF9).
        bool AlreadyApplied(const std::filesystem::path& targetPath, const PatchOperation& op)
        {
            std::map<uint64_t, uint8_t> expected;
            for (const BinaryEdit& edit : op.edits)
            {
                for (size_t i = 0; i < edit.bytes.size(); ++i) { expected[edit.offset + i] = edit.bytes[i]; }
            }
            for (const BinaryEdit& edit : op.edits)
            {
                std::vector<uint8_t> current;
                try
                {
                    current = BinaryUtility::ReadBytesAtOffset(targetPath, edit.offset, edit.bytes.size());
                }
                catch (const std::exception&)
                {
                    return false; // can't read -> treat as not-applied, let ApplyAll's own read surface the real error
                }
                for (size_t i = 0; i < current.size(); ++i)
                {
                    if (current[i] != expected[edit.offset + i]) { return false; }
                }
            }
            return true;
        }
    }

    std::vector<PatchResult> PatchRunner::ApplyAll(
        const std::filesystem::path& gameRoot,
        const std::vector<PatchOperation>& operations)
    {
        std::vector<PatchResult> results;
        results.reserve(operations.size());

        for (const auto& op : operations)
        {
            PatchResult result;
            result.operation = &op;

            std::filesystem::path targetPath = gameRoot / op.targetFile;
            targetPath = targetPath.lexically_normal().make_preferred();

            std::error_code ec;
            if (!std::filesystem::exists(targetPath, ec))
            {
                result.outcome = PatchOutcome::Failed;
                result.error = "target file not found: " + targetPath.string();
                results.push_back(std::move(result));
                continue;
            }

            if (AlreadyApplied(targetPath, op))
            {
                result.outcome = PatchOutcome::AlreadyApplied;
                results.push_back(std::move(result));
                continue;
            }

            try
            {
                BinaryUtility::ReplaceBytes(targetPath, op.edits);
                result.outcome = PatchOutcome::Applied;
            }
            catch (const std::exception& e)
            {
                result.outcome = PatchOutcome::Failed;
                result.error = e.what();
            }

            results.push_back(std::move(result));
        }

        return results;
    }
}
