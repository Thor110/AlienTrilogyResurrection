#include "MissionAssessmentScreen.h"
#include "MenuBackground.h"
#include "../Audio/SfxPlayer.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>

namespace ALTEngine::Screens
{
    using ALTEngine::Audio::SfxId;
    using ALTEngine::Audio::SfxPlayer;
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Bootstrap::TextHeight;
    using ALTEngine::Bootstrap::TextWidth;

    namespace
    {
        constexpr Color COLOR_TITLE{ 24, 130, 52, 255 };
        constexpr Color COLOR_LABEL{ 51, 255, 102, 255 };
        constexpr Color COLOR_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_HOLLOW{ 40, 40, 40, 255 };
        constexpr Color COLOR_READY{ 51, 255, 102, 255 };

        constexpr int BAR_SEGMENTS = 30;
        constexpr float SECONDS_PER_ROW = 1.5f; // how long each row's count-up animation takes

        // Red -> orange -> yellow -> green, matching the reference bar's
        // gradient across its full width - a segment's colour depends on
        // its POSITION along the bar (0=left/low, 1=right/high), not on
        // the current percentage value.
        Color GradientColor(float t)
        {
            struct Stop { float t; Color c; };
            static const std::array<Stop, 4> stops{ {
                { 0.0f, Color{ 220, 40, 40, 255 } },
                { 0.33f, Color{ 230, 140, 30, 255 } },
                { 0.66f, Color{ 220, 220, 30, 255 } },
                { 1.0f, Color{ 51, 255, 102, 255 } },
            } };
            t = std::clamp(t, 0.0f, 1.0f);
            for (size_t i = 0; i + 1 < stops.size(); ++i)
            {
                if (t <= stops[i + 1].t)
                {
                    float span = stops[i + 1].t - stops[i].t;
                    float local = span > 0 ? (t - stops[i].t) / span : 0.0f;
                    auto lerp = [&](uint8_t a, uint8_t b) { return static_cast<uint8_t>(a + (b - a) * local); };
                    return Color{ lerp(stops[i].c.r, stops[i + 1].c.r), lerp(stops[i].c.g, stops[i + 1].c.g), lerp(stops[i].c.b, stops[i + 1].c.b), 255 };
                }
            }
            return stops.back().c;
        }

        void DrawStatRow(SDL_Renderer* renderer, const std::string& label, float currentPercent, int x, int y, int barX, int barWidth, int scale)
        {
            Color labelColor = GradientColor(currentPercent / 100.0f);
            DrawBitmapText(renderer, label, x, y, scale, COLOR_LABEL);

            char pctBuf[8];
            std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(currentPercent));
            DrawBitmapText(renderer, pctBuf, barX, y, scale, labelColor);

            int barY = y + TextHeight(scale) + scale * 3;
            int segGap = 2;
            int segWidth = (barWidth - (BAR_SEGMENTS - 1) * segGap) / BAR_SEGMENTS;
            int segHeight = scale * 6;
            int filledSegments = static_cast<int>((currentPercent / 100.0f) * BAR_SEGMENTS);

            for (int i = 0; i < BAR_SEGMENTS; ++i)
            {
                int segX = barX + i * (segWidth + segGap);
                SDL_FRect rect{ static_cast<float>(segX), static_cast<float>(barY), static_cast<float>(segWidth), static_cast<float>(segHeight) };
                if (i < filledSegments)
                {
                    Color c = GradientColor(static_cast<float>(i) / (BAR_SEGMENTS - 1));
                    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
                    SDL_RenderFillRect(renderer, &rect);
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, COLOR_HOLLOW.r, COLOR_HOLLOW.g, COLOR_HOLLOW.b, 255);
                    SDL_RenderRect(renderer, &rect);
                }
            }
        }
    }

    MissionAssessmentResult MissionAssessmentScreen::Run(const std::filesystem::path& cdDirectory, const MissionAssessmentStats& stats)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { true };
        }
        SDL_Renderer* renderer = app.Renderer();

        int bgW = 0, bgH = 0;
        SDL_Texture* background = LoadMenuBackground(cdDirectory, renderer, 1, bgW, bgH);

        std::array<float, 3> finalValues{ stats.aliensPercent, stats.secretsPercent, stats.missionPercent };
        std::array<const char*, 3> labels{ "Aliens:", "Secrets:", "Mission:" };

        Uint64 startTicks = SDL_GetTicks();
        bool fullyRevealed = false;

        MissionAssessmentResult result;
        bool running = true;

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.windowClosed = true; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                {
                    if (!fullyRevealed) { fullyRevealed = true; }
                    else { running = false; }
                }
            }
            if (!running) { break; }

            float elapsedSeconds = static_cast<float>(SDL_GetTicks() - startTicks) / 1000.0f;

            std::array<float, 3> current{};
            for (int i = 0; i < 3; ++i)
            {
                if (fullyRevealed) { current[static_cast<size_t>(i)] = finalValues[static_cast<size_t>(i)]; continue; }
                float rowStart = SECONDS_PER_ROW * i;
                float rowElapsed = std::clamp(elapsedSeconds - rowStart, 0.0f, SECONDS_PER_ROW);
                float rowProgress = rowElapsed / SECONDS_PER_ROW;
                current[static_cast<size_t>(i)] = finalValues[static_cast<size_t>(i)] * rowProgress;
            }
            if (elapsedSeconds >= SECONDS_PER_ROW * 3) { fullyRevealed = true; }

            DrawMenuBackground(renderer, background, bgW, bgH);

            int scale = 3;
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            std::string title = "MISSION ASSESSMENT";
            DrawBitmapText(renderer, title, (windowW - TextWidth(title, scale)) / 2, scale * 8, scale, COLOR_TITLE);

            int margin = scale * 8;
            int rowSpacing = 130;
            int barX = margin + 180;
            int barWidth = windowW - barX - margin;
            for (int i = 0; i < 3; ++i)
            {
                int y = scale * 8 + 80 + i * rowSpacing;
                DrawStatRow(renderer, labels[static_cast<size_t>(i)], current[static_cast<size_t>(i)], margin, y, barX, barWidth, scale);
            }

            if (fullyRevealed)
            {
                std::string prompt = "Press any key to continue";
                DrawBitmapText(renderer, prompt, (windowW - TextWidth(prompt, scale)) / 2, windowH - scale * 20, scale, COLOR_READY);
            }

            SDL_RenderPresent(renderer);
        }

        if (background) { SDL_DestroyTexture(background); }
        return result;
    }
}
