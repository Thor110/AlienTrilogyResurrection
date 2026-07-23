#include "SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ALTEngine::Audio
{
    namespace
    {
        // TODO(Edward): fill in real filenames (no extension - .RAW is
        // appended) once determined. Empty string = no sound assigned
        // yet, Play() no-ops for it.
        const std::unordered_map<SfxId, std::string> kSfxFilenames = {
            { SfxId::MenuMove,   "" },
            { SfxId::MenuSelect, "" },
            { SfxId::MenuBack,   "" },
        };
    }

    void SfxPlayer::Play(SfxId id, const std::filesystem::path& cdDirectory)
    {
        auto it = kSfxFilenames.find(id);
        if (it == kSfxFilenames.end() || it->second.empty())
        {
            return; // no sound assigned to this SfxId yet
        }

        std::filesystem::path path = cdDirectory / "SFX" / (it->second + ".RAW");
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            SDL_Log("SfxPlayer: %s not found, skipping", path.string().c_str());
            return;
        }

        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in.is_open()) { return; }
        auto size = static_cast<size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!in) { return; }

        SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().SfxAudioStream();
        if (!stream) { return; }

        SDL_PutAudioStreamData(stream, data.data(), static_cast<int>(data.size()));
    }
}
