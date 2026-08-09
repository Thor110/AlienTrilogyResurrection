#pragma once

#include "LevelLoader.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // Animated level textures - the consumer of `unknownListA`/`unknownListB`,
    // and the answer to the long-running "flag 8 renders the wrong texture"
    // problem.
    //
    // THE MECHANISM. For a level quad whose draw-routine byte (on-disk +0x12,
    // `ModelQuad::flags`) is 8, `texIndex` is NOT an index into the level's
    // texture descriptor array. It is an ANIMATOR ORDINAL - an index into
    // listA. The face's texture is whatever texIndex that animator is
    // currently outputting.
    //
    // This is why those faces rendered plain dark wall plating: resolved as
    // descriptors, L111's flag-8 texIndex values 0/1/2 land on the wall tiles
    // at page 0 (0,0)/(32,0)/(64,0), which is what was on screen.
    //
    // EVIDENCE (L111LEV.MAP + 111GFX.B16):
    //   - The level declares exactly 10 animators (header `unknownBlockA`).
    //     The 150 flag-8 quads use texIndex 0,1,2,3,4,5,6,9 - every one a
    //     valid animator ordinal, ZERO out of range.
    //   - The animator outputs are the light artwork, confirmed by dumping
    //     the tiles: group 0 alternates descriptors 319/320, a 32x32 red lamp
    //     dim vs bright; groups 1 and 2 alternate 315/316 and 317/318, 32x8
    //     glowing strips; group 3 runs 238 -> 239 -> 240 once and stops, a
    //     control panel going red, red, then BOTH LIGHTS YELLOW.
    //   - Face counts match the artwork's shape: 36 faces on the 32x32 lamp,
    //     72 + 36 on the 32x8 strips - i.e. 36 identical wall fixtures of four
    //     faces each (one lamp, three strips), plus six single control-panel
    //     faces. 36 + 6 = the 42 panels Edward's spatial clustering found
    //     independently, and 144 + 6 = 150.
    //   - listA entries 3..9 all carry listId 3. Seven animator instances
    //     sharing one frame program is exactly what per-face state requires:
    //     each control panel has to change independently when its own switch
    //     is thrown.
    //
    // WHY THE OLD OBJECTION DOESN'T HOLD. It was argued that texIndex cannot
    // be an animator index because every draw loop bounds-checks it against
    // the descriptor count first. The check (Ghidra: FUN_00025648) is
    // `texIndex < DAT_002458e2`, and DAT_002458e2 is 339 for L111. Animator
    // ordinals 0-9 pass it trivially. The check rejects garbage; it says
    // nothing about what the value means to the routine it then dispatches to.
    //
    // STILL A HYPOTHESIS, STRICTLY. The body of draw routine 8 (table slot at
    // 0x000a70b8) has not been decompiled, so the final link - routine 8
    // reading the animator's output - is inferred rather than read. Everything
    // else above is measured. If routine 8 turns out to do something further
    // (a UV transform on top, say), that goes here.
    class TextureAnimator
    {
    public:
        struct Animator
        {
            int16_t listId = -1;      // which frame program; -1 = stopped/unused
            uint8_t variant = 0;      // advanced by ChangeTexture (script command 9)
            uint8_t variantMax = 0;   // program only runs once variant reaches this
            int ip = 0;               // index into this program's frame list
            int ticksRemaining = 0;
            int speed = 1;            // ticks each output frame is held
            bool stopped = false;
            uint16_t currentTex = 0;  // the descriptor index being output
            bool hasOutput = false;   // false until the program emits its first frame
        };

        void Reset(const LevelGeometry& level)
        {
            // listA: {int16 listId, uint8 variant, uint8 variantMax}
            animators.clear();
            animators.reserve(level.unknownListA.size());
            for (uint32_t word : level.unknownListA)
            {
                Animator a;
                a.listId = static_cast<int16_t>(word & 0xffff);
                a.variant = static_cast<uint8_t>((word >> 16) & 0xff);
                a.variantMax = static_cast<uint8_t>((word >> 24) & 0xff);
                animators.push_back(a);
            }

            // listB: {uint16 groupId, uint16 payload}. Payload bit 15 set is a
            // control opcode; clear is an output descriptor index. Entries for
            // a group are contiguous and in program order.
            programs.clear();
            for (uint32_t word : level.unknownListB)
            {
                uint16_t groupId = static_cast<uint16_t>(word & 0xffff);
                uint16_t payload = static_cast<uint16_t>((word >> 16) & 0xffff);
                if (groupId >= programs.size()) { programs.resize(groupId + 1); }
                programs[groupId].push_back(payload);
            }

            // Prime each animator so a face has something to draw on frame one
            // rather than flashing whatever descriptor 0 happens to be.
            for (Animator& a : animators) { Prime(a); }
        }

        // One tick. `randomBits` feeds the random-speed opcodes.
        void Tick(int randomBits)
        {
            for (Animator& a : animators) { TickOne(a, randomBits); }
        }

        // Script command 9 (ChangeTexture). Advances an animator's variant
        // counter with a clamped add, exactly as ToggleLight does to a light
        // record (Ghidra: FUN_00029d44 is the light equivalent). This is what
        // arms a panel that only changes when a switch is thrown.
        void ChangeTexture(size_t animatorIndex, int delta)
        {
            if (animatorIndex >= animators.size()) { return; }
            Animator& a = animators[animatorIndex];
            int value = static_cast<int>(a.variant) + delta;
            if (value < 0) { value = 0; }
            if (value > a.variantMax) { value = a.variantMax; }
            a.variant = static_cast<uint8_t>(value);
        }

        // The descriptor index a flag-8 face using this animator should draw.
        // Returns false when the ordinal is out of range or the animator has
        // produced nothing yet, in which case the caller should fall back to
        // its existing behaviour rather than draw something arbitrary.
        bool CurrentTexture(int animatorOrdinal, uint16_t& outTexIndex) const
        {
            if (animatorOrdinal < 0 || animatorOrdinal >= static_cast<int>(animators.size())) { return false; }
            const Animator& a = animators[static_cast<size_t>(animatorOrdinal)];
            if (!a.hasOutput) { return false; }
            outTexIndex = a.currentTex;
            return true;
        }

        // Snapshot of every animator's current output, for detecting whether a
        // tick actually changed anything worth re-uploading.
        std::vector<uint16_t> OutputSnapshot() const
        {
            std::vector<uint16_t> out;
            out.reserve(animators.size());
            for (const Animator& a : animators) { out.push_back(a.hasOutput ? a.currentTex : 0xffff); }
            return out;
        }

        size_t Count() const { return animators.size(); }
        const std::vector<Animator>& Animators() const { return animators; }

    private:
        // Opcode meanings per Edward's decode of the frame-list format:
        //   1        loop back to the start of the program
        //   2-6      hold each frame for 1/2/3/4/16 ticks
        //   7-10     random hold, (rand & 0x03/0x07/0x0f/0x1f) + 1
        //   11       stop - the program holds its last frame forever
        static int SpeedForOpcode(int opcode, int randomBits)
        {
            switch (opcode)
            {
            case 2: return 1;
            case 3: return 2;
            case 4: return 3;
            case 5: return 4;
            case 6: return 16;
            case 7: return (randomBits & 0x03) + 1;
            case 8: return (randomBits & 0x07) + 1;
            case 9: return (randomBits & 0x0f) + 1;
            case 10: return (randomBits & 0x1f) + 1;
            default: return 1;
            }
        }

        const std::vector<uint16_t>* ProgramFor(const Animator& a) const
        {
            if (a.listId < 0 || a.listId >= static_cast<int>(programs.size())) { return nullptr; }
            const std::vector<uint16_t>& p = programs[static_cast<size_t>(a.listId)];
            return p.empty() ? nullptr : &p;
        }

        // Runs the program forward until it emits its first output frame, so
        // an unarmed animator still shows a sensible starting texture. Opcodes
        // encountered on the way are applied.
        void Prime(Animator& a)
        {
            const std::vector<uint16_t>* program = ProgramFor(a);
            if (!program) { return; }

            for (size_t guard = 0; guard < program->size() * 2 + 4; ++guard)
            {
                if (a.ip < 0 || a.ip >= static_cast<int>(program->size())) { a.ip = 0; }
                uint16_t entry = (*program)[static_cast<size_t>(a.ip)];

                if ((entry & 0x8000) == 0)
                {
                    a.currentTex = entry;
                    a.hasOutput = true;
                    a.ip++;
                    a.ticksRemaining = a.speed;
                    return;
                }

                int opcode = entry & 0x7fff;
                if (opcode == 11) { a.stopped = true; return; }
                if (opcode == 1) { a.ip = 0; continue; }
                a.speed = SpeedForOpcode(opcode, 0);
                a.ip++;
            }
        }

        void TickOne(Animator& a, int randomBits)
        {
            if (a.stopped) { return; }

            // Arming gate: a program with variantMax > 0 does not advance until
            // ChangeTexture has driven its variant up. This is what makes the
            // seven group-3 control panels wait for their own switch.
            if (a.variant < a.variantMax) { return; }

            const std::vector<uint16_t>* program = ProgramFor(a);
            if (!program) { return; }

            if (a.ticksRemaining > 0) { a.ticksRemaining--; return; }

            // Walk opcodes until the next output frame. The guard stops a
            // malformed program (an OP1 loop with no frame in it) spinning.
            for (size_t guard = 0; guard < program->size() * 2 + 4; ++guard)
            {
                if (a.ip < 0 || a.ip >= static_cast<int>(program->size())) { a.ip = 0; }
                uint16_t entry = (*program)[static_cast<size_t>(a.ip)];

                if ((entry & 0x8000) == 0)
                {
                    a.currentTex = entry;
                    a.hasOutput = true;
                    a.ip++;
                    a.ticksRemaining = a.speed;
                    return;
                }

                int opcode = entry & 0x7fff;
                if (opcode == 11) { a.stopped = true; return; }
                if (opcode == 1) { a.ip = 0; continue; }
                a.speed = SpeedForOpcode(opcode, randomBits);
                a.ip++;
            }
        }

        std::vector<Animator> animators;
        std::vector<std::vector<uint16_t>> programs; // indexed by groupId
    };

    // The draw-routine byte value that marks an animated level face. On disk
    // this is quad +0x12, which ModelQuad calls `flags`.
    constexpr uint8_t DRAW_ROUTINE_ANIMATED = 8;
}
