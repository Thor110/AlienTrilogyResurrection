#include "OverrideVideo.h"
#include "../Bootstrap/FsUtil.h"

namespace ALTEngine::Video
{
    std::optional<std::filesystem::path> FindOverrideVideo(const std::filesystem::path& cdDirectory, const std::string& baseName)
    {
        std::filesystem::path overrideAviDir = cdDirectory / "Override" / "AVI";

        for (const char* ext : { ".mp4", ".avi", ".mkv", ".webm" })
        {
            if (auto found = ALTEngine::Bootstrap::FindEntryCaseInsensitive(overrideAviDir, baseName + ext))
            {
                return found;
            }
        }
        return std::nullopt;
    }
}
