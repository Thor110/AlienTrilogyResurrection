#pragma once

#include <string>
#include <vector>

#include "MenuNode.h"
#include "../Bootstrap/AudioSettings.h"
#include "../Bootstrap/GameplaySettings.h"
#include "../Bootstrap/InputActions.h"
#include "../Bootstrap/KeyBindings.h"
#include "../Bootstrap/Localization.h"
#include "../Bootstrap/RenderSettings.h"
#include "../Bootstrap/StringId.h"

namespace ALTEngine::Menu
{
    // Internal label of the main-menu level list.
    //
    // Deliberately NOT "Level Select": the Modern feature toggle uses that, and
    // `label` is what every comparison in MenuController keys on. While both
    // carried it, choosing "On" for the toggle matched the level-start handler
    // and the engine tried to load a level called "On" (Edward, 2026). Shared as
    // a constant so the two places that must agree cannot drift again - they
    // already did once, as two separate string literals.
    inline constexpr const char* LEVEL_SELECT_LIST_LABEL = "Level Select List";

    // OPTOBJ.BND model indices - CONFIRMED against the real OPTOBJ.BND
    // (Edward, 2026): all 14 sections' identifier bytes match this list
    // exactly, in exact section order (M000-M013 = index 0-13). Also
    // cross-confirmed independently by ModelRenderer.cs's own header-byte
    // comment before that, and originally guessed correctly from that
    // same comment's ordering before either confirmation existed.
    //
    // Real-world device names for indices 2/3/13 use the actual original
    // button text (SpaceOrb 360, Gravis Grip/Gravis Pad, VFX-1) rather
    // than the generic visual-impression names (Gamepad/Multitap/
    // Headphones) Edward had initially used purely from looking at the
    // models - see the per-constant comments below.
    //
    // See Formats/ModelIndices.h for the full three-catalog reference
    // (OBJ3D/PICKMOD/OPTOBJ) salvaged alongside this.
    //
    // Lives in the header (not MenuTree.cpp, where it originally was) so
    // other translation units can reference it too - MenuController.cpp
    // needs ModelIndex::Multitap/SpeakerMusic/SpeakerSfx to pick the
    // right transparency colour when loading those models.
    namespace ModelIndex
    {
        constexpr int Joystick = 0;
        constexpr int Camera = 1;
        constexpr int Gamepad = 2;  // real device: SpaceOrb 360 (confirmed original button text) - "Gamepad" was Edward's visual-impression label from looking at the model, not the actual device name
        constexpr int Multitap = 3; // real devices: Gravis Grip AND Gravis Pad (confirmed original button text) - both use this index, since both look like a multitap; "Multitap" was likewise a visual impression, not necessarily the model's actual real-world identity
        constexpr int HarddriveLeft = 4;  // Hard Drive Saving <-
        constexpr int HarddriveRight = 5; // Hard Drive Loading ->
        constexpr int CameraCrossedOut = 6;
        constexpr int Keyboard = 7;
        constexpr int Mouse = 8;
        constexpr int Computer = 9; // Computer, Monitor and Keyboard
        constexpr int NetworkedComputers = 10; // Two Linked Computers, Monitors and Keyboards (Multiplayer)
        constexpr int SpeakerMusic = 11; // Speaker (Disc Music)
        constexpr int SpeakerSfx = 12;   // Speaker (Sound Effects)
        constexpr int Headphones = 13;   // real device: VFX-1 (confirmed original button text) - "Headphones" was Edward's visual-impression label; VFX-1 was actually a VR headset
    }

    // Current values of every persisted setting the Options tree needs,
    // so BuildMainMenuTree can compute each settings list's
    // initialSelectedChild (which child should be highlighted the first
    // time that list is entered) - matching what's actually saved,
    // rather than always defaulting to the first option (Edward, 2026).
    struct MenuSettingsSnapshot
    {
        ALTEngine::Bootstrap::RenderFidelity quality = ALTEngine::Bootstrap::RenderFidelity::Original;
        std::string resolutionLabel; // e.g. "1920x1080" - matches one of resolutionLabels, or empty if none match
        bool vsync = true;
        ALTEngine::Bootstrap::DisplayMode displayMode = ALTEngine::Bootstrap::DisplayMode::Fullscreen;
        ALTEngine::Bootstrap::Difficulty difficulty = ALTEngine::Bootstrap::Difficulty::AcidReign;
        bool cameraSwayOn = true;
        ALTEngine::Bootstrap::Language language = ALTEngine::Bootstrap::Language::English;
    };

    // Builds the root menu tree: Main Menu (Start Game / Multiplayer /
    // Load Game / Options) with the full Options subtree (Volume,
    // Controls, Difficulty, Camera Sway, Graphics, Language, Credits)
    // attached under "Options" - matches the reference screenshots.
    // "Graphics" is new, not in the original game - contains "Quality"
    // (the authentic-vs-smoothed toggle) and "Resolution".
    //
    // `resolutionLabels` becomes the Resolution submenu's children (e.g.
    // "1920x1080", "1280x720") - passed in rather than queried here so
    // this stays SDL-free and unit-testable; MenuController queries the
    // real display modes and passes them in. Pass an empty vector (the
    // default) for testing - Resolution will just have no options.
    //
    // `settings` is the current value of every persisted setting - see
    // MenuSettingsSnapshot above. Pass the default-constructed snapshot
    // (as before) for testing without real settings.
    //
    // Model indices are provisional (see MenuTree.cpp's ModelIndex
    // namespace) - not resolved OPTOBJ section indices, since we don't
    // have OPTOBJ.BND to confirm real indices against. Sub-items not
    // visually confirmed in the reference set (Mouse's own redefine
    // screen, Gravis Pad/SpaceOrb 360/VFX-1 controller models) are
    // structurally present but their modelIndex is left unset (-1, falls
    // back to "Computer") since there's no reference image confirming
    // what they'd actually show.
    // `keyBindings` supplies the current keyboard/mouse bindings shown
    // in Controls > Keyboard/Mouse > Redefine (Edward, 2026). Pass a
    // Config-backed KeyBindings from the caller; there's no meaningful
    // "default" for testing without one since it needs a real Config.
    // `levelSelectLabels` adds a TEMPORARY "Level Select" list above Start
    // Game, one row per level, for jumping straight into any level to test it.
    // Pass an empty vector to leave it out entirely.
    //
    // TEMPORARY - the original had no level select and the shipped game should
    // not either. To remove: drop this parameter, the LevelSelect() list in
    // MenuTree.cpp, the "Level Select" branch in MenuController, and the block
    // in main.cpp that acts on it.
    MenuNode BuildMainMenuTree(const std::vector<std::string>& resolutionLabels, const MenuSettingsSnapshot& settings,
                                ALTEngine::Bootstrap::KeyBindings& keyBindings, ALTEngine::Bootstrap::AudioSettings& audioSettings,
                                bool includeCredits = true,
                                const std::vector<std::string>& levelSelectLabels = {});
}
