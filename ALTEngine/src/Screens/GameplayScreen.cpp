#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/LevelLoader.h"
#include "../Renderer/ModelRenderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace ALTEngine::Screens
{
    using ALTEngine::Bootstrap::AppWindow;
    using ALTEngine::Bootstrap::Color;
    using ALTEngine::Bootstrap::ComputeMenuScale;
    using ALTEngine::Bootstrap::DrawBitmapText;
    using ALTEngine::Renderer::FpsCamera;
    using ALTEngine::Renderer::ModelRenderer;

    namespace
    {
        constexpr float MOVE_SPEED = 2000.0f;   // world units/sec - a guess, matching the level's own coordinate scale (vertices span tens of thousands of units)
        constexpr float MOUSE_SENSITIVITY = 0.0025f; // radians per pixel of mouse delta - a starting guess, not tuned against real hardware yet
        constexpr float MAX_PITCH = 1.4f;       // just under 90 degrees, avoids the view flipping past vertical

        // "1.1.1" -> "111" - confirmed against the real filename
        // (L111LEV.MAP). Not confirmed whether every level code follows
        // this exact digit-concatenation pattern, only the one example
        // available.
        std::string LevelDigitsFromCode(const std::string& levelCode)
        {
            std::string digits;
            for (char c : levelCode) { if (c >= '0' && c <= '9') { digits += c; } }
            return digits;
        }

        // Level files live in disc-sector folders (SECT11, SECT12, etc -
        // confirmed from DiscFileManifest.json), NOT the generic "GFX"
        // folder most other assets use, and the mapping from level code
        // to which SECT folder isn't a clean formula (SECT11 alone
        // covers 1.1.1 through 1.3.1). Rather than hardcode that
        // mapping, search every SECT* folder for the expected filename -
        // more robust to manifest details than a lookup table.
        std::optional<std::filesystem::path> FindInSectFolders(const std::filesystem::path& cdDirectory, const std::string& filename)
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(cdDirectory, ec))
            {
                if (!entry.is_directory()) { continue; }
                std::string name = entry.path().filename().string();
                if (name.rfind("SECT", 0) != 0) { continue; }

                std::filesystem::path candidate = entry.path() / filename;
                std::error_code existsEc;
                if (std::filesystem::exists(candidate, existsEc)) { return candidate; }
            }
            return std::nullopt;
        }

        void ComputeVertexBounds(const ALTEngine::Formats::LevelGeometry& level, float& outMinY, float& outCenterX, float& outCenterY, float& outCenterZ)
        {
            float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
            float minY = minX, maxY = maxX, minZ = minX, maxZ = maxX;
            for (const auto& v : level.vertices)
            {
                minX = std::min(minX, static_cast<float>(v.x)); maxX = std::max(maxX, static_cast<float>(v.x));
                minY = std::min(minY, static_cast<float>(v.y)); maxY = std::max(maxY, static_cast<float>(v.y));
                minZ = std::min(minZ, static_cast<float>(v.z)); maxZ = std::max(maxZ, static_cast<float>(v.z));
            }
            outMinY = minY;
            outCenterX = (minX + maxX) / 2.0f;
            outCenterY = (minY + maxY) / 2.0f;
            outCenterZ = (minZ + maxZ) / 2.0f;
        }
    }

    GameplayResult GameplayScreen::Run(
        const std::filesystem::path& cdDirectory,
        Bootstrap::Language language,
        const std::string& missionLevelCode,
        Bootstrap::KeyBindings& keyBindings,
        Bootstrap::AudioSettings& audioSettings)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { GameplayOutcome::WindowClosed };
        }
        SDL_Renderer* renderer = app.Renderer();

        PlayerInventoryState inventory;

        // Resolve and load the level
        std::string digits = LevelDigitsFromCode(missionLevelCode);
        auto mapPath = FindInSectFolders(cdDirectory, "L" + digits + "LEV.MAP");
        auto gfxPath = FindInSectFolders(cdDirectory, digits + "GFX.B16");

        bool levelReady = false;
        std::string cacheKey = "LEVEL:" + digits;
        FpsCamera camera;

        if (mapPath.has_value() && gfxPath.has_value())
        {
            if (ModelRenderer::Initialize())
            {
                levelReady = ModelRenderer::LoadLevel(cacheKey, *mapPath, *gfxPath);
            }

            if (levelReady)
            {
                try
                {
                    auto level = ALTEngine::Formats::LevelLoader::Load(*mapPath);
                    float minY, cx, cy, cz;
                    ComputeVertexBounds(level, minY, cx, cy, cz);
                    // Placeholder start position - see the header's doc
                    // comment on the playerStartX/Y coordinate-system
                    // question. Uses the level's own geometric center at
                    // a reasonable height above its lowest point, not the
                    // header's playerStartX/Y fields.
                    camera.x = cx;
                    camera.z = cz;
                    camera.y = minY + 300.0f;
                }
                catch (const std::exception& e)
                {
                    SDL_Log("GameplayScreen: failed to read level header for player start: %s", e.what());
                }
            }
        }
        else
        {
            SDL_Log("GameplayScreen: could not find level files for '%s' (looked for L%sLEV.MAP / %sGFX.B16 under CD/SECT*)",
                    missionLevelCode.c_str(), digits.c_str(), digits.c_str());
        }

        GameplayResult result;
        bool running = true;
        Uint64 lastTicks = SDL_GetTicks();

        // Mouse look (Edward, 2026: "mouse look rather than arrow keys
        // for the camera control") needs relative mode - captures and
        // hides the cursor, and SDL_GetRelativeMouseState then reports
        // motion as per-frame deltas rather than absolute position.
        // Only while there's actually something to look around at;
        // released again before the pause menu (a keyboard-navigated
        // menu has no use for a captured cursor) and re-captured after
        // returning if gameplay is still running.
        if (levelReady) { SDL_SetWindowRelativeMouseMode(app.Window(), true); }

        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT) { result.outcome = GameplayOutcome::WindowClosed; running = false; }
                else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == keyBindings.GetKey(ALTEngine::Bootstrap::InputAction::Pause))
                {
                    SDL_SetWindowRelativeMouseMode(app.Window(), false);
                    PauseMenuResult pauseResult = PauseMenuScreen::Run(cdDirectory, language, missionLevelCode, inventory, audioSettings);
                    if (pauseResult.outcome == PauseMenuOutcome::WindowClosed)
                    {
                        result.outcome = GameplayOutcome::WindowClosed;
                        running = false;
                    }
                    else if (pauseResult.outcome == PauseMenuOutcome::ExitGame)
                    {
                        result.outcome = GameplayOutcome::ExitGame;
                        running = false;
                    }
                    else if (levelReady)
                    {
                        SDL_SetWindowRelativeMouseMode(app.Window(), true);
                        float discardDx = 0.0f, discardDy = 0.0f;
                        SDL_GetRelativeMouseState(&discardDx, &discardDy); // drain any stale delta before the next real frame
                    }
                    lastTicks = SDL_GetTicks(); // don't count time spent in the pause menu as a movement frame
                }
            }
            if (!running) { break; }

            Uint64 nowTicks = SDL_GetTicks();
            float dt = static_cast<float>(nowTicks - lastTicks) / 1000.0f;
            lastTicks = nowTicks;
            dt = std::min(dt, 0.1f); // clamp so a stall/hitch doesn't teleport the camera

            if (levelReady)
            {
                const bool* keys = SDL_GetKeyboardState(nullptr);

                float mouseDx = 0.0f, mouseDy = 0.0f;
                SDL_GetRelativeMouseState(&mouseDx, &mouseDy);
                // Read live rather than cached once - lets the Controls
                // > Mouse > Mouse Sensitivity slider take effect
                // immediately without needing a restart (Edward, 2026).
                // 5/10 (the slider's own default) maps to exactly
                // MOUSE_SENSITIVITY, so this changes nothing for anyone
                // who hasn't touched the setting.
                float sensitivity = MOUSE_SENSITIVITY * (static_cast<float>(keyBindings.MouseSensitivity()) / 5.0f);
                camera.yaw += mouseDx * sensitivity;
                camera.pitch = std::clamp(camera.pitch - mouseDy * sensitivity, -MAX_PITCH, MAX_PITCH);

                // Ground-plane movement (X/Z only) relative to yaw -
                // matches typical FPS convention of not flying up/down
                // just from looking up/down.
                float forwardX = std::sin(camera.yaw);
                float forwardZ = -std::cos(camera.yaw);
                float rightX = std::cos(camera.yaw);
                float rightZ = std::sin(camera.yaw);

                using ALTEngine::Bootstrap::InputAction;
                float moveX = 0, moveZ = 0;
                if (keys[keyBindings.GetKey(InputAction::MoveForward)]) { moveX += forwardX; moveZ += forwardZ; }
                if (keys[keyBindings.GetKey(InputAction::MoveBackward)]) { moveX -= forwardX; moveZ -= forwardZ; }
                if (keys[keyBindings.GetKey(InputAction::StrafeRight)]) { moveX += rightX; moveZ += rightZ; }
                if (keys[keyBindings.GetKey(InputAction::StrafeLeft)]) { moveX -= rightX; moveZ -= rightZ; }

                float moveLen = std::sqrt(moveX * moveX + moveZ * moveZ);
                if (moveLen > 0.0001f)
                {
                    camera.x += (moveX / moveLen) * MOVE_SPEED * dt;
                    camera.z += (moveZ / moveLen) * MOVE_SPEED * dt;
                }
            }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            if (levelReady)
            {
                std::vector<uint8_t> pixels = ModelRenderer::RenderLevelToRgba(cacheKey, camera, windowW, windowH);
                if (!pixels.empty())
                {
                    SDL_Texture* frameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, windowW, windowH);
                    if (frameTexture)
                    {
                        SDL_UpdateTexture(frameTexture, nullptr, pixels.data(), windowW * 4);
                        SDL_RenderTexture(renderer, frameTexture, nullptr, nullptr);
                        SDL_DestroyTexture(frameTexture);
                    }
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderClear(renderer);
                }
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                int scale = ComputeMenuScale(renderer);
                DrawBitmapText(renderer, "(level failed to load - see console)", scale * 8, scale * 8, scale, Color{ 220, 40, 40, 255 });
            }

            SDL_RenderPresent(renderer);
        }

        SDL_SetWindowRelativeMouseMode(app.Window(), false);
        ModelRenderer::UnloadLevels();
        return result;
    }
}
