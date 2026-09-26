#pragma once

#include <array>
#include "DspMath.h"

namespace enh::dsp
{
    /** HEADPHONES (OUTPUT MONITOR): takes a headphone's own colouration out, toward the Harman over-ear
        target - what most listeners prefer - from a published measurement of that model.

        Sennheiser HD 599 / HD 599 SE (one acoustic design): oratory1990's measurement, corrected by AutoEQ
        (github.com/jaakkopasanen/AutoEq, results/oratory1990/over-ear/Sennheiser HD 599). Rtings'
        independent measurement agrees on the big moves: the open back's missing deep bass (+6 dB shelf),
        a mid-bass hump, a dip around 1.7 kHz that thins the mids, and two narrow peaks at ~3.2 and
        ~5.8 kHz - the kind that tire the ear over a long session.

        Ten biquads per channel, no latency. Switching fades over 30 ms (a crossfade with the uncorrected
        signal), so it never steps the audio. After the COMPARE switch, so A and B are heard on the same
        corrected headphones; before EAR GUARD and the output limiter, which look after what the bass
        shelf adds to the peaks. */
    class HeadphoneEQ
    {
    public:
        static constexpr int numFilters = 10;

        void prepare (double sampleRate, int channels) noexcept
        {
            sr = sampleRate;
            chans = std::min (channels, 2);
            design();
            reset();
        }

        void reset() noexcept
        {
            for (auto& ch : state)
                for (auto& s : ch)
                    s.reset();
            fade = 0.0f;
        }

        /** model: 0 off, 1 Sennheiser HD 599 / 599 SE (the method's index). */
        void process (float* const* ch, int numChannels, int n, int model) noexcept
        {
            const float want = model == 1 ? 1.0f : 0.0f;
            if (fade == 0.0f && want == 0.0f)
                return;
            if (fade == 0.0f)
                for (auto& c : state)
                    for (auto& s : c)
                        s.reset();   // coming in: starts clean (the fade covers the start)

            const float step = (float) (1.0 / (0.030 * sr));
            const int nc = std::min (numChannels, chans);
            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + step) : std::max (want, fade - step);
                for (int c = 0; c < nc; ++c)
                {
                    const float x = ch[c][i];
                    float y = x * makeup;
                    for (int f = 0; f < numFilters; ++f)
                        y = state[(size_t) c][(size_t) f].process (coeffs[(size_t) f], y);
                    ch[c][i] = x + fade * (y - x);
                }
            }
        }

    private:
        void design() noexcept
        {
            // oratory1990 / AutoEQ, Harman over-ear target: type, Hz, Q, dB
            struct F { char type; double hz, q, db; };
            static constexpr std::array<F, numFilters> hd599 {{
                { 'L', 105.0, 0.70, 6.1 },  { 'P', 40.0, 3.06, 1.0 },   { 'P', 152.0, 0.59, -4.3 },
                { 'P', 1751.0, 1.45, 4.4 }, { 'P', 4485.0, 2.72, -1.9 }, { 'P', 3175.0, 4.70, -2.0 },
                { 'P', 3690.0, 4.38, 0.7 }, { 'P', 5771.0, 5.56, -2.8 }, { 'P', 9629.0, 1.71, 6.0 },
                { 'H', 10000.0, 0.70, -0.7 },
            }};
            for (int f = 0; f < numFilters; ++f)
            {
                const auto& d = hd599[(size_t) f];
                coeffs[(size_t) f] = d.type == 'L' ? BiquadCoeffs::lowShelf (sr, d.hz, d.q, d.db)
                                   : d.type == 'H' ? BiquadCoeffs::highShelf (sr, d.hz, d.q, d.db)
                                                   : BiquadCoeffs::peaking (sr, d.hz, d.q, d.db);
            }
            // As loud with it as without (on broadband sound, K-weighted - EnhAudioLab): the shelf's extra
            // bass peaks are the output limiter's; AutoEQ's own -6.2 dB preamp would make it sound worse by
            // simply being quieter
            makeup = dbToGain (makeupDb);
        }

        static constexpr float makeupDb = -1.8f;
        double sr = 48000.0;
        int chans = 2;
        float makeup = 1.0f, fade = 0.0f;
        std::array<BiquadCoeffs, numFilters> coeffs {};
        std::array<std::array<BiquadState, numFilters>, 2> state {};
    };
}
