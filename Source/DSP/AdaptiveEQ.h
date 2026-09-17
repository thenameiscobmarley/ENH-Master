#pragma once

#include "BandAnalyzer.h"
#include "FootstepDetector.h"
#include "PDController.h"
#include "HarmonicPlanner.h"

namespace enh::dsp
{
    /** Source-dependent adaptive EQ: the tonal half of CLARITY.

        There is no built-in target curve. Every control tick the curve is derived from the
        programme's own long-term spectrum (BandAnalyzer::ltasDb, integration time set by ADAPT):

          balance    treble (2.5-10 kHz) and bass (40-160 Hz) are measured against the midrange;
                     only a clearly muffled/harsh or thin/boomy source is partly rebalanced, and
                     the rebalancing never moves the midrange itself (a normal balance: nothing)
          local      the spectrum is compared with a local-linear smoothing of itself; resonant
                     build-ups are cut and holes are filled, wherever they are (lifts only where
                     there is real content, so empty bands and noise are not boosted)
          bursts     momentary resonances well above a band's long-term level are tamed
          footstep   detected steps lift the bands their energy actually rose in and gently
                     duck nearby maskers
          add        (ADD mode only) lifts where HarmonicPlanner found definition / body missing
                     and brings up quiet details (decays, tails, distant sounds) in bands that carry
                     content - never a fixed region

        Midrange guard: 250 Hz - 5 kHz carries footsteps, voices and most harmonics, so cuts
        there are limited (-1.5 to -3 dB) and never land on the current footstep's bands.
        The curve is anchored on the midrange (its mean there is 0 dB); auto gain handles level.

        Each band follows its target through a PD controller; the 24 overlapping peaking filters
        are then solved (regularised least squares) so the applied response matches the curve
        instead of piling up where neighbouring bands overlap.
    */
    class AdaptiveEQ
    {
    public:
        struct Settings
        {
            float normalize = 0.5f;      // 0..1  strength of the source-derived correction
            float boost = 0.0f;          // 0..1  ADD mode: detail lift on top of the correction
            float speed = 0.4f;          // 0..1
            bool footstepMode = false;
            HarmonicPlanner::Band depth, clarity;   // where enhancement is useful right now
            float strength = 1.0f;       // device STRENGTH: scales the whole applied curve (0..5)
        };

        void prepare (double sampleRate, double controlRate, const BandAnalyzer&, int maxChannels);
        void reset();

        /** Control-rate update: analysis -> targets -> PD controllers -> solved filter gains. */
        void update (const BandAnalyzer&, const FootstepDetector&, const Settings&, float dt) noexcept;

        void process (float* const* channels, int numChannels, int startSample, int numSamples) noexcept;

        /** Applied response at each band centre (what the display shows). */
        float getGainDb (int band) const noexcept   { return response[(size_t) band]; }
        float getTargetDb (int band) const noexcept { return controllers[(size_t) band].value; }
        float getFilterGainDb (int band) const noexcept { return filterGain[(size_t) band]; }
        float getActivity() const noexcept          { return activity; }
        float getTrebleBalanceDb() const noexcept   { return trebleBalance; }
        float getBassBalanceDb() const noexcept     { return bassBalance; }

        static constexpr double filterQ = 1.6;

    private:
        static constexpr int maxChannels = 2;
        void analyseSource (const BandAnalyzer&, const FootstepDetector&, const Settings&) noexcept;
        using Matrix = std::array<std::array<float, numBands>, numBands>;

        std::array<PeakingDesigner, numBands> designers {};
        std::array<BiquadCoeffs, numBands> coeffs {};
        std::array<std::array<BiquadState, numBands>, maxChannels> states {};
        std::array<PDController, numBands> controllers {};
        std::array<float, numBands> octave {}, midGuard {}, edgeLift {}, trebleWeight {}, bassWeight {}, filterGain {}, response {}, lastDesigned {};
        std::array<bool, numBands> midRegion {}, trebleRegion {}, bassRegion {};
        float trebleBalance = 0.0f, bassBalance = 0.0f;
        std::array<float, numBands> curve {};          // source-derived correction (before footstep priority)
        std::array<float, numBands> addCurve {};       // ADD-mode enhancement lift
        std::array<float, 11> localWeight {};
        int analysisCountdown = 0;
        Matrix overlap {}, solve {};
        std::array<bool, numBands> active {};
        int activeCount = 0;
        float lookahead = 0.01f;
        float activity = 0.0f;
    };
}
