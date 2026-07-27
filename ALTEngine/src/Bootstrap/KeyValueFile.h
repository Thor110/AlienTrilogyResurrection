#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace ALTEngine::Bootstrap
{
    // Parses a simple "key=value" text file, one entry per line, lines
    // without an "=" ignored. Same format Config already used for
    // altengine.cfg - extracted here so it's not duplicated for every
    // new file that wants this format (language packs, etc). Returns an
    // empty map if the file doesn't exist or can't be opened - callers
    // should treat that as "nothing here yet", not an error, since
    // that's the expected state for a language pack that hasn't been
    // installed.
    inline std::unordered_map<std::string, std::string> ParseKeyValueFile(const std::filesystem::path& path)
    {
        std::unordered_map<std::string, std::string> values;
        std::ifstream file(path);
        if (!file.is_open()) { return values; }

        std::string line;
        while (std::getline(file, line))
        {
            size_t separator = line.find('=');
            if (separator == std::string::npos) { continue; }
            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            values[key] = value;
        }
        return values;
    }

    // Writes a "key=value" file, one entry per line. Overwrites any
    // existing file at `path`.
    inline void WriteKeyValueFile(const std::filesystem::path& path, const std::unordered_map<std::string, std::string>& values)
    {
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) { return; }
        for (const auto& [key, value] : values)
        {
            file << key << "=" << value << "\n";
        }
    }
}
