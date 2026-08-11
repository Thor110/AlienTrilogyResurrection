#pragma once

#include "HudPanel.h"
#include "../Formats/BndTextureLoader.h"
#include "../Screens/PlayerHudState.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Renderer
{
    // Draws the original's HUD - health row and ammo row - from the panel
    // sheet's own glyphs and palette.
    //
    // Everything positional comes from the decompilation; see HudPanel.h for
    // where each number was read from. The one thing invented here is how the
    // 320x240 virtual space maps to the window: the original scales by
    // outputWidth/320 and outputHeight/240 independently (FUN_000498dc), so a
    // non-4:3 window stretches the HUD. That is reproduced rather than
    // letterboxed, because it is what the original does.
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

        void Unload();

        // Draws the HUD over the current frame. `outputWidth/Height` are the
        // real render target size.
        void Draw(SDL_Renderer* renderer, const ALTEngine::Screens::PlayerHudState& state,
                  int outputWidth, int outputHeight) const;

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
        struct Contact { float dx, dz; };
        void DrawTracker(SDL_Renderer* renderer, const std::vector<Contact>& contacts,
                         float playerYaw, int outputWidth, int outputHeight) const;

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
        // Blits one descriptor rect into the 320x240 virtual space at (x, y),
        // scaled to the output.
        void DrawDescriptor(SDL_Renderer* renderer, int descriptorIndex, int x, int y,
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
