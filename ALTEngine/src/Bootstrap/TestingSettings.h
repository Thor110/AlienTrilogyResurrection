#pragma once

#include "Config.h"

namespace ALTEngine::Bootstrap
{
    // TESTING options, under Options > Modern > Testing. Aids for going over
    // the levels face by face, not part of the game: each is off by default
    // and none is touched by Modern's Enable All, its presets or its Custom
    // mode, so turning the modern features on or off never turns these on.
    //
    // Disable Enemy Spawn - the level starts with no creatures at all: none
    //   placed, none for the triggers, eggs, crates or vents to let out.
    // Face Index Display  - outlines the level face under the crosshair and
    //   prints its number, texture, draw routine and file offset, so a face
    //   with a wrong texture can be found and patched by its number.
    enum class TestingOption
    {
        DisableEnemySpawn,
        FaceIndexDisplay,
    };

    class TestingSettings
    {
    public:
        explicit TestingSettings(Config& config) : config(config) {}

        static constexpr TestingOption AllOptions[] = {
            TestingOption::DisableEnemySpawn,
            TestingOption::FaceIndexDisplay,
        };

        bool IsOn(TestingOption option) const
        {
            auto value = config.Get(KeyFor(option));
            return value.has_value() && *value == "On";
        }

        void Set(TestingOption option, bool enabled) { config.Set(KeyFor(option), enabled ? "On" : "Off"); }

        // The menu row's internal label - what MenuController matches on.
        static const char* MenuLabel(TestingOption option)
        {
            switch (option)
            {
            case TestingOption::DisableEnemySpawn: return "Disable Enemy Spawn";
            case TestingOption::FaceIndexDisplay: return "Face Index Display";
            }
            return "";
        }

        static bool ForMenuLabel(const std::string& label, TestingOption& out)
        {
            for (TestingOption option : AllOptions)
            {
                if (label == MenuLabel(option)) { out = option; return true; }
            }
            return false;
        }

    private:
        static const char* KeyFor(TestingOption option)
        {
            switch (option)
            {
            case TestingOption::DisableEnemySpawn: return "TestingDisableEnemySpawn";
            case TestingOption::FaceIndexDisplay: return "TestingFaceIndexDisplay";
            }
            return "";
        }

        Config& config;
    };
}
