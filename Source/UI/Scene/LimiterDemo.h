#pragma once

#include <cmath>
#include "../../DSP/SpectralLimiter.h"

namespace pad
{
    /** Dev-only (PAD_UI_TEST_DEMO): a plausible SPECTRAL LIMITER state for screenshots without audio -
        a cut following a bass region that drifts between ~55 and ~95 Hz, no broadband protection.
        Shared by the renderer (meters, analyser curtain) and the view (the analyser's readout). */
    inline std::array<enh::dsp::SpectralLimiter::Slot, enh::dsp::SpectralLimiter::numSlots> demoLimiterSlots (double t) noexcept
    {
        std::array<enh::dsp::SpectralLimiter::Slot, enh::dsp::SpectralLimiter::numSlots> slots {};
        slots[0] = { enh::dsp::SpectralLimiter::Shape::bell, (float) (72.0 + 20.0 * std::sin (t * 0.45)), 1.3f,
                     (float) (7.5 + 2.0 * std::sin (t * 1.1)) };
        return slots;
    }
}
