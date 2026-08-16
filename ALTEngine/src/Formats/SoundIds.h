#pragma once

namespace ALTEngine::Formats
{
    // Sound ids, and how the original resolves one to a sample. RESOLVED.
    //
    // HOW AN ID IS USED. The three play entry points (FUN_00040ba0,
    // FUN_00040b2c, FUN_00040c38) write the id into a request block and pass it
    // to FUN_0004566c / FUN_00045650 / FUN_00045688, each of which does exactly
    // one thing: FUN_0005b0a6(*param_1 + 1, ...). So the id enters the driver as
    // `id + 1` - a zero-based index into a one-based sample bank. Nothing above
    // the driver ever touches a filename.
    //
    // WHAT THE BANK IS. The .TXT files in the SFX folder. 111SFX.TXT is one line
    // per slot, in order, with blank.raw padding the slots that level does not
    // use - so THE SOUND ID IS THE LINE NUMBER, zero-based. The "111" is the
    // level code, and each level ships its own file, which is how the same id
    // can be a different enemy's cry on different levels.
    //
    // This was checked, not assumed. Three ids the decompilation uses land on
    // files whose names state their purpose, and no other indexing does:
    //     0x23/0x24/0x25 -> 5003brik / 5003metl / 5003watr
    //                       three material variants of one slot, and the code
    //                       picks the third on the cell attribute that also
    //                       silences footsteps. Water.
    //     0x26           -> 5004astp, "a step"
    //     0x0b           -> 0503-fla, which NEWSFX.BAT shows is a copy of
    //                       flamloop.raw - and 0x0b is the one id FUN_0003e93c
    //                       plays without a positional call, exactly what a
    //                       looping flamethrower needs.
    //
    // A CORRECTION THIS FORCED. The two footstep ids were labelled the wrong way
    // round here. 0x26 is 5004astp and IS the footstep; 0x2c is 0204ripl, a
    // Ripley vocalisation, and it is the one that gets the positional call. So
    // the branch in FUN_0003d00c is not "footstep, or a different footstep on
    // damage" - it is "footstep, or Ripley reacting". Which way the 0x40 flag
    // runs is still open, but the FILES settle which sound is which.
    namespace SoundIds
    {
        // ---- ids the decompilation actually uses ----
        inline constexpr int MENU_SELECT = 0x00;        // 0101sele
        inline constexpr int FLAME_LOOP = 0x0b;         // 0503-fla, played non-positionally
        inline constexpr int ENEMY_PROXIMITY = 0x1e;    // enemyprf, on a subtype-2 death
        inline constexpr int GRENADE_ALT = 0x20;        // 0402gren, subtype 7 at <=0x19 health
        inline constexpr int RIPLEY_VOCAL_04 = 0x04;    // 0203ripl, subtype 7 one time in eight

        inline constexpr int IMPACT_BRICK = 0x23;       // 5003brik, levels 0x0c-0x15
        inline constexpr int IMPACT_METAL = 0x24;       // 5003metl, every other level
        inline constexpr int IMPACT_WATER = 0x25;       // 5003watr, cell attribute 8 or 9

        // 0x2c is the footstep - it varies by episode (0204/0206/0208ripl) and
        // NEWSFX.BAT builds it from shipft*.raw, "ship footstep". 0x26 is
        // 5004astp, identical on all 45 levels, played only when the 0x40 flag
        // is set. See PlayerCamera.h.
        inline constexpr int FOOTSTEP = 0x2c;
        inline constexpr int FOOTSTEP_FLAGGED = 0x26;

        // WEAPON FIRE. Every weapon has three slots - xx01, xx02 and xx05CLIP -
        // and NEWSFX.BAT retunes the xx02 of all four it touches: newhand4 onto
        // 0602hand, newshot2 onto 0702shot, prfl2b_1 onto 0302puls, smart1 onto
        // 0902smar. Those replacement names are the weapon reports, so xx02 is
        // the firing sound. Four for four, but it is a PATTERN rather than a
        // traced call site - if the pistol sounds wrong, try the xx01 slot one
        // lower.
        inline constexpr int PISTOL_SHOT = 0x0d;        // 0602hand
        inline constexpr int SHOTGUN_SHOT = 0x10;       // 0702shot
        inline constexpr int PULSE_SHOT = 0x06;         // 0302puls
        inline constexpr int SMARTGUN_SHOT = 0x13;      // 0902smar
        inline constexpr int FLAME_SHOT = 0x0b;         // 0503-fla, the loop

