#pragma once

#include <cmath>
#include <array>

namespace pad::gfx
{
    struct Vec3
    {
        float x = 0, y = 0, z = 0;

        Vec3 operator+ (Vec3 o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator- (Vec3 o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
        Vec3 operator* (float s) const noexcept { return { x * s, y * s, z * s }; }
        Vec3 operator- () const noexcept       { return { -x, -y, -z }; }
    };

    inline float dot (Vec3 a, Vec3 b) noexcept   { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline Vec3  cross (Vec3 a, Vec3 b) noexcept { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    inline float length (Vec3 v) noexcept        { return std::sqrt (dot (v, v)); }
    inline Vec3  normalise (Vec3 v) noexcept     { auto l = length (v); return l > 1.0e-9f ? v * (1.0f / l) : Vec3 { 0, 1, 0 }; }

    /** Column-major 4x4 matrix (OpenGL convention). */
    struct Mat4
    {
        std::array<float, 16> m {};

        static Mat4 identity() noexcept
        {
            Mat4 r;
            r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
            return r;
        }

        float& at (int row, int col) noexcept       { return m[(size_t) (col * 4 + row)]; }
        float  at (int row, int col) const noexcept { return m[(size_t) (col * 4 + row)]; }

        Mat4 operator* (const Mat4& o) const noexcept
        {
            Mat4 r;
            for (int c = 0; c < 4; ++c)
                for (int rr = 0; rr < 4; ++rr)
                {
                    float s = 0;
                    for (int k = 0; k < 4; ++k)
                        s += at (rr, k) * o.at (k, c);
                    r.at (rr, c) = s;
                }
            return r;
        }

        Vec3 transformPoint (Vec3 p) const noexcept
        {
            return { at (0, 0) * p.x + at (0, 1) * p.y + at (0, 2) * p.z + at (0, 3),
                     at (1, 0) * p.x + at (1, 1) * p.y + at (1, 2) * p.z + at (1, 3),
                     at (2, 0) * p.x + at (2, 1) * p.y + at (2, 2) * p.z + at (2, 3) };
        }

        Vec3 transformDir (Vec3 p) const noexcept
        {
            return { at (0, 0) * p.x + at (0, 1) * p.y + at (0, 2) * p.z,
                     at (1, 0) * p.x + at (1, 1) * p.y + at (1, 2) * p.z,
                     at (2, 0) * p.x + at (2, 1) * p.y + at (2, 2) * p.z };
        }

        static Mat4 translation (Vec3 t) noexcept
        {
            auto r = identity();
            r.at (0, 3) = t.x; r.at (1, 3) = t.y; r.at (2, 3) = t.z;
            return r;
        }

        static Mat4 scale (float s) noexcept
        {
            auto r = identity();
            r.at (0, 0) = r.at (1, 1) = r.at (2, 2) = s;
            return r;
        }

        static Mat4 scale (float sx, float sy, float sz) noexcept
        {
            auto r = identity();
            r.at (0, 0) = sx; r.at (1, 1) = sy; r.at (2, 2) = sz;
            return r;
        }

        static Mat4 rotationX (float a) noexcept
        {
            auto r = identity();
            const float c = std::cos (a), s = std::sin (a);
            r.at (1, 1) = c; r.at (1, 2) = -s;
            r.at (2, 1) = s; r.at (2, 2) = c;
            return r;
        }

        static Mat4 rotationY (float a) noexcept
        {
            auto r = identity();
            const float c = std::cos (a), s = std::sin (a);
            r.at (0, 0) = c;  r.at (0, 2) = s;
            r.at (2, 0) = -s; r.at (2, 2) = c;
            return r;
        }

        static Mat4 rotationZ (float a) noexcept
        {
            auto r = identity();
            const float c = std::cos (a), s = std::sin (a);
            r.at (0, 0) = c; r.at (0, 1) = -s;
            r.at (1, 0) = s; r.at (1, 1) = c;
            return r;
        }

        static Mat4 perspective (float fovYRadians, float aspect, float zNear, float zFar) noexcept
        {
            Mat4 r;
            const float f = 1.0f / std::tan (fovYRadians * 0.5f);
            r.at (0, 0) = f / aspect;
            r.at (1, 1) = f;
            r.at (2, 2) = (zFar + zNear) / (zNear - zFar);
            r.at (2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
            r.at (3, 2) = -1.0f;
            return r;
        }

        static Mat4 lookAt (Vec3 eye, Vec3 target, Vec3 up) noexcept
        {
            const auto f = normalise (target - eye);
            const auto s = normalise (cross (f, up));
            const auto u = cross (s, f);

            auto r = identity();
            r.at (0, 0) = s.x;  r.at (0, 1) = s.y;  r.at (0, 2) = s.z;
            r.at (1, 0) = u.x;  r.at (1, 1) = u.y;  r.at (1, 2) = u.z;
            r.at (2, 0) = -f.x; r.at (2, 1) = -f.y; r.at (2, 2) = -f.z;
            r.at (0, 3) = -dot (s, eye);
            r.at (1, 3) = -dot (u, eye);
            r.at (2, 3) = dot (f, eye);
            return r;
        }
    };
}
