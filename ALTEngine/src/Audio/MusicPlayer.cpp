#include "MusicPlayer.h"
#include "../Bootstrap/AppWindow.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <optional>

namespace ALTEngine::Audio
{
    std::optional<FeedChunk> ComputeNextFeedChunk(
        size_t bufferLen, size_t& feedPosition, int queuedBytes, int targetQueueBytes, size_t chunkSizeHint)
    {
        if (bufferLen == 0 || queuedBytes >= targetQueueBytes) { return std::nullopt; }

        size_t remaining = bufferLen - feedPosition;
        if (remaining == 0)
        {
            feedPosition = 0;
            remaining = bufferLen;
        }

        size_t chunk = remaining < chunkSizeHint ? remaining : chunkSizeHint;
        size_t offset = feedPosition;
        feedPosition += chunk;
        return FeedChunk{ offset, chunk };
    }

    namespace
    {
        Uint8* buffer = nullptr;
        Uint32 bufferLen = 0;
        size_t feedPosition = 0; // how far into `buffer` we've queued so far
        SDL_AudioSpec spec{};
        bool active = false;
        int volume = 8; // 0-10, matches MenuNode::sliderValue's own default/scale

        void FeedMore()
        {
            if (!active || !buffer) { return; }

            SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().MusicAudioStream(spec);
            if (!stream) { return; }

            int bytesPerSecond = spec.freq * spec.channels * SDL_AUDIO_BYTESIZE(spec.format);
            int targetQueueBytes = bytesPerSecond * 2; // keep ~2 seconds buffered ahead

            while (true)
            {
                auto chunk = ComputeNextFeedChunk(bufferLen, feedPosition, SDL_GetAudioStreamQueued(stream),
                                                   targetQueueBytes, static_cast<size_t>(bytesPerSecond));
                if (!chunk.has_value()) { break; }
                SDL_PutAudioStreamData(stream, buffer + chunk->sourceOffset, static_cast<int>(chunk->length));
            }
        }
    }

    void MusicPlayer::PlayLooped(const std::filesystem::path& path)
    {
        Stop();

        SDL_AudioSpec loadedSpec{};
        Uint8* loadedBuffer = nullptr;
        Uint32 loadedLen = 0;
        if (!SDL_LoadWAV(path.string().c_str(), &loadedSpec, &loadedBuffer, &loadedLen))
        {
            SDL_Log("MusicPlayer::PlayLooped: could not load %s: %s", path.string().c_str(), SDL_GetError());
            return;
        }

        spec = loadedSpec;
        buffer = loadedBuffer;
        bufferLen = loadedLen;
        feedPosition = 0;
        active = true;

        // PlayLooped may (re)open a fresh audio stream if the format
        // differs from whatever was playing before (see
        // AppWindow::MusicAudioStream) - a freshly opened stream starts
        // at SDL's own default gain (1.0), so the persisted volume needs
        // reapplying here too, not just in SetVolume.
        if (SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().MusicAudioStream(spec))
        {
            SDL_SetAudioStreamGain(stream, static_cast<float>(volume) / 10.0f);
        }

        FeedMore(); // start filling immediately rather than waiting for the next Update()
    }

    void MusicPlayer::SetVolume(int volume0to10)
    {
        volume = std::clamp(volume0to10, 0, 10);
        if (!active) { return; }

        SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().MusicAudioStream(spec);
        if (stream) { SDL_SetAudioStreamGain(stream, static_cast<float>(volume) / 10.0f); }
    }

    void MusicPlayer::Stop()
    {
        bool wasActive = active;
        active = false;
        if (buffer) { SDL_free(buffer); buffer = nullptr; }
        bufferLen = 0;
        feedPosition = 0;

        if (wasActive)
        {
            SDL_AudioStream* stream = ALTEngine::Bootstrap::AppWindow::Instance().MusicAudioStream(spec);
            if (stream) { SDL_ClearAudioStream(stream); }
        }
    }

    void MusicPlayer::Update()
    {
        FeedMore();
    }
}
