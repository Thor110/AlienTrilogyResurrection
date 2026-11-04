#pragma once

#include "HudPanel.h"
#include "../Game/ResupplyTimer.h"
#include "../Screens/PlayerHudState.h"

#include <algorithm>
#include <array>

namespace ALTEngine::Renderer
{
    // THE LEFT SIDE OF THE HUD, top to bottom: the Live Minimap (a Modern
    // option), the resupply countdown, the carried-item stack and the typed
    // messages. Each starts below the one above it, so none covers another.
    // Without the minimap the countdown and the stack keep the original's
    // places, (16, 19) and y 0x37; with it they move down below the map, in
    // the same order (Edward, 2026). The messages start at HUD_MESSAGE_Y, or
    // below the stack if it reaches that far. All in 320x240 HUD units.
    struct HudLeftColumn
    {
        int resupplyY = 0;
        int itemTop = 0;
        int itemBottom = 0;   // == itemTop when no item is shown
        int messageTop = 0;
    };

    // The carried-item stack's boxes, top to bottom, each only while its count
    // is above 0 (FUN_0003aac8). Slot 1 is Fire 2's: the grenade launcher with
    // the pulse rifle up, the seismic charges with anything else
    // (0x3ac5a-0x3acc3). The one list both the drawing and the layout read.
    struct HudItem { const char* label; int count; };
    struct HudItems
    {
        std::array<HudItem, 4> items{};
        int count = 0;
    };

    inline HudItems ShownHudItems(const ALTEngine::Screens::PlayerHudState& state)
    {
        HudItems out;
        auto add = [&](const char* label, int count) { if (count > 0) { out.items[static_cast<size_t>(out.count++)] = { label, count }; } };
        if (state.currentWeapon == HUD_GRENADE_WEAPON) { add("GREN", state.grenades); }
        else { add("CHRG", state.seismicCharges); }
        add("ARMR", state.armourItem);
        add("SPD", state.terrainSpeed);
        add("SHLD", state.terrainShield);
        return out;
    }

    inline int HudItemCount(const ALTEngine::Screens::PlayerHudState& state) { return ShownHudItems(state).count; }

    // How wide the item boxes are drawn. The label sits at the left and the
    // count at the right, so a box must hold both with a gap between them;
    // 54 left a four-letter label and one digit 3 pixels apart, and a longer
    // count ran into the label (Edward, 2026). Every box in the stack takes the width of
    // the widest one, so the column stays straight, and never goes below 54.
    // Text is the 8x8 font at scale 1: 9 pixels a character less the last
    // pixel of spacing (Bootstrap::TextWidth, which pulls in SDL).
    inline constexpr int HUD_ITEM_GLYPH_ADVANCE = 9;
    inline constexpr int HUD_ITEM_LABEL_GAP = 5;

    inline int HudItemTextWidth(int characters)
    {
        return characters > 0 ? characters * HUD_ITEM_GLYPH_ADVANCE - 1 : 0;
    }

    inline int HudItemDigits(int count)
    {
        int digits = count < 0 ? 2 : 1;   // a minus sign counts as one
        for (int v = count < 0 ? -count : count; v >= 10; v /= 10) { ++digits; }
        return digits;
    }

    inline int HudItemWidth(const HudItems& shown)
    {
        int width = HUD_ITEM_WIDTH;
        for (int i = 0; i < shown.count; ++i)
        {
            const HudItem& item = shown.items[static_cast<size_t>(i)];
            int labelLength = 0;
            while (item.label && item.label[labelLength] != '\0') { ++labelLength; }
            const int needed = HUD_ITEM_PADDING + HudItemTextWidth(labelLength) + HUD_ITEM_LABEL_GAP
                             + HudItemTextWidth(HudItemDigits(item.count)) + HUD_ITEM_PADDING;
            width = std::max(width, needed);
        }
        return width;
    }

    inline HudLeftColumn LayoutHudLeftColumn(const ALTEngine::Screens::PlayerHudState& state,
                                             bool minimapShown, bool resupplyShown)
    {
        HudLeftColumn out;
        if (!minimapShown)
        {
            out.resupplyY = ALTEngine::Game::ResupplyTimer::HUD_Y;
            out.itemTop = HUD_ITEM_STACK_TOP;
        }
        else
        {
            int y = HUD_MINIMAP_BOTTOM + HUD_LEFT_COLUMN_GAP;
            out.resupplyY = y;
            if (resupplyShown) { y += HUD_FONT_GLYPH_HEIGHT + HUD_LEFT_COLUMN_GAP; }
            out.itemTop = y;
        }
        const int items = HudItemCount(state);
        // The last box ends HUD_ITEM_HEIGHT below its row, not a whole step.
        out.itemBottom = items > 0 ? out.itemTop + (items - 1) * HUD_ITEM_ROW_STEP + HUD_ITEM_HEIGHT : out.itemTop;
        int messageTop = HUD_MESSAGE_Y;
        if (minimapShown) { messageTop = std::max(messageTop, out.itemTop); }
        if (items > 0) { messageTop = std::max(messageTop, out.itemBottom + HUD_LEFT_COLUMN_GAP); }
        out.messageTop = messageTop;
        return out;
    }
}
