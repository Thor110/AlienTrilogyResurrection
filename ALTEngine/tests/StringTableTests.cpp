#include "TestFramework.h"

#include "Bootstrap/Strings.h"

#include <string>

using ALTEngine::Bootstrap::StringId;

namespace
{
    std::string English(StringId id)
    {
        const char* s = ALTEngine::Bootstrap::StringsEnglish()[static_cast<std::size_t>(id)];
        return s ? s : "";
    }
}

// The built-in English table is filled by position, so a row added to or
// removed from StringId without the same change here shifts every string after
// it - every label and description below it then reads its neighbour's text.
TEST_CASE("Strings: the built-in English table lines up with StringId")
{
    CHECK(English(StringId::Modern) == std::string("Modern"));
    CHECK(English(StringId::EnableAll) == std::string("Enable All"));
    CHECK(English(StringId::KeepItems) == std::string("Keep Items"));
    CHECK(English(StringId::DescKeepItems) == std::string("Carry weapons and ammo between levels"));
    CHECK(English(StringId::PlayerJumping) == std::string("Player Jumping"));
    CHECK(English(StringId::FreeLook) == std::string("Free Look"));
    CHECK(English(StringId::DisableResupplyTimer) == std::string("Disable Resupply Timer"));
    CHECK(English(StringId::DescAllTestingOptions) == std::string("Turns every testing option on or off at once"));
}
