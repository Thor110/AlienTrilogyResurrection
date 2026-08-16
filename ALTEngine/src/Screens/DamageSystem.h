#pragma once

#include <cstdint>

namespace ALTEngine::Screens
{
    // How a projectile hurts something.
    //
    //   FUN_0002a628  projectile vs entity: works out how much, then calls
    //   FUN_000313b0  applies it - the whole damage model in one function
    //   FUN_000310ac  survived: play the hit reaction
    //   FUN_00030b04  died: destroy
    //   FUN_00027b90  spawn: copies health and subtype out of the entity template
    //
    // Entity fields this touches, all offsets from the entity base:
    //   +0x4e  a busy flag; non-zero blocks damage
    //   +0x56  an alternate damage value, used by the special projectile types
    //   +0x58  HEALTH, int16
    //   +0x6c  flags; bit 0x20 raised on death when the killer flag is set
    //   +0x6f  lifecycle state - see EntityState below
    //   +0x74  subtype, which selects the death effect
    namespace Damage
    {
        // The state byte at +0x6f. These are not armour classes, which is what
        // the guards in FUN_000313b0 look like at first glance - they are
        // ANIMATION states, written by the very functions that end a hit:
        // FUN_000310ac sets 6 when a hit reaction starts and FUN_00030b04 sets
        // 7 when the thing dies.
        //
        // So "state > 5 means no damage" is a hit-invulnerability window, not a
        // material property. A crate being knocked about cannot be hurt again
        // until its reaction finishes, which is what stops a burst from a
        // continuous weapon deleting everything in one tick.
        enum EntityState : uint8_t
        {
            STATE_NORMAL = 0,
            STATE_ACTIVE = 5,      // the highest state that still takes damage
            STATE_HIT = 6,         // reacting to a hit - immune
            STATE_DYING = 7,       // immune
            STATE_STATE8 = 8,      // set by FUN_000358a4; untraced, also immune
        };

        // FUN_000313b0's guards, in order.
        inline bool CanBeDamaged(int state, int busyFlag, int subtype)
        {
            if (busyFlag != 0) { return false; }
            if (state == STATE_HIT) { return false; }
            if (state > STATE_ACTIVE) { return false; }
            // One specific pairing is also exempt: subtype 10 in state 4.
            // Untraced why.
            if (subtype == 0xa && state == 4) { return false; }
            return true;
        }

        // How much a hit takes off, from FUN_0002a628.
        //
        // The amount is the projectile's +0x1a field, not its strength (+0x1c).
        //
        // AND +0x1a IS ALSO THE PROJECTILE'S REACH. FUN_0002a448, the clearance
        // test, uses `(*(short *)(p + 0x1a) << 4)` as the distance it probes -
        // the same field, shifted left 4. So a projectile's damage and how far
        // it travels per step are one number: faster rounds hit harder because
        // it is literally the same value. Pistol 0x10 probes 256 units, shotgun
        // 0x50 probes 1280.
        //
        // Strength (+0x1c) only decides whether that number gets doubled:
        //
        //     damage = speed * (strength >= initialStrength / 2 ? 2 : 1)
        //
        // So a round does double damage while it is still fresh and single
        // damage once it has decayed past halfway. A two-step falloff, not a
        // curve.
        inline int HitDamage(int speed, int strength, int initialStrength)
        {
            const bool fresh = (strength >= initialStrength / 2);
            return fresh ? speed * 2 : speed;
        }

        // PROJECTILE RANGE: NOT DERIVED, AND NOT GUESSED HERE.
        //
        // What stops a round in the original is in the update loop, and that
        // loop is the function missing from the export - FUN_0002a628 and
        // FUN_0002ad10 have no callers anywhere in 1244 decompiled functions,
        // so whatever drives them sits in an un-decompiled gap (most likely
        // 0x29d44-0x2a260, immediately before the particle block).
        //
        // The candidate is the strength byte at +0x1c, which is the only
        // per-projectile field with no other job: pistol 0x14, shotgun 0x10,
        // flame 0x1c, heavy 0x1c. If it is a tick countdown then the damage
        // halving at strength < initial/2 is a HALFWAY-THROUGH-FLIGHT rule,
        // which would make it a range falloff and explain why the field exists
        // at all. That reading is consistent but unproven, and the numbers it
        // produces for the flamethrower come out longer-ranged than the pistol,
        // which is wrong - so it is NOT implemented.
        //
        // Until the loop is exported, rounds here fly until they hit something.
        // That is a known departure, stated rather than papered over.

