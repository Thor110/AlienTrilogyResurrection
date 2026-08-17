#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Formats/LevelLoader.h"
#include "PlayerHudState.h"
#include "PlayerCamera.h"
#include "WeaponSystem.h"
#include "ParticleSystem.h"
#include "ExplosionEffects.h"
#include "ShatterEffects.h"
#include "DamageSystem.h"
#include "../Audio/SfxPlayer.h"
#include "../Formats/SoundIds.h"
#include "../Renderer/WeaponView.h"
#include "../Renderer/HudRenderer.h"
#include "../Renderer/Minimap.h"
#include "../Audio/MusicPlayer.h"
#include "../Bootstrap/ModernSettings.h"
#include "../Renderer/ModelPreview.h"
#include "../Formats/ModelIndices.h"
#include "../Formats/ModelLoader.h"
#include "../Formats/BndTextureLoader.h"
#include "../Formats/Obj3DTexture.h"
#include "../Renderer/ModelRenderer.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <set>
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
        // Movement speed is no longer a single tuned number - the original's
        // walk/run tuning sets, acceleration and deceleration are transcribed
        // in PlayerCamera.h and applied per logic tick. The one number still
        // unresolved there is the LOGIC TICK RATE, which scales all of them;
        // the footstep-cadence method described in this comment previously is
        // still how to solve it, and is written out at PlayerCamera::TICK_HZ.
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
        // The switch models come in families of FOUR consecutive meshes, and
        // every family uses the same state order:
        //   +0 red left light, +1 red right light, +2 both off, +3 both yellow
        // (see ModelIndices.h - the pattern repeats for small, large,
        // small+battery, large+battery and the four Boneship variants).
        //
        // That is the SAME sequence the level's animated control-panel faces
        // run: animator group 3 plays descriptors 238 -> 239 -> 240, which
        // dumped out as top-light red, lower-light red, then both lights
        // yellow. So a switch object and a switch wall panel are the same
        // state machine expressed two different ways - the panel by swapping
        // texture, the object by swapping mesh. That is why the switches exist
        // as individual models rather than one model with changing textures:
        // OBJ3D meshes carry no animation channel, so a state change has to be
        // a different mesh.
        enum class SwitchState { RedLeft = 0, RedRight = 1, BothOff = 2, BothYellow = 3 };

        // Base (RedLeft) mesh of the family a crate type belongs to, or -1 if
        // the type is not a switch.
        int SwitchFamilyBase(uint8_t type)
        {
            using namespace ALTEngine::Formats::ModelIndices;
            switch (type)
            {
            case 22: return Obj3D::SwitchRedLeftLight;                    // small switch
            case 24: return Obj3D::SwitchRedLeftLight;                    // small switch, "with animation"
            case 26: return Obj3D::LargeSwitchBatteryRedLeftLight;        // wide, battery
            case 27: return Obj3D::LargeSwitchBatteryRedLeftLight;        // wide, battery
            default: return -1;
            }
        }

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
        // Ejected casings are drawn with the ammo PICKUP mesh shrunk down - see
        // the note at the particle draw. GUESS, chosen by eye.
        constexpr float CASING_MODEL_SCALE = 0.25f;

        // Particle sprite sizes. The original works in screen pixels - a
        // fragment is literally 2x2 - and these are the world-space equivalents
        // at a typical engagement range. GUESSES, and the first thing to adjust
        // if tracers look too fat or too thin.
        // Where the fireball sits relative to the barrel's base. GUESS - the
        // original's explosion carries its own position which has not been read.
        constexpr float EXPLOSION_HEIGHT_OFFSET = 256.0f;

        constexpr float FRAGMENT_HALF_SIZE = 6.0f;
        constexpr float TRACER_MIN_HALF_SIZE = 5.0f;

        // A small round dot, generated rather than loaded - the original's
        // tracers and fragments are untextured coloured primitives, so there is
        // no artwork to read. Hard-edged alpha, because the fragment shader cuts
        // out at 0.5 rather than blending.
        inline std::vector<uint8_t> BuildParticleSprite(int size)
        {
            std::vector<uint8_t> rgba(static_cast<size_t>(size) * size * 4, 0);
            const float centre = (size - 1) * 0.5f;
            const float radius = centre + 0.25f;
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    const float dx = x - centre;
                    const float dy = y - centre;
                    const bool inside = (dx * dx + dy * dy) <= (radius * radius);
                    uint8_t* px = &rgba[(static_cast<size_t>(y) * size + x) * 4];
                    px[0] = 255; px[1] = 240; px[2] = 190;   // a warm white
                    px[3] = inside ? 255 : 0;
                }
            }
            return rgba;
        }

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
        // The cell attribute under a world position, for picking the impact
        // sound (attributes 8 and 9 are the wet ones).
        inline uint8_t ImpactCellAttribute(const ALTEngine::Formats::LevelGeometry& level,
                                           float originX, float originZ, float worldX, float worldZ)
        {
            int cx = ToGridSpaceX(worldX, originX) >> 9;
            int cz = ToGridSpaceZ(worldZ, originZ) >> 9;
            if (cx < 0 || cz < 0 || cx >= level.header.mapLength || cz >= level.header.mapWidth) { return 0; }
            size_t ci = static_cast<size_t>(cz) * static_cast<size_t>(level.header.mapLength)
                      + static_cast<size_t>(cx);
            if (ci >= level.collisionGrid.size()) { return 0; }
            return level.collisionGrid[ci].attribute;
        }

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
        Bootstrap::LanguageSettings& languageSettings,
        PlayerInventoryState* carriedInventory)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return { GameplayOutcome::WindowClosed };
        }
        SDL_Renderer* renderer = app.Renderer();

        // Carried across levels when the caller supplies one; otherwise a fresh
        // pistol-only loadout, which is how the original always starts a level.
        PlayerInventoryState localInventory;
        PlayerInventoryState& inventory = carriedInventory ? *carriedInventory : localInventory;

        // Resolve and load the level
        std::string digits = LevelDigitsFromCode(missionLevelCode);

        // The impact sound differs on levels 0x0c-0x15, and the original tests
        // its own level index rather than the code - see Particles::ImpactSound.
        const int levelIdForSfx = digits.empty() ? 0 : std::atoi(digits.c_str());

        // Level music. The original resolves this through one table indexed by
        // its internal level id (Ghidra: FUN_0004263c -> FUN_0004258c); see
        // MusicPlayer.h for the decoded table and the caveat on how a level CODE
        // maps to that id. Nothing played music during gameplay at all before
        // this, which is why levels were silent.
        {
            int levelId = ALTEngine::Audio::LevelIdForCode(digits);
            int track = ALTEngine::Audio::LevelMusicTrack(levelId);
            std::string fileName = ALTEngine::Audio::MusicTrackFileName(track);
            if (fileName.empty())
            {
                SDL_Log("GameplayScreen: no music for level code '%s' (levelId=%d, track=%d) - silent",
                        digits.c_str(), levelId, track);
                ALTEngine::Audio::MusicPlayer::Stop();
            }
            else
            {
                std::filesystem::path musicPath = cdDirectory / "MUSIC" / fileName;
                std::error_code musicEc;
                if (!std::filesystem::is_regular_file(musicPath, musicEc))
                {
                    // Not an error: the tracks have to be ripped off the disc,
                    // and a missing one should just mean silence.
                    SDL_Log("GameplayScreen: level %s wants %s but it is not in CD/MUSIC - silent",
                            digits.c_str(), fileName.c_str());
                    ALTEngine::Audio::MusicPlayer::Stop();
                }
                else
                {
                    SDL_Log("GameplayScreen: level %s -> levelId %d -> CD track %d -> %s",
                            digits.c_str(), levelId, track, fileName.c_str());
                    ALTEngine::Audio::MusicPlayer::PlayLooped(musicPath);
                    ALTEngine::Audio::MusicPlayer::SetVolume(audioSettings.MusicVolume());
                }
            }
        }
        auto mapPath = FindInSectFolders(cdDirectory, "L" + digits + "LEV.MAP");
        auto gfxPath = FindInSectFolders(cdDirectory, digits + "GFX.B16");

        bool levelReady = false;
        std::string cacheKey = "LEVEL:" + digits;
        FpsCamera camera;
        ALTEngine::Formats::LevelGeometry level;
        std::vector<ALTEngine::Renderer::PlacedObject> placedObjects;

        // Destructible crates and barrels, one per placed object that can be
        // shot. See DamageSystem.h - the health value is derived from the three
        // pistol rounds a crate takes, not read out of the entity template.
        std::vector<ALTEngine::Screens::Damage::Target> damageTargets;
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

            // Sealed inside a crate. A crate's drop1/drop2 are INDICES INTO THE
            // LEVEL'S PICKUP ARRAY, so a crate's contents are ordinary pickups
            // that happen to start locked away - not a separate list. They are
            // hidden and uncollectable until the crate breaks.
            bool contained = false;
            uint8_t pickupIndex = 0xFF;   // its slot in level.pickups
        };
        std::vector<LivePickup> livePickups;
        // Switch/object state, indexed by object record. Non-zero means
        // already thrown - the original latches on this and fails, which
        // aborts the whole command chain rather than re-running it.
        std::vector<uint8_t> objectState;
        // Switch objects that need their mesh swapped as their state changes.
        // placedIndex -> crate record index, plus the family's base mesh.
        struct SwitchVisual { size_t placedIndex; size_t crateIndex; int familyBase; };
        std::vector<SwitchVisual> switchVisuals;
        int switchBlinkTicks = 0;
        bool switchBlinkPhase = false;
        // Read once here rather than threaded through Run's already long
        // settings chain - worth tidying if more of these appear.
        bool autoOpenDoors = false;
        // Free Look defaults OFF, i.e. the original's automatic pitch. Read
        // once here and refreshed whenever the pause menu closes, so toggling
        // it mid-game takes effect without a restart.
        bool freeLook = false;
        bool liveMinimap = false;
        bool cheatFullyLoaded = false;
        bool cheatMaximumHealth = false;
        // Cells the player has seen, for the map's fog of war. One byte per
        // cell, sized on first use. Owning an Auto Mapper bypasses it entirely.
        std::vector<uint8_t> minimapVisited;

        // HUD. The panel sheet is per-chapter, chosen from the level id (see
        // Renderer/HudPanel.h), and the state is the original's own health /
        // armour / ammo model.
        ALTEngine::Renderer::HudRenderer hud;
        ALTEngine::Renderer::WeaponView weaponView;
        bool firePressedLastTick = false;
        bool altFirePressedLastTick = false;
        ALTEngine::Screens::WeaponSystem::Runtime weaponRuntime;
        ALTEngine::Screens::Particles::Pool particles;
        ALTEngine::Screens::ExplosionEffects explosions;
        ALTEngine::Screens::ShatterEffects shatter;

        // The held weapon. Driven by the select keys below; the pistol is the
        // only thing available at the start of a level.
        int selectedWeapon = 0;
        ALTEngine::Screens::WeaponSystem::SwitchAnimation weaponSwitch;
        std::array<bool, 6> weaponKeyWasDown{ false, false, false, false, false, false };

        // What the ammo mirror last saw, so pickups can be applied as DELTAS.
        //
        // This used to copy the inventory straight into hudState.ammoRemainder
        // every frame, which quietly made firing impossible: the weapon system
        // decremented a round and the mirror put it back on the same frame, and
        // a reload that moved a magazine into the remainder was wiped just as
        // fast. Nothing visible ever happened (Edward, 2026).
        std::array<int, ALTEngine::Screens::PlayerHudState::WEAPON_COUNT> mirroredAmmo{ 0, 0, 0, 0, 0 };
        bool ammoSeeded = false;
        ALTEngine::Screens::PlayerHudState hudState;
        bool hudLoadAttempted = false;
        {
            ALTEngine::Bootstrap::Config modernConfig;
            ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
            autoOpenDoors = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::AutoOpenDoors);
            freeLook = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FreeLook);
            liveMinimap = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LiveMinimap);
            // The cheat's own stored state, so it stays on across levels while
            // Enable Cheats is active.
            if (modern.IsActive(ALTEngine::Bootstrap::ModernFeature::EnableCheats))
            {
                auto stored = modernConfig.Get("CheatFullyLoaded");
                cheatFullyLoaded = stored.has_value() && *stored == "On";
                auto storedHealth = modernConfig.Get("CheatMaximumHealth");
                cheatMaximumHealth = storedHealth.has_value() && *storedHealth == "On";
            }
            // The feature is "unlimited render distance", so the original's
            // fade is the INVERSE of it.
            bool unlimited = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::RenderDistance);
            ModelRenderer::SetDrawDistanceFade(!unlimited);
            // Diagnostic: prints exactly what was read from disk, so a
            // "setting doesn't stick" report can be pinned to either the store
            // or the menu display rather than guessed at.
            SDL_Log("GameplayScreen: modern mode=%s freeLook=%d renderDistance=%d(stored %d) "
                    "-> drawDistanceFade=%d, config=%s",
                    modern.Mode() == ALTEngine::Bootstrap::ModernMode::On ? "On"
                        : modern.Mode() == ALTEngine::Bootstrap::ModernMode::Custom ? "Custom" : "Off",
                    (int)freeLook, (int)unlimited,
                    (int)modern.FeatureSetting(ALTEngine::Bootstrap::ModernFeature::RenderDistance),
                    (int)!unlimited, modernConfig.FilePath().string().c_str());
        }
        int lastTriggerAction = 0;
        bool prevUseHeld = false;
        int lastCellIndex = -1;
        float tickAccumulator = 0.0f;

        // Player camera state. Runs at the original's own logic rate rather
        // than the 60 Hz door tick - see PlayerCamera::TICK_HZ.
        ALTEngine::Screens::PlayerCamera::State playerCam;
        float playerTickAccumulator = 0.0f;
        float pendingMouseYaw = 0.0f;   // angle units, accumulated between player ticks
        float pendingMouseY = 0.0f;     // raw mouse Y, drives the original's mouse-forward
        bool playerCamSeeded = false;
        // Camera Sway (the original's DAT_000acea0) gates head bob, view roll
        // and the yaw sway together. Footsteps keep running either way.
        bool cameraSway = cameraSwaySettings.Get();

        if (mapPath.has_value() && gfxPath.has_value())
        {
            if (ModelRenderer::Initialize())
            {
                // Apply the persisted Graphics > Quality choice here too -
                // gameplay can be the first thing to initialise the renderer
                // (loading straight into a level), in which case the menu
                // never got a chance to push it.
                ModelRenderer::SetTextureSmoothing(
                    renderSettings.Get() == Bootstrap::RenderFidelity::Smoothed);

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

                    // playerStartAngle uses the same 8-step compass the crates
                    // do (0=N, 2=E, 4=S, 6=W), not the 4096-step space entity
                    // angles use.
                    //
                    // CORRECTED: this previously mirrored the angle the way
                    // crate facing is mirrored - `pi - angle * pi/4`. That is
                    // right for a crate, whose value rotates a MODEL whose
                    // local axes are mirrored relative to the world, and wrong
                    // for a camera yaw, which is a direction.
                    //
                    // With this camera's convention, forward = (sin yaw,
                    // -cos yaw) and grid +X east / +Z south, the direct mapping
                    // gives 0=North, 2=East, 4=South, 6=West as intended. The
                    // mirrored one swapped North and South while still agreeing
                    // on East and West - and L111's angle is 2, i.e. East, one
                    // of the exact two values where the two formulas cannot be
                    // told apart. That is why it survived: the only level with
                    // data to hand could not distinguish them (Edward, 2026 -
                    // spawned facing the wrong way in another level).
                    camera.yaw = static_cast<float>(level.header.playerStartAngle) * (3.14159265f / 4.0f);

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
                    std::vector<int> meshesToLoad = {
                        ALTEngine::Formats::ModelIndices::Obj3D::ExplosiveBarrel,
                        ALTEngine::Formats::ModelIndices::Obj3D::SingleCrate,
                        ALTEngine::Formats::ModelIndices::Obj3D::DoubleCrate,
                        ALTEngine::Formats::ModelIndices::Obj3D::SteelCoil,
                    };
                    // All FOUR state meshes of every switch family, not just
                    // the one shown at spawn. The renderer silently skips a
                    // PlacedObject whose mesh was never loaded, so a switch
                    // would vanish the moment it changed state otherwise.
                    for (int base : { ALTEngine::Formats::ModelIndices::Obj3D::SwitchRedLeftLight,
                                      ALTEngine::Formats::ModelIndices::Obj3D::LargeSwitchBatteryRedLeftLight })
                    {
                        for (int state = 0; state < 4; ++state) { meshesToLoad.push_back(base + state); }
                    }
                    for (int meshNumber : meshesToLoad)
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
                    for (size_t ci = 0; ci < level.crates.size(); ++ci)
                    {
                        const auto& crate = level.crates[ci];
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
                        // Object scale, 12.12 fixed point over 0x1000.
                        //
                        // CORRECTED: 0xe00 uniform = 0.875, not the
                        // (0xc00, 0xd98, 0xc00) = 0.75/0.849609375/0.75 that
                        // was here. That triple is real, but it belongs to a
                        // DIFFERENT draw function. There are three sibling
                        // entity draw routines in the original, and they do
                        // not agree:
                        //
                        //   FUN_0003765c  0xe00 uniform, skipped entirely for
                        //                 entity type 0x17 (scale 1.0)
                        //   FUN_000377e4  0xc00 / 0xd98 / 0xc00, non-uniform
                        //   FUN_00037930  no scale call at all
                        //
                        // The one that draws these objects is FUN_0003765c.
                        // What settles it is which texture descriptor set each
                        // one binds (Ghidra: FUN_00018bcc's call sites):
                        //   DAT_00244608 <- resource 0x7d, the SAME on every
                        //                   level: the object/pickup set.
                        //   DAT_002408c0 <- resource 0x7f/0x83/0x84/0x85,
                        //                   chosen by a switch on DAT_000ae10c.
                        //
                        // *** CAUTION: THAT SECOND CLAIM IS NOW DOUBTFUL. ***
                        // DAT_000ae10c was taken to be an episode selector, and
                        // "episode-dependent therefore enemy graphics" is what
                        // this scale choice rested on. It is almost certainly the
                        // LANGUAGE instead: the same variable's switch in
                        // FUN_00050c58 emits the Spanish string "de disparo" for
                        // case 3, and its cases are 0/3/4/5 rather than a
                        // contiguous episode range.
                        //
                        // So DAT_002408c0 is likely a language-selected set (art
                        // containing words), and the argument for which of the
                        // three draw functions handles crates does NOT hold.
                        // 0.875 is left in place because it looks right on screen
                        // and 0.75/0.85/0.75 visibly did not - but treat it as
                        // observed rather than derived until the draw function is
                        // identified some other way.
                        // FUN_0003765c binds DAT_00244608 for everything
                        // except type 0x14 on levels 0x16-0x22; FUN_000377e4
                        // binds DAT_002408c0 unconditionally. So the
                        // non-uniform triple is the ENEMY scale, and objects
                        // take 0.875 uniform.
                        //
                        // NOT YET HANDLED: type 0x17 gets no scale at all in
                        // FUN_0003765c. Which crate type that maps to has not
                        // been established, so it is not special-cased here -
                        // if one crate looks 14% too small next to the others,
                        // that is the one.
                        placed.scaleX = 0.875f;
                        placed.scaleY = 0.875f;
                        placed.scaleZ = 0.875f;

                        int familyBase = SwitchFamilyBase(crate.type);
                        if (familyBase >= 0)
                        {
                            switchVisuals.push_back({ placedObjects.size(), ci, familyBase });
                        }

                        // Destructible. Switches are not - shooting a switch
                        // does nothing in the original - so only the crate and
                        // barrel families get a damage target.
                        if (familyBase < 0)
                        {
                            ALTEngine::Screens::Damage::Target target;
                            target.x = worldX;
                            target.y = floorY;
                            target.z = worldZ;
                            const bool isBarrel = (crate.type == 23);
                            target.kind = isBarrel ? ALTEngine::Screens::Damage::TARGET_BARREL
                                                   : ALTEngine::Screens::Damage::TARGET_CRATE;
                            target.health = isBarrel
                                          ? ALTEngine::Screens::Damage::BARREL_HEALTH_UNKNOWN
                                          : ALTEngine::Screens::Damage::CRATE_HEALTH_DERIVED;
                            target.placedObjectIndex = static_cast<int>(placedObjects.size());
                            // The cell this object stamps as occupied. Clearing
                            // that stamp is what reopens the square when the
                            // object is destroyed - see the destruction handler.
                            target.crateIndex = static_cast<int>(ci);
                            target.facing = crate.rotation;
                            target.objectType = crate.type;
                            target.modelKey = placed.cacheKey;
                            target.scaleX = placed.scaleX;
                            target.scaleY = placed.scaleY;
                            target.scaleZ = placed.scaleZ;
                            target.cellIndex = static_cast<int>(
                                static_cast<size_t>(crate.y) * level.header.mapLength + static_cast<size_t>(crate.x));
                            damageTargets.push_back(target);
                        }

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
                        // Which pickups a crate holds, needed before the
                        // placement loop so their gate byte can be overridden.
                        std::set<uint8_t> crateHeldPickups;
                        for (const auto& crate : level.crates)
                        {
                            if (crate.drop == 2) { continue; }   // holds an enemy, not pickups
                            if (crate.drop1 != 0xFF) { crateHeldPickups.insert(crate.drop1); }
                            if (crate.drop2 != 0xFF) { crateHeldPickups.insert(crate.drop2); }
                        }

                        for (size_t pickupSlot = 0; pickupSlot < level.pickups.size(); ++pickupSlot)
                        {
                            const auto& pickup = level.pickups[pickupSlot];
                            // Record byte 6 gates level-start spawning:
                            // non-zero means the pickup does not simply appear
                            // on the floor.
                            //
                            // BUT A CRATE'S CONTENTS ARE ALSO GATED THIS WAY, and
                            // skipping them meant they were never created at all,
                            // so breaking a crate had nothing to reveal (Edward,
                            // 2026). On level 1-1 the correlation is exact: all
                            // 18 free pickups have byte 6 == 0 and all 10 that a
                            // crate holds have byte 6 == 1. So the byte means
                            // "something is holding this", and the crate records
                            // say what.
                            //
                            // Crate contents are therefore created here and
                            // hidden; anything else still gated stays skipped.
                            const bool heldByCrate = crateHeldPickups.count(static_cast<uint8_t>(pickupSlot)) != 0;
                            if (pickup.z != 0 && !heldByCrate) { continue; }

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
                            live.pickupIndex = static_cast<uint8_t>(pickupSlot);
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
                        // Seal the pickups that crates hold.
                        //
                        // The crate record's byte 3 says WHAT it holds:
                        //   0 -> pickups, drop1 and drop2 indexing the pickup array
                        //   2 -> an enemy, drop1 indexing the MONSTER array
                        //
                        // Level 1-1 makes the second case obvious: every drop==2
                        // crate points at a type-2 monster parked at x=9, in a
                        // tidy row at y=83/85/87/89 off the edge of the playable
                        // map. They are held there until something lets them
                        // out - which is how the original does a facehugger
                        // bursting from a crate.
                        int sealed = 0;
                        int enemyCrates = 0;
                        for (const auto& crate : level.crates)
                        {
                            if (crate.drop == 2) { enemyCrates++; continue; }
                            for (uint8_t index : { crate.drop1, crate.drop2 })
                            {
                                if (index == 0xFF) { continue; }
                                for (size_t k = 0; k < livePickups.size(); ++k)
                                {
                                    if (livePickups[k].pickupIndex != index) { continue; }
                                    livePickups[k].contained = true;
                                    if (livePickups[k].placedIndex < placedObjects.size())
                                    {
                                        placedObjects[livePickups[k].placedIndex].visible = false;
                                    }
                                    sealed++;
                                }
                            }
                        }
                        SDL_Log("GameplayScreen: %d pickups sealed in crates, %d crates hold an enemy",
                                sealed, enemyCrates);

                        // The particle sheet, uploaded once for the level.
                        ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                            "particle", BuildParticleSprite(8), 8, 8);

                        // EXPLGFX's barrel page - nine 84x84 frames.
                        if (!explosions.Load(cdDirectory))
                        {
                            SDL_Log("GameplayScreen: EXPLGFX.B16 not loaded - explosions will be silent-invisible");
                        }

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
                                                                        renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings, keyBindings,
                                                                      levelReady ? &level : nullptr,
                                                                      ToGridSpaceX(camera.x, originX) / 512.0f,
                                                                      ToGridSpaceZ(camera.z, originZ) / 512.0f,
                                                                      camera.yaw,
                                                                      inventory.hasAutoMapper ? nullptr : &minimapVisited);
                    // Re-read on return: the pause menu can reach the same
                    // Options screen the boot menu does, so this can have
                    // changed while paused. Reading it once at level load
                    // meant a mid-game toggle silently did nothing.
                    {
                        ALTEngine::Bootstrap::Config modernConfig;
                        ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
                        autoOpenDoors = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::AutoOpenDoors);
                        freeLook = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FreeLook);
                        liveMinimap = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LiveMinimap);
                        ModelRenderer::SetDrawDistanceFade(
                            !modern.IsActive(ALTEngine::Bootstrap::ModernFeature::RenderDistance));
                    }

                    // "Fully Loaded" is a toggle, not a one-shot: while it is on
                    // the loadout is topped up every tick, so "unlimited
                    // ammunition" actually stays unlimited as it is spent.
                    cheatFullyLoaded = pauseResult.cheatFullyLoaded;
                    cheatMaximumHealth = pauseResult.cheatMaximumHealth;
                    if (cheatFullyLoaded)
                    {
                        inventory.GiveEverything();
                        SDL_Log("GameplayScreen: cheat 'Fully Loaded' enabled");
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

            // Keep the music stream fed. PlayLooped only starts it; Update()
            // pushes more audio and is what makes it loop. Deliberately here in
            // the per-FRAME loop rather than inside the fixed 60Hz tick, so it
            // keeps running while paused or while the tick is otherwise skipped
            // - the menu does the same in its own loop.
            ALTEngine::Audio::MusicPlayer::Update();
            dt = std::min(dt, 0.1f); // clamp so a stall/hitch doesn't teleport the camera

            if (levelReady)
            {
                const bool* keys = SDL_GetKeyboardState(nullptr);

                float mouseDx = 0.0f, mouseDy = 0.0f;
                const SDL_MouseButtonFlags mouseButtons = SDL_GetRelativeMouseState(&mouseDx, &mouseDy);

                // The mouse fires. The original reads its buttons into the same
                // input word as the keyboard (DAT_000b0cc8 / DAT_000b0cc4), so
                // left and right button ARE Fire 1 and Fire 2 - they are not a
                // separate binding, and holding either behaves exactly like
                // holding the bound key, including the per-weapon difference
                // between edge and held.
                const bool mouseFire1 = (mouseButtons & SDL_BUTTON_LMASK) != 0;
                const bool mouseFire2 = (mouseButtons & SDL_BUTTON_RMASK) != 0;
                // Read live rather than cached once - lets the Controls
                // > Mouse > Mouse Sensitivity slider take effect
                // immediately without needing a restart (Edward, 2026).
                // 5/10 (the slider's own default) maps to exactly
                // MOUSE_SENSITIVITY, so this changes nothing for anyone
                // who hasn't touched the setting.
                float sensitivity = MOUSE_SENSITIVITY * (static_cast<float>(keyBindings.MouseSensitivity()) / 5.0f);

                // Mouse deltas accumulate here and are consumed by the player
                // tick below, so a 144 Hz machine does not feed the fixed-rate
                // movement code more input than a 30 Hz one.
                //
                // The original adds its mouse X straight to yaw, 1:1, in
                // 4096-per-turn units (FUN_0003efcc, DAT_00404656). Converting
                // through the existing radians-based sensitivity keeps the
                // Controls > Mouse Sensitivity slider producing exactly the
                // feel it did before this change.
                pendingMouseYaw += mouseDx * sensitivity
                    * (static_cast<float>(ALTEngine::Screens::PlayerCamera::ANGLE_UNITS) / 6.28318530718f);
                pendingMouseY += mouseDy;

                // ---- weapon selection -----------------------------------
                // Keys 1-5 pick a weapon directly and 6 steps to the next one,
                // which is the original's scheme. A key for a weapon that has
                // not been picked up does nothing at all rather than selecting
                // an empty slot.
                //
                // Availability is what PICKUP sets. This used to read `equipped`
                // instead, which nothing ever wrote during play - so collecting
                // the shotgun made it available and left it unselectable
                // (Edward, 2026).
                {
                    using ALTEngine::Bootstrap::InputAction;
                    const InputAction selectKeys[5] = {
                        InputAction::SelectWeapon1, InputAction::SelectWeapon2,
                        InputAction::SelectWeapon3, InputAction::SelectWeapon4,
                        InputAction::SelectWeapon5,
                    };

                    for (int i = 0; i < 5; ++i)
                    {
                        const bool down = keys[keyBindings.GetKey(selectKeys[i])];
                        if (down && !weaponKeyWasDown[static_cast<size_t>(i)])
                        {
                            // Only start a swap for a weapon that is actually
                            // held and is not the one already in hand.
                            if (i != selectedWeapon && inventory.Available(i)) { weaponSwitch.Begin(i); }
                        }
                        weaponKeyWasDown[static_cast<size_t>(i)] = down;
                    }

                    const bool nextDown = keys[keyBindings.GetKey(InputAction::NextWeapon)];
                    if (nextDown && !weaponKeyWasDown[5])
                    {
                        const int next = inventory.NextAvailable(selectedWeapon);
                        if (next != selectedWeapon) { weaponSwitch.Begin(next); }
                    }
                    weaponKeyWasDown[5] = nextDown;

                    // If the held weapon somehow is not available - a fresh
                    // level, or a loadout reset - fall back to the pistol.
                    if (!inventory.Available(selectedWeapon)) { selectedWeapon = 0; }
                }

                // Free Look is ours, not the original's - it never let the
                // player aim the view at all. With it on the mouse drives pitch
                // and PlayerCamera's ramp system stands down; with it off the
                // original's automatic pitch runs instead.
                //
                // Free Look is also what makes the -GAP override geometry
                // necessary: the holes above door frames are only visible if
                // you can look up, which originally you could not.
                if (freeLook)
                {
                    camera.pitch = std::clamp(camera.pitch - mouseDy * sensitivity, -MAX_PITCH, MAX_PITCH);
                }

                using ALTEngine::Bootstrap::InputAction;
                namespace PC = ALTEngine::Screens::PlayerCamera;

                // Seed the integer pose from whatever the spawn code put in
                // `camera`, once, so the two representations start in sync.
                if (!playerCamSeeded)
                {
                    playerCam.yaw = static_cast<int>(std::lround(camera.yaw * (PC::ANGLE_UNITS / 6.28318530718f))) & PC::ANGLE_MASK;
                    playerCam.pitch = 0;
                    playerCamSeeded = true;
                }

                // The player runs at the original's own logic rate. Every
                // constant in PlayerCamera.h is per-tick at that rate, so this
                // deliberately does NOT share the 60 Hz door tick below.
                playerTickAccumulator += dt;
                const float playerTickLength = 1.0f / static_cast<float>(PC::TICK_HZ);
                int playerTicksThisFrame = 0;
                while (playerTickAccumulator >= playerTickLength)
                {
                    playerTickAccumulator -= playerTickLength;
                    playerTicksThisFrame++;

                    PC::Input pcInput;

                    // Turn vs strafe. The original had no dedicated turn keys -
                    // its direction keys turned, and the strafe modifier is what
                    // converted them to a sidestep. Turn Left/Right (Q/E by
                    // default) are a departure that keeps Strafe Left/Right on
                    // A/D, where anyone playing with a mouse expects them.
                    //
                    // The strafe modifier keeps its original meaning: held, it
                    // turns the turn keys into strafing too. It does nothing to
                    // A/D, which already strafe.
                    const bool strafeMod = keys[keyBindings.GetKey(InputAction::StrafeModifier)];
                    const bool turnLeftKey = keys[keyBindings.GetKey(InputAction::TurnLeft)];
                    const bool turnRightKey = keys[keyBindings.GetKey(InputAction::TurnRight)];

                    pcInput.forward = keys[keyBindings.GetKey(InputAction::MoveForward)];
                    pcInput.backward = keys[keyBindings.GetKey(InputAction::MoveBackward)];
                    pcInput.strafeLeft = keys[keyBindings.GetKey(InputAction::StrafeLeft)] || (strafeMod && turnLeftKey);
                    pcInput.strafeRight = keys[keyBindings.GetKey(InputAction::StrafeRight)] || (strafeMod && turnRightKey);
                    pcInput.turnLeft = !strafeMod && turnLeftKey;
                    pcInput.turnRight = !strafeMod && turnRightKey;

                    // RunModifier is a hold. RunMode is very likely a toggle in
                    // the original, but nothing traced confirms that, so it is
                    // OR'd in as a second hold key rather than inventing toggle
                    // state - MARKED AS A GUESS.
                    pcInput.run = keys[keyBindings.GetKey(InputAction::RunModifier)]
                               || keys[keyBindings.GetKey(InputAction::RunMode)];

                    pcInput.turn180 = keys[keyBindings.GetKey(InputAction::Turnaround)];
                    pcInput.terrainSpeedTicks = hudState.terrainSpeed;

                    // Manual look: the original gates look up/down behind a
                    // modifier and reuses the forward/back keys for it. There is
                    // no separate Look action in the port's binding list, so it
                    // is wired to the strafe modifier + forward/back, which is
                    // the closest available shape. MARKED AS A GUESS - if the
                    // original binds this elsewhere the mapping moves, not the
                    // logic in PlayerCamera.h.
                    pcInput.look = false;
                    pcInput.lookUp = false;
                    pcInput.lookDown = false;

                    // Mouse. Spread the accumulated delta over the ticks this
                    // frame owes so a single large delta is not applied twice.
                    pcInput.mouseYaw = static_cast<int>(std::lround(pendingMouseYaw));
                    pendingMouseYaw -= static_cast<float>(pcInput.mouseYaw);
                    // Mouse-forward is the original's scheme and only makes
                    // sense when the mouse is not being used to aim.
                    pcInput.mouseForward = freeLook ? 0 : static_cast<int>(std::lround(pendingMouseY));

                    // The cell underfoot: drives ramp pitch, the sluggish-cell
                    // slowdown and footstep suppression.
                    uint8_t cellAttribute = 0;
                    if (levelReady)
                    {
                        int cx = ToGridSpaceX(camera.x, originX) >> 9;
                        int cz = ToGridSpaceZ(camera.z, originZ) >> 9;
                        if (cx >= 0 && cz >= 0 && cx < level.header.mapLength && cz < level.header.mapWidth)
                        {
                            size_t ci = static_cast<size_t>(cz) * static_cast<size_t>(level.header.mapLength)
                                      + static_cast<size_t>(cx);
                            if (ci < level.collisionGrid.size()) { cellAttribute = level.collisionGrid[ci].attribute; }
                        }
                    }
                    // The hazard flag selects which footstep sound plays. The
                    // damage attributes are the ones FUN_0003dff0 reacts to.
                    const bool onHazardCell = (cellAttribute == 7 || cellAttribute == 8 || cellAttribute == 0xc);

                    PC::Tick(playerCam, pcInput, cellAttribute, onHazardCell, cameraSway, freeLook);

                    // Apply the tick's movement through the existing collision
                    // mover, per axis, so wall sliding keeps working.
                    float stepX = 0.0f, stepZ = 0.0f;
                    PC::MovementStep(playerCam, stepX, stepZ);
                    if (stepX != 0.0f || stepZ != 0.0f)
                    {
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

                    // ---- weapon ----------------------------------------
                    // The original keeps two input words and reads the fire key
                    // from a DIFFERENT one depending on the weapon: DAT_000b0cc4
                    // (newly pressed) for the pistol and shotgun, DAT_000b0cc8
                    // (held) for the flamethrower, pulse rifle and smartgun.
                    // That is what makes two of them semi-automatic and three of
                    // them continuous, and it is per-weapon rather than a
                    // property of the key. See WeaponSystem::FireMode.
                    namespace WS = ALTEngine::Screens::WeaponSystem;

                    const bool fireHeld = keys[keyBindings.GetKey(InputAction::Fire1)] || mouseFire1;
                    const bool firePressed = fireHeld && !firePressedLastTick;
                    firePressedLastTick = fireHeld;

                    // The swap happens at the bottom of the lower, out of sight.
                    const int swapTo = weaponSwitch.Tick();
                    if (swapTo >= 0)
                    {
                        const int equipped = inventory.Equip(swapTo);
                        if (equipped >= 0) { selectedWeapon = equipped; }
                    }
                    weaponView.SetSwitchOffset(weaponSwitch.Offset());

                    weaponRuntime.weapon = hudState.currentWeapon;
                    weaponRuntime.stateChanged = false;

                    const bool triggerActive =
                        (WS::Def(weaponRuntime.weapon).primary == WS::FIRE_ON_PRESS) ? firePressed : fireHeld;

                    if (triggerActive && WS::TryPrimaryFire(weaponRuntime, hudState))
                    {
                        // A shot spawns the round AND the casing - FUN_0003d5b0
                        // calls the weapon's projectile spawn, which then calls
                        // FUN_0002b37c for the ejection once the round is away.
                        namespace P = ALTEngine::Screens::Particles;
                        // Projectile type by weapon. All five are traced now -
                        // each spawn function was matched to a weapon by which
                        // ammo counter it decrements. The type numbers do NOT
                        // follow the weapon order.
                        static constexpr int TYPE_FOR_WEAPON[5] = {
                            P::TYPE_PISTOL,   // 0 pistol       FUN_0002bb74
                            P::TYPE_SHOTGUN,  // 1 shotgun      FUN_0002bbc0
                            P::TYPE_PULSE,    // 2 pulse rifle  FUN_0002bc18
                            P::TYPE_FLAME,    // 3 flamethrower FUN_0002c1d4
                            P::TYPE_SMARTGUN, // 4 smartgun     FUN_0002bcac
                        };
                        const int projectileType = TYPE_FOR_WEAPON[hudState.currentWeapon];

                        // The pulse rifle, flamethrower and smartgun each put
                        // THREE projectiles out per pull, with the later ones
                        // launched faster (0x80 apart) so a burst strings out in
                        // flight rather than travelling as a clump.
                        const int shots = P::ShotsPerPull(projectileType);
                        for (int shot = 0; shot < shots; ++shot)
                        {
                            const int launchSpeed = playerCam.speed + shot * P::BurstSpeedStep;
                            P::Particle* round = particles.SpawnProjectile(
                                projectileType, camera.x, camera.y, camera.z,
                                playerCam.viewYaw, playerCam.viewPitch, launchSpeed);

                            // The smartgun's three shots get progressively
                            // shorter lives, so they die at three ranges
                            // (FUN_0002bcac: +0x1c = 0x10 - n).
                            if (round && projectileType == P::TYPE_SMARTGUN)
                            {
                                round->life = P::SmartgunLife(shot);
                                round->strength = round->life;
                            }
                        }

                        particles.SpawnCasing(P::TYPE_CASING_A, camera.x, camera.y, camera.z,
                                              playerCam.viewYaw, playerCam.speed);
                    }

                    // Reload and out-of-ammo run only from idle, as a separate
                    // pass after the fire keys - the same order FUN_0003efcc has.
                    //
                    // `canSwitchAway` stands in for FUN_00038c78. Automatic
                    // weapon switching is not implemented, so this is false,
                    // which takes the original's fallback path: the pistol is
                    // topped up to one round so the player is never left with
                    // nothing. That is real traced behaviour, not a stopgap.
                    WS::UpdateIdle(weaponRuntime, hudState, false);
                    WS::TickNoise(weaponRuntime);

                    // Weapon audio.
                    //
                    // THIS IS A STAND-IN. In the original the report comes from
                    // an OP_EVENT opcode inside the weapon's firing sequence, so
                    // it lands on whichever frame the artist put it on. Those
                    // sequences are data this cannot read yet (the pistol's are
                    // at 0x0901a4), and the sequences built here are synthesised
                    // and contain no opcodes at all - which is why nothing was
                    // playing. Firing on the state change instead gets the sound
                    // out at roughly the right moment; replacing it with the real
                    // sequence is a data change, not a code one.
                    // The PISTOL carries its own OP_EVENT and plays itself, so
                    // firing it here as well would double the report. Every
                    // other weapon still runs a synthesised sequence with no
                    // opcodes in it, so those need the stand-in until their
                    // sequence tables are dumped.
                    if (weaponRuntime.stateChanged
                        && weaponRuntime.state == WS::STATE_FIRING
                        && hudState.currentWeapon > 0
                        && hudState.currentWeapon < ALTEngine::Screens::PlayerHudState::WEAPON_COUNT)
                    {
                        ALTEngine::Audio::SfxPlayer::PlaySlot(
                            ALTEngine::Formats::SoundIds::WEAPON_SHOT[hudState.currentWeapon],
                            digits.c_str(), cdDirectory);
                    }
                    if (weaponRuntime.stateChanged && weaponRuntime.state == WS::STATE_RELOAD
                        && hudState.currentWeapon >= 0
                        && hudState.currentWeapon < ALTEngine::Screens::PlayerHudState::WEAPON_COUNT)
                    {
                        const int clip = ALTEngine::Formats::SoundIds::WEAPON_CLIP[hudState.currentWeapon];
                        if (clip >= 0)
                        {
                            ALTEngine::Audio::SfxPlayer::PlaySlot(clip, digits.c_str(), cdDirectory);
                        }
                    }

                    // Starting the animation IS the state change (FUN_000400fc).
                    // Not every state animates. The out-of-ammo arms in
                    // FUN_0003efcc set state 4 and call FUN_000524b0, NOT
                    // FUN_000400fc - the empty state leaves the last frame up.
                    // Secondary fire. Only the pulse rifle has one that does
                    // anything yet: FUN_0003efcc's case 2 spends a grenade,
                    // sets state 3 and starts that state's animation - which is
                    // the sequence carrying the 0401gren sound cue.
                    const bool altFireHeld = keys[keyBindings.GetKey(InputAction::Fire2)] || mouseFire2;
                    const bool altFirePressed = altFireHeld && !altFirePressedLastTick;
                    altFirePressedLastTick = altFireHeld;

                    if (altFirePressed
                        && hudState.currentWeapon == ALTEngine::Renderer::HUD_GRENADE_WEAPON
                        && weaponRuntime.state == WS::STATE_IDLE
                        && inventory.pulseRifle.grenades > 0)
                    {
                        inventory.pulseRifle.grenades--;
                        weaponRuntime.SetState(WS::STATE_GRENADE);
                        weaponRuntime.noise = WS::GRENADE_NOISE;
                    }

                    if (weaponRuntime.stateChanged && WS::StateHasAnimation(weaponRuntime.state))
                    {
                        weaponView.PlayState(weaponRuntime.state);
                    }

                    weaponView.SetCameraDip(playerCam.bobOffsetY * 64);
                    if (weaponView.Tick())
                    {
                        // The sequence ending is what returns the weapon to idle
                        // (FUN_0003e93c), except from the latched empty states.
                        weaponRuntime.stateChanged = false;
                        WS::OnAnimationEnded(weaponRuntime);
                        if (weaponRuntime.stateChanged && WS::StateHasAnimation(weaponRuntime.state))
                        {
                            weaponView.PlayState(weaponRuntime.state);
                        }
                    }

                    // Move every live particle. A round that runs into geometry
                    // shatters where it stopped - two pieces for a pistol round,
                    // eight for a shotgun pellet (FUN_0002abe0).
                    if (levelReady)
                    {
                        namespace D = ALTEngine::Screens::Damage;
                        for (D::Target& target : damageTargets) { D::TickTarget(target); }
                        explosions.Tick();
                        shatter.Tick([&](float sx, float sz) {
                            return ALTEngine::Formats::FindFloorHeightGridSpace(
                                level, ToGridSpaceX(sx, originX), ToGridSpaceZ(sz, originZ));
                        });
                        hudState.TickCarriedItems();

                        // Destroying something: clear its occupancy so the
                        // square opens up, spill what it held, and - if it was a
                        // barrel - set off the blast, which can take out its
                        // neighbours and start a chain.
                        auto ReleaseContents = [&](int crateIndex) {
                            if (crateIndex < 0 || crateIndex >= static_cast<int>(level.crates.size())) { return; }
                            const auto& crate = level.crates[static_cast<size_t>(crateIndex)];
                            if (crate.drop == 2)
                            {
                                // Holds an enemy - drop1 indexes the MONSTER
                                // array, and the monster is parked off the edge
                                // of the map until released. Enemies do not
                                // exist yet, so this only records the intent.
                                SDL_Log("crate %d would release monster %u", crateIndex, crate.drop1);
                                return;
                            }
                            for (uint8_t index : { crate.drop1, crate.drop2 })
                            {
                                if (index == 0xFF) { continue; }
                                for (auto& live : livePickups)
                                {
                                    if (live.pickupIndex != index || !live.contained) { continue; }
                                    live.contained = false;
                                    if (live.placedIndex < placedObjects.size())
                                    {
                                        placedObjects[live.placedIndex].visible = true;
                                    }
                                }
                            }
                        };

                        auto ClearOccupancy = [&](int cellIdx) {
                            if (cellIdx < 0 || cellIdx >= static_cast<int>(level.collisionGrid.size())) { return; }
                            auto& cell = level.collisionGrid[static_cast<size_t>(cellIdx)];
                            cell.unknown13 = 0;
                            cell.unknown5 = 0;
                        };

                        // Depth-limited so a room packed with barrels cannot
                        // recurse without bound; the original has no such limit
                        // because its blast is iterative.
                        std::vector<int> blastQueue;
                        auto DestroyTarget = [&](D::Target& target) {
                            if (target.placedObjectIndex >= 0
                                && target.placedObjectIndex < static_cast<int>(placedObjects.size()))
                            {
                                placedObjects[static_cast<size_t>(target.placedObjectIndex)].visible = false;
                            }
                            ClearOccupancy(target.cellIndex);
                            ReleaseContents(target.crateIndex);

                            // Tear the object into pieces of its own mesh. This
                            // is the real break-up; the effect sprites below are
                            // a separate system that plays alongside it.
                            if (const auto* faces = ALTEngine::Renderer::ModelRenderer::ModelFaces(target.modelKey))
                            {
                                shatter.Shatter(*faces,
                                                ALTEngine::Renderer::ModelRenderer::ModelSheetKey(target.modelKey),
                                                target.x, target.y, target.z,
                                                target.scaleX, target.scaleY, target.scaleZ,
                                                target.objectType, target.facing);
                            }
                            if (target.kind == D::TARGET_BARREL)
                            {
                                blastQueue.push_back(target.cellIndex);
                                // The fireball goes off where the barrel stood,
                                // lifted so it sits around the barrel's middle
                                // rather than on the floor.
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_BARREL,
                                                 target.x, target.y + EXPLOSION_HEIGHT_OFFSET, target.z);
                            }
                            else
                            {
                                // A crate breaks into six scattered pieces, on
                                // an axis chosen by which way it faces - see
                                // ExplosionEffects::SpawnScatter.
                                explosions.SpawnScatter(target.x, target.y, target.z,
                                                        target.facing, target.objectType);
                            }
                        };

                        // Walk the blast out from a cell, two rings, gated the
                        // way FUN_000368c8 gates it.
                        auto RunBlast = [&](int originCell) {
                            if (originCell < 0 || originCell >= static_cast<int>(level.collisionGrid.size())) { return; }
                            const int width = level.header.mapLength;
                            const int ox = originCell % width;
                            const int oz = originCell / width;
                            const auto& origin = level.collisionGrid[static_cast<size_t>(originCell)];

                            bool ringOneOpen[8] = { false, false, false, false, false, false, false, false };
                            auto Reachable = [&](int dx, int dz, int& outCell) {
                                const int cx = ox + dx;
                                const int cz = oz + dz;
                                outCell = -1;
                                if (cx < 0 || cz < 0 || cx >= width || cz >= level.header.mapWidth) { return false; }
                                const size_t ci = static_cast<size_t>(cz) * static_cast<size_t>(width)
                                                + static_cast<size_t>(cx);
                                if (ci >= level.collisionGrid.size()) { return false; }
                                const auto& cell = level.collisionGrid[ci];
                                if (cell.attribute > D::BLAST_MAX_ATTRIBUTE) { return false; }
                                const int step = static_cast<int>(cell.floorHeight) - static_cast<int>(origin.floorHeight);
                                if (step > D::BLAST_MAX_HEIGHT_STEP || step < -D::BLAST_MAX_HEIGHT_STEP) { return false; }
                                outCell = static_cast<int>(ci);
                                return true;
                            };

                            std::vector<int> hit;
                            for (int i = 0; i < 8; ++i)
                            {
                                int cell = -1;
                                if (Reachable(D::BLAST_RING1[i].dx, D::BLAST_RING1[i].dz, cell))
                                {
                                    ringOneOpen[i] = true;
                                    hit.push_back(cell);
                                }
                            }
                            for (const auto& outer : D::BLAST_RING2)
                            {
                                if (!ringOneOpen[outer.parent]) { continue; }  // culled with its parent
                                int cell = -1;
                                if (Reachable(outer.offset.dx, outer.offset.dz, cell)) { hit.push_back(cell); }
                            }

                            for (int cell : hit)
                            {
                                for (D::Target& other : damageTargets)
                                {
                                    // Objects caught in a blast are removed
                                    // outright - no health check.
                                    if (!other.destroyed && other.cellIndex == cell)
                                    {
                                        other.destroyed = true;
                                        other.state = D::STATE_DYING;
                                        DestroyTarget(other);
                                    }
                                }
                            }
                        };

                        particles.Tick([&](float px, float py, float pz, int hitType, int hitLife) {
                            // Something destructible first. The damage is the
                            // projectile's SPEED field doubled while the round is
                            // still fresh - see DamageSystem::HitDamage, and note
                            // that the field named like strength is not the one
                            // used as damage.
                            for (D::Target& target : damageTargets)
                            {
                                if (target.destroyed) { continue; }
                                const float dx = px - target.x;
                                const float dz = pz - target.z;
                                // Per-axis box, as FUN_0002a628 does - and the
                                // box doubles for shotgun pellets. The extents
                                // are a placeholder until the entity template is
                                // read; the SHAPE of the test is traced.
                                const float half = D::HitHalfExtent(hitType);
                                if (dx > half || dx < -half || dz > half || dz < -half) { continue; }

                                // The damage comes from THIS round's own type,
                                // not the pistol's. It was hardcoded to the
                                // pistol's numbers, which made every weapon hit
                                // for 32 - wrong for all four of the others, and
                                // it would have hidden the barrel rule entirely.
                                // Falloff: the round's remaining life against
                                // what it started with. Full damage over the
                                // first half of the flight, half over the second
                                // (FUN_0002a628).
                                const auto def = ALTEngine::Screens::Particles::DefFor(hitType);
                                const int damage = D::HitDamage(def.speed, hitLife, def.strength);

                                if (D::ApplyHit(target, damage, hitType))
                                {
                                    if (target.placedObjectIndex >= 0
                                        && target.placedObjectIndex < static_cast<int>(placedObjects.size()))
                                    {
                                        placedObjects[static_cast<size_t>(target.placedObjectIndex)].visible = false;
                                    }

                                    // Clear the occupancy stamp so the square
                                    // becomes walkable. A crate blocks by writing
                                    // its type into byte 12 of its cell at spawn
                                    // (IsCellBlocking treats any non-zero,
                                    // non-0xFF value there as solid), so removing
                                    // the crate has to remove the stamp too -
                                    // otherwise the wreckage keeps blocking a
                                    // square that is visibly empty.
                                    //
                                    // Byte 4, which carries the crate's record
                                    // index, is cleared with it: it only has
                                    // meaning while byte 12 identifies an object.
                                    // Barrels explode, crates break apart.
                                    ALTEngine::Audio::SfxPlayer::PlaySlot(
                                        target.kind == D::TARGET_BARREL
                                            ? ALTEngine::Formats::SoundIds::BARREL_EXPLODE
                                            : ALTEngine::Formats::SoundIds::CRATE_BREAK,
                                        digits.c_str(), cdDirectory);

                                    DestroyTarget(target);

                                    // Run every blast this set off, including
                                    // ones started by the chain itself.
                                    for (size_t q = 0; q < blastQueue.size(); ++q)
                                    {
                                        ALTEngine::Audio::SfxPlayer::PlaySlot(
                                            ALTEngine::Formats::SoundIds::BARREL_EXPLODE,
                                            digits.c_str(), cdDirectory);
                                        RunBlast(blastQueue[q]);
                                    }
                                    blastQueue.clear();
                                }
                                return true; // the round stops here and shatters
                            }

                            // A PROJECTILE IS A POINT IN ONE CELL. It was going
                            // through CanOccupy, which is the PLAYER's test: a
                            // 400x400 footprint sampled at five points plus a
                            // step-up rule. That stopped rounds as soon as any
                            // corner of an imaginary body came within 200 units
                            // of a wall, which in a corridor is almost at once -
                            // hence only being able to break a crate from right
                            // next to it.
                            //
                            // The original tests one cell and no radius:
                            // FUN_0002b534 derives a single index as
                            // (x >> 9) + width * (z >> 9) and probes that, then
                            // steps once and probes again. No footprint, no
                            // step-up.
                            if (ALTEngine::Formats::IsCellBlocking(level,
                                    ToGridSpaceX(px, originX), ToGridSpaceZ(pz, originZ)))
                            {
                                // Impact sound picked by where it landed, not by
                                // what fired - brick, metal or water.
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Screens::Particles::ImpactSound(
                                        levelIdForSfx, ImpactCellAttribute(level, originX, originZ, px, pz)),
                                    digits.c_str(), cdDirectory);
                                // The impact puff - uv records 25..31, which
                                // FUN_0002ad48 selects for every projectile type.
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_IMPACT,
                                                 px, py, pz);
                                return true;
                            }

                            const float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(
                                level, ToGridSpaceX(px, originX), ToGridSpaceZ(pz, originZ));
                            return py < floorY;
                        });
                    }

                    // The animation's own sound cue. OP_EVENT's operand IS a
                    // slot id - FUN_0003e93c passes it straight through - so an
                    // animation carries its own audio and the weapon report
                    // lands on the frame the artist put it on.
                    if (weaponView.SoundCued())
                    {
                        ALTEngine::Audio::SfxPlayer::PlaySlot(
                            weaponView.CuedSoundId(), digits.c_str(), cdDirectory);
                    }

                    // Footsteps, each half-stride of the bob phase.
                    if (playerCam.footstep)
                    {
                        ALTEngine::Audio::SfxPlayer::PlaySlot(
                            PC::FootstepSound(onHazardCell), digits.c_str(), cdDirectory);
                    }
                }

                // Hand the integer pose back to the renderer's float camera.
                // Yaw always; pitch only when the original owns it, so Free
                // Look's mouse pitch is not overwritten.
                if (playerTicksThisFrame > 0)
                {
                    pendingMouseY = 0.0f;
                    camera.yaw = PC::ToRadians(playerCam.viewYaw);
                    if (!freeLook) { camera.pitch = PC::ToRadians(playerCam.viewPitch); }
                    camera.roll = PC::ToRadians(playerCam.viewRoll);
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
                                // Arms (or advances) a light record's
                                // variant counter. This is what brings a
                                // blink light to life: every one of L111's
                                // 18 mode-1 records loads with
                                // variant 0 / variantMax 1, so it is
                                // completely static until this fires. The
                                // 30 mode-3 records load already armed
                                // (variantMax 0) and blink from the start.
                                //
                                // `modifier` is the delta, matching
                                // FUN_00029d44's signed add-and-clamp.
                                // Treated as signed: the original passes it
                                // in a char.
                                ModelRenderer::ToggleLevelLight(
                                    cacheKey, static_cast<int>(c.objectIndex),
                                    static_cast<int>(static_cast<int8_t>(c.modifier)));
                                SDL_Log("GameplayScreen: ToggleLight light=%u delta=%d",
                                        c.objectIndex, static_cast<int>(static_cast<int8_t>(c.modifier)));
                                // Does not set the success flag in the
                                // original either, so continuing is correct.
                                break;
                            case 9: // Change Texture
                                // Arms (or advances) a texture animator, the
                                // direct counterpart of command 0 on a light
                                // record. This is what makes a control panel
                                // change when its switch is thrown: L111's
                                // animators 2-9 all load with
                                // variant 0 / variantMax 1 and do nothing at
                                // all until this fires at them.
                                ModelRenderer::ChangeLevelTexture(
                                    cacheKey, static_cast<int>(c.objectIndex),
                                    static_cast<int>(static_cast<int8_t>(c.modifier)));
                                SDL_Log("GameplayScreen: ChangeTexture animator=%u delta=%d",
                                        c.objectIndex, static_cast<int>(static_cast<int8_t>(c.modifier)));
                                // Does not set the success flag in the
                                // original either, so continuing is correct.
                                break;
                            case 2: // Spawn Pickup
                            case 3: // Spawn Monster
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
                        if (live.collected || live.contained || live.cellIndex != cellIndex) { continue; }
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
                        // Facing, for the ahead-probe only. Ground plane, so
                        // pitch is deliberately ignored.
                        const float probeForwardX = std::sin(camera.yaw);
                        const float probeForwardZ = -std::cos(camera.yaw);
                        for (int step = 1; step <= 2; ++step)
                        {
                            int ax = (pgx + static_cast<int>(probeForwardX * 512.0f * step)) >> 9;
                            int az = (pgz + static_cast<int>(probeForwardZ * 512.0f * step)) >> 9;
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

                        // Level lights. Steps the blink state machines and
                        // re-uploads vertex colours only when a light
                        // actually changed state. The jitter argument is
                        // the (rand & 3) + 1 the original adds to each
                        // blink duration.
                        ModelRenderer::TickLevelLights(cacheKey, SDL_rand(256));

                        hudState.Tick();
                        if (cheatFullyLoaded) { inventory.GiveEverything(); }

                        // 200 is the game's own ceiling: pickup type 0x17 sets it
                        // outright and FUN_0003a674 clamps the derm patch display
                        // there, so it is the real maximum rather than 100.
                        if (cheatMaximumHealth)
                        {
                            hudState.health = 200;
                            hudState.dead = false;
                        }
                        hud.TickTracker();

                        // Remember where the player has been, for the map's fog
                        // of war. Without this the visited array stays empty and
                        // the map draws nothing but the player marker.
                        if (levelReady)
                        {
                            ALTEngine::Renderer::MarkMinimapVisited(
                                level, minimapVisited,
                                ToGridSpaceX(camera.x, originX) >> 9,
                                ToGridSpaceZ(camera.z, originZ) >> 9);
                        }

                        // Switch objects. A switch that has not been thrown
                        // alternates its two red lights; once thrown it holds
                        // both-yellow. Same states and same 16-tick cadence as
                        // the animated control-panel FACES (animator group 3
                        // runs descriptors 238 -> 239 -> 240 on a speed-6
                        // hold), because they are the same state machine - the
                        // panel swaps texture, the object has to swap mesh.
                        //
                        // NOT VERIFIED: the cadence is taken from the texture
                        // animator's opcode 6 rather than read out of an
                        // object-animation path, and BothOff is never shown -
                        // it is most likely the unpowered state for a
                        // battery switch before its battery is fitted, but
                        // nothing has been traced that selects it.
                        switchBlinkTicks++;
                        if (switchBlinkTicks >= 16)
                        {
                            switchBlinkTicks = 0;
                            switchBlinkPhase = !switchBlinkPhase;
                        }
                        for (const SwitchVisual& sv : switchVisuals)
                        {
                            if (sv.placedIndex >= placedObjects.size()) { continue; }
                            bool thrown = (sv.crateIndex < objectState.size()) && (objectState[sv.crateIndex] != 0);
                            SwitchState state = thrown ? SwitchState::BothYellow
                                                       : (switchBlinkPhase ? SwitchState::RedRight : SwitchState::RedLeft);
                            placedObjects[sv.placedIndex].cacheKey.modelIndex =
                                sv.familyBase + static_cast<int>(state);
                        }

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
                    // Head bob rides on top of the eye height. It only ever
                    // dips - the original's vertical term is
                    // min(sin p, sin p+180), which is -|sin p| - so the camera
                    // never rises above eye level, only sinks up to 64 units
                    // below it twice per stride. See PlayerCamera.h.
                    //
                    // STAND_OFFSET + EYE_HEIGHT is kept as-is rather than
                    // replaced with the original's DAT_000b0b4c (0x180 = 384),
                    // because 384 against this 800 is an unexplained factor of
                    // two and the current value is known to look right. See the
                    // note on EYE_HEIGHT_ORIGINAL in PlayerCamera.h.
                    camera.y = floorY + STAND_OFFSET + EYE_HEIGHT
                             + static_cast<float>(playerCam.bobOffsetY);
                }
            }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            if (levelReady)
            {
                // Live particles ride along as PlacedObjects. Casings ARE
                // little models in the original - FUN_0002b37c stores a model
                // pointer from DAT_00248160 or DAT_0024815c into the object -
                // so going through the existing object path keeps them properly
                // depth-sorted against the level for free, rather than needing a
                // separate pass that would draw them through walls.
                //
                // WHICH model those two pointers are is not resolved, so the
                // PickMod shell meshes stand in at a small scale. They are the
                // ammo PICKUP models, so they are the right shape and the wrong
                // size; the scale below is chosen by eye and is a GUESS.
                //
                // PlacedObject carries a single rotation axis while a real
                // casing tumbles on two (FUN_0002b37c seeds +0x10 and +0x12).
                // Only the yaw is applied here - the second axis needs a
                // renderer change and is not worth one yet.
                // Particle sprites. The tracers and impact fragments finally
                // have somewhere to be drawn.
                //
                // The original draws a bullet as a LINE from its position to
                // position + a fraction of its velocity (FUN_0002d3e4), and a
                // fragment as a 2x2 pixel quad (FUN_0002d1b4). A line is not
                // something the billboard pass can express, so a tracer is drawn
                // as a quad stretched along its travel instead - same streak,
                // different primitive. MARKED as a departure.
                std::vector<ALTEngine::Renderer::PlacedSprite> placedSprites;
                if (levelReady)
                {
                    namespace P = ALTEngine::Screens::Particles;
                    for (const P::Particle& particle : particles.All())
                    {
                        if (!particle.alive) { continue; }
                        if (particle.type == P::TYPE_CASING_A || particle.type == P::TYPE_CASING_B) { continue; }

                        // The original's own culling: nothing closer than 0x200,
                        // nothing past 0x1801.
                        const float dx = particle.x - camera.x;
                        const float dz = particle.z - camera.z;
                        const float distance = std::sqrt(dx * dx + dz * dz);
                        if (distance < P::DRAW_NEAR_CUTOFF || distance > P::DRAW_FAR_CUTOFF) { continue; }

                        ALTEngine::Renderer::PlacedSprite spark;
                        spark.textureKey = "particle";
                        spark.x = particle.x;
                        spark.y = particle.y;
                        spark.z = particle.z;

                        if (particle.type == P::TYPE_FRAGMENT)
                        {
                            spark.halfWidth = FRAGMENT_HALF_SIZE;
                            spark.halfHeight = FRAGMENT_HALF_SIZE;
                        }
                        else
                        {
                            // Stretched along the direction of travel, by the
                            // same fraction of the velocity the original's line
                            // spans.
                            const float speed = std::sqrt(particle.vx * particle.vx + particle.vz * particle.vz);
                            const float streak = speed * P::TracerVelocityFraction(particle.type) * 0.5f;
                            spark.halfWidth = (streak > TRACER_MIN_HALF_SIZE) ? streak : TRACER_MIN_HALF_SIZE;
                            spark.halfHeight = TRACER_MIN_HALF_SIZE;
                        }
                        placedSprites.push_back(spark);
                    }
                    explosions.Collect(placedSprites, camera.x, camera.z, true);
                    shatter.Collect(placedSprites, camera.x, camera.z);
                }

                const size_t staticObjectCount = placedObjects.size();
                if (levelReady)
                {
                    namespace P = ALTEngine::Screens::Particles;
                    for (const P::Particle& particle : particles.All())
                    {
                        if (!particle.alive) { continue; }
                        if (particle.type != P::TYPE_CASING_A && particle.type != P::TYPE_CASING_B) { continue; }

                        ALTEngine::Renderer::PlacedObject shell;
                        shell.cacheKey = { ALTEngine::Renderer::ModelCatalog::Pickmod,
                                           ALTEngine::Formats::ModelIndices::PickMod::PistolShell };
                        shell.x = particle.x;
                        shell.y = particle.y;
                        shell.z = particle.z;
                        shell.rotationRadians = ALTEngine::Screens::PlayerCamera::ToRadians(particle.angleY);
                        shell.scaleX = shell.scaleY = shell.scaleZ = CASING_MODEL_SCALE;
                        placedObjects.push_back(shell);
                    }
                }

                std::vector<uint8_t> pixels = ModelRenderer::RenderLevelToRgba(cacheKey, camera, windowW, windowH,
                                                                               placedObjects, placedSprites);
                placedObjects.resize(staticObjectCount); // drop this frame's particles again
                if (!pixels.empty())
                {
                    SDL_Texture* frameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, windowW, windowH);
                    if (frameTexture)
                    {
                        SDL_UpdateTexture(frameTexture, nullptr, pixels.data(), windowW * 4);
                        SDL_RenderTexture(renderer, frameTexture, nullptr, nullptr);

                        // HUD. Loaded lazily on the first frame that has a
                        // renderer to make a texture with; the sheet is picked
                        // by chapter from the level id.
                        if (!hudLoadAttempted)
                        {
                            hudLoadAttempted = true;
                            int levelId = ALTEngine::Audio::LevelIdForCode(digits);
                            hud.Load(renderer, cdDirectory,
                                     ALTEngine::Renderer::HudPanelFileForLevelId(levelId),
                                     ALTEngine::Bootstrap::LanguageFolderName(language));
                            // Border made for the minimap, from
                            // OVERRIDE/CUSTOM. Absent falls back to the
                            // tracker's panel strip.
                            hud.LoadCustomBorder(renderer, cdDirectory);
                        }
                        // Mirror the inventory into the HUD state each frame.
                        //
                        // The inventory already tracks per-weapon ammo as a
                        // single number, which is the total the original's HUD
                        // displays - so it goes into the remainder slot with the
                        // unit counter left at zero. The unit/remainder split
                        // exists for reload behaviour that does not exist yet;
                        // when it does, the split becomes meaningful and this
                        // mapping should move into the inventory itself rather
                        // than being flattened here.
                        //
                        // Weapon order matches the original's 0-4 indices, which
                        // is also the inventory's declaration order.
                        {
                            const std::array<int, ALTEngine::Screens::PlayerHudState::WEAPON_COUNT> current{
                                inventory.pistol.ammo, inventory.shotgun.ammo, inventory.flamethrower.ammo,
                                inventory.pulseRifle.ammo, inventory.smartGun.ammo
                            };

                            for (size_t w = 0; w < current.size(); ++w)
                            {
                                const int magazine = ALTEngine::Screens::PlayerHudState::ROUNDS_PER_UNIT[w];

                                if (!ammoSeeded)
                                {
                                    // First frame: split the inventory total
                                    // into the original's unit/remainder pair.
                                    // A total that divides exactly leaves the
                                    // last magazine LOADED rather than banked,
                                    // so the player can fire immediately.
                                    if (magazine > 0)
                                    {
                                        int units = current[w] / magazine;
                                        int rem = current[w] % magazine;
                                        if (rem == 0 && units > 0) { units--; rem = magazine; }
                                        hudState.ammoUnits[w] = static_cast<int16_t>(units);
                                        hudState.ammoRemainder[w] = static_cast<int16_t>(rem);
                                    }
                                    else
                                    {
                                        hudState.ammoUnits[w] = 0;
                                        hudState.ammoRemainder[w] = static_cast<int16_t>(current[w]);
                                    }
                                }
                                else if (current[w] != mirroredAmmo[w])
                                {
                                    // A pickup happened. Apply only the change,
                                    // so the rounds already spent stay spent.
                                    int rem = hudState.ammoRemainder[w] + (current[w] - mirroredAmmo[w]);
                                    int units = hudState.ammoUnits[w];
                                    if (magazine > 0)
                                    {
                                        while (rem > magazine) { rem -= magazine; units++; }
                                    }
                                    if (rem < 0) { rem = 0; }
                                    hudState.ammoUnits[w] = static_cast<int16_t>(units);
                                    hudState.ammoRemainder[w] = static_cast<int16_t>(rem);
                                }

                                mirroredAmmo[w] = current[w];
                            }
                            ammoSeeded = true;

                            // Grenades. The original keeps them in the high half
                            // of the pulse rifle's own ammo word (DAT_000b0ac6),
                            // capped at 0x14 - they belong to that weapon, not to
                            // a slot of their own.
                            hudState.grenades = static_cast<int16_t>(
                                inventory.pulseRifle.grenades > ALTEngine::Screens::PlayerHudState::GRENADE_MAX
                                    ? ALTEngine::Screens::PlayerHudState::GRENADE_MAX
                                    : inventory.pulseRifle.grenades);
                        }

                        // The select keys own this now, in the canonical order
                        // (see PlayerInventoryState::ByIndex).
                        hudState.currentWeapon = selectedWeapon;

                        // The held weapon goes INTO the HUD's own 320x240
                        // surface, under the panels, rather than being scaled
                        // into the window on its own.
                        //
                        // Drawing it separately put its bottom edge on a
                        // different pixel row than the screen edge, because the
                        // two took different rounding paths to get there - which
                        // showed up as a gap under the sprite that moved as the
                        // weapon bobbed (Edward, 2026). At 1:1 in the shared
                        // surface the anchor lands on row 240 exactly, every
                        // frame, and the single blit at the end scales both
                        // together.
                        weaponView.SetWeapon(renderer, cdDirectory, hudState.currentWeapon);
                        hud.Draw(renderer, hudState, windowW, windowH, [&] {
                            weaponView.Draw(renderer,
                                            ALTEngine::Renderer::HUD_VIRTUAL_WIDTH,
                                            ALTEngine::Renderer::HUD_VIRTUAL_HEIGHT);
                        });

                        // Motion tracker. Contacts are world-space offsets from
                        // the player; enemies do not exist yet, so this is empty
                        // and the dish draws with its sweep and no blips. Wiring
                        // it now means the enemy work only has to fill the list.
                        std::vector<ALTEngine::Renderer::HudRenderer::Contact> contacts;
                        hud.DrawTracker(renderer, contacts, camera.yaw, windowW, windowH);

                        // Live Minimap (Modern option). Drawn over the
                        // presented world frame with SDL_Renderer, the same way
                        // the pause menu draws - the 3D pass is already resolved
                        // to a texture by this point, so a 2D overlay is free.
                        //
                        // NOT gated on owning an Auto Mapper. It was, and that
                        // made the option look broken: hasAutoMapper is only set
                        // by collecting pickup type 16, so on any level without
                        // one - or before reaching it - turning the option on did
                        // nothing at all and gave no clue why. Asking for a live
                        // map IS the departure; requiring the pickup on top of it
                        // just hides the feature (Edward, 2026).
                        if (liveMinimap && levelReady)
                        {
                            // Top-left, lined up with the rest of the HUD: the
                            // ammo frame's own left gap horizontally, and the
                            // health frame's rows vertically, so the two boxes
                            // read as one line across the top (Edward, 2026).
                            //
                            // Positioned in the HUD's 320x240 space and scaled
                            // like everything else, rather than as a fraction of
                            // the window, so it stays aligned at any resolution.
                            float hudScaleX = static_cast<float>(windowW)
                                            / static_cast<float>(ALTEngine::Renderer::HUD_VIRTUAL_WIDTH);
                            float hudScaleY = static_cast<float>(windowH)
                                            / static_cast<float>(ALTEngine::Renderer::HUD_VIRTUAL_HEIGHT);

                            // HUD_MINIMAP_X/Y is the outer corner of the whole
                            // assembly INCLUDING the strips, so the map itself is
                            // inset by one pixel horizontally and by the strip's
                            // thickness vertically (Edward, 2026 - "this includes
                            // the black bars").
                            SDL_FRect mapRect{
                                (ALTEngine::Renderer::HUD_MINIMAP_X + 1) * hudScaleX,
                                (ALTEngine::Renderer::HUD_MINIMAP_Y
                                 + ALTEngine::Renderer::HUD_MINIMAP_STRIP_H) * hudScaleY,
                                ALTEngine::Renderer::HUD_MINIMAP_W * hudScaleX,
                                ALTEngine::Renderer::HUD_MINIMAP_H * hudScaleY
                            };

                            ALTEngine::Renderer::MinimapStyle style;
                            style.alpha = 190;          // readable without hiding the world
                            style.drawTriggers = false; // too noisy at this size
                            style.drawBorder = false;   // the edge strips frame it instead
                            style.alignTopLeft = true;  // pin to HUD_MINIMAP_X/Y, do not centre

                            // The Auto Mapper reveals everything; without it the
                            // player only sees where they have been.
                            const std::vector<uint8_t>* fog =
                                inventory.hasAutoMapper ? nullptr : &minimapVisited;
                            SDL_FRect drawn = ALTEngine::Renderer::DrawMinimap(
                                renderer, level, mapRect,
                                ToGridSpaceX(camera.x, originX) / 512.0f,
                                ToGridSpaceZ(camera.z, originZ) / 512.0f,
                                camera.yaw, style, fog);

                            // Frame what was actually drawn, in PIXELS. Going via
                            // HUD units and re-scaling rounds twice, which left a
                            // one-pixel gap under the map. One pixel of leeway
                            // each side, matching the tracker's 223..305 around a
                            // dish of 224..304 (Edward, 2026).
                            if (drawn.w > 0.0f && drawn.h > 0.0f)
                            {
                                SDL_FRect framed{ drawn.x - hudScaleX, drawn.y,
                                                  drawn.w + hudScaleX * 2.0f, drawn.h };
                                hud.DrawEdgeStripsPixels(renderer, framed,
                                                         ALTEngine::Renderer::HUD_MINIMAP_STRIP_H * hudScaleY);
                            }
                        }
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
