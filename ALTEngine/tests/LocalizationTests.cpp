#include "TestFramework.h"

#include "Bootstrap/Localization.h"

#include <filesystem>
#include <fstream>

namespace L = ALTEngine::Bootstrap;
using L::Language;

TEST_CASE("Localization: a video's names go chosen language, English, then the others")
{
    const auto names = L::LocalizedBaseNameCandidates("GAMEOVER", Language::Italian);
    CHECK_EQ(names.size(), static_cast<size_t>(4));
    CHECK(names[0] == "IGAMEOVE");
    CHECK(names[1] == "GAMEOVER");
    CHECK(names[2] == "FGAMEOVE");
    CHECK(names[3] == "SGAMEOVE");

    const auto english = L::LocalizedBaseNameCandidates("INTRO", Language::English);
    CHECK(english[0] == "INTRO");
    CHECK_EQ(english.size(), static_cast<size_t>(4));
}

TEST_CASE("Localization: a missing language's video falls back to the one on the disc (Pox83)")
{
    const auto dir = std::filesystem::temp_directory_path() / "altengine_localization_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "INTRO.AVI") << "x";

    // French chosen, an English disc: the English video.
    const auto french = L::FindLocalizedVideo(dir, "INTRO", Language::French);
    CHECK(french.has_value());
    if (french) { CHECK(french->filename() == "INTRO.AVI"); }

    // The chosen language's own, when it's there.
    std::ofstream(dir / "FINTRO.AVI") << "x";
    const auto own = L::FindLocalizedVideo(dir, "INTRO", Language::French);
    CHECK(own.has_value());
    if (own) { CHECK(own->filename() == "FINTRO.AVI"); }

    // A French-only disc with English chosen: the French video.
    std::filesystem::remove(dir / "INTRO.AVI");
    const auto english = L::FindLocalizedVideo(dir, "INTRO", Language::English);
    CHECK(english.has_value());
    if (english) { CHECK(english->filename() == "FINTRO.AVI"); }

    CHECK(!L::FindLocalizedVideo(dir, "DEATH1", Language::English).has_value());
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("Localization: the mission text and panels try the chosen language first, then the rest")
{
    const auto spanish = L::LanguageSuffixCandidates(Language::Spanish);
    CHECK(spanish.front() == 'S');
    CHECK_EQ(spanish.size(), static_cast<size_t>(7));
    const auto english = L::LanguageSuffixCandidates(Language::English);
    CHECK(english[0] == 'U');
    CHECK(english[1] == 'E');
    CHECK_EQ(english.size(), static_cast<size_t>(7));
}