        // Fresh damage per weapon, from the speed fields in
        // Particles::PROJECTILE_*:
        //     pistol  0x10 -> 32     shotgun 0x50 -> 160
        //     flame   0x60 -> 192    heavy   0xa0 -> 320
        inline constexpr int PISTOL_FRESH_DAMAGE = 0x10 * 2;

        // CRATE HEALTH: DERIVED, not read.
        //
        // Three pistol rounds destroy a crate in the original (Edward, 2026).
        // A fresh pistol hit is 32 and death is health <= 0, so two hits must
        // leave it standing and three must not:
        //     64 < health <= 96
        // 96 is the top of that window and a round number. Anything in 65..96
        // reproduces the three shots, so this is right in EFFECT even if the
        // stored value differs.
        //
        // This briefly read 1280, from misreading a report that the PORT was
        // taking about 40 rounds as a statement about the original. It was a
        // bug report. The cause was HIT_REACTION_TICKS below - see the note
        // there - and inflating the health to match hid it instead of fixing it.
        //
        // THE REAL VALUE IS READABLE, BUT NOT FROM THE .MAP. FUN_00027b90 copies
        // health out of the entity's TEMPLATE record - the uint16 at byte offset
        // 0x14 (word index 10), with the subtype byte at 0x0d and the alternate
        // damage field at 0x12 of the same record.
        //
        // CHECKED AGAINST THE FILE, so nobody has to look again: L111LEV.MAP's
        // 44 object records have no health field. Every byte is accounted for -
        // x, y, type, drop, two unknowns, two drop indices, then unknown3 and
        // unknown4 constant at 1, unknown5 through unknown8 and unknown10
        // constant at 0, and rotation. Nothing varies that could carry health,
        // and nothing varies per instance of the same type at all. Object health
        // is therefore per-TYPE and lives in the template, exactly as
        // FUN_00027b90 says.
        //
        // (The port's own Crate layout is confirmed correct by the same pass:
        // all 44 records satisfy its documented invariants - drop in {0,2},
        // unknown2 in {0,10}, rotation even, x/y inside the map - and no other
        // alignment does.)
        //
        // DIFFICULTY: object health almost certainly does not vary with it.
        // Monster records carry BOTH a per-instance health byte and a
        // per-instance difficulty byte (L111 has type 2 at health 1-2 and type 6
        // at health 30, spread across difficulties 0/1/2), so difficulty is
        // expressed by selecting and tuning monster instances. Object records
        // carry neither field, so there is nothing for a difficulty setting to
        // act on short of scaling at spawn.
        inline constexpr int CRATE_HEALTH_DERIVED = 96;

        inline constexpr int BARREL_HEALTH_UNKNOWN = 96; // GUESS - no measurement for barrels

        // What a barrel can be hurt by.
        //
        // BEHAVIOUR IS CONFIRMED, MECHANISM IS NOT. The pistol cannot damage a
        // barrel at all - not slowly, not with enough rounds. The shotgun and
        // everything above it can (Edward, 2026, measured). That is implemented
        // here as a flat rule because the behaviour is certain even though the
        // code path that produces it has not been found.
        //
        // WHERE I HAVE ALREADY LOOKED, so the search is not repeated:
        //   - FUN_000313b0 subtracts its damage argument unconditionally. It
        //     never sees what fired. Its three early-outs are all entity state:
        //     a busy flag at +0x4e, the animation state at +0x6f (6 = reacting
        //     to a hit, 7 = dying), and the one specific pairing of subtype 10
        //     in state 4.
        //   - FUN_0002a628 does branch on the projectile type, but the branch
        //     ADDS damage for types 0x0b/0x0c/0x13 rather than denying it to
        //     anything, and the pistol is type 0.
        //   - The entity template fields that are not yet identified (+0x52,
        //     +0x54, +0x5a, +0x60, +0x62, +0x70, +0x72) are all compared against
        //     fixed constants at spawn or used as table strides. None is tested
        //     against a projectile.
        //
        // WHERE TO LOOK NEXT: FUN_0002a628's box test gates on
        //     (*(uint *)(entity[0x1e] + 6) & CONCAT22(uVar2, local_26)) >> 0x10
        // where uVar2 comes from the PROJECTILE's linked record. That is an AND
        // of two masks with the result tested for zero - the exact shape of a
        // "these two do not interact" rule, and the only such test in the chain.
        // Resolving what entity +0x78 points at, and what the projectile's
        // +0x0c record holds at offset 8, should produce the real rule.
        enum ProjectileClass
        {
            PROJECTILE_CLASS_PISTOL = 0,
            PROJECTILE_CLASS_HEAVIER = 1,
        };

