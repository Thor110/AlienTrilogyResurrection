#include "TestFramework.h"

#include "Game/ResupplyTimer.h"

#include <vector>

namespace RT = ALTEngine::Game::ResupplyTimer;

TEST_CASE("ResupplyTimer: only the bonus levels have one, with FUN_0003b518's limits")
{
    CHECK(!RT::Start(0).active);
    CHECK(!RT::Start(8).active);
    CHECK(RT::Start(2).seconds == 60);    // 113
    CHECK(RT::Start(9).seconds == 30);    // 155
    CHECK(RT::Start(16).seconds == 60);   // 242
    CHECK(RT::Start(27).seconds == 45);   // 352
    CHECK(RT::Start(30).seconds == 45);   // 353
    CHECK(RT::Text(RT::Start(2)) == "60");
}

TEST_CASE("ResupplyTimer: a second is 0x46 interrupts, and zero ends the level")
{
    RT::State s = RT::Start(9);
    for (int i = 0; i < 0x46 / 2 - 1; ++i) { RT::Pass(s, 2); }   // 68 interrupts
    CHECK(s.seconds == 30);
    RT::Pass(s, 2);                                              // 70
    CHECK(s.seconds == 29);
    CHECK(!s.expired);
    for (int i = 0; i < 29 * 35; ++i) { RT::Pass(s, 2); }
    CHECK(s.seconds == 0);
    CHECK(s.expired);
    CHECK(RT::Text(s) == "00");
}

TEST_CASE("ResupplyTimer: the cues on the seconds the task sounds them")
{
    CHECK(RT::CueFor(20, 9) == 0x64);    // 155: emergency
    CHECK(RT::CueFor(17, 2) == 0x65);    // 113: attention
    CHECK(RT::CueFor(20, 27) == 0x62);   // 352: klaxon
    CHECK(RT::CueFor(14, 2) == 0x63);    // all personnel
    CHECK(RT::CueFor(14, 27) == 0x62);
    CHECK(RT::CueFor(11, 2) == -1);
    CHECK(RT::CueFor(11, 30) == 0x62);
    CHECK(RT::CueFor(19, 2) == -1);

    RT::State s = RT::Start(2);
    std::vector<int> cues;
    for (int i = 0; i < 60 * 70; ++i) { const int c = RT::Pass(s, 1); if (c >= 0) { cues.push_back(c); } }
    CHECK(cues == std::vector<int>({ 0x65, 0x65, 0x63 }));
}

TEST_CASE("ResupplyTimer: blinks from 17 seconds down")
{
    RT::State s = RT::Start(2);
    CHECK(!RT::Blinking(s));
    s.seconds = 17;
    CHECK(RT::Blinking(s));
}
