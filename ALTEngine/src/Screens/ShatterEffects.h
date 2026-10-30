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
            // +0x18: the speed of the next bounce, 8 less each time.
            int bounce = 0;
            // The piece's own four corners around its centre, in the model's
            // space and scale, with their UVs (FUN_000464ec's cut).
            float cx[4] = {}, cy[4] = {}, cz[4] = {}, cu[4] = {}, cv[4] = {};
            float yaw = 0.0f;   // the object's turn, radians, as it was drawn
            int angleX = 0, angleY = 0, angleZ = 0;
            int spinX = 0, spinY = 0, spinZ = 0;
            int life = 0;
            std::string sheetKey;
            bool alive = false;
        };

        // Tears an object apart. `faces` is the model's own face list and
        // `sheetKey` the sprite sheet its texture was uploaded under - both from
        // ModelRenderer. `yawRadians` and the scales are the object's own, as it
        // was drawn (its rotation plus the model's base turn).
        //
        // FUN_000464ec, per face: the edge midpoints and the centre, the centre
        // moved by (random byte - 0x80) on each axis, and four pieces each of
        // { a corner, an edge midpoint, the centre, the other edge midpoint },
        // taken relative to that centre and all starting there. Each wears its
        // quarter of the face's texture and turns with the object (+0x12) while
        // spinning about the model's Z axis (+0x14), which FUN_0002d054 draws
        // through the full transform - so a piece keeps its face's plane: one
        // from a front face spins like a plate, one from a top face tumbles.
        void Shatter(const std::vector<ALTEngine::Renderer::ModelRenderer::ModelFace>& faces,
                     const std::string& sheetKey,
                     float originX, float originY, float originZ,
                     float objectScaleX, float objectScaleY, float objectScaleZ,
                     int objectType, float yawRadians)
        {
            namespace OS = ALTEngine::Formats;
            const float launchSpeed = static_cast<float>(OS::LaunchSpeed(objectType));
            const float cy = std::cos(yawRadians), sy = std::sin(yawRadians);

            for (const auto& face : faces)
            {
                if (shards.size() + OS::SHARDS_PER_FACE > MAX_SHARDS) { return; }

                // Corners in the object's scale, in the model's space.
                float px[4], py[4], pz[4];
                for (int c = 0; c < 4; ++c)
                {
                    px[c] = face.px[c] * objectScaleX;
                    py[c] = face.py[c] * objectScaleY;
                    pz[c] = face.pz[c] * objectScaleZ;
                }
                auto mid = [](float a, float b) { return (a + b) * 0.5f; };
                // The centre: halfway between the 0-3 and 1-2 edges' midpoints,
                // then jittered.
                float ccx = mid(mid(px[0], px[3]), mid(px[1], px[2])) + static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);
                float ccy = mid(mid(py[0], py[3]), mid(py[1], py[2])) + static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);
                float ccz = mid(mid(pz[0], pz[3]), mid(pz[1], pz[2])) + static_cast<float>(rng.Bits(0xff) - OS::FACE_CENTRE_JITTER);
                const float cu = (face.pu[0] + face.pu[1] + face.pu[2] + face.pu[3]) * 0.25f;
                const float cv = (face.pv[0] + face.pv[1] + face.pv[2] + face.pv[3]) * 0.25f;

                // Where the centre is in the world: the object's own transform.
                const float wx = originX + ccx * cy + ccz * sy;
                const float wy = originY + ccy;
                const float wz = originZ - ccx * sy + ccz * cy;

                for (int k = 0; k < OS::SHARDS_PER_FACE; ++k)
                {
                    const int n = (k + 1) & 3, p = (k + 3) & 3;
                    Shard shard;
                    shard.alive = true;
                    shard.sheetKey = sheetKey;
                    shard.yaw = yawRadians;

                    // { corner, midpoint to the next, centre, midpoint to the last }
                    const float qx[4] = { px[k], mid(px[k], px[n]), ccx, mid(px[k], px[p]) };
                    const float qy[4] = { py[k], mid(py[k], py[n]), ccy, mid(py[k], py[p]) };
                    const float qz[4] = { pz[k], mid(pz[k], pz[n]), ccz, mid(pz[k], pz[p]) };
                    const float qu[4] = { face.pu[k], mid(face.pu[k], face.pu[n]), cu, mid(face.pu[k], face.pu[p]) };
                    const float qv[4] = { face.pv[k], mid(face.pv[k], face.pv[n]), cv, mid(face.pv[k], face.pv[p]) };
                    for (int c = 0; c < 4; ++c)
                    {
                        shard.cx[c] = qx[c] - ccx; shard.cy[c] = qy[c] - ccy; shard.cz[c] = qz[c] - ccz;
                        shard.cu[c] = qu[c]; shard.cv[c] = qv[c];
                    }

                    shard.x = wx;
                    shard.y = wy;
                    shard.z = wz;

                    // UP, with a small sideways spread. Not radially outward -
                    // see LaunchSpeed.
                    shard.vx = static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD) - OS::LAUNCH_SPREAD_CENTRE);
                    shard.vy = launchSpeed + static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD));
                    shard.vz = static_cast<float>(rng.Bits(OS::LAUNCH_SPREAD) - OS::LAUNCH_SPREAD_CENTRE);

                    // ONE axis spins: +0x10 is 0, +0x12 the object's turn, and
                    // only +0x14 moves.
                    shard.angleX = 0;
                    shard.angleY = 0;
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

        // A window pane breaking (FUN_00045808): the pane is a 3x3 grid of
        // points 0x200 apart (DAT_000ad4b8 for a window facing z, the grid in
        // x/y; DAT_000ad500 for one facing x, in z/y), cut into four quads
        // around its centre point, which is moved by (random & 0x1ff) - 0x100
        // on every axis - out of the pane too. Each quad is its own particle
        // (type 0xf), all four starting at the pane's centre with the given
        // velocity, the up speed each getting (random & 0xf) - 8 more. Each
        // wears its quarter of the glass texture, the inner corner taking the
        // texture's middle. They never spin (0x2d1a0 skips type 0xf), and move,
        // bounce and shrink exactly as a shard does - the same arm, 0x2e21e.
        void Glass(const std::string& sheetKey, float x, float y, float z,
                   float vx, float vy, float vz, bool facesZ)
        {
            namespace OS = ALTEngine::Formats;
            if (shards.size() + 4 > MAX_SHARDS) { return; }
            const float jAcross = static_cast<float>(rng.Bits(0x1ff) - 0x100);
            const float jUp = static_cast<float>(rng.Bits(0x1ff) - 0x100);
            const float jOut = static_cast<float>(rng.Bits(0x1ff) - 0x100);
            for (const auto& piece : OS::GlassPieces(jAcross, jUp, jOut, facesZ))
            {
                Shard shard;
                shard.alive = true;
                shard.sheetKey = sheetKey;
                for (int c = 0; c < 4; ++c)
                {
                    shard.cx[c] = piece.x[c]; shard.cy[c] = piece.y[c]; shard.cz[c] = piece.z[c];
                    shard.cu[c] = piece.u[c]; shard.cv[c] = piece.v[c];
                }
                shard.x = x; shard.y = y; shard.z = z;
                shard.vx = vx;
                shard.vy = vy + static_cast<float>(rng.Bits(0x0f) - 8);
                shard.vz = vz;
                shard.life = rng.Bits(0x0f) + OS::GLASS_LIFE_BASE;
                shard.bounce = rng.Bits(0x0f) + OS::GLASS_BOUNCE_BASE;
                shards.push_back(shard);
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
                // Drawn at (life << 6) / 0x1000 of its size (FUN_0002d054).
                const float scale = static_cast<float>(shard.life << OS::SHARD_SCALE_SHIFT) / 0x1000;
                sprite.corners = true;
                for (int c = 0; c < 4; ++c)
                {
                    sprite.cornerX[c] = shard.cx[c] * scale;
                    sprite.cornerY[c] = shard.cy[c] * scale;
                    sprite.cornerZ[c] = shard.cz[c] * scale;
                    sprite.cornerU[c] = shard.cu[c];
                    sprite.cornerV[c] = shard.cv[c];
                }

                // Oriented, not billboarded - a shard has to show its back.
                // Spun about the model's Z, then turned with the object.
                sprite.billboard = false;
                sprite.rotX = 0.0f;
                sprite.rotY = shard.yaw;
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
