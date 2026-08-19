#pragma once

#include "HudPanel.h"
#include "../Formats/BndTextureLoader.h"
#include "../Screens/PlayerHudState.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <functional>
#include <vector>

namespace ALTEngine::Renderer
{
    // Draws the original's HUD - health row and ammo row - from the panel
    // sheet's own glyphs and palette.
    //
    // Everything positional comes from the decompilation; see HudPanel.h for
    // where each number was read from.
    //
    // HOW THE ORIGINAL GETS THE HUD ONTO THE SCREEN - traced, and not what this
    // used to do. FUN_0003aac8 builds each HUD element as a display-list entry
    // whose draw routine is FUN_000498dc, and sets the entry's byte +0xb to 1.
    // That byte selects FUN_000498dc's SECOND branch, which:
    //   - does NOT apply the outputWidth/320, outputHeight/240 scaling its
    //     first branch applies,
    //   - forces the clip bounds to 0x13f x 0xef (319 x 239), and
    //   - swaps the draw surface pointer (fix_off32_000ad58c) to a separate
    //     buffer for the duration of the call, then restores it.
    // In other words the HUD is rasterised at 1:1 into its own 320x240 surface
    // at whole-pixel coordinates, and that surface is what reaches the screen.
    // Nothing in the HUD is ever scaled per element.
    //
    // This renders into a 320x240 target and blits it once, which is the same
    // thing. It is why the per-element rounding workarounds that used to live
    // in this file are gone: at 1:1 there is nothing to round.
    //
    // The final blit still stretches to the window rather than letterboxing,
    // since independent per-axis scaling is what the original's own scaled path
    // does for everything else.
    class HudRenderer
    {
    public:
        // Loads the panel sheet for a level. `fileIndex` comes from
        // HudPanelFileForLevelId. Safe to call repeatedly; reloads only when the
        // index changes. Returns false if the sheet could not be found, in which
        // case Draw does nothing.
        // `languageFolderName` is the current language's folder ("English"),
        // used to disambiguate when several language variants of the sheet are
        // present. Pass empty to take the first match.
        bool Load(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int fileIndex,
                  const std::string& languageFolderName = {});

        // Draws a string in the game's OWN HUD font, the one the ammo row uses.
        //
        // Font A holds 91 glyphs and its '0' is glyph 0x10, which puts glyph 0 at
        // character 0x20 - so the set is printable ASCII from space to 'z' and
        // covers letters, not only digits. That makes it usable for the typed
        // messages, and it is the right font for them: the 8x8 bitmap font the
        // menus use is a different, chunkier face and looks oversized against a
        // 320x240 HUD.
        //
        // Returns the pixel width drawn, in HUD units.
        // `letterSpacing` is added to every glyph's advance. Font A wants 0 (its
        // rect carries a blank column already); font C wants 1.
        int DrawHudText(SDL_Renderer* renderer, const std::string& text, int x, int y,
                        int firstDescriptor, float scaleX, float scaleY,
                        int letterSpacing = 0) const;

        void Unload();

        // Draws the HUD over the current frame. `outputWidth/Height` are the
        // real render target size.
        // `drawUnderPanels`, if given, runs after the 320x240 surface is
        // cleared and before any panel is drawn, with that surface still bound
        // as the render target. It is how the weapon gets composited at 1:1
        // alongside the HUD instead of being scaled separately into the window -
        // the original puts both in the same surface, and doing it any other way
        // leaves the weapon's bottom edge landing on a different pixel row than
        // the screen edge does.
        void Draw(SDL_Renderer* renderer, const ALTEngine::Screens::PlayerHudState& state,
                  int outputWidth, int outputHeight,
                  const std::function<void()>& drawUnderPanels = {},
                  const std::function<void()>& drawOverPanels = {}) const;

        // Draws the tracker's edge strips above and below an arbitrary box, in
        // 320x240 HUD space. Used to frame the live minimap so it matches the
        // tracker's styling.
        // `thicknessScale` multiplies the strip's height, so a box drawn at
        // double size gets strips of proportionate weight rather than hairlines.
        void DrawEdgeStrips(SDL_Renderer* renderer, int left, int right, int top, int bottom,
                            int outputWidth, int outputHeight, int thicknessScale = 1) const;

        // Same, but in device PIXELS. Used where the box being framed is already
        // known in pixels - converting back into HUD units and re-scaling rounds
        // twice and leaves a one-pixel gap under the box.
        void DrawEdgeStripsPixels(SDL_Renderer* renderer, const SDL_FRect& box, float thickness) const;

