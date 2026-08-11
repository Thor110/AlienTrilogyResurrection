#include "HudRenderer.h"

#include <algorithm>
#include <cctype>
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
        constexpr SDL_Color BAR_FILL{ 176, 216, 176, 255 };
        constexpr SDL_Color BAR_FRAME{ 24, 24, 16, 255 };

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
            int top = -1, bottom = -1;
            for (int y = 1; y < frame.height - 1; ++y)
            {
                if (!clearAt(x, y)) { continue; }
                if (top < 0)
                {
                    if (clearAt(x, y - 1)) { continue; }   // not the start of a notch
                    top = y;
                }
                bottom = y;
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

        SDL_FRect src{ static_cast<float>(r.x), static_cast<float>(r.y),
                       static_cast<float>(r.width), static_cast<float>(r.height) };
        SDL_FRect dst = SnapToPixels(x * scaleX, y * scaleY, r.width * scaleX, r.height * scaleY);
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

        DrawNumber(renderer, state.health, HUD_HEALTH_TEXT_X, HUD_HEALTH_ROW_TOP,
                   HUD_FONT_B_FIRST_DESCRIPTOR, scaleX, scaleY);

        // ---- ammo row ------------------------------------------------------
        int ammo = state.CurrentAmmoTotal();
        drawSlots(ammoSlots, HUD_OVERLAY_AMMO_X + ammoFrame.insetX,
                  HUD_OVERLAY_AMMO_Y + ammoFrame.insetY,
                  HudAmmoLitSlots(ammo), BAR_FILL);

        DrawFrame(renderer, ammoFrame, HUD_OVERLAY_AMMO_X, HUD_OVERLAY_AMMO_Y, scaleX, scaleY);

        DrawNumber(renderer, ammo, HUD_AMMO_TEXT_X, HUD_AMMO_ROW_TOP,
                   HUD_FONT_FIRST_DESCRIPTOR, scaleX, scaleY);
    }
}
