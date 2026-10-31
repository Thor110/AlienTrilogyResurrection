#include "TestFramework.h"

#include "Screens/MissionAssessmentScreen.h"
#include "Bootstrap/ModernSettings.h"

#include <string>

using ALTEngine::Screens::MissionAssessmentScreen;

TEST_CASE("MissionAssessment: FUN_00014ec0's figure colours, red to yellow to green")
{
    const auto c = MissionAssessmentScreen::FigureColours(25);
    // Red until 10 below the pass mark.
    CHECK(c[0].r == 0x99); CHECK(c[0].g == 0); CHECK(c[0].b == 0);
    CHECK(c[15].r == 0x99); CHECK(c[15].g == 0);
    // Then green climbs 9 a step and snaps to 0x99 past 0x8f.
    CHECK(c[16].g == 9);
    CHECK(c[30].g == 0x87);
    CHECK(c[31].g == 0x99);
    CHECK(c[75].r == 0x99); CHECK(c[75].g == 0x99);   // yellow
    // Past 75 red falls 9 a step, gone once under 10.
    CHECK(c[76].r == 0x90);
    CHECK(c[90].r == 0x12);
    CHECK(c[91].r == 0);
    CHECK(c[100].r == 0); CHECK(c[100].g == 0x99);     // green
}

TEST_CASE("MissionAssessment: the pass mark moves where green starts")
{
    const auto lo = MissionAssessmentScreen::FigureColours(15);
    const auto hi = MissionAssessmentScreen::FigureColours(45);
    CHECK(lo[5].g == 0); CHECK(lo[6].g == 9);
    CHECK(hi[35].g == 0); CHECK(hi[36].g == 9);
}

TEST_CASE("MissionAssessment: the Level Timer's times read m:ss.cc, and h:mm:ss.cc from an hour")
{
    using ALTEngine::Screens::FormatRunTime;
    CHECK(FormatRunTime(0.0) == "0:00.00");
    CHECK(FormatRunTime(-3.0) == "0:00.00");
    CHECK(FormatRunTime(5.0) == "0:05.00");
    CHECK(FormatRunTime(83.456) == "1:23.45");     // rounded down
    CHECK(FormatRunTime(599.999) == "9:59.99");
    CHECK(FormatRunTime(3600.0) == "1:00:00.00");
    CHECK(FormatRunTime(3723.5) == "1:02:03.50");
}

TEST_CASE("Modern: the Level Timer is a feature of its own")
{
    using ALTEngine::Bootstrap::ModernFeature;
    CHECK(std::string(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ModernFeature::LevelTimer)) == "Level Timer");
    CHECK(std::string(ALTEngine::Bootstrap::ModernSettings::KeyFor(ModernFeature::LevelTimer)) == "ModernLevelTimer");
    bool listed = false;
    for (ModernFeature f : ALTEngine::Bootstrap::ModernSettings::AllFeatures) { listed = listed || f == ModernFeature::LevelTimer; }
    CHECK(listed);
}

#include "Audio/SfxPlayer.h"
#include "Formats/LightTable.h"

TEST_CASE("Sound: FUN_00040c38's distance volumes by mode")
{
    using ALTEngine::Audio::SfxPlayer;
    CHECK(SfxPlayer::DistanceVolume(2, 0, 0) == 0x1fff / float(0x3fff));          // a vent, close to
    CHECK(SfxPlayer::DistanceVolume(1, 0, 0) == 0x2fff / float(0x3fff));          // a creature
    CHECK(SfxPlayer::DistanceVolume(3, 0, 0) == 1.0f);                            // a lift the player set off
    CHECK(SfxPlayer::DistanceVolume(2, 0x8000, 0) == (0x1fff - 0x1e00) / float(0x3fff));   // held at the cap
    CHECK(SfxPlayer::DistanceVolume(1, 0x1000, 0x800) == (0x2fff - 0x1400) / float(0x3fff)); // 0x1000 + 0x400 = cap
    CHECK(SfxPlayer::DistanceVolume(4, 0x3c01, 0) == 0.0f);                        // silent far off
    CHECK(SfxPlayer::CreatureVolume(0, 0) == SfxPlayer::DistanceVolume(1, 0, 0));
}

TEST_CASE("Lights: the lamp reports full strength once, 32 ticks on (sound 0x18)")
{
    ALTEngine::Formats::LightTable table;
    table.Reset({});
    table.StartEquipment(ALTEngine::Formats::LightTable::Equipment::Lamp);
    int fullAt = -1;
    for (int t = 1; t <= 40; ++t)
    {
        table.Tick(0);
        if (table.TakeEquipmentFull()) { CHECK(fullAt < 0); fullAt = t; }
    }
    CHECK_EQ(fullAt, 32);
    CHECK(!table.TakeEquipmentFull());
}
