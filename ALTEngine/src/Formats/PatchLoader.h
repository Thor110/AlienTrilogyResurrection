#pragma once

#include <filesystem>
#include <vector>

#include "PatchSet.h"

namespace ALTEngine::Formats
{
    class PatchLoader
    {
    public:
        // Parses a Patches.json file (see data/Patches.json / the
        // extraction script's output format: hex-string offsets and
        // byte arrays). Throws on malformed JSON or an unrecognised
        // shape - a patch file failing to parse should stop the boot
        // sequence, not silently skip patches.
        static std::vector<PatchOperation> Load(const std::filesystem::path& jsonPath);
    };
}
