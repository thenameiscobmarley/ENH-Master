#pragma once

#include "DspMath.h"
#include "PDController.h"

namespace enh::dsp
{
    /** SUB enhance (+ BOOST).

        - Dynamic low shelf: lifts the sub region more when it is weak relative to the
          programme and less when it already dominates (upward compression, no mud).
          The lift follows a PD controller like every other adaptive gain.
        - Psychoacoustic bass: the sub band is saturated and band-passed around
          120-240 Hz so the fundamental is "heard" on headsets/small drivers.
        - BOOST: higher lift ceiling, a tuned 55 Hz punch peak and more harmonics.
    */
    class SubEnhancer
    {
    public:
        struct Settings
        {
            float amount = 0.0f;   // 0..1
            bool boost = false;
            float speed = 0.4f;    // shared ADAPT SPEED
        };

        void prepare (double sampleRate, double controlRate);
        void reset();

        /** Per-sample measurement of the input (call before process for the same samples). */
        inline void measure (float mono) noexcept
        {
            const float lp = lowState2.process (lowCoeffs, lowState1.process (lowCoeffs, mono));
            subFollower.push (lp * lp);
            fullFollower.push (mono * mono);
        }

        void update (const Settings&, float dt) noexcept;
        void process (float* const* channels, int numChannels, int start, int n) noexcept;

        float getLiftDb() const noexcept { return lift.value; }

    private:
        static constexpr int maxChannels = 2;
        double sr = 48000.0;

        BiquadCoeffs lowCoeffs, shelfCoeffs, punchCoeffs, harmonicLow, harmonicBand;
        BiquadState lowState1, lowState2;
        PowerFollower subFollower, fullFollower;

        struct Channel
        {
            BiquadState shelf, punch, split1, split2, hBand1, hBand2;
        };

        std::array<Channel, maxChannels> channels {};
        PDController lift;
        float harmonicMix = 0.0f, harmonicTarget = 0.0f, harmonicStep = 0.0f;
        float drive = 2.0f;
        float lastPunchDb = 1000.0f, lastShelfDb = 1000.0f;
    };
}
