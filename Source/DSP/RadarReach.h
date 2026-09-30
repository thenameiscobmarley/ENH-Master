#pragma once

#include <array>
#include <cmath>
#include "DspMath.h"

namespace enh::dsp
{
    /** FOOTSTEP RADAR's REACH: every quiet impact in the footstep range brought forward, whatever the game -
        without having to recognise it as a step first. 24 bands (100 Hz - 8 kHz); in each, a fast level
        against its slow, sustained one says when the band strikes. When several bands strike together (an
        impact is broadband; a single band's flicker is noise), what struck is lifted - and the quieter the
        band has been, the more (up to +24 dB at REACH 10: a step from far off, as if near). A band that is
        already loud (a gunshot, an explosion, a near step) gets little or nothing; sustained sound (music,
        voices, wind, rain's hiss) never opens the gate. Only added: what is there stays as it is. */
    class RadarReach
    {
    public:
        static constexpr int numBands = 24;
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (int b = 0; b < numBands; ++b)
                coeffs[(size_t) b] = SvfCoeffs::make (sr, std::min (100.0 * std::pow (80.0, b / (double) (numBands - 1)), 0.45 * sr), 2.6);
            auto pole = [this] (double s) { return (float) std::exp (-1.0 / (s * sr)); };
            fastAtk = pole (0.0004); fastRel = pole (0.030); slowAtk = pole (0.200); slowRel = pole (0.500); gainK = 1.0f - pole (0.002);
            windowFall = pole (0.045); rateFall = pole (2.0);
            reset();
        }
        void reset() { for (auto& c : st) for (auto& s : c) s.reset(); fast = {}; slow = {}; gain = {}; strikePrev = 0.0f; window = 0.0f; rate = 0.0f; wasBroad = false; }

        /** reach 0 .. 10; voiced 0 .. 1 (the radar's periodicity: a voice or music playing). Adds the lifted
            impacts to l / r in place; returns the lift now (dB). */
        float process (float& l, float& r, float reach, float voiced, float stepLikely = 1.0f) noexcept
        {
            const float most = std::pow (10.0f, 2.4f * std::clamp (reach, 0.0f, 10.0f) / 20.0f) - 1.0f;   // extra gain at full lift (+24 dB at 10)
            // An impact starts when many bands strike together; it is lifted for its first ~45 ms only (a
            // step is over by then - a syllable or a note goes on), and not when impacts come faster than any
            // walker (rain, gravel poured, rattles) or while a voice or music holds a pitch
            const bool broad = strikePrev > 3.0f;
            if (broad && ! wasBroad) { window = 1.0f; rate += 1.0f; }
            wasBroad = broad;
            window *= windowFall;
            rate *= rateFall;
            const float perSecond = rate / 2.0f;
            // ...and, above all, only where the radar says a step is: one it is confirming now, or one a walker it
            // follows is due to take (stepLikely: its detector's word, 0 .. 1 - see FootstepRadar::process)
            const float allowed = window * std::clamp ((7.0f - perSecond) / 4.0f, 0.0f, 1.0f)
                                * (1.0f - std::clamp ((voiced - 0.45f) / 0.3f, 0.0f, 1.0f) * (1.0f - stepLikely)) * stepLikely;
            float addL = 0.0f, addR = 0.0f, strike = 0.0f, lift = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                const auto& c = coeffs[(size_t) b];
                const float yl = st[0][(size_t) b].process (c, l).band, yr = st[1][(size_t) b].process (c, r).band;
                const float e = 0.5f * (yl * yl + yr * yr) + 1.0e-14f;
                auto& f = fast[(size_t) b]; f = e > f ? e + fastAtk * (f - e) : e + fastRel * (f - e);
                auto& s = slow[(size_t) b]; s = e > s ? e + slowAtk * (s - e) : e + slowRel * (s - e);
                const float open = std::clamp ((f / (s + 1.0e-14f) - 2.0f) / 6.0f, 0.0f, 1.0f);
                strike += open;
                // the quieter the impact itself (not its surroundings), the more it may be lifted: a band striking
                // at -30 dBFS or louder gets nothing (a gunshot, a near step); at -75 dBFS, all
                const float fastDb = 10.0f * std::log10 (f);
                const float quiet = std::clamp ((-30.0f - fastDb) / 45.0f, 0.0f, 1.0f);
                const float want = most * open * open * quiet * allowed;
                auto& g = gain[(size_t) b]; g += gainK * (want - g);
                addL += yl * g; addR += yr * g;
                lift = std::max (lift, g);
            }
            strikePrev = strike;
            l += addL; r += addR;
            return 20.0f * std::log10 (1.0f + lift);
        }

    private:
        double sr = 48000.0;
        std::array<SvfCoeffs, numBands> coeffs {};
        std::array<std::array<SvfState, numBands>, 2> st {};
        std::array<float, numBands> fast {}, slow {}, gain {};
        float fastAtk = 0, fastRel = 0, slowAtk = 0, slowRel = 0, gainK = 0, strikePrev = 0.0f, windowFall = 0, rateFall = 0, window = 0, rate = 0;
        bool wasBroad = false;
    };
}
