#include "BootSequence.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace ALTEngine::Formats::BootSequence
{
    std::vector<std::string> DefaultVideos()
    {
        return { "FOXDKAUD", "ALOGODUK", "PRBLOGO", "INTRO" };
    }

    std::vector<std::string> VideosFromJson(const std::string& jsonText)
    {
        const nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
        if (root.is_discarded() || !root.contains("steps") || !root["steps"].is_array()) { return {}; }

        std::vector<std::pair<int, std::string>> videos;
        for (const auto& step : root["steps"])
        {
            if (!step.is_object() || step.value("type", std::string()) != "video") { continue; }
            const std::string path = step.value("path", std::string());
            if (path.empty()) { continue; }
            const std::string stem = std::filesystem::path(path).stem().string();
            if (!stem.empty()) { videos.emplace_back(step.value("order", 0), stem); }
        }
        std::stable_sort(videos.begin(), videos.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<std::string> names;
        names.reserve(videos.size());
        for (auto& [order, name] : videos) { names.push_back(std::move(name)); }
        return names;
    }

    std::vector<std::string> LoadVideos(const std::filesystem::path& jsonPath)
    {
        std::ifstream in(jsonPath, std::ios::binary);
        if (!in) { return DefaultVideos(); }
        const std::string text{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
        std::vector<std::string> names = VideosFromJson(text);
        return names.empty() ? DefaultVideos() : names;
    }
}
