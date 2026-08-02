#pragma once

#include <array>
#include <cmath>

namespace ALTEngine::Renderer
{
    // Column-major 4x4 float matrix, matching HLSL's default (and
    // SDL_GPU/D3D convention) - stored as 16 floats, m[col*4+row].
    struct Mat4
    {
        std::array<float, 16> m{};

        static Mat4 Identity()
        {
            Mat4 r;
            r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
            return r;
        }

        static Mat4 RotationY(float radians)
        {
            Mat4 r = Identity();
            float c = std::cos(radians), s = std::sin(radians);
            r.m[0] = c;  r.m[8] = s;
            r.m[2] = -s; r.m[10] = c;
            return r;
        }

        static Mat4 Translation(float x, float y, float z)
        {
            Mat4 r = Identity();
            r.m[12] = x; r.m[13] = y; r.m[14] = z;
            return r;
        }

        // Right-handed look-at, matching the standard textbook formula.
        static Mat4 LookAt(float eyeX, float eyeY, float eyeZ, float atX, float atY, float atZ, float upX, float upY, float upZ)
        {
            float fx = atX - eyeX, fy = atY - eyeY, fz = atZ - eyeZ;
            float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
            fx /= flen; fy /= flen; fz /= flen;

            // s = f x up
            float sx = fy * upZ - fz * upY;
            float sy = fz * upX - fx * upZ;
            float sz = fx * upY - fy * upX;
            float slen = std::sqrt(sx * sx + sy * sy + sz * sz);
            sx /= slen; sy /= slen; sz /= slen;

            // u = s x f
            float ux = sy * fz - sz * fy;
            float uy = sz * fx - sx * fz;
            float uz = sx * fy - sy * fx;

            Mat4 r = Identity();
            r.m[0] = sx; r.m[4] = sy; r.m[8] = sz;
            r.m[1] = ux; r.m[5] = uy; r.m[9] = uz;
            r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz;
            r.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
            r.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
            r.m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
            return r;
        }

        // Right-handed perspective, D3D/SDL_GPU depth range [0,1]
        // (not OpenGL's [-1,1] - matters for the [10]/[14] terms).
        static Mat4 Perspective(float fovYRadians, float aspect, float nearZ, float farZ)
        {
            float f = 1.0f / std::tan(fovYRadians / 2.0f);
            Mat4 r{};
            r.m[0] = f / aspect;
            r.m[5] = f;
            r.m[10] = farZ / (nearZ - farZ);
            r.m[11] = -1.0f;
            r.m[14] = (nearZ * farZ) / (nearZ - farZ);
            return r;
        }

        static Mat4 Multiply(const Mat4& a, const Mat4& b)
        {
            Mat4 r{};
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                    }
                    r.m[col * 4 + row] = sum;
                }
            }
            return r;
        }
    };
}
