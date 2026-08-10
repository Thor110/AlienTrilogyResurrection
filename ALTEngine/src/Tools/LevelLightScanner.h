#pragma once

#include <filesystem>
#include <string>

namespace ALTEngine::Tools
{
    // ONE-SHOT SCANNER - meant to be run once and then deleted.
    //
    // Walks every level in data/LevelManifest.json (all 36 campaign levels plus
    // the 10 multiplayer ones) and writes a catalogue of every face that
    // carries a light or an animation, so the replacement system has a
    // authoritative list to work from instead of rediscovering it per level.
    //
    // TO REMOVE WHEN DONE: delete LevelLightScanner.h/.cpp, the one call in
    // main, and the src/Tools/LevelLightScanner.cpp line in CMakeLists.txt.
    // Nothing else references it.
    //
    // WHAT COUNTS AS INTERESTING. Every face has a light id, so listing them
    // all would just be the whole level. The scan keeps a face when either:
    //   - its draw routine is 8, i.e. it is an animated face whose texIndex is
    //     an animator ordinal, or
    //   - the light record it points at is not a plain static one (mode != 0),
    //     meaning it blinks (modes 1 and 3) or is gouraud shaded (mode 4).
    // Those are exactly the faces a replacement has to reproduce; ordinary
    // mode-0 faces need nothing but their light index, which the mesh carries
    // anyway.
    //
    // Faces are identified by their VERTEX INDICES, not a face number, for the
    // same reason the patch manifest is: face numbering depends on export order
    // and is not stable, the vertex set is.
    struct ScanResult
    {
        bool ok = false;
        int levelsScanned = 0;
        int levelsMissing = 0;
        int facesRecorded = 0;
        std::string message;
    };

    // `cdDirectory` is the CD root (the folder holding SECT11, SECT90 ...).
    // `manifestPath` is data/LevelManifest.json. Writes `outputPath`.
    ScanResult ScanAllLevelsForLights(const std::filesystem::path& cdDirectory,
                                      const std::filesystem::path& manifestPath,
                                      const std::filesystem::path& outputPath);
}
