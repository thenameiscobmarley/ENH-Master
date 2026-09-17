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

        // SERAPH
        std::atomic<float> silkSmoothingDb { 0.0f };   // deepest resonance dip right now
        std::atomic<float> haloDb { -60.0f };          // tail level relative to the dry programme
        std::atomic<float> silkBlend { 0.0f }, haloBlend { 0.0f };

        // SERAPH live display: per-process activity per channel [activity * 2 + ch] (dB), auto level (dB),
        // and SMOOTH's current dip per detection band (dB, <= 0)
        std::array<std::atomic<float>, 16> seraphActivityDb {};
        std::atomic<float> seraphLevelDb { 0.0f };
        std::array<std::atomic<float>, 28> silkDipDb {};
    };
}
