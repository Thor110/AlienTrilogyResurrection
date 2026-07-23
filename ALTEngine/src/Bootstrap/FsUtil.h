#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

namespace ALTEngine::Bootstrap
{
    inline std::string ToLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Case-insensitive lookup of an entry's actual on-disk path directly
    // inside `directory`, or std::nullopt if not present. Windows'
    // filesystem is case-insensitive but std::filesystem::exists isn't -
    // this matters throughout since disc/game file naming conventions are
    // inconsistent about casing.
    inline std::optional<std::filesystem::path> FindEntryCaseInsensitive(const std::filesystem::path& directory, const std::string& name)
    {
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec))
        {
            return std::nullopt;
        }

        std::string target = ToLowerAscii(name);
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
        {
            if (ToLowerAscii(entry.path().filename().string()) == target)
            {
                return entry.path();
            }
        }
        return std::nullopt;
    }

    inline bool HasEntryCaseInsensitive(const std::filesystem::path& directory, const std::string& name)
    {
        return FindEntryCaseInsensitive(directory, name).has_value();
    }
}
