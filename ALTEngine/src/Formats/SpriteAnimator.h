#pragma once

#include <cstdint>
#include <vector>

namespace ALTEngine::Formats
{
    // The original's generic sprite animator - a tiny bytecode VM that every
    // animated sprite runs on, weapons and enemies alike.
    //
    //   FUN_00028a6c  the per-tick step: count down, advance, refresh the frame
    //   FUN_000288e0  the opcode interpreter the step calls
    //
    // A SEQUENCE is an array of uint16. An entry with bit 0x8000 CLEAR is a
    // frame index and stops interpretation for this tick. An entry with 0x8000
    // SET is an opcode: bits 8..14 are the opcode, bits 0..7 the operand. So a
    // sequence is a flat list of frames with control codes interleaved, and the
    // interpreter runs codes until it lands on a frame.
    //
    // Frames themselves are 12-byte records: the original computes
    // `record = frameTableBase + frameIndex * 0xc` (FUN_00028a6c). Only the
    // index is kept here; what a record contains is a separate question and
    // does not affect the VM.
    namespace SpriteAnim
    {
        // Opcodes, read out of FUN_000288e0's dispatch. Names describe what the
        // code DOES, which is traced; what each one is FOR is mostly not.
        enum Opcode : uint8_t
        {
            // Stores the operand in `param` and raises FLAG_EVENT for this
            // tick.
            //
            // CONFIRMED: the operand is a SOUND ID. FUN_0003e93c tests the
            // weapon animator's flags for 0x10 and passes `param` straight to
            // FUN_00040b2c(id, -1), then to FUN_00052b28(id, playerX >> 16) for
            // the positional call - the same pair the footstep code uses with
            // sound 0x2c. Id 0xb is special-cased and skips the positional call.
            //
            // So an animation sequence carries its own sound cues inline, which
            // is how a weapon's report lands on the right frame.
            OP_EVENT = 1,

            // Loop back to `loopStart`. First time through, the operand seeds
            // the counter; after that the counter decrements and the animation
            // ends when it reaches zero. An operand of 0 seeds zero every time
            // and so loops forever, which is how a held pose is expressed.
            OP_LOOP = 2,

            OP_SET_FLAG1 = 3,     // raises FLAG_USER1
            OP_CLEAR_FLAG1 = 4,   // clears FLAG_USER1
            OP_NOP_5 = 5,         // advances one entry, nothing else
            OP_END = 6,           // stops; raises FLAG_ENDED
            OP_NOP_7 = 7,         // advances one entry, nothing else
            OP_END_ALT = 8,       // stops; raises FLAG_ENDED_ALT
            OP_SET_FLAG8 = 9,     // raises FLAG_USER8
        };

        // Status flags, the original's byte at +0x36.
        enum Flags : uint8_t
        {
            FLAG_USER1 = 0x01,       // set/cleared by opcodes 3 and 4
            FLAG_ENDED = 0x02,       // opcode 6, or a loop counter running out
            FLAG_ENDED_ALT = 0x04,   // opcode 8
            // NOT the entity status flag of the same number. This byte is the
            // ANIMATOR's, at +0x36 of the animator struct - which for an entity
            // sits at +0x80, so at entity +0xb6. The entity's own flags byte is
            // at +0x6c and its bit 8 is the ceiling-hang flag. Unrelated.
            FLAG_USER8 = 0x08,       // opcode 9
            FLAG_EVENT = 0x10,       // opcode 1 fired THIS tick; cleared every tick

            // Either end flag freezes the displayed frame - FUN_00028a6c tests
            // `(flags & 6) == 0` before refreshing it.
            FLAG_ENDED_MASK = FLAG_ENDED | FLAG_ENDED_ALT,
        };

        inline constexpr uint16_t Op(Opcode opcode, uint8_t operand = 0)
        {
            return static_cast<uint16_t>(0x8000u | (static_cast<uint16_t>(opcode) << 8) | operand);
        }

        inline constexpr bool IsOpcode(uint16_t entry) { return (entry & 0x8000u) != 0; }

        // One animator instance. Field comments give the original's offset from
        // the instance base, so this can be checked against the decompilation
        // directly (the player's weapon instance lives at DAT_000b0a68).
        struct Animator
        {
            uint16_t frameTimer = 0;      // +0x30, ticks until the next advance
            uint16_t frameDuration = 0;   // +0x32, reload value for the above
            size_t pc = 0;                // +0x18, index into the sequence
            size_t loopStart = 0;         // +0x1c, where OP_LOOP jumps back to
            uint16_t loopCounter = 0;     // +0x3a
            uint8_t flags = 0;            // +0x36
            uint16_t frameIndex = 0;      // +0x3c, the frame to draw
            uint16_t param = 0;           // +0x38, OP_EVENT's operand

            bool Ended() const { return (flags & FLAG_ENDED_MASK) != 0; }
            bool EventFired() const { return (flags & FLAG_EVENT) != 0; }
        };

