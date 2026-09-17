#pragma once

#include "BandAnalyzer.h"

namespace enh::dsp
{
    /** Footstep detector for game audio (control rate).

        A footstep is modelled as a short onset with energy in a low "thump" region
        (70-260 Hz) and/or a high "scuff/click" region (1.8-7 kHz) that stands out from
        that region's own background. Onsets are rejected or down-weighted when they are
        near full scale or broadband (gunfire, explosions), mid-dominated (voices, weapon
        bodies), or keep ringing past ~110 ms. Repeating onsets with a walking rhythm
        (0.22-0.9 s apart) raise confidence.

        Output: confidence 0..1 with instant attack and ~200 ms release.
    */
    class FootstepDetector
    {
    public:
        void prepare (const BandAnalyzer&, double controlRate);
        void reset();
        float update (const BandAnalyzer&, float dt) noexcept;

        float getConfidence() const noexcept { return confidence; }
        int getEventCount() const noexcept   { return eventCount; }

        /** Last computed decision factors (for tests / diagnostics). */
        struct Trace { float onset, balance, share, hot, broadband, midDominance, sustain, raw, confirmed, rhythm; };
        Trace trace {};

        /** Band weights of the footstep regions / competing regions, for the adaptive EQ. */
        std::array<float, numBands> stepWeight {}, competitorWeight {};

    private:
        struct Group
        {
            std::array<bool, numBands> member {};
            float background = -90.0f;

            float power (const std::array<PowerFollower, numBands>& f) const noexcept
            {
                float sum = 0.0f;
                for (int k = 0; k < numBands; ++k)
                    if (member[(size_t) k])
                        sum += f[(size_t) k].env;
                return sum;
            }
        };

        Group low, high, mid;
        float confidence = 0.0f, rhythm = 0.0f, sustainPenalty = 0.0f, hotHold = 0.0f, eventPeakDb = -120.0f, fullBackground = -90.0f;
        double clock = 0.0, lastEvent = -10.0, lastInterval = 0.0;
        int eventCount = 0;

        static constexpr int maxConfirmTicks = 32;
        std::array<float, maxConfirmTicks> rawHistory {};
        int historyPos = 0, confirmTicks = 9;
    };
}
