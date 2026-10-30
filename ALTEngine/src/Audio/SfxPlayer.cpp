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

        // Which sample each voice was last given. The original won't start a
        // sample that is still playing on any channel (FUN_0005aed9 walks its
        // channels and returns if one holds the same sample and isn't done),
        // so a creature's cue repeating on every loop of its walk, or several
        // creatures cueing at once, never stack up into a drone.
        std::unordered_map<SDL_AudioStream*, const std::vector<uint8_t>*>& VoiceSamples()
        {
            static std::unordered_map<SDL_AudioStream*, const std::vector<uint8_t>*> playing;
            return playing;
        }

        const std::unordered_map<SfxId, std::string> kMenuSounds = {
            { SfxId::MenuMove,   "0101sele" },
            { SfxId::MenuSelect, "0102sele" },
            { SfxId::MenuBack,   "0102sele" },
        };
    }

    const std::vector<uint8_t>& SfxPlayer::Sample(const std::string& stem, const std::filesystem::path& cdDirectory)
    {
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
        return it->second;
    }

    void SfxPlayer::PlayFile(const std::string& stem, const std::filesystem::path& cdDirectory, float volume,
                              bool once)
    {
        if (stem.empty()) { return; }
        const std::vector<uint8_t>& sample = Sample(stem, cdDirectory);
        if (sample.empty()) { return; }

        // Each sound gets a voice of its own and the device mixes them, so
        // gunfire, footsteps and impacts overlap the way the original's
        // channel driver lets them (AppWindow::AcquireSfxVoice) - except that
        // a game sound never overlaps itself (see VoiceSamples). The menus'
        // ticks aren't the game's sound driver and repeat as fast as keys go.
        auto& app = ALTEngine::Bootstrap::AppWindow::Instance();
        auto& playing = VoiceSamples();
        for (const auto& [stream, held] : playing)
        {
            if (once && held == &sample && SDL_GetAudioStreamQueued(stream) > 0) { return; }
        }
        SDL_AudioStream* voice = app.AcquireSfxVoice();
        if (!voice) { return; }
        playing[voice] = &sample;
        // Voices are shared, so every sound sets its own gain.
        SDL_SetAudioStreamGain(voice, app.SfxGain() * volume);
        SDL_PutAudioStreamData(voice, sample.data(), static_cast<int>(sample.size()));
    }

    void SfxPlayer::Play(SfxId id, const std::filesystem::path& cdDirectory)
    {
        auto it = kMenuSounds.find(id);
        if (it == kMenuSounds.end()) { return; }
        PlayFile(it->second, cdDirectory, 1.0f, false);
    }

    void SfxPlayer::PlaySlot(int slotId, const char* levelCode, const std::filesystem::path& cdDirectory)
    {
        const char* name = SfxBank::SlotName(levelCode, slotId);
        if (!name || name[0] == '\0') { return; }

        // Looping samples (the flamethrower, steam, a running lift) are flagged
        // in the manifests and need to be held and retriggered rather than
        // fired once. Nothing drives a continuous sound yet, so they are played
        // as one-shots for now - noted so it is not mistaken for correct.
        PlayFile(name, cdDirectory, 1.0f, true);
    }

    void SfxPlayer::PlayPlayerSlot(int slotId, const char* levelCode, const std::filesystem::path& cdDirectory)
    {
        const char* name = SfxBank::SlotName(levelCode, slotId);
        if (!name || name[0] == '\0') { return; }
        const std::string stem = name;
        const std::vector<uint8_t>& sample = Sample(stem, cdDirectory);
        if (sample.empty()) { return; }

        auto& app = ALTEngine::Bootstrap::AppWindow::Instance();
        SDL_AudioStream* voice = app.PlayerSfxVoice();
        if (!voice) { return; }
        auto& playing = VoiceSamples();
        // FUN_0005b21b's one exception: sample 0xc (slot 0x0b, the
        // flamethrower's loop) already on the channel and not finished.
        const bool looping = SfxBank::Loops(name);
        if (looping && playing[voice] == &sample && SDL_GetAudioStreamQueued(voice) > 0) { return; }
        SDL_ClearAudioStream(voice);
        playing[voice] = &sample;
        SDL_SetAudioStreamGain(voice, app.SfxGain());
        SDL_PutAudioStreamData(voice, sample.data(), static_cast<int>(sample.size()));
    }

    void SfxPlayer::PlaySlotAtVolume(int slotId, float volume, const char* levelCode,
                                     const std::filesystem::path& cdDirectory)
    {
        const char* name = SfxBank::SlotName(levelCode, slotId);
        if (!name || name[0] == '\0') { return; }
        PlayFile(name, cdDirectory, volume, true);
    }

    float SfxPlayer::CreatureVolume(float dx, float dz)
    {
        const float ax = dx < 0 ? -dx : dx;
        const float az = dz < 0 ? -dz : dz;
        float d = ax > az ? ax + az / 2.0f : az + ax / 2.0f;
        if (d > 0x1400) { d = 0x1400; }
        return (static_cast<float>(0x2fff) - d) / static_cast<float>(0x3fff);
    }

    void SfxPlayer::SetVolume(int volume0to10)
    {
        ALTEngine::Bootstrap::AppWindow::Instance().SetSfxGain(static_cast<float>(volume0to10) / 10.0f);
    }

    void SfxPlayer::ClearCache()
    {
        VoiceSamples().clear();
        Cache().clear();
    }
}
