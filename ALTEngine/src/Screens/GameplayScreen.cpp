#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/LevelLoader.h"
#include "../Formats/ModelIndices.h"
#include "../Formats/ModelLoader.h"
#include "../Formats/BndTextureLoader.h"
#include "../Formats/Obj3DTexture.h"
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
        // TODO: still a guess. The real value lives at DAT_000b0a64 in
        // the original and could not be found statically - it is written
        // through an unresolved struct pointer. It can be recovered
        // empirically instead: the view-bob phase advances by
        // (speed * 3) >> 1 per tick and a footstep fires each time it
        // crosses 0x800, so matching footstep cadence against a recording
        // of the original solves for speed directly.
        constexpr float MOVE_SPEED = 2000.0f;   // world units/sec
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

        // Crate::type -> OBJ3D model index, per AlienTrilogyMapLoader.cs's
        // own documented type table (Edward, 2026). Not every type has an
        // obvious single-model match yet (the various switch/locker
        // types need their own dedicated models, not attempted here) -
        // covers the clearest, most common cases and falls back to
        // SingleCrate for anything else, so an unrecognised type still
        // spawns *something* visible and positioned rather than nothing.
        int Obj3DIndexForCrateType(uint8_t type)
        {
            using namespace ALTEngine::Formats::ModelIndices;
            switch (type)
            {
            case 23: return Obj3D::ExplosiveBarrel;  // "barrel explodes"
            case 25: return Obj3D::DoubleCrate;       // "double stacked boxes"
            case 33: return Obj3D::SteelCoil;
            // Switches (Edward, 2026: "there are supposed to be switches
            // in the level in a few places, currently they are crates,
            // if I recall that was just for testing") - per
            // AlienTrilogyMapLoader.cs's own type comments: 22/24 are
            // small switches, 26/27 are wide switches that require a
            // battery. Crate carries no field for which light-state
            // variant (red-left/red-right/both-off/both-yellow) a given
            // instance should actually show, so these pick one
            // representative model per family - the switch SHAPE is now
            // correct, the exact light state shown is still a
            // placeholder until that's resolved (likely needs the
            // level's door/lock state, not anything in the Crate record
            // itself).
            case 22: return Obj3D::SwitchBothLightsOff;             // "another small switch"
            case 24: return Obj3D::SwitchBothLightsOff;             // "switch with animation (small switch)"
            case 26: return Obj3D::LargeSwitchBatteryBothLightsOff; // "wide switch with zipper" - battery required
            case 27: return Obj3D::LargeSwitchBatteryBothLightsOff; // "wide switch without zipper" - battery required
            default: return Obj3D::SingleCrate;       // 20/21/28/29/32/>37 and anything else - "a regular box", still a placeholder for these
            }
        }

        // Player collision constants, taken from the game's own entity
        // mover (Ghidra: FUN_00031afc / FUN_000315f0).
        constexpr float COLLISION_RADIUS = 200.0f; // 400-unit footprint, sampled at -r, centre, +r
        constexpr float MAX_STEP_UP = 256.0f;      // a rise steeper than this blocks movement
        constexpr float EYE_HEIGHT = 768.0f;       // camera sits this far above the player's feet
        constexpr float STAND_OFFSET = 32.0f;      // feet sit this far above the floor

        // World space -> grid space, where cell = coord >> 9. Inverts the
        // crate placement used above, so player and objects agree.
        int ToGridSpaceX(float worldX, float centerPosX) { return static_cast<int>(std::lround(worldX - 512.0f * centerPosX)); }
        int ToGridSpaceZ(float worldZ, float centerPosZ) { return static_cast<int>(std::lround(worldZ + 512.0f * centerPosZ)); }

        // Can the player's footprint sit at this world position? Samples
        // the centre and the four corners of the +/-200 box: every point
        // must be non-blocking, and no point may rise more than the
        // step-up limit above the floor the player is currently on.
        bool CanOccupy(const ALTEngine::Formats::LevelGeometry& level,
                       float centerPosX, float centerPosZ,
                       float worldX, float worldZ, float currentFloorY)
        {
            constexpr float R = COLLISION_RADIUS;
            const float offsets[5][2] = { {0, 0}, {-R, -R}, {R, -R}, {-R, R}, {R, R} };
            for (const auto& o : offsets)
            {
                int gx = ToGridSpaceX(worldX + o[0], centerPosX);
                int gz = ToGridSpaceZ(worldZ + o[1], centerPosZ);
                if (ALTEngine::Formats::IsCellBlocking(level, gx, gz)) { return false; }
                float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                if (floorY - currentFloorY > MAX_STEP_UP) { return false; }
            }
            return true;
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
        Bootstrap::Language& language,
        const std::string& missionLevelCode,
        Bootstrap::KeyBindings& keyBindings,
        Bootstrap::AudioSettings& audioSettings,
        Bootstrap::RenderSettings& renderSettings,
        Bootstrap::ResolutionSettings& resolutionSettings,
        Bootstrap::DifficultySettings& difficultySettings,
        Bootstrap::CameraSwaySettings& cameraSwaySettings,
        Bootstrap::LanguageSettings& languageSettings)
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
        ALTEngine::Formats::LevelGeometry level;
        std::vector<ALTEngine::Renderer::PlacedObject> placedObjects;
        // World <-> grid-space transform. Grid space is what the game
        // itself uses: cell = coord >> 9, sub-cell = coord & 0x1ff.
        // Derived from the confirmed crate placement below, so the two
        // stay consistent by construction.
        float centerPosX = 0.0f;
        float centerPosZ = 0.0f;

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
                    level = ALTEngine::Formats::LevelLoader::Load(*mapPath);

                    // Grid centring, needed by both the player spawn and
                    // the object placement below.
                    centerPosX = -static_cast<float>(level.header.mapLength) / 2.0f;
                    if (level.header.mapLength % 2 == 0) { centerPosX += 0.5f; }
                    centerPosZ = static_cast<float>(level.header.mapWidth) / 2.0f;
                    if (level.header.mapWidth % 2 == 0) { centerPosZ -= 0.5f; }

                    // Player start from the header. playerStartX/Y are
                    // cell coordinates - for L111 they are (39, 100)
                    // against a 92x105 grid, landing on a cell with no
                    // wall bytes and attribute 0. Placed with the same
                    // transform the crates use so the player shares one
                    // coordinate space with them.
                    camera.x = 512.0f * (centerPosX + static_cast<float>(level.header.playerStartX));
                    camera.z = 512.0f * (static_cast<float>(level.header.playerStartY) - centerPosZ) + 256.0f;
                    camera.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                                   level,
                                   ToGridSpaceX(camera.x, centerPosX),
                                   ToGridSpaceZ(camera.z, centerPosZ))
                               + STAND_OFFSET + EYE_HEIGHT;

                    // playerStartAngle is 2 for L111, which fits the same
                    // 8-step facing scheme the crates use (0=N, 2=E, 4=S,
                    // 6=W) rather than the 4096-step space used for
                    // entity angles elsewhere. Mirrored the same way crate
                    // facing is. Unverified - position can be checked
                    // independently of which way you end up looking.
                    camera.yaw = 3.14159265f - static_cast<float>(level.header.playerStartAngle) * (3.14159265f / 4.0f);

                    // Object spawning (Edward, 2026: "all of the level
                    // objects spawning") - crates/barrels/switches first,
                    // since they map cleanly onto OBJ3D via
                    // Obj3DIndexForCrateType above. Monsters/pickups/
                    // doors/lifts are a separate, later step - pickups
                    // and monsters need PICKMOD models and AI/animation
                    // state that doesn't exist yet, and doors/lifts need
                    // open/close animation state this static placement
                    // approach doesn't cover.
                    std::vector<ALTEngine::Renderer::PreloadRequest> crateRequests;
                    for (int meshNumber : { ALTEngine::Formats::ModelIndices::Obj3D::ExplosiveBarrel,
                                             ALTEngine::Formats::ModelIndices::Obj3D::SingleCrate,
                                             ALTEngine::Formats::ModelIndices::Obj3D::DoubleCrate,
                                             ALTEngine::Formats::ModelIndices::Obj3D::SteelCoil,
                                             ALTEngine::Formats::ModelIndices::Obj3D::SwitchBothLightsOff,
                                             ALTEngine::Formats::ModelIndices::Obj3D::LargeSwitchBatteryBothLightsOff })
                    {
                        auto obj3dPath = FindInSectFolders(cdDirectory, "OBJ3D.BND");
                        auto texPath = ALTEngine::Formats::ResolveObj3DTextureFile(cdDirectory, meshNumber, language);
                        if (!obj3dPath.has_value() || !texPath.has_value()) { continue; }

                        ALTEngine::Renderer::PreloadRequest req;
                        req.cacheKey = { ALTEngine::Renderer::ModelCatalog::Obj3d, meshNumber };
                        req.meshNumber = meshNumber;
                        req.objBndPath = *obj3dPath;
                        req.gfxBndPath = *texPath;
                        crateRequests.push_back(req);
                    }
                    ModelRenderer::PreloadBatch(crateRequests);

                    // Map-space -> world-space. crate.x/crate.y are GRID
                    // CELL indices (0-255ish), not raw vertex-space
                    // units - AlienTrilogyMapLoader.cs's own
                    // scalingFactor (1/512f) is applied to the MAP MESH
                    // to shrink it down to Unity's scale, while spawned
                    // objects use raw grid coordinates unscaled; that
                    // only lines up because 1 grid cell = 512 raw
                    // vertex-space units. This renderer keeps the level
                    // mesh at its true raw scale instead of shrinking it,
                    // so grid coordinates need scaling UP by that same
                    // 512 factor (Edward, 2026: "all the objects
                    // currently spawn in a single location").
                    //
                    // That alone isn't enough, though - the C# reference
                    // ALSO re-centres the map mesh itself, via
                    // `child.transform.localPosition = new(-mapLength/2
                    // [+.5 if even], 2, mapWidth/2 [-.5 if even])`
                    // (BuildMapMesh), so that the shrunk mesh lines up
                    // with the unscaled grid-coordinate objects. This
                    // renderer's level mesh has no such offset applied to
                    // it (it's placed at its true, unshifted vertex-space
                    // position) - solving Unity's own placement equation
                    // for what raw-vertex-space offset achieves the same
                    // alignment gives posX/posZ below (Edward, 2026: "yes
                    // they all spawn, but far in the distance" - without
                    // this, objects land entirely outside the level's own
                    // geometry rather than merely offset within it,
                    // confirmed against real L111LEV.MAP data).
                    constexpr float GRID_CELL_TO_WORLD_UNITS = 512.0f;

                    // Vertical position via the CONFIRMED formula from a
                    // full Ghidra decompilation of the real game's own
                    // GetFloorHeight (Edward, 2026: floorHeight * 32,
                    // adjusted by the `attribute` byte for ramps/stairs,
                    // evaluated at the exact cell corner since that's
                    // where the real spawn code places entities). This
                    // replaces an earlier quad-geometry search entirely -
                    // that was a heuristic guessing at something the real
                    // game reads from one byte plus a known scale; this is
                    // the actual formula. See LevelLoader::FindFloorHeight.
                    for (const auto& crate : level.crates)
                    {
                        // Sub-cell placement, in world units (one cell =
                        // 512). X takes no offset, Z takes half a cell -
                        // both established against a known-good
                        // reference crate.
                        constexpr float HALF_CELL = GRID_CELL_TO_WORLD_UNITS * 0.5f;
                        float worldX = GRID_CELL_TO_WORLD_UNITS * (centerPosX + static_cast<float>(crate.x));
                        float worldZ = GRID_CELL_TO_WORLD_UNITS * (static_cast<float>(crate.y) - centerPosZ) + HALF_CELL;
                        // "entities rest 32 units above the floor" -
                        // confirmed standing offset from the same
                        // decompilation, on top of the floor height itself.
                        float floorY = ALTEngine::Formats::FindFloorHeight(level, crate.x, crate.y) + 32.0f;

                        ALTEngine::Renderer::PlacedObject placed;
                        placed.cacheKey = { ALTEngine::Renderer::ModelCatalog::Obj3d, Obj3DIndexForCrateType(crate.type) };
                        placed.x = worldX;
                        placed.y = floorY;
                        placed.z = worldZ;
                        // Facing byte: 0=N, 2=E, 4=S, 6=W (45 degrees per
                        // step). Mirrored about the E-W axis - angle =
                        // PI - step - because our world Z runs opposite
                        // to the coordinate frame those bytes were
                        // authored in. That swaps N and S while leaving
                        // E and W alone, which is what the level data
                        // shows: the rot=2 (East) wide switch was already
                        // correct, both rot=0 (North) small switches were
                        // facing the wall.
                        placed.rotationRadians = 3.14159265f - static_cast<float>(crate.rotation) * (3.14159265f / 4.0f);
                        placedObjects.push_back(placed);
                    }

                    // Doors. Their meshes live in this same .MAP under
                    // "D" sections, textured from the level's own pages,
                    // and can span more than one page - so each page
                    // becomes its own cache entry and its own PlacedObject
                    // at the same transform. Placed closed; the opening
                    // state machine is not implemented yet.
                    try
                    {
                        auto doorMeshes = ALTEngine::Formats::ModelLoader::Load(*mapPath, "D");
                        auto levelTextures = ALTEngine::Formats::BndTextureLoader::Load(*gfxPath);

                        for (size_t m = 0; m < doorMeshes.size(); ++m)
                        {
                            auto groups = ALTEngine::Formats::BuildRenderMeshPerGroup(doorMeshes[m], levelTextures.uvRects);
                            for (size_t page = 0; page < groups.size(); ++page)
                            {
                                if (groups[page].indices.empty()) { continue; }
                                if (page >= levelTextures.textures.size()) { continue; }
                                ALTEngine::Renderer::ModelCacheKey key{
                                    ALTEngine::Renderer::ModelCatalog::LevelDoor,
                                    static_cast<int>(m) * 8 + static_cast<int>(page) };
                                ModelRenderer::RegisterMesh(key, groups[page], levelTextures.textures[page]);
                            }
                        }

                        for (const auto& door : level.doors)
                        {
                            size_t cellIndex = static_cast<size_t>(door.y) * level.header.mapLength + static_cast<size_t>(door.x);
                            int cellFloor = (cellIndex < level.collisionGrid.size())
                                                ? level.collisionGrid[cellIndex].floorHeight : 0;

                            float worldX = GRID_CELL_TO_WORLD_UNITS * (centerPosX + static_cast<float>(door.x));
                            float worldZ = GRID_CELL_TO_WORLD_UNITS * (static_cast<float>(door.y) - centerPosZ) + GRID_CELL_TO_WORLD_UNITS * 0.5f;
                            // Closed height: floorHeight * 32 + 16.
                            float worldY = static_cast<float>(cellFloor) * 32.0f + 16.0f;

                            for (int page = 0; page < 5; ++page)
                            {
                                ALTEngine::Renderer::ModelCacheKey key{
                                    ALTEngine::Renderer::ModelCatalog::LevelDoor,
                                    static_cast<int>(door.modelIndex) * 8 + page };

                                ALTEngine::Renderer::PlacedObject placed;
                                placed.cacheKey = key;
                                placed.x = worldX;
                                placed.y = worldY;
                                placed.z = worldZ;
                                // Same facing convention as the crates.
                                placed.rotationRadians = 3.14159265f - static_cast<float>(door.rotation) * (3.14159265f / 4.0f);
                                placedObjects.push_back(placed);
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        SDL_Log("GameplayScreen: door setup failed: %s", e.what());
                    }
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
                    PauseMenuResult pauseResult = PauseMenuScreen::Run(cdDirectory, language, missionLevelCode, inventory, audioSettings,
                                                                        renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings, keyBindings);
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
                    float stepX = (moveX / moveLen) * MOVE_SPEED * dt;
                    float stepZ = (moveZ / moveLen) * MOVE_SPEED * dt;

                    // Per-axis movement, matching the game's separate X
                    // and Z movers - moving each axis independently is
                    // what lets you slide along a wall instead of
                    // sticking to it.
                    if (levelReady)
                    {
                        float currentFloorY = ALTEngine::Formats::FindFloorHeightGridSpace(
                            level,
                            ToGridSpaceX(camera.x, centerPosX),
                            ToGridSpaceZ(camera.z, centerPosZ));

                        if (CanOccupy(level, centerPosX, centerPosZ, camera.x + stepX, camera.z, currentFloorY)) { camera.x += stepX; }
                        if (CanOccupy(level, centerPosX, centerPosZ, camera.x, camera.z + stepZ, currentFloorY)) { camera.z += stepZ; }
                    }
                    else
                    {
                        camera.x += stepX;
                        camera.z += stepZ;
                    }
                }

                // Follow the floor. No gravity or fall handling yet - the
                // camera is simply pinned to the surface underfoot.
                if (levelReady)
                {
                    float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(
                        level,
                        ToGridSpaceX(camera.x, centerPosX),
                        ToGridSpaceZ(camera.z, centerPosZ));
                    camera.y = floorY + STAND_OFFSET + EYE_HEIGHT;
                }
            }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            if (levelReady)
            {
                std::vector<uint8_t> pixels = ModelRenderer::RenderLevelToRgba(cacheKey, camera, windowW, windowH, placedObjects);
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
