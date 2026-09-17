#pragma once

#include "../Scene/DeviceLayout.h"
#include "../../Parameters/ParameterBridge.h"

namespace pad
{
    /** Parameter index a control drives right now (a mode-switched knob follows its mode). */
    inline int boundParameter (const ParameterBridge& bridge, int controlIndex)
    {
        if (! juce::isPositiveAndBelow (controlIndex, layout::numControls))
            return -1;

        const auto& c = layout::controls[(size_t) controlIndex];

        if (c.altParamId != nullptr && c.modeParamId != nullptr)
            if (const int mode = bridge.indexOf (c.modeParamId); mode >= 0 && bridge.getNormalised (mode) > 0.5f)
                return bridge.indexOf (c.altParamId);

        return bridge.indexOf (c.paramId);
    }
}
