#pragma once

#include <filesystem>

namespace ALTEngine::Audio
{
    // Placeholder set - just what the menu needs right now (button
    // press/select/back). Extend as more UI/gameplay sounds are wired in.
    enum class SfxId
    {
        MenuMove,
        MenuSelect,
        MenuBack,
    };

    // Plays a game SFX (.RAW - 8-bit unsigned PCM, mono, 11025Hz, no WAV
    // header - see SoundEffects.cs) via the shared AppWindow SFX stream.
    //
    // The filename for each SfxId is a placeholder (empty) right now -
    // see SfxPlayer.cpp's table. Play() silently no-ops for any SfxId
    // with no filename set, or if the mapped file doesn't exist - fill in
    // real filenames there once determined; nothing else needs to change.
    class SfxPlayer
    {
    public:
        static void Play(SfxId id, const std::filesystem::path& cdDirectory);
    };
}
