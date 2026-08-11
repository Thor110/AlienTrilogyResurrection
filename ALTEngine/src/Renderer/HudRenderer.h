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

        bool Ready() const { return sheet != nullptr; }

        ~HudRenderer() { Unload(); }

    private:
        // Blits one descriptor rect into the 320x240 virtual space at (x, y),
        // scaled to the output.
        void DrawDescriptor(SDL_Renderer* renderer, int descriptorIndex, int x, int y,
                            float scaleX, float scaleY) const;

        // Blits an arbitrary region of the sheet page. The frame artwork spans
        // several descriptors, so it is taken as a raw rect rather than looked up.
        void DrawSheetRegion(SDL_Renderer* renderer, int srcX, int srcY, int w, int h,
                             int dstX, int dstY, float scaleX, float scaleY) const;

        // Draws a number right-aligned the way the original does: it walks the
        // digits most significant first, advancing by each glyph's own width.
        // Returns the width consumed.
        int DrawNumber(SDL_Renderer* renderer, int value, int x, int y,
                       int firstDescriptor, float scaleX, float scaleY) const;

        SDL_Texture* sheet = nullptr;
        std::vector<ALTEngine::Formats::BxRectangle> rects;
        int loadedFileIndex = -1;
    };
}
