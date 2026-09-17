#pragma once

#include "../Render/GLMath.h"
#include "DeviceLayout.h"
#include <algorithm>

namespace pad
{
    /** Pure function of (aspect, parallax): identical on the GL thread (rendering)
        and on the message thread (picking). */
    struct CameraRig
    {
        gfx::Mat4 view, proj, viewProj;
        gfx::Vec3 eye, forward, right, up;
        gfx::Vec3 lightDir;   // direction *towards* the key light
        float tanHalfFovY = 0.25f, aspect = 1.0f;

        static CameraRig build (float aspectRatio, float parallaxX, float parallaxY)
        {
            constexpr float deg = 3.14159265f / 180.0f;
            constexpr float fovY = 24.0f * deg;
            constexpr float basePitch = 52.0f * deg;

            CameraRig c;
            c.aspect = std::max (0.2f, aspectRatio);
            c.tanHalfFovY = std::tan (fovY * 0.5f);

            // Frame the whole unit (incl. rack ears) regardless of window shape.
            const float halfW = layout::earOuterX + 0.04f;
            const float halfD = layout::bodyHalfD + 0.05f;
            const float vertHalf = halfD * std::sin (basePitch) + 0.40f * std::cos (basePitch);

            const float distW = halfW / (c.tanHalfFovY * c.aspect);
            const float distV = vertHalf / c.tanHalfFovY;
            const float distance = std::max (distW, distV) * 1.09f + halfD * std::cos (basePitch) * 0.85f;

            const float yaw   = parallaxX * 5.0f * deg;
            const float pitch = basePitch + parallaxY * 3.0f * deg;

            const gfx::Vec3 target { 0.0f, 0.40f, 0.06f };
            const gfx::Vec3 dir { std::sin (yaw) * std::cos (pitch), std::sin (pitch), std::cos (yaw) * std::cos (pitch) };

            c.eye = target + dir * distance;
            c.forward = gfx::normalise (target - c.eye);
            c.right = gfx::normalise (gfx::cross (c.forward, { 0, 1, 0 }));
            c.up = gfx::cross (c.right, c.forward);

            c.view = gfx::Mat4::lookAt (c.eye, target, { 0, 1, 0 });
            c.proj = gfx::Mat4::perspective (fovY, c.aspect, std::max (0.5f, distance - 6.0f), distance + 30.0f);
            c.viewProj = c.proj * c.view;

            // Key light drifts slightly opposite to the camera for a living highlight.
            c.lightDir = gfx::normalise ({ -0.55f - parallaxX * 0.35f, 1.0f, 0.45f - parallaxY * 0.30f });
            return c;
        }

        /** ndc in [-1, 1], +y up. */
        gfx::Vec3 rayDirection (float ndcX, float ndcY) const noexcept
        {
            return gfx::normalise (forward + right * (ndcX * tanHalfFovY * aspect) + up * (ndcY * tanHalfFovY));
        }

        /** Intersect the ray with the horizontal plane y = planeY. */
        bool intersectPlaneY (float ndcX, float ndcY, float planeY, float& outX, float& outZ) const noexcept
        {
            const auto d = rayDirection (ndcX, ndcY);
            if (std::abs (d.y) < 1.0e-5f)
                return false;

            const float t = (planeY - eye.y) / d.y;
            if (t <= 0.0f)
                return false;

            outX = eye.x + d.x * t;
            outZ = eye.z + d.z * t;
            return true;
        }
    };
}
