#include "Config.h"
#include "KeyValueFile.h"
#include "PlatformPaths.h"

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
        values = ParseKeyValueFile(filePath);
    }

    void Config::Save() const
    {
        WriteKeyValueFile(filePath, values);
    }
}
