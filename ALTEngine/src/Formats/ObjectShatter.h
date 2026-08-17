#pragma once

#include <cstdint>

namespace ALTEngine::Formats
{
    // How a crate or barrel comes apart.
    //
    // CONFIRMED, from the listing. This is not the flat effect sprites - those
    // are a separate thing that plays alongside. A destroyed object is torn into
    // TEXTURED QUADS taken from its own mesh, which then spin as they fly.
    //
    // THE CHAIN:
    //   FUN_00046ffc   walks the object's face list and shatters each face
    //   FUN_000464ec   turns ONE face into FOUR particles of type 6
    //   FUN_0002d054   draws a type-6 particle: the face's own quad, with its own
    //                  light record and texture index, under a full 3D transform
    //
    // FUN_00046ffc's walk is the same face-list shape the level geometry uses:
    // stride 0x14, with bit 0x80 of byte +0x12 marking the last face - exactly
    // what FUN_00025648 does for level faces. It also steps 0x28 instead of 0x14
    // when bit 0x80 of +0x26 is clear, so it skips a face in some cases.
    //
    // FUN_000464ec gates on the face's DRAW ROUTINE byte (+0x13): only routines
    // 0 and 2 shatter. Faces drawn by any other routine are left out, so an
    // object does not shed its special-cased surfaces.
    //
    // FOUR SHARDS PER FACE, allocated one at a time from the same particle pool
    // everything else uses (FUN_0002aa3c), and the whole face is abandoned if any
    // of the four cannot be allocated - the already-taken ones are handed back
    // (FUN_0002a9b4). So a busy scene degrades by dropping whole faces rather
    // than leaving half-shattered geometry.
    inline constexpr int SHARDS_PER_FACE = 4;

    // The face-list walk, matching FUN_00025648's.
    inline constexpr int FACE_STRIDE = 0x14;
    inline constexpr int FACE_STRIDE_ALT = 0x28;
    inline constexpr int FACE_LAST_FLAG = 0x80;    // bit 0x80 of byte +0x12
    inline constexpr int FACE_DRAW_ROUTINE = 0x13; // only routines 0 and 2 shatter

    // HOW A FACE IS CUT. Read from FUN_000464ec's body, not inferred.
    //
    // param_2 is the face and param_2[0..3] are its FOUR vertex pointers. It
    // takes the midpoint of each edge (FUN_0004dc20 at 0x800 is a halfway lerp)
    // and the centre of the face, then builds four pieces each of which is
    //     { a corner, an edge midpoint, the centre, the other edge midpoint }
    //
    // So a face is cut into FOUR QUADS, not triangles - the ordinary
    // subdivision of a quad. If a crate appears to break into triangles, that
    // is because some of its faces are already triangular in the model, not
    // because the shatter makes triangles.
    //
    // The centre is jittered by +/-0x80 on each axis before the cut, so the four
    // pieces are not identical quarters.
    inline constexpr int FACE_CENTRE_JITTER = 0x80;

    // LAUNCH VELOCITY. This is what the 0x28/0x20 per-object value actually is -
    // it is the UPWARD launch speed, not a size:
    //     vx = random(0..0x1f) - 0x10
    //     vy = launchSpeed + random(0..0x1f)
    //     vz = random(0..0x1f) - 0x10
    // so pieces go UP with a small sideways spread, and fall back. They are not
    // thrown radially outward from the object's centre.
    inline constexpr int LAUNCH_SPEED_BARREL = 0x28;
    inline constexpr int LAUNCH_SPEED_DEFAULT = 0x20;
    inline constexpr int LAUNCH_TYPE_BARREL = 0x17;   // 23 - the barrel
    inline constexpr int LAUNCH_SPREAD = 0x1f;
    inline constexpr int LAUNCH_SPREAD_CENTRE = 0x10;

    inline constexpr int LaunchSpeed(int objectType)
    {
        return (objectType == LAUNCH_TYPE_BARREL) ? LAUNCH_SPEED_BARREL : LAUNCH_SPEED_DEFAULT;
    }

    // SIZE is separate, and it is random per shard: the byte at +0x1c is
    // (random & 0xf) + 0x20, and FUN_0002d054 applies it as (byte << 6) on all
    // three axes. So every piece is a slightly different size regardless of what
    // it came off.
    inline constexpr int SHARD_SCALE_BASE = 0x20;
    inline constexpr int SHARD_SCALE_RANDOM = 0x0f;
    inline constexpr int SHARD_SCALE_SHIFT = 6;

    // SPIN IS ABOUT ONE AXIS ONLY, and this matters to how it looks.
    //
    // FUN_000464ec writes the three rotation shorts as:
    //     +0x10 = 0                      always
    //     +0x12 = the object's own field  fixed
    //     +0x14 = random & 0xfff          the only one that moves
    // and FUN_0002d054 adds 0x40 to +0x14 alone on every draw.
    //
    // So a piece turns in its own plane like a spinning plate - it does not
    // tumble. Spinning all three axes, which is the obvious thing to write,
    // looks quite different and is wrong.
    inline constexpr int SHARD_SPIN_PER_FRAME = 0x40;

    // Draw distance for a shard, from FUN_0002d054's own gate. Tighter than the
    // flat effects': they vanish beyond 0x2000 rather than 0x1e00, and there is
    // a near cut at 0x200.
    inline constexpr int SHARD_NEAR_CUTOFF = 0x200;
    inline constexpr int SHARD_FAR_CUTOFF = 0x2000;

    // Type 0xf is the same idea one level up: a WHOLE MODEL thrown as a single
    // particle rather than a face. FUN_00045808 spawns those, from FUN_00045fe4,
    // and FUN_0002d054 draws them through a different routine table entry
    // (0xa7154 rather than 0xa7134). Not used by crates or barrels.
    inline constexpr uint8_t PARTICLE_TYPE_FACE_SHARD = 6;
    inline constexpr uint8_t PARTICLE_TYPE_WHOLE_MODEL = 0xf;
}
