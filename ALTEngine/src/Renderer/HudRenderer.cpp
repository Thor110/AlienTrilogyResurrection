#include "HudRenderer.h"

#include "HudPanel.h"
#include "../Formats/OverrideImage.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace ALTEngine::Renderer
{
    namespace
    {
        // The two bar colours, read from the panel sheet's own CL00 palette at
        // the CLUT indices the flat-quad primitive uses. Hardcoded rather than
        // looked up at runtime because BndTextureLoader resolves the palette
        // into RGBA when it builds the page and does not keep the CLUT around;
        // both sheets agree on index 5, which is the fill.
        // Bar green: entry 5 of PANEL.PAL, given by Edward as RGB 72/164/72.
        //
        // Replaces the 176/216/176 read out of the sheet's own CL00 at the CLUT
        // index the flat-quad primitive uses - that was too pale. Used as a
        // literal rather than looked up, since nothing yet establishes whether
        // the HUD actually samples PANEL.PAL at runtime; if it turns out it does,
        // this becomes a palette read and the value should agree.
        constexpr SDL_Color BAR_FILL{ 72, 164, 72, 255 };
        constexpr SDL_Color BAR_FRAME{ 24, 24, 16, 255 };

        // The counters read GREY in the original, not white (Edward, 2026). The
        // glyphs on the sheet are near-white (200,200,200 is the second most
        // common colour in the page), so they are tinted down rather than
        // recoloured.
        //
        // A value set by eye, not a measurement - the original's own glyph colour
        // comes from that display-list entry's three colour bytes, and while the
        // glyph path writes 0x80 to each, 0x80 through the textured primitive's
        // >> 2 into a 6-bit register is "neutral", not a tint. So there is no
        // stated grey to transcribe.
        constexpr Uint8 NUMBER_TINT = 190;

        // Finds the panel sheet for `fileIndex`.
        //
        // The sheets live in the LANGUAGE folder, not GFX (Edward, 2026) - which
        // is the final confirmation that they are the language-selected
        // descriptor set the fonts and word labels come from, matching the switch
        // on DAT_000ae10c in the decompilation. They are also absent from
        // DiscFileManifest.json, whose GFX list is known to be partial.
        //
        // Name shape is PNL<index>GFX<letter>.16, where the trailing letter is a
        // language code ("U" on Edward's disc). Which letter belongs to which
        // language is NOT established, so:
        //   - a file inside a subfolder named after the current language wins,
        //     since that is unambiguous
        //   - otherwise the first match is taken and every candidate is logged,
        //     so the letter-to-language mapping can be filled in from a real run
        //     rather than guessed at here.
        std::filesystem::path FindPanelSheet(const std::filesystem::path& cdDirectory, int fileIndex,
                                             const std::string& languageFolderName)
        {
            auto lower = [](std::string v) {
                for (char& c : v) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
                return v;
            };

            std::string wanted = "pnl" + std::to_string(fileIndex) + "gfx";
            std::string wantedFolder = lower(languageFolderName);
            std::error_code ec;

            // Candidates are RANKED rather than first-come. Recursive iteration
            // order is not defined, and taking the first match picked the French
            // sheet out of a LANGUAGE folder while English was selected.
            //   +4 = inside a subfolder named after the current language
            //   +2 = directly in LANGUAGE (where the files actually live)
            //   +1 = suffix letter E, the preferred variant
            //   +0 = suffix letter U or anything else
            // Summed, so the location matters more than the letter but the letter
            // breaks ties between two files in the same place.
            std::vector<std::filesystem::path> matches;
            std::filesystem::path best;
            int bestRank = -1;

            // LANGUAGE first and recursively, then the other plausible homes.
            for (const char* folder : { "LANGUAGE", "GFX", "." })
            {
                std::filesystem::path dir = cdDirectory / folder;
                if (!std::filesystem::is_directory(dir, ec)) { continue; }

                for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
                     it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                {
                    if (ec) { break; }
                    if (!it->is_regular_file(ec)) { continue; }

                    std::string name = lower(it->path().filename().string());
                    if (name.rfind(wanted, 0) != 0) { continue; }
                    if (name.size() < wanted.size() + 3) { continue; }      // <letter>.16
                    if (name.substr(name.size() - 3) != ".16") { continue; }

                    matches.push_back(it->path());

                    std::string parent = lower(it->path().parent_path().filename().string());
                    int rank = 0;
                    if (!wantedFolder.empty() && parent == wantedFolder) { rank += 4; }
                    else if (parent == "language") { rank += 2; }

                    // The letter immediately before ".16".
                    char letter = name[name.size() - 4];
                    if (letter == 'e') { rank += 1; }

                    if (rank > bestRank) { bestRank = rank; best = it->path(); }
                }
                if (!matches.empty()) { break; }
            }

            if (matches.empty()) { return {}; }

            if (matches.size() > 1)
            {
                SDL_Log("HudRenderer: %zu candidate panel sheets for index %d, chose %s:",
                        matches.size(), fileIndex, best.filename().string().c_str());
                for (const auto& m : matches) { SDL_Log("    %s", m.string().c_str()); }
            }
            return best;
        }
    }

    namespace
    {
        // Snaps a destination rect to whole device pixels.
        //
        // The HUD is a 320x240 space stretched to the window, and the vertical
        // scale is routinely fractional - 1080/240 is 4.5. A destination at
        // y * 4.5 lands on a half pixel, and NEAREST sampling of a half-pixel
        // offset reaches one texel outside the source rect. On the health frame,
        // whose descriptor is flush against its neighbours with no padding, that
        // pulled the adjacent artwork's row in and showed as the top-right corner
        // tearing (Edward, 2026).
        //
        // Rounding position and extent independently, rather than rounding the
        // size, keeps adjacent elements from overlapping or leaving seams.
        SDL_FRect SnapToPixels(float x, float y, float w, float h)
        {
            float left = SDL_roundf(x);
            float top = SDL_roundf(y);
            float right = SDL_roundf(x + w);
            float bottom = SDL_roundf(y + h);
            return SDL_FRect{ left, top, right - left, bottom - top };
        }
    }

    HudRenderer::Frame HudRenderer::TightenFrame(const ALTEngine::Formats::BndTexture& page,
                                                 const ALTEngine::Formats::BxRectangle& rect)
    {
        // The frame descriptors are PADDED: descriptor 173's rect is 130x18 but
        // its artwork is only 126x15, sitting 3 rows down. Drawing the padded
        // rect put its top edge at sheet y 97, immediately below the font row -
        // and with a non-integer output scale, NEAREST sampling pulled a row of
        // white glyph pixels in, which showed as a streak above the ammo counter
        // (Edward, 2026).
        //
        // Tightening to the real content both removes the bleed and confirms
        // Edward's own measurements of 126x15 at (0,100) and 98x34 at (0,115).
        Frame out;
        int minX = rect.width, minY = rect.height, maxX = -1, maxY = -1;

        for (int y = 0; y < rect.height; ++y)
        {
            for (int x = 0; x < rect.width; ++x)
            {
                int px = rect.x + x;
                int py = rect.y + y;
                if (px < 0 || py < 0 || px >= page.width || py >= page.height) { continue; }
                size_t i = (static_cast<size_t>(py) * page.width + px) * 4;
                if (page.rgba[i] == 0 && page.rgba[i + 1] == 0 && page.rgba[i + 2] == 0) { continue; }
                if (x < minX) { minX = x; }
                if (y < minY) { minY = y; }
                if (x > maxX) { maxX = x; }
                if (y > maxY) { maxY = y; }
            }
        }

        if (maxX < 0) { return out; }   // nothing but key colour

        out.rect.x = rect.x + minX;
        out.rect.y = rect.y + minY;
        out.rect.width = maxX - minX + 1;
        out.rect.height = maxY - minY + 1;
        out.rect.page = rect.page;
        out.insetX = minX;
        out.insetY = minY;
        out.valid = true;
        return out;
    }

    std::vector<HudRenderer::BarSlot> HudRenderer::ScanBarSlots(
        const ALTEngine::Formats::BndTexture& page,
        const ALTEngine::Formats::BxRectangle& frame)
    {
        // A slot is a run of columns that are keyed transparent over several
        // rows. Recording each run's TOP AND BOTTOM as well as its x matters:
        // filling the frame's full height instead let the bar leak upward
        // through the transparent part above each slot, which made the bars look
        // taller than the frame (Edward, 2026).
        std::vector<BarSlot> slots;
        int runStart = -1, runTop = 0, runBottom = 0;

        auto flush = [&](int endColumn) {
            if (runStart < 0) { return; }
            slots.push_back({ runStart, endColumn - runStart + 1, runTop, runBottom });
            runStart = -1;
        };

        auto clearAt = [&](int x, int y) {
            int px = frame.x + x;
            int py = frame.y + y;
            if (px < 0 || py < 0 || px >= page.width || py >= page.height) { return true; }
            size_t i = (static_cast<size_t>(py) * page.width + px) * 4;
            return page.rgba[i] == 0 && page.rgba[i + 1] == 0 && page.rgba[i + 2] == 0;
        };

        for (int x = 0; x < frame.width; ++x)
        {
            // A slot is an ENCLOSED notch: a run of keyed pixels with opaque
            // artwork both above and below it inside this frame. Without the
            // enclosure test the frame's transparent surround counts as clear
            // too, and on the ammo frame - which is a taller crop than the health
            // one - every column qualified and all 22 slots merged into a single
            // run.
            // Take the LOWEST enclosed run in the column, extended down to the
            // last keyed pixel above the frame's bottom edge.
            //
            // Taking the first run's top and bottom made the health bars a pixel
            // shorter than the ammo bars: that frame's staircase artwork breaks
            // each column into several short runs, so the health notches scanned
            // as 4 tall against ammo's 7 (Edward, 2026 - "the green lines below
            // it are one pixel shorter").
            int top = -1, bottom = -1;
            for (int y = 1; y < frame.height - 1; ++y)
            {
                if (!clearAt(x, y)) { continue; }
                if (top < 0 && !clearAt(x, y - 1)) { top = y; }   // start of a notch
                if (top >= 0) { bottom = y; }                     // keep extending
            }
            bool enclosed = (top >= 0) && (bottom >= top) && !clearAt(x, bottom + 1);
            bool isSlot = enclosed && (bottom - top + 1) >= 3;

            if (isSlot)
            {
                if (runStart < 0) { runStart = x; runTop = top; runBottom = bottom; }
                else
                {
                    if (top > runTop) { runTop = top; }
                    if (bottom < runBottom) { runBottom = bottom; }
                }
                if (x == frame.width - 1) { flush(x); }
            }
            else
            {
                flush(x - 1);
            }
        }

        return slots;
    }

    bool HudRenderer::Load(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int fileIndex,
                           const std::string& languageFolderName)
    {
        if (sheet && loadedFileIndex == fileIndex) { return true; }
        Unload();

        std::filesystem::path path = FindPanelSheet(cdDirectory, fileIndex, languageFolderName);
        if (path.empty())
        {
            SDL_Log("HudRenderer: no panel sheet found for index %d under %s - HUD disabled",
                    fileIndex, cdDirectory.string().c_str());
            return false;
        }

        ALTEngine::Formats::BndTextureSet set = ALTEngine::Formats::BndTextureLoader::Load(path);
        if (set.textures.empty() || set.uvRects.empty())
        {
            SDL_Log("HudRenderer: %s parsed to no pages or no descriptors - HUD disabled",
                    path.string().c_str());
            return false;
        }

        const ALTEngine::Formats::BndTexture& page = set.textures[0];
        sheet = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                  page.width, page.height);
        if (!sheet)
        {
            SDL_Log("HudRenderer: could not create panel texture: %s", SDL_GetError());
            return false;
        }
        // Key out the transparent colour so the frame's cut-outs let the bars
        // through. Without this the overlay's interior is opaque and either hides
        // the bars (drawn over them) or gets hidden by them (drawn under).
        //
        // Edward described the key as magenta, which is how some viewers show it,
        // but there is no magenta in this sheet: BndTextureLoader resolves the
        // palette, and the key resolves to PURE BLACK. Confirmed by inspection -
        // 61 distinct colours in the page, of which rgb(0,0,0) is far the most
        // common at 24840 of 65536 pixels, and CL00 entries 1 and 2 are both pure
        // black. Palette entry 0 is rgb(8,0,0), which is NOT keyed here; if
        // corners still look wrong that near-black is the next candidate.
        std::vector<uint8_t> keyed = page.rgba;
        int keyedCount = 0;
        for (size_t i = 0; i + 3 < keyed.size(); i += 4)
        {
            if (keyed[i] == 0 && keyed[i + 1] == 0 && keyed[i + 2] == 0)
            {
                keyed[i + 3] = 0;
                keyedCount++;
            }
        }
        SDL_UpdateTexture(sheet, nullptr, keyed.data(), page.width * 4);
        SDL_SetTextureBlendMode(sheet, SDL_BLENDMODE_BLEND);
        // The HUD is pixel art in a tiny virtual space blown up to the window,
        // so it must be point sampled or the glyphs turn to mush.
        SDL_SetTextureScaleMode(sheet, SDL_SCALEMODE_NEAREST);

        rects = set.uvRects;
        loadedFileIndex = fileIndex;

        // Scan the frames for their bar slots, from the ORIGINAL page (before
        // keying) since the scan looks for the key colour itself.
        if (static_cast<size_t>(HUD_OVERLAY_HEALTH_DESCRIPTOR) < rects.size())
        {
            healthFrame = TightenFrame(page, rects[HUD_OVERLAY_HEALTH_DESCRIPTOR]);
            if (healthFrame.valid) { healthSlots = ScanBarSlots(page, healthFrame.rect); }
        }
        if (static_cast<size_t>(HUD_OVERLAY_AMMO_DESCRIPTOR) < rects.size())
        {
            ammoFrame = TightenFrame(page, rects[HUD_OVERLAY_AMMO_DESCRIPTOR]);
            if (ammoFrame.valid) { ammoSlots = ScanBarSlots(page, ammoFrame.rect); }
        }
        SDL_Log("HudRenderer: health frame %dx%d inset (%d,%d), %zu slots; "
                "ammo frame %dx%d inset (%d,%d), %zu slots",
                healthFrame.rect.width, healthFrame.rect.height, healthFrame.insetX, healthFrame.insetY,
                healthSlots.size(),
                ammoFrame.rect.width, ammoFrame.rect.height, ammoFrame.insetX, ammoFrame.insetY,
                ammoSlots.size());

        SDL_Log("HudRenderer: loaded %s - %dx%d page, %zu descriptors, %d px keyed transparent",
                path.string().c_str(), page.width, page.height, rects.size(), keyedCount);
        return true;
    }

    void HudRenderer::Unload()
    {
        if (sheet) { SDL_DestroyTexture(sheet); sheet = nullptr; }
        if (customBorder) { SDL_DestroyTexture(customBorder); customBorder = nullptr; customBorderW = 0; customBorderH = 0; }
        rects.clear();
        healthSlots.clear();
        ammoSlots.clear();
        healthFrame = Frame{};
        ammoFrame = Frame{};
        loadedFileIndex = -1;
    }

    void HudRenderer::DrawDescriptor(SDL_Renderer* renderer, int descriptorIndex, int x, int y,
                                     float scaleX, float scaleY) const
    {
        if (descriptorIndex < 0 || static_cast<size_t>(descriptorIndex) >= rects.size()) { return; }
        const ALTEngine::Formats::BxRectangle& r = rects[static_cast<size_t>(descriptorIndex)];

        // Descriptor rects are INCLUSIVE on the disc and BxParser adds +1 to
        // width and height on read. That +1 is right for a sampler bound but
        // wrong for a glyph's own extent, so a digit drew one pixel wider than
        // the original's - confirmed by counting pixels across the zero
        // (Edward, 2026). The advance already subtracted it; the source and
        // destination rects did not.
        float w = static_cast<float>(r.width - 1);
        float h = static_cast<float>(r.height - 1);
        if (w < 1.0f) { w = static_cast<float>(r.width); }
        if (h < 1.0f) { h = static_cast<float>(r.height); }

        SDL_FRect src{ static_cast<float>(r.x), static_cast<float>(r.y), w, h };
        SDL_FRect dst = SnapToPixels(x * scaleX, y * scaleY, w * scaleX, h * scaleY);
        SDL_RenderTexture(renderer, sheet, &src, &dst);
    }

    void HudRenderer::DrawFrame(SDL_Renderer* renderer, const Frame& frame, int x, int y,
                                float scaleX, float scaleY) const
    {
        if (!sheet || !frame.valid) { return; }
        DrawSheetRegion(renderer, frame.rect.x, frame.rect.y, frame.rect.width, frame.rect.height,
                        x + frame.insetX, y + frame.insetY, scaleX, scaleY);
    }

    void HudRenderer::DrawSheetRegion(SDL_Renderer* renderer, int srcX, int srcY, int w, int h,
                                      int dstX, int dstY, float scaleX, float scaleY) const
    {
        if (!sheet || w <= 0 || h <= 0) { return; }
        SDL_FRect src{ static_cast<float>(srcX), static_cast<float>(srcY),
                       static_cast<float>(w), static_cast<float>(h) };
        SDL_FRect dst = SnapToPixels(dstX * scaleX, dstY * scaleY, w * scaleX, h * scaleY);
        SDL_RenderTexture(renderer, sheet, &src, &dst);
    }

    int HudRenderer::DrawNumber(SDL_Renderer* renderer, int value, int x, int y,
                                int firstDescriptor, float scaleX, float scaleY) const
    {
        // The original clamps to 0..999 and always emits three digits, most
        // significant first, advancing by each glyph's own width.
        int clamped = std::clamp(value, 0, 999);
        int digits[3] = { clamped / 100, (clamped % 100) / 10, clamped % 10 };

        int cursor = x;
        for (int digit : digits)
        {
            int descriptor = firstDescriptor + HUD_FONT_DIGIT_GLYPH + digit;
            DrawDescriptor(renderer, descriptor, cursor, y, scaleX, scaleY);
            if (static_cast<size_t>(descriptor) < rects.size())
            {
                // Advance by the RAW stored width, which is one less than the
                // width BxParser reports.
                //
                // FUN_00039068 builds the advance table as u1 - u0 straight off
                // the descriptor, and BxParser adds +1 to width and height on
                // read (an inclusive-to-exclusive conversion that is right for
                // sampling but wrong for an advance). Using the reported width
                // made each digit a pixel too wide, so "045" ran 3 pixels long
                // and butted against the first ammo bar - which is what made the
                // decompiled text x of 0x12 look wrong (Edward, 2026).
                cursor += rects[static_cast<size_t>(descriptor)].width - 1;
            }
        }
        return cursor - x;
    }

    void HudRenderer::Draw(SDL_Renderer* renderer, const ALTEngine::Screens::PlayerHudState& state,
                           int outputWidth, int outputHeight) const
    {
        if (!sheet || rects.empty()) { return; }
        if (outputWidth <= 0 || outputHeight <= 0) { return; }

        // Independent axis scaling, matching FUN_000498dc.
        float scaleX = static_cast<float>(outputWidth) / static_cast<float>(HUD_VIRTUAL_WIDTH);
        float scaleY = static_cast<float>(outputHeight) / static_cast<float>(HUD_VIRTUAL_HEIGHT);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        auto fillRect = [&](int x0, int y0, int x1, int y1, const SDL_Color& c) {
            if (x1 <= x0) { return; }
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
            SDL_FRect dst = SnapToPixels(x0 * scaleX, y0 * scaleY,
                                         (x1 - x0) * scaleX, (y1 - y0) * scaleY);
            SDL_RenderFillRect(renderer, &dst);
        };

        // Fills the lit slots of a bar. Slot positions come from the overlay
        // artwork (see HudPanel.h) rather than from the bar's own extent, so the
        // fill lands exactly in the transparent gaps the frame leaves. Drawn
        // full-height; the overlay on top clips each slot to its real height,
        // which is what produces the staircase.
        auto drawSlots = [&](const std::vector<BarSlot>& slots, int frameX, int frameY,
                             int litSlots, const SDL_Color& c) {
            int n = std::min(litSlots, static_cast<int>(slots.size()));
            for (int i = 0; i < n; ++i)
            {
                const BarSlot& s = slots[static_cast<size_t>(i)];
                fillRect(frameX + s.x, frameY + s.top,
                         frameX + s.x + s.width, frameY + s.bottom + 1, c);
            }
        };

        // ---- health row ----------------------------------------------------
        // Order: bar, then the frame artwork OVER it, then the number. The frame
        // is keyed transparent where the bar should show through, so it has to be
        // on top - it is what trims a pixel off the bar's corners and separates
        // the segments.
        drawSlots(healthSlots, HUD_OVERLAY_HEALTH_X + healthFrame.insetX,
                  HUD_OVERLAY_HEALTH_Y + healthFrame.insetY,
                  HudHealthLitSlots(state.health), BAR_FILL);

        DrawFrame(renderer, healthFrame, HUD_OVERLAY_HEALTH_X, HUD_OVERLAY_HEALTH_Y, scaleX, scaleY);

        SDL_SetTextureColorMod(sheet, NUMBER_TINT, NUMBER_TINT, NUMBER_TINT);
        DrawNumber(renderer, state.health, HUD_HEALTH_TEXT_X, HUD_HEALTH_ROW_TOP,
                   HUD_FONT_B_FIRST_DESCRIPTOR, scaleX, scaleY);
        SDL_SetTextureColorMod(sheet, 255, 255, 255);

        // ---- ammo row ------------------------------------------------------
        int ammo = state.CurrentAmmoTotal();
        drawSlots(ammoSlots, HUD_OVERLAY_AMMO_X + ammoFrame.insetX,
                  HUD_OVERLAY_AMMO_Y + ammoFrame.insetY,
                  HudAmmoLitSlots(ammo), BAR_FILL);

        DrawFrame(renderer, ammoFrame, HUD_OVERLAY_AMMO_X, HUD_OVERLAY_AMMO_Y, scaleX, scaleY);

        SDL_SetTextureColorMod(sheet, NUMBER_TINT, NUMBER_TINT, NUMBER_TINT);
        DrawNumber(renderer, ammo, HUD_AMMO_TEXT_X, HUD_AMMO_ROW_TOP,
                   HUD_FONT_FIRST_DESCRIPTOR, scaleX, scaleY);
        SDL_SetTextureColorMod(sheet, 255, 255, 255);
    }

    void HudRenderer::DrawEdgeStrips(SDL_Renderer* renderer, int left, int right, int top, int bottom,
                                     int outputWidth, int outputHeight, int thicknessScale) const
    {
        if (!sheet || rects.empty()) { return; }
        if (outputWidth <= 0 || outputHeight <= 0) { return; }
        if (static_cast<size_t>(HUD_TRACKER_STRIP_DESCRIPTOR) >= rects.size()) { return; }

        float scaleX = static_cast<float>(outputWidth) / static_cast<float>(HUD_VIRTUAL_WIDTH);
        float scaleY = static_cast<float>(outputHeight) / static_cast<float>(HUD_VIRTUAL_HEIGHT);

        const ALTEngine::Formats::BxRectangle& sr = rects[HUD_TRACKER_STRIP_DESCRIPTOR];
        SDL_FRect src{ static_cast<float>(sr.x), static_cast<float>(sr.y),
                       static_cast<float>(sr.width), static_cast<float>(sr.height) };
        int width = right - left;
        if (width <= 0) { return; }

        SDL_SetTextureAlphaMod(sheet, static_cast<Uint8>(HUD_TRACKER_ALPHA));

        // The strip sits just OUTSIDE the box on each side.
        //
        // The TOP one is the flipped copy, not the bottom. The artwork's own
        // orientation is the bottom edge, so drawing it unflipped at the top put
        // both strips the same way up (Edward, 2026).
        int thickness = sr.height * (thicknessScale < 1 ? 1 : thicknessScale);

        SDL_FRect topRect = SnapToPixels(left * scaleX, (top - thickness) * scaleY,
                                         width * scaleX, thickness * scaleY);
        SDL_RenderTextureRotated(renderer, sheet, &src, &topRect, 0.0, nullptr, SDL_FLIP_VERTICAL);

        SDL_FRect bottomRect = SnapToPixels(left * scaleX, bottom * scaleY,
                                            width * scaleX, thickness * scaleY);
        SDL_RenderTexture(renderer, sheet, &src, &bottomRect);

        SDL_SetTextureAlphaMod(sheet, 255);
    }

    bool HudRenderer::LoadCustomBorder(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory)
    {
        if (customBorder) { return true; }

        // Deployed by CMake to GameData/CD/OVERRIDE/CUSTOM. Loaded through the
        // existing override image path, which takes a root plus a key and
        // appends ".png" itself.
        std::optional<ALTEngine::Formats::OverrideImage> image =
            ALTEngine::Formats::TryLoadOverrideImage(cdDirectory / "OVERRIDE", "CUSTOM/minimap-border");
        if (!image)
        {
            // Absent is the normal case on an install without it; the panel strip
            // is used instead.
            return false;
        }
        if (image->width <= 0 || image->height <= 0 || image->rgba.empty())
        {
            SDL_Log("HudRenderer: CUSTOM/minimap-border.png decoded to nothing - using the panel strip");
            return false;
        }

        customBorder = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                         image->width, image->height);
        if (!customBorder) { return false; }

        // Key opaque black to transparent, as the panel sheet is (Edward, 2026).
        // This particular PNG already has an alpha channel and almost all of it
        // is transparent, so this is belt and braces for a future border saved
        // without one - and it costs one pass over 230 pixels.
        std::vector<uint8_t> keyedBorder = image->rgba;
        for (size_t i = 0; i + 3 < keyedBorder.size(); i += 4)
        {
            if (keyedBorder[i] == 0 && keyedBorder[i + 1] == 0 && keyedBorder[i + 2] == 0)
            {
                keyedBorder[i + 3] = 0;
            }
        }
        SDL_UpdateTexture(customBorder, nullptr, keyedBorder.data(), image->width * 4);
        SDL_SetTextureBlendMode(customBorder, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(customBorder, SDL_SCALEMODE_NEAREST);
        customBorderW = image->width;
        customBorderH = image->height;

        SDL_Log("HudRenderer: loaded custom minimap border - %dx%d", customBorderW, customBorderH);
        return true;
    }

    void HudRenderer::DrawEdgeStripsPixels(SDL_Renderer* renderer, const SDL_FRect& box,
                                           float thickness) const
    {
        if (box.w <= 0.0f || thickness <= 0.0f) { return; }

        // Prefer the custom border when it is present - it was made for this box,
        // whereas the panel strip is the tracker's and is the wrong proportions
        // here (Edward, 2026).
        if (customBorder)
        {
            SDL_SetTextureAlphaMod(customBorder, static_cast<Uint8>(HUD_TRACKER_ALPHA));
            // Drawn at the thickness the caller asked for - which is the
            // tracker's own strip height - rather than the PNG's 5 pixels, so
            // the minimap's bars match the tracker's (Edward, 2026). The image
            // is stretched to fit, which is fine for a border of solid runs.
            float h = thickness;
            SDL_FRect top = SnapToPixels(box.x, box.y - h, box.w, h);
            SDL_RenderTextureRotated(renderer, customBorder, nullptr, &top, 0.0, nullptr, SDL_FLIP_VERTICAL);
            SDL_FRect bottom = SnapToPixels(box.x, box.y + box.h, box.w, h);
            SDL_RenderTexture(renderer, customBorder, nullptr, &bottom);
            SDL_SetTextureAlphaMod(customBorder, 255);
            return;
        }

        if (!sheet || rects.empty()) { return; }
        if (static_cast<size_t>(HUD_TRACKER_STRIP_DESCRIPTOR) >= rects.size()) { return; }

        const ALTEngine::Formats::BxRectangle& sr = rects[HUD_TRACKER_STRIP_DESCRIPTOR];
        SDL_FRect src{ static_cast<float>(sr.x), static_cast<float>(sr.y),
                       static_cast<float>(sr.width), static_cast<float>(sr.height) };

        // Top is the flipped copy; the artwork reads as the bottom edge.
        SDL_FRect top = SnapToPixels(box.x, box.y - thickness, box.w, thickness);
        SDL_RenderTextureRotated(renderer, sheet, &src, &top, 0.0, nullptr, SDL_FLIP_VERTICAL);

        SDL_FRect bottom = SnapToPixels(box.x, box.y + box.h, box.w, thickness);
        SDL_RenderTexture(renderer, sheet, &src, &bottom);
    }

    void HudRenderer::TickTracker()
    {
        HudTrackerAdvance(trackerFrame, trackerTimer, trackerPause);
    }

    void HudRenderer::DrawTracker(SDL_Renderer* renderer, const std::vector<Contact>& contacts,
                                  float playerYaw, int outputWidth, int outputHeight) const
    {
        if (!sheet || rects.empty()) { return; }
        if (outputWidth <= 0 || outputHeight <= 0) { return; }

        float scaleX = static_cast<float>(outputWidth) / static_cast<float>(HUD_VIRTUAL_WIDTH);
        float scaleY = static_cast<float>(outputHeight) / static_cast<float>(HUD_VIRTUAL_HEIGHT);

        int tile = HUD_TRACKER_FIRST_DESCRIPTOR + trackerFrame;
        if (static_cast<size_t>(tile) >= rects.size()) { return; }
        const ALTEngine::Formats::BxRectangle& r = rects[static_cast<size_t>(tile)];

        SDL_FRect src{ static_cast<float>(r.x), static_cast<float>(r.y),
                       static_cast<float>(r.width), static_cast<float>(r.height) };

        // Four quadrants from ONE tile, mirrored - which is what the original
        // does by swapping the quad's UVs rather than storing four copies. The
        // artwork tile is the top-right quarter.
        struct Quad { int x, y; SDL_FlipMode flip; };
        // The tile is the dish's TOP-LEFT quarter: its circle centre sits at its
        // own BOTTOM-RIGHT corner. So the unflipped tile goes top-left and the
        // other three mirror away from the dish centre.
        //
        // Both earlier guesses had the vertical flip inverted, which put the
        // circle centres at the top and bottom edges instead of in the middle
        // (Edward, 2026). The original achieves the same mirroring by exchanging
        // bytes between the quad's UV corner slots (+0x11/+0x19/+0x21/+0x29 in
        // FUN_0003a1d0) rather than by a flip flag; the result is equivalent.
        const Quad quads[4] = {
            { HUD_TRACKER_DISH_LEFT, HUD_TRACKER_DISH_TOP, SDL_FLIP_NONE },                      // top left
            { HUD_TRACKER_CENTRE_X, HUD_TRACKER_DISH_TOP, SDL_FLIP_HORIZONTAL },                 // top right
            { HUD_TRACKER_DISH_LEFT, HUD_TRACKER_CENTRE_Y, SDL_FLIP_VERTICAL },                  // bottom left
            { HUD_TRACKER_CENTRE_X, HUD_TRACKER_CENTRE_Y,
              static_cast<SDL_FlipMode>(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL) },              // bottom right
        };

        SDL_SetTextureAlphaMod(sheet, static_cast<Uint8>(HUD_TRACKER_ALPHA));

        for (const Quad& q : quads)
        {
            SDL_FRect dst = SnapToPixels(q.x * scaleX, q.y * scaleY,
                                         HUD_TRACKER_QUADRANT_W * scaleX,
                                         HUD_TRACKER_QUADRANT_H * scaleY);
            SDL_RenderTextureRotated(renderer, sheet, &src, &dst, 0.0, nullptr, q.flip);
        }

        // Edge strips, above and below, from one descriptor used twice.
        if (static_cast<size_t>(HUD_TRACKER_STRIP_DESCRIPTOR) < rects.size())
        {
            const ALTEngine::Formats::BxRectangle& sr = rects[HUD_TRACKER_STRIP_DESCRIPTOR];
            SDL_FRect strip{ static_cast<float>(sr.x), static_cast<float>(sr.y),
                             static_cast<float>(sr.width), static_cast<float>(sr.height) };
            int width = HUD_TRACKER_RIGHT - HUD_TRACKER_LEFT;

            // Top is the flipped copy; the artwork reads as the bottom edge.
            SDL_FRect top = SnapToPixels(HUD_TRACKER_LEFT * scaleX, HUD_TRACKER_TOP_STRIP_Y * scaleY,
                                         width * scaleX, sr.height * scaleY);
            SDL_RenderTextureRotated(renderer, sheet, &strip, &top, 0.0, nullptr, SDL_FLIP_VERTICAL);

            SDL_FRect bottom = SnapToPixels(HUD_TRACKER_LEFT * scaleX, HUD_TRACKER_BOTTOM_STRIP_Y * scaleY,
                                            width * scaleX, sr.height * scaleY);
            SDL_RenderTexture(renderer, sheet, &strip, &bottom);
        }

        SDL_SetTextureAlphaMod(sheet, 255);

        // Contacts. Range gate, world-to-cell shift and rotation by the player's
        // facing, all as FUN_0003a008 does - so the dish is always
        // player-forward.
        SDL_SetRenderDrawColor(renderer, 255, 127, 0, 255);
        float sinYaw = std::sin(-playerYaw);
        float cosYaw = std::cos(-playerYaw);

        for (const Contact& c : contacts)
        {
            if (std::fabs(c.dx) > HUD_TRACKER_RANGE || std::fabs(c.dz) > HUD_TRACKER_RANGE) { continue; }

            // >> 9 is the world-to-cell shift; the dish is 32 cells across its
            // 64-pixel height, so a cell is two pixels.
            float cellX = c.dx / 512.0f;
            float cellZ = c.dz / 512.0f;
            float rx = cellX * cosYaw - cellZ * sinYaw;
            float rz = cellX * sinYaw + cellZ * cosYaw;

            float px = HUD_TRACKER_CENTRE_X + rx;
            float pz = HUD_TRACKER_CENTRE_Y + rz;
            SDL_FRect blip = SnapToPixels(px * scaleX, pz * scaleY,
                                          HUD_TRACKER_BLIP_SIZE * scaleX,
                                          HUD_TRACKER_BLIP_SIZE * scaleY);
            SDL_RenderFillRect(renderer, &blip);
        }
    }
}
