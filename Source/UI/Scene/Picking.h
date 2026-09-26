#pragma once

#include "CameraRig.h"
#include "DeviceLayout.h"

namespace pad
{
    /** Which control (index into layout::controls) is under a normalised device coordinate,
        or -1. Shared by the message thread (mouse events) and the render thread (polled pointer). */
    inline int pickControl (const CameraRig& cam, float ndcX, float ndcY) noexcept
    {
        using namespace layout;

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            float lx = 0, lz = 0;

            if (c.kind == ControlKind::button)
            {
                if (cam.intersectUnit (c.unit, ndcX, ndcY, 0.03f, lx, lz)
                    && Rect { c.x, c.z, buttonOutline (c.buttonStyle).halfW + 0.035f, buttonOutline (c.buttonStyle).halfD + 0.035f }.contains (lx, lz))
                    return i;
            }
            else if (c.kind == ControlKind::toggle)
            {
                if (cam.intersectUnit (c.unit, ndcX, ndcY, 0.02f, lx, lz)
                    && Rect { c.x, c.z, switchOutline (c.switchStyle).halfW + 0.03f, switchOutline (c.switchStyle).halfD + 0.03f }.contains (lx, lz))
                    return i;
            }
            else
            {
                const float r = knobBodyRadius (c);
                if (cam.intersectUnit (c.unit, ndcX, ndcY, r * 0.7f, lx, lz)
                    && std::hypot (lx - c.x, lz - c.z) < r * 1.9f)
                    return i;
            }
        }

        return -1;
    }

    /** Which unit's faceplate is under a normalised device coordinate, or -1 (the case, the room). */
    inline int pickUnit (const CameraRig& cam, float ndcX, float ndcY) noexcept
    {
        using namespace layout;
        for (int unit = 0; unit < numUnits; ++unit)
        {
            float lx = 0.0f, lz = 0.0f;
            if (cam.intersectUnit (unit, ndcX, ndcY, 0.0f, lx, lz) && std::abs (lx) <= unitHalfW (unit) && std::abs (lz) <= unitHalfH (unit))
                return unit;
        }
        return -1;
    }
}
