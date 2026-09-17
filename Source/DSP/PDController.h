#pragma once

#include "DspMath.h"

namespace enh::dsp
{
    /** Proportional-derivative controller (no integral term) that drives a gain in dB
        towards a moving target.

        The plant is an integrator (value' = velocity), so the loop is
            value' = Kp * e + Kd * e',   e = target - value
        Solving for value' with e' = target' - value' gives
            value' = (Kp * e + Kd * target') / (1 + Kd)
        which is stable for any Kd >= 0 and keeps Kp * dt / (1 + Kd) < 1 in practice.

        Latency compensation: the target is extrapolated along its (smoothed) slope by the
        analysis-to-reaction delay, so the gain arrives when the event does rather than late.
    */
    struct PDController
    {
        float value = 0.0f;
        float velocity = 0.0f;
        float lastTarget = 0.0f;
        float targetSlope = 0.0f;

        void reset (float v = 0.0f) noexcept
        {
            value = lastTarget = v;
            velocity = targetSlope = 0.0f;
        }

        float step (float target, float kp, float kd, float dt, float lookaheadSeconds,
                    float slopeSmoothing, float limit) noexcept
        {
            const float rawSlope = (target - lastTarget) / dt;
            lastTarget = target;
            targetSlope += (rawSlope - targetSlope) * slopeSmoothing;

            const float predicted = target + std::clamp (targetSlope * lookaheadSeconds, -3.0f, 3.0f);
            const float error = predicted - value;

            velocity = (kp * error + kd * targetSlope) / (1.0f + kd);
            value = std::clamp (value + velocity * dt, -limit, limit);
            return value;
        }
    };
}
