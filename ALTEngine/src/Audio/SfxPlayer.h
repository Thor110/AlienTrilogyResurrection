#pragma once

#include <filesystem>
#include <vector>
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

        // The same, at a fraction of the SFX volume: a creature's sound, which
        // the original plays louder the nearer it is (FUN_00040c38).
        static void PlaySlotAtVolume(int slotId, float volume, const char* levelCode,
                                     const std::filesystem::path& cdDirectory);

        // FUN_00040c38's volume for a creature's sound (its mode 2): the
        // distance as max(|dx|, |dz|) + min / 2, held at 0x1400, taken off
        // 0x2fff, out of 0x3fff. So 3/4 volume close to, falling to 0.44.
        static float CreatureVolume(float dx, float dz) { return DistanceVolume(1, dx, dz); }

        // The weapons' animation cues (FUN_0003e93c) on the player's channel, as the
        // original's FUN_0005b21b plays them: each one cuts off the last, so a
        // held smartgun or pulse rifle is heard on every shot. Only the
        // flamethrower's loop (slot 0x0b) is left running if it already is.
        static void PlayPlayerSlot(int slotId, const char* levelCode,
                                   const std::filesystem::path& cdDirectory);

        // A held sound: a vent's jet (FUN_000358a4 starts it, the vent's task
        // stops it with FUN_00040aec). It keeps a voice of its own, and a
        // sample the manifests flag as looping (steam, flame, a lift running)
        // goes round until stopped; any other plays once. Returns a handle, or
        // -1 when nothing could start (no sample, or no voice to spare).
        static int StartLoop(int slotId, float volume, const char* levelCode,
                             const std::filesystem::path& cdDirectory);
        static void StopLoop(int handle);
        static void StopAllLoops();
        // Call once a frame: keeps the looping samples queued, and frees the
        // voices of held sounds that have finished.
        static void UpdateLoops();

        // FUN_00040c38's volume by its mode (bx), out of 0x3fff. The distance
        // is max(|dx|, |dz|) + min / 2, held at a cap and taken off a base:
        //   0, 2  base 0x1fff, cap 0x1e00   (a lift on its own, a vent's jet)
        //   1     base 0x2fff, cap 0x1400   (a creature)
        //   3     base 0x3fff, cap 0x1e00   (a lift the player set off)
        //   4     base 0x3fff, silent past 0x3c00
        static float DistanceVolume(int mode, float dx, float dz)
        {
            const float ax = dx < 0 ? -dx : dx;
            const float az = dz < 0 ? -dz : dz;
            float d = ax > az ? ax + az / 2.0f : az + ax / 2.0f;
            float base = 0x1fff;
            switch (mode)
            {
            case 1: base = 0x2fff; if (d > 0x1400) { d = 0x1400; } break;
            case 3: base = 0x3fff; if (d > 0x1e00) { d = 0x1e00; } break;
            case 4: base = 0x3fff; if (d > 0x3c00) { return 0.0f; } break;
            default: if (d > 0x1e00) { d = 0x1e00; } break;   // 0 and 2
            }
            return (base - d) / static_cast<float>(0x3fff);
        }
        static float VentVolume(float dx, float dz) { return DistanceVolume(2, dx, dz); }

        // Stops the player's channel if it is playing this slot's sample (the
        // original's FUN_0003cf68 on the light's held channel).
        static void StopPlayerSlot(int slotId, const char* levelCode,
                                   const std::filesystem::path& cdDirectory);

        // Samples are read once and kept. Call between levels to release them.
        static void ClearCache();

        // The Options SFX volume, 0..10. Applies to every voice at once.
        static void SetVolume(int volume0to10);

    private:
        // The sample for a stem, loaded on first use; empty if missing.
        static const std::vector<uint8_t>& Sample(const std::string& stem, const std::filesystem::path& cdDirectory);

        // `once`: not while the same sample is still playing (the game's rule).
        static void PlayFile(const std::string& stem, const std::filesystem::path& cdDirectory,
                             float volume = 1.0f, bool once = false);
    };
}
