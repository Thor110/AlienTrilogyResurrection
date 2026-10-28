#include "SfxPlayer.h"
#include "SfxBank.h"
#include "../Bootstrap/AppWindow.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace ALTEngine::Audio
{
    namespace
    {
        // Sample cache, keyed by stem. Loaded on first use and kept, so a
        // footstep costs one file read for the whole level rather than one per
        // step. An empty vector marks a sample that failed to load, so a
        // missing file is not retried every time it is asked for.
        std::unordered_map<std::string, std::vector<uint8_t>>& Cache()
        {
            static std::unordered_map<std::string, std::vector<uint8_t>> cache;
            return cache;
        }

        const std::unordered_map<SfxId, std::string> kMenuSounds = {
            { SfxId::MenuMove,   "0101sele" },
            { SfxId::MenuSelect, "0102sele" },
            { SfxId::MenuBack,   "0102sele" },
        };
    }

    void SfxPlayer::PlayFile(const std::string& stem, const std::filesystem::path& cdDirectory)
    {
        if (stem.empty()) { return; }

        auto& cache = Cache();
        auto it = cache.find(stem);
        if (it == cache.end())
        {
            std::vector<uint8_t> data;
            std::filesystem::path path = cdDirectory / "SFX" / (stem + ".RAW");

            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (in.is_open())
            {
                auto size = static_cast<size_t>(in.tellg());
                in.seekg(0, std::ios::beg);
                data.resize(size);
                if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
                {
                    data.clear();
                }
            }
            else
            {
                SDL_Log("SfxPlayer: %s not found", path.string().c_str());
            }

            it = cache.emplace(stem, std::move(data)).first;
        }

        if (it->second.empty()) { return; }

        // Each sound gets a voice of its own and the device mixes them, so
        // gunfire, footsteps and impacts overlap the way the original's
        // channel driver lets them (AppWindow::AcquireSfxVoice).
        SDL_AudioStream* voice = ALTEngine::Bootstrap::AppWindow::Instance().AcquireSfxVoice();
        if (!voice) { return; }
        SDL_PutAudioStreamData(voice, it->second.data(), static_cast<int>(it->second.size()));
    }

    void SfxPlayer::Play(SfxId id, const std::filesystem::path& cdDirectory)
    {
        auto it = kMenuSounds.find(id);
        if (it == kMenuSounds.end()) { return; }
        PlayFile(it->second, cdDirectory);
    }

    void SfxPlayer::PlaySlot(int slotId, const char* levelCode, const std::filesystem::path& cdDirectory)
    {
        const char* name = SfxBank::SlotName(levelCode, slotId);
        if (!name || name[0] == '\0') { return; }

        // Looping samples (the flamethrower, steam, a running lift) are flagged
        // in the manifests and need to be held and retriggered rather than
        // fired once. Nothing drives a continuous sound yet, so they are played
        // as one-shots for now - noted so it is not mistaken for correct.
        PlayFile(name, cdDirectory);
    }

    void SfxPlayer::SetVolume(int volume0to10)
    {
        ALTEngine::Bootstrap::AppWindow::Instance().SetSfxGain(static_cast<float>(volume0to10) / 10.0f);
    }

    void SfxPlayer::ClearCache()
    {
        Cache().clear();
    }
}
