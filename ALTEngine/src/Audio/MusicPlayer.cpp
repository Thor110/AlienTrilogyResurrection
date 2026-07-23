#include "MusicPlayer.h"
#include "../Bootstrap/AppWindow.h"

#include <SDL3/SDL.h>
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

        FeedMore(); // start filling immediately rather than waiting for the next Update()
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
