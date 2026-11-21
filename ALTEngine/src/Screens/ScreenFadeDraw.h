#pragma once

#include "../Formats/ScreenFade.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <functional>

namespace ALTEngine::Screens
{
    // The screen fades (Formats::ScreenFade) on a screen drawn in 2D: its
    // pictures and text are neutral (0x80) primitives in the original, so the
    // whole frame is multiplied by what a neutral colour comes out as. Over
    // the 3D view, where each face has a light of its own, the fade is put on
    // each colour instead (ModelRenderer::SetScreenFade).
    namespace ScreenFadeDraw
    {
        namespace SF = ALTEngine::Formats::ScreenFade;

        // Over everything drawn so far this frame.
        inline void Overlay(SDL_Renderer* renderer, const SF::State& fade)
        {
            if (SF::Identity(fade)) { return; }
            const float factor = SF::NeutralFactor(fade);
            const int v = static_cast<int>(std::lround(std::fmin(std::fmax(factor, 0.0f), 1.0f) * 255.0f));
            SDL_BlendMode previousMode = SDL_BLENDMODE_NONE;
            Uint8 pr = 0, pg = 0, pb = 0, pa = 0;
            SDL_GetRenderDrawBlendMode(renderer, &previousMode);
            SDL_GetRenderDrawColor(renderer, &pr, &pg, &pb, &pa);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MUL);
            SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(v), static_cast<Uint8>(v), static_cast<Uint8>(v), 255);
            SDL_RenderFillRect(renderer, nullptr);
            SDL_SetRenderDrawBlendMode(renderer, previousMode);
            SDL_SetRenderDrawColor(renderer, pr, pg, pb, pa);
        }

        // The fade's own clock. The original steps once a drawn frame, at most
        // 34.87 times a second: here one step a frame at most, once 1/34.87 s
        // has passed - so a slow frame (the first one of a level) shows each
        // step rather than skipping the fade.
        struct Clock
        {
            Uint64 last = 0;
            double pending = 0.0;

            void Start() { last = SDL_GetTicks(); pending = 0.0; }
            void Advance(SF::State& fade)
            {
                const Uint64 now = SDL_GetTicks();
                pending += static_cast<double>(now - last) / 1000.0;
                last = now;
                constexpr double PERIOD = 1.0 / SF::STEP_HZ;
                if (pending >= PERIOD)
                {
                    pending -= PERIOD;
                    if (pending > PERIOD) { pending = PERIOD; }
                    if (SF::Busy(fade)) { SF::Step(fade); }
                }
                if (!SF::Busy(fade)) { pending = 0.0; }
            }
        };

        // FUN_0004eefc and the loop the original runs on it (`while
        // (DAT_000ad846 != 0)`): the screen's last frame drawn again, darker
        // each step, until the fade is done. False if the window was closed.
        inline bool RunFadeOut(SDL_Renderer* renderer, SF::State& fade, const std::function<void()>& draw)
        {
            SF::StartOut(fade);
            Clock clock;
            clock.Start();
            while (SF::Busy(fade))
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_EVENT_QUIT) { return false; }
                }
                draw();
                Overlay(renderer, fade);
                SDL_RenderPresent(renderer);
                SDL_Delay(1);
                clock.Advance(fade);
            }
            return true;
        }

        // Black for `steps` frames (the 30 passes FUN_00014f44 waits between
        // the assessment and the prompt). False if the window was closed.
        inline bool HoldBlack(SDL_Renderer* renderer, int steps)
        {
            const Uint64 start = SDL_GetTicks();
            const double seconds = static_cast<double>(steps) / SF::STEP_HZ;
            while (static_cast<double>(SDL_GetTicks() - start) / 1000.0 < seconds)
            {
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_EVENT_QUIT) { return false; }
                }
                SDL_SetRenderTarget(renderer, nullptr);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderPresent(renderer);
                SDL_Delay(1);
            }
            return true;
        }
    }
}