        inline bool BarrelIsVulnerableTo(int projectileType)
        {
            // Particles::TYPE_PISTOL is 0. Everything else - shotgun, flame,
            // pulse, smartgun - gets through.
            return projectileType != 0;
        }

        // The hit test is a PER-AXIS BOX, not a radius, and the box belongs to
        // the target. FUN_0002a628 reads three half-extents from the entity -
        // the high halves at +0x08, +0x0a and +0x0c - and requires the
        // projectile to be inside all three:
        //
        //     |dx| < halfX  &&  |dy| < halfY  &&  |dz| < halfZ
        //
        // AND ALL THREE ARE DOUBLED WHEN THE PROJECTILE TYPE IS 1. That is the
        // shotgun. A shotgun pellet is tested against a box twice the size on
        // every axis, which is how the original makes it forgiving to aim
        // without simulating a spread of pellets. Nothing else gets this.
        //
        // The extent VALUES come from the entity template, same record as the
        // health, and are still unread - so a single radius stands in for all
        // three. This is the last placeholder in the hit path and it goes away
        // with the same dump that fixes the health.
        inline constexpr float HIT_HALF_EXTENT_PLACEHOLDER = 256.0f;

        // Projectile type 1 is the shotgun (Particles::TYPE_SHOTGUN).
        inline constexpr int SHOTGUN_BOX_MULTIPLIER = 2;

        inline float HitHalfExtent(int projectileType)
        {
            return (projectileType == 1)
                 ? HIT_HALF_EXTENT_PLACEHOLDER * SHOTGUN_BOX_MULTIPLIER
                 : HIT_HALF_EXTENT_PLACEHOLDER;
        }

        // What kind of thing is being shot. Only matters for the barrel rule.
        enum TargetKind
        {
            TARGET_CRATE = 0,
            TARGET_BARREL = 1,
        };

        struct Target
        {
            int kind = TARGET_CRATE;
            float x = 0, y = 0, z = 0;
            int health = 0;
            int state = STATE_NORMAL;
            int hitCooldown = 0;   // ticks left in the hit reaction
            bool destroyed = false;
            int placedObjectIndex = -1;
            int cellIndex = -1;        // collision cell this object stamps as occupied
            int crateIndex = -1;       // its record in level.crates, for its contents
        };

        // Length of the hit-reaction window - ZERO for static objects, and this
        // is the fix for the port taking about 40 pistol rounds to break a crate
        // instead of 3.
        //
        // The original's window is however long the reaction ANIMATION runs
        // (FUN_000310ac sets state 6, and the state clears when the animation
        // ends). A crate has no hit animation, so its window is nothing. I had
        // invented a flat 6 ticks and applied it to everything, which meant a
        // round arriving inside that window was silently swallowed - it stopped
        // and shattered against the crate, looking like a hit, and did no
        // damage. With the pistol's own fire cycle also 6 ticks, the two beat
        // against each other and most shots landed inside the window.
        //
        // Left as a named constant rather than deleted because animate targets
        // DO have this window and enemies will need it - it just has to come
        // from a real animation length, not a number I picked.
        inline constexpr int HIT_REACTION_TICKS = 0;

        // Returns true if this hit destroyed the target. `projectileType` is
        // the Particles type byte, needed only for the barrel rule.
        inline bool ApplyHit(Target& target, int damage, int projectileType)
        {
            if (target.destroyed) { return false; }
            if (target.kind == TARGET_BARREL && !BarrelIsVulnerableTo(projectileType)) { return false; }
            if (!CanBeDamaged(target.state, target.hitCooldown, 0)) { return false; }

            target.health -= damage;
            if (target.health > 0)
            {
                // Survived: FUN_000310ac, which sets state 6 for as long as the
                // reaction animation runs. A target with NO reaction never
                // enters that state at all - entering it with a zero-length
                // window would latch it there permanently, since nothing would
                // ever tick it back out.
                if (HIT_REACTION_TICKS > 0)
                {
                    target.state = STATE_HIT;
                    target.hitCooldown = HIT_REACTION_TICKS;
                }
                return false;
            }

            // Died: FUN_00030b04, which sets state 7 and subtype 6.
            target.state = STATE_DYING;
            target.destroyed = true;
            return true;
        }

