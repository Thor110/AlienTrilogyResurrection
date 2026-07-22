#include "Config.h"
#include "PlatformPaths.h"

#include <fstream>
#include <sstream>
#include <string>

namespace ALTEngine::Bootstrap
{

    Config::Config()
        : filePath(ExecutableDirectory() / "altengine.cfg")
    {
        Load();
    }

    std::optional<std::string> Config::Get(const std::string& key) const
    {
        auto it = values.find(key);
        if (it == values.end()) { return std::nullopt; }
        return it->second;
    }

    void Config::Set(const std::string& key, const std::string& value)
    {
        values[key] = value;
        Save();
    }

    void Config::Load()
    {
        std::ifstream file(filePath);
        if (!file.is_open()) { return; }

        std::string line;
        while (std::getline(file, line))
        {
            size_t separator = line.find('=');
            if (separator == std::string::npos) { continue; }
            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            values[key] = value;
        }
    }

    void Config::Save() const
    {
        std::ofstream file(filePath, std::ios::trunc);
        if (!file.is_open()) { return; }

        for (const auto& [key, value] : values)
        {
            file << key << "=" << value << "\n";
        }
    }
}
