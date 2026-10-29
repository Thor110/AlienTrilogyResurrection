#include "TestFramework.h"

#include "Screens/MissionAssessmentScreen.h"

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
