#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace ALTEngine::Bootstrap
{
    // Minimal persistent key/value store, backed by a flat text file
    // ("altengine.cfg") next to the executable. Deliberately simple -
    // this is just for remembering the located game directory between
    // runs, not a general settings system.
    class Config
    {
    public:
        Config();

        // Reads a value for `key`, or std::nullopt if not present.
        std::optional<std::string> Get(const std::string& key) const;

        // Writes/overwrites a value for `key` and saves immediately.
        void Set(const std::string& key, const std::string& value);

        // Full path to the config file on disk.
        std::filesystem::path FilePath() const { return filePath; }

    private:
        void Load();
        void Save() const;

        std::filesystem::path filePath;
        std::unordered_map<std::string, std::string> values;
    };
}
