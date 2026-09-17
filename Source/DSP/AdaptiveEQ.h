#pragma once

#include "BandAnalyzer.h"
#include "FootstepDetector.h"
#include "PDController.h"

namespace enh::dsp
{
    /** Adaptive, masking-aware spectral leveler: the heart of CLARITY.

        For every analysis band it compares the band with its spectral neighbourhood and
        with its own recent history:
          - bands that dominate their neighbourhood, or burst above their own level, are
            pulled down (masking reduction / multiband dynamics);
          - bands buried under their neighbourhood but above their noise floor are lifted,
            weighted towards detail regions (bass definition, vocal presence, air);
          - in footstep mode, detected steps lift the footstep regions and briefly duck the
            regions that usually mask them.
        Each band gain follows its target through its own PD controller whose gain is
        self-tuned from that band's rolling variance. Gains are applied with 24 overlapping
        peaking filters in series, so the correction is continuous across frequency.
    */
    class AdaptiveEQ
    {
    public:
        struct Settings
        {
            float clarity = 0.5f;        // 0..1
            float speed = 0.4f;          // 0..1
            bool footstepMode = false;
        };

        void prepare (double sampleRate, double controlRate, const BandAnalyzer&, int maxChannels);
        void reset();

        /** Control-rate update: targets -> PD controllers -> filter coefficients. */
        void update (const BandAnalyzer&, const FootstepDetector&, const Settings&, float dt) noexcept;

        void process (float* const* channels, int numChannels, int startSample, int numSamples) noexcept;

        float getGainDb (int band) const noexcept   { return controllers[(size_t) band].value; }
        float getActivity() const noexcept          { return activity; }

    private:
        static constexpr int maxChannels = 2;

        std::array<PeakingDesigner, numBands> designers {};
        std::array<BiquadCoeffs, numBands> coeffs {};
        std::array<std::array<BiquadState, numBands>, maxChannels> states {};
        std::array<PDController, numBands> controllers {};
        std::array<float, numBands> detailWeight {};
        std::array<bool, numBands> active {};
        int activeCount = 0;
        float lookahead = 0.01f;
        float activity = 0.0f;
    };
}
