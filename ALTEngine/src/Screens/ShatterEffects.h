#pragma once

#include "../Formats/ObjectShatter.h"
#include "../Formats/SpriteAnimator.h"
#include "../Renderer/ModelRenderer.h"
#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // Crates and barrels coming apart into pieces of their own mesh.
    //
    // See Formats/ObjectShatter.h for the trace. In short: FUN_00046ffc walks a
    // destroyed object's face list, FUN_000464ec turns each face into FOUR
    // type-6 particles, and FUN_0002d054 draws each one as the face's own quad -
    // its texture, its light record - under a full 3D rotation that advances
    // 0x40 every frame.
    //
    // So a shard is real geometry wearing the object's artwork, which is why
    // flat effect sprites never looked like a crate breaking: they are a
    // different system that plays alongside this one.
    class ShatterEffects
    {
    public:
        struct Shard
        {
            float x = 0, y = 0, z = 0;
            float vx = 0, vy = 0, vz = 0;
            // The quarter's full size; it is drawn at life / 64 of this.
            float halfWidth = 16.0f;
            float halfHeight = 16.0f;
            // +0x18: the speed of the next bounce, 8 less each time.
            int bounce = 0;
            float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
            int angleX = 0, angleY = 0, angleZ = 0;
            int spinX = 0, spinY = 0, spinZ = 0;
            int life = 0;
            std::string sheetKey;
            bool alive = false;
        };

        // Tears an object apart. `faces` is the model's own face list and
        // `sheetKey` the sprite sheet its texture was uploaded under - both from
        // ModelRenderer.
        //
        // `scale` is the original's per-object shard size: 0x28 for a barrel,
        // 0x20 for everything else (Formats::ShardScale). The original applies
        // it as (value << 6) on all three axes of the face; here it scales the
        // face's own half-extents, so a shard stays proportional to the piece it
        // came from.
        void Shatter(const std::vector<ALTEngine::Renderer::ModelRenderer::ModelFace>& faces,
                     const std::string& sheetKey,
                     float originX, float originY, float originZ,
                     float objectScaleX, float objectScaleY, float objectScaleZ,
                     int objectType, int objectFacing)
        {
            namespace OS = ALTEngine::Formats;
            const float launchSpeed = static_cast<float>(OS::LaunchSpeed(objectType));

            for (const auto& face : faces)
            {
                if (shards.size() + OS::SHARDS_PER_FACE > MAX_SHARDS) { return; }

                // The face's own position on the object, so pieces start where
                // that part of the crate actually was.
                const float fx = originX + face.cx * objectScaleX;
                const float fy = originY + face.cy * objectScaleY;
                const float fz = originZ + face.cz * objectScaleZ;

                // The original jitters the face centre before cutting, so the
                // four quarters are not identical.
                const float jx = static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);
                const float jy = static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);
                const float jz = static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);

                for (int i = 0; i < OS::SHARDS_PER_FACE; ++i)
                {
                    Shard shard;
                    shard.alive = true;
                    shard.sheetKey = sheetKey;

                    // A quarter of the face: from a corner to the centre
                    // (FUN_000464ec cuts at the edge midpoints and the
                    // centre). It is drawn at (life << 6) / 0x1000 of that
                    // (FUN_0002d054), so it shrinks away as it flies.
                    shard.halfWidth = face.halfWidth * 0.5f * std::max(objectScaleX, objectScaleZ);
                    shard.halfHeight = face.halfHeight * 0.5f * objectScaleY;

                    // Each quarter takes its own corner of the face's UV rect,
                    // matching the corner/midpoint/centre cut.
                    const float um = (face.u0 + face.u1) * 0.5f;
                    const float vm = (face.v0 + face.v1) * 0.5f;
                    shard.u0 = (i & 1) ? um : face.u0;
                    shard.u1 = (i & 1) ? face.u1 : um;
                    shard.v0 = (i & 2) ? vm : face.v0;
                    shard.v1 = (i & 2) ? face.v1 : vm;

                    // All four start from the jittered centre.
                    shard.x = fx + jx;
                    shard.y = fy + jy;
                    shard.z = fz + jz;

                    // UP, with a small sideways spread. Not radially outward -
                    // see LaunchSpeed.
                    shard.vx = static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD) - OS::LAUNCH_SPREAD_CENTRE);
                    shard.vy = launchSpeed + static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD));
                    shard.vz = static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD) - OS::LAUNCH_SPREAD_CENTRE);

                    // ONE axis spins. The first angle is always zero, the second
                    // is fixed from the object, and only the third turns.
                    shard.angleX = 0;
                    shard.angleY = objectFacing;
                    shard.angleZ = rng.Bits(PlayerCamera::ANGLE_MASK);
                    shard.spinX = 0;
                    shard.spinY = 0;
                    shard.spinZ = OS::SHARD_SPIN_PER_FRAME;

                    // +0x1c, the life: (random & 0xf) + 0x20 ticks. +0x18,
                    // the first bounce: the launch speed + (random & 0xf).
                    shard.life = rng.Bits(OS::SHARD_SCALE_RANDOM) + OS::SHARD_SCALE_BASE;
                    shard.bounce = static_cast<int>(launchSpeed) + rng.Bits(0x0f);
                    shards.push_back(shard);
                }
            }
        }

        // One of the original's particle passes, the type 6 arm at 0x2e21e:
        // the life runs down first (0x2da0e) and the shard is gone at zero;
        // every particle then moves by its velocity (FUN_00047524), and a
        // shard moves up or down by it a second time. Above the floor + 0x40
        // its fall speeds up by 4 (exactly 4 turns to -2, never 0). Below it,
        // it is put back at that height, stops moving sideways and bounces up
        // at its bounce speed, which drops by 8 each time; under 4 it stays
        // down, and a shard that is not moving up or down is gone.
        template <typename FloorFn>
        void Tick(FloorFn floorAt)
        {
            for (Shard& shard : shards)
            {
                if (!shard.alive) { continue; }
                if (--shard.life <= 0) { shard.alive = false; continue; }

                shard.angleX = (shard.angleX + shard.spinX) & PlayerCamera::ANGLE_MASK;
                shard.angleY = (shard.angleY + shard.spinY) & PlayerCamera::ANGLE_MASK;
                shard.angleZ = (shard.angleZ + shard.spinZ) & PlayerCamera::ANGLE_MASK;

                shard.x += shard.vx;
                shard.y += shard.vy;
                shard.z += shard.vz;

                if (shard.vy == 0.0f) { shard.alive = false; continue; }
                shard.y += shard.vy;
                const float rest = floorAt(shard.x, shard.z) + REST_HEIGHT;
                if (shard.y < rest)
                {
                    shard.y = rest;
                    shard.bounce -= BOUNCE_LOSS;
                    shard.vy = shard.bounce < BOUNCE_MIN ? 0.0f : static_cast<float>(shard.bounce);
                    shard.vx = shard.vz = 0.0f;
                }
                else
                {
                    shard.vy = (shard.vy == GRAVITY) ? -2.0f : shard.vy - GRAVITY;
                }
            }

            shards.erase(std::remove_if(shards.begin(), shards.end(),
                                        [](const Shard& s) { return !s.alive; }),
                         shards.end());
        }

        void Collect(std::vector<ALTEngine::Renderer::PlacedSprite>& out,
                     float cameraX, float cameraZ) const
        {
            namespace OS = ALTEngine::Formats;
            for (const Shard& shard : shards)
            {
                if (!shard.alive) { continue; }

                // The original's own gate on a type-6 particle.
                const float dx = shard.x - cameraX;
                const float dz = shard.z - cameraZ;
                const float distance = std::sqrt(dx * dx + dz * dz);
                if (distance <= OS::SHARD_NEAR_CUTOFF || distance >= OS::SHARD_FAR_CUTOFF) { continue; }

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = shard.sheetKey;
                sprite.x = shard.x; sprite.y = shard.y; sprite.z = shard.z;
                const float scale = static_cast<float>(shard.life << OS::SHARD_SCALE_SHIFT) / 0x1000;
                sprite.halfWidth = shard.halfWidth * scale;
                sprite.halfHeight = shard.halfHeight * scale;
                sprite.u0 = shard.u0; sprite.v0 = shard.v0;
                sprite.u1 = shard.u1; sprite.v1 = shard.v1;

                // Oriented, not billboarded - a shard has to show its back.
                sprite.billboard = false;
                sprite.rotX = PlayerCamera::ToRadians(shard.angleX);
                sprite.rotY = PlayerCamera::ToRadians(shard.angleY);
                sprite.rotZ = PlayerCamera::ToRadians(shard.angleZ);
                out.push_back(sprite);
            }
        }

        size_t LiveCount() const { return shards.size(); }
        void Clear() { shards.clear(); }

    private:
        // The type 6 arm's numbers (0x2e21e): gravity, the height above the
        // floor a shard comes to rest at, the bounce lost each landing and the
        // least that bounces at all.
        static constexpr float GRAVITY = 4.0f;
        static constexpr float REST_HEIGHT = 0x40;
        static constexpr int BOUNCE_LOSS = 8;
        static constexpr int BOUNCE_MIN = 4;

        // A ceiling on live pieces. The original is bounded by its particle pool
        // of 128, which everything else draws from too; this is a separate
        // budget so a big crate cannot starve the tracers.
        static constexpr size_t MAX_SHARDS = 192;

        std::vector<Shard> shards;
        Particles::Rng rng;
    };
}
