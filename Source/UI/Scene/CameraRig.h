#pragma once

#include "../HardwareKit.h"
#include "DeviceLayout.h"
#include <algorithm>

namespace pad
{
    /** Where the camera is looking: 0 = the whole rack, 1 = filling the frame with `unit`. */
    struct CameraFocus
    {
        int unit = layout::enhUnit;
        float amount = 0.0f;      // 0..1, animated
    };

    /** Pure function of (aspect, parallax, focus): identical on the GL thread (rendering) and on
        the message thread (picking). Views the rack from the front, slightly from above.

        Four units in a case are too tall to frame at a readable size all at once, so the camera
        has a focus: at focus 0 it frames the whole rack (the composition), and as it moves toward
        a unit it dollies in and tracks up or down to that unit, which stays bolted in place. The
        rest of the rack is still there, running off the top and bottom of the frame - the rack
        does not change, only where you are standing.
    */
    struct CameraRig
    {
        gfx::Mat4 view, proj, viewProj;
        gfx::Vec3 eye, forward, right, up;
        gfx::Vec3 lightDir;   // direction *towards* the key light (world)
        float tanHalfFovY = 0.2f, aspect = 1.0f;

        using Focus = CameraFocus;

        static CameraRig build (float aspectRatio, float parallaxX, float parallaxY, Focus focus = Focus())
        {
            constexpr float deg = layout::pi / 180.0f;
            constexpr float fovY = 22.0f * deg;
            constexpr float basePitch = 11.0f * deg;

            CameraRig c;
            c.aspect = std::max (0.2f, aspectRatio);
            c.tanHalfFovY = std::tan (fovY * 0.5f);

            // Wide framing: the whole curved case, bottom unit to top unit plus its overhang
            const auto bottom = layout::unitOrigin (layout::rackOrder.front());
            const auto top = layout::unitOrigin (layout::rackOrder.back());
            const float wideBottom = bottom.y - layout::unitHalfH (layout::rackOrder.front()) - layout::caseOverhang;
            const float wideTop = top.y + layout::unitHalfH (layout::rackOrder.back()) + layout::caseOverhang;
            const float wideHalfW = layout::caseSideX + layout::caseCheekW + 0.10f;
            const float wideHalfV = 0.5f * (wideTop - wideBottom) + 0.05f;
            const float wideDist = std::max (wideHalfW / (c.tanHalfFovY * c.aspect), wideHalfV / c.tanHalfFovY) * 1.02f + 0.25f;
            const float wideCentre = 0.5f * (wideTop + wideBottom);

            // Close framing: one unit and a little of its neighbours
            const float t = std::clamp (focus.amount, 0.0f, 1.0f);
            const float unitHalfV = layout::unitHalfH (focus.unit) + 0.22f;
            const float unitHalfW = layout::faceHalfW + 0.06f;
            const float nearDist = std::max (unitHalfW / (c.tanHalfFovY * c.aspect), unitHalfV / c.tanHalfFovY) * 1.02f + 0.35f;
            const float nearCentre = layout::unitOrigin (focus.unit).y;

            // Ease between the two so the move feels like walking up to the rack, not a jump cut
            const float e = t * t * (3.0f - 2.0f * t);
            const float distance = wideDist + (nearDist - wideDist) * e;
            const float centreY = wideCentre + (nearCentre - wideCentre) * e;

            const float yaw   = parallaxX * 2.0f * deg;
            const float pitch = basePitch * (1.0f - 0.7f * e) + parallaxY * 1.2f * deg;

            const gfx::Vec3 target { 0.0f, centreY, layout::arcCentreZ - layout::arcRadius + 0.45f };
            const gfx::Vec3 dir { std::sin (yaw) * std::cos (pitch), std::sin (pitch), std::cos (yaw) * std::cos (pitch) };

            c.eye = target + dir * distance;
            c.forward = gfx::normalise (target - c.eye);
            c.right = gfx::normalise (gfx::cross (c.forward, { 0, 1, 0 }));
            c.up = gfx::cross (c.right, c.forward);

            c.view = gfx::Mat4::lookAt (c.eye, target, { 0, 1, 0 });
            c.proj = gfx::Mat4::perspective (fovY, c.aspect, std::max (0.5f, distance - 6.0f), distance + 40.0f);
            c.viewProj = c.proj * c.view;

            // Key light: the window, up and to the left, drifting slightly against the parallax.
            c.lightDir = gfx::normalise ({ -0.50f - parallaxX * 0.15f, 0.85f + parallaxY * 0.08f, 0.80f });
            return c;
        }

        /** ndc in [-1, 1], +y up. */
        gfx::Vec3 rayDirection (float ndcX, float ndcY) const noexcept
        {
            return gfx::normalise (forward + right * (ndcX * tanHalfFovY * aspect) + up * (ndcY * tanHalfFovY));
        }

        /** Intersect a unit's faceplate plane, `height` out from it (the units are tilted on
            the arc, so each one has its own plane). Returns panel-local (x, z). */
        bool intersectUnit (int unit, float ndcX, float ndcY, float height, float& localX, float& localZ) const noexcept
        {
            const auto n = layout::unitNormal (unit);
            const auto origin = layout::unitOrigin (unit) + n * height;
            const auto d = rayDirection (ndcX, ndcY);

            const float denom = gfx::dot (d, n);
            if (std::abs (denom) < 1.0e-5f)
                return false;

            const float t = gfx::dot (origin - eye, n) / denom;
            if (t <= 0.0f)
                return false;

            const auto hit = eye + d * t;

            // Panel axes: x across (world +x), z down the panel (the arc's tangent, downward)
            const gfx::Vec3 down { 0.0f, -n.z, n.y };   // n rotated 90 degrees in the yz plane
            localX = hit.x - origin.x;
            localZ = gfx::dot (hit - origin, down);
            return true;
        }
    };
}
