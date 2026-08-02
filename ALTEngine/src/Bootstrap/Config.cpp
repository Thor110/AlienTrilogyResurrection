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
        // Merge with whatever is currently on disk before writing. More
        // than one Config instance can be alive at once, and each holds
        // its own snapshot taken when it was constructed - without this,
        // a long-lived instance writing any single key would rewrite the
        // whole file from its stale map and silently drop keys another
        // instance had saved since.
        auto onDisk = ParseKeyValueFile(filePath);
        for (const auto& [k, v] : onDisk)
        {
            if (values.find(k) == values.end()) { values[k] = v; }
        }
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
