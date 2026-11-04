#include "TestFramework.h"

#include "Renderer/HudLayout.h"

#include <string>

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
    CHECK(c.messageTop == R::HUD_MESSAGE_Y);
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
