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

    // A path as UTF-8 text, never throwing. path::string() converts to the
    // Windows code page and throws std::system_error on a name it cannot
    // represent - a folder with a character outside it is enough - so any
    // name shown, compared or logged goes through this instead.
    inline std::string PathText(const std::filesystem::path& p)
    {
        try
        {
            const auto u8 = p.u8string();
            return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
        }
        catch (...) { return std::string(); }
    }

    // The reverse: UTF-8 text (as PathText makes, and as the config keeps
    // it) back to a path.
    inline std::filesystem::path PathFromText(const std::string& text)
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    // Case-insensitive lookup of an entry's actual on-disk path directly
    // inside `directory`, or std::nullopt if not present. Windows'
    // filesystem is case-insensitive but std::filesystem::exists isn't -
    // this matters throughout since disc/game file naming conventions are
    // inconsistent about casing. Never throws: a folder that cannot be read,
    // or stops being readable part way, just has no such entry.
    inline std::optional<std::filesystem::path> FindEntryCaseInsensitive(const std::filesystem::path& directory, const std::string& name)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec)) { return std::nullopt; }

        const std::string target = ToLowerAscii(name);
        std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec);
        for (const std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
        {
            if (ToLowerAscii(PathText(it->path().filename())) == target) { return it->path(); }
        }
        return std::nullopt;
    }

    inline bool HasEntryCaseInsensitive(const std::filesystem::path& directory, const std::string& name)
    {
        return FindEntryCaseInsensitive(directory, name).has_value();
    }
}
