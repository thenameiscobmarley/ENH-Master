#pragma once

#include <array>
#include <atomic>

namespace enh::dsp
{
    inline constexpr int numBands = 24;

    /** Lock-free values published by the audio thread for the UI. */
    struct EngineMeters
    {
        std::array<std::atomic<float>, numBands> bandGainDb {};   // adaptive EQ gains
        std::atomic<float> footstepConfidence { 0.0f };           // 0..1
        std::atomic<float> enhancement { 0.0f };                  // 0..1 overall activity
        std::atomic<float> subLiftDb { 0.0f };
        std::atomic<float> autoGainDb { 0.0f };
        std::atomic<float> outputPeakDb { -100.0f };
    };
}
