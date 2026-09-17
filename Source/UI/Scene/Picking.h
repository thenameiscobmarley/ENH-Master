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
            const float centreY = unitCenterY (c.unit);
            float lx = 0, lz = 0;

            if (c.kind == ControlKind::button)
            {
                if (cam.intersectPanel (ndcX, ndcY, 0.03f, lx, lz, centreY)
                    && Rect { c.x, c.z, buttonHalfW + 0.05f, buttonHalfD + 0.05f }.contains (lx, lz))
                    return i;
            }
            else if (c.kind == ControlKind::toggle)
            {
                if (cam.intersectPanel (ndcX, ndcY, 0.08f, lx, lz, centreY)
                    && Rect { c.x, c.z, 0.08f, 0.13f }.contains (lx, lz))
                    return i;
            }
            else
            {
                const float r = knobBodyRadius (c);
                if (cam.intersectPanel (ndcX, ndcY, r * 0.7f, lx, lz, centreY)
                    && std::hypot (lx - c.x, lz - c.z) < r * 1.9f)
                    return i;
            }
        }

        return -1;
    }
}