        // THE BLAST, from FUN_000368c8.
        //
        // It is not a radius. The explosion walks the CELL GRID outward from the
        // cell it went off in, in two rings: first 8 cells from an offset table
        // at DAT_000acb00, then 16 more from DAT_000acb20. Each candidate cell
        // is kept only if:
        //   - its attribute (+10) is 0x13 or less - anything above is a wall
        //   - its floor height (+0xb) is within 0xc of the origin cell's
        //   - it is not attribute 6 with less than 8 units of headroom
        // and a second-ring cell is discarded along with its first-ring parent,
        // so the blast cannot reach round a corner it could not reach through.
        //
        // What it then does to each surviving cell, by the occupancy byte (+0xc):
        //   0xFF        the player: FUN_0003e5a8(0x32, 5) - a flat 50
        //   0x01-0x0F   a monster: damaged by the ENTITY'S OWN +0x56 value, not
        //               by any blast figure
        //   above 0x0F  an object: DESTROYED OUTRIGHT, no health check at all
        //
        // That last one is the chain reaction. A barrel inside another barrel's
        // blast is simply removed, which fires its own blast, and so on.
        inline constexpr int BLAST_PLAYER_DAMAGE = 0x32;
        inline constexpr int BLAST_MAX_ATTRIBUTE = 0x13;
        inline constexpr int BLAST_MAX_HEIGHT_STEP = 0xc;

        // RING OFFSETS. Ring 1 is read from 0x000acb00; ring 2 is DERIVED, and
        // the derivation is checked rather than assumed.
        //
        // Ring 1 is the 8 neighbours in CLOCKWISE order starting at the top-left
        // corner - not the row-major order that seems natural, which is what was
        // guessed here before and which put the wrong parent on five of the
        // sixteen outer cells.
        //
        // Ring 2's own table at 0x000acb20 has not been dumped, but it does not
        // need to be. FUN_000368c8 clears each outer slot alongside its parent,
        // and those groupings are visible in the code: ring1[0] carries outer
        // 0/1/15, ring1[1] carries 2, ring1[2] carries 3/4/5, ring1[5] carries
        // 10, ring1[6] carries 11/12/13, ring1[7] carries 14. Corners carry three
        // outer cells and edges carry one. Only one 16-cell clockwise ring
        // starting at (-2,-2) satisfies all of that, and in it every outer cell
        // is adjacent to its stated parent - checked, 16 of 16.
        struct CellOffset { int dx; int dz; };

        inline constexpr CellOffset BLAST_RING1[8] = {
            { -1, -1 },  // 0 corner
            {  0, -1 },  // 1 edge
            {  1, -1 },  // 2 corner
            {  1,  0 },  // 3 edge
            {  1,  1 },  // 4 corner
            {  0,  1 },  // 5 edge
            { -1,  1 },  // 6 corner
            { -1,  0 },  // 7 edge
        };

        // Each outer cell with the inner cell the blast must pass through.
        struct RingTwoCell { CellOffset offset; int parent; };
        inline constexpr RingTwoCell BLAST_RING2[16] = {
            { { -2, -2 }, 0 }, { { -1, -2 }, 0 }, { {  0, -2 }, 1 }, { {  1, -2 }, 2 },
            { {  2, -2 }, 2 }, { {  2, -1 }, 2 }, { {  2,  0 }, 3 }, { {  2,  1 }, 4 },
            { {  2,  2 }, 4 }, { {  1,  2 }, 4 }, { {  0,  2 }, 5 }, { { -1,  2 }, 6 },
            { { -2,  2 }, 6 }, { { -2,  1 }, 6 }, { { -2,  0 }, 7 }, { { -2, -1 }, 0 },
        };

        inline void TickTarget(Target& target)
        {
            if (target.hitCooldown > 0 && --target.hitCooldown == 0)
            {
                if (!target.destroyed) { target.state = STATE_NORMAL; }
            }
        }
    }
}
