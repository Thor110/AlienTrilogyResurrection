#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/LevelLoader.h"
#include "../Bootstrap/ModernSettings.h"
#include "../Renderer/ModelPreview.h"
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
        int ToGridSpaceX(float worldX, float originX) { return static_cast<int>(std::lround(worldX + originX)); }
        int ToGridSpaceZ(float worldZ, float originZ) { return static_cast<int>(std::lround(worldZ + originZ)); }

        // Can the player's footprint sit at this world position? Samples
        // the centre and the four corners of the +/-200 box: every point
        // must be non-blocking, and no point may rise more than the
        // step-up limit above the floor the player is currently on.
        bool CanOccupy(const ALTEngine::Formats::LevelGeometry& level,
                       float originX, float originZ,
                       float worldX, float worldZ, float currentFloorY)
        {
            constexpr float R = COLLISION_RADIUS;
            const float offsets[5][2] = { {0, 0}, {-R, -R}, {R, -R}, {-R, R}, {R, R} };
            for (const auto& o : offsets)
            {
                int gx = ToGridSpaceX(worldX + o[0], originX);
                int gz = ToGridSpaceZ(worldZ + o[1], originZ);
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
        // Grid space is the game's own coordinate system: cell =
        // coord >> 9, sub-cell = coord & 0x1ff. World space is grid
        // space shifted by this origin, which is the level's own
        // geometric origin - confirmed against the vertex bounds, where
        // grid 0 lands exactly on the minimum vertex coordinate on both
        // axes. Everything (player, crates, doors) goes through this one
        // transform, and the sub-cell offsets come from the game's own
        // per-entity formulas rather than being fudged in here.
        float originX = 0.0f;
        float originZ = 0.0f;

        // Live door state. Doors drive the collision grid directly: the
        // ceiling byte of the four cells they cover rises and falls with
        // the mesh, one height unit (32 world units) per tick, so
        // movement clearance and the visual stay in lockstep.
        struct DoorState
        {
            enum class Phase { Idle, Opening, Open, Closing };
            bool alongZ = false;
            int floorUnits = 0;
            int travel = 30;
            int progress = 0;            // 0 .. travel
            uint8_t threshold = 1;
            uint8_t unlockProgress = 0;
            // Consumed when the door starts opening. Without this a door
            // whose unlock progress is kept (staysUnlocked) re-opens the
            // instant it finishes closing, forever - reaching the threshold
            // is what unlocks it, but a fresh trigger is what opens it.
            // Doors opened by throwing a switch stay open; only doors you
            // open by walking up to them close behind you. NOT derived from
            // the disassembly - the dumped Open state has a single
            // distance check with no such distinction - so this is
            // behavioural until that is traced.
            bool switchOperated = false;
            // Unlock progress is reset when the door finishes closing in
            // both ordinary branches. Only doors with flag 0x40 keep it,
            // and those go through a cooldown counted in hold ticks.
            bool keepsUnlockOnClose = false;
            int holdTicks = 0;
            int cooldown = 0;
            Phase phase = Phase::Idle;
            std::array<size_t, 4> cells{};
            int cellCount = 0;
            size_t placedFirst = 0;
            int placedCount = 0;
            float worldX = 0.0f, worldZ = 0.0f, baseY = 0.0f;
        };
        std::vector<DoorState> doorStates;
        // Pickups spin in place. The original advances their angle by 0x10
        // per tick in a 4096-step space.
        std::vector<size_t> pickupPlaced;
        float pickupAngle = 0.0f;

        // Live pickups, for walk-over collection.
        struct LivePickup
        {
            size_t placedIndex = 0;
            int cellIndex = -1;
            uint8_t type = 0;
            uint8_t amount = 0;
            uint8_t multiplier = 0;
            bool collected = false;
        };
        std::vector<LivePickup> livePickups;
        // Switch/object state, indexed by object record. Non-zero means
        // already thrown - the original latches on this and fails, which
        // aborts the whole command chain rather than re-running it.
        std::vector<uint8_t> objectState;
        // Read once here rather than threaded through Run's already long
        // settings chain - worth tidying if more of these appear.
        bool autoOpenDoors = false;
        {
            ALTEngine::Bootstrap::Config modernConfig;
            ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
            autoOpenDoors = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::AutoOpenDoors);
        }
        int lastTriggerAction = 0;
        bool prevUseHeld = false;
        int lastCellIndex = -1;
        float tickAccumulator = 0.0f;

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
                    originX = static_cast<float>(level.header.mapLength) / 2.0f * 512.0f;
                    originZ = static_cast<float>(level.header.mapWidth) / 2.0f * 512.0f;

                    // Player start from the header. playerStartX/Y are
                    // cell coordinates - for L111 they are (39, 100)
                    // against a 92x105 grid, landing on a cell with no
                    // wall bytes and attribute 0. Placed with the same
                    // transform the crates use so the player shares one
                    // coordinate space with them.
                    camera.x = static_cast<float>(level.header.playerStartX) * 512.0f + 256.0f - originX;
                    camera.z = static_cast<float>(level.header.playerStartY) * 512.0f + 256.0f - originZ;
                    camera.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                                   level,
                                   ToGridSpaceX(camera.x, originX),
                                   ToGridSpaceZ(camera.z, originZ))
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
                        float worldX = static_cast<float>(crate.x) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originX;
                        float worldZ = static_cast<float>(crate.y) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originZ;
                        // "entities rest 32 units above the floor" -
                        // confirmed standing offset from the same
                        // decompilation, on top of the floor height itself.
                        // No standing offset: every OBJ3D mesh has its origin
                        // exactly at its base (vertex Y runs 0 upward), so the
                        // mesh sits on the floor when placed at the floor
                        // height itself. The +32 used for entities belongs to
                        // the monster spawn path, not to objects.
                        float floorY = ALTEngine::Formats::FindFloorHeight(level, crate.x, crate.y);

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
                        // Hardcoded in the original's object draw path as a
                        // 12.12 fixed-point triple (0xc00, 0xd98, 0xc00),
                        // applied to the matrix for every object type alike.
                        // A 512-unit crate becomes 384 in a 512 cell, which
                        // is where the gaps between adjacent crates come
                        // from. Doors do not go through this path.
                        placed.scaleX = 0.75f;
                        placed.scaleY = 0.849609375f;
                        placed.scaleZ = 0.75f;
                        placedObjects.push_back(placed);

                        // Occupancy, as the original's object spawn does:
                        // byte 12 takes the type, byte 4 the record index.
                        size_t crateCell = static_cast<size_t>(crate.y) * level.header.mapLength + static_cast<size_t>(crate.x);
                        if (crateCell < level.collisionGrid.size())
                        {
                            level.collisionGrid[crateCell].unknown13 = crate.type;
                            level.collisionGrid[crateCell].unknown5 = static_cast<uint8_t>(&crate - level.crates.data());
                        }
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

                        // Door travel distance in height units: 30 on
                        // the first two levels, 43 from level 2 onward.
                        int doorTravel = (digits == "111" || digits == "112") ? 30 : 43;

                        for (const auto& door : level.doors)
                        {
                            // Orientation 2 and 6 run along Z, 0 and 4 along
                            // X. 0 vs 4 and 2 vs 6 are 180 degrees apart and
                            // only affect the model's yaw.
                            bool alongZ = (door.rotation == 2 || door.rotation == 6);

                            // The mesh origin is the centre of the 4-cell
                            // span, not the centre of the anchor cell. The
                            // span runs from grid-1 to grid+2, so its centre
                            // lands on the boundary between grid and grid+1 -
                            // hence +512 along the door's own axis, and an
                            // ordinary +256 cell centre across it.
                            float gameX = static_cast<float>(door.x) * 512.0f + (alongZ ? 256.0f : 512.0f);
                            float gameZ = static_cast<float>(door.y) * 512.0f + (alongZ ? 512.0f : 256.0f);

                            float worldX = gameX - originX;
                            float worldZ = gameZ - originZ;

                            // Four covered cells, anchored one cell BACK
                            // along the door's axis.
                            int anchorX = static_cast<int>(door.x) - (alongZ ? 0 : 1);
                            int anchorZ = static_cast<int>(door.y) - (alongZ ? 1 : 0);

                            size_t ownCell = static_cast<size_t>(door.y) * level.header.mapLength + static_cast<size_t>(door.x);
                            int cellFloor = (ownCell < level.collisionGrid.size())
                                                ? level.collisionGrid[ownCell].floorHeight : 0;

                            // Flag 0x40 means the door starts open: its
                            // ceiling is already raised by the full travel
                            // distance and the mesh sits at that height.
                            bool startsOpen = (door.unknown & 0x40) != 0;
                            int ceilingUnits = startsOpen ? cellFloor + doorTravel : cellFloor;
                            float worldY = static_cast<float>(startsOpen ? ceilingUnits : cellFloor) * 32.0f + 16.0f;

                            for (int step = 0; step < 4; ++step)
                            {
                                int cx = anchorX + (alongZ ? 0 : step);
                                int cz = anchorZ + (alongZ ? step : 0);
                                if (cx < 0 || cz < 0 || cx >= level.header.mapLength || cz >= level.header.mapWidth) { continue; }
                                size_t ci = static_cast<size_t>(cz) * level.header.mapLength + static_cast<size_t>(cx);
                                if (ci >= level.collisionGrid.size()) { continue; }
                                level.collisionGrid[ci].floorHeight = static_cast<uint8_t>(cellFloor);
                                level.collisionGrid[ci].ceilingHeight = static_cast<uint8_t>(ceilingUnits);
                            }

                            DoorState ds;
                            ds.alongZ = alongZ;
                            ds.floorUnits = cellFloor;
                            ds.travel = doorTravel;
                            ds.progress = startsOpen ? doorTravel : 0;
                            ds.threshold = door.lockState == 0 ? 1 : door.lockState;
                            ds.unlockProgress = 0;
                            ds.keepsUnlockOnClose = (door.unknown & 0x40) != 0;
                            ds.holdTicks = static_cast<int>(door.time) * 4;
                            ds.phase = startsOpen ? DoorState::Phase::Open : DoorState::Phase::Idle;
                            ds.worldX = worldX;
                            ds.worldZ = worldZ;
                            ds.baseY = static_cast<float>(cellFloor) * 32.0f + 16.0f;
                            for (int step = 0; step < 4; ++step)
                            {
                                int cx = anchorX + (alongZ ? 0 : step);
                                int cz = anchorZ + (alongZ ? step : 0);
                                if (cx < 0 || cz < 0 || cx >= level.header.mapLength || cz >= level.header.mapWidth) { continue; }
                                size_t ci = static_cast<size_t>(cz) * level.header.mapLength + static_cast<size_t>(cx);
                                if (ci < level.collisionGrid.size()) { ds.cells[static_cast<size_t>(ds.cellCount++)] = ci; }
                            }
                            ds.placedFirst = placedObjects.size();
                            ds.placedCount = 5;
                            doorStates.push_back(ds);

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
                                placed.rotationRadians = 3.14159265f - static_cast<float>(door.rotation) * (3.14159265f / 4.0f);
                                placedObjects.push_back(placed);
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        SDL_Log("GameplayScreen: door setup failed: %s", e.what());
                    }

                    // Pickups. PICKMOD.BND lives in CD/GFX, not a SECT
                    // folder, and MissionBriefingScreen has already preloaded
                    // all 28 slots into the shared model cache by the time we
                    // get here - so this only needs to place them, using the
                    // same cache keys that preload used.
                    {
                        int spawned = 0;
                        for (const auto& pickup : level.pickups)
                        {
                            // Record byte 6 gates level-start spawning:
                            // non-zero means the pickup only appears once a
                            // script fires SpawnPickup.
                            if (pickup.z != 0) { continue; }

                            // Type 24 is remapped to 21 for both model and
                            // scale lookup.
                            int type = (pickup.type == 0x18) ? 0x15 : pickup.type;

                            ALTEngine::Renderer::PlacedObject placed;
                            placed.cacheKey = ALTEngine::Renderer::ModelPreviewSource::ForPickmod(cdDirectory, type).CacheKey();
                            placed.x = static_cast<float>(pickup.x) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originX;
                            placed.z = static_cast<float>(pickup.y) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originZ;

                            // Height offset table: 96 for type 20, 64 for
                            // everything else in the range we spawn.
                            float heightOffset = (type == 20) ? 96.0f : 64.0f;
                            placed.y = ALTEngine::Formats::FindFloorHeight(level, pickup.x, pickup.y) + heightOffset;

                            // Scale table, 12.12 fixed point, indexed by type.
                            // Only 16 rows exist, so higher types are clamped
                            // rather than read past the end.
                            static const float PICKUP_SCALE[16] = {
                                1.0f, 1.0f, 1.0f, 1.0f, 1.25f, 0.25f,
                                1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.75f, 1.75f
                            };
                            float scale = PICKUP_SCALE[type < 16 ? type : 15];
                            placed.scaleX = scale;
                            placed.scaleY = scale;
                            placed.scaleZ = scale;

                            LivePickup live;
                            live.placedIndex = placedObjects.size();
                            live.cellIndex = static_cast<int>(pickup.y) * level.header.mapLength + static_cast<int>(pickup.x);
                            live.type = pickup.type;
                            live.amount = pickup.amount;
                            live.multiplier = pickup.multiplier;
                            livePickups.push_back(live);

                            pickupPlaced.push_back(placedObjects.size());
                            placedObjects.push_back(placed);
                            spawned++;
                        }
                        SDL_Log("GameplayScreen: %d of %zu pickups placed (rest are script-spawned)",
                                spawned, level.pickups.size());
                        objectState.assign(level.crates.size(), 0);
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
                    // Re-read on return: the pause menu can reach the same
                    // Options screen the boot menu does, so this can have
                    // changed while paused. Reading it once at level load
                    // meant a mid-game toggle silently did nothing.
                    {
                        ALTEngine::Bootstrap::Config modernConfig;
                        ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
                        autoOpenDoors = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::AutoOpenDoors);
                    }

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
                            ToGridSpaceX(camera.x, originX),
                            ToGridSpaceZ(camera.z, originZ));

                        if (CanOccupy(level, originX, originZ, camera.x + stepX, camera.z, currentFloorY)) { camera.x += stepX; }
                        if (CanOccupy(level, originX, originZ, camera.x, camera.z + stepZ, currentFloorY)) { camera.z += stepZ; }
                    }
                    else
                    {
                        camera.x += stepX;
                        camera.z += stepZ;
                    }
                }

                // Triggers and door ticking.
                if (levelReady)
                {
                    // Runs one trigger's command chain. Shared by the
                    // automatic path (walking onto the cell) and the
                    // interact key, so both behave identically once fired.
                    // viaInteract distinguishes a deliberate use from simply
                    // walking onto the cell. Activate Object only succeeds on
                    // the former, which is what keeps the automatic-door
                    // option from also throwing switches.
                    auto runTriggerChain = [&](int action, bool viaInteract)
                    {
                        if (static_cast<size_t>(action) >= level.actions.size()) { return; }
                        const auto& trigger = level.actions[static_cast<size_t>(action)];
                        if ((trigger.activationMask & 0x01) == 0 || trigger.enable == 0) { return; }

                        int cmd = trigger.commandStart;
                        bool sawActivateObject = false;
                        std::vector<bool> visited(level.logics.size(), false);
                        while (cmd != 0xFF && static_cast<size_t>(cmd) < level.logics.size() && !visited[static_cast<size_t>(cmd)])
                        {
                            visited[static_cast<size_t>(cmd)] = true;
                            const auto& c = level.logics[static_cast<size_t>(cmd)];

                            // The chain is conditional: handlers return
                            // success and a failure aborts the remainder.
                            // That is how a level says "only if the switch
                            // actually worked". Commands we have not built
                            // yet must therefore stop the chain rather than
                            // fall through - otherwise a switch-operated
                            // door opens without its switch.
                            switch (c.action)
                            {
                            case 1: // Unlock Door
                            case 6: // Open Door
                                if (c.objectIndex >= doorStates.size()) { cmd = 0xFF; break; }
                                else
                                {
                                    DoorState& ds = doorStates[c.objectIndex];

                                    // Both fail (and abort the chain) if the
                                    // door is already fully unlocked or busy.
                                    if (ds.phase != DoorState::Phase::Idle || ds.unlockProgress >= ds.threshold)
                                    {
                                        cmd = 0xFF;
                                        break;
                                    }

                                    // Open Door additionally requires this to
                                    // be the final increment: a door needing
                                    // two activations cannot be opened by its
                                    // own trigger until something else - a
                                    // switch - has supplied the first one.
                                    if (c.action == 6 && ds.unlockProgress < ds.threshold - 1)
                                    {
                                        cmd = 0xFF;
                                        break;
                                    }

                                    if (sawActivateObject) { ds.switchOperated = true; }
                                    ds.unlockProgress = static_cast<uint8_t>(ds.unlockProgress + c.modifier);

                                }
                                break;
                            case 8: // End Level
                                SDL_Log("GameplayScreen: EndLevel trigger fired (action %d)", action);
                                break;
                            case 0: // Toggle Light
                            case 2: // Spawn Pickup
                            case 3: // Spawn Monster
                            case 9: // Change Texture
                                // Do not set the success flag in the
                                // original either, so continuing is correct.
                                break;
                            case 4: // Activate Object
                            {
                                // Pressing use on the cell is what activates
                                // the switch. Walking onto it is not.
                                if (!viaInteract) { cmd = 0xFF; break; }

                                size_t oi = c.objectIndex;
                                if (oi >= level.crates.size() || oi >= objectState.size()) { cmd = 0xFF; break; }

                                // Latches: a thrown switch fails, and the
                                // chain aborts on the failure.
                                if (objectState[oi] != 0) { cmd = 0xFF; break; }

                                // Types 22 and 26 need a battery and consume
                                // one; type 24 is exempt.
                                uint8_t type = level.crates[oi].type;
                                if (type == 22 || type == 26)
                                {
                                    if (!inventory.HasBatteries())
                                    {
                                        SDL_Log("GameplayScreen: switch %zu needs a battery", oi);
                                        cmd = 0xFF;
                                        break;
                                    }
                                    inventory.batteries--;
                                }

                                objectState[oi] = 1;
                                sawActivateObject = true;
                                break;
                            }
                            default:
                                // Lift commands (5, 7) also gate the chain on
                                // their result. Unimplemented, so stop here.
                                cmd = 0xFF;
                                break;
                            }
                            if (cmd == 0xFF) { break; }
                            cmd = c.nextStep;
                        }
                    };

                    int pgx = ToGridSpaceX(camera.x, originX);
                    int pgz = ToGridSpaceZ(camera.z, originZ);
                    int cellX = pgx >> 9, cellZ = pgz >> 9;
                    int cellIndex = -1;
                    if (cellX >= 0 && cellZ >= 0 && cellX < level.header.mapLength && cellZ < level.header.mapWidth)
                    {
                        cellIndex = cellZ * level.header.mapLength + cellX;
                    }

                    // Only evaluated when the player changes cell, and then
                    // only when the new cell's action byte differs from the
                    // last one acted on - standing still fires nothing, and
                    // stepping onto a plain cell clears the latch so the
                    // same trigger can fire again on re-entry.
                    if (cellIndex != lastCellIndex)
                    {
                        // Occupancy: byte 12 holds the occupier's type, 0 when
                        // free, and is written on cell change rather than every
                        // tick. 0xFF is used for the player - the real value is
                        // unknown, and anything non-zero outside the monster
                        // (1-19) and object (0x14-0x21) ranges behaves the same
                        // for the only test that reads it.
                        if (lastCellIndex >= 0 && static_cast<size_t>(lastCellIndex) < level.collisionGrid.size())
                        {
                            level.collisionGrid[static_cast<size_t>(lastCellIndex)].unknown13 = 0;
                        }
                        if (cellIndex >= 0 && static_cast<size_t>(cellIndex) < level.collisionGrid.size())
                        {
                            level.collisionGrid[static_cast<size_t>(cellIndex)].unknown13 = 0xFF;
                        }
                        lastCellIndex = cellIndex;
                        int action = 0;
                        if (cellIndex >= 0 && static_cast<size_t>(cellIndex) < level.collisionGrid.size())
                        {
                            action = level.collisionGrid[static_cast<size_t>(cellIndex)].scriptAction;
                        }

                        if (action == 0) { lastTriggerAction = 0; }
                        else if (autoOpenDoors && action != lastTriggerAction)
                        {
                            lastTriggerAction = action;
                            runTriggerChain(action, false);
                        }
                    }

                    // Walk-over collection. Applied on whichever cell the
                    // player is standing in, checked every frame rather than
                    // only on cell change so a pickup spawned underfoot by a
                    // script is still picked up.
                    for (auto& live : livePickups)
                    {
                        if (live.collected || live.cellIndex != cellIndex) { continue; }
                        live.collected = true;
                        if (live.placedIndex < placedObjects.size()) { placedObjects[live.placedIndex].visible = false; }

                        int amount = static_cast<int>(live.amount) * (live.multiplier == 0 ? 1 : live.multiplier);
                        switch (live.type)
                        {
                        case 0:  inventory.pistol.available = true;       inventory.pistol.ammo += amount;       break;
                        case 1:  inventory.shotgun.available = true;      inventory.shotgun.ammo += amount;      break;
                        case 2:  inventory.pulseRifle.available = true;   inventory.pulseRifle.ammo += amount;   break;
                        case 3:  inventory.flamethrower.available = true; inventory.flamethrower.ammo += amount; break;
                        case 4:  inventory.smartGun.available = true;     inventory.smartGun.ammo += amount;     break;
                        case 7:  inventory.batteries += (amount > 0 ? amount : 1); break;
                        case 9:  inventory.pistol.ammo += amount;       break;
                        case 10: inventory.shotgun.ammo += amount;      break;
                        case 11: inventory.pulseRifle.ammo += amount;   break;
                        case 13: inventory.flamethrower.ammo += amount; break;
                        case 14: inventory.smartGun.ammo += amount;     break;
                        case 16: inventory.hasAutoMapper = true;  break;
                        case 25: inventory.hasShoulderLamp = true; break;
                        default: break; // health, armour and the rest need player stats we do not have yet
                        }
                        SDL_Log("GameplayScreen: collected pickup type %d (amount %d)", live.type, amount);
                    }

                    // Interact key - the original's way of opening a door.
                    // Fires on the press edge for whatever trigger cell the
                    // player is standing on, independent of the automatic
                    // path's latch, so it works whether or not Automatic
                    // Doors is enabled and can be pressed repeatedly.
                    bool useHeld = keys[keyBindings.GetKey(InputAction::Use)];
                    if (useHeld && !prevUseHeld)
                    {
                        // The original tests only the cell the player stands
                        // in - confirmed, there is no neighbourhood or facing
                        // test anywhere in the trigger paths. We additionally
                        // test the cell directly ahead, so a door can be used
                        // from the side its trigger cells are not on. That is
                        // a deliberate departure, limited to the interact key:
                        // one cell cannot reach through a wall.
                        // Two cells ahead, not one: a door occupies a cell of
                        // its own, so from the far side the first cell ahead
                        // is the door itself and its trigger sits beyond that.
                        int probes[3] = { cellIndex, -1, -1 };
                        for (int step = 1; step <= 2; ++step)
                        {
                            int ax = (pgx + static_cast<int>(forwardX * 512.0f * step)) >> 9;
                            int az = (pgz + static_cast<int>(forwardZ * 512.0f * step)) >> 9;
                            if (ax >= 0 && az >= 0 && ax < level.header.mapLength && az < level.header.mapWidth)
                            {
                                probes[step] = az * level.header.mapLength + ax;
                            }
                        }

                        for (int probe : probes)
                        {
                            if (probe < 0 || static_cast<size_t>(probe) >= level.collisionGrid.size()) { continue; }
                            int useAction = level.collisionGrid[static_cast<size_t>(probe)].scriptAction;
                            if (useAction != 0) { runTriggerChain(useAction, true); break; }
                        }
                    }
                    prevUseHeld = useHeld;

                    // Fixed 60 Hz tick - the original moves doors one
                    // height unit per tick, and quantising matters: the
                    // 32-unit step is what keeps the step-up test and
                    // clearance behaving.
                    tickAccumulator += dt;
                    while (tickAccumulator >= 1.0f / 60.0f)
                    {
                        tickAccumulator -= 1.0f / 60.0f;
                        // 0x10 of a 4096-step turn per tick.
                        pickupAngle += 6.28318531f * 16.0f / 4096.0f;
                        for (size_t pi : pickupPlaced)
                        {
                            if (pi < placedObjects.size()) { placedObjects[pi].rotationRadians = pickupAngle; }
                        }

                        for (auto& ds : doorStates)
                        {
                            switch (ds.phase)
                            {
                            case DoorState::Phase::Idle:
                                // Re-checked every tick against the counter
                                // alone - the original needs no fresh trigger
                                // here, because closing resets the counter.
                                if (ds.cooldown > 0) { ds.cooldown--; }
                                else if (ds.unlockProgress >= ds.threshold) { ds.phase = DoorState::Phase::Opening; }
                                break;
                            case DoorState::Phase::Opening:
                                ds.progress++;
                                if (ds.progress >= ds.travel) { ds.progress = ds.travel; ds.phase = DoorState::Phase::Open; }
                                break;
                            case DoorState::Phase::Open:
                            {
                                // Measured across the doorway, not along it:
                                // a door running along Z is walked through
                                // in X, so that is the axis that matters.
                                float d = ds.alongZ ? (ds.worldX - camera.x) : (ds.worldZ - camera.z);
                                if (d < 0) { d = -d; }
                                if (ds.switchOperated) { break; }
                                bool occupied = false;
                                for (int i = 0; i < ds.cellCount; ++i)
                                {
                                    if (level.collisionGrid[ds.cells[static_cast<size_t>(i)]].unknown13 != 0) { occupied = true; break; }
                                }
                                if (d > 1024.0f && !occupied) { ds.phase = DoorState::Phase::Closing; }
                                break;
                            }
                            case DoorState::Phase::Closing:
                                ds.progress--;
                                if (ds.progress <= 0)
                                {
                                    ds.progress = 0;
                                    ds.phase = DoorState::Phase::Idle;
                                    if (ds.keepsUnlockOnClose) { ds.cooldown = ds.holdTicks; }
                                    else { ds.unlockProgress = 0; }
                                }
                                break;
                            }

                            // Push the current height into both the grid and
                            // the mesh, so collision and visuals agree.
                            uint8_t ceilingUnits = static_cast<uint8_t>(ds.floorUnits + ds.progress);
                            for (int i = 0; i < ds.cellCount; ++i)
                            {
                                level.collisionGrid[ds.cells[static_cast<size_t>(i)]].ceilingHeight = ceilingUnits;
                            }
                            float y = ds.baseY + static_cast<float>(ds.progress) * 32.0f;
                            for (int i = 0; i < ds.placedCount; ++i)
                            {
                                size_t pi = ds.placedFirst + static_cast<size_t>(i);
                                if (pi < placedObjects.size()) { placedObjects[pi].y = y; }
                            }
                        }
                    }
                }

                // Follow the floor. No gravity or fall handling yet - the
                // camera is simply pinned to the surface underfoot.
                if (levelReady)
                {
                    float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(
                        level,
                        ToGridSpaceX(camera.x, originX),
                        ToGridSpaceZ(camera.z, originZ));
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
