#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace ALTEngine::Audio
{
    struct FeedChunk
    {
        size_t sourceOffset;
        size_t length;
    };

    // Pure decision logic (no SDL dependency) used internally by
    // MusicPlayer's looping - exposed here specifically so it's directly
    // unit-testable without needing a real (and, for device-bound
    // streams, racy to inspect) SDL audio stream. See MusicPlayer.cpp.
    std::optional<FeedChunk> ComputeNextFeedChunk(
        size_t bufferLen, size_t& feedPosition, int queuedBytes, int targetQueueBytes, size_t chunkSizeHint);

    // Looped WAV playback - intended for the ripped CDDA music tracks
    // (CD/MUSIC/trackNN.wav, real WAV files with proper headers, unlike
    // the headerless SFX .RAW files - see SfxPlayer).
    //
    // There's no callback-based looping here, just periodic re-queueing
    // of the next chunk as the audio device consumes what's already
    // queued - Update() must be called once per frame from whatever
    // owns the render loop (MenuController right now) for looping to
    // actually happen.
    class MusicPlayer
    {
    public:
        // Loads `path` and starts looping playback, replacing whatever
        // was playing before. Silently no-ops (logs) if the file can't
        // be loaded - missing music shouldn't block the menu.
        static void PlayLooped(const std::filesystem::path& path);

        // Stops whatever's currently playing and releases the loaded
        // buffer. Safe to call even if nothing is playing.
        static void Stop();

        // Sets playback volume, 0-10 (matching MenuNode::sliderValue's
        // own scale) - takes effect immediately on whatever's currently
        // playing, and persists for whatever plays next via
        // PlayLooped. Clamped to 0-10 (Edward, 2026 - functional volume
        // sliders).
        static void SetVolume(int volume0to10);

        // Must be called once per frame - see class comment.
        static void Update();
    };
}
