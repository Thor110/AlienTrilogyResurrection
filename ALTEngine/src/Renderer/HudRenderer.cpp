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

        SDL_Log("HudRenderer: loaded %s - %dx%d page, %zu descriptors, %d px keyed transparent",
                path.string().c_str(), page.width, page.height, rects.size(), keyedCount);
        return true;
    }

    void HudRenderer::Unload()
    {
        if (sheet) { SDL_DestroyTexture(sheet); sheet = nullptr; }
        rects.clear();
        loadedFileIndex = -1;
    }

    void HudRenderer::DrawDescriptor(SDL_Renderer* renderer, int descriptorIndex, int x, int y,
                                     float scaleX, float scaleY) const
    {
        if (descriptorIndex < 0 || static_cast<size_t>(descriptorIndex) >= rects.size()) { return; }
        const ALTEngine::Formats::BxRectangle& r = rects[static_cast<size_t>(descriptorIndex)];

        SDL_FRect src{ static_cast<float>(r.x), static_cast<float>(r.y),
                       static_cast<float>(r.width), static_cast<float>(r.height) };
        SDL_FRect dst{ x * scaleX, y * scaleY, r.width * scaleX, r.height * scaleY };
        SDL_RenderTexture(renderer, sheet, &src, &dst);
    }

    void HudRenderer::DrawSheetRegion(SDL_Renderer* renderer, int srcX, int srcY, int w, int h,
                                      int dstX, int dstY, float scaleX, float scaleY) const
    {
        if (!sheet || w <= 0 || h <= 0) { return; }
        SDL_FRect src{ static_cast<float>(srcX), static_cast<float>(srcY),
                       static_cast<float>(w), static_cast<float>(h) };
        SDL_FRect dst{ dstX * scaleX, dstY * scaleY, w * scaleX, h * scaleY };
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
                cursor += rects[static_cast<size_t>(descriptor)].width;
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
            SDL_FRect dst{ x0 * scaleX, y0 * scaleY, (x1 - x0) * scaleX, (y1 - y0) * scaleY };
            SDL_RenderFillRect(renderer, &dst);
        };

        // Fills the lit slots of a bar. Slot positions come from the overlay
        // artwork (see HudPanel.h) rather than from the bar's own extent, so the
        // fill lands exactly in the transparent gaps the frame leaves. Drawn
        // full-height; the overlay on top clips each slot to its real height,
        // which is what produces the staircase.
        auto drawSlots = [&](int overlayX, int firstSlot, int litSlots, int y0, int y1,
                             const SDL_Color& c) {
            for (int i = 0; i < litSlots; ++i)
            {
                int x = overlayX + firstSlot + i * HUD_BAR_SLOT_PITCH;
                fillRect(x, y0, x + HUD_BAR_SLOT_WIDTH, y1, c);
            }
        };

        // ---- health row ----------------------------------------------------
        // Order: bar, then the frame artwork OVER it, then the number. The frame
        // is keyed transparent where the bar should show through, so it has to be
        // on top - it is what trims a pixel off the bar's corners and separates
        // the segments.
        drawSlots(HUD_OVERLAY_HEALTH_X, HUD_HEALTH_SLOT_FIRST,
                  HudHealthLitSlots(state.health),
                  HUD_OVERLAY_HEALTH_Y, HUD_OVERLAY_HEALTH_Y + HUD_OVERLAY_HEALTH_H,
                  BAR_FILL);

        DrawSheetRegion(renderer, HUD_OVERLAY_HEALTH_SRC_X, HUD_OVERLAY_HEALTH_SRC_Y,
                        HUD_OVERLAY_HEALTH_W, HUD_OVERLAY_HEALTH_H,
                        HUD_OVERLAY_HEALTH_X, HUD_OVERLAY_HEALTH_Y, scaleX, scaleY);

        DrawNumber(renderer, state.health, HUD_HEALTH_TEXT_X, HUD_HEALTH_ROW_TOP,
                   HUD_FONT_B_FIRST_DESCRIPTOR, scaleX, scaleY);

        // ---- ammo row ------------------------------------------------------
        int ammo = state.CurrentAmmoTotal();
        drawSlots(HUD_OVERLAY_AMMO_X, HUD_AMMO_SLOT_FIRST,
                  HudAmmoLitSlots(ammo),
                  HUD_OVERLAY_AMMO_Y, HUD_OVERLAY_AMMO_Y + HUD_OVERLAY_AMMO_H,
                  BAR_FILL);

        DrawSheetRegion(renderer, HUD_OVERLAY_AMMO_SRC_X, HUD_OVERLAY_AMMO_SRC_Y,
                        HUD_OVERLAY_AMMO_W, HUD_OVERLAY_AMMO_H,
                        HUD_OVERLAY_AMMO_X, HUD_OVERLAY_AMMO_Y, scaleX, scaleY);

        DrawNumber(renderer, ammo, HUD_OVERLAY_AMMO_X + HUD_AMMO_TEXT_INSET, HUD_AMMO_ROW_TOP,
                   HUD_FONT_FIRST_DESCRIPTOR, scaleX, scaleY);
    }
}
