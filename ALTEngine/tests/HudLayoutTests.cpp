#include "TestFramework.h"
#include "Bootstrap/GameplaySettings.h"

#include "Renderer/HudLayout.h"

#include <string>
#include <vector>

namespace R = ALTEngine::Renderer;
using ALTEngine::Screens::PlayerHudState;

namespace
{
    PlayerHudState WithItems(int count)
    {
        PlayerHudState s;
        s.currentWeapon = 0;
        s.seismicCharges = count > 0 ? 3 : 0;
        s.armourItem = count > 1 ? 1 : 0;
        s.terrainSpeed = count > 2 ? 1 : 0;
        s.terrainShield = count > 3 ? 1 : 0;
        return s;
    }
}

TEST_CASE("HUD left side: without the minimap the countdown and the items keep the original's places")
{
    const auto c = R::LayoutHudLeftColumn(WithItems(1), false, true);
    CHECK(c.resupplyY == ALTEngine::Game::ResupplyTimer::HUD_Y);
    CHECK(c.itemTop == R::HUD_ITEM_STACK_TOP);
    // The messages at the original's place, moved below the box if it reaches it.
    CHECK(c.messageX == R::HUD_MESSAGE_ORIGINAL_X);
    CHECK(c.messageTop >= c.itemBottom + R::HUD_LEFT_COLUMN_GAP);
    const auto bare = R::LayoutHudLeftColumn(WithItems(0), false, false);
    CHECK(bare.messageX == R::HUD_MESSAGE_ORIGINAL_X);
    CHECK(bare.messageTop == R::HUD_MESSAGE_ORIGINAL_Y);
}

TEST_CASE("HUD left side: with the minimap everything starts below it, countdown, items, messages in turn")
{
    for (int items = 0; items <= 4; ++items)
    {
        for (bool resupply : { false, true })
        {
            const auto c = R::LayoutHudLeftColumn(WithItems(items), true, resupply);
            CHECK(c.resupplyY > R::HUD_MINIMAP_BOTTOM);
            CHECK(c.itemTop > R::HUD_MINIMAP_BOTTOM);
            if (resupply) { CHECK(c.itemTop >= c.resupplyY + R::HUD_FONT_GLYPH_HEIGHT); }
            CHECK(c.messageTop > R::HUD_MINIMAP_BOTTOM);
            CHECK(c.messageTop >= c.itemBottom);
            CHECK(R::HudItemCount(WithItems(items)) == items);
        }
    }
}

TEST_CASE("HUD left side: the messages never start inside the item stack")
{
    for (int items = 0; items <= 4; ++items)
    {
        const auto c = R::LayoutHudLeftColumn(WithItems(items), false, false);
        CHECK(c.messageTop >= c.itemBottom);
    }
    // The grenade slot counts only with the pulse rifle up; the charges only without it.
    PlayerHudState s; s.currentWeapon = R::HUD_GRENADE_WEAPON; s.grenades = 4; s.seismicCharges = 2;
    CHECK(R::HudItemCount(s) == 1);
    s.grenades = 0;
    CHECK(R::HudItemCount(s) == 0);
}

TEST_CASE("HUD items: the boxes are wide enough that the count never runs into the label")
{
    for (int count : { 1, 9, 10, 99, 100, 999, 1000, 9999, 32767 })
    {
        PlayerHudState s;
        s.seismicCharges = static_cast<int16_t>(count);
        s.armourItem = static_cast<int16_t>(count);
        s.terrainSpeed = static_cast<int16_t>(count);
        s.terrainShield = static_cast<int16_t>(count);
        const R::HudItems shown = R::ShownHudItems(s);
        const int width = R::HudItemWidth(shown);
        CHECK(width >= R::HUD_ITEM_WIDTH);
        for (int i = 0; i < shown.count; ++i)
        {
            const std::string label = shown.items[static_cast<size_t>(i)].label;
            const std::string text = std::to_string(shown.items[static_cast<size_t>(i)].count);
            const int labelRight = R::HUD_ITEM_X + R::HUD_ITEM_PADDING + R::HudItemTextWidth(static_cast<int>(label.size()));
            const int countLeft = R::HUD_ITEM_X + width - R::HUD_ITEM_PADDING - R::HudItemTextWidth(static_cast<int>(text.size()));
            CHECK(countLeft - labelRight >= R::HUD_ITEM_LABEL_GAP);
        }
    }
}