        // Starts a sequence, exactly as FUN_000400fc sets the player's weapon
        // animator up.
        //
        // NOTE THE LAYOUT, which is not obvious: entry 0 is the frame DURATION,
        // the program counter starts at 1, and the loop target starts at 2. The
        // timer starts at 1, so the very first tick advances immediately -
        // which means entry 1 is never displayed and the first frame the player
        // actually sees is entry 2. That is why the loop target is 2 and not 1.
        // Transcribed rather than tidied; a sequence built for this layout needs
        // a filler at index 1.
        inline void Start(Animator& animator, const std::vector<uint16_t>& sequence)
        {
            animator = Animator{};
            if (sequence.empty()) { return; }
            animator.frameDuration = sequence[0];
            animator.pc = 1;
            animator.loopStart = 2;
            animator.frameTimer = 1;
        }

        // FUN_000288e0. Runs control codes until it reaches a frame entry or an
        // opcode stops it.
        inline void RunOpcodes(Animator& animator, const std::vector<uint16_t>& sequence)
        {
            bool stop = false;
            animator.flags = static_cast<uint8_t>(animator.flags & ~FLAG_EVENT);

            // The original has no bound here - it trusts the sequence to be well
            // formed. A malformed one would spin forever, so this caps it; the
            // cap is ours, and it can only be reached by data the original would
            // itself have hung on.
            for (int guard = 0; guard < 4096; ++guard)
            {
                if (animator.pc >= sequence.size()) { return; }
                const uint16_t entry = sequence[animator.pc];
                if (!IsOpcode(entry) || stop) { return; }

                const uint8_t opcode = static_cast<uint8_t>((entry >> 8) & 0x7f);
                const uint8_t operand = static_cast<uint8_t>(entry & 0xff);

                switch (opcode)
                {
                case OP_EVENT:
                    animator.param = operand;
                    animator.flags |= FLAG_EVENT;
                    animator.pc++;
                    break;

                case OP_LOOP:
                    if (animator.loopCounter == 0)
                    {
                        animator.loopCounter = operand;
                        animator.pc = animator.loopStart;
                    }
                    else
                    {
                        animator.loopCounter--;
                        if (animator.loopCounter == 0)
                        {
                            stop = true;
                            animator.flags |= FLAG_ENDED;
                        }
                        else
                        {
                            animator.pc = animator.loopStart;
                        }
                    }
                    break;

                case OP_SET_FLAG1:
                    animator.flags |= FLAG_USER1;
                    animator.pc++;
                    break;

                case OP_CLEAR_FLAG1:
                    animator.flags = static_cast<uint8_t>(animator.flags & ~FLAG_USER1);
                    animator.pc++;
                    break;

                case OP_END:
                    stop = true;
                    animator.flags |= FLAG_ENDED;
                    break;

                case OP_END_ALT:
                    stop = true;
                    animator.flags |= FLAG_ENDED_ALT;
                    break;

                case OP_SET_FLAG8:
                    animator.flags |= FLAG_USER8;
                    animator.pc++;
                    break;

                // Opcodes 5 and 7, and anything unrecognised, just step over.
                // The original's default arm does exactly this.
                default:
                    animator.pc++;
                    break;
                }
            }
        }

        // FUN_00028a6c. One tick. Returns true on a tick where the displayed
        // frame changed, so callers can skip redundant work.
        inline bool Tick(Animator& animator, const std::vector<uint16_t>& sequence)
        {
            // FLAG_EVENT is a ONE-TICK PULSE. RunOpcodes raises it and clears it
            // on its own entry, but it only runs on the ticks where the frame
            // timer expires - so on a sequence with a duration above 1 the flag
            // would still be standing on the ticks in between, and a caller
            // polling it once per tick would fire the sound repeatedly. The
            // pistol's report played twice before this, once per tick of its
            // two-tick first frame.
            animator.flags = static_cast<uint8_t>(animator.flags & ~FLAG_EVENT);

            if (sequence.empty()) { return false; }
            if (animator.frameTimer == 0) { return false; }

            animator.frameTimer--;
            if (animator.frameTimer != 0) { return false; }

            animator.frameTimer = animator.frameDuration;
            animator.pc++;
            RunOpcodes(animator, sequence);

            // Either end flag freezes the frame where it is.
            if ((animator.flags & FLAG_ENDED_MASK) != 0) { return false; }
            if (animator.pc >= sequence.size()) { return false; }

            const uint16_t previous = animator.frameIndex;
            animator.frameIndex = sequence[animator.pc];
            return animator.frameIndex != previous;
        }

        // Builds a sequence for this layout: duration, the skipped filler at
        // index 1, then the frames, then a terminator.
        //
        // `loopForever` uses OP_LOOP with operand 0, which reseeds a zero
        // counter every time and so never terminates - the idiom for a held
        // pose. Otherwise the sequence ends with OP_END and the animation
        // freezes on its last frame with FLAG_ENDED raised.
        inline std::vector<uint16_t> BuildSequence(uint16_t frameDuration,
                                                   const std::vector<uint16_t>& frames,
                                                   bool loopForever)
        {
            std::vector<uint16_t> out;
            out.reserve(frames.size() + 3);
            out.push_back(frameDuration);
            out.push_back(frames.empty() ? 0 : frames.front()); // index 1, never displayed
            for (uint16_t f : frames) { out.push_back(f); }
            out.push_back(loopForever ? Op(OP_LOOP, 0) : Op(OP_END));
            return out;
        }
    }
}
