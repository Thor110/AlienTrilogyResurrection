// ORIGINAL MUSIC SELECTION - decoded from the executable.
//
// Every piece of music in the original resolves through ONE number, which is
// why menu and level music are the same mechanism rather than two:
//
//   FUN_0004263c(id)      the only entry point. Clamps id to <= 0x2c, looks the
//                         track up in the table at DAT_000acefa and hands it to
//                         FUN_0004258c.
//   FUN_0004258c(track)   plays it. Returns immediately if -nocd was given
//                         (DAT_000b0cee). track == -1 means stop
//                         (FUN_00057d70); otherwise FUN_000581a8 starts it and
//                         the current track is kept in DAT_000acf54+2.
//   FUN_000425fc()        stop: writes 0xffff over the current-track slot.
//
// The single call site passes DAT_000b0ca8 - the CURRENT LEVEL ID, the same
// variable behind the episode texture sets and the 0x16-0x22 draw branch.
//
// THE LOOKUP IS OFFSET BY ONE WORD. The original reads a 32-BIT int at
// base + id*2 and shifts it right by 0x10, which takes the HIGH half - so the
// track for id N is the word at index N+1, not N. That is why the table's first
// word is 0 and never used. Getting this wrong would shift every level's music
// by one and still look plausible.
//
// The 45 words at 0x000acefa decode to tracks 2..17 - sixteen audio tracks,
// with track 1 being the data track. That is exactly how a mixed-mode disc is
// laid out, which is the strongest confirmation the offset reading is right.
//

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

namespace ALTEngine::Audio
{
    // CD audio track for a level id, transcribed from the table at
    // 0x000acefa. Returns -1 for "stop"/unknown, matching the original's own
    // sentinel.
    //
    // Level 44 is reachable through the original's clamp but reads one word
    // PAST the table, straight into DAT_000acf54 (the current-track slot) - an
    // off-by-one in the original that no real level appears to hit. Reported as
    // -1 here rather than reproducing a garbage read.
    inline int LevelMusicTrack(int levelId)
    {
        // Index N holds the track for level N, already un-shifted from the
        // original's high-word read.
        static constexpr int TRACKS[44] = {
             6, 10,  2, 13,  9,  2,  8,  2,  7,  2,
            11, 12,  6, 13, 10,  7,  8, 13, 11,  8,
             4, 12, 10,  5,  3,  5,  9,  4,  5,  8,
             3,  5, 11,  5, 12, 14, 15, 16, 17, 14,
            15, 16, 17, 14
        };
        if (levelId < 0 || levelId >= 44) { return -1; }
        return TRACKS[levelId];
    }

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
