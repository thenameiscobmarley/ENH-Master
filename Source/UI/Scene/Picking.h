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

            if (c.kind == ControlKind::toggle)
            {
                if (cam.intersectPanel (ndcX, ndcY, 0.12f, lx, lz)
                    && Rect { c.x, c.z, switchPlateHalfW + 0.08f, switchPlateHalfD + 0.10f }.contains (lx, lz))
                    return i;
            }
            else if (cam.intersectPanel (ndcX, ndcY, capTop * 0.6f * c.scale, lx, lz)
                     && std::hypot (lx - c.x, lz - c.z) < bezelRadius * c.scale * 1.08f)
            {
                return i;
            }
        }

        return -1;
    }
}
