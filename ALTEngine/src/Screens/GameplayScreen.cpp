#include "GameplayScreen.h"
#include "PauseMenuScreen.h"
#include "../Bootstrap/AppWindow.h"
#include "../Bootstrap/Font8x8.h"
#include "../Bootstrap/TestingSettings.h"
#include "../Formats/FacePick.h"
#include "../Formats/LevelLoader.h"
#include "PlayerHudState.h"
#include "PickupSystem.h"
#include "HudMessages.h"
#include "GameOverSequence.h"
#include "FaceHugOverlay.h"
#include "DroppedPickups.h"
#include "PlayerCamera.h"
#include "WeaponSystem.h"
#include "ParticleSystem.h"
#include "ExplosionEffects.h"
#include "ShatterEffects.h"
#include "GameplayObjects.h"
#include "GameplayLevelBuild.h"
#include "Enemies.h"
#include "../Game/Blast.h"
#include "../Game/Doors.h"
#include "../Game/ResupplyTimer.h"
#include "../Game/ProjectileCollision.h"
#include "SmartgunTargeting.h"
#include "../Game/CreatureSight.h"
#include "../Game/FlowField.h"
#include "../Game/Lifts.h"
#include "../Game/PlayerMover.h"
#include "../Game/Triggers.h"
#include "SaveSlotScreen.h"
#include "LevelSnapshot.h"
#include "../Game/Progression.h"

