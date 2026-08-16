#pragma once

#include "../Formats/SpriteAnimator.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Renderer
{
    // Every weapon's frame offsets and animation sequences, transcribed from the
    // contiguous data block at 0x090000-0x0903ff. Nothing here is synthesised.
    //
    // FUN_000401d0 holds one pointer array per weapon; each entry is a pair,
    // {frame table, sequence}, one pair per state. The arrays live at 0x0ace28,
    // 0x0ace48, 0x0ace58, 0x0ace70 and 0x0ace88.
    //
    // A frame record is 12 bytes: int16 offsetX, offsetY, width, height, then a
    // 4-byte offset into the compressed frame data. Only the offsets are needed
    // here - SpriteFrameLoader already decodes the .B16 for the pixels - and
    // they are the sprite's TOP-LEFT relative to the weapon anchor, not a
    // bottom alignment.
    //
    // A sequence is uint16: [0] frame duration, [1] frame count, then the
    // program. Entries with bit 0x8000 clear are frame indices; entries with it
    // set are opcodes. See Formats/SpriteAnimator.h.
    //
    // WHICH TABLE IS WHICH WEAPON is settled by the sound ids their OP_EVENT
    // opcodes carry, which is the only unambiguous evidence in the data:
    //     0x0ace58 -> 0x0d 0602hand              pistol
    //     0x0ace48 -> 0x10/0x0f 0702/0701shot    shotgun
    //     0x0ace28 -> 0x06 0302puls, 0x07 0401gren
    //                                            PULSE RIFLE, and its fourth
    //                                            state is the grenade launcher
    //     0x0ace70 -> 0x0b 0503-fla, 0x0a 0501flam
    //                                            flamethrower - and its firing
    //                                            sequence is a single frame on
    //                                            OP_LOOP(6), replaying the
    //                                            sound each pass, which is what
    //                                            a continuous weapon looks like
    //     0x0ace88 -> 0x13/0x12 0902/0901smar    smartgun
    //
    // THIS CORRECTS AN ASSUMPTION. The four-state table is the PULSE RIFLE, not
    // the flamethrower - the M41A's underslung grenade launcher is the extra
    // state, and 0401gren confirms it. So in FUN_0003efcc, the case that fires
    // grenades is the pulse rifle and the case after it is the flamethrower,
    // which is the reverse of what WeaponSystem.h currently assumes for those
    // two indices. See ORIGINAL_TABLE_ORDER below.
    namespace WeaponData
    {
        namespace SA = ALTEngine::Formats::SpriteAnim;
        using SA::Op;

        struct FrameOffset { int x; int y; };

        // ---- Pistol, table 0x0ace58
        // IDLE: 1 frame(s), 40x68
        inline constexpr FrameOffset PISTOL_IDLE_OFFSETS[] = { { -7, -64 } };
        inline const std::vector<uint16_t> PISTOL_IDLE_SEQUENCE{ 4, 1, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x00) };
        // FIRING: 3 frame(s), 40x88 40x72 40x68
        inline constexpr FrameOffset PISTOL_FIRING_OFFSETS[] = { { -10, -86 }, { -7, -67 }, { -6, -64 } };
        inline const std::vector<uint16_t> PISTOL_FIRING_SEQUENCE{ 2, 3, Op(SA::OP_EVENT, 0x0d), Op(SA::OP_SET_FLAG1), 0, 1, 2, Op(SA::OP_END) };
        // RELOAD: 3 frame(s), 76x84 104x108 84x76
        inline constexpr FrameOffset PISTOL_RELOAD_OFFSETS[] = { { -39, -82 }, { -38, -105 }, { -50, -74 } };
        inline const std::vector<uint16_t> PISTOL_RELOAD_SEQUENCE{ 3, 3, Op(SA::OP_SET_FLAG1), 0, 1, 2, Op(SA::OP_END) };

        // ---- Shotgun, table 0x0ace48
        // IDLE: 1 frame(s), 84x80
        inline constexpr FrameOffset SHOTGUN_IDLE_OFFSETS[] = { { -37, -76 } };
        inline const std::vector<uint16_t> SHOTGUN_IDLE_SEQUENCE{ 4, 1, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x00) };
        // FIRING: 7 frame(s), 72x104 76x92 72x84 64x84 76x92 64x84 72x84
        inline constexpr FrameOffset SHOTGUN_FIRING_OFFSETS[] = { { -26, -100 }, { -29, -90 }, { -24, -81 }, { -21, -82 }, { -37, -87 }, { -21, -82 }, { -24, -81 } };
        inline const std::vector<uint16_t> SHOTGUN_FIRING_SEQUENCE{ 2, 7, Op(SA::OP_EVENT, 0x10), Op(SA::OP_SET_FLAG1), 0, 1, 2, 3, Op(SA::OP_EVENT, 0x0f), 4, 5, 6, Op(SA::OP_END) };

        // ---- PulseRifle, table 0x0ace28
        // IDLE: 1 frame(s), 84x68
        inline constexpr FrameOffset PULSERIFLE_IDLE_OFFSETS[] = { { -18, -64 } };
        inline const std::vector<uint16_t> PULSERIFLE_IDLE_SEQUENCE{ 4, 1, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x00) };
        // FIRING: 3 frame(s), 84x88 84x92 88x92
        inline constexpr FrameOffset PULSERIFLE_FIRING_OFFSETS[] = { { -18, -84 }, { -20, -88 }, { -21, -87 } };
        inline const std::vector<uint16_t> PULSERIFLE_FIRING_SEQUENCE{ 2, 3, Op(SA::OP_EVENT, 0x06), Op(SA::OP_SET_FLAG1), 0, 1, 2, Op(SA::OP_END) };
        // RELOAD: 4 frame(s), 92x76 128x76 124x76 92x76
        inline constexpr FrameOffset PULSERIFLE_RELOAD_OFFSETS[] = { { -29, -73 }, { -36, -73 }, { -35, -73 }, { -29, -73 } };
        inline const std::vector<uint16_t> PULSERIFLE_RELOAD_SEQUENCE{ 2, 4, Op(SA::OP_SET_FLAG1), 0, 1, 2, 3, Op(SA::OP_END) };
        // GRENADE: 2 frame(s), 72x56 80x64
        inline constexpr FrameOffset PULSERIFLE_GRENADE_OFFSETS[] = { { -1, -52 }, { -12, -60 } };
        inline const std::vector<uint16_t> PULSERIFLE_GRENADE_SEQUENCE{ 2, 2, Op(SA::OP_EVENT, 0x07), Op(SA::OP_SET_FLAG1), 0, 1, Op(SA::OP_END) };

        // ---- Flamethrower, table 0x0ace70
        // IDLE: 1 frame(s), 68x72
        inline constexpr FrameOffset FLAMETHROWER_IDLE_OFFSETS[] = { { -17, -69 } };
        inline const std::vector<uint16_t> FLAMETHROWER_IDLE_SEQUENCE{ 4, 1, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x00) };
        // FIRING: 1 frame(s), 64x72
        inline constexpr FrameOffset FLAMETHROWER_FIRING_OFFSETS[] = { { -15, -68 } };
        inline const std::vector<uint16_t> FLAMETHROWER_FIRING_SEQUENCE{ 1, 1, Op(SA::OP_EVENT, 0x0b), Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x06) };
        // RELOAD: 5 frame(s), 64x72 72x88 92x100 72x88 64x72
        inline constexpr FrameOffset FLAMETHROWER_RELOAD_OFFSETS[] = { { -15, -68 }, { -22, -83 }, { -42, -98 }, { -22, -83 }, { -15, -68 } };
        inline const std::vector<uint16_t> FLAMETHROWER_RELOAD_SEQUENCE{ 4, 5, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_EVENT, 0x0a), 1, 2, 3, 4, Op(SA::OP_END) };

        // ---- Smartgun, table 0x0ace88
        // IDLE: 1 frame(s), 120x56
        inline constexpr FrameOffset SMARTGUN_IDLE_OFFSETS[] = { { -52, -51 } };
        inline const std::vector<uint16_t> SMARTGUN_IDLE_SEQUENCE{ 1, 1, Op(SA::OP_SET_FLAG1), 0, Op(SA::OP_LOOP, 0x00) };
        // FIRING: 4 frame(s), 124x112 128x108 124x112 128x108
        inline constexpr FrameOffset SMARTGUN_FIRING_OFFSETS[] = { { -60, -107 }, { -59, -103 }, { -60, -107 }, { -59, -103 } };
        inline const std::vector<uint16_t> SMARTGUN_FIRING_SEQUENCE{ 2, 4, Op(SA::OP_EVENT, 0x13), Op(SA::OP_SET_FLAG1), 0, 1, 2, 3, Op(SA::OP_END) };
        // RELOAD: 4 frame(s), 128x52 124x84 116x116 128x52
        inline constexpr FrameOffset SMARTGUN_RELOAD_OFFSETS[] = { { -56, -49 }, { -67, -79 }, { -62, -111 }, { -56, -49 } };
        inline const std::vector<uint16_t> SMARTGUN_RELOAD_SEQUENCE{ 2, 4, Op(SA::OP_SET_FLAG1), 0, 1, Op(SA::OP_EVENT, 0x12), 2, 3, Op(SA::OP_END) };

        // The original's weapon order, from FUN_000401d0's switch:
        //     0 pistol, 1 shotgun, 2 pulse rifle, 3 flamethrower, 4 smartgun
        //
        // PlayerHudState orders its weapons pistol, shotgun, flamethrower,
        // pulse rifle, smartgun - indices 2 and 3 the other way round. Rather
        // than renumber the port's inventory, which would touch pickups, the
        // HUD and saved state, the mapping is expressed here.
        //
        // MARKED: this assumes the port's own order is the correct one for
        // everything else. If weapon 3 selects the flamethrower in the original,
        // it is this table that is wrong, not the port.
        struct WeaponTables
        {
            const FrameOffset* offsets[4];
            size_t offsetCounts[4];
            const std::vector<uint16_t>* sequences[4];
        };

        inline const WeaponTables& For(int portWeaponIndex)
        {
            static const WeaponTables pistol{
                { PISTOL_IDLE_OFFSETS, PISTOL_FIRING_OFFSETS, PISTOL_RELOAD_OFFSETS, nullptr },
                { 1, 3, 3, 0 },
                { &PISTOL_IDLE_SEQUENCE, &PISTOL_FIRING_SEQUENCE, &PISTOL_RELOAD_SEQUENCE, nullptr } };
            static const WeaponTables shotgun{
                { SHOTGUN_IDLE_OFFSETS, SHOTGUN_FIRING_OFFSETS, nullptr, nullptr },
                { 1, 7, 0, 0 },
                { &SHOTGUN_IDLE_SEQUENCE, &SHOTGUN_FIRING_SEQUENCE, nullptr, nullptr } };
            static const WeaponTables flame{
                { FLAMETHROWER_IDLE_OFFSETS, FLAMETHROWER_FIRING_OFFSETS, FLAMETHROWER_RELOAD_OFFSETS, nullptr },
                { 1, 1, 5, 0 },
                { &FLAMETHROWER_IDLE_SEQUENCE, &FLAMETHROWER_FIRING_SEQUENCE, &FLAMETHROWER_RELOAD_SEQUENCE, nullptr } };
            static const WeaponTables pulse{
                { PULSERIFLE_IDLE_OFFSETS, PULSERIFLE_FIRING_OFFSETS, PULSERIFLE_RELOAD_OFFSETS, PULSERIFLE_GRENADE_OFFSETS },
                { 1, 3, 4, 2 },
                { &PULSERIFLE_IDLE_SEQUENCE, &PULSERIFLE_FIRING_SEQUENCE, &PULSERIFLE_RELOAD_SEQUENCE, &PULSERIFLE_GRENADE_SEQUENCE } };
            static const WeaponTables smart{
                { SMARTGUN_IDLE_OFFSETS, SMARTGUN_FIRING_OFFSETS, SMARTGUN_RELOAD_OFFSETS, nullptr },
                { 1, 4, 4, 0 },
                { &SMARTGUN_IDLE_SEQUENCE, &SMARTGUN_FIRING_SEQUENCE, &SMARTGUN_RELOAD_SEQUENCE, nullptr } };

            switch (portWeaponIndex)
            {
            case 1:  return shotgun;
            case 2:  return pulse;   // canonical order: 2 is the pulse rifle
            case 3:  return flame;   //                  3 is the flamethrower
            case 4:  return smart;
            default: return pistol;
            }
        }
    }
}
