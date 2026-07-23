#include "OverrideImage.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG   // .obj (models) get their own loader later - this file only ever needs PNG
#include "../../third_party/stb/stb_image.h"

#include <cstdio>

namespace ALTEngine::Formats
{
    std::optional<OverrideImage> TryLoadOverrideImage(const std::filesystem::path& overrideRoot, const std::string& key)
    {
        std::filesystem::path path = overrideRoot / (key + ".png");

        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            return std::nullopt; // no override present - expected, not an error
        }

        int width = 0, height = 0, channels = 0;
        stbi_uc* decoded = stbi_load(path.string().c_str(), &width, &height, &channels, 4); // force RGBA
        if (!decoded)
        {
            std::fprintf(stderr, "TryLoadOverrideImage: found %s but failed to decode: %s\n",
                         path.string().c_str(), stbi_failure_reason());
            return std::nullopt;
        }

        OverrideImage image;
        image.width = width;
        image.height = height;
        image.rgba.assign(decoded, decoded + (static_cast<size_t>(width) * height * 4));
        stbi_image_free(decoded);

        return image;
    }
}
