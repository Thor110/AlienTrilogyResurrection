#include "MissionAssessmentScreen.h"
#include "MenuBackground.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Strings.h"
#include "../Formats/BndTextureLoader.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace ALTEngine::Screens
{
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::StringId;

    namespace
    {
        constexpr int SCREEN_W = 320;
        constexpr int SCREEN_H = 240;
        constexpr int TITLE_Y = 0x18;
        constexpr int PROMPT_Y = 0xd2;
        // Modern: Level Timer. Its line goes under the last bar (which ends at
        // 0xc2) and the prompt moves down to the screen's last 16 pixels.
        constexpr int TIMES_Y = 0xc6;
        constexpr int TIMES_PROMPT_Y = 0xdf;
        constexpr int LABEL_X = 0x1a;              // 0x10 for the third language (FUN_00012a80)
        constexpr int FIGURE_X = 0x6e;
        constexpr int BAR_X = 0x64;
        constexpr int ROW_LABEL_Y[3] = { 0x46, 0x79, 0xac };
        constexpr int ROW_FIGURE_Y[3] = { 0x3e, 0x71, 0xa4 };
        constexpr int ROW_BAR_Y[3] = { 0x3a, 0x6d, 0xa0 };
        constexpr int BAR_FRAME_DESCRIPTOR = 118;
        constexpr int SEGMENT_HEIGHT = 0x22;
        constexpr int SEGMENT_WIDTHS[10] = { 18, 18, 17, 17, 16, 16, 16, 16, 16, 15 };   // 0xa655c
        constexpr int SEGMENT_FIRST_COLOUR = 0xc2;  // +3 a segment, +1 every 5 pixels
        constexpr int TICK_SOUND = 0x27;
        constexpr double FRAME_HZ = 1193182.0 / 17109.0 / 2.0;   // the original's design point, 34.87

        struct Font
        {
            SDL_Texture* texture = nullptr;
            std::vector<ALTEngine::Formats::BxRectangle> rects;
        };

        Font LoadFont(const std::filesystem::path& cdDirectory, SDL_Renderer* renderer)
        {
            Font font;
            for (const char* ext : { ".BND", ".B16", ".16" })
            {
                const auto path = cdDirectory / "GFX" / (std::string("FONT1GFX") + ext);
                std::error_code ec;
                if (!std::filesystem::exists(path, ec)) { continue; }
                try
                {
                    // Palette index 0 is the key, as the original's fills test.
                    auto set = ALTEngine::Formats::BndTextureLoader::Load(path, std::nullopt, { { 0 } });
                    if (set.textures.empty()) { break; }
                    const auto& page = set.textures[0];
                    font.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, page.width, page.height);
                    if (font.texture)
                    {
                        SDL_UpdateTexture(font.texture, nullptr, page.rgba.data(), page.width * 4);
                        SDL_SetTextureScaleMode(font.texture, SDL_SCALEMODE_NEAREST);
                        SDL_SetTextureBlendMode(font.texture, SDL_BLENDMODE_BLEND);
                    }
                    font.rects = set.uvRects;
                }
                catch (const std::exception& e) { SDL_Log("MissionAssessmentScreen: FONT1GFX: %s", e.what()); }
                break;
            }
            if (!font.texture) { SDL_Log("MissionAssessmentScreen: FONT1GFX not found"); }
            return font;
        }

        // The original's colour bytes have 0x80 as full brightness.
        SDL_Color Tint(MissionAssessmentScreen::Rgb c)
        {
            auto up = [](uint8_t v) { return static_cast<uint8_t>(std::min(255, v * 2)); };
            return SDL_Color{ up(c.r), up(c.g), up(c.b), 255 };
        }

        // A descriptor's rect is u0..u1 inclusive, but the original's quads
        // span u1 - u0 (FUN_00018680), so the last row and column are not
        // drawn; drawing them shows bits of the neighbouring glyph. The bar
        // frame is drawn a pixel larger each way (FUN_00012858), so it keeps
        // the whole rect.
        void DrawDescriptor(SDL_Renderer* renderer, const Font& font, int index, float x, float y, SDL_Color tint, bool whole = false)
        {
            if (!font.texture || index < 0 || static_cast<size_t>(index) >= font.rects.size()) { return; }
            const auto& r = font.rects[static_cast<size_t>(index)];
            const float w = float(r.width - (whole ? 0 : 1));
            const float h = float(r.height - (whole ? 0 : 1));
            SDL_FRect src{ float(r.x), float(r.y), w, h };
            SDL_FRect dst{ x, y, w, h };
            SDL_SetTextureColorMod(font.texture, tint.r, tint.g, tint.b);
            SDL_RenderTexture(renderer, font.texture, &src, &dst);
        }

        // FUN_000188b0 / FUN_000187f4: a glyph is descriptor (char - 0x20),
        // the next one starts at its width less one, a space is 8 wide.
        int TextWidth(const Font& font, const std::string& text)
        {
            int w = 0;
            for (unsigned char ch : text)
            {
                if (ch == ' ') { w += 8; continue; }
                const int d = int(ch) - 0x20;
                if (d >= 0 && static_cast<size_t>(d) < font.rects.size()) { w += font.rects[static_cast<size_t>(d)].width - 1; }
            }
            return w;
        }

        void DrawText(SDL_Renderer* renderer, const Font& font, const std::string& text, int x, int y, SDL_Color tint)
        {
            for (unsigned char ch : text)
            {
                if (ch == ' ') { x += 8; continue; }
                const int d = int(ch) - 0x20;
                if (d < 0 || static_cast<size_t>(d) >= font.rects.size()) { continue; }
                DrawDescriptor(renderer, font, d, float(x), float(y), tint);
                x += font.rects[static_cast<size_t>(d)].width - 1;
            }
        }

        void DrawCentred(SDL_Renderer* renderer, const Font& font, const std::string& text, int y, SDL_Color tint)
        {
            DrawText(renderer, font, text, (SCREEN_W - TextWidth(font, text)) / 2, y, tint);
        }

        // FUN_00012628 and the "%" after it.
        std::string Figure(int value) { return std::to_string(std::clamp(value, 0, 9999)) + "%"; }

        // The flat fill (0x4e53c): each column takes screen-palette entry
        // (segment colour + column / 5).
        void DrawSegments(SDL_Renderer* renderer, int value, int y, const std::array<SDL_Color, 256>& palette)
        {
            int x = BAR_X + 5;
            int left = value;
            for (int s = 0; s < 10 && left > 0; ++s, left -= 10)
            {
                const int w = left < 10 ? (left * 10 * SEGMENT_WIDTHS[s]) / 100 : SEGMENT_WIDTHS[s];
                const int base = SEGMENT_FIRST_COLOUR + 3 * s;
                for (int col = 0; col < w; ++col)
                {
                    const SDL_Color c = palette[static_cast<size_t>(std::min(255, base + col / 5))];
                    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                    SDL_FRect r{ float(x + col), float(y), 1.0f, float(SEGMENT_HEIGHT) };
                    SDL_RenderFillRect(renderer, &r);
                }
                x += w;
            }
        }

        // The fill reads the game's working palette (0x361cf0), which still
        // holds the level's colours: FUN_0004f2dc copies the first 0x1aa bytes
        // of PALS\LEVnnn.PAL (loaded to 0x361360 via the table at 0xd846) into
        // entries 0x72-0xff, so entry 0xc2 is the file's 80th colour. The fill
        // halves each byte onto the 0x80-is-full scale, so on screen the byte
        // is the brightness. Whether the file holds 8-bit or 6-bit values is
        // not confirmed here, so a file with nothing above 63 is taken as
        // 6-bit. With no usable file the segments fall back to a red-to-green
        // ramp.
        std::array<SDL_Color, 256> LoadSegmentPalette(const std::filesystem::path& cdDirectory, const std::string& digits)
        {
            constexpr size_t FIRST = 0x72;
            constexpr size_t BYTES = 0x1aa;
            std::array<SDL_Color, 256> pal{};
            std::vector<uint8_t> raw;
            if (!digits.empty())
            {
                const auto path = cdDirectory / "PALS" / ("LEV" + digits + ".PAL");
                SDL_IOStream* io = SDL_IOFromFile(path.string().c_str(), "rb");
                if (io)
                {
                    raw.resize(BYTES);
                    raw.resize(SDL_ReadIO(io, raw.data(), BYTES));
                    SDL_CloseIO(io);
                }
            }
            const size_t usedFrom = (SEGMENT_FIRST_COLOUR - FIRST) * 3;
            if (raw.size() >= usedFrom + 3 * 32)
            {
                const bool sixBit = std::all_of(raw.begin(), raw.end(), [](uint8_t v) { return v <= 63; });
                for (size_t i = 0; i * 3 + 2 < raw.size() && FIRST + i < 256; ++i)
                {
                    auto c = [&](size_t k) { return static_cast<uint8_t>(sixBit ? raw[k] * 255 / 63 : raw[k]); };
                    pal[FIRST + i] = SDL_Color{ c(i * 3), c(i * 3 + 1), c(i * 3 + 2), 255 };
                }
                return pal;
            }
            SDL_Log("MissionAssessmentScreen: no usable PALS/LEV%s.PAL, bar segments use a stand-in ramp", digits.c_str());
            for (int i = 0; i < 256; ++i)
            {
                const float t = std::clamp((i - SEGMENT_FIRST_COLOUR) / 32.0f, 0.0f, 1.0f);
                pal[static_cast<size_t>(i)] = SDL_Color{ uint8_t(220 * (1.0f - t)), uint8_t(60 + 160 * t), 30, 255 };
            }
            return pal;
        }

        std::string TrOr(StringId id, Bootstrap::Language language, const char* fallback)
        {
            std::string s = Bootstrap::Tr(id, language);
            return s.empty() ? std::string(fallback) : s;
        }
    }

    MissionAssessmentResult MissionAssessmentScreen::Run(const std::filesystem::path& cdDirectory,
                                                         const MissionAssessmentStats& stats,
                                                         Bootstrap::Language language)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated()) { return { true }; }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);
        Font font = LoadFont(cdDirectory, renderer);
        const auto segmentPalette = LoadSegmentPalette(cdDirectory, stats.levelDigits);
        const auto colours = FigureColours(stats.requiredPercent);

        SDL_Texture* canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, SCREEN_W, SCREEN_H);
        if (canvas) { SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST); }

        const std::string title = TrOr(StringId::GsMissionAssessment, language, "MISSION ASSESSMENT");
        const std::array<std::string, 3> labels{
            TrOr(StringId::GsAliens, language, "Aliens"),
            TrOr(StringId::GsSecrets, language, "Secrets"),
            TrOr(StringId::GsMissionLabel, language, "Mission") };
        const std::string prompt = TrOr(StringId::GsPressAnyKey, language, "Press any key");
        const std::string times = stats.showTimes
            ? TrOr(StringId::AssessmentTime, language, "Time") + " " + FormatRunTime(stats.levelSeconds) + "  "
              + TrOr(StringId::AssessmentRun, language, "Run") + " " + FormatRunTime(stats.runSeconds)
            : std::string();
        const int labelX = (language == Bootstrap::Language::Spanish) ? 0x10 : LABEL_X;

        const std::array<int, 3> target{
            std::clamp(int(stats.aliensPercent + 0.5f), 0, 100),
            std::clamp(int(stats.secretsPercent + 0.5f), 0, 100),
            std::clamp(int(stats.missionPercent + 0.5f), 0, 100) };
        std::array<int, 3> shown{ 0, 0, 0 };
        unsigned tickCount = 0;
        bool counted = false;

        MissionAssessmentResult result;
        bool running = true;
        Uint64 last = SDL_GetTicks();
        double frameClock = 0.0;
        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if ((event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    // A press while the figures count jumps every row to its
                    // final value (Edward, 2026); the next press leaves.
                    if (counted) { running = false; }
                    else { shown = target; counted = true; }
                }
            }
            if (!running) { break; }

            // One count a frame of the original, the rows in turn.
            const Uint64 now = SDL_GetTicks();
            frameClock += double(now - last) / 1000.0;
            last = now;
            while (frameClock >= 1.0 / FRAME_HZ && !counted)
            {
                frameClock -= 1.0 / FRAME_HZ;
                int row = 0;
                while (row < 3 && shown[size_t(row)] >= target[size_t(row)]) { ++row; }
                if (row == 3) { counted = true; break; }
                shown[size_t(row)]++;
                if ((++tickCount & 7u) == 0) { ALTEngine::Audio::SfxPlayer::PlaySlot(TICK_SOUND, "111", cdDirectory); }
            }
            if (counted) { frameClock = 0.0; }

            if (canvas) { SDL_SetRenderTarget(renderer, canvas); }
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (background) { SDL_RenderTexture(renderer, background, nullptr, nullptr); }

            const SDL_Color white{ 255, 255, 255, 255 };
            DrawCentred(renderer, font, title, TITLE_Y, white);
            for (int i = 0; i < 3; ++i)
            {
                const SDL_Color c = Tint(colours[size_t(std::clamp(shown[size_t(i)], 0, 100))]);
                DrawSegments(renderer, shown[size_t(i)], ROW_BAR_Y[i], segmentPalette);
                DrawDescriptor(renderer, font, BAR_FRAME_DESCRIPTOR, float(BAR_X), float(ROW_BAR_Y[i]), white, true);
                DrawText(renderer, font, Figure(shown[size_t(i)]), FIGURE_X, ROW_FIGURE_Y[i], c);
                DrawText(renderer, font, labels[size_t(i)], labelX, ROW_LABEL_Y[i], c);
            }
            if (stats.showTimes) { DrawCentred(renderer, font, times, TIMES_Y, white); }
            if (counted) { DrawCentred(renderer, font, prompt, stats.showTimes ? TIMES_PROMPT_Y : PROMPT_Y, white); }

            if (canvas)
            {
                SDL_SetRenderTarget(renderer, nullptr);
                DrawMenuBackground(renderer, canvas, SCREEN_W, SCREEN_H);
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(1);
        }

        if (canvas) { SDL_DestroyTexture(canvas); }
        if (font.texture) { SDL_DestroyTexture(font.texture); }
        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