#include <nlohmann/json.hpp>
#include "EnemySprites.h"
#include "DamageSystem.h"
#include "../Audio/SfxPlayer.h"
#include "../Formats/SoundIds.h"
#include "../Renderer/WeaponView.h"
#include "../Renderer/HudRenderer.h"
#include "../Renderer/Minimap.h"
#include "../Audio/MusicPlayer.h"
#include "../Bootstrap/ModernSettings.h"
#include "../Bootstrap/GameplaySettings.h"
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
#include <vector>

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
        using namespace GameplayObjects;

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

        // Player collision constants, taken from the game's own entity
        // mover (Ghidra: FUN_00031afc / FUN_000315f0).

        // A thrown grenade's or charge's +0x1a (FUN_0002c4e0, FUN_0002c830):
        // what it takes off an object it strikes, and, doubled, off the queen.
        constexpr int EXPLOSIVE_HIT_DAMAGE = 0x140;

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

        // The player's feet are ON the floor and the eye is 0x300 above them -
        // see PlayerCamera::EYE_HEIGHT_ORIGINAL for how that was settled.
        constexpr float EYE_HEIGHT = static_cast<float>(ALTEngine::Screens::PlayerCamera::EYE_HEIGHT_ORIGINAL);

        // World space -> grid space, where cell = coord >> 9. Inverts the
        // crate placement used above, so player and objects agree.
        // Testing: Face Index Display (Modern > Testing). Picks the level face
        // along the centre of the view and outlines it (Formats/FacePick.h),
        // with a dot where the pick is taken.
        void DrawFacePick(SDL_Renderer* renderer, const ALTEngine::Formats::LevelGeometry& level,
                          const ALTEngine::Renderer::FpsCamera& view, int windowW, int windowH, int& pickedFace)
        {
            namespace F = ALTEngine::Formats;
            const F::FaceView fv{ view.x, view.y, view.z, view.yaw, view.pitch, view.roll, view.fovYRadians };
            float fx, fy, fz;
            F::ViewForward(fv, fx, fy, fz);
            pickedFace = F::PickLevelFace(level, view.x, view.y, view.z, fx, fy, fz).face;

            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            const SDL_FRect dot{ static_cast<float>(windowW) * 0.5f - 2.0f, static_cast<float>(windowH) * 0.5f - 2.0f, 4.0f, 4.0f };
            SDL_RenderFillRect(renderer, &dot);
            if (pickedFace < 0) { return; }

            std::vector<SDL_FPoint> points;
            for (const auto& [x, y] : F::ProjectFaceOutline(level, pickedFace, fv, windowW, windowH))
            {
                points.push_back({ x, y });
            }
            if (points.size() < 2) { return; }
            points.push_back(points.front());

            // Twice, a pixel apart, so the outline shows at high resolutions.
            SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
            for (auto& p : points) { p.x += 1.0f; p.y += 1.0f; }
            SDL_RenderLines(renderer, points.data(), static_cast<int>(points.size()));
        }

        int ToGridSpaceX(float worldX, float originX) { return static_cast<int>(std::lround(worldX + originX)); }
        int ToGridSpaceZ(float worldZ, float originZ) { return static_cast<int>(std::lround(worldZ + originZ)); }

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

        // Live door state - see Game/Doors.h. Doors drive the collision grid
        // directly, so movement clearance and the visual stay in lockstep.
        std::vector<ALTEngine::Game::DoorState> doorStates;

        // Lifts (Game/Lifts.h) and where each one's platform pieces sit in
        // placedObjects.
        std::vector<ALTEngine::Game::Lifts::Lift> lifts;
        std::vector<std::pair<size_t, size_t>> liftPlaced;   // first, count

        // The level's scripted triggers - see Game/Triggers.h. One system for
        // walking onto a cell and for the use key.
        ALTEngine::Game::Triggers::System triggers;
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

            // In flight, having just been thrown out of a crate.
            float vx = 0, vy = 0, vz = 0;
            bool falling = false;

            // Waiting for a trigger. A pickup whose record byte 6 is non-zero
            // and that no crate holds appears when Spawn Pickup commands
            // (FUN_0003bb14) have added up to that byte: byte 5 is the counter.
            bool waitingOnTrigger = false;
            uint8_t triggerCount = 0;
            uint8_t triggerThreshold = 0;
        };
        std::vector<LivePickup> livePickups;
        // A handler's grenades in flight (FUN_0002b240, particle type 0xd),
        // stepped on the particle pass by the arm at 0x2dd0c.
        struct EnemyGrenade
        {
            float x = 0, y = 0, z = 0;
            float vx = 0, vy = 0, vz = 0;
            int bounce = 0x10;   // +0x18: the next bounce's lift, 2 less each time
            int life = 0x40;     // +0x1c: goes off when it runs out
            int triggerLatch = 0;  // +0x1f (FUN_00041784)
        };
        std::vector<EnemyGrenade> enemyGrenades;
        // The player's thrown explosives: the pulse rifle's grenade (particle
        // type 0xc, FUN_0002c4e0) and the seismic charge (0x13, FUN_0002c830).
        // Both fly by the particle pass's arm at 0x2db80 and go off when their
        // life runs out (0x2da19), on an object or creature, or dropping into a
        // hole.
        struct PlayerExplosive
        {
            float x = 0, y = 0, z = 0;
            float vx = 0, vy = 0, vz = 0;
            int bounce = 8;      // +0x18: the next bounce's lift, 2 less each time
            int life = 0x40;     // +0x1c
            bool charge = true;  // a seismic charge, or else a grenade
            int spin = 0;        // +0x10: a charge's first angle, +0x20 each tick it is drawn
            int triggerLatch = 0;  // +0x1f: the cell trigger it last set off (FUN_000414f0)
        };
        std::vector<PlayerExplosive> playerExplosives;
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
        // Free Look defaults OFF, i.e. the original's automatic pitch. Read
        // once here and refreshed whenever the pause menu closes, so toggling
        // it mid-game takes effect without a restart.
        bool freeLook = false;
        // Modern: Player Jumping. The height of the jump above the floor and
        // its vertical speed, per player tick, and the key's last state.
        bool playerJumping = false;
        // Modern: Faster Xenomorphs. Off, creatures run on alternate ticks, as
        // the original's do (see logicTick); on, the aliens run on every tick
        // and everything else stays on the alternate ones.
        bool fasterXenomorphs = false;
        // The original's -so switch, the port's default: one pass of the task
        // list per two interrupts, so every tick is a creature tick and the
        // base tick is half as fast. Modern: Speed Compensation turns it off,
        // for the original's default (the player at twice the creatures' rate).
        bool speedCompensationOff = true;
        // Modern: Save Anywhere. Opening the pause menu makes its Save Game
        // keep the level as it stands (Screens/LevelSnapshot.h).
        bool saveAnywhere = false;
        // Knockback (Game::PlayerMover): its strength and heading.
        int knockbackStrength = 0;
        int knockbackYaw = 0;
        // Counts logic ticks. The original runs its creatures, its particles
        // and its firing only on alternate passes of the task list
        // (`test byte [0x6164], 1` at 0x342c1 and 0x2d929; firing and the
        // weapon's animation on the drawing pass) - half the rate of the
        // player, the doors and the lifts. A fire-on-press weapon's press
        // waits for the next such tick rather than being lost.
        unsigned logicTick = 0;
        // Faster Xenomorphs: of the passes the original skips for creatures,
        // the aliens take one in FASTER_XENO_OFF_PASS_EVERY, so they run at
        // 1 + 1/N times the original's rate. Every off-pass (N = 1, double
        // speed) was too fast in play (Edward, 2026); N = 2 gives 1.5x.
        constexpr unsigned FASTER_XENO_OFF_PASS_EVERY = 2;
        unsigned fasterXenoOffPass = 0;
        bool firePressPending = false;
        // The weapon's own clock (Bootstrap::WeaponPacingSettings). The
        // original fires and animates the weapon once per drawn frame, which
        // its speed compensation does not make up for, so its rate of fire
        // followed the machine's frame rate. Here a weapon pass comes once per
        // weaponFrameSeconds of game time, whatever the port's frame rate.
        double weaponClock = 0.0;
        double weaponFrameSeconds = ALTEngine::Bootstrap::WeaponPacingSettings::DEFAULT
            / ALTEngine::Screens::PlayerCamera::ORIGINAL_TIMER_HZ;
        float jumpOffset = 0.0f;
        float jumpVelocity = 0.0f;
        bool jumpHeld = false;
        bool liveMinimap = false;
        // Modern > Testing (TestingSettings.h).
        bool testingNoEnemies = false;
        bool testingFaceIndex = false;
        bool testingGhost = false;
        bool testingNoResupply = false;
        // Ghost Mode's own height for the player's feet, taken from where they
        // stood when it came on; `ghostActive` marks that it has been taken.
        float ghostFeetY = 0.0f;
        bool ghostActive = false;
        // The face under the crosshair this frame, -1 for none.
        int pickedFace = -1;
        // Modern "No Hit Immunity": every hit lands, during a creature's hit
        // reaction and the immunity after it.
        bool ignoreStun = false;
        bool cheatFullyLoaded = false;
        bool cheatMaximumHealth = false;
        // Cells the player has seen, for the map's fog of war. One byte per
        // cell, sized on first use. Owning an Auto Mapper bypasses it entirely.
        // Run Mode's latch (DAT_000ad584 bit 1) and its key's last state.
        bool runModeToggled = false;
        bool runModeWasDown = false;
        std::vector<uint8_t> minimapVisited;   // each cell's seen bit (byte 7, bit 0)
        std::vector<uint8_t> minimapAreas;     // the discovered areas (DAT_00245bb4)

        // HUD. The panel sheet is per-chapter, chosen from the level id (see
        // Renderer/HudPanel.h), and the state is the original's own health /
        // armour / ammo model.
        ALTEngine::Renderer::HudRenderer hud;
        ALTEngine::Renderer::WeaponView weaponView;
        bool firePressedLastTick = false;
        bool altFirePressedLastTick = false;
        ALTEngine::Screens::WeaponSystem::Runtime weaponRuntime;

        // The path network's flow field, rebuilt as the player changes node.
        ALTEngine::Game::FlowField flowField;
        uint32_t creatureRandom = 0x1234567u;   // FUN_000100ec stands in
        ALTEngine::Screens::Particles::Pool particles;
        ALTEngine::Screens::Particles::Rng dropRng;
        ALTEngine::Screens::ExplosionEffects explosions;
        ALTEngine::Screens::HudMessages hudMessages;
        ALTEngine::Screens::FaceHugOverlay faceHug;
        std::vector<std::string> faceHugSheets;
        ALTEngine::Screens::ShatterEffects shatter;
        bool glassSheetReady = false;            // the "glass" sheet is uploaded for this level
        // Cryopod covers: their placed objects (one per level texture page)
        // and how far each has tipped open, in 4096ths of a turn.
        struct Cryopod { size_t crateIndex = 0; size_t placedIndex = 0; int angle = 0; };
        std::vector<Cryopod> cryopods;
        float cryopodOpenSign = -1.0f;
        bool cryopodOpenSignKnown = false;
        bool cryopodCoverReady = false;          // the per-level cover mesh is registered
        ALTEngine::Screens::Particles::Rng glassRng;
        // The objects' destruction delays (Damage::Target::fuse).
        ALTEngine::Screens::Particles::Rng fuseRng;
        std::vector<ALTEngine::Screens::Enemies::Enemy> enemies;

        // The trigger the player last fired, so a trigger runs once on entry
        // rather than every tick it is stood on.
        int playerLastTrigger = 0;

        // Set when health reaches zero; the sequence plays once the frame is done
        // rather than mid-tick, so the last frame the player saw is the one that
        // killed them.
        bool playerDead = false;
        int deathVideoIndex = 1;
        ALTEngine::Screens::EnemySprites enemySprites;

        // The held weapon. Driven by the select keys below; the pistol is the
        // only thing available at the start of a level.
        int selectedWeapon = 0;
        // The mission assessment's tally (Game::Progression::LevelTally): what
        // the level holds and what has been done to it, by type, and which
        // secret areas (cell byte 5) the player has stood in.
        ALTEngine::Game::Progression::LevelTally tally;
        std::vector<uint8_t> secretsSeen(256, 0);
        // Every change made to the level's light and texture animators, in
        // order ({0 texture / 1 light, index, delta}): a thrown switch's panel,
        // a broken window. The snapshot replays them, since the animators are
        // the renderer's and start afresh with the level.
        std::vector<std::array<int, 3>> surfaceChanges;
        ALTEngine::Screens::WeaponSystem::SwitchAnimation weaponSwitch;
        std::array<bool, 6> weaponKeyWasDown{ false, false, false, false, false, false };

        ALTEngine::Screens::PlayerHudState hudState;
        bool hudLoadAttempted = false;
        {
            ALTEngine::Bootstrap::Config modernConfig;
            ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
            freeLook = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FreeLook);
            playerJumping = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::PlayerJumping);
            fasterXenomorphs = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FasterXenomorphs);
            speedCompensationOff = !modern.IsActive(ALTEngine::Bootstrap::ModernFeature::SpeedCompensation);
            saveAnywhere = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::SaveAnywhere);
            liveMinimap = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LiveMinimap);
            {
                ALTEngine::Bootstrap::TestingSettings testing(modernConfig);
                testingNoEnemies = testing.IsOn(ALTEngine::Bootstrap::TestingOption::DisableEnemySpawn);
                testingFaceIndex = testing.IsOn(ALTEngine::Bootstrap::TestingOption::FaceIndexDisplay);
                testingGhost = testing.IsOn(ALTEngine::Bootstrap::TestingOption::GhostMode);
                testingNoResupply = testing.IsOn(ALTEngine::Bootstrap::TestingOption::DisableResupplyTimer);
            }
            ignoreStun = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::NoHitImmunity);
            weaponFrameSeconds = ALTEngine::Bootstrap::WeaponPacingSettings(modernConfig).Get()
                / ALTEngine::Screens::PlayerCamera::ORIGINAL_TIMER_HZ;

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
        bool prevUseHeld = false;
        int lastCellIndex = -1;

        // Player camera state. Runs at the original's own logic rate rather
        // than the door tick - see PlayerCamera::TICK_HZ.
        ALTEngine::Screens::PlayerCamera::State playerCam;
        float playerTickAccumulator = 0.0f;
        // Modern: Level Timer. The level's game time: one tick's length for
        // each logic tick the player runs, from the first to the one the End
        // Level trigger fires on, so pauses and loading never count.
        double levelSeconds = 0.0;
        float pendingMouseYaw = 0.0f;   // angle units, accumulated between player ticks
        float pendingMouseY = 0.0f;     // raw mouse Y, drives the original's mouse-forward
        bool playerCamSeeded = false;

        // Where the player was at the start of the most recent player tick, so
        // the frame can be drawn between that and where they are now. The
        // simulation itself stays on its fixed tick; only the view is smoothed.
        // Without this, movement on a 60Hz+ display stepped visibly at the
        // 30Hz tick rate.
        float prevCamX = 0.0f, prevCamZ = 0.0f, prevCamYaw = 0.0f;

        // The player's Y in the original's sense (DAT_000b0a4a): the floor under
        // them. The camera sits EYE_HEIGHT above this.
        float playerFeetY = 0.0f;
        bool prevCamValid = false;
        // Camera Sway (the original's DAT_000acea0) gates head bob, view roll
        // and the yaw sway together. Footsteps keep running either way.
        bool cameraSway = cameraSwaySettings.Get();

        // Modern: draw tracers along their real direction of travel rather than
        // as the original's flat sideways streak.


        // ---- Modern: Save Anywhere --------------------------------------------
        // The level as it stands, as the save file's snapshot, and putting one
        // back over the level as loaded. The pieces are LevelSnapshot's; this
        // is what puts them together.
        auto hexBytes = [](const std::vector<uint8_t>& bytes) {
            static const char* hexDigits = "0123456789abcdef";
            std::string out;
            for (uint8_t b : bytes) { out.push_back(hexDigits[b >> 4]); out.push_back(hexDigits[b & 15]); }
            return out;
        };
        auto bytesFromHex = [](const std::string& hex) {
            std::vector<uint8_t> out;
            auto nibble = [](char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
            for (size_t i = 0; i + 1 < hex.size(); i += 2)
            {
                out.push_back(static_cast<uint8_t>(nibble(hex[i]) << 4 | nibble(hex[i + 1])));
            }
            return out;
        };

        auto captureSnapshot = [&]() -> std::string {
            namespace LS = ALTEngine::Screens::LevelSnapshot;
            nlohmann::json j;
            j["version"] = LS::VERSION;
            j["level"] = digits;
            j["grid"] = LS::GridToHex(level.collisionGrid);
            j["player"] = { { "x", camera.x }, { "z", camera.z }, { "weapon", selectedWeapon },
                            { "camera", LS::SaveCamera(playerCam) }, { "hud", LS::SaveHud(hudState) } };
            j["doors"] = nlohmann::json::array();
            for (const auto& d : doorStates) { j["doors"].push_back(LS::SaveDoor(d)); }
            j["lifts"] = nlohmann::json::array();
            for (const auto& l : lifts) { j["lifts"].push_back(LS::SaveLift(l)); }
            j["triggers"] = hexBytes(triggers.Uses());
            j["pickups"] = nlohmann::json::array();
            for (const auto& live : livePickups)
            {
                const bool placed = live.placedIndex < placedObjects.size();
                j["pickups"].push_back({ { "collected", live.collected }, { "contained", live.contained },
                                         { "waiting", live.waitingOnTrigger }, { "count", live.triggerCount },
                                         { "cell", live.cellIndex },
                                         { "x", placed ? placedObjects[live.placedIndex].x : 0.0f },
                                         { "y", placed ? placedObjects[live.placedIndex].y : 0.0f },
                                         { "z", placed ? placedObjects[live.placedIndex].z : 0.0f } });
            }
            j["targets"] = nlohmann::json::array();
            for (const auto& t : damageTargets) { j["targets"].push_back(LS::SaveTarget(t)); }
            j["objects"] = hexBytes(objectState);
            j["enemies"] = nlohmann::json::array();
            for (const auto& e : enemies) { j["enemies"].push_back(LS::SaveEnemy(e)); }
            j["minimap"] = hexBytes(minimapVisited);
            j["minimapAreas"] = hexBytes(minimapAreas);
            // The assessment's tally so far: what's been killed, destroyed or
            // thrown, and the secret areas found. The totals are the level's own.
            j["killed"] = tally.killed;
            j["secrets"] = hexBytes(secretsSeen);
            j["surfaces"] = surfaceChanges;
            return j.dump();
        };

        // Over the level as just built. Each piece is only put back if it
        // matches what was built (the same number of doors, creatures, ...),
        // so a save from a changed level file can't put anything out of range.
        auto applySnapshot = [&](const std::string& text) -> bool {
            namespace LS = ALTEngine::Screens::LevelSnapshot;
            namespace PC = ALTEngine::Screens::PlayerCamera;
            nlohmann::json j;
            try { j = nlohmann::json::parse(text); }
            catch (const std::exception&) { return false; }
            if (j.value("version", 0) != LS::VERSION || j.value("level", std::string{}) != digits) { return false; }
            try
            {
                LS::GridFromHex(level.collisionGrid, j.value("grid", std::string{}));

                const auto& d = j["doors"];
                if (d.is_array() && d.size() == doorStates.size())
                {
                    for (size_t i = 0; i < d.size(); ++i) { LS::LoadDoor(doorStates[i], d[i]); }
                }
                const auto& l = j["lifts"];
                if (l.is_array() && l.size() == lifts.size())
                {
                    for (size_t i = 0; i < l.size(); ++i) { LS::LoadLift(lifts[i], l[i]); }
                }
                triggers.RestoreUses(bytesFromHex(j.value("triggers", std::string{})));

                const auto& p = j["pickups"];
                if (p.is_array() && p.size() == livePickups.size())
                {
                    for (size_t i = 0; i < p.size(); ++i)
                    {
                        auto& live = livePickups[i];
                        live.collected = p[i].value("collected", live.collected);
                        live.contained = p[i].value("contained", live.contained);
                        live.waitingOnTrigger = p[i].value("waiting", live.waitingOnTrigger);
                        live.triggerCount = p[i].value("count", live.triggerCount);
                        live.cellIndex = p[i].value("cell", live.cellIndex);
                        live.falling = false;
                        live.vx = live.vy = live.vz = 0.0f;
                        if (live.placedIndex < placedObjects.size())
                        {
                            auto& placed = placedObjects[live.placedIndex];
                            placed.x = p[i].value("x", placed.x);
                            placed.y = p[i].value("y", placed.y);
                            placed.z = p[i].value("z", placed.z);
                            placed.visible = !live.collected && !live.contained && !live.waitingOnTrigger;
                        }
                    }
                }
                const auto& t = j["targets"];
                if (t.is_array() && t.size() == damageTargets.size())
                {
                    for (size_t i = 0; i < t.size(); ++i)
                    {
                        auto& target = damageTargets[i];
                        LS::LoadTarget(target, t[i]);
                        if (target.destroyed && target.fuse == 0 && target.placedObjectIndex >= 0
                            && target.placedObjectIndex < static_cast<int>(placedObjects.size()))
                        {
                            placedObjects[static_cast<size_t>(target.placedObjectIndex)].visible = false;
                        }
                    }
                }
                const auto objects = bytesFromHex(j.value("objects", std::string{}));
                if (objects.size() == objectState.size()) { objectState = objects; }

                const auto& e = j["enemies"];
                if (e.is_array() && e.size() == enemies.size())
                {
                    for (size_t i = 0; i < e.size(); ++i) { LS::LoadEnemy(enemies[i], e[i]); }
                }
                minimapVisited = bytesFromHex(j.value("minimap", std::string{}));
                minimapAreas = bytesFromHex(j.value("minimapAreas", std::string{}));
                if (j.contains("killed") && j["killed"].is_array() && j["killed"].size() == tally.killed.size())
                {
                    for (size_t i = 0; i < tally.killed.size(); ++i) { tally.killed[i] = j["killed"][i]; }
                }
                const auto seen = bytesFromHex(j.value("secrets", std::string{}));
                if (seen.size() == secretsSeen.size()) { secretsSeen = seen; }

                // The switches' panels and the broken windows, replayed onto the
                // fresh animators in the order they happened.
                if (j.contains("surfaces") && j["surfaces"].is_array())
                {
                    surfaceChanges.clear();
                    for (const auto& c : j["surfaces"])
                    {
                        if (!c.is_array() || c.size() != 3) { continue; }
                        const std::array<int, 3> change{ c[0].get<int>(), c[1].get<int>(), c[2].get<int>() };
                        if (change[0] == 1) { ModelRenderer::ToggleLevelLight(cacheKey, change[1], change[2]); }
                        else { ModelRenderer::ChangeLevelTexture(cacheKey, change[1], change[2]); }
                        surfaceChanges.push_back(change);
                    }
                }

                const auto& player = j["player"];
                camera.x = player.value("x", camera.x);
                camera.z = player.value("z", camera.z);
                LS::LoadCamera(playerCam, player["camera"]);
                camera.yaw = PC::ToRadians(playerCam.yaw);
                playerCamSeeded = true;
                prevCamValid = false;
                LS::LoadHud(hudState, player["hud"]);
                const int weapon = player.value("weapon", selectedWeapon);
                if (inventory.Equip(weapon) >= 0) { selectedWeapon = weapon; }
            }
            catch (const std::exception& ex)
            {
                SDL_Log("GameplayScreen: the saved level couldn't be put back fully: %s", ex.what());
            }
            return true;
        };

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
                // A lamp or goggles still lit when the level was left stays lit.
                if (levelReady && (inventory.lampTicks > 0 || inventory.gogglesTicks > 0))
                {
                    ModelRenderer::SetLevelLightEquipment(cacheKey, inventory.lampTicks > 0 ? 1 : 2);
                }
            }

            if (levelReady)
            {
                try
                {
                    level = ALTEngine::Formats::LevelLoader::Load(*mapPath);
                    // Each face's cell, so only the cells the view walk takes
                    // in are drawn (CreatureSight::FaceCells / DrawnCells).
                    ModelRenderer::SetLevelFaceCells(cacheKey, ALTEngine::Game::CreatureSight::FaceCells(level));

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
                    playerFeetY = ALTEngine::Formats::FindFloorHeightGridSpace(
                                   level,
                                   ToGridSpaceX(camera.x, originX),
                                   ToGridSpaceZ(camera.z, originZ));
                    camera.y = playerFeetY + EYE_HEIGHT;
                    camera.fovYRadians = ALTEngine::Screens::PlayerCamera::VIEW_FOV_Y_RADIANS;

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
                    // Every model the level's objects use. For a switch, all
                    // FOUR state meshes, not just the one shown at spawn: the
                    // renderer silently skips a PlacedObject whose mesh was
                    // never loaded, so a switch would vanish the moment it
                    // changed state otherwise.
                    const bool episodeThree = EpisodeThreeObjects(digits);
                    std::vector<int> meshesToLoad;
                    auto want = [&](int mesh) {
                        if (std::find(meshesToLoad.begin(), meshesToLoad.end(), mesh) == meshesToLoad.end()) { meshesToLoad.push_back(mesh); }
                    };
                    want(ALTEngine::Formats::ModelIndices::Obj3D::SingleCrate);
                    for (const auto& crate : level.crates)
                    {
                        want(Obj3DIndexForCrateType(crate.type, episodeThree));
                        const int family = SwitchFamilyBase(crate.type, episodeThree);
                        if (family >= 0) { for (int state = 0; state < 4; ++state) { want(family + state); } }
                    }
                    for (int meshNumber : meshesToLoad)
                    {
                        // OBJ3D.BND is in GFX; looking in the SECT folders
                        // found nothing, and the objects only showed because
                        // the menu's own preload had already loaded them.
                        std::optional<std::filesystem::path> obj3dPath = cdDirectory / "GFX" / "OBJ3D.BND";
                        { std::error_code ec; if (!std::filesystem::exists(*obj3dPath, ec)) { obj3dPath.reset(); } }
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
                    // The last level's own meshes - doors, lifts, the cryopod
                    // cover - go before this level builds any (see
                    // UnloadLevelModels). This ran at the doors, after the
                    // cover was built, and took the cover with it.
                    ModelRenderer::UnloadLevelModels();

                    cryopods.clear();
                    cryopodOpenSignKnown = false;
                    cryopodCoverReady = false;
                    // The cryopod cover, OBJ3D model 0x28: every face is
                    // routine 14 (FUN_000367b4 loads it with the flag
                    // FUN_00036700 turns into routine 14), a flat colour half
                    // over what is behind - entry 2 of CLUT slot 0, the level's
                    // first palette (span mode 6, 0x5eceb). Rebuilt per level
                    // with that colour over a white texel, so the colour shows
                    // as it is.
                    if (std::any_of(level.crates.begin(), level.crates.end(),
                                    [](const auto& c) { return c.type == CRYOPOD_OBJECT_TYPE; }))
                    {
                        try
                        {
                            const std::filesystem::path obj3dPath = cdDirectory / "GFX" / "OBJ3D.BND";
                            const auto meshes = ALTEngine::Formats::ModelLoader::Load(obj3dPath);
                            const int podMesh = ALTEngine::Formats::ModelIndices::Obj3D::StasisPodCover;
                            const auto* mesh = ALTEngine::Formats::ModelLoader::FindByNumber(meshes, podMesh);
                            const auto levelSet = ALTEngine::Formats::BndTextureLoader::Load(*gfxPath);
                            const auto cover = mesh ? GameplayLevelBuild::BuildCryopodCover(*mesh, levelSet.firstPaletteRgb)
                                                    : std::nullopt;
                            if (cover)
                            {
                                cryopodCoverReady = ModelRenderer::RegisterMesh(
                                    { ALTEngine::Renderer::ModelCatalog::LevelObject, podMesh }, cover->mesh, cover->texture);
                                cryopodOpenSign = cover->openSign;
                                cryopodOpenSignKnown = true;
                                SDL_Log("GameplayScreen: cryopod cover colour %d,%d,%d at half",
                                        cover->colour[0], cover->colour[1], cover->colour[2]);
                            }
                        }
                        catch (const std::exception& e)
                        {
                            SDL_Log("GameplayScreen: cryopod cover not rebuilt for this level (%s)", e.what());
                        }
                    }

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
                        placed.cacheKey = { ALTEngine::Renderer::ModelCatalog::Obj3d, Obj3DIndexForCrateType(crate.type, episodeThree) };
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
                        // Type 0x17 - the explosive barrel - gets no scale at
                        // all in FUN_0003765c, and the cryopod (0x20) is drawn
                        // by FUN_00037930, which has no scale call: both 1:1.
                        const float objectScale = (crate.type == 0x17 || crate.type == CRYOPOD_OBJECT_TYPE) ? 1.0f : 0.875f;
                        placed.scaleX = objectScale;
                        placed.scaleY = objectScale;
                        placed.scaleZ = objectScale;

                        // Types 0x15, 0x1c and 0x1d have no model (FUN_00037184
                        // gives them none): they stand in a window, a
                        // breakable wall or a curtain, blocking the cell and
                        // taking the shots, and what shows is the level's own
                        // faces - which their destruction steps on to the
                        // broken texture (FUN_00037ff8).
                        const bool invisibleObject = IsInvisibleObjectType(crate.type);
                        if (invisibleObject) { placed.visible = false; }

                        int familyBase = SwitchFamilyBase(crate.type, episodeThree);
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
                            // The record's own health and indestructible flag
                            // (FUN_00037184) - crates range from 16 to 80 on the
                            // first levels, barrels are 16.
                            target.health = ALTEngine::Game::ProjectileCollision::ObjectHealth(crate);
                            target.indestructible = ALTEngine::Game::ProjectileCollision::ObjectIndestructible(crate);
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

                        if (crate.type == CRYOPOD_OBJECT_TYPE && cryopodCoverReady)
                        {
                            placed.cacheKey = { ALTEngine::Renderer::ModelCatalog::LevelObject,
                                                ALTEngine::Formats::ModelIndices::Obj3D::StasisPodCover };
                        }
                        placedObjects.push_back(placed);
                        // A cryopod's cover tips open once activated (0x37bb0).
                        if (crate.type == CRYOPOD_OBJECT_TYPE)
                        {
                            cryopods.push_back({ ci, placedObjects.size() - 1, 0 });
                            if (!damageTargets.empty() && damageTargets.back().crateIndex == static_cast<int>(ci))
                            {
                                // Its task never looks at damage.
                                damageTargets.back().indestructible = true;
                            }
                        }

                        // Occupancy, as the original's object spawn does:
                        // byte 12 takes the type, byte 4 the record index.
                        size_t crateCell = static_cast<size_t>(crate.y) * level.header.mapLength + static_cast<size_t>(crate.x);
                        if (crateCell < level.collisionGrid.size())
                        {
                            level.collisionGrid[crateCell].unknown13 = crate.type;
                            level.collisionGrid[crateCell].unknown5 = static_cast<uint8_t>(&crate - level.crates.data());
                        }
                        // A window, breakable wall or curtain fills a block of
                        // cells (FUN_00037084): record byte 8 along x and byte
                        // 9 along z from its own. On L231 the grenade wall is
                        // 2 x 5, which is what makes the whole wall solid.
                        if (invisibleObject)
                        {
                            for (int dz = 0; dz < crate.unknown4; ++dz)
                            {
                                for (int dx = 0; dx < crate.unknown3; ++dx)
                                {
                                    const int x = crate.x + dx, z = crate.y + dz;
                                    if (x >= level.header.mapLength || z >= level.header.mapWidth) { continue; }
                                    auto& cell = level.collisionGrid[static_cast<size_t>(z) * level.header.mapLength + static_cast<size_t>(x)];
                                    cell.unknown13 = crate.type;
                                    cell.unknown5 = static_cast<uint8_t>(&crate - level.crates.data());
                                }
                            }
                            if (!damageTargets.empty() && damageTargets.back().crateIndex == static_cast<int>(ci))
                            {
                                damageTargets.back().cellsWide = std::max(1, static_cast<int>(crate.unknown3));
                                damageTargets.back().cellsDeep = std::max(1, static_cast<int>(crate.unknown4));
                            }
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

                        doorStates = ALTEngine::Game::Doors::Build(
                            level, ALTEngine::Game::Doors::TravelForLevel(digits), originX, originZ);

                        for (size_t d = 0; d < doorStates.size(); ++d)
                        {
                            const auto& door = level.doors[d];
                            auto& ds = doorStates[d];
                            ds.placedFirst = placedObjects.size();
                            ds.placedCount = 5;

                            for (int page = 0; page < 5; ++page)
                            {
                                ALTEngine::Renderer::ModelCacheKey key{
                                    ALTEngine::Renderer::ModelCatalog::LevelDoor,
                                    static_cast<int>(door.modelIndex) * 8 + page };

                                ALTEngine::Renderer::PlacedObject placed;
                                placed.cacheKey = key;
                                placed.x = ds.worldX;
                                placed.y = ds.startY;
                                placed.z = ds.worldZ;
                                placed.rotationRadians = 3.14159265f - static_cast<float>(door.rotation) * (3.14159265f / 4.0f);
                                placedObjects.push_back(placed);
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        SDL_Log("GameplayScreen: door setup failed: %s", e.what());
                    }

                    // Lifts. Their cells take the starting floor now; their
                    // platforms are the "L0" meshes in this .MAP (as ALTViewer
                    // reads them), drawn at the centre of the lift's first cell at
                    // its floor + 0x10, as FUN_00041ba4 draws them. Which mesh
                    // a lift uses is its record's byte 12 - an index into a
                    // model table (0x1a8354) the original fills from these
                    // sections, taken here to be their order. Not testable on
                    // the demo levels, which have no lifts.
                    lifts = ALTEngine::Game::Lifts::Build(level);
                    liftPlaced.clear();
                    if (!lifts.empty())
                    {
                        try
                        {
                            auto liftMeshes = ALTEngine::Formats::ModelLoader::Load(*mapPath, "L0");
                            auto levelTextures = ALTEngine::Formats::BndTextureLoader::Load(*gfxPath);
                            for (size_t m = 0; m < liftMeshes.size(); ++m)
                            {
                                auto groups = ALTEngine::Formats::BuildRenderMeshPerGroup(liftMeshes[m], levelTextures.uvRects);
                                for (size_t page = 0; page < groups.size(); ++page)
                                {
                                    if (groups[page].indices.empty() || page >= levelTextures.textures.size()) { continue; }
                                    ALTEngine::Renderer::ModelCacheKey key{
                                        ALTEngine::Renderer::ModelCatalog::LevelLift,
                                        static_cast<int>(m) * 8 + static_cast<int>(page) };
                                    ModelRenderer::RegisterMesh(key, groups[page], levelTextures.textures[page]);
                                }
                            }
                            for (const auto& lift : lifts)
                            {
                                const size_t first = placedObjects.size();
                                const int mesh = (static_cast<size_t>(lift.model) < liftMeshes.size()) ? lift.model : 0;
                                for (int page = 0; page < 5; ++page)
                                {
                                    ALTEngine::Renderer::PlacedObject placed;
                                    placed.cacheKey = { ALTEngine::Renderer::ModelCatalog::LevelLift, mesh * 8 + page };
                                    placed.x = lift.DrawX() - originX;
                                    placed.y = lift.PlatformY();
                                    placed.z = lift.DrawZ() - originZ;
                                    placedObjects.push_back(placed);
                                }
                                liftPlaced.emplace_back(first, placedObjects.size() - first);
                            }
                        }
                        catch (const std::exception& e)
                        {
                            SDL_Log("GameplayScreen: lift meshes not loaded: %s", e.what());
                        }
                        SDL_Log("GameplayScreen: %zu lift(s)", lifts.size());
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
                            if (crate.flags == 2) { continue; }   // holds an enemy, not pickups
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
                            const bool waitsOnTrigger = (pickup.triggerThreshold != 0 && !heldByCrate);

                            // Type 24 is remapped to 21 for both model and
                            // scale lookup.
                            int type = (pickup.type == 0x18) ? 0x15 : pickup.type;

                            ALTEngine::Renderer::PlacedObject placed;
                            placed.cacheKey = ALTEngine::Renderer::ModelPreviewSource::ForPickmod(cdDirectory, type).CacheKey();
                            placed.x = static_cast<float>(pickup.x) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originX;
                            placed.z = static_cast<float>(pickup.y) * GRID_CELL_TO_WORLD_UNITS + 256.0f - originZ;

                            // The floor at the cell's centre, where the item
                            // stands, plus DAT_000acde8's offset for its type.
                            placed.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                                           level, pickup.x * 512 + 256, pickup.y * 512 + 256)
                                     + static_cast<float>(ALTEngine::Screens::DroppedPickups::DropHeight(type));
                            placed.scaleX = placed.scaleY = placed.scaleZ = ALTEngine::Screens::DroppedPickups::DrawScale(type);

                            LivePickup live;
                            live.pickupIndex = static_cast<uint8_t>(pickupSlot);
                            live.placedIndex = placedObjects.size();
                            live.cellIndex = static_cast<int>(pickup.y) * level.header.mapLength + static_cast<int>(pickup.x);
                            // 0x18 becomes 0x15 as the record is read (FUN_0003b93c).
                            live.type = static_cast<uint8_t>(type);
                            live.amount = pickup.amount;
                            live.multiplier = pickup.multiplier;
                            live.waitingOnTrigger = waitsOnTrigger;
                            live.triggerCount = pickup.triggerCount;
                            live.triggerThreshold = pickup.triggerThreshold;
                            livePickups.push_back(live);

                            // Hidden until its trigger count is reached.
                            placed.visible = !waitsOnTrigger;
                            pickupPlaced.push_back(placedObjects.size());
                            placedObjects.push_back(placed);
                            if (!waitsOnTrigger) { spawned++; }
                        }
                        SDL_Log("GameplayScreen: %d of %zu pickups placed (rest are in crates or wait on a trigger)",
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
                            if (crate.flags == 2) { enemyCrates++; continue; }
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
                        // One white texel: a bullet's line takes its colour
                        // from the vertices alone (FUN_0002d3e4).
                        ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                            "line", std::vector<uint8_t>{ 255, 255, 255, 255 }, 1, 1);

                        // A window's glass, cut out of the level's own texture
                        // page as its own sheet, for the panes it breaks into.
                        glassSheetReady = false;
                        try
                        {
                            const auto glassSet = ALTEngine::Formats::BndTextureLoader::Load(*gfxPath);
                            if (const auto cut = GameplayLevelBuild::CutTextureRect(
                                    glassSet, static_cast<size_t>(GlassTextureIndex(digits))))
                            {
                                glassSheetReady = ModelRenderer::UploadSpriteSheet("glass", cut->rgba, cut->width, cut->height);
                            }
                        }
                        catch (const std::exception& e)
                        {
                            SDL_Log("GameplayScreen: no glass texture (%s)", e.what());
                        }

                        // EXPLGFX's barrel page - nine 84x84 frames.
                        if (!explosions.Load(cdDirectory))
                        {
                            SDL_Log("GameplayScreen: EXPLGFX.B16 not loaded - explosions will be silent-invisible");
                        }

                        // The enemy roster for this difficulty. Monsters parked
                        // off the playable map are crate contents and start out
                        // of play.
                        enemyGrenades.clear();
                        playerExplosives.clear();
                        minimapVisited.clear();
                        minimapAreas.clear();
                        enemies = ALTEngine::Screens::Enemies::Build(
                            level, static_cast<int>(difficultySettings.Get()), originX, originZ);
                        // Testing: Disable Enemy Spawn. An empty roster leaves
                        // the triggers, eggs, crates and vents nothing to let
                        // out - every one of them looks its monster up by
                        // index and does nothing when it is not there.
                        if (testingNoEnemies) { enemies.clear(); }
                        {
                            size_t waiting = 0;
                            for (const auto& e : enemies) { if (!e.active) { waiting++; } }
                            SDL_Log("GameplayScreen: %zu monsters, %zu active at start, %zu waiting on a trigger",
                                    enemies.size(), enemies.size() - waiting, waiting);
                        }

                        // Enemy artwork comes out of the LEVEL's own graphics
                        // file - the executable names no enemy sprite files at
                        // all, only the weapons and the level GFX sets.
                        // The face-crawl overlay. Its two halves are animations 16
                        // and 17 of the hugger's own table, with frames in
                        // FINGERS.B16.
                        if (faceHug.Load(cdDirectory))
                        {
                            faceHugSheets.clear();
                            const auto pieces = faceHug.AllFrames();
                            for (size_t i = 0; i < pieces.size(); ++i)
                            {
                                const std::string key = "facehug:" + std::to_string(i);
                                ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                                    key, faceHug.ToRgba(*pieces[i]), pieces[i]->width, pieces[i]->height);
                                faceHugSheets.push_back(key);
                            }
                            SDL_Log("FaceHugOverlay: %zu frames ready", faceHugSheets.size());
                        }
                        else
                        {
                            SDL_Log("FaceHugOverlay: FINGERS.B16 not loaded");
                        }

                        if (!enemySprites.Load(cdDirectory, digits, enemies))
                        {
                            SDL_Log("GameplayScreen: no enemy sprites loaded for level %s", digits.c_str());
                        }

                        objectState.assign(level.crates.size(), 0);

                        // The tally's totals, as the level builds them: each
                        // creature record by type (FUN_0002f3b0, those in
                        // crates too) and each object below 0x25
                        // (FUN_00037184); the secret areas are as many as the
                        // highest area number a cell carries (FUN_000282c8).
                        tally = {};
                        std::fill(secretsSeen.begin(), secretsSeen.end(), 0);
                        for (const auto& e : enemies)
                        {
                            if (e.type >= 0 && e.type < ALTEngine::Game::Progression::TALLY_SIZE) { tally.total[e.type]++; }
                        }
                        for (const auto& crate : level.crates)
                        {
                            if (crate.type < ALTEngine::Game::Progression::TALLY_SIZE) { tally.total[crate.type]++; }
                        }
                        for (const auto& cell : level.collisionGrid)
                        {
                            tally.secretsTotal = std::max<int>(tally.secretsTotal, cell.unknown6);
                        }
                        triggers = ALTEngine::Game::Triggers::System(level);

                        // What Save Game in the pause menu writes: this level,
                        // from its start, with the loadout the player brought.
                        {
                            ALTEngine::Game::SaveData checkpoint;
                            checkpoint.name = ALTEngine::Game::SaveGame::DefaultName(digits);
                            checkpoint.levelDigits = digits;
                            checkpoint.difficulty = static_cast<int>(difficultySettings.Get());
                            checkpoint.inventory = inventory;
                            SetCheckpoint(checkpoint);
                        }
                        // A mid-level save being loaded: the level as it was,
                        // over the level as built.
                        if (const std::string saved = TakePendingRestore(digits); !saved.empty())
                        {
                            SDL_Log("GameplayScreen: restoring the saved level state: %s",
                                    applySnapshot(saved) ? "done" : "not this level's, ignored");
                        }
                        ALTEngine::Screens::Enemies::ResetLevelState();
                        flowField.Reset(level);
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
        bool levelComplete = false;
        // The resupply levels' countdown (Game::ResupplyTimer, FUN_0003b518).
        ALTEngine::Game::ResupplyTimer::State resupply =
            ALTEngine::Game::ResupplyTimer::Start(ALTEngine::Game::Progression::CampaignIndex(digits));

        // Dead: one of the six death videos then GAMEOVER, and back to the menu
        // - see GameOverSequence. The clip is chosen by HOW you died, which
        // ApplyDamage records. Checked wherever damage is dealt; the sequence
        // itself runs once the frame is done.
        auto checkPlayerDeath = [&]() {
            if (!hudState.dead || playerDead) { return; }
            playerDead = true;
            running = false;
            deathVideoIndex = ALTEngine::Screens::GameOverSequence::DeathVideoForCause(hudState.lastDamageCause);
            SDL_Log("player killed (cause %d) - DEATH%d", hudState.lastDamageCause, deathVideoIndex);
        };

        // The 3D frame's texture, kept across frames (see the present below).
        SDL_Texture* frameTexture = nullptr;
        int frameTextureW = 0;
        int frameTextureH = 0;

        // What each trigger command does to this level. Built once; the
        // trigger system decides when they run and in what order.
        ALTEngine::Game::Triggers::Handlers triggerHandlers;
        triggerHandlers.door = [&](int doorIndex, bool isOpen, int amount, bool viaSwitch,
                                   ALTEngine::Game::Triggers::Source source) {
            if (doorIndex < 0 || static_cast<size_t>(doorIndex) >= doorStates.size()) { return false; }
            // Walking up to a door never opens it: it takes the use key, as in
            // the original. A failed door command aborts the rest of that
            // chain, which is what the old walk-on path did too.
            if (source == ALTEngine::Game::Triggers::Source::WalkOn) { return false; }
            return ALTEngine::Game::Doors::ApplyCommand(doorStates[static_cast<size_t>(doorIndex)], isOpen, amount, viaSwitch);
        };
        triggerHandlers.toggleLight = [&](int light, int delta) {
            // Arms (or advances) a light record's variant counter - what brings
            // a blink light to life, matching FUN_00029d44's add-and-clamp.
            ModelRenderer::ToggleLevelLight(cacheKey, light, delta);
            surfaceChanges.push_back({ 1, light, delta });
            SDL_Log("GameplayScreen: ToggleLight light=%d delta=%d", light, delta);
        };
        triggerHandlers.changeTexture = [&](int animator, int delta) {
            // The texture-animator counterpart of ToggleLight: what makes a
            // control panel change when its switch is thrown.
            ModelRenderer::ChangeLevelTexture(cacheKey, animator, delta);
            surfaceChanges.push_back({ 0, animator, delta });
            SDL_Log("GameplayScreen: ChangeTexture animator=%d delta=%d", animator, delta);
        };
        triggerHandlers.spawnMonster = [&](int monster, int amount) {
            if (ALTEngine::Screens::Enemies::TriggerSpawn(enemies, monster, amount))
            {
                SDL_Log("GameplayScreen: trigger spawned monster %d", monster);
                // A creature let out into a cell with no zones - a sealed duct
                // behind a wall vent, as L111's huggers are - could never see
                // the player there, so it would wait in the wall forever. It
                // comes out alerted instead; once in the room the usual sight
                // rules take over. NOT TRACED (Edward, 2026: they should come
                // out of the vents as you pass the triggers).
                for (auto& released : enemies)
                {
                    if (released.monsterIndex != monster || !released.active) { continue; }
                    const int cell = ALTEngine::Formats::CellIndex(level,
                        ToGridSpaceX(released.x, originX) >> 9, ToGridSpaceZ(released.z, originZ) >> 9);
                    if (ALTEngine::Game::ProjectileCollision::CellZones(level, cell) == 0)
                    {
                        released.ai.alerted = true;
                    }
                }
            }
        };
        // Commands 5 and 7: count toward a lift's threshold (Game/Lifts.h).
        triggerHandlers.lift = [&](int liftIndex, int amount, bool finalIncrementOnly) {
            if (liftIndex < 0 || static_cast<size_t>(liftIndex) >= lifts.size()) { return false; }
            return ALTEngine::Game::Lifts::AddToCounter(lifts[static_cast<size_t>(liftIndex)], amount, finalIncrementOnly);
        };
        triggerHandlers.spawnPickup = [&](int pickupIndex, int amount) {
            // FUN_0003bb14: add to the counter, clamped to the threshold, and the
            // pickup appears once they meet.
            for (auto& live : livePickups)
            {
                if (live.pickupIndex != pickupIndex || !live.waitingOnTrigger) { continue; }
                int count = live.triggerCount + amount;
                if (count > live.triggerThreshold) { count = live.triggerThreshold; }
                if (count < 0) { count = 0; }
                live.triggerCount = static_cast<uint8_t>(count);
                if (live.triggerCount == live.triggerThreshold)
                {
                    live.waitingOnTrigger = false;
                    if (live.placedIndex < placedObjects.size()) { placedObjects[live.placedIndex].visible = true; }
                    SDL_Log("GameplayScreen: trigger spawned pickup %d", pickupIndex);
                }
            }
        };
        triggerHandlers.activateObject = [&](int objectIndex) {
            const size_t oi = static_cast<size_t>(objectIndex);
            if (objectIndex < 0 || oi >= level.crates.size() || oi >= objectState.size()) { return false; }
            // Latches: a thrown switch fails, and the chain aborts on the failure.
            if (objectState[oi] != 0) { return false; }
            // Types 22 and 26 need a battery and consume one; type 24 is exempt.
            const uint8_t type = level.crates[oi].type;
            if (type == 22 || type == 26)
            {
                if (!inventory.HasBatteries())
                {
                    SDL_Log("GameplayScreen: switch %zu needs a battery", oi);
                    return false;
                }
                inventory.batteries--;
            }
            objectState[oi] = 1;
            // FUN_00036f4c's FUN_00040ba0(0x5c, -1): the sound of anything
            // being switched on - a switch, a cryopod.
            ALTEngine::Audio::SfxPlayer::PlaySlot(OBJECT_ACTIVATE_SOUND, digits.c_str(), cdDirectory);
            // Throwing a switch counts toward the mission (FUN_00036f4c).
            if (type < ALTEngine::Game::Progression::TALLY_SIZE) { tally.killed[type]++; }
            return true;
        };
        triggerHandlers.endLevel = [&]() {
            SDL_Log("GameplayScreen: End Level trigger fired");
            levelComplete = true;
        };
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
                    // Modern: Save Anywhere - the pause menu's Save Game keeps
                    // the level as it stands now.
                    if (saveAnywhere && levelReady && !playerDead)
                    {
                        ALTEngine::Game::SaveData checkpoint;
                        if (auto current = CurrentCheckpoint()) { checkpoint = *current; }
                        checkpoint.name = ALTEngine::Game::SaveGame::DefaultName(digits);
                        checkpoint.levelDigits = digits;
                        checkpoint.difficulty = static_cast<int>(difficultySettings.Get());
                        checkpoint.inventory = inventory;
                        checkpoint.snapshot = captureSnapshot();
                        SetCheckpoint(checkpoint);
                    }
                    const std::vector<uint8_t> pauseMapReveal = levelReady
                        ? ALTEngine::Game::CreatureSight::MapReveal(level, minimapVisited, minimapAreas, inventory.hasAutoMapper)
                        : std::vector<uint8_t>{};
                    PauseMenuResult pauseResult = PauseMenuScreen::Run(cdDirectory, language, missionLevelCode, inventory, audioSettings,
                                                                        renderSettings, resolutionSettings, difficultySettings, cameraSwaySettings, languageSettings, keyBindings,
                                                                      levelReady ? &level : nullptr,
                                                                      ToGridSpaceX(camera.x, originX) / 512.0f,
                                                                      ToGridSpaceZ(camera.z, originZ) / 512.0f,
                                                                      camera.yaw,
                                                                      &pauseMapReveal);
                    // Re-read on return: the pause menu can reach the same
                    // Options screen the boot menu does, so this can have
                    // changed while paused. Reading it once at level load
                    // meant a mid-game toggle silently did nothing.
                    {
                        ALTEngine::Bootstrap::Config modernConfig;
                        ALTEngine::Bootstrap::ModernSettings modern(modernConfig);
                                    freeLook = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FreeLook);
                        playerJumping = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::PlayerJumping);
                        fasterXenomorphs = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::FasterXenomorphs);
                        speedCompensationOff = !modern.IsActive(ALTEngine::Bootstrap::ModernFeature::SpeedCompensation);
                        saveAnywhere = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::SaveAnywhere);
                        liveMinimap = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::LiveMinimap);
                        {
                            ALTEngine::Bootstrap::TestingSettings testing(modernConfig);
                            testingNoEnemies = testing.IsOn(ALTEngine::Bootstrap::TestingOption::DisableEnemySpawn);
                            testingFaceIndex = testing.IsOn(ALTEngine::Bootstrap::TestingOption::FaceIndexDisplay);
                            testingGhost = testing.IsOn(ALTEngine::Bootstrap::TestingOption::GhostMode);
                            testingNoResupply = testing.IsOn(ALTEngine::Bootstrap::TestingOption::DisableResupplyTimer);
                        }
                        ignoreStun = modern.IsActive(ALTEngine::Bootstrap::ModernFeature::NoHitImmunity);
                        weaponFrameSeconds = ALTEngine::Bootstrap::WeaponPacingSettings(modernConfig).Get()
                            / ALTEngine::Screens::PlayerCamera::ORIGINAL_TIMER_HZ;

                        ModelRenderer::SetDrawDistanceFade(
                            !modern.IsActive(ALTEngine::Bootstrap::ModernFeature::RenderDistance));
                    }

                    // "Fully Loaded" is a toggle, not a one-shot: while it is on
                    // the loadout is topped up every tick, so "unlimited
                    // ammunition" actually stays unlimited as it is spent.
                    cheatFullyLoaded = pauseResult.cheatFullyLoaded;
                    // The lamp or goggles lit from the pause menu: sound 0x17
                    // as it starts to come up (0x29dd7).
                    if (pauseResult.useLight && levelReady)
                    {
                        const int lit = inventory.UseLight();
                        if (lit != 0)
                        {
                            ModelRenderer::SetLevelLightEquipment(cacheKey, lit);
                            ALTEngine::Audio::SfxPlayer::PlaySlot(LIGHT_ON_SOUND, digits.c_str(), cdDirectory);
                        }
                    }
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
                    else if (pauseResult.outcome == PauseMenuOutcome::LoadGame)
                    {
                        result.outcome = GameplayOutcome::LoadGame;
                        result.loadSlot = pauseResult.loadSlot;
                        running = false;
                    }
                    else if (levelReady)
                    {
                        SDL_SetWindowRelativeMouseMode(app.Window(), true);
                        float discardDx = 0.0f, discardDy = 0.0f;
                        SDL_GetRelativeMouseState(&discardDx, &discardDy); // drain any stale delta before the next real frame
                    }
                    // The pause menu's Options can change the SFX volume.
                    ALTEngine::Audio::SfxPlayer::SetVolume(audioSettings.SfxVolume());
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
                            const int slot = ALTEngine::Screens::PlayerInventoryState::SlotForSelectKey(i);
                            if (slot != selectedWeapon && inventory.Selectable(slot)) { weaponSwitch.Begin(slot); }
                        }
                        weaponKeyWasDown[static_cast<size_t>(i)] = down;
                    }

                    const bool nextDown = keys[keyBindings.GetKey(InputAction::NextWeapon)];
                    if (nextDown && !weaponKeyWasDown[5])
                    {
                        const int next = inventory.NextSelectable(selectedWeapon);
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
                    // The original starts at its rest pitch (0x00040512).
                    playerCam.pitch = PC::PITCH_REST;
                    playerCamSeeded = true;
                }

                // The player runs at the original's own logic rate (see
                // PlayerCamera::TICK_HZ), as do the doors and lifts below; the
                // creatures, particles and firing on every other tick. Every
                // constant in PlayerCamera.h is per tick at it.
                playerTickAccumulator += dt;
                const float playerTickLength = (speedCompensationOff ? 2.0f : 1.0f) / static_cast<float>(PC::TICK_HZ);
                int playerTicksThisFrame = 0;
                // Each pass's alternate-pass flag, in order, so the world's
                // tick below runs the same passes (see the world tick).
                std::vector<bool> passHalves;
                while (playerTickAccumulator >= playerTickLength)
                {
                    playerTickAccumulator -= playerTickLength;
                    playerTicksThisFrame++;
                    if (!levelComplete && !playerDead) { levelSeconds += playerTickLength; }
                    // The original's alternate pass (see logicTick).
                    const bool halfPass = ((logicTick++ & 1u) == 0) || speedCompensationOff;
                    passHalves.push_back(halfPass);
                    const bool fasterXenoPass = fasterXenomorphs && !halfPass
                        && (fasterXenoOffPass++ % FASTER_XENO_OFF_PASS_EVERY) == 0;
                    const bool creaturePass = halfPass || fasterXenoPass;
                    // The resupply countdown's task runs every pass, the timer
                    // interrupt counting its seconds (one or two a tick).
                    // Testing: Disable Resupply Timer holds it where it is.
                    if (resupply.active && !resupply.expired && !playerDead && !testingNoResupply)
                    {
                        const int cue = ALTEngine::Game::ResupplyTimer::Pass(resupply, speedCompensationOff ? 2 : 1);
                        if (cue >= 0) { ALTEngine::Audio::SfxPlayer::PlaySlot(cue, digits.c_str(), cdDirectory); }
                        if (resupply.expired) { levelComplete = true; }
                    }
                    // The original's last pass, for the weapon: firing, the
                    // switch and the animation. A frame shorter than a tick
                    // still gets one weapon pass a tick, as the original's
                    // fastest frame still had one pass.
                    weaponClock += playerTickLength;
                    const bool weaponPass = weaponClock >= weaponFrameSeconds;
                    if (weaponPass) { weaponClock = std::min(weaponClock - weaponFrameSeconds, weaponFrameSeconds); }

                    prevCamX = camera.x;
                    prevCamZ = camera.z;
                    prevCamYaw = PC::ToRadians(playerCam.viewYaw);
                    prevCamValid = true;

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

                    // RunModifier is a hold; RunMode is a toggle. The original
                    // flips a latched bit (DAT_000ad584 ^= 2) on each press of
                    // Caps Lock, once per press, and ORs it with either Shift
                    // held into the run bit (FUN_0004d438).
                    {
                        const bool runModeDown = keys[keyBindings.GetKey(InputAction::RunMode)];
                        if (runModeDown && !runModeWasDown) { runModeToggled = !runModeToggled; }
                        runModeWasDown = runModeDown;
                    }
                    pcInput.run = keys[keyBindings.GetKey(InputAction::RunModifier)] || runModeToggled;

                    pcInput.turn180 = keys[keyBindings.GetKey(InputAction::Turnaround)];
                    pcInput.terrainSpeedTicks = hudState.terrainSpeed;

                    // Manual look, on the original's own Look Up / Look Down
                    // keys (FUN_0003efcc). Held, they tilt the view - and with it
                    // the aim - by up to 0x80 either way; released, the pitch
                    // eases back to rest. This is how anything on the floor gets
                    // shot without Free Look.
                    pcInput.lookUp = keys[keyBindings.GetKey(InputAction::LookUp)];

                    // Modern: Player Jumping. OURS - the original has no jump.
                    // A press on the ground lifts the player, the original's
                    // own gravity (0xc a tick, a creature's) brings them down,
                    // and the floor follow below keeps them over the floor and
                    // under the ceiling.
                    {
                        const bool jumpDown = playerJumping && keys[keyBindings.GetKey(InputAction::Jump)];
                        if (jumpDown && !jumpHeld && jumpOffset <= 0.0f && jumpVelocity == 0.0f)
                        {
                            // Twice the lift and four times the gravity on a
                            // tick twice as long: the same hop.
                            jumpVelocity = PC::JUMP_LIFT * (speedCompensationOff ? 2.0f : 1.0f);
                        }
                        jumpHeld = jumpDown;
                        if (jumpVelocity != 0.0f || jumpOffset > 0.0f)
                        {
                            jumpOffset += jumpVelocity;
                            jumpVelocity -= PC::JUMP_GRAVITY * (speedCompensationOff ? 4.0f : 1.0f);
                            if (jumpOffset <= 0.0f) { jumpOffset = 0.0f; jumpVelocity = 0.0f; }
                        }
                    }
                    pcInput.lookDown = keys[keyBindings.GetKey(InputAction::LookDown)];
                    pcInput.look = pcInput.lookUp || pcInput.lookDown;

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
                        if (const auto* cell = ALTEngine::Formats::CellAt(level, cx, cz))
                        {
                            cellAttribute = cell->attribute;
                            // A secret area found: the original marks the
                            // camera's cell's area each drawn frame (0x274e1).
                            if (cell->unknown6 != 0) { secretsSeen[cell->unknown6] = 1; }
                        }
                    }
                    // The hazard flag selects which footstep sound plays. The
                    // damage attributes are the ones FUN_0003dff0 reacts to.
                    const bool onHazardCell = (cellAttribute == 7 || cellAttribute == 8 || cellAttribute == 0xc);

                    PC::Tick(playerCam, pcInput, cellAttribute, onHazardCell, cameraSway, freeLook);

                    // The original's own mover (Game::PlayerMover, FUN_0003dff0):
                    // the tick's step, plus a slippery ramp's push and any
                    // knockback, x first and then z.
                    float stepX = 0.0f, stepZ = 0.0f;
                    PC::MovementStep(playerCam, stepX, stepZ);
                    {
                        namespace PM = ALTEngine::Game::PlayerMover;
                        int pushX = 0, pushZ = 0;
                        if (!testingGhost && PM::RampPush(cellAttribute, playerCam.speed, playerCam.running, pushX, pushZ))
                        {
                            stepX += static_cast<float>(pushX);
                            stepZ += static_cast<float>(pushZ);
                        }
                        if (knockbackStrength > 0)
                        {
                            stepX += PC::Sin(knockbackYaw) / 4096.0f * static_cast<float>(knockbackStrength);
                            stepZ -= PC::Cos(knockbackYaw) / 4096.0f * static_cast<float>(knockbackStrength);
                            knockbackStrength -= PM::KNOCKBACK_FADE;
                            if (knockbackStrength < 0) { knockbackStrength = 0; }
                        }
                    }
                    if (stepX != 0.0f || stepZ != 0.0f)
                    {
                        // Testing: Ghost Mode. Straight through everything, and
                        // up or down along the view's pitch by however far the
                        // step goes the way the view faces.
                        if (testingGhost)
                        {
                            camera.x += stepX;
                            camera.z += stepZ;
                            if (ghostActive)
                            {
                                const float along = stepX * std::sin(camera.yaw) - stepZ * std::cos(camera.yaw);
                                const float pitch = std::clamp(camera.pitch, -1.3f, 1.3f);
                                ghostFeetY += along * std::tan(pitch);
                            }
                        }
                        else if (levelReady)
                        {
                            namespace PM = ALTEngine::Game::PlayerMover;
                            PM::World mw;
                            mw.cell = [&](int x, int z) { return ALTEngine::Formats::CellAt(level, x >> 9, z >> 9); };
                            mw.floor = [&](int x, int z) {
                                return static_cast<int>(ALTEngine::Formats::FindFloorHeightGridSpace(level, x, z));
                            };
                            mw.ceiling = [&](int x, int z) {
                                const auto* c = ALTEngine::Formats::CellAt(level, x >> 9, z >> 9);
                                return c ? static_cast<int>(c->ceilingHeight) * 32 : 0;
                            };
                            // A creature's cell (+0xc). The player's own mark is
                            // lifted before it moves (0x3e008).
                            const int ownCellX = ToGridSpaceX(camera.x, originX) >> 9;
                            const int ownCellZ = ToGridSpaceZ(camera.z, originZ) >> 9;
                            mw.occupied = [&](int x, int z) {
                                // Including a creature that shares it: clearing
                                // the player's mark clears the cell.
                                if ((x >> 9) == ownCellX && (z >> 9) == ownCellZ) { return false; }
                                // An object - crate, barrel, switch - stamps its
                                // type into the cell at spawn (+0xc).
                                if (const auto* c = ALTEngine::Formats::CellAt(level, x >> 9, z >> 9))
                                {
                                    if (c->unknown13 != 0 && c->unknown13 != 0xFF) { return true; }
                                }
                                for (const auto& other : enemies)
                                {
                                    if (!other.active || !other.alive || other.pouncing || ALTEngine::Screens::IsVent(other.type)) { continue; }
                                    if ((ToGridSpaceX(other.x, originX) >> 9) == (x >> 9)
                                        && (ToGridSpaceZ(other.z, originZ) >> 9) == (z >> 9)) { return true; }
                                }
                                return false;
                            };
                            const int gx = ToGridSpaceX(camera.x, originX);
                            const int gz = ToGridSpaceZ(camera.z, originZ);
                            const int gy = static_cast<int>(std::lround(playerFeetY));
                            const int nx = PM::MoveX(mw, gx, gz, gy, static_cast<int>(std::lround(stepX)));
                            const int nz = PM::MoveZ(mw, nx, gz, gy, static_cast<int>(std::lround(stepZ)));
                            // Exactly where the mover put the player. Keeping a
                            // fraction left the position a unit off the slide's
                            // 200 clearance, so the cross-axis probe caught the
                            // wall and the slide along it stuttered.
                            camera.x = static_cast<float>(nx) - originX;
                            camera.z = static_cast<float>(nz) - originZ;
                        }
                        else
                        {
                            camera.x += stepX;
                            camera.z += stepZ;
                        }
                    }

                    // Harmful floors (FUN_0003dff0), only while standing on the
                    // floor - the height under the player did not change this
                    // tick.
                    if (levelReady)
                    {
                        const int fgx = ToGridSpaceX(camera.x, originX);
                        const int fgz = ToGridSpaceZ(camera.z, originZ);
                        const float floorNow = ALTEngine::Formats::FindFloorHeightGridSpace(level, fgx, fgz);
                        if (floorNow == playerFeetY && !testingGhost)
                        {
                            if (const auto* underfoot = ALTEngine::Formats::CellAt(level, fgx >> 9, fgz >> 9))
                            {
                                hudState.ApplyFloorHazard(underfoot->attribute);
                                checkPlayerDeath();
                            }
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
                    // On the weapon's own (drawing-pass) rate, like its animation.
                    const int swapTo = weaponPass ? weaponSwitch.Tick() : -1;
                    if (swapTo >= 0)
                    {
                        const int equipped = inventory.Equip(swapTo);
                        if (equipped >= 0)
                        {
                            selectedWeapon = equipped;
                            // FUN_000401d0 puts the new weapon in state 0. Without
                            // this an automatic switch carried the empty weapon's
                            // state across, and the new one could not fire.
                            weaponRuntime.SetState(WS::STATE_IDLE);
                        }
                    }
                    weaponView.SetSwitchOffset(weaponSwitch.Offset());

                    weaponRuntime.weapon = hudState.currentWeapon;
                    weaponRuntime.stateChanged = false;

                    // Firing happens on the weapon pass only (FUN_0003d5b0 runs
                    // on the drawing pass); a press in between is kept.
                    if (firePressed) { firePressPending = true; }
                    const bool onPress = WS::Def(weaponRuntime.weapon).primary == WS::FIRE_ON_PRESS;
                    const bool triggerActive = weaponPass && (onPress ? firePressPending : fireHeld);
                    if (weaponPass) { firePressPending = false; }

                    if (triggerActive && WS::TryPrimaryFire(weaponRuntime, inventory))
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
                            P::TYPE_SMARTGUN, // 3 smartgun     FUN_0002c1d4
                            P::TYPE_FLAME,    // 4 flamethrower FUN_0002c0a0 / FUN_0002bcac
                        };
                        const int projectileType = TYPE_FOR_WEAPON[hudState.currentWeapon];

                        // The pulse rifle, flamethrower and smartgun each put
                        // THREE projectiles out per pull, with the later ones
                        // launched faster (0x80 apart) so a burst strings out in
                        // flight rather than travelling as a clump.
                        // FIRE ALONG WHERE THE PLAYER IS ACTUALLY LOOKING.
                        //
                        // This used playerCam.viewPitch, which is only the
                        // authoritative pitch when the original's automatic ramp
                        // pitch is driving it. With Free Look on, the mouse writes
                        // camera.pitch directly and PlayerCamera never touches its
                        // own - so viewPitch sat at whatever the ramps last left
                        // it, usually zero, and every round left the barrel flat
                        // no matter how far up or down the player aimed. Rounds
                        // travelled on two axes only (Edward, 2026).
                        //
                        // camera.pitch is the one the renderer builds the view
                        // from, so it is the one to fire along. Converted back
                        // into the 4096-per-turn units the projectile maths uses.
                        const int firingPitch = static_cast<int>(std::lround(
                            camera.pitch * (PC::ANGLE_UNITS / 6.28318530718f))) & PC::ANGLE_MASK;

                        // The smartgun's lock-on list, nearest first (FUN_0002c12c).
                        namespace SGT = ALTEngine::Screens::SmartgunTargeting;
                        std::vector<SGT::Candidate> smartTargets;
                        if (projectileType == P::TYPE_SMARTGUN)
                        {
                            const int playerCell = ALTEngine::Formats::CellIndex(
                                level, ToGridSpaceX(camera.x, originX) >> 9, ToGridSpaceZ(camera.z, originZ) >> 9);
                            std::vector<SGT::Creature> creatures;
                            creatures.reserve(enemies.size());
                            for (const auto& enemy : enemies)
                            {
                                const int enemyCell = ALTEngine::Formats::CellIndex(
                                    level, ToGridSpaceX(enemy.x, originX) >> 9, ToGridSpaceZ(enemy.z, originZ) >> 9);
                                creatures.push_back({ enemy.x, enemy.z, enemy.type, enemy.state, enemy.distance,
                                                      enemy.active && enemy.alive
                                                          && ALTEngine::Game::ProjectileCollision::ShareZone(level, playerCell, enemyCell) });
                            }
                            smartTargets = SGT::FindTargets(creatures, camera.x, camera.z, playerCam.viewYaw);
                        }

                        const int shots = P::ShotsPerPull(projectileType);
                        for (int shot = 0; shot < shots; ++shot)
                        {
                            // Every pulse round costs one (FUN_0002bc18 decrements
                            // per projectile and stops when the magazine runs
                            // dry). TryPrimaryFire has paid for the first.
                            if (shot > 0 && projectileType == P::TYPE_PULSE)
                            {
                                if (inventory.pulseRifle.rounds <= 0) { break; }
                                inventory.pulseRifle.rounds--;
                            }
                            // The smartgun likewise pays per round (FUN_0002c1d4).
                            if (shot > 0 && projectileType == P::TYPE_SMARTGUN)
                            {
                                if (inventory.smartGun.rounds <= 0) { break; }
                                inventory.smartGun.rounds--;
                            }

                            // A smartgun round with a target is launched at it.
                            if (projectileType == P::TYPE_SMARTGUN && static_cast<size_t>(shot) < smartTargets.size())
                            {
                                const auto& target = enemies[static_cast<size_t>(smartTargets[static_cast<size_t>(shot)].index)];
                                const float sy = static_cast<float>(PC::Sin(playerCam.viewYaw)) / PC::TRIG_ONE;
                                const float cy = static_cast<float>(PC::Cos(playerCam.viewYaw)) / PC::TRIG_ONE;
                                const float lx = camera.x + sy * SGT::LAUNCH_AHEAD;
                                const float ly = camera.y - SGT::LAUNCH_DROP;
                                const float lz = camera.z - cy * SGT::LAUNCH_AHEAD;
                                const float aimY = target.y + static_cast<float>(
                                    ALTEngine::Game::ProjectileCollision::CreatureExtents(target.type).y) / 2.0f;
                                const SGT::Vec v = SGT::AimVelocity(lx, ly, lz, target.x, aimY, target.z,
                                                                    static_cast<float>(SGT::SPEED + playerCam.speed));
                                particles.SpawnAimed(projectileType, lx, ly, lz, v.x, v.y, v.z);
                                continue;
                            }

                            // The pulse rifle's later rounds go faster, so a burst
                            // strings out, as do untargeted smartgun rounds; the
                            // flamethrower has its own scatter (see SpawnProjectile).
                            const int launchSpeed = (projectileType == P::TYPE_FLAME)
                                ? playerCam.speed : playerCam.speed + shot * P::BurstSpeedStep;
                            P::Particle* round = particles.SpawnProjectile(
                                projectileType, camera.x, camera.y, camera.z,
                                playerCam.viewYaw, firingPitch, launchSpeed);

                            // Flamethrower particle n (1, 2) lives 0x10 - n ticks
                            // and starts n steps out (FUN_0002bcac). The steps go
                            // through the particle pass's own collision test (see
                            // Particle::preSteps), as the original's call
                            // FUN_0002a448 after each one; leaving the map drops it.
                            if (round && projectileType == P::TYPE_FLAME)
                            {
                                const int n = shot + 1;
                                round->life = P::FlameLife(shot);
                                round->strength = round->life;
                                const float ex = round->x + round->vx * static_cast<float>(n);
                                const float ez = round->z + round->vz * static_cast<float>(n);
                                if (!ALTEngine::Formats::CellAt(level, ToGridSpaceX(ex, originX) >> 9,
                                                                ToGridSpaceZ(ez, originZ) >> 9))
                                {
                                    round->alive = false;
                                }
                                else
                                {
                                    round->preSteps = n;
                                }
                            }
                        }

                        // Only the pistol and the shotgun eject anything:
                        // FUN_0002bb74 calls FUN_0002b37c(0), a 0x15 casing, and
                        // FUN_0002bbc0 calls it with 1, a 0x14 shell.
                        // The shotgun kicks the player back (FUN_0002bbc0: 0x40).
                        if (projectileType == P::TYPE_SHOTGUN)
                        {
                            knockbackStrength = ALTEngine::Game::PlayerMover::KNOCKBACK_SHOTGUN;
                            knockbackYaw = (playerCam.viewYaw + 0x800) & PC::ANGLE_MASK;
                        }
                        if (projectileType == P::TYPE_PISTOL || projectileType == P::TYPE_SHOTGUN)
                        {
                            particles.SpawnCasing(projectileType == P::TYPE_PISTOL ? P::TYPE_CASING_A : P::TYPE_CASING_B,
                                                  camera.x, camera.y, camera.z,
                                                  playerCam.viewYaw, playerCam.speed, firingPitch);
                        }
                    }

                    // Reload and out-of-ammo run only from idle, as a separate
                    // pass after the fire keys - the same order FUN_0003efcc has.
                    //
                    // `canSwitchAway` is FUN_00038c78: is there another weapon
                    // that can still fire. If not, the original's fallback - the
                    // pistol topped up to one round. If so, the weapon goes empty
                    // and is switched for the best one left (FUN_00038e64).
                    {
                        const int best = inventory.BestOtherSelectable(selectedWeapon);
                        WS::UpdateIdle(weaponRuntime, inventory, best >= 0);
                        if (weaponRuntime.state == WS::STATE_EMPTY && best >= 0 && !weaponSwitch.Busy())
                        {
                            weaponSwitch.Begin(best);
                        }
                    }
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

                    // Both throws start from the player and fly along the view
                    // (FUN_0002c4e0 / FUN_0002c830): (0, -0x18, speed) turned by
                    // the pitch, then by the camera's yaw-and-roll triple, from
                    // below the eye - the charge 0x100 below it, the grenade
                    // 0x80, and both moved along x by the walk's dip
                    // (DAT_000b0b4e).
                    //
                    // The camera's roll is 0x800, half a turn (DAT_000b0a54, set
                    // once at 0x0004050b), so its view space has y pointing down
                    // the screen: the -0x18 is 0x18 UP the screen. A throw
                    // leaves below the eye rising at 0x18 on top of the view's
                    // pitch, and gravity (one a tick, 0x2dcf9) turns it over -
                    // the arc. This had the 0x18 pointing down, so a level throw
                    // only fell away from the start.
                    auto throwExplosive = [&](bool charge) {
                        const int pitch = static_cast<int>(std::lround(
                            camera.pitch * (PC::ANGLE_UNITS / 6.28318530718f))) & PC::ANGLE_MASK;
                        // The grenade at the player's speed + 0x100, at most
                        // 0x100; the charge at + 0x40, at most 0x80.
                        const int speed = charge ? std::min(playerCam.speed + 0x40, 0x80)
                                                 : std::min(playerCam.speed + 0x100, 0x100);
                        const float ps = static_cast<float>(PC::Sin(pitch)) / PC::TRIG_ONE;
                        const float pc = static_cast<float>(PC::Cos(pitch)) / PC::TRIG_ONE;
                        const float ys = static_cast<float>(PC::Sin(playerCam.viewYaw)) / PC::TRIG_ONE;
                        const float yc = static_cast<float>(PC::Cos(playerCam.viewYaw)) / PC::TRIG_ONE;
                        // Port pitch is positive up, the view's (0, up 0x18, speed).
                        const float forward = static_cast<float>(speed) * pc - 0x18 * ps;
                        PlayerExplosive e;
                        e.charge = charge;
                        e.x = camera.x + static_cast<float>(playerCam.throwOffsetX);
                        e.y = camera.y - (charge ? 0x100 : 0x80);
                        e.z = camera.z;
                        e.vx = forward * ys;
                        e.vz = -forward * yc;
                        e.vy = static_cast<float>(speed) * ps + 0x18 * pc;
                        e.bounce = charge ? 8 : 0x10;
                        e.life = charge ? 0x40 : 0x30;
                        playerExplosives.push_back(e);
                    };

                    // Fire 2 with anything but the pulse rifle throws a seismic
                    // charge (FUN_0003d794): the noise 0x280 and sound 9, with no
                    // change to the weapon's own state. It never did anything.
                    if (altFirePressed
                        && hudState.currentWeapon != ALTEngine::Renderer::HUD_GRENADE_WEAPON
                        && weaponRuntime.state == WS::STATE_IDLE
                        && inventory.seismicCharges > 0)
                    {
                        inventory.seismicCharges--;
                        weaponRuntime.noise = WS::GRENADE_NOISE;
                        ALTEngine::Audio::SfxPlayer::PlaySlot(SEISMIC_THROW_SOUND, digits.c_str(), cdDirectory);
                        throwExplosive(true);
                    }

                    if (altFirePressed
                        && hudState.currentWeapon == ALTEngine::Renderer::HUD_GRENADE_WEAPON
                        && weaponRuntime.state == WS::STATE_IDLE
                        && inventory.pulseRifle.grenades > 0)
                    {
                        inventory.pulseRifle.grenades--;
                        // The grenade itself, which until now was never launched.
                        throwExplosive(false);
                        // And the grenade, less (0x2c52c: 0x20).
                        knockbackStrength = ALTEngine::Game::PlayerMover::KNOCKBACK_GRENADE;
                        knockbackYaw = (playerCam.viewYaw + 0x800) & PC::ANGLE_MASK;
                        weaponRuntime.SetState(WS::STATE_GRENADE);
                        weaponRuntime.noise = WS::GRENADE_NOISE;
                    }

                    if (weaponRuntime.stateChanged && WS::StateHasAnimation(weaponRuntime.state))
                    {
                        weaponView.PlayState(weaponRuntime.state);
                    }

                    weaponView.SetCameraDip(playerCam.bobOffsetY * 64);
                    if (weaponPass && weaponView.Tick())
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
                        // The footprint probe from FUN_000315f0: the candidate is
                        // pushed MOVE_PROBE_LEAD along the axis of travel, and
                        // three cells are sampled across the other axis at -200,
                        // 0 and +200. A creature therefore needs a 400-unit span
                        // clear, which is why they do not clip corners.
                        // CELL OCCUPANCY (+0x0c), kept the original's way: the
                        // player's cell carries 0xff (its mover sets it every
                        // tick, 0x3e3d6) and every live creature marks its own.
                        // A creature lifts its own mark before it moves or looks
                        // (FUN_00031f3c, FUN_0003231c) and marks wherever it ends
                        // up, so creatures queue behind one another, stop at the
                        // player, and block each other's line of fire.
                        //
                        // Rebuilt every tick rather than read from the map: the
                        // map's own +0x0c bytes are stale (see below), so the
                        // runtime must be writing live entities over them.
                        std::vector<uint8_t> occupancy(level.collisionGrid.size(), 0);
                        // Which creature a mark of 1 stands for (the index byte
                        // the original keeps at +4 alongside it), for blasts.
                        std::vector<int> markedCreature(level.collisionGrid.size(), -1);
                        auto occupancyCell = [&](float x, float z) {
                            return ALTEngine::Formats::CellIndex(level, ToGridSpaceX(x, originX) >> 9,
                                                                 ToGridSpaceZ(z, originZ) >> 9);
                        };
                        auto mark = [&](int index, uint8_t value) {
                            if (index >= 0 && static_cast<size_t>(index) < occupancy.size()) { occupancy[static_cast<size_t>(index)] = value; }
                        };
                        auto markCreature = [&](int index, const ALTEngine::Screens::Enemies::Enemy* creature) {
                            mark(index, creature ? 1 : 0);
                            if (index >= 0 && static_cast<size_t>(index) < markedCreature.size())
                            {
                                markedCreature[static_cast<size_t>(index)] = creature ? static_cast<int>(creature - enemies.data()) : -1;
                            }
                        };
                        auto marked = [&](int index) {
                            return index >= 0 && static_cast<size_t>(index) < occupancy.size()
                                && occupancy[static_cast<size_t>(index)] != 0;
                        };
                        // Objects first: every crate, barrel, switch and wall
                        // block keeps its type in the cell from its spawn until
                        // it is destroyed, and creatures are stopped by it as
                        // by anything else there (FUN_000315f0, FUN_0003231c
                        // refuse any non-zero +0x0c).
                        for (size_t c = 0; c < occupancy.size() && c < level.collisionGrid.size(); ++c)
                        {
                            const uint8_t stamp = level.collisionGrid[c].unknown13;
                            if (stamp >= 0x14 && stamp != 0xFF) { occupancy[c] = stamp; }
                        }
                        mark(occupancyCell(camera.x, camera.z), 0xff);
                        // Vents stand in walls and floors and never move: they
                        // don't take part. Nor does a hugger on the player's
                        // face: the pounce clears its cell and leaves the
                        // creature list's cell tests (FUN_00030918's handler
                        // never marks one).
                        for (const auto& other : enemies)
                        {
                            if (other.active && other.alive && !other.pouncing && !ALTEngine::Screens::IsVent(other.type))
                            {
                                markCreature(occupancyCell(other.x, other.z), &other);
                            }
                        }

                        // The flow field follows the player's node (cell byte +6).
                        {
                            const auto* playerCell = ALTEngine::Formats::CellAt(
                                level, ToGridSpaceX(camera.x, originX) >> 9, ToGridSpaceZ(camera.z, originZ) >> 9);
                            if (playerCell) { flowField.Update(playerCell->unknown7); }
                        }

                        // What the common creature task asks of the level, in
                        // world coordinates. `aiCreature` is the one being run.
                        const ALTEngine::Screens::Enemies::Enemy* aiCreature = nullptr;
                        ALTEngine::Screens::CreatureAI::World aiWorld;
                        auto walkableAt = [&](int x, int z, bool allowPlayer) {
                            const int gx = ToGridSpaceX(static_cast<float>(x), originX);
                            const int gz = ToGridSpaceZ(static_cast<float>(z), originZ);
                            if (gx < 0 || gz < 0) { return false; }
                            const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                            if (!cell) { return false; }
                            if (cell->attribute >= 0x14 && cell->attribute <= 0x2c) { return false; }
                            const int index = ALTEngine::Formats::CellIndex(level, gx >> 9, gz >> 9);
                            const uint8_t mark = (index >= 0 && static_cast<size_t>(index) < occupancy.size())
                                ? occupancy[static_cast<size_t>(index)] : 0;
                            return mark == 0 || (allowPlayer && mark == 0xff);
                        };
                        aiWorld.cellOpenOrPlayer = [&](int x, int z) { return walkableAt(x, z, true); };
                        aiWorld.cellOpen = [&](int x, int z) { return walkableAt(x, z, false); };
                        aiWorld.lineTo = [&](int x, int z, int distance) {
                            if (!aiCreature) { return false; }
                            namespace CS = ALTEngine::Game::CreatureSight;
                            CS::Query q;
                            q.x = static_cast<float>(ToGridSpaceX(aiCreature->x, originX));
                            q.z = static_cast<float>(ToGridSpaceZ(aiCreature->z, originZ));
                            q.y = aiCreature->y;
                            q.targetX = static_cast<float>(ToGridSpaceX(static_cast<float>(x), originX));
                            q.targetZ = static_cast<float>(ToGridSpaceZ(static_cast<float>(z), originZ));
                            q.distance = distance;
                            q.halfHeight = ALTEngine::Game::ProjectileCollision::CreatureExtents(aiCreature->type).y;
                            q.ceilingCrawler = aiCreature->onCeiling;
                            q.tallType = (aiCreature->type == 7);
                            q.occupancy = &occupancy;
                            return CS::LineOfFire(level, q);
                        };
                        aiWorld.nodeAt = [&](int x, int z) {
                            const auto* cell = ALTEngine::Formats::CellAt(
                                level, ToGridSpaceX(static_cast<float>(x), originX) >> 9,
                                ToGridSpaceZ(static_cast<float>(z), originZ) >> 9);
                            return cell ? static_cast<int>(cell->unknown7) : -1;
                        };
                        aiWorld.nodeToward = [&](int node) { return flowField.Step(node, true); };
                        aiWorld.nodeAway = [&](int node) { return flowField.Step(node, false); };
                        aiWorld.nodePosition = [&](int node, int& x, int& z) {
                            int gx = 0, gz = 0;
                            if (!flowField.Position(node, gx, gz)) { return false; }
                            x = static_cast<int>(std::lround(static_cast<float>(gx) - originX));
                            z = static_cast<int>(std::lround(static_cast<float>(gz) - originZ));
                            return true;
                        };
                        aiWorld.random = [&]() {
                            creatureRandom = creatureRandom * 1103515245u + 12345u;
                            return static_cast<int>((creatureRandom >> 16) & 0xffff);
                        };

                        auto enemyAxisClear = [&](float x, float z, bool alongX, float velocity, float halfHeight, bool hanging) {
                            namespace EB = ALTEngine::Screens::EnemyBehaviour;
                            // FUN_000315f0's height tests run against the floor
                            // the creature stands on now.
                            const float standing = ALTEngine::Formats::FindFloorHeightGridSpace(
                                level, ToGridSpaceX(x, originX), ToGridSpaceZ(z, originZ));
                            // A creature on the ceiling measures the ceiling
                            // instead (FUN_000315f0's +0x6c bit 8 arm): no fall
                            // of it by more than 0x200, the probes within 0x200
                            // of one another, and no headroom test.
                            float standingCeiling = 0.0f;
                            if (hanging)
                            {
                                const auto* here = ALTEngine::Formats::CellAt(level, ToGridSpaceX(x, originX) >> 9, ToGridSpaceZ(z, originZ) >> 9);
                                standingCeiling = here ? static_cast<float>(here->ceilingHeight) * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT : 0.0f;
                            }
                            float highest = -1e9f, lowest = 1e9f;
                            const float lead = (velocity >= 0.0f) ? EB::MOVE_PROBE_LEAD
                                                                  : -EB::MOVE_PROBE_LEAD;
                            const float probeX = alongX ? x + lead : x;
                            const float probeZ = alongX ? z : z + lead;

                            for (int side = -1; side <= 1; ++side)
                            {
                                const float offset = static_cast<float>(side) * EB::MOVE_FOOTPRINT_HALF;
                                const float sx = alongX ? probeX : probeX + offset;
                                const float sz = alongX ? probeZ + offset : probeZ;

                                const int cellX = ToGridSpaceX(sx, originX) >> 9;
                                const int cellZ = ToGridSpaceZ(sz, originZ) >> 9;
                                const auto* cell = ALTEngine::Formats::CellAt(level, cellX, cellZ);
                                if (!cell) { return false; }

                                if (EB::AttributeBlocks(cell->attribute)) { return false; }

                                // AN OCCUPIED CELL IS REFUSED (FUN_000315f0): the
                                // player's, so a creature stops at the edge of it
                                // and a leaping hugger drops there rather than
                                // sailing past, and other creatures'. The mover's
                                // own mark is already lifted - in a cell shared
                                // with the player, the player's with it.
                                if (marked(ALTEngine::Formats::CellIndex(level, cellX, cellZ))) { return false; }

                                // HEIGHTS (FUN_000315f0): no probe may rise more
                                // than 0x100 above the creature's floor, and the
                                // ceiling over each must clear the creature's own
                                // half height above that floor.
                                const float floorHere = ALTEngine::Formats::FindFloorHeightGridSpace(
                                    level, ToGridSpaceX(sx, originX), ToGridSpaceZ(sz, originZ));
                                const float ceilingHere = static_cast<float>(cell->ceilingHeight)
                                                        * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
                                if (hanging)
                                {
                                    if (ceilingHere < standingCeiling && standingCeiling - ceilingHere > EB::CEILING_STEP) { return false; }
                                    highest = std::max(highest, ceilingHere);
                                    lowest = std::min(lowest, ceilingHere);
                                    continue;
                                }
                                if (floorHere > standing && floorHere - standing > EB::CREATURE_STEP_UP) { return false; }
                                if (ceilingHere < standing + halfHeight) { return false; }
                                highest = std::max(highest, floorHere);
                                lowest = std::min(lowest, floorHere);

                                // (The map's own +0x0c bytes are editor leftovers
                                // and the loader clears them; the marks tested
                                // above are the runtime ones - objects, the
                                // player and creatures.)
                            }
                            // Nor may the three probes disagree by more than 0x100
                            // - the leading edge must be level enough to stand on.
                            if (highest - lowest > (hanging ? EB::CEILING_STEP : EB::CREATURE_STEP_UP)) { return false; }
                            return true;
                        };


                        // Creatures run on the original's alternate pass; with
                        // Faster Xenomorphs the aliens also take some of the
                        // others (FASTER_XENO_OFF_PASS_EVERY).
                        // What the renderer would have drawn from here: the
                        // zones creatures test the player against.
                        // The walk takes the original's camera yaw (DAT_000b0ba2),
                        // whose 0 looks toward +z (the ring table's angle 0); the
                        // port's yaw 0 looks toward -z, so it is turned half round.
                        // Passing the port's own yaw walked the cells BEHIND the
                        // player, and creatures tested the player against those.
                        const uint16_t drawnZones = ALTEngine::Game::CreatureSight::DrawnZoneMask(level,
                            ToGridSpaceX(camera.x, originX) >> 9, ToGridSpaceZ(camera.z, originZ) >> 9,
                            (playerCam.viewYaw + 0x800) & PC::ANGLE_MASK);
                        if (creaturePass)
                        for (auto& enemy : enemies)
                        {
                            if (!halfPass && !ALTEngine::Screens::IsXenomorph(enemy.type)) { continue; }
                            // The player's FEET, not the eye: FUN_00032154 measures
                            // the vertical separation from DAT_000b0a4a.
                            // What the creature can see of the player, for its attack
                            // gates (FUN_0003231c, FUN_00033a1c). Only worth working
                            // out inside the line-of-fire range.
                            // Lift this creature's own mark before it looks or moves.
                            const bool marks = enemy.active && enemy.alive && !ALTEngine::Screens::IsVent(enemy.type);
                            if (marks) { markCreature(occupancyCell(enemy.x, enemy.z), nullptr); }

                            if (enemy.active && enemy.alive)
                            {
                                namespace CS = ALTEngine::Game::CreatureSight;
                                const int egx = ToGridSpaceX(enemy.x, originX);
                                const int egz = ToGridSpaceZ(enemy.z, originZ);
                                const int pgx = ToGridSpaceX(camera.x, originX);
                                const int pgz = ToGridSpaceZ(camera.z, originZ);
                                enemy.zoneVisible = CS::ZoneVisible(level,
                                    ALTEngine::Formats::CellIndex(level, egx >> 9, egz >> 9), drawnZones);
                                // The gunners and the egg look along a ray
                                // (FUN_00032aec, to 0x1555); everything else
                                // along the ground (FUN_0003231c, to 0x2000).
                                // The ray's else used to sit after the ground
                                // test and clear it for every other creature,
                                // so no hugger or other close attacker ever
                                // had a line to the player and none attacked.
                                const bool rayLooker = enemy.type == 1 || (enemy.type >= 0x0b && enemy.type <= 0x0e);
                                enemy.lineOfFire = false;
                                if (!rayLooker && enemy.distance <= CS::MAX_DISTANCE)
                                {
                                    CS::Query q;
                                    q.x = static_cast<float>(egx);
                                    q.y = enemy.y;
                                    q.z = static_cast<float>(egz);
                                    q.targetX = static_cast<float>(pgx);
                                    q.targetZ = static_cast<float>(pgz);
                                    q.distance = enemy.distance;
                                    q.halfHeight = ALTEngine::Game::ProjectileCollision::CreatureExtents(enemy.type).y;
                                    q.ceilingCrawler = enemy.onCeiling;
                                    q.tallType = (enemy.type == 7);
                                    q.occupancy = &occupancy;
                                    enemy.lineOfFire = CS::LineOfFire(level, q);
                                }
                                if (rayLooker)
                                {
                                    CS::Query3D q;
                                    q.x = static_cast<float>(egx);
                                    q.y = enemy.y;
                                    q.z = static_cast<float>(egz);
                                    // +0x54, the bearing to the player (0x357a6, 0x34c7d).
                                    q.heading = ALTEngine::Game::OriginalMath::Bearing(egx - pgx, egz - pgz);
                                    q.targetX = static_cast<float>(pgx);
                                    q.targetZ = static_cast<float>(pgz);
                                    q.targetFloorY = playerFeetY;
                                    q.distance = enemy.distance;
                                    q.occupancy = &occupancy;
                                    enemy.lineOfFire = CS::LineOfFire3D(level, q);
                                }
                            }
                            aiCreature = &enemy;
                            {
                                const auto* standing = ALTEngine::Formats::CellAt(level,
                                    ToGridSpaceX(enemy.x, originX) >> 9, ToGridSpaceZ(enemy.z, originZ) >> 9);
                                enemy.standingAttribute = standing ? standing->attribute : 0;
                            }
                            enemy.animationFrames = enemySprites.FrameCount(enemy.type, enemy.animation);
                            const bool wasAlive = enemy.alive;
                            ALTEngine::Screens::Enemies::Tick(enemy, camera.x, playerFeetY, camera.z,
                                                              aiWorld, weaponRuntime.noise > 0);
                            // A creature's death counts toward the assessment
                            // (FUN_00030b04), whatever killed it.
                            if (wasAlive && !enemy.alive
                                && enemy.type >= 0 && enemy.type < ALTEngine::Game::Progression::TALLY_SIZE)
                            {
                                tally.killed[enemy.type]++;
                            }
                            // An egg letting its hugger out (FUN_00035530), or a
                            // colonist its burster (FUN_00035624).
                            if (enemy.releaseMonster >= 0)
                            {
                                namespace CT = ALTEngine::Screens::CreatureTasks;
                                const bool colonist = enemy.type == 10;
                                auto* released = ALTEngine::Screens::Enemies::ReleaseFromEgg(
                                    enemies, enemy.releaseMonster, enemy.x, enemy.y, enemy.z,
                                    ALTEngine::Screens::Enemies::BearingTo(enemy, camera.x, camera.z),
                                    colonist ? CT::Colonist::BURSTER_SPEED : CT::EggRelease::SPEED,
                                    colonist ? CT::Colonist::BURSTER_LIFT : CT::EggRelease::LIFT);
                                if (released && colonist)
                                {
                                    // Its leap plays this to its end (0x35643).
                                    released->actAnimation = CT::Colonist::BURSTER_ANIMATION;
                                    ALTEngine::Screens::Enemies::RestartAnimation(
                                        *released, CT::Colonist::BURSTER_ANIMATION);
                                }
                                else if (released)
                                {
                                    ALTEngine::Screens::Enemies::RestartAnimation(enemy, CT::Egg::ANIM_RELEASE);
                                }
                                SDL_Log("GameplayScreen: %s %d %s monster %d", colonist ? "colonist" : "egg",
                                        enemy.monsterIndex, released ? "released" : "could not release",
                                        enemy.releaseMonster);
                                enemy.releaseMonster = -1;
                            }
                            // The queen's death runs the command chain her record
                            // names (FUN_00040e2c).
                            if (enemy.deathChain >= 0)
                            {
                                SDL_Log("GameplayScreen: creature %d's death runs command %d",
                                        enemy.monsterIndex, enemy.deathChain);
                                triggers.RunCommands(enemy.deathChain, triggerHandlers);
                                enemy.deathChain = -1;
                            }
                            if (enemy.cried)
                            {
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Screens::CreatureTasks::Colonist::SOUND_CRY, digits.c_str(), cdDirectory);
                            }
                            // FUN_0003bc9c: a creature's pickup counts toward its
                            // trigger and, once due, is thrown out where it stands.
                            if (enemy.dropPickup >= 0)
                            {
                                for (auto& live : livePickups)
                                {
                                    if (live.pickupIndex != enemy.dropPickup || !live.waitingOnTrigger) { continue; }
                                    if (live.triggerCount < live.triggerThreshold) { live.triggerCount++; }
                                    if (live.triggerCount != live.triggerThreshold) { break; }
                                    live.waitingOnTrigger = false;
                                    live.contained = false;
                                    const int gx = ToGridSpaceX(enemy.x, originX) >> 9;
                                    const int gz = ToGridSpaceZ(enemy.z, originZ) >> 9;
                                    const int cell = ALTEngine::Formats::CellIndex(level, gx, gz);
                                    if (cell >= 0) { live.cellIndex = cell; }
                                    const auto flight = ALTEngine::Screens::DroppedPickups::Launch(
                                        static_cast<uint8_t>(enemy.facingOctant), dropRng);
                                    live.vx = flight.vx;
                                    live.vy = flight.vy;
                                    live.vz = flight.vz;
                                    live.falling = true;
                                    if (live.placedIndex < placedObjects.size())
                                    {
                                        auto& placed = placedObjects[live.placedIndex];
                                        // 0x200 above the floor where the creature
                                        // stands (FUN_0003bc9c: FUN_00027e28 + 0x200),
                                        // not the crate drop's per-type offset.
                                        placed.x = enemy.x;
                                        placed.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                                                       level, ToGridSpaceX(enemy.x, originX), ToGridSpaceZ(enemy.z, originZ))
                                                 + 0x200;
                                        placed.z = enemy.z;
                                        placed.visible = true;
                                    }
                                    break;
                                }
                                enemy.dropPickup = -1;
                            }

                            // A gunner's round (CreatureTasks::Gunner), aimed at
                            // the player's chest along the creature's heading.
                            if (enemy.fireShot)
                            {
                                namespace GN = ALTEngine::Screens::CreatureTasks::Gunner;
                                namespace PCm = ALTEngine::Screens::PlayerCamera;
                                enemy.fireShot = false;
                                const float sy = enemy.y + static_cast<float>(GN::LaunchHeight(enemy.type));
                                const float flat = std::max(1.0f, std::hypot(camera.x - enemy.x, camera.z - enemy.z));
                                const float pitch = std::atan2(playerFeetY + GN::AIM_HEIGHT - sy, flat);
                                const float along = std::cos(pitch) * GN::SPEED;
                                particles.SpawnEnemyRound(enemy.x, sy, enemy.z,
                                                          PCm::Sin(enemy.facing) / 4096.0f * along,
                                                          std::sin(pitch) * GN::SPEED,
                                                          -PCm::Cos(enemy.facing) / 4096.0f * along);
                            }

                            // A handler's grenade (FUN_0002b240): thrown from 0x180
                            // up at 0x80 along its heading - pitched 45 degrees,
                            // so 0x5a flat, when the player stands higher or
                            // lower - and lofted at 0x20.
                            if (enemy.fireGrenade)
                            {
                                namespace PCm = ALTEngine::Screens::PlayerCamera;
                                enemy.fireGrenade = false;
                                EnemyGrenade g;
                                g.x = enemy.x;
                                g.y = enemy.y + 0x180;
                                g.z = enemy.z;
                                const float flat = std::fabs(playerFeetY - (enemy.y - ALTEngine::Screens::EnemyBehaviour::STAND_CLEARANCE)) > 1.0f
                                    ? 0x80 * 0.70710678f : 0x80;
                                g.vx = PCm::Sin(enemy.facing) / 4096.0f * flat;
                                g.vz = -PCm::Cos(enemy.facing) / 4096.0f * flat;
                                g.vy = 0x20;
                                enemyGrenades.push_back(g);
                            }

                            // The animation script's sound cue (FUN_00040d44).
                            // OURS: only from a creature in a zone the player
                            // can see, or within 0x1400 (where the original's
                            // volume bottoms out). The original attenuates but
                            // never cuts, and every creature's walk starts, and
                            // cues, at the level's start (Edward, 2026: "I can
                            // hear facehuggers all over the place from the
                            // moment I spawn").
                            if (enemy.animSound >= 0
                                && (enemy.zoneVisible || enemy.distance < 0x1400)
                                && enemy.ticks > 1)
                            {
                                ALTEngine::Audio::SfxPlayer::PlaySlotAtVolume(
                                    enemy.animSound,
                                    ALTEngine::Audio::SfxPlayer::CreatureVolume(enemy.x - camera.x, enemy.z - camera.z),
                                    digits.c_str(), cdDirectory);
                                enemy.animSound = -1;
                            }

                            // A vent's jet, sound, burst and switch-off
                            // (CreatureTasks::Vent). The sound plays once as the
                            // jet starts: the port's mixer has no loops to hold.
                            if (ALTEngine::Screens::IsVent(enemy.type))
                            {
                                namespace VT = ALTEngine::Screens::CreatureTasks::Vent;
                                const VT::Params vp = VT::For(enemy.type);
                                if (enemy.ventEmit)
                                {
                                    // The vent's heading is its record's turned half
                                    // round (FUN_0002e638), but FUN_0002b0e4 rotates
                                    // a velocity along +z, the reverse of the port's
                                    // (sin, -cos) heading - so in the port's terms the
                                    // jet runs along the record's own heading. Turned
                                    // round, 45 of the 59 horizontal vents in the
                                    // game jetted straight into the wall block behind
                                    // them (L112's pipes: "inside the pipe", seen only
                                    // up close - Edward, 2026); the record's way, 1.
                                    const int jetYaw = (enemy.facing + 0x800) & ALTEngine::Screens::PlayerCamera::ANGLE_MASK;
                                    particles.SpawnVent(vp.particleType, enemy.x, enemy.y, enemy.z,
                                                        vp.speed, jetYaw, enemy.ventDown);
                                }
                                if (enemy.ventSound)
                                {
                                    ALTEngine::Audio::SfxPlayer::PlaySlot(
                                        vp.steam ? VT::SOUND_STEAM : VT::SOUND_FLAME, digits.c_str(), cdDirectory);
                                }
                                if (enemy.ventBurst)
                                {
                                    // FUN_000358a4 state 7: FUN_0002aaf0 at the
                                    // vent - the general blast and sound 0x29,
                                    // and no blast damage (that function does
                                    // none).
                                    explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_CRATE,
                                                     enemy.x, enemy.y, enemy.z, false);
                                    ALTEngine::Audio::SfxPlayer::PlaySlot(
                                        ALTEngine::Formats::SoundIds::CRATE_BREAK, digits.c_str(), cdDirectory);
                                }
                                if (enemy.ventSwitchedOff)
                                {
                                    // A vent shut off counts as dealt with (FUN_000358a4).
                                    if (enemy.type < ALTEngine::Game::Progression::TALLY_SIZE) { tally.killed[enemy.type]++; }
                                    if (vp.steam) { hudMessages.ShowSteamValveClosed(language); }
                                    else { hudMessages.ShowFlameJetShutDown(language); }
                                }
                                continue;   // it doesn't move or stand on the floor
                            }

                            // Each axis resolved separately, as the original's two
                            // step helpers do - so a creature blocked on one axis
                            // still slides along the other.
                            const float stepHalfHeight = static_cast<float>(
                                ALTEngine::Game::ProjectileCollision::CreatureExtents(enemy.type).y);
                            // A refused step slides up to what refused it, and
                            // one off a ledge hops down (FUN_000315f0's ends),
                            // both measured against the floor the move began on.
                            {
                                namespace EB = ALTEngine::Screens::EnemyBehaviour;
                                const float startFloor = ALTEngine::Formats::FindFloorHeightGridSpace(
                                    level, ToGridSpaceX(enemy.x, originX), ToGridSpaceZ(enemy.z, originZ));
                                const bool crawler = enemy.ai.ceilingCrawler;
                                if (enemy.stepX != 0.0f)
                                {
                                    const int gx = ToGridSpaceX(enemy.x, originX);
                                    const int step = static_cast<int>(std::lround(enemy.stepX));
                                    int to = 0;
                                    if (enemyAxisClear(enemy.x, enemy.z, true, enemy.stepX, stepHalfHeight, enemy.onCeiling))
                                    {
                                        to = EB::LedgeStep(gx + step, step, startFloor,
                                            ALTEngine::Formats::FindFloorHeightGridSpace(level, gx + step, ToGridSpaceZ(enemy.z, originZ)),
                                            crawler, enemy.vy);
                                    }
                                    else { to = EB::BlockedStep(gx, step); }
                                    enemy.x += static_cast<float>(to - gx);
                                }
                                if (enemy.stepZ != 0.0f)
                                {
                                    const int gz = ToGridSpaceZ(enemy.z, originZ);
                                    const int step = static_cast<int>(std::lround(enemy.stepZ));
                                    int to = 0;
                                    if (enemyAxisClear(enemy.x, enemy.z, false, enemy.stepZ, stepHalfHeight, enemy.onCeiling))
                                    {
                                        to = EB::LedgeStep(gz + step, step, startFloor,
                                            ALTEngine::Formats::FindFloorHeightGridSpace(level, ToGridSpaceX(enemy.x, originX), gz + step),
                                            crawler, enemy.vy);
                                    }
                                    else { to = EB::BlockedStep(gz, step); }
                                    enemy.z += static_cast<float>(to - gz);
                                }
                            }

                            // And mark wherever it ended up.
                            if (marks && enemy.alive && !enemy.pouncing) { markCreature(occupancyCell(enemy.x, enemy.z), &enemy); }

                            // Moving sets the velocity flag; attacking and dying
                            // clear it. Nothing else touches it, so a creature's
                            // blip follows its own move/attack rhythm.
                            if (enemy.x != enemy.lastX || enemy.z != enemy.lastZ)
                            {
                                enemy.velocityActive = true;
                            }
                            enemy.lastX = enemy.x;
                            enemy.lastZ = enemy.z;

                            // Stand on the floor - or, mid-leap, rise and fall
                            // under gravity (FUN_00031f3c's vertical half). A
                            // colonist's task never calls the mover: it stays
                            // where its spawn put it, 0x40 up its wall.
                            if (enemy.type != 10)
                            {
                                const int gx = ToGridSpaceX(enemy.x, originX);
                                const int gz = ToGridSpaceZ(enemy.z, originZ);
                                const float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                                const float ceilingY = cell
                                    ? static_cast<float>(cell->ceilingHeight) * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT
                                    : floorY + 0x10000;
                                const float halfHeight = static_cast<float>(
                                    ALTEngine::Game::ProjectileCollision::CreatureExtents(enemy.type).y);
                                ALTEngine::Screens::Enemies::SettleVertical(enemy, floorY, ceilingY, halfHeight);
                            }
                            if (enemy.pendingDamage > 0)
                            {
                                // Through the one damage routine, so the cooldown,
                                // the terrain shield and armour all apply - this
                                // used to subtract health directly.
                                const int healthBefore = hudState.health;
                                hudState.ApplyDamage(enemy.pendingDamage,
                                                     ALTEngine::Screens::EnemyBehaviour::AttackForSubtype(enemy.type).cause);
                                // A directed hit (FUN_0003e5d4 - all but the
                                // hugger's and burster's) that takes health
                                // knocks the player along the attacker's facing.
                                if (hudState.health != healthBefore
                                    && !ALTEngine::Screens::EnemyBehaviour::AttackForSubtype(enemy.type).melee)
                                {
                                    knockbackStrength = ALTEngine::Game::PlayerMover::KNOCKBACK_HIT;
                                    knockbackYaw = enemy.facing;
                                }
                                checkPlayerDeath();
                            }
                            if (enemy.spawnDeathEffect)
                            {
                                enemy.spawnDeathEffect = false;

                                // NO EXPLOSION. A creature dying was spawning the
                                // crate-shatter effect, which is plainly wrong -
                                // they have their own death frames and fall to the
                                // floor (Edward, 2026). FUN_00030b04 starts the
                                // death animation (EnemyBehaviour::DeathAnimation)
                                // and the body stays.
                            }

                            // The body, once its death animation has played
                            // (the common task's state 8, 0x34eb3): in a low
                            // cell of attribute 6 it is taken away; on bare
                            // floor above Easy an alien's leaves acid there
                            // (attribute 0x0c, which burns the player).
                            if ((ALTEngine::Screens::CreatureAI::UsesCommonTask(enemy.type) || enemy.type == 7)
                                && enemy.state == ALTEngine::Screens::EnemyBehaviour::STATE_DEAD
                                && !enemy.bodyRemoved && enemy.animationEnded)
                            {
                                auto* cell = ALTEngine::Formats::CellAt(level, ToGridSpaceX(enemy.x, originX) >> 9,
                                                                        ToGridSpaceZ(enemy.z, originZ) >> 9);
                                if (cell && cell->attribute == 6 && cell->ceilingHeight - cell->floorHeight < 4)
                                {
                                    enemy.bodyRemoved = true;
                                }
                                else if (cell && cell->attribute == 0
                                         && static_cast<int>(difficultySettings.Get()) != 0
                                         && enemy.type >= 2 && enemy.type <= 9)
                                {
                                    // Types 2-6, the ceiling ones (8, 9), which
                                    // become 6 and 5 as they die, and the queen,
                                    // whose own task leaves it too (0x36455).
                                    cell->attribute = 0x0c;
                                }
                            }
                        }

                        // Items still in the air after a crate threw them, at
                        // the rate they were tuned at (every other tick).
                        for (auto& live : livePickups)
                        {
                            if (!halfPass) { break; }
                            if (!live.falling || live.placedIndex >= placedObjects.size()) { continue; }
                            auto& placed = placedObjects[live.placedIndex];

                            live.vy -= ALTEngine::Screens::DroppedPickups::GRAVITY;
                            placed.x += live.vx;
                            placed.y += live.vy;
                            placed.z += live.vz;

                            const float floor = ALTEngine::Formats::FindFloorHeightGridSpace(
                                level, ToGridSpaceX(placed.x, originX), ToGridSpaceZ(placed.z, originZ));
                            const float rest = floor + static_cast<float>(
                                ALTEngine::Screens::DroppedPickups::DropHeight(live.type));
                            if (placed.y <= rest)
                            {
                                // Landed, at its type's height above the floor
                                // like any other item (FUN_0003c258). Its cell is
                                // wherever it came to rest, not where the crate was.
                                placed.y = rest;
                                live.falling = false;
                                const int gx = ToGridSpaceX(placed.x, originX) >> 9;
                                const int gz = ToGridSpaceZ(placed.z, originZ) >> 9;
                                const int landed = ALTEngine::Formats::CellIndex(level, gx, gz);
                                if (landed >= 0) { live.cellIndex = landed; }
                            }
                        }
                        if (halfPass) { explosions.Tick(); }
                        if (halfPass) shatter.Tick([&](float sx, float sz) {
                            return ALTEngine::Formats::FindFloorHeightGridSpace(
                                level, ToGridSpaceX(sx, originX), ToGridSpaceZ(sz, originZ));
                        });
                        // At the rate they were tuned at, and in step with the
                        // pouncing hugger's own clock.
                        if (halfPass)
                        {
                            hudState.TickCarriedItems();
                            // The lamp's or goggles' time (FUN_0003e698): when it
                            // runs out the light fades and sound 0x19 plays.
                            if (inventory.TickLight())
                            {
                                ModelRenderer::SetLevelLightEquipment(cacheKey, 0);
                                ALTEngine::Audio::SfxPlayer::PlaySlot(LIGHT_OFF_SOUND, digits.c_str(), cdDirectory);
                            }
                            hudMessages.Tick();
                            faceHug.Tick();
                        }

                        // The crawl runs while a hugger is pouncing.
                        {
                            // ONE CLOCK. The overlay starts when a pounce starts and
                            // then runs on its own - it must NOT be ended when the
                            // creature finishes, because the crawl outlasts it. That
                            // is what made the two drift out of time.
                            bool anyPouncing = false;
                            for (const auto& enemy : enemies)
                            {
                                if (enemy.pouncing) { anyPouncing = true; break; }
                            }
                            if (anyPouncing && !faceHug.Active()) { faceHug.Begin(); }

                            // The creature dies when the crawl has slid back off,
                            // which is what LAB_00030918 waits for.
                            if (faceHug.Finished())
                            {
                                for (auto& enemy : enemies)
                                {
                                    if (enemy.pouncing) { enemy.overlayFinished = true; }
                                }
                            }
                        }

                        // Destroying something: clear its occupancy so the
                        // square opens up, spill what it held, and - if it was a
                        // barrel - set off the blast, which can take out its
                        // neighbours and start a chain.
                        auto ReleaseContents = [&](int crateIndex, float dropX, float dropY, float dropZ,
                                                   int dropCell) {
                            if (crateIndex < 0 || crateIndex >= static_cast<int>(level.crates.size())) { return; }
                            const auto& crate = level.crates[static_cast<size_t>(crateIndex)];
                            if (crate.flags == 2)
                            {
                                // Holds an enemy - drop1 indexes the MONSTER
                                // array, and that monster is parked off the edge
                                // of the map until released.
                                // Let it out AT THE CRATE. Like the held
                                // pickups, a crate's monster is parked off the
                                // playable map until something opens the box.
                                if (!ALTEngine::Screens::Enemies::Release(enemies, crate.drop1,
                                                                          dropX, dropY, dropZ,
                                                                          crate.rotation))
                                {
                                    SDL_Log("crate %d: monster %u not in the roster at this difficulty",
                                            crateIndex, crate.drop1);
                                }
                                return;
                            }

                            for (uint8_t index : { crate.drop1, crate.drop2 })
                            {
                                if (index == 0xFF) { continue; }
                                for (auto& live : livePickups)
                                {
                                    if (live.pickupIndex != index || !live.contained) { continue; }

                                    // MOVE IT TO THE CRATE. A held pickup is
                                    // parked somewhere else entirely - crate 1
                                    // sits at (31,67) while the thing it holds
                                    // is recorded at (23,97), right across the
                                    // map. The same trick the crate monsters use,
                                    // parked at x=9.
                                    //
                                    // Revealing it where it was recorded put the
                                    // item in a corner of the level instead of
                                    // in front of the player (Edward, 2026), so
                                    // release has to relocate it as well as
                                    // unhide it.
                                    live.contained = false;
                                    live.cellIndex = dropCell;

                                    // Thrown out in the direction the crate
                                    // faces - see DroppedPickups.
                                    const auto flight = ALTEngine::Screens::DroppedPickups::Launch(
                                        crate.rotation, dropRng);
                                    live.vx = flight.vx;
                                    live.vy = flight.vy;
                                    live.vz = flight.vz;
                                    live.falling = true;

                                    if (live.placedIndex < placedObjects.size())
                                    {
                                        auto& placed = placedObjects[live.placedIndex];
                                        placed.x = dropX;
                                        placed.y = dropY + ALTEngine::Screens::DroppedPickups::DropHeight(live.type);
                                        placed.z = dropZ;
                                        placed.visible = true;
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
                        // FUN_00045fe4: a broken window's glass. A window facing
                        // z (rotation 0 or 4) throws a column of panes from each
                        // cell along x, starting at the object and 0x200 apart,
                        // at the middle of its depth; one facing x (2 or 6) the
                        // same along z, at the middle of its width. Each column
                        // is two panes, 0x100 and 0x300 up (three on campaign
                        // level 0xd, L212, the third 0x500 up), flying away from
                        // the side the player is on at (random & 0xf) + 0x10,
                        // and up at the same. The original's third pane on a
                        // window facing x uses the z-facing grid - kept.
                        auto BreakWindow = [&](const D::Target& target) {
                            if (!glassSheetReady) { return; }
                            const bool facesZ = (target.facing & 2) == 0;
                            const int columns = facesZ ? target.cellsWide : target.cellsDeep;
                            float x = target.x, z = target.z;
                            if (facesZ) { z += static_cast<float>((target.cellsDeep - 1) * 0x100); }
                            else { x += static_cast<float>((target.cellsWide - 1) * 0x100); }
                            const bool playerBeyond = facesZ ? (camera.z > z) : (camera.x > x);
                            const int rows = (digits == "212") ? 3 : 2;
                            for (int col = 0; col < columns; ++col)
                            {
                                for (int row = 0; row < rows; ++row)
                                {
                                    const float up = static_cast<float>(glassRng.Bits(0x0f) + 0x10);
                                    float away = static_cast<float>(glassRng.Bits(0x0f) + 0x10);
                                    if (playerBeyond) { away = -away; }
                                    shatter.Glass("glass", x, target.y + static_cast<float>(0x100 + row * 0x200), z,
                                                  facesZ ? 0.0f : away, up, facesZ ? away : 0.0f,
                                                  facesZ || row == 2);
                                }
                                if (facesZ) { x += 0x200; } else { z += 0x200; }
                            }
                        };

                        auto DestroyTarget = [&](D::Target& target) {
                            // A window (object type 0x1c) has its own arm in the
                            // modelless objects' handler (0x38097): its glass
                            // flies out (FUN_00045fe4), sound 0x6c, its cells
                            // reopen and its texture animators step on - and
                            // that is all. No tally, no contents, no scatter.
                            const bool isWindow = target.objectType == WINDOW_OBJECT_TYPE;
                            // Destroying an object counts toward the mission (FUN_00037dd0).
                            if (!isWindow && target.objectType >= 0 && target.objectType < ALTEngine::Game::Progression::TALLY_SIZE)
                            {
                                tally.killed[target.objectType]++;
                            }
                            if (target.placedObjectIndex >= 0
                                && target.placedObjectIndex < static_cast<int>(placedObjects.size()))
                            {
                                placedObjects[static_cast<size_t>(target.placedObjectIndex)].visible = false;
                            }
                            ClearOccupancy(target.cellIndex);
                            // And the rest of a wall's block (FUN_000370d4).
                            if (target.cellsWide > 1 || target.cellsDeep > 1)
                            {
                                const int length = level.header.mapLength;
                                for (int dz = 0; dz < target.cellsDeep; ++dz)
                                {
                                    for (int dx = 0; dx < target.cellsWide; ++dx)
                                    {
                                        ClearOccupancy(target.cellIndex + dz * length + dx);
                                    }
                                }
                            }
                            if (isWindow)
                            {
                                BreakWindow(target);
                                ALTEngine::Audio::SfxPlayer::PlaySlot(WINDOW_BREAK_SOUND, digits.c_str(), cdDirectory);
                            }
                            else
                            {
                                ReleaseContents(target.crateIndex, target.x, target.y, target.z, target.cellIndex);
                            }

                            // The faces that show it broken: each of its texture
                            // animators steps on by one - a window's glass, a
                            // curtain torn down. Only the modelless objects'
                            // handler does this (FUN_00037ff8).
                            if (target.crateIndex >= 0 && target.crateIndex < static_cast<int>(level.crates.size())
                                && IsInvisibleObjectType(level.crates[static_cast<size_t>(target.crateIndex)].type))
                            {
                                const auto& crate = level.crates[static_cast<size_t>(target.crateIndex)];
                                const int first = ALTEngine::Formats::TextureStepFirst(crate);
                                const int count = ALTEngine::Formats::TextureStepCount(crate);
                                for (int i = 0; i < count; ++i)
                                {
                                    ModelRenderer::ChangeLevelTexture(cacheKey, first + i, 1);
                                    surfaceChanges.push_back({ 0, first + i, 1 });
                                }
                            }

                            // Tear the object into pieces of its own mesh. This
                            // is the real break-up; the effect sprites below are
                            // a separate system that plays alongside it.
                            const bool hasModel = target.crateIndex < 0
                                || target.crateIndex >= static_cast<int>(level.crates.size())
                                || !IsInvisibleObjectType(level.crates[static_cast<size_t>(target.crateIndex)].type);
                            if (const auto* faces = hasModel ? ALTEngine::Renderer::ModelRenderer::ModelFaces(target.modelKey) : nullptr)
                            {
                                // Turned as the object was drawn: its rotation
                                // (from the record's compass) and the model's own.
                                const float yaw = 3.14159265f - static_cast<float>(target.facing) * (3.14159265f / 4.0f)
                                    + ALTEngine::Renderer::ModelRenderer::ModelBaseRotation(target.modelKey);
                                shatter.Shatter(*faces,
                                                ALTEngine::Renderer::ModelRenderer::ModelSheetKey(target.modelKey),
                                                target.x, target.y, target.z,
                                                target.scaleX, target.scaleY, target.scaleZ,
                                                target.objectType, yaw);
                            }
                            if (target.kind == D::TARGET_BARREL)
                            {
                                blastQueue.push_back(target.cellIndex);
                                // FUN_0002ab54 from 0x37d74: the barrel's own
                                // fireball and sound 0x21, 0x30 under the floor
                                // it stood on; the frames' anchors stand the
                                // fire up from there.
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_BARREL,
                                                 target.x,
                                                 target.y - ALTEngine::Formats::ExplosionGraphics::BARREL_EFFECT_DROP,
                                                 target.z);
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Formats::SoundIds::BARREL_EXPLODE, digits.c_str(), cdDirectory);
                            }
                            else if (!isWindow)
                            {
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Formats::SoundIds::CRATE_BREAK, digits.c_str(), cdDirectory);
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
                            namespace Blast = ALTEngine::Game::Blast;
                            const Blast::Rings rings = Blast::ReachedRings(level, originCell);

                            // Each reached cell's occupant, by its mark
                            // (FUN_000368c8): the player takes 0x32 in the
                            // inner ring and 0x19 in the outer; the creature
                            // the mark names takes its whole health, unless it
                            // is the queen, and goes up in a crate's burst
                            // (FUN_0002aaf0, 0x100 up) whether or not it dies.
                            // One occupant a cell - a second creature sharing
                            // it is missed, as in the original.
                            auto occupant = [&](int cell, bool inner) {
                                if (cell < 0 || static_cast<size_t>(cell) >= occupancy.size()) { return; }
                                const uint8_t m = occupancy[static_cast<size_t>(cell)];
                                if (m == 0xff)
                                {
                                    hudState.ApplyDamage(inner ? Blast::PLAYER_DAMAGE_INNER : Blast::PLAYER_DAMAGE_OUTER,
                                                         Blast::DAMAGE_CAUSE);
                                    return;
                                }
                                const int index = markedCreature[static_cast<size_t>(cell)];
                                if (m == 0 || index < 0 || static_cast<size_t>(index) >= enemies.size()) { return; }
                                auto& creature = enemies[static_cast<size_t>(index)];
                                if (!creature.active) { return; }
                                if (creature.type != Blast::IMMUNE_CREATURE_TYPE && creature.alive)
                                {
                                    ALTEngine::Screens::Enemies::ApplyHit(creature, creature.health, ignoreStun);
                                }
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_CRATE,
                                                 creature.x, creature.y + 0x100, creature.z);
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Formats::SoundIds::CRATE_BREAK, digits.c_str(), cdDirectory);
                            };
                            for (int cell : rings.inner) { occupant(cell, true); }
                            for (int cell : rings.outer) { occupant(cell, false); }
                            checkPlayerDeath();

                            auto catchObjects = [&](int cell, bool inner) {
                                for (D::Target& other : damageTargets)
                                {
                                    // Objects caught in a blast go - no health
                                    // check - unless they are flagged
                                    // indestructible, after their fuse.
                                    // Any cell of its block reaches it.
                                    const int length = level.header.mapLength;
                                    const int dx = (cell % length) - (other.cellIndex % length);
                                    const int dz = (cell / length) - (other.cellIndex / length);
                                    const bool covers = other.cellIndex >= 0 && dx >= 0 && dz >= 0
                                        && dx < other.cellsWide && dz < other.cellsDeep;
                                    if (!other.destroyed && !other.indestructible && covers)
                                    {
                                        other.destroyed = true;
                                        other.state = D::STATE_DYING;
                                        other.fuse = inner ? fuseRng.Bits(7) + 4 : fuseRng.Bits(0xf) + 8;
                                    }
                                }
                            };
                            for (int cell : rings.inner) { catchObjects(cell, true); }
                            for (int cell : rings.outer) { catchObjects(cell, false); }
                        };

                        // Objects counting down to breaking or exploding
                        // (Damage::Target::fuse), on the objects' pass. A
                        // barrel's blast can light more fuses.
                        if (halfPass)
                        {
                            for (D::Target& target : damageTargets)
                            {
                                if (target.fuse > 0 && --target.fuse == 0) { DestroyTarget(target); }
                            }
                            for (size_t q = 0; q < blastQueue.size(); ++q) { RunBlast(blastQueue[q]); }
                            blastQueue.clear();
                        }

                        // The vents' jets (ParticleSystem.h TickVents): they
                        // scald the player with cause 5, as FUN_0002a8f8 does.
                        // Particles too run on the alternate pass (0x2d929).
                        if (halfPass)
                        particles.TickVents(
                            [&](float px, float pz, float& floorY, float& ceilingY) {
                                const auto* cell = ALTEngine::Formats::CellAt(
                                    level, ToGridSpaceX(px, originX) >> 9, ToGridSpaceZ(pz, originZ) >> 9);
                                floorY = cell ? static_cast<float>(cell->floorHeight) * 32.0f : 0.0f;
                                ceilingY = cell ? static_cast<float>(cell->ceilingHeight) * 32.0f : 0.0f;
                            },
                            camera.x, playerFeetY, camera.z,
                            [&](int damage) {
                                hudState.ApplyDamage(damage, ALTEngine::Screens::GameOverSequence::CAUSE_EXPLOSION);
                                checkPlayerDeath();
                            });

                        // How a particle meets the world, in the original's own order
                        // - see Game/ProjectileCollision.h. Returns 0 to keep going,
                        // 1 to stop (a round shatters), 2 to stop without shattering.
                        if (halfPass)
                        particles.Tick([&](float px, float py, float pz, int hitType, int hitLife,
                                           float clearX, float clearY, float clearZ, int& roundLatch) -> int {
                            namespace PCol = ALTEngine::Game::ProjectileCollision;
                            namespace P = ALTEngine::Screens::Particles;
                            const int gx = ToGridSpaceX(px, originX);
                            const int gz = ToGridSpaceZ(pz, originZ);

                            // ---- blood ----------------------------------------
                            // Acid burns the player it lands on; any drop that
                            // reaches the floor leaves a splat and is gone.
                            if (P::IsBlood(hitType))
                            {
                                if (hitType != P::TYPE_BLOOD_WHITE
                                    && PCol::AcidTouchesPlayer(px, py, pz, camera.x, playerFeetY, camera.z))
                                {
                                    hudState.ApplyDamage(1, PCol::ACID_DAMAGE_CAUSE);
                                    checkPlayerDeath();
                                    return 2;
                                }
                                const float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                if (py < floorY)
                                {
                                    explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_IMPACT,
                                                     clearX, floorY, clearZ);
                                    return 2;
                                }
                                return 0;
                            }

                            // ---- casings: come to rest on the floor -----------
                            if (hitType == P::TYPE_CASING_A || hitType == P::TYPE_CASING_B)
                            {
                                const bool wall = ALTEngine::Formats::IsCellBlocking(level, gx, gz);
                                return (wall || py < ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz)) ? 1 : 0;
                            }

                            // ---- a creature's round (the arm at 0x2de7e) -------
                            // The world first: a wall, floor or ceiling stops
                            // it with a puff. Then the player (FUN_0002a8f8):
                            // 0 to 0x400 above the feet and 0x100 across, 1
                            // damage, cause 3.
                            if (hitType == P::TYPE_ENEMY_ROUND)
                            {
                                const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                                const float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                const bool solid = !cell || ALTEngine::Formats::IsCellBlocking(level, gx, gz)
                                    || py < floorY
                                    || py > static_cast<float>(cell->ceilingHeight) * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
                                if (solid)
                                {
                                    explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_IMPACT, clearX, clearY, clearZ);
                                    return 2;
                                }
                                const float above = py - playerFeetY;
                                if (above >= 0.0f && above <= P::VENT_TOUCH_HEIGHT
                                    && std::fabs(px - camera.x) < P::VENT_TOUCH_HALF
                                    && std::fabs(pz - camera.z) < P::VENT_TOUCH_HALF)
                                {
                                    const int healthBefore = hudState.health;
                                    hudState.ApplyDamage(1, ALTEngine::Screens::GameOverSequence::CAUSE_ENEMY_PROJECTILE);
                                    if (hudState.health != healthBefore)
                                    {
                                        // Along the round's flight (its +0x12).
                                        knockbackStrength = ALTEngine::Game::PlayerMover::KNOCKBACK_HIT;
                                        knockbackYaw = static_cast<int>(std::lround(
                                            std::atan2(px - clearX, -(pz - clearZ)) * (4096.0 / 6.28318530718))) & 0xfff;
                                    }
                                    checkPlayerDeath();
                                    return 2;
                                }
                                return 0;
                            }

                            // ---- the player's rounds --------------------------
                            const auto def = P::DefFor(hitType);

                            // 1. The cell: walls, floor, ceiling, liquids, objects.
                            const PCol::CellResult cell = PCol::TestCell(level, gx, gz, py, hitType, def.speed);
                            if (cell.blocked)
                            {
                                if (cell.objectHit >= 0)
                                {
                                    for (D::Target& target : damageTargets)
                                    {
                                        if (target.crateIndex != cell.objectHit || target.destroyed) { continue; }
                                        if (target.indestructible) { break; }
                                        target.health -= cell.damage;
                                        if (target.health > 0) { break; }

                                        target.state = D::STATE_DYING;
                                        target.destroyed = true;
                                        // It goes a few ticks later (FUN_00036e9c).
                                        target.fuse = fuseRng.Bits(7) + 1;
                                        break;
                                    }
                                }

                                // Impact sound by where it landed - brick, metal or
                                // water - and the puff at the last clear position,
                                // flipped if it was the ceiling that stopped it.
                                const auto* hitCell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                                const bool hitCeiling = hitCell
                                    && py > static_cast<float>(hitCell->ceilingHeight) * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    P::ImpactSound(levelIdForSfx, hitCell ? hitCell->attribute : 0),
                                    digits.c_str(), cdDirectory);
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_IMPACT,
                                                 clearX, clearY, clearZ, hitCeiling);
                                return 1;
                            }

                            // 2. Creatures: a box per creature type, centred on it,
                            // and only if the two cells share a zone.
                            const int roundCell = ALTEngine::Formats::CellIndex(level, gx >> 9, gz >> 9);
                            bool hitAny = false;
                            for (auto& enemy : enemies)
                            {
                                // From the hit reaction on, rounds pass straight
                                // through - unless the No Hit Immunity option is on.
                                const bool targetable = PCol::CreatureTargetable(enemy.state)
                                    || (ignoreStun && enemy.state == D::STATE_HIT);
                                if (!enemy.active || !enemy.alive || !targetable) { continue; }
                                // A vent with no health is never marked in its
                                // cell, and rounds find creatures by the mark.
                                if (ALTEngine::Screens::IsVent(enemy.type) && enemy.health <= 0) { continue; }
                                if (!PCol::OverlapsCreature(px, py, pz, enemy.x, enemy.y, enemy.z, enemy.type, hitType)) { continue; }
                                const int enemyCell = ALTEngine::Formats::CellIndex(
                                    level, ToGridSpaceX(enemy.x, originX) >> 9, ToGridSpaceZ(enemy.z, originZ) >> 9);
                                if (!PCol::ShareZone(level, roundCell, enemyCell)) { continue; }

                                // Double damage while the round is fresh (FUN_0002a628).
                                // ApplyHit leaves a dying creature alive for one
                                // more tick, so its own arm runs the death.
                                ALTEngine::Screens::Enemies::ApplyHit(
                                    enemy, D::HitDamage(def.speed, hitLife, def.strength), ignoreStun);
                                const int blood = PCol::BloodTypeForCreature(enemy.type);
                                if (blood >= 0) { particles.SpawnBlood(blood, px, py, pz); }
                                hitAny = true;
                                if (PCol::StopsAtFirstCreature(hitType)) { return 2; }
                            }
                            if (hitAny) { return 2; }

                            // 3. Triggers a round can set off (flag 0x02).
                            triggers.OnShotEnterCell(roundLatch, roundCell, triggerHandlers);
                            return 0;
                        });

                        // A handler's grenades, on the particle pass (0x2dd0c).
                        // In the original's order: its time running out
                        // (0x2da19), the cell and object test (FUN_0002a2d8),
                        // the player (FUN_0002a8f8: 0x19, then the blast as
                        // well), then the floor.
                        // FUN_0002a2d8: an explosive against its cell, in the
                        // original's order. Returns 0 if it flies on, 1 if it
                        // goes off with the effect alone, 2 if the blast
                        // (FUN_000368c8) goes with it. `damage` is its +0x1a,
                        // taken off an object it strikes (FUN_00036e9c).
                        //   - attribute 0x14: the blast.
                        //   - an object of type 0x15, 0x1c or 0x1d (the
                        //     breakable walls and windows): the damage; the
                        //     blast for 0x15 only.
                        //   - any other object, when the explosive is between
                        //     the floor and 0x200 above it (0x400 for type
                        //     0x19): the damage and the blast. Higher, it
                        //     flies over.
                        //   - above the ceiling: brought 0x100 under it (not in
                        //     an open cell, attribute 6), the effect alone.
                        // Nothing here stops it at an ordinary wall cell, and
                        // nothing does in the original either.
                        auto explosiveCellRules = [&](float x, float& y, float z, int damage) -> int {
                            const int gx = ToGridSpaceX(x, originX);
                            const int gz = ToGridSpaceZ(z, originZ);
                            const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                            if (!cell) { return 0; }
                            auto damageObject = [&]() {
                                const int at = ALTEngine::Formats::CellIndex(level, gx >> 9, gz >> 9);
                                const int length = level.header.mapLength;
                                for (D::Target& other : damageTargets)
                                {
                                    const int dx = (at % length) - (other.cellIndex % length);
                                    const int dz = (at / length) - (other.cellIndex / length);
                                    const bool covers = other.cellIndex >= 0 && dx >= 0 && dz >= 0
                                        && dx < other.cellsWide && dz < other.cellsDeep;
                                    if (!covers || other.destroyed || other.indestructible) { continue; }
                                    other.health -= damage;
                                    if (other.health < 1)
                                    {
                                        other.destroyed = true;
                                        other.state = D::STATE_DYING;
                                        other.fuse = fuseRng.Bits(7) + 1;
                                    }
                                    break;
                                }
                            };
                            if (cell->attribute == 0x14) { return 2; }
                            const uint8_t object = cell->unknown13;
                            if (object == 0x15 || object == 0x1c || object == 0x1d)
                            {
                                damageObject();
                                return object == 0x15 ? 2 : 1;
                            }
                            if (object > 0x13 && object != 0xff)
                            {
                                const float floor = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                const float reach = (object == 0x19) ? 0x400 : 0x200;
                                if (y >= floor && y <= floor + reach) { damageObject(); return 2; }
                            }
                            const float ceiling = static_cast<float>(cell->ceilingHeight) * ALTEngine::Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
                            if (y > ceiling)
                            {
                                if (cell->attribute != 6) { y = ceiling - 0x100; }
                                return 1;
                            }
                            return 0;
                        };

                        if (halfPass && !enemyGrenades.empty())
                        {
                            // The same as the player's: FUN_0002aaf0's general
                            // blast at its own position, sound 0x29, then the
                            // blast (0x2da30 / 0x2dd2f).
                            auto detonate = [&](float x, float y, float z, bool effect, bool blast) {
                                const int cell = ALTEngine::Formats::CellIndex(
                                    level, ToGridSpaceX(x, originX) >> 9, ToGridSpaceZ(z, originZ) >> 9);
                                if (effect)
                                {
                                    explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_CRATE, x, y, z);
                                    ALTEngine::Audio::SfxPlayer::PlaySlot(
                                        ALTEngine::Formats::SoundIds::CRATE_BREAK, digits.c_str(), cdDirectory);
                                }
                                if (blast && cell >= 0) { blastQueue.push_back(cell); }
                                for (size_t q = 0; q < blastQueue.size(); ++q) { RunBlast(blastQueue[q]); }
                                blastQueue.clear();
                            };
                            for (size_t gi = 0; gi < enemyGrenades.size();)
                            {
                                EnemyGrenade& g = enemyGrenades[gi];
                                bool gone = false;
                                const int gx = ToGridSpaceX(g.x, originX);
                                const int gz = ToGridSpaceZ(g.z, originZ);
                                const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                                int ruled = 0;
                                if (!cell) { gone = true; }
                                else if (--g.life <= 0) { detonate(g.x, g.y, g.z, true, true); gone = true; }
                                // The cell and its object (0x2dd0e).
                                else if ((ruled = explosiveCellRules(g.x, g.y, g.z, 0x19)) != 0)
                                {
                                    detonate(g.x, g.y, g.z, true, ruled == 2); gone = true;
                                }
                                // The player (FUN_0002a8f8): 0x19 with cause 5,
                                // then the blast - and no effect (0x2dd5c).
                                else if (g.y - playerFeetY >= 0.0f && g.y - playerFeetY <= 0x400
                                         && std::fabs(g.x - camera.x) < 0x100 && std::fabs(g.z - camera.z) < 0x100)
                                {
                                    hudState.ApplyDamage(0x19, ALTEngine::Screens::GameOverSequence::CAUSE_EXPLOSION);
                                    checkPlayerDeath();
                                    detonate(g.x, g.y, g.z, false, true); gone = true;
                                }
                                else
                                {
                                    // Its cell's triggers, flag 8 (FUN_00041784).
                                    triggers.OnShotEnterCell(g.triggerLatch,
                                        ALTEngine::Formats::CellIndex(level, gx >> 9, gz >> 9), triggerHandlers,
                                        ALTEngine::Game::Triggers::Source::EnemyThrow);
                                    const float floor = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                    if (g.y < floor + 0x20)
                                    {
                                        // Into a hole: the blast, and the effect
                                        // 0x200 up (0x2ddc4).
                                        if (floor - g.y > 0x40) { detonate(g.x, g.y + 0x200, g.z, true, true); gone = true; }
                                        else
                                        {
                                            // A bounce, lower each time; then it
                                            // slides, halving its speed.
                                            g.y = floor + 0x20;
                                            g.vy = static_cast<float>(g.bounce);
                                            if (g.bounce > 4) { g.bounce -= 2; }
                                            else { g.vx = std::trunc(g.vx / 2.0f); g.vz = std::trunc(g.vz / 2.0f); }
                                        }
                                    }
                                    else
                                    {
                                        g.vy -= 2.0f;
                                    }
                                    if (!gone) { g.x += g.vx; g.y += g.vy; g.z += g.vz; }
                                }
                                if (gone) { enemyGrenades.erase(enemyGrenades.begin() + static_cast<std::ptrdiff_t>(gi)); }
                                else { ++gi; }
                            }
                        }

                        // The player's grenades and seismic charges, on the same
                        // pass (the arm at 0x2db80). In its order: time running
                        // out (0x2da19), the cell and its object (FUN_0002a2d8),
                        // a creature (FUN_0002a628), the cell's triggers
                        // (FUN_000414f0), then the floor.
                        if (halfPass && !playerExplosives.empty())
                        {
                            // FUN_0002aaf0 at the explosive's own position: the
                            // general blast (table 0xad008, centred on it) and
                            // sound 0x29; then, unless it only hit the ceiling,
                            // the blast (FUN_000368c8).
                            auto detonate = [&](float x, float y, float z, bool blast = true) {
                                const int cell = ALTEngine::Formats::CellIndex(
                                    level, ToGridSpaceX(x, originX) >> 9, ToGridSpaceZ(z, originZ) >> 9);
                                explosions.Spawn(ALTEngine::Formats::ExplosionGraphics::EFFECT_CRATE, x, y, z);
                                ALTEngine::Audio::SfxPlayer::PlaySlot(
                                    ALTEngine::Formats::SoundIds::CRATE_BREAK, digits.c_str(), cdDirectory);
                                if (blast && cell >= 0) { blastQueue.push_back(cell); }
                                for (size_t q = 0; q < blastQueue.size(); ++q) { RunBlast(blastQueue[q]); }
                                blastQueue.clear();
                            };
                            for (size_t ei = 0; ei < playerExplosives.size();)
                            {
                                PlayerExplosive& e = playerExplosives[ei];
                                bool gone = false;
                                const int gx = ToGridSpaceX(e.x, originX);
                                const int gz = ToGridSpaceZ(e.z, originZ);
                                const auto* cell = ALTEngine::Formats::CellAt(level, gx >> 9, gz >> 9);
                                // A creature it strikes (FUN_0002a628): inside the
                                // creature's box, and the creature can still be
                                // hit. Everything but the queen takes its whole
                                // health; the queen takes twice the explosive's
                                // 0x140. It bleeds 0x40 big drops from where the
                                // explosive is (FUN_0002af24, FUN_0002ada4).
                                auto strikeCreature = [&]() -> bool {
                                    namespace PCol = ALTEngine::Game::ProjectileCollision;
                                    for (auto& enemy : enemies)
                                    {
                                        if (!enemy.active || !enemy.alive || ALTEngine::Screens::IsVent(enemy.type)) { continue; }
                                        if (!PCol::CreatureTargetable(enemy.state)) { continue; }
                                        if (!PCol::OverlapsCreature(e.x, e.y, e.z, enemy.x, enemy.y, enemy.z,
                                                                    enemy.type, ALTEngine::Screens::Particles::TYPE_PULSE)) { continue; }
                                        const int damage = (enemy.type == ALTEngine::Game::Blast::IMMUNE_CREATURE_TYPE)
                                            ? EXPLOSIVE_HIT_DAMAGE * 2 : enemy.health;
                                        ALTEngine::Screens::Enemies::ApplyHit(enemy, damage, ignoreStun);
                                        const int blood = PCol::BloodTypeForCreature(enemy.type);
                                        if (blood >= 0) { particles.SpawnBlood(blood, e.x, e.y, e.z, true); }
                                        return true;
                                    }
                                    return false;
                                };
                                int ruled = 0;
                                if (!cell) { gone = true; }
                                else if (--e.life <= 0) { detonate(e.x, e.y, e.z); gone = true; }
                                else if ((ruled = explosiveCellRules(e.x, e.y, e.z, EXPLOSIVE_HIT_DAMAGE)) != 0)
                                {
                                    detonate(e.x, e.y, e.z, ruled == 2); gone = true;
                                }
                                else if (strikeCreature())
                                {
                                    detonate(e.x, e.y, e.z); gone = true;
                                }
                                else
                                {
                                    // Its cell's triggers, as a round's
                                    // (FUN_000414f0, 0x2dc17).
                                    triggers.OnShotEnterCell(e.triggerLatch,
                                        ALTEngine::Formats::CellIndex(level, gx >> 9, gz >> 9), triggerHandlers);
                                    const float floor = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                                    if (e.y < floor + 0x20)
                                    {
                                        // Dropped into a hole: it goes off 0x200 up.
                                        if (floor - e.y > 0x40) { detonate(e.x, e.y + 0x200, e.z); gone = true; }
                                        else
                                        {
                                            e.y = floor + 0x20;
                                            e.vy = static_cast<float>(e.bounce);
                                            if (e.bounce > 4) { e.bounce -= 2; }
                                            else { e.vx = std::trunc(e.vx / 2.0f); e.vz = std::trunc(e.vz / 2.0f); }
                                        }
                                    }
                                    else
                                    {
                                        e.vy -= 1.0f;   // 0x2dcf9: one a tick
                                    }
                                    if (!gone) { e.x += e.vx; e.y += e.vy; e.z += e.vz; }
                                    // FUN_0002cf14 turns a charge every tick it runs.
                                    if (!gone && e.charge) { e.spin = (e.spin + 0x20) & 0xfff; }
                                }
                                if (gone) { playerExplosives.erase(playerExplosives.begin() + static_cast<std::ptrdiff_t>(ei)); }
                                else { ++ei; }
                            }
                        }
                    }

                    // The animation's own sound cue. OP_EVENT's operand IS a
                    // slot id - FUN_0003e93c passes it straight through - so an
                    // animation carries its own audio and the weapon report
                    // lands on the frame the artist put it on.
                    // On the player's channel, each cue cutting off the last
                    // (FUN_0003e93c -> FUN_00040b2c with EBX -1 -> FUN_0005b21b).
                    // Only on the weapon's own pass, as FUN_0003e93c reads the
                    // cue as it steps the animation: the cue stays up until the
                    // next step, so reading it every player tick played it two
                    // or three times over, each one now cutting off the last.
                    if (weaponPass && weaponView.SoundCued())
                    {
                        ALTEngine::Audio::SfxPlayer::PlayPlayerSlot(
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
                    }

                    // Walking onto a trigger cell runs its chain once on entry.
                    // Door commands in it fail (doors take the use key - see
                    // the door handler); lights, spawns and the level exit
                    // fire, as they do in the original's mover.
                    triggers.OnEnterCell(playerLastTrigger, cellIndex, triggerHandlers);

                    // Collection, as the pickup task does it (0x3c8d0): an item
                    // within 0x300 of the player on both ground axes and 0x400
                    // vertically (FUN_0003c5d0) - so one on a table in the next
                    // cell is in reach - and only if the player has room for it
                    // (Pickups::Wanted); otherwise it stays where it is.
                    for (auto& live : livePickups)
                    {
                        if (live.collected || live.contained || live.waitingOnTrigger || live.falling) { continue; }
                        if (live.placedIndex >= placedObjects.size()) { continue; }
                        {
                            const auto& item = placedObjects[live.placedIndex];
                            if (std::fabs(item.x - camera.x) >= 768.0f || std::fabs(item.z - camera.z) >= 768.0f
                                || std::fabs(item.y - playerFeetY) >= 1024.0f) { continue; }
                        }
                        if (!ALTEngine::Screens::Pickups::Wanted(live.type, static_cast<int>(live.amount), inventory, hudState)) { continue; }
                        live.collected = true;
                        if (live.placedIndex < placedObjects.size()) { placedObjects[live.placedIndex].visible = false; }

                        int amount = static_cast<int>(live.amount) * (live.multiplier == 0 ? 1 : live.multiplier);

                        // The whole table, from FUN_000387c0 - see
                        // Screens/PickupSystem.h. This used to handle a dozen of
                        // the 26 types and let the rest fall through, which is
                        // why derm patches did nothing (Edward, 2026), and it
                        // had the smartgun and flamethrower crossed: pickup type
                        // 3 fills DAT_000b0aca, which is the SMARTGUN's counter,
                        // not the flamethrower's.
                        if (!ALTEngine::Screens::Pickups::Apply(live.type, amount, live.multiplier,
                                                                inventory, hudState))
                        {
                            SDL_Log("GameplayScreen: pickup type %d has no handler", live.type);
                        }

                        // Type it out on the HUD, using the original's own name
                        // for the thing - index 137 + type in its string list.
                        hudMessages.ShowPickup(live.type, language);
                        SDL_Log("GameplayScreen: collected pickup type %d (amount %d)", live.type, amount);
                    }

                    // Interact key - the original's way of opening a door.
                    // Fires on the press edge for the trigger cell the player
                    // is standing on - the only cell the original tests; there
                    // is no neighbourhood or facing test anywhere in its
                    // trigger paths. So a door whose trigger cells are all on
                    // one side can only be opened from that side.
                    bool useHeld = keys[keyBindings.GetKey(InputAction::Use)];
                    if (useHeld && !prevUseHeld && cellIndex >= 0
                        && static_cast<size_t>(cellIndex) < level.collisionGrid.size())
                    {
                        int useAction = level.collisionGrid[static_cast<size_t>(cellIndex)].scriptAction;
                        if (useAction != 0) { triggers.Run(useAction, ALTEngine::Game::Triggers::Source::Interact, triggerHandlers); }
                    }
                    prevUseHeld = useHeld;

                    // The world's tick: the same passes as the player's above,
                    // one for one, each with that pass's own alternate-pass
                    // flag - the original runs everything from one pass counter
                    // (player every pass, the creatures and cosmetic cadences on
                    // alternate ones). It used to keep an accumulator and a
                    // counter of its own, which could fall a tick out of step.
                    // Doors and lifts run every pass: the original moves a door
                    // one height unit per tick, and quantising matters - the
                    // 32-unit step keeps the step-up test and clearance behaving.
                    for (const bool worldHalf : passHalves)
                    {

                        if (worldHalf)
                        {
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

                            // What the player has seen, for the map: every cell the
                            // view walk takes in, and the area they stand in
                            // (CreatureSight::MarkSeen).
                            if (levelReady)
                            {
                                ALTEngine::Game::CreatureSight::MarkSeen(
                                    level, minimapVisited, minimapAreas,
                                    ToGridSpaceX(camera.x, originX) >> 9,
                                    ToGridSpaceZ(camera.z, originZ) >> 9,
                                    playerCam.viewYaw);
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
                        }

                        for (size_t li = 0; li < lifts.size(); ++li)
                        {
                            namespace LF = ALTEngine::Game::Lifts;
                            const auto result = LF::Tick(lifts[li], level);
                            if (result.firstActivation) { hudMessages.ShowLiftActivated(language); }
                            if (result.started) { ALTEngine::Audio::SfxPlayer::PlaySlot(LF::SOUND_START, digits.c_str(), cdDirectory); }
                            if (result.stopped) { ALTEngine::Audio::SfxPlayer::PlaySlot(LF::SOUND_STOP, digits.c_str(), cdDirectory); }
                            if (li < liftPlaced.size())
                            {
                                for (size_t k = 0; k < liftPlaced[li].second; ++k)
                                {
                                    const size_t pi = liftPlaced[li].first + k;
                                    if (pi < placedObjects.size()) { placedObjects[pi].y = lifts[li].PlatformY(); }
                                }
                            }
                        }

                        for (auto& ds : doorStates)
                        {
                            // The notice: a switch-operated door reads as "powered
                            // up", one you reach yourself as "activated" - see
                            // HudMessages for why that split and how confident it is.
                            switch (ALTEngine::Game::Doors::Tick(ds, level, camera.x, camera.z))
                            {
                            case ALTEngine::Game::DoorNotice::DoorActivated: hudMessages.ShowDoorActivated(language); break;
                            case ALTEngine::Game::DoorNotice::DoorPoweredUp: hudMessages.ShowDoorPoweredUp(language); break;
                            case ALTEngine::Game::DoorNotice::LiftActivated: hudMessages.ShowLiftActivated(language); break;
                            case ALTEngine::Game::DoorNotice::None: break;
                            }

                            const float y = ds.MeshY();
                            for (int i = 0; i < ds.placedCount; ++i)
                            {
                                size_t pi = ds.placedFirst + static_cast<size_t>(i);
                                if (pi < placedObjects.size()) { placedObjects[pi].y = y; }
                            }
                        }

                        // Cryopod covers tipping open once activated (0x37bb0):
                        // the third angle falls 8 a tick to -0x100. The model
                        // file is not to hand to check which way that turns
                        // it in the port's frame, so it turns whichever way
                        // lifts the side most of the model lies on.
                        if (!cryopodOpenSignKnown && !cryopods.empty())
                        {
                            if (const auto* faces = ALTEngine::Renderer::ModelRenderer::ModelFaces(
                                    { ALTEngine::Renderer::ModelCatalog::Obj3d, ALTEngine::Formats::ModelIndices::Obj3D::StasisPodCover }))
                            {
                                float sumX = 0.0f;
                                for (const auto& face : *faces) { for (int c = 0; c < 4; ++c) { sumX += face.px[c]; } }
                                cryopodOpenSign = (sumX < 0.0f) ? -1.0f : 1.0f;
                                cryopodOpenSignKnown = true;
                            }
                        }
                        for (auto& pod : cryopods)
                        {
                            if (pod.crateIndex >= objectState.size() || objectState[pod.crateIndex] == 0) { continue; }
                            if (pod.angle > -CRYOPOD_OPEN_LIMIT) { pod.angle -= CRYOPOD_OPEN_STEP; }
                            if (pod.placedIndex < placedObjects.size())
                            {
                                placedObjects[pod.placedIndex].rollRadians =
                                    cryopodOpenSign * static_cast<float>(-pod.angle) * (6.28318531f / 4096.0f);
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
                    // A jump rides on top, held under the ceiling.
                    if (jumpOffset > 0.0f)
                    {
                        const auto* cell = ALTEngine::Formats::CellAt(
                            level, ToGridSpaceX(camera.x, originX) >> 9, ToGridSpaceZ(camera.z, originZ) >> 9);
                        if (cell)
                        {
                            const float headroom = static_cast<float>(cell->ceilingHeight) * 32.0f
                                                 - ALTEngine::Screens::PlayerCamera::JUMP_HEAD_CLEARANCE
                                                 - EYE_HEIGHT - floorY;
                            if (jumpOffset > headroom)
                            {
                                jumpOffset = std::max(0.0f, headroom);
                                if (jumpVelocity > 0.0f) { jumpVelocity = 0.0f; }
                            }
                        }
                    }
                    playerFeetY = floorY + jumpOffset;
                    camera.y = floorY + jumpOffset + EYE_HEIGHT + static_cast<float>(playerCam.bobOffsetY);

                    // Testing: Ghost Mode holds its own height, starting from
                    // where the player stood when it came on. Turned off, the
                    // floor takes them back.
                    if (testingGhost)
                    {
                        if (!ghostActive) { ghostFeetY = playerFeetY; ghostActive = true; }
                        playerFeetY = ghostFeetY;
                        camera.y = ghostFeetY + EYE_HEIGHT;
                    }
                    else
                    {
                        ghostActive = false;
                    }
                }
            }

            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            if (levelReady)
            {
                // Live particles ride along as PlacedObjects. Casings ARE
                // little models in the original - FUN_0002b37c stores a model
                // pointer from the pickup model table into the object - so going
                // through the object path keeps them depth-sorted against the
                // level.
                // Particle sprites. The tracers and impact fragments finally
                // have somewhere to be drawn.
                //
                // The original draws a bullet as a LINE from its position to
                // position + a fraction of its velocity (FUN_0002d3e4), and a
                // fragment as a 2x2 pixel quad (FUN_0002d1b4). The sprite pass
                // draws a line as a strip one of the original's pixels wide,
                // from the round to its tip, turned to face the camera.
                std::vector<ALTEngine::Renderer::PlacedSprite> placedSprites;
                if (levelReady)
                {
                    namespace P = ALTEngine::Screens::Particles;
                    // The jets pick a frame at random each frame (FUN_000100ec).
                    static uint32_t ventFlicker = 1;
                    for (const P::Particle& particle : particles.All())
                    {
                        if (!particle.alive) { continue; }
                        if (particle.type == P::TYPE_CASING_A || particle.type == P::TYPE_CASING_B) { continue; }

                        // The original's own culling: nothing closer than 0x200,
                        // nothing past 0x1801.
                        //
                        // A 3D distance, not a ground-plane one. On the ground
                        // plane a particle at the player's feet measures as
                        // almost zero away and the near cut throws it out, which
                        // hid every impact fired downward.
                        const float dx = particle.x - camera.x;
                        const float dy = particle.y - camera.y;
                        const float dz = particle.z - camera.z;
                        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                        const int nearCutoff = particle.type == P::TYPE_FLAME
                            ? ALTEngine::Formats::ExplosionGraphics::FlameSprites::NEAR_CUTOFF
                            : P::DRAW_NEAR_CUTOFF;
                        if (distance < nearCutoff || distance > P::DRAW_FAR_CUTOFF) { continue; }

                        ALTEngine::Renderer::PlacedSprite spark;
                        spark.textureKey = "particle";
                        spark.x = particle.x;
                        spark.y = particle.y;
                        spark.z = particle.z;

                        // One pixel of the original's 320x240 view at this
                        // distance: the projection puts a world length L at
                        // L * 0x100 / depth pixels.
                        const float originalPixel = distance / 256.0f;
                        if (particle.type == P::TYPE_FRAGMENT)
                        {
                            // A 2x2 pixel square (FUN_0002d1b4), whatever the
                            // distance.
                            spark.halfWidth = originalPixel;
                            spark.halfHeight = originalPixel;
                        }
                        else if (P::IsVentParticle(particle.type))
                        {
                            // Its own frame and size by its life (the draw's
                            // table at 0x2d6a0; ExplosionGraphics::VentSprites),
                            // between 0x200 and 0x1800 away.
                            namespace VS = ALTEngine::Formats::ExplosionGraphics::VentSprites;
                            if (distance < 0x200 || distance > 0x1800) { continue; }
                            const bool steam = particle.type == P::TYPE_VENT_STEAM_H || particle.type == P::TYPE_VENT_STEAM_V;
                            const bool vertical = particle.type == P::TYPE_VENT_STEAM_V || particle.type == P::TYPE_VENT_FLAME_V;
                            ventFlicker = ventFlicker * 1103515245u + 12345u;
                            const VS::Look look = VS::For(steam, vertical, particle.life, static_cast<int>(ventFlicker >> 16));
                            if (!explosions.UseRecord(look.record, spark, steam)) { continue; }
                            spark.halfWidth = look.halfWidth;
                            spark.halfHeight = look.halfHeight;
                            spark.r = spark.g = spark.b = look.grey;
                        }
                        else if (P::IsBlood(particle.type))
                        {
                            // FUN_0002d2bc: a square sprite from the effect
                            // sheet - records 0x20, 0x21 and 0x22 on its first
                            // three draws, 0x18 after - half the size of its
                            // remaining life in world units (life << 8 over the
                            // depth, at the focal length 0x100), in the light's
                            // colour with blue taken out and red too for type 9
                            // (an alien's green acid) or green for type 8 (red
                            // blood); type 10 keeps the light's colour. These
                            // were drawn as white streaks.
                            const int age = particle.strength - particle.life;
                            const int record = (age >= 0 && age < 3) ? 0x20 + age : 0x18;
                            if (!explosions.UseRecord(record, spark)) { continue; }
                            spark.halfWidth = spark.halfHeight = static_cast<float>(std::max(particle.life, 1) * (particle.bigBlood ? 4 : 1));
                            spark.r = (particle.type == P::TYPE_BLOOD_ACID) ? 0.0f : 1.0f;
                            spark.g = (particle.type == P::TYPE_BLOOD_WHITE) ? 0.0f : 1.0f;
                            spark.b = (particle.type == P::TYPE_BLOOD_ACID_B) ? 1.0f : 0.0f;
                        }
                        else if (particle.type == P::TYPE_FLAME)
                        {
                            // A flickering sprite that swells with age
                            // (FUN_0002c94c; ExplosionGraphics::FlameSprites).
                            namespace FS = ALTEngine::Formats::ExplosionGraphics::FlameSprites;
                            ventFlicker = ventFlicker * 1103515245u + 12345u;
                            const FS::Look look = FS::For(particle.life, static_cast<int>(ventFlicker >> 16));
                            if (!explosions.UseRecord(look.record, spark)) { continue; }
                            spark.halfWidth = spark.halfHeight = look.halfSize;
                            spark.r = spark.g = spark.b = FS::SHADE;
                        }
                        else
                        {
                            // A ROUND IS A LINE (FUN_0002d6ec, FUN_0002d3e4),
                            // one pixel wide, from where it is to where it
                            // will be: position + velocity, or + velocity/2
                            // for the pistol. Its colour runs from one set
                            // per type at the round to light 1's at the tip,
                            // both from the level's light table, so with the
                            // shipped lights (light 0 grey, light 1 white):
                            //     pistol (0)       yellow  -> white
                            //     pulse rifle (2)  teal    -> white
                            //     smartgun (4)     cyan    -> white
                            //     a creature's (5) dark red -> white
                            // The shotgun's pellets (1) are never drawn. Types
                            // 0, 2 and 4 are left out at the lowest detail
                            // setting and type 5 at any but the highest
                            // (DAT_000a63fc); the port has no such setting,
                            // so it draws them all, as the highest does.
                            //
                            // The streak used to be a strip centred on the
                            // round and turned by an angle a quarter-turn out,
                            // so a round fired ahead lay across the screen,
                            // and stood on its edge it all but vanished from
                            // behind. Now it is the line itself: from the
                            // round to its tip, its flat side to the camera,
                            // one of the original's pixels wide at each end,
                            // in two halves to carry the colour's run.
                            if (particle.type == P::TYPE_SHOTGUN) { continue; }
                            uint8_t light0[3] = { 160, 160, 160 }, light1[3] = { 255, 255, 255 };
                            ModelRenderer::LevelLightCorner(cacheKey, 0, light0);
                            ModelRenderer::LevelLightCorner(cacheKey, 1, light1);
                            float head[3];
                            switch (particle.type)
                            {
                            case P::TYPE_PISTOL:   head[0] = light1[0]; head[1] = light1[1]; head[2] = 0; break;
                            case P::TYPE_PULSE:    head[0] = 0; head[1] = light0[1]; head[2] = light1[2]; break;
                            case P::TYPE_SMARTGUN: head[0] = 0; head[1] = light1[1]; head[2] = light1[2]; break;
                            default:               head[0] = light0[0]; head[1] = 0; head[2] = 0; break;
                            }
                            const float tail[3] = { float(light1[0]), float(light1[1]), float(light1[2]) };

                            const float fraction = P::TracerVelocityFraction(particle.type);
                            const float ex = particle.vx * fraction, ey = particle.vy * fraction, ez = particle.vz * fraction;
                            // Across the line, facing the camera.
                            const float mx = particle.x + ex * 0.5f - camera.x;
                            const float my = particle.y + ey * 0.5f - camera.y;
                            const float mz = particle.z + ez * 0.5f - camera.z;
                            float sx = ey * mz - ez * my, sy = ez * mx - ex * mz, sz = ex * my - ey * mx;
                            float side = std::sqrt(sx * sx + sy * sy + sz * sz);
                            float length = std::sqrt(ex * ex + ey * ey + ez * ez);
                            float lx = ex, ly = ey, lz = ez;
                            if (side < 1e-3f || length < 1e-3f)
                            {
                                // Coming straight at the camera, or not moving:
                                // a one-pixel dot.
                                const float yawRadians = camera.yaw;
                                sx = std::cos(yawRadians); sy = 0.0f; sz = std::sin(yawRadians);
                                side = 1.0f;
                                lx = 0.0f; ly = originalPixel; lz = 0.0f;
                            }
                            sx /= side; sy /= side; sz /= side;
                            auto pixelAt = [&](float x, float y, float z) {
                                const float ddx = x - camera.x, ddy = y - camera.y, ddz = z - camera.z;
                                return std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz) / 256.0f * 0.5f;
                            };
                            spark.textureKey = "line";
                            spark.u0 = spark.v0 = 0.0f;
                            spark.u1 = spark.v1 = 1.0f;
                            spark.billboard = false;
                            spark.corners = true;
                            for (int half = 0; half < 2; ++half)
                            {
                                const float t0 = half * 0.5f, t1 = t0 + 0.5f;
                                const float ax = lx * t0, ay = ly * t0, az = lz * t0;
                                const float bx = lx * t1, by = ly * t1, bz = lz * t1;
                                const float wa = pixelAt(particle.x + ax, particle.y + ay, particle.z + az);
                                const float wb = pixelAt(particle.x + bx, particle.y + by, particle.z + bz);
                                ALTEngine::Renderer::PlacedSprite segment = spark;
                                segment.cornerX[0] = ax + sx * wa; segment.cornerY[0] = ay + sy * wa; segment.cornerZ[0] = az + sz * wa;
                                segment.cornerX[1] = bx + sx * wb; segment.cornerY[1] = by + sy * wb; segment.cornerZ[1] = bz + sz * wb;
                                segment.cornerX[2] = bx - sx * wb; segment.cornerY[2] = by - sy * wb; segment.cornerZ[2] = bz - sz * wb;
                                segment.cornerX[3] = ax - sx * wa; segment.cornerY[3] = ay - sy * wa; segment.cornerZ[3] = az - sz * wa;
                                for (int corner = 0; corner < 4; ++corner) { segment.cornerU[corner] = 0.5f; segment.cornerV[corner] = 0.5f; }
                                // The colour at the half's middle.
                                const float t = half == 0 ? 0.25f : 0.75f;
                                segment.r = (head[0] + (tail[0] - head[0]) * t) / 255.0f;
                                segment.g = (head[1] + (tail[1] - head[1]) * t) / 255.0f;
                                segment.b = (head[2] + (tail[2] - head[2]) * t) / 255.0f;
                                placedSprites.push_back(segment);
                            }
                            continue;
                        }
                        placedSprites.push_back(spark);
                    }
                    explosions.Collect(placedSprites, camera.x, camera.y, camera.z, true);
                    // Each creature in the light of its cell (FUN_0002fb84 reads
                    // the cell's byte 0x0e entry, as the models do).
                    const size_t firstCreatureSprite = placedSprites.size();
                    enemySprites.Collect(enemies, placedSprites, camera.x, camera.z, true);
                    for (size_t si = firstCreatureSprite; si < placedSprites.size(); ++si)
                    {
                        auto& sprite = placedSprites[si];
                        const auto* cell = ALTEngine::Formats::CellAt(
                            level, ToGridSpaceX(sprite.x, originX) >> 9, ToGridSpaceZ(sprite.z, originZ) >> 9);
                        float rgb[3];
                        if (cell && ModelRenderer::LevelLightColour(cacheKey, cell->lighting, rgb))
                        {
                            sprite.r = rgb[0]; sprite.g = rgb[1]; sprite.b = rgb[2];
                        }
                    }
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
                        // Entries 27 (the pistol's) and 26 (the shotgun's) of the
                        // pickup model table (FUN_0002b37c), turned by both of
                        // its angles, at the draw's scale (FUN_0002cf14).
                        shell.cacheKey = { ALTEngine::Renderer::ModelCatalog::Pickmod,
                                           particle.type == P::TYPE_CASING_B
                                               ? ALTEngine::Formats::ModelIndices::PickMod::ShotgunAmmunition
                                               : ALTEngine::Formats::ModelIndices::PickMod::PistolShell };
                        shell.x = particle.x;
                        shell.y = particle.y;
                        shell.z = particle.z;
                        shell.pitchRadians = ALTEngine::Screens::PlayerCamera::ToRadians(particle.angleX);
                        shell.rotationRadians = ALTEngine::Screens::PlayerCamera::ToRadians(particle.angleY);
                        shell.scaleX = shell.scaleY = shell.scaleZ = P::CasingDrawScale(particle.type);
                        placedObjects.push_back(shell);
                    }

                    // A handler's grenades: entry 12 of the pickup model table
                    // (DAT_00248124, FUN_0002b240), the pulse rifle grenade
                    // the player throws, drawn where it is at full size, from
                    // 0x200 ahead of the view as the player's are
                    // (FUN_0002cf14).
                    for (const EnemyGrenade& g : enemyGrenades)
                    {
                        namespace PCv = ALTEngine::Screens::PlayerCamera;
                        const float ahead = (g.x - camera.x) * (static_cast<float>(PCv::Sin(playerCam.viewYaw)) / PCv::TRIG_ONE)
                                          - (g.z - camera.z) * (static_cast<float>(PCv::Cos(playerCam.viewYaw)) / PCv::TRIG_ONE);
                        if (ahead <= 0x200 || ahead >= 0x1e00) { continue; }
                        ALTEngine::Renderer::PlacedObject shell;
                        shell.cacheKey = { ALTEngine::Renderer::ModelCatalog::Pickmod,
                                           ALTEngine::Formats::ModelIndices::PickMod::PulseRifleGrenade };
                        shell.x = g.x;
                        shell.y = g.y;
                        shell.z = g.z;
                        placedObjects.push_back(shell);
                    }
                    // The player's grenades and charges, each with its own
                    // pickup model (FUN_0002cf14 draws a model for both). It
                    // draws them where they are, at full size: only particle
                    // type 0x15 gets a scale (0x800, half). They were drawn
                    // here at half size and 0x20 low, which left a charge
                    // resting on the floor almost entirely under it.
                    //
                    // Only from 0x200 to 0x1e00 ahead of the view, measured
                    // along its yaw (FUN_0002cf14), so a throw is not seen until
                    // it is 0x200 out; drawn earlier it seemed to come up from
                    // the floor under the player. A drawn charge turns 0x20 on
                    // its first angle each tick, which the grenade does not.
                    namespace PC = ALTEngine::Screens::PlayerCamera;
                    const float viewForwardX = static_cast<float>(PC::Sin(playerCam.viewYaw)) / PC::TRIG_ONE;
                    const float viewForwardZ = -static_cast<float>(PC::Cos(playerCam.viewYaw)) / PC::TRIG_ONE;
                    for (PlayerExplosive& e : playerExplosives)
                    {
                        const float ahead = (e.x - camera.x) * viewForwardX + (e.z - camera.z) * viewForwardZ;
                        if (ahead <= 0x200 || ahead >= 0x1e00) { continue; }
                        ALTEngine::Renderer::PlacedObject shell;
                        shell.cacheKey = { ALTEngine::Renderer::ModelCatalog::Pickmod,
                                           e.charge ? ALTEngine::Formats::ModelIndices::PickMod::SeismicCharge
                                                    : ALTEngine::Formats::ModelIndices::PickMod::PulseRifleGrenade };
                        shell.x = e.x;
                        shell.y = e.y;
                        shell.z = e.z;
                        shell.pitchRadians = PC::ToRadians(e.spin);
                        placedObjects.push_back(shell);
                    }
                }

                // The view, drawn between the last two player ticks (see
                // prevCamX). Everything that simulates or tests collision keeps
                // using `camera`; this copy is only for drawing.
                ALTEngine::Renderer::FpsCamera view = camera;
                if (prevCamValid)
                {
                    namespace PC = ALTEngine::Screens::PlayerCamera;
                    const float tickLength = (speedCompensationOff ? 2.0f : 1.0f) / static_cast<float>(PC::TICK_HZ);
                    const float alpha = std::clamp(playerTickAccumulator / tickLength, 0.0f, 1.0f);
                    view.x = prevCamX + (camera.x - prevCamX) * alpha;
                    view.z = prevCamZ + (camera.z - prevCamZ) * alpha;

                    // Yaw the short way round, plus whatever mouse turn has
                    // arrived since the last tick and not been simulated yet -
                    // so mouse look answers every frame rather than every tick.
                    constexpr float TWO_PI = 6.28318530718f;
                    float turn = camera.yaw - prevCamYaw;
                    while (turn > TWO_PI * 0.5f) { turn -= TWO_PI; }
                    while (turn < -TWO_PI * 0.5f) { turn += TWO_PI; }
                    view.yaw = prevCamYaw + turn * alpha
                             + pendingMouseYaw * (TWO_PI / static_cast<float>(PC::ANGLE_UNITS));
                }

                // Every model takes the light of the cell it is in (its byte
                // 0x0e; FUN_000269b8 and its siblings), looked up where it is
                // now so lifts, doors and anything thrown or dropped follow.
                // The level-face objects (window glass, a cryopod's cover)
                // carry their faces' own lights and are left alone.
                for (auto& object : placedObjects)
                {
                    if (object.cacheKey.catalog == ALTEngine::Renderer::ModelCatalog::LevelObject) { continue; }
                    const auto* cell = ALTEngine::Formats::CellAt(
                        level, ToGridSpaceX(object.x, originX) >> 9, ToGridSpaceZ(object.z, originZ) >> 9);
                    object.lightId = cell ? cell->lighting : -1;
                }
                // The cells the original would draw from here (0x2745b): its
                // view walk from the camera's cell along the view's yaw, out to
                // its last ring (14 cells). Its cones were made for its own
                // view; the port's can be wider (aspect, and looking up or down
                // turns the screen's corners further round), so each cone
                // widens to the screen's widest bearing and the walk adds a
                // cell's width per ring, so nothing at the edges drops out.
                if (levelReady)
                {
                    namespace PCd = ALTEngine::Screens::PlayerCamera;
                    const float aspect = windowH > 0 ? static_cast<float>(windowW) / static_cast<float>(windowH) : 1.0f;
                    const float tanV = std::tan(view.fovYRadians * 0.5f);
                    const float pitch = std::fabs(view.pitch);
                    // The forward reach of the screen's corner ray most
                    // turned towards the floor or ceiling.
                    const float forward = std::cos(pitch) - tanV * std::sin(pitch);
                    const float halfFov = forward > 0.05f ? std::atan(tanV * aspect / forward) : 3.14159265f;
                    const int minHalfWidth = std::min(0x800,
                        static_cast<int>(halfFov * (PCd::ANGLE_UNITS / 6.28318530718f)) + 0x20);
                    const int yawUnits = static_cast<int>(std::lround(view.yaw * (PCd::ANGLE_UNITS / 6.28318530718f)));
                    std::vector<uint8_t> drawnCells = ALTEngine::Game::CreatureSight::DrawnCells(
                        level, ToGridSpaceX(view.x, originX) >> 9, ToGridSpaceZ(view.z, originZ) >> 9,
                        (yawUnits + 0x800) & PCd::ANGLE_MASK, minHalfWidth);
                    // A camera off the map, or in a cell with no zones (inside
                    // a wall), walks to nothing; draw the whole level then
                    // rather than nothing.
                    if (std::count(drawnCells.begin(), drawnCells.end(), uint8_t(1)) <= 1) { drawnCells.clear(); }
                    ModelRenderer::SetLevelDrawnCells(cacheKey, drawnCells);
                }
                // Kept between frames: a new frame-sized buffer each frame
                // was a needless allocation and clear.
                static std::vector<uint8_t> pixels;
                ModelRenderer::RenderLevelToRgba(cacheKey, view, windowW, windowH, placedObjects, placedSprites, pixels);
                placedObjects.resize(staticObjectCount); // drop this frame's particles again
                if (!pixels.empty())
                {
                    // One texture for the whole level, recreated only when the
                    // window size changes - this used to be created and
                    // destroyed every frame.
                    if (frameTexture && (frameTextureW != windowW || frameTextureH != windowH))
                    {
                        SDL_DestroyTexture(frameTexture);
                        frameTexture = nullptr;
                    }
                    if (!frameTexture)
                    {
                        frameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, windowW, windowH);
                        frameTextureW = windowW;
                        frameTextureH = windowH;
                    }
                    if (frameTexture)
                    {
                        SDL_UpdateTexture(frameTexture, nullptr, pixels.data(), windowW * 4);
                        SDL_RenderTexture(renderer, frameTexture, nullptr, nullptr);

                        // Testing: Face Index Display. The face under the
                        // crosshair, outlined, with a dot at the centre of the
                        // view where the pick is taken; its numbers go on the
                        // HUD below.
                        pickedFace = -1;
                        if (testingFaceIndex)
                        {
                            DrawFacePick(renderer, level, view, windowW, windowH, pickedFace);
                        }

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
                        // The HUD prints what the inventory holds. The inventory is
                        // the only copy of the ammunition - firing, reloading and
                        // pickups all change it directly - so this is a plain
                        // read, every frame, with nothing to keep in step.
                        {
                            hudState.currentAmmoDisplay = inventory.AmmoTotal(selectedWeapon);

                            // Grenades. The original keeps them in the high half
                            // of the pulse rifle's own ammo word (DAT_000b0ac6),
                            // capped at 0x14 - they belong to that weapon, not to
                            // a slot of their own.
                            hudState.grenades = static_cast<int16_t>(
                                inventory.pulseRifle.grenades > ALTEngine::Screens::PlayerHudState::GRENADE_MAX
                                    ? ALTEngine::Screens::PlayerHudState::GRENADE_MAX
                                    : inventory.pulseRifle.grenades);
                            hudState.seismicCharges = static_cast<int16_t>(std::min(inventory.seismicCharges, 999));
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
                        hud.Draw(renderer, hudState, windowW, windowH,
                            [&] {
                                weaponView.Draw(renderer,
                                                ALTEngine::Renderer::HUD_VIRTUAL_WIDTH,
                                                ALTEngine::Renderer::HUD_VIRTUAL_HEIGHT);
                            },
                            [&] {
                                // ---- the face hugger on the camera --------
                                //
                                // Two halves of one mirrored sprite, meeting at
                                // x=0xa0, climbing from 0x154 below the screen.
                                // Drawn over the panels because it is on the
                                // camera, not behind the HUD.
                                if (faceHug.Active())
                                {
                                    for (const auto& piece : faceHug.Visible())
                                    {
                                        if (piece.sheetIndex < 0
                                            || piece.sheetIndex >= static_cast<int>(faceHugSheets.size())
                                            || !piece.frame) { continue; }

                                        ALTEngine::Renderer::ModelRenderer::DrawSpriteSheet2D(
                                            renderer, faceHugSheets[static_cast<size_t>(piece.sheetIndex)],
                                            static_cast<float>(piece.x), static_cast<float>(piece.y),
                                            piece.width, piece.height);
                                    }
                                }

                                // The typed-out messages, at 1:1 in the HUD
                                // surface so the text lands on whole pixels.
                                // Newest at the bottom, the way a terminal
                                // scrolls.
                                // The resupply countdown, two digits in the HUD
                                // font at (16, 19) (FUN_00043720): steady above
                                // 17 seconds, blinking from 17 down.
                                if (resupply.active && !testingNoResupply)
                                {
                                    namespace RT = ALTEngine::Game::ResupplyTimer;
                                    const SDL_Color colour = !RT::Blinking(resupply) ? SDL_Color{ 0, 255, 0, 255 }
                                        : RT::BlinkPhase(resupply) ? SDL_Color{ 255, 255, 0, 255 } : SDL_Color{ 255, 0, 0, 255 };
                                    hud.DrawHudText(renderer, RT::Text(resupply), RT::HUD_X, RT::HUD_Y,
                                                    ALTEngine::Renderer::HUD_FONT_FIRST_DESCRIPTOR, 1.0f, 1.0f, 0, colour);
                                }

                                // Testing: Face Index Display - the picked face's
                                // number, texture, draw routine and file offset,
                                // and its vertex list, centred at the top.
                                if (testingFaceIndex)
                                {
                                    namespace R = ALTEngine::Renderer;
                                    std::string lines[2];
                                    if (pickedFace >= 0)
                                    {
                                        const auto& q = level.quads[static_cast<size_t>(pickedFace)];
                                        char buf[160];
                                        std::snprintf(buf, sizeof(buf), "FACE %d  TEX %u  ROUTINE %u  LIGHT %u",
                                                      pickedFace, static_cast<unsigned>(q.texIndex),
                                                      static_cast<unsigned>(q.flags), static_cast<unsigned>(q.reserved));
                                        lines[0] = buf;
                                        std::snprintf(buf, sizeof(buf), "OFFSET 0x%zX  V %d %d %d %d",
                                                      ALTEngine::Formats::LevelFaceFileOffset(level, pickedFace),
                                                      q.a, q.b, q.c, q.d);
                                        lines[1] = buf;
                                    }
                                    else
                                    {
                                        lines[0] = "NO FACE";
                                    }
                                    int y = 3;
                                    for (const std::string& line : lines)
                                    {
                                        if (line.empty()) { continue; }
                                        // Font C's letters are about 5 pixels wide;
                                        // close enough to centre the line.
                                        const int width = static_cast<int>(line.size())
                                            * (5 + R::HUD_FONT_C_LETTER_SPACING);
                                        hud.DrawHudText(renderer, line, std::max(0, (R::HUD_VIRTUAL_WIDTH - width) / 2), y,
                                                        R::HUD_FONT_C_FIRST_DESCRIPTOR, 1.0f, 1.0f,
                                                        R::HUD_FONT_C_LETTER_SPACING, SDL_Color{ 255, 255, 0, 255 });
                                        y += R::HUD_MESSAGE_LINE_HEIGHT;
                                    }
                                }

                                const auto visible = hudMessages.Visible();
                                if (visible.empty()) { return; }

                                // NO PANEL AT ALL - just the text, the way the
                                // original prints it. A filled box behind it was
                                // wrong twice over: rounded first, then square,
                                // when there should be nothing there (Edward,
                                // 2026).
                                //
                                // Drawn in the game's own SMALL HUD font - font C,
                                // 5 pixels tall against font A's 9. The menus' 8x8
                                // face and font A were both too big against a
                                // 320x240 surface.
                                namespace R = ALTEngine::Renderer;
                                int lineY = R::HUD_MESSAGE_Y;
                                for (size_t i = 0; i < visible.size(); ++i)
                                {
                                    const std::string& line = visible[i];
                                    const int drawn = hud.DrawHudText(
                                        renderer, line, R::HUD_MESSAGE_X, lineY,
                                        R::HUD_FONT_C_FIRST_DESCRIPTOR, 1.0f, 1.0f,
                                        R::HUD_FONT_C_LETTER_SPACING);

                                    // A caret on whichever line is still being
                                    // revealed, so it reads as being typed.
                                    if (i == hudMessages.TypingLine())
                                    {
                                        // Glyph 0x06, the font's own block cursor.
                                        const std::string caret(1, R::HUD_MESSAGE_CARET_CHAR);
                                        hud.DrawHudText(renderer, caret,
                                                        R::HUD_MESSAGE_X + drawn, lineY,
                                                        R::HUD_FONT_C_FIRST_DESCRIPTOR, 1.0f, 1.0f,
                                                        R::HUD_FONT_C_LETTER_SPACING);
                                    }
                                    lineY += R::HUD_MESSAGE_LINE_HEIGHT;
                                }
                            });

                        // Motion tracker. DrawTracker draws the dish AND its
                        // blips, in that order, so the contacts land on top of the
                        // artwork.
                        //
                        // I had drawn them through the over-panels hook instead,
                        // which runs BEFORE the dish - so the dish covered them
                        // (Edward, 2026). The plumbing for this was already here
                        // and empty; I should have filled it rather than adding a
                        // second path.
                        std::vector<ALTEngine::Renderer::HudRenderer::Contact> contacts;
                        for (const auto& enemy : enemies)
                        {
                            if (!enemy.active) { continue; }
                            ALTEngine::Renderer::HudRenderer::Contact c;
                            c.dx = enemy.x - camera.x;
                            c.dz = enemy.z - camera.z;
                            c.type = enemy.type;
                            c.state = enemy.state;
                            c.moving = enemy.velocityActive;
                            contacts.push_back(c);
                        }
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

                            // What has been seen in the discovered areas; with the
                            // Auto Mapper, every cell of a discovered area.
                            const std::vector<uint8_t> hudMapReveal = ALTEngine::Game::CreatureSight::MapReveal(
                                level, minimapVisited, minimapAreas, inventory.hasAutoMapper);
                            const std::vector<uint8_t>* fog = &hudMapReveal;
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

            // Leave after the frame is drawn, so the exit is seen before the
            // assessment screen. Dying on the same frame takes precedence.
            if (levelComplete && !playerDead && running)
            {
                result.outcome = GameplayOutcome::LevelComplete;
                result.levelSeconds = levelSeconds;
                running = false;
            }
        }

        SDL_SetWindowRelativeMouseMode(app.Window(), false);
        if (frameTexture) { SDL_DestroyTexture(frameTexture); frameTexture = nullptr; }

        result.aliensTotal = static_cast<int>(enemies.size());
        for (const auto& enemy : enemies)
        {
            if (!enemy.alive) { result.aliensKilled++; }
        }
        tally.secretsFound = 0;
        for (int area = 1; area < 256; ++area) { if (secretsSeen[static_cast<size_t>(area)] != 0) { tally.secretsFound++; } }
        result.tally = tally;

        // The game over sequence, after the level is done being drawn so the last
        // frame the player saw is the one that killed them.
        if (playerDead)
        {
            ModelRenderer::UnloadLevels();
            ALTEngine::Screens::GameOverSequence::Play(cdDirectory, language, deathVideoIndex);
            result.outcome = GameplayOutcome::PlayerDied;
            return result;
        }

        ModelRenderer::UnloadLevels();
        return result;
    }
}
