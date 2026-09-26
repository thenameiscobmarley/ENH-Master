#pragma once

#include "../HardwareKit.h"
#include "DeviceLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace pad
{
    /** Where the camera is looking: 0 = the whole rack, 1 = filling the frame with `unit`. */
    struct CameraFocus
    {
        int unit = layout::enhUnit;
        float amount = 0.0f;      // 0..1, animated
        // Where the close framing is centred and how tall it is, as the renderer glides them from one
        // unit to the next (so a change of unit is a camera move, never a cut). NaN: the unit's own.
        float centreY = std::numeric_limits<float>::quiet_NaN();
        float halfV = std::numeric_limits<float>::quiet_NaN();
        float side = std::numeric_limits<float>::quiet_NaN();    // 0: at the rack, 1: at the LUNCHBOX beside it
        float halfW = std::numeric_limits<float>::quiet_NaN();   // how wide the close framing is
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
            const auto bottom = layout::unitOrigin (layout::bottomUnit());
            const auto top = layout::unitOrigin (layout::topUnit());
            const float wideBottom = bottom.y - layout::unitHalfH (layout::bottomUnit()) - layout::caseOverhang;
            const float wideTop = top.y + layout::unitHalfH (layout::topUnit()) + layout::caseOverhang;
            // (and the LUNCHBOX beside it, when it is out: the frame then centres between the two)
            const float wideLeft = -(layout::caseSideX + layout::caseCheekW + 0.10f);
            const float wideRight = layout::isShown (layout::lunchboxUnit)
                                  ? layout::unitOrigin (layout::lunchboxUnit).x + layout::lbHalfW + 0.30f : -wideLeft;
            const float wideHalfW = 0.5f * (wideRight - wideLeft);
            const float wideCentreX = 0.5f * (wideRight + wideLeft);
            const float wideHalfV = 0.5f * (wideTop - wideBottom) + 0.05f;
            const float wideDist = std::max (wideHalfW / (c.tanHalfFovY * c.aspect), wideHalfV / c.tanHalfFovY) * 1.02f + 0.25f;
            const float wideCentre = 0.5f * (wideTop + wideBottom);

            // Close framing: one unit and a little of its neighbours
            const float t = std::clamp (focus.amount, 0.0f, 1.0f);
            const float unitHalfV = std::isnan (focus.halfV) ? layout::unitHalfH (focus.unit) + 0.22f : focus.halfV;
            const float unitHalfW = std::isnan (focus.halfW) ? layout::unitHalfW (focus.unit) + 0.06f : focus.halfW;
            const float nearDist = std::max (unitHalfW / (c.tanHalfFovY * c.aspect), unitHalfV / c.tanHalfFovY) * 1.02f + 0.35f;
            const float nearCentre = std::isnan (focus.centreY) ? layout::unitOrigin (focus.unit).y : focus.centreY;

            // Ease between the two so the move feels like walking up to the rack, not a jump cut
            const float e = t * t * (3.0f - 2.0f * t);
            const float distance = wideDist + (nearDist - wideDist) * e;
            const float centreY = wideCentre + (nearCentre - wideCentre) * e;
            // Beside the rack, the LUNCHBOX: `side` glides the close framing across to it and back
            const float side = std::isnan (focus.side) ? (focus.unit == layout::lunchboxUnit ? 1.0f : 0.0f) : focus.side;
            const auto lbAt = layout::unitOrigin (layout::lunchboxUnit);
            const bool lbOut = layout::isShown (layout::lunchboxUnit);
            const float faceZ = layout::arcCentreZ - layout::arcRadius + 0.45f;
            const float nearX = lbOut ? side * lbAt.x : 0.0f, nearZ = lbOut ? faceZ + side * (lbAt.z + 0.20f - faceZ) : faceZ;
            const float centreX = wideCentreX + (nearX - wideCentreX) * e;
            const float centreZ = faceZ + (nearZ - faceZ) * e;

            const float yaw   = parallaxX * 2.0f * deg;
            // (the LUNCHBOX stands upright at eye level: seen straight on, its rack partner from a little above)
            const float pitch = basePitch * (1.0f - (0.7f + 0.3f * side) * e) + parallaxY * 1.2f * deg;

            const gfx::Vec3 target { centreX, centreY, centreZ };
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
            if (! layout::isShown (unit))   // out of the case (SIMPLE view): nothing to hit
                return false;
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
