#include "TestFramework.h"

#include "Audio/Speech.h"
#include "Screens/HudMessages.h"
#include "Screens/PickupSystem.h"

#include <filesystem>
#include <fstream>
#include <string>

using ALTEngine::Screens::HudMessages;
namespace SP = ALTEngine::Audio::Speech;

TEST_CASE("HUD messages: one at a time, a character a pass, held 0x28 passes once typed")
{
    HudMessages m;
    m.Show(std::string("ABC"));
    CHECK(m.Visible().empty());
    m.Tick();
    REQUIRE(m.Visible().size() == 1);
    CHECK(m.Visible()[0] == "A");
    m.Tick();
    m.Tick();
    CHECK(m.Visible()[0] == "ABC");
    // 3 characters + 0x28: gone on the pass that reaches it.
    for (int i = 3; i < 3 + HudMessages::HOLD_TICKS - 1; ++i) { m.Tick(); }
    CHECK(!m.Visible().empty());
    m.Tick();
    CHECK(m.Visible().empty());
    CHECK(m.Empty());
}

TEST_CASE("HUD messages: the newest goes in the highest free slot and the lowest shows next; a fifth is dropped")
{
    HudMessages m;
    m.Show(std::string("A"));      // slot 3
    m.Tick();                      // A starts showing
    m.Show(std::string("B"));      // slot 2
    m.Show(std::string("C"));      // slot 1
    m.Show(std::string("D"));      // slot 0
    m.Show(std::string("E"));      // no slot: dropped
    for (int i = 0; i < 1 + HudMessages::HOLD_TICKS; ++i) { m.Tick(); }
    // A is done; the lowest occupied slot is D's.
    CHECK(m.Visible()[0] == "D");
}

TEST_CASE("HUD messages: the caret is solid while typing, then blinks four passes on, four off")
{
    HudMessages m;
    m.Show(std::string("AB"));
    m.Tick();
    CHECK(m.TypingLine() == 0);
    m.Tick();                      // count 2: typed
    CHECK(m.TypingLine() == 0);
    m.Tick();                      // count 3
    CHECK(m.TypingLine() == 0);
    m.Tick();                      // count 4: bit 2 set, off
    CHECK(m.TypingLine() == HudMessages::NO_CARET);
    for (int i = 0; i < 4; ++i) { m.Tick(); }   // count 8: on again
    CHECK(m.TypingLine() == 0);
}

TEST_CASE("Speech: the original's id-to-line table (DAT_000ae0ac)")
{
    CHECK(SP::LineForId(0) == 0);
    CHECK(SP::LineForId(19) == 19);
    CHECK(SP::LineForId(24) == 20);
    CHECK(SP::LineForId(29) == 21);
    CHECK(SP::LineForId(0x2a) == 34);
    CHECK(SP::LineForId(20) == 0);
}

TEST_CASE("Speech: a bank list's lines give the sample names")
{
    const auto dir = std::filesystem::temp_directory_path() / "alt_speech_test";
    std::filesystem::create_directories(dir);
    const auto file = dir / "SPCH.TXT";
    {
        std::ofstream out(file);
        out << "X:cd\\sfx\\VC01C.raw 0 \r\n" << "X:cd\\sfx\\vc02.raw 0\n" << "\n";
    }
    const auto stems = SP::ReadBankList(file);
    REQUIRE(stems.size() == 2);
    CHECK(stems[0] == "vc01c");
    CHECK(stems[1] == "vc02");
    std::filesystem::remove_all(dir);
}

TEST_CASE("Pickups: a weapon or its ammunition sounds that weapon; everything else 0x28 (FUN_0003c6f4)")
{
    namespace PK = ALTEngine::Screens::Pickups;
    CHECK(PK::SoundFor(0x00) == 0x0c);
    CHECK(PK::SoundFor(0x0a) == 0x0f);
    CHECK(PK::SoundFor(0x0c) == 0x07);
    CHECK(PK::SoundFor(0x03) == 0x0a);
    CHECK(PK::SoundFor(0x0e) == 0x12);
    CHECK(PK::SoundFor(0x15) == 0x28);
}
