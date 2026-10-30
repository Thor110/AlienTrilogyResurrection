#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // data/BootSequence.json: the fixed startup sequence. Only its video steps
    // are read here - the base names of the AVIs to play, in the steps'
    // order (AVI/FOXDKAUD.AVI -> "FOXDKAUD"), which main.cpp resolves for the
    // language and plays. The splash images are shown by their own code.
    namespace BootSequence
    {
        // The intro videos, as the original plays them. Used when the file is
        // missing or holds no video steps.
        std::vector<std::string> DefaultVideos();

        // The video base names from the JSON text, in step order; empty if it
        // does not parse or has none.
        std::vector<std::string> VideosFromJson(const std::string& jsonText);

        // The same, read from a file; DefaultVideos() if it cannot be read or
        // has none.
        std::vector<std::string> LoadVideos(const std::filesystem::path& jsonPath);
    }
}
