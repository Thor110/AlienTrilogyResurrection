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

        // How much audio may sit unplayed before new sounds are dropped.
        //
        // The SFX stream is a single queue, so anything pushed while something
        // is already playing lands AFTER it rather than mixing with it. Without
        // a cap, a burst of footsteps or a full-auto weapon would queue seconds
        // of audio and drift further behind the action with every shot. At
        // 11025 bytes a second this is about a quarter of a second of slack -
        // enough for sounds to overlap slightly, little enough that they stay
        // in sync with what is on screen.
        //
        // Mixing properly needs a voice mixer, which the original has (its
        // driver allocates channels) and this does not yet.
        constexpr int MAX_QUEUED_BYTES = 2756;

        const std::unordered_map<SfxId, std::string> kMenuSounds = {
            { SfxId::MenuMove,   "0101sele" },
            { SfxId::MenuSelect, "0102sele" },
            { SfxId::MenuBack,   "0102sele" },
        };
    }

    void SfxPlayer::PlayFile(const std::string& stem, const std::filesystem::path& cdDirectory)
    {
        if (stem.empty()) { return; }

        SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().SfxAudioStream();
        if (!stream) { return; }

        // Drop rather than queue behind a backlog - see MAX_QUEUED_BYTES.
        if (SDL_GetAudioStreamQueued(stream) > MAX_QUEUED_BYTES) { return; }

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
        SDL_PutAudioStreamData(stream, it->second.data(), static_cast<int>(it->second.size()));
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

    void SfxPlayer::ClearCache()
    {
        Cache().clear();
    }
}