        inline constexpr int PISTOL_CLIP = 0x0e;        // 0605clip
        inline constexpr int SHOTGUN_CLIP = 0x11;       // 0705clip
        inline constexpr int SMARTGUN_CLIP = 0x14;      // 0905clip

        // Fire and reload sounds by weapon index, matching PlayerHudState's
        // ordering: pistol, shotgun, flamethrower, pulse rifle, smartgun.
        inline constexpr int WEAPON_SHOT[] = { PISTOL_SHOT, SHOTGUN_SHOT, FLAME_SHOT, PULSE_SHOT, SMARTGUN_SHOT };
        inline constexpr int WEAPON_CLIP[] = { PISTOL_CLIP, SHOTGUN_CLIP, -1, -1, SMARTGUN_CLIP };

        // Named slots worth having even though no traced call site uses them yet.
        inline constexpr int PULSE_FIRE = 0x05;         // 0301puls
        inline constexpr int GRENADE_FIRE = 0x07;       // 0401gren
        inline constexpr int FLAME_FIRE = 0x0a;         // 0501flam
        inline constexpr int PISTOL_FIRE = 0x0c;        // 0601hand
        inline constexpr int PISTOL_ALT = 0x0d;         // 0602hand
        inline constexpr int PISTOL_RELOAD = 0x0e;      // 0605clip
        inline constexpr int SHOTGUN_FIRE = 0x0f;       // 0701shot
        inline constexpr int SHOTGUN_ALT = 0x10;        // 0702shot
        inline constexpr int SHOTGUN_RELOAD = 0x11;     // 0705clip
        inline constexpr int SMARTGUN_FIRE = 0x12;      // 0901smar
        inline constexpr int SMARTGUN_RELOAD = 0x14;    // 0905clip
        inline constexpr int MOTION_TRACKER = 0x15;     // 1002moti
        inline constexpr int BARREL_EXPLODE = 0x21;     // 5001barr
        inline constexpr int SEISMIC = 0x22;            // 5002seis
        inline constexpr int TEXT_BEEP = 0x27;          // 5005text
        inline constexpr int PICKUP = 0x28;             // pickup
        inline constexpr int CRATE_BREAK = 0x29;        // 4101crat
        inline constexpr int DOOR_OPEN = 0x2a;          // 3101open
        inline constexpr int DOOR_THUD = 0x2b;          // 3103thud

        // The enemy slots. Blank on this level where unused, filled per level
        // with whatever that level's roster needs, in two blocks:
        //     0x30-0x35   1601lnch 1602flor 1603face 1604skit 1605dies 1606inja
        //     0x43-0x49   1801bite 1902clwc 1903atck 1904dies 1905hisa 1908inja
        //                 1805whip
        //     0x5a, 0x5c  2701powr, 2703powb
        // The mnemonics repeat per group - lnch/flor/face/skit/dies/inja for
        // one type, bite/clw/atck/dies/his/inja/whip for another - so the block
        // is a fixed per-enemy layout rather than an arbitrary list.
        inline constexpr int ENEMY_BLOCK_A = 0x30;
        inline constexpr int ENEMY_BLOCK_B = 0x43;