TEST_CASE("Motion tracker: one sweep is 96 timer ticks, as FUN_00039f8c steps it from the 69.74 Hz interrupt")
{
    // HudRenderer::TickTracker's stepping: work on every other timer tick.
    int tick = 0, frame = 0, timer = 0, pause = 0;
    std::vector<int> wraps;
    for (int t = 1; t <= 96 * 4; ++t)
    {
        if ((++tick & 1) == 0) { continue; }
        const int before = frame;
        R::HudTrackerAdvance(frame, timer, pause);
        if (before != 0 && frame == 0) { wraps.push_back(t); }
    }
    REQUIRE(wraps.size() >= 3);
    // 96 ticks = 1.38 s: about the 1.31 s between the clicks in 1002moti, which
    // is why the sample plays through rather than being restarted each sweep.
    CHECK(wraps[1] - wraps[0] == 96);
    CHECK(wraps[2] - wraps[1] == 96);
}

TEST_CASE("HUD items: speed and shield show their ticks in 0x1e units, at least 1 (0x39ebd)")
{
    PlayerHudState s;
    s.terrainSpeed = 0x1e * 20;
    s.terrainShield = 5;
    const R::HudItems shown = R::ShownHudItems(s);
    REQUIRE(shown.count == 2);
    CHECK_EQ(shown.items[0].count, 20);
    CHECK_EQ(shown.items[1].count, 1);
}

TEST_CASE("HUD Size: each group shrinks toward its own corner; the default, the largest, is the original's size")
{
    namespace B = ALTEngine::Bootstrap;
    CHECK(B::HudSizeSettings::DEFAULT == B::HudSizeSettings::MAXIMUM);
    CHECK(B::HudSizeSettings::ScaleFor(B::HudSizeSettings::MAXIMUM) == 1.0f);
    CHECK(B::HudSizeSettings::ScaleFor(B::HudSizeSettings::MINIMUM) < 0.6f);
    // At 1, nothing moves.
    float x = 215, y = 19, w = 98, h = 34;
    R::HudScaleRect(x, y, w, h, R::HudCorner{ true, false }, 1.0f);
    CHECK(x == 215.0f); CHECK(y == 19.0f); CHECK(w == 98.0f);
    // Health, top right, at half size: its right edge keeps its gap to the screen's.
    x = 215; y = 19; w = 98; h = 34;
    R::HudScaleRect(x, y, w, h, R::HudCorner{ true, false }, 0.5f);
    CHECK(x + w == 320.0f - (320.0f - 313.0f) * 0.5f);
    CHECK(y == 9.5f); CHECK(w == 49.0f);
    // The tracker, bottom right, at 150%: still on screen.
    x = R::HUD_TRACKER_LEFT; y = R::HUD_TRACKER_TOP_STRIP_Y; w = R::HUD_TRACKER_RIGHT - R::HUD_TRACKER_LEFT; h = 70;
    R::HudScaleRect(x, y, w, h, R::HUD_CORNER_BOTTOM_RIGHT, 1.5f);
    CHECK(x + w <= 320.0f); CHECK(y + h <= 240.0f);
    // The split lines run between the groups.
    CHECK(R::HUD_SPLIT_X > R::HUD_OVERLAY_AMMO_X + 130);
    CHECK(R::HUD_SPLIT_X < R::HUD_OVERLAY_HEALTH_X);
    CHECK(R::HUD_SPLIT_Y < R::HUD_TRACKER_TOP_STRIP_Y);
}