        // Loads OVERRIDE/CUSTOM/minimap-border.png, a border made for the
        // minimap rather than borrowed from the tracker's panel strip. Absent is
        // fine - DrawEdgeStripsPixels falls back to the panel strip.
        bool LoadCustomBorder(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory);

        // One gameplay tick of the motion tracker sweep.
        void TickTracker();

        // Draws the tracker dish and its contacts. `contacts` are world-space
        // offsets from the player, already in the game's units; they are gated
        // by range, converted to cells and rotated by the player's facing here,
        // exactly as FUN_0003a008 does.
        // A tracker contact: the world-space offset from the player, plus what
        // the original's own filter tests - the entity's type, its state, and
        // whether any of its velocity fields is non-zero.
        struct Contact
        {
            float dx = 0, dz = 0;
            int type = 0;
            int state = 0;
            bool moving = false;
        };
        void DrawTracker(SDL_Renderer* renderer, const std::vector<Contact>& contacts,
                         float playerYaw, int outputWidth, int outputHeight) const;

        // The half-rate counter FUN_00039f8c keeps. Drives both the sweep and the
        // blip blink.
        int TrackerTick() const { return trackerTick; }

        bool Ready() const { return sheet != nullptr; }

        // One transparent slot in a bar frame: where the fill shows through.
        // Offsets are relative to the frame's top-left corner.
        struct BarSlot { int x, width, top, bottom; };

        // A frame's artwork, tightened to its non-transparent content. `inset`
        // is where the content starts inside the descriptor's rect, so the
        // drawn position can be offset to match.
        struct Frame
        {
            ALTEngine::Formats::BxRectangle rect{};
            int insetX = 0;
            int insetY = 0;
            bool valid = false;
        };

        ~HudRenderer() { Unload(); }

    private:
        // The original's 320x240 HUD surface. Created on first use and kept for
        // the renderer's lifetime; Draw() is const, hence mutable.
        mutable SDL_Texture* hudTarget = nullptr;

        // Blits one descriptor rect into the 320x240 virtual space at (x, y),
        // scaled to the output.
        // x/y in DEVICE PIXELS - see the definition.
        void DrawDescriptor(SDL_Renderer* renderer, int descriptorIndex, float x, float y,
                            float scaleX, float scaleY) const;

        // Finds the transparent slots inside a frame descriptor: contiguous
        // columns whose keyed run is tall enough to be a bar slot, with that
        // run's top and bottom. `page` is the un-keyed RGBA.
        // Shrinks a descriptor's rect to the artwork actually in it.
        static Frame TightenFrame(const ALTEngine::Formats::BndTexture& page,
                                  const ALTEngine::Formats::BxRectangle& rect);

        static std::vector<BarSlot> ScanBarSlots(const ALTEngine::Formats::BndTexture& page,
                                                 const ALTEngine::Formats::BxRectangle& frame);

        // Blits an arbitrary region of the sheet page. The frame artwork spans
        // several descriptors, so it is taken as a raw rect rather than looked up.
        void DrawFrame(SDL_Renderer* renderer, const Frame& frame, int x, int y,
                       float scaleX, float scaleY) const;

        void DrawSheetRegion(SDL_Renderer* renderer, int srcX, int srcY, int w, int h,
                             int dstX, int dstY, float scaleX, float scaleY) const;

        // Draws a number right-aligned the way the original does: it walks the
        // digits most significant first, advancing by each glyph's own width.
        // Returns the width consumed.
        int DrawNumber(SDL_Renderer* renderer, int value, int x, int y,
                       int firstDescriptor, float scaleX, float scaleY) const;

        // Slots scanned out of the frame artwork at load time - see
        // ScanBarSlots. Derived from the sheet rather than hardcoded, so the
        // alien-styled PNL1 frames work without a second table.
        std::vector<BarSlot> healthSlots;
        std::vector<BarSlot> ammoSlots;
        // Sweep state, advanced by TickTracker.
        int trackerFrame = 0;
        int trackerTimer = 0;
        int trackerPause = 0;
        int trackerTick = 0;

        Frame healthFrame;
        Frame ammoFrame;

        SDL_Texture* customBorder = nullptr;
        int customBorderW = 0;
        int customBorderH = 0;

        SDL_Texture* sheet = nullptr;
        std::vector<ALTEngine::Formats::BxRectangle> rects;
        int loadedFileIndex = -1;
    };
}
