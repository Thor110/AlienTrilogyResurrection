#pragma once

#include "LevelLoader.h"
#include "RenderMesh.h"

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace ALTEngine::Formats
{
    // TESTING: which level face a ray hits first - for the Face Index Display
    // (Modern > Testing), which names the face under the crosshair so a face
    // with a wrong texture can be found, and patched, by its number.
    //
    // The face number is the index into LevelGeometry::quads - the same number
    // the Patch/LxxxLEV.txt lists use - and its record sits at file offset
    // 0x14 + faceRecordsPos + 20 * face.
    //
    // Faces that draw nothing are skipped, and so is the back of a face that is
    // culled, since you see straight through it; what is left is what is on
    // screen. A face's front is the side its (b - a) x (c - a) normal points to,
    // which is the side the renderer's counter-clockwise front face shows.
    struct FacePickResult
    {
        int face = -1;          // -1: nothing hit
        float distance = 0.0f;  // along the (unit) ray
    };

    inline FacePickResult PickLevelFace(const LevelGeometry& level,
                                        float ox, float oy, float oz,
                                        float dx, float dy, float dz,
                                        float maxDistance = 60000.0f)
    {
        FacePickResult best;
        float bestT = maxDistance;
        const auto vertex = [&](int32_t i, double& x, double& y, double& z) {
            if (i < 0 || static_cast<size_t>(i) >= level.vertices.size()) { return false; }
            const auto& v = level.vertices[static_cast<size_t>(i)];
            x = v.x; y = v.y; z = v.z;
            return true;
        };

        for (size_t f = 0; f < level.quads.size(); ++f)
        {
            const ModelQuad& q = level.quads[f];
            const DrawRoutine routine = ClassifyDrawRoutine(q.flags);
            if (routine.fill == FaceFill::Hidden) { continue; }

            const int32_t tris[2][3] = { { q.a, q.b, q.c }, { q.a, q.c, q.d } };
            const int count = (q.d == -1) ? 1 : 2;
            for (int s = 0; s < count; ++s)
            {
                double ax, ay, az, bx, by, bz, cx, cy, cz;
                if (!vertex(tris[s][0], ax, ay, az) || !vertex(tris[s][1], bx, by, bz)
                    || !vertex(tris[s][2], cx, cy, cz)) { continue; }

                const double e1x = bx - ax, e1y = by - ay, e1z = bz - az;
                const double e2x = cx - ax, e2y = cy - ay, e2z = cz - az;

                // Back of a culled face: not drawn from here.
                if (routine.culled)
                {
                    const double nx = e1y * e2z - e1z * e2y;
                    const double ny = e1z * e2x - e1x * e2z;
                    const double nz = e1x * e2y - e1y * e2x;
                    if (nx * dx + ny * dy + nz * dz >= 0.0) { continue; }
                }

                // Moller-Trumbore.
                const double px = dy * e2z - dz * e2y;
                const double py = dz * e2x - dx * e2z;
                const double pz = dx * e2y - dy * e2x;
                const double det = e1x * px + e1y * py + e1z * pz;
                if (std::fabs(det) < 1e-9) { continue; }
                const double inv = 1.0 / det;
                const double tx = ox - ax, ty = oy - ay, tz = oz - az;
                const double u = (tx * px + ty * py + tz * pz) * inv;
                if (u < 0.0 || u > 1.0) { continue; }
                const double qx = ty * e1z - tz * e1y;
                const double qy = tz * e1x - tx * e1z;
                const double qz = tx * e1y - ty * e1x;
                const double v = (dx * qx + dy * qy + dz * qz) * inv;
                if (v < 0.0 || u + v > 1.0) { continue; }
                const double t = (e2x * qx + e2y * qy + e2z * qz) * inv;
                if (t > 0.0 && t < bestT)
                {
                    bestT = static_cast<float>(t);
                    best.face = static_cast<int>(f);
                    best.distance = bestT;
                }
            }
        }
        return best;
    }

    // A view as the renderer builds it (Renderer::FpsCamera): eye, yaw and
    // pitch in radians, roll about the view axis, vertical field of view.
    struct FaceView
    {
        float x = 0, y = 0, z = 0;
        float yaw = 0, pitch = 0, roll = 0;
        float fovY = 1.221f;
    };

    // The view's forward vector - ModelRenderer's: yaw 0 looks down -z.
    inline void ViewForward(const FaceView& v, float& fx, float& fy, float& fz)
    {
        fx = std::sin(v.yaw) * std::cos(v.pitch);
        fy = std::sin(v.pitch);
        fz = -std::cos(v.yaw) * std::cos(v.pitch);
    }

    // A face's outline in window pixels, as the renderer projects it: its
    // LookAt (up rolled about the view axis, side = forward x up) and its
    // perspective, clipped at its near plane (20). Empty when the face is
    // wholly behind the eye.
    inline std::vector<std::pair<float, float>> ProjectFaceOutline(const LevelGeometry& level, int face,
                                                                   const FaceView& view, int width, int height)
    {
        std::vector<std::pair<float, float>> out;
        if (face < 0 || static_cast<size_t>(face) >= level.quads.size()) { return out; }

        float fx, fy, fz;
        ViewForward(view, fx, fy, fz);
        float ux = 0.0f, uy = 1.0f, uz = 0.0f;
        if (view.roll != 0.0f)
        {
            float sx = fz, sz = -fx;
            const float slen = std::sqrt(sx * sx + sz * sz);
            if (slen > 0.0001f)
            {
                sx /= slen; sz /= slen;
                ux = sx * std::sin(view.roll);
                uy = std::cos(view.roll);
                uz = sz * std::sin(view.roll);
            }
        }
        float rx = fy * uz - fz * uy, ry = fz * ux - fx * uz, rz = fx * uy - fy * ux;
        const float rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (rlen < 0.0001f) { return out; }
        rx /= rlen; ry /= rlen; rz /= rlen;
        const float vx = ry * fz - rz * fy, vy = rz * fx - rx * fz, vz = rx * fy - ry * fx;

        const ModelQuad& q = level.quads[static_cast<size_t>(face)];
        const int corners = (q.d == -1) ? 3 : 4;
        const int32_t ids[4] = { q.a, q.b, q.c, q.d };
        struct C { float x, y, z; };
        std::vector<C> poly;
        for (int i = 0; i < corners; ++i)
        {
            if (ids[i] < 0 || static_cast<size_t>(ids[i]) >= level.vertices.size()) { return out; }
            const auto& p = level.vertices[static_cast<size_t>(ids[i])];
            const float px = static_cast<float>(p.x) - view.x;
            const float py = static_cast<float>(p.y) - view.y;
            const float pz = static_cast<float>(p.z) - view.z;
            poly.push_back({ px * rx + py * ry + pz * rz, px * vx + py * vy + pz * vz, px * fx + py * fy + pz * fz });
        }

        constexpr float NEAR_Z = 20.0f;
        std::vector<C> clipped;
        for (size_t i = 0; i < poly.size(); ++i)
        {
            const C& a = poly[i];
            const C& b = poly[(i + 1) % poly.size()];
            const bool aIn = a.z >= NEAR_Z, bIn = b.z >= NEAR_Z;
            if (aIn) { clipped.push_back(a); }
            if (aIn != bIn)
            {
                const float t = (NEAR_Z - a.z) / (b.z - a.z);
                clipped.push_back({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, NEAR_Z });
            }
        }

        const float halfW = static_cast<float>(width) * 0.5f;
        const float halfH = static_cast<float>(height) * 0.5f;
        const float tanHalf = std::tan(view.fovY * 0.5f);
        const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
        for (const C& c : clipped)
        {
            out.emplace_back(halfW + c.x / (c.z * tanHalf * aspect) * halfW,
                             halfH - c.y / (c.z * tanHalf) * halfH);
        }
        return out;
    }

    // The face's record offset in the .MAP file (see above).
    inline size_t LevelFaceFileOffset(const LevelGeometry& level, int face)
    {
        return 0x14 + level.faceRecordsPos + 20 * static_cast<size_t>(face);
    }
}
