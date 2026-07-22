#include "PatchRunner.h"
#include "BinaryUtility.h"

#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        bool AlreadyApplied(const std::filesystem::path& targetPath, const PatchOperation& op)
        {
            const BinaryEdit& first = op.edits.front();
            std::vector<uint8_t> current;
            try
            {
                current = BinaryUtility::ReadBytesAtOffset(targetPath, first.offset, first.bytes.size());
            }
            catch (const std::exception&)
            {
                return false; // can't read -> treat as not-applied, let ApplyAll's own read surface the real error
            }
            return current == first.bytes;
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
