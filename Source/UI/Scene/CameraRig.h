#pragma once

#include "../Render/GLMath.h"
#include "DeviceLayout.h"
#include <algorithm>

namespace pad
{
    /** Pure function of (aspect, parallax): identical on the GL thread (rendering)
        and on the message thread (picking). Views the faceplate from the front,
        slightly from above, like gear sitting on a desk. */
    struct CameraRig
    {
        gfx::Mat4 view, proj, viewProj;
        gfx::Vec3 eye, forward, right, up;
        gfx::Vec3 lightDir;   // direction *towards* the key light (world)
        float tanHalfFovY = 0.2f, aspect = 1.0f;

        static CameraRig build (float aspectRatio, float parallaxX, float parallaxY)
        {
            constexpr float deg = layout::pi / 180.0f;
            constexpr float fovY = 22.0f * deg;
            constexpr float basePitch = 16.0f * deg;

            CameraRig c;
            c.aspect = std::max (0.2f, aspectRatio);
            c.tanHalfFovY = std::tan (fovY * 0.5f);

            // Frame the faceplate (incl. ears) plus a sliver of the lid above it.
            const float halfW = layout::faceHalfW + 0.10f;
            const float vertHalf = layout::faceHalfH + 0.30f;

            const float distW = halfW / (c.tanHalfFovY * c.aspect);
            const float distV = vertHalf / c.tanHalfFovY;
            const float distance = std::max (distW, distV) * 1.07f + 0.55f;

            const float yaw   = parallaxX * 2.0f * deg;
            const float pitch = basePitch + parallaxY * 1.2f * deg;

            const gfx::Vec3 target { 0.0f, layout::faceCenterY + 0.12f, 0.45f };
            const gfx::Vec3 dir { std::sin (yaw) * std::cos (pitch), std::sin (pitch), std::cos (yaw) * std::cos (pitch) };

            c.eye = target + dir * distance;
            c.forward = gfx::normalise (target - c.eye);
            c.right = gfx::normalise (gfx::cross (c.forward, { 0, 1, 0 }));
            c.up = gfx::cross (c.right, c.forward);

            c.view = gfx::Mat4::lookAt (c.eye, target, { 0, 1, 0 });
            c.proj = gfx::Mat4::perspective (fovY, c.aspect, std::max (0.5f, distance - 5.0f), distance + 30.0f);
            c.viewProj = c.proj * c.view;

            // Key light: upper-left-front, drifting slightly against the parallax.
            c.lightDir = gfx::normalise ({ -0.50f - parallaxX * 0.15f, 0.85f + parallaxY * 0.08f, 0.80f });
            return c;
        }

        /** ndc in [-1, 1], +y up. */
        gfx::Vec3 rayDirection (float ndcX, float ndcY) const noexcept
        {
            return gfx::normalise (forward + right * (ndcX * tanHalfFovY * aspect) + up * (ndcY * tanHalfFovY));
        }

        /** Intersect with a plane parallel to the faceplate, `height` out from it.
            Returns panel-local (x, z). */
        bool intersectPanel (float ndcX, float ndcY, float height, float& localX, float& localZ) const noexcept
        {
            const auto d = rayDirection (ndcX, ndcY);
            if (std::abs (d.z) < 1.0e-5f)
                return false;

            const float t = (layout::frontZ + height - eye.z) / d.z;
            if (t <= 0.0f)
                return false;

            localX = eye.x + d.x * t;
            localZ = layout::faceCenterY - (eye.y + d.y * t);
            return true;
        }
    };
}
