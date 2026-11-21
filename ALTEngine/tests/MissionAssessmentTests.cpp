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

TEST_CASE("Sound: the original's volume curve (0x5b032), full at 0x63")
{
    using ALTEngine::Audio::SfxPlayer;
    CHECK(SfxPlayer::Gain(SfxPlayer::FIXED_LEVEL, 10) == 1.0f);
    CHECK(SfxPlayer::Gain(SfxPlayer::FIXED_LEVEL, 6) == 1.0f);        // fixed paths are full from slider 6
    CHECK(SfxPlayer::Gain(SfxPlayer::FIXED_LEVEL, 5) == 97 / 99.0f); // (0x7d0 * 50) >> 10
    CHECK(SfxPlayer::Gain(0, 10) == 0.0f);
    CHECK(SfxPlayer::Gain(0x1fb0, 10) == 99 / 99.0f);
    CHECK(SfxPlayer::Gain(0x1bff, 10) == 87 / 99.0f);                // a creature at its distance cap
    CHECK(SfxPlayer::Gain(0x3fff, 0) == 0.0f);
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

#include "Formats/ScreenFade.h"

TEST_CASE("Screen fades: FUN_0004eec0 / FUN_0004eefc / FUN_0004f020")
{
    namespace SF = ALTEngine::Formats::ScreenFade;
    SF::State s;
    CHECK(SF::Identity(s));

    // In: 340, 255, 170, 85 off, then nothing; five frames.
    SF::StartIn(s);
    int r = 0x80, g = 0x80, b = 0x80;
    SF::Apply(s, r, g, b);
    CHECK_EQ(r, 0);
    SF::Step(s); SF::Step(s); SF::Step(s);   // step 4: 85 off
    r = 0x80; g = 0xff; b = 0x10;
    SF::Apply(s, r, g, b);
    CHECK_EQ(r, 0x80 - 0x55);
    CHECK_EQ(g, 0xff - 0x55);
    CHECK_EQ(b, 0);
    SF::Step(s);                              // step 5: whole
    CHECK(SF::Identity(s));
    CHECK(SF::Busy(s));
    SF::Step(s);
    CHECK(!SF::Busy(s));

    // Out: 11 a step, or 44 for a colour with a channel over 0x7e.
    SF::StartOut(s);
    CHECK(SF::Identity(s));
    SF::Step(s); SF::Step(s);                 // step 23: 2 steps in
    r = 0x7e; g = 0x40; b = 0x00;
    SF::Apply(s, r, g, b);
    CHECK_EQ(r, 0x7e - 22);
    CHECK_EQ(g, 0x40 - 22);
    r = 0x80; g = 0x40; b = 0x00;
    SF::Apply(s, r, g, b);
    CHECK_EQ(r, 0x80 - 88);
    CHECK_EQ(g, 0);
    CHECK(SF::NeutralFactor(s) == (0x80 - 88) / 128.0f);
    for (int i = 0; i < 23; ++i) { SF::Step(s); }
    CHECK(!SF::Busy(s));
    CHECK(s.heldBlack);                       // black until the next fade in
    r = g = b = 0xff;
    SF::Apply(s, r, g, b);
    CHECK_EQ(r + g + b, 0);
    SF::StartIn(s);
    CHECK(!s.heldBlack);
}

#include "Screens/MultiplayerResultsScreen.h"

TEST_CASE("Multiplayer results: FUN_00015228's table, highest score first")
{
    using ALTEngine::Screens::MultiplayerResultsPlayer;
    namespace MR = ALTEngine::Screens::MultiplayerResults;
    std::vector<MultiplayerResultsPlayer> players(2);
    players[0].name = "AL"; players[0].number = 1; players[0].score = 1; players[0].kills = { 1, 2 };
    players[1].name = "RIPLEY"; players[1].number = 2; players[1].score = 3; players[1].kills = { 3, 0 };
    const auto cells = MR::Layout(players);
    // n = 2: left = 2 * 0x1b + 8 = 0x3e, top = 2 * 10 + 0x40 = 0x54.
    CHECK(cells[0].text == std::string("RIPLEY"));        // the higher score's column first
    CHECK_EQ(cells[0].x, 0x3e + 0x3c + 1 * 5);              // 6 letters: one space in
    CHECK_EQ(cells[0].y, 0x54);
    CHECK_EQ(cells[1].x, 0x3e + 0x3c + 0x37 + 3 * 5);       // "AL": three in
    CHECK(cells[2].text == std::string("="));
    CHECK_EQ(cells[2].x, 0x3e + 2 * 0x37 + 0x4b);
    // RIPLEY's row: the name right-set, 3 against AL, own deaths 0, score 3.
    CHECK_EQ(cells[3].x, 0x3e + (8 - 6) * 5);
    CHECK_EQ(cells[3].y, 0x54 + 0x14);
    CHECK(cells[4].text == std::string("   0"));
    CHECK(cells[5].text == std::string("   3"));
    CHECK(cells[6].text == std::string("   3"));
    // AL's row: killed itself once - shown as -1 in its own column.
    CHECK(cells[9].text == std::string("  -1"));
}
