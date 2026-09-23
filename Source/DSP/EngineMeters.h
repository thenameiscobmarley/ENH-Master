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

        // TIDE (compressor): what the adaptive detector has decided, right now
        std::atomic<float> tideGrDb { 0.0f };
        std::atomic<float> tideThresholdDb { -20.0f };
        std::atomic<float> tideRatio { 2.0f };
        std::atomic<float> tideInputDb { -100.0f }, tideOutputDb { -100.0f };
        std::atomic<float> tideAdaptivity { 0.0f };

        // LUMEN (leveler): lift per band, and what each band is measuring
        std::array<std::atomic<float>, 3> lumenGainDb {};
        std::array<std::atomic<float>, 3> lumenLevelDb {};
        std::atomic<float> lumenTotalDb { 0.0f }, lumenActivity { 0.0f };

        // SPECTRAL LIMITER: the moving cuts as applied (shape 0 bell / 1 low shelf / 2 high shelf),
        // the deepest of them, and broadband protection (all dB of attenuation, >= 0)
        std::array<std::atomic<float>, 3> limitHz {}, limitOctaves {}, limitDepthDb {};
        std::array<std::atomic<int>, 3> limitShape {};
        std::atomic<float> limitDeepestDb { 0.0f }, limitBroadbandDb { 0.0f };
        std::atomic<float> limitMakeupDb { 0.0f };   // loudness keeper: the lift that holds the rest level (dB)

        // SERAPH
        std::atomic<float> silkSmoothingDb { 0.0f };   // deepest resonance dip right now
        std::atomic<float> haloDb { -60.0f };          // tail level relative to the dry programme
        std::atomic<float> silkBlend { 0.0f }, haloBlend { 0.0f };

        // SERAPH live display: per-process activity per channel [activity * 2 + ch] (dB), auto level (dB),
        // and SMOOTH's current dip per detection band (dB, <= 0)
        std::array<std::atomic<float>, 16> seraphActivityDb {};
        std::atomic<float> seraphLevelDb { 0.0f };

        // What the auto modes are doing to the knobs, so the panel can turn them: AUTO heaven's blend and
        // what it chose (DSP units: space, decay s, shimmer, tone, width, air, sub), and MATCH's gain (dB)
        std::atomic<float> heavenAutoBlend { 0.0f };
        std::array<std::atomic<float>, 7> heavenAutoChoice {};
        std::atomic<float> silkMatchDb { 0.0f };

        // LEVEL & LOUDNESS: the rack's output loudness (LUFS, BS.1770) and true peak (dBTP)
        std::atomic<float> momentaryLufs { -120.0f }, shortTermLufs { -120.0f }, integratedLufs { -70.0f }, truePeakDb { -120.0f };
        std::atomic<float> levelDb { 0.0f };   // the LEVEL knob as applied (smoothed)

        // MIX BALANCER: what each band's fader is doing (dB) and its level now (dB)
        std::array<std::atomic<float>, 6> balanceGainDb {}, balanceLevelDb {};
        std::array<std::atomic<float>, 28> balanceFineGainDb {};   // spectral mode's third-octave faders (dB, as applied)
        std::atomic<float> balanceResolution { 0.0f };             // 0 = six bands .. 1 = spectral
        std::atomic<float> balanceMakeupDb { 0.0f };               // loudness keeper's lift (dB)

        // DEEP SUB: what it adds (dBFS RMS), and the bass note it is following
        std::atomic<float> deepGeneratedDb { -120.0f }, deepPitchHz { 55.0f }, deepConfidence { 0.0f };

        // Output limiter: spectral cut per region (low, low-mid, mid, high) and broadband (dB)
        std::array<std::atomic<float>, 4> outputRegionCutDb {};
        std::atomic<float> outputLimitDb { 0.0f };
        std::atomic<float> targetGainDb { 0.0f };    // LOUDNESS TARGET's gain (0 when off)
        std::array<std::atomic<float>, 28> silkDipDb {};
    };
}
