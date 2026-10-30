#pragma once

#include "../Formats/ModelIndices.h"
#include "../Game/Progression.h"

#include <cstdint>
#include <string>

namespace ALTEngine::Screens::GameplayObjects
{
    // The rules for a level's objects (crates, switches, barrels, windows,
    // cryopods) that the gameplay screen applies: which model each type draws,
    // which have none, and the constants of the ones with tasks of their own.
    // All from FUN_00037184 and the object tasks it registers.

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
    //
    // FUN_00037184's switch on the object type. The four switch types and
    // the plain crate take a second model on episode 3's levels (campaign
    // index 0x16-0x22, 3.1.1 onward) and on 908/909 (the byte table at
    // 0xa706b): the boneship switches and the egg husk.
    inline int SwitchFamilyBase(uint8_t type, bool episodeThree)
    {
        using namespace ALTEngine::Formats::ModelIndices;
        switch (type)
        {
        case 0x16: return episodeThree ? Obj3D::BoneshipSwitchRedLeftLight3 : Obj3D::SwitchBatteryRedLeftLight;
        case 0x18: return episodeThree ? Obj3D::BoneshipSwitchRedLeftLight1 : Obj3D::SwitchRedLeftLight;
        case 0x1a: return episodeThree ? Obj3D::BoneshipSwitchRedLeftLight4 : Obj3D::LargeSwitchBatteryRedLeftLight;
        case 0x1b: return episodeThree ? Obj3D::BoneshipSwitchRedLeftLight2 : Obj3D::LargeSwitchRedLeftLight;
        default: return -1;
        }
    }

    // The objects whose model changes on episode 3 (FUN_00037184's test of
    // DAT_000b0ca8): campaign levels 3.1.1 to 3.9.1, and 908 and 909.
    inline bool EpisodeThreeObjects(const std::string& digits)
    {
        const int index = ALTEngine::Game::Progression::CampaignIndex(digits);
        if (index >= 0x16 && index <= 0x22) { return true; }
        return digits == "908" || digits == "909";
    }

    // Objects with no model of their own (FUN_00037184's arm for 0x15,
    // 0x1c and 0x1d sets none).
    inline bool IsInvisibleObjectType(uint8_t type)
    {
        return type == 0x15 || type == 0x1c || type == 0x1d;
    }

    // The OBJ3D model an object type draws with (FUN_00037184; the
    // model pointers at DAT_00247b84 are OBJ3D's models in order).
    inline int Obj3DIndexForCrateType(uint8_t type, bool episodeThree)
    {
        using namespace ALTEngine::Formats::ModelIndices;
        // A switch shows both lights off until its state says otherwise.
        const int family = SwitchFamilyBase(type, episodeThree);
        if (family >= 0) { return family + static_cast<int>(SwitchState::BothOff); }
        switch (type)
        {
        case 0x14: return episodeThree ? Obj3D::EggHusk : Obj3D::SingleCrate;
        case 0x17: return Obj3D::ExplosiveBarrel;
        case 0x19: return Obj3D::DoubleCrate;
        case 0x20: return Obj3D::StasisPodCover;
        case 0x21: case 0x22: case 0x23: case 0x24: case 0x25:
            return Obj3D::SteelCoil + (type - 0x21);   // 35-39
        default: return Obj3D::SingleCrate;            // and the modelless 0x15/0x1c/0x1d, never drawn
        }
    }

    // FUN_0003d794's FUN_00040ba0(9, -1): the sound a seismic charge's
    // throw plays.
    constexpr int SEISMIC_THROW_SOUND = 9;
    // A window: the object type with its own arm in the modelless objects'
    // handler (0x38097), and the sound that arm plays (FUN_00040c38(0x6c)).
    constexpr int WINDOW_OBJECT_TYPE = 0x1c;
    // The cryopod: object type 0x20, its task at 0x37bb0. Once activated
    // (+0x1a set), its third angle falls by 8 a tick until it passes
    // -0x100 - the cover tipping open about its own Z.
    constexpr uint8_t CRYOPOD_OBJECT_TYPE = 0x20;
    constexpr int OBJECT_ACTIVATE_SOUND = 0x5c;
    constexpr int CRYOPOD_OPEN_STEP = 8;
    constexpr int CRYOPOD_OPEN_LIMIT = 0x100;
    constexpr int WINDOW_BREAK_SOUND = 0x6c;

    // The level texture a window's flying glass wears (FUN_00045808): entry
    // 0x118 of the level's texture table on campaign level 4 (L131), 0x158
    // on level 0xc (L211) and 0x14f on every other.
    inline int GlassTextureIndex(const std::string& digits)
    {
        const int index = ALTEngine::Game::Progression::CampaignIndex(digits);
        if (index == 4) { return 0x118; }
        if (index == 0xc) { return 0x158; }
        return 0x14f;
    }
}
