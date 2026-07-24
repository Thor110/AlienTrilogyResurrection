#pragma once

#include <filesystem>

namespace ALTEngine::Screens
{
    struct MissionAssessmentStats
    {
        float aliensPercent = 0.0f;
        float secretsPercent = 0.0f;
        float missionPercent = 0.0f;
    };

    struct MissionAssessmentResult
    {
        bool windowClosed = false;
    };

    // End-of-level "MISSION ASSESSMENT" screen - three stats (Aliens /
    // Secrets / Mission), each animating in sequence (Aliens counts up
    // first, then Secrets, then Mission - confirmed by comparing the two
    // reference screenshots: "end-level-counting.png" shows Aliens
    // already at its final value while Secrets is still climbing and
    // Mission hasn't started, "end-level-counted.png" shows all three
    // settled). A keypress mid-animation skips straight to final values,
    // same convention as MissionBriefingScreen's typewriter.
    //
    // No real gameplay stat tracking exists yet, so the caller supplies
    // MissionAssessmentStats directly (there's nowhere to compute real
    // percentages from) - "throw together quickly using the current
    // systems" per Edward, 2026.
    class MissionAssessmentScreen
    {
    public:
        static MissionAssessmentResult Run(
            const std::filesystem::path& cdDirectory,
            const MissionAssessmentStats& stats);
    };
}
