#pragma once

#include <filesystem>
#include <string>

namespace ALTEngine::Audio
{
    // Menu sounds, kept as names because the menus run before a level (and so
    // before a slot table) exists. Both resolve to real bank slots now.
    enum class SfxId
    {
        MenuMove,    // 0101sele
        MenuSelect,  // 0102sele
        MenuBack,    // 0102sele
    };

    // Plays a game SFX - .RAW, 8-bit unsigned PCM, mono, 11025Hz, no header,
    // which is exactly the format AppWindow's SFX stream is opened with, so the
    // bytes go straight in with no conversion.
    class SfxPlayer
    {
    public:
        static void Play(SfxId id, const std::filesystem::path& cdDirectory);

        // Play by SLOT ID - the numbers the decompilation uses. `levelCode` is
        // the "111" form; the slot table is per level (see SfxBank.h).
        //
        // A slot the level does not use is silent, not an error: the original
        // pads its tables with blank.raw precisely so every id is always valid.
        static void PlaySlot(int slotId, const char* levelCode,
                             const std::filesystem::path& cdDirectory);

        // Samples are read once and kept. Call between levels to release them.
        static void ClearCache();

    private:
        static void PlayFile(const std::string& stem, const std::filesystem::path& cdDirectory);
    };
}