        // The slot table, read from 111SFX.TXT line by line.
        // Slot 0x6e is the last real entry; unlisted slots are blank.raw.
        inline constexpr const char* SLOT_TABLE[] = {
            /* 0x00 */ "0101sele",
            /* 0x01 */ "0102sele",
            /* 0x02 */ "0201ripl",
            /* 0x03 */ "0202ripl",
            /* 0x04 */ "0203ripl",
            /* 0x05 */ "0301puls",
            /* 0x06 */ "0302puls",
            /* 0x07 */ "0401gren",
            /* 0x08 */ "0402gren",
            /* 0x09 */ "0405gren",
            /* 0x0a */ "0501flam",
            /* 0x0b */ "0503-fla",
            /* 0x0c */ "0601hand",
            /* 0x0d */ "0602hand",
            /* 0x0e */ "0605clip",
            /* 0x0f */ "0701shot",
            /* 0x10 */ "0702shot",
            /* 0x11 */ "0705clip",
            /* 0x12 */ "0901smar",
            /* 0x13 */ "0902smar",
            /* 0x14 */ "0905clip",
            /* 0x15 */ "1002moti",
            /* 0x16 */ "1003moti",
            /* 0x17 */ "1201-nit",
            /* 0x18 */ "1202-nit",
            /* 0x19 */ "1203-nit",
            /* 0x1a */ "1301auto",
            /* 0x1b */ "1302auto",
            /* 0x1c */ "0503-fla",
            /* 0x1d */ "0602hand",
            /* 0x1e */ "enemyprf",
            /* 0x1f */ "smart3a2",
            /* 0x20 */ "0402gren",
            /* 0x21 */ "5001barr",
            /* 0x22 */ "5002seis",
            /* 0x23 */ "5003brik",
            /* 0x24 */ "5003metl",
            /* 0x25 */ "5003watr",
            /* 0x26 */ "5004astp",
            /* 0x27 */ "5005text",
            /* 0x28 */ "pickup",
            /* 0x29 */ "4101crat",
            /* 0x2a */ "3101open",
            /* 0x2b */ "3103thud",
            /* 0x2c */ "0204ripl",
            /* 0x2d */ "blank",
            /* 0x2e */ "blank",
            /* 0x2f */ "blank",
            /* 0x30 */ "1601lnch",
            /* 0x31 */ "1602flor",
            /* 0x32 */ "1603face",
            /* 0x33 */ "1604skit",
            /* 0x34 */ "1605dies",
            /* 0x35 */ "1606inja",
            /* 0x36 */ "blank",
            /* 0x37 */ "blank",
            /* 0x38 */ "blank",
            /* 0x39 */ "blank",
            /* 0x3a */ "blank",
            /* 0x3b */ "blank",
            /* 0x3c */ "blank",
            /* 0x3d */ "blank",
            /* 0x3e */ "blank",
            /* 0x3f */ "blank",
            /* 0x40 */ "blank",
            /* 0x41 */ "blank",
            /* 0x42 */ "blank",
            /* 0x43 */ "1801bite",
            /* 0x44 */ "1902clwc",
            /* 0x45 */ "1903atck",
            /* 0x46 */ "1904dies",
            /* 0x47 */ "1905hisa",
            /* 0x48 */ "1908inja",
            /* 0x49 */ "1805whip",
            /* 0x4a */ "blank",
            /* 0x4b */ "blank",
            /* 0x4c */ "blank",
            /* 0x4d */ "blank",
            /* 0x4e */ "blank",
            /* 0x4f */ "blank",
            /* 0x50 */ "blank",
            /* 0x51 */ "blank",
            /* 0x52 */ "blank",
            /* 0x53 */ "blank",
            /* 0x54 */ "blank",
            /* 0x55 */ "blank",
            /* 0x56 */ "blank",
            /* 0x57 */ "blank",
            /* 0x58 */ "blank",
            /* 0x59 */ "blank",
            /* 0x5a */ "2701powr",
            /* 0x5b */ "blank",
            /* 0x5c */ "2703powb",
            /* 0x5d */ "blank",
            /* 0x5e */ "blank",
            /* 0x5f */ "blank",
            /* 0x60 */ "blank",
            /* 0x61 */ "blank",
            /* 0x62 */ "blank",
            /* 0x63 */ "blank",
            /* 0x64 */ "blank",
            /* 0x65 */ "blank",
            /* 0x66 */ "blank",
            /* 0x67 */ "blank",
            /* 0x68 */ "blank",
            /* 0x69 */ "blank",
            /* 0x6a */ "blank",
            /* 0x6b */ "blank",
            /* 0x6c */ "blank",
            /* 0x6d */ "blank",
            /* 0x6e */ "blank",
        };
        // SPCH.TXT is the same shape for speech: 35 lines, VC01C through VC43,
        // so a speech id is a line number in that file rather than in this one.
        // It is a separate bank, not an extension of this table.
        inline constexpr int SPEECH_LINE_COUNT = 35;
    }
}
