#pragma once

#include <array>
#include <cmath>
#include "UnitKit.h"

namespace enh::dsp
{
    /** VELVETIZER (BSK-14D1) - smooths sound the way good analog gear does, then colours it.
        (Designed in the Rack Unit Designer by the owner; the sound built to its panel.)

          - VELVET LOW / MID / HIGH (0 - 10, under 250 Hz, 250 Hz - 3.5 kHz, over 3.5 kHz): how much each
            band is smoothed - its spiky transients rounded off (a fast level against a slower one: only
            the part that stands out is taken down) and a soft analog saturation that thickens it.
          - GRAIN (0 - 10): texture - fine, low-level even and odd harmonics, like tape and valves.
          - CRISP (0 - 10): clarity given back on top - the attack of the treble (not its level) brought
            forward, so smooth never means dull.
          - COLOR TYPE A and B (six models each: TUBE, TAPE, TRANSFORMER, CONSOLE, TRANSISTOR, CRYSTAL) and
            BALANCE between them (0: all A, 100: all B).
          - ADD / BALANCE (the two buttons): ADD puts the warmth on top of the sound (it gets fuller and a
            little louder); BALANCE rebalances what is there - as loud as it came in.
          - POWER, and the red button: BYPASS (the sound as it came in, for comparing).

        The VELVET dB+ meter: how much the transients are being smoothed. Anti-aliased curves, no latency,
        no allocation; bit-for-bit out while POWER is off or BYPASS is in. */
    class Velvetizer
    {
    public:
        static constexpr int numBands = 3, numColours = 6;
        static constexpr float colourMix = 0.30f;   // how much of a colour model's harmonics are blended in

        struct Settings
        {
            bool power = false, bypass = false, balanceMode = true;
            std::array<float, numBands> velvet { 5.0f, 5.0f, 5.0f };   // 0 .. 10
            float grain = 2.5f, crisp = 5.0f;                         // 0 .. 10
            int colourA = 0, colourB = 1;                             // 0 .. 5
            float balance = 50.0f;                                    // 0 .. 100
        };

        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            for (auto& s : split) s.setup (sr, 250.0, 3500.0);
            auto pole = [this] (double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); };
            fastAtt = pole (0.0005); fastRel = pole (0.040); slowAtt = pole (0.020); slowRel = pole (0.250);
            grAtt = pole (0.0003); grRel = pole (0.030);
            matchK = 1.0f - pole (1.0);
            crispHp = BiquadCoeffs::highPass (sr, 3500.0, 0.707);
            tapeLp = 1.0f - pole (1.0 / (2.0 * 3.141592653589793 * 14000.0));
            ironLp = 1.0f - pole (1.0 / (2.0 * 3.141592653589793 * 220.0));
            fadeStep = (float) (1.0 / (0.030 * sr));
            reset();
        }

        void reset() noexcept
        {
            resetDsp();
            fade = 0.0f;
            primed = false;
            meterDb = 0.0f;
        }

        /** The VELVET dB+ meter: how far transients are taken down now (dB, >= 0). */
        float getVelvetDb() const noexcept { return meterDb; }

        void process (float* const* ch, int numChannels, int n, const Settings& s) noexcept
        {
            const float want = s.power && ! s.bypass ? 1.0f : 0.0f;
            if (numChannels < 1 || n <= 0 || (fade == 0.0f && want == 0.0f))
            {
                meterDb = 0.0f;
                return;
            }
            if (fade == 0.0f)
                resetDsp();

            const int nc = std::min (numChannels, 2);
            for (int b = 0; b < numBands; ++b)
                target (velvetG[(size_t) b], std::clamp (s.velvet[(size_t) b], 0.0f, 10.0f) / 10.0f, n);
            target (grainG, std::clamp (s.grain, 0.0f, 10.0f) / 10.0f, n);
            target (crispG, std::clamp (s.crisp, 0.0f, 10.0f) / 10.0f, n);
            target (balG, std::clamp (s.balance, 0.0f, 100.0f) / 100.0f, n);
            target (modeG, s.balanceMode ? 1.0f : 0.0f, n);
            primed = true;
            const int ca = std::clamp (s.colourA, 0, numColours - 1), cb = std::clamp (s.colourB, 0, numColours - 1);

            const float mgFrom = matchGain;
            if (inPow > 1.0e-9f && outPow > 1.0e-12f)
                matchGain = std::clamp (std::sqrt (inPow / outPow), 0.25f, 4.0f);
            const float mgStep = (matchGain - mgFrom) / (float) n;
            // ADD: the warmth on top, 1 dB louder than it came in (a saturator's harmonics partly cancel the
            // peaks they come from, so "on top" alone came out quieter)
            const float agFrom = addGain;
            if (inPow > 1.0e-9f && addPow > 1.0e-12f)
                addGain = std::clamp (1.122f * std::sqrt (inPow / addPow), 0.25f, 4.0f);
            const float agStep = (addGain - agFrom) / (float) n;
            float grMax = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + fadeStep) : std::max (want, fade - fadeStep);
                const float grain = grainG.at (i), crisp = crispG.at (i), bal = balG.at (i), mode = modeG.at (i);
                for (int c = 0; c < nc; ++c)
                {
                    auto& st = chan[(size_t) c];
                    const float x = ch[c][i];
                    float b[numBands];
                    split[(size_t) c].split (x, b);
                    float lin = 0.0f, harm = 0.0f;   // the smoothed sound, and the harmonics added to it
                    for (int k = 0; k < numBands; ++k)
                    {
                        const float a = velvetG[(size_t) k].at (i);
                        const float v = b[k], av = std::abs (v);
                        // Transients rounded off: what stands above the slower level comes down, gently
                        auto& f = st.fast[(size_t) k];
                        auto& sl = st.slow[(size_t) k];
                        f = av > f ? av + fastAtt * (f - av) : av + fastRel * (f - av);
                        sl = av > sl ? av + slowAtt * (sl - av) : av + slowRel * (sl - av);
                        const float over = f / (1.1f * sl + 1.0e-6f);
                        const float want_ = over > 1.0f ? std::pow (over, -1.6f * a) : 1.0f;
                        auto& g = st.gr[(size_t) k];
                        g = want_ < g ? want_ + grAtt * (g - want_) : want_ + grRel * (g - want_);
                        grMax = std::max (grMax, -20.0f * std::log10 (std::max (g, 1.0e-4f)));
                        const float sm = v * g;
                        // A soft analog saturation that thickens it (more of it the more velvet), driven against the
                        // band's own level: about -28 dB of harmonics at 5, -18 dB at 10, loud or quiet
                        const float lvl = st.level[(size_t) k].process (sm);
                        lin += sm;
                        harm += (0.15f + 0.45f * a) * kit::harmonicsAt (st.sat[(size_t) k], sm, lvl, 0.7f + 1.0f * a, 0.08f);
                    }
                    float y = lin + harm;
                    // GRAIN: fine low-level harmonics (an asymmetric curve's difference only, well under the sound)
                    const float lvlY = st.levelY.process (y);
                    harm += grain * 0.08f * kit::harmonicsAt (st.grain, y, lvlY, 1.5f, 0.3f);
                    y = lin + harm;
                    // COLOR TYPE A and B, balanced
                    const float colA = colour (ca, y, lvlY, st, 0), colB = colour (cb, y, lvlY, st, 1);
                    harm += colourMix * (colA + bal * (colB - colA) - y);   // (a colour, blended in: never a fuzz)
                    y = lin + harm;
                    // CRISP: the treble's attack (its HF above its own slow level) brought forward
                    const float h = st.hp.process (crispHp, x), ah = std::abs (h);
                    st.hf = ah > st.hf ? ah + fastAtt * (st.hf - ah) : ah + fastRel * (st.hf - ah);
                    st.hs = ah > st.hs ? ah + slowAtt * (st.hs - ah) : ah + slowRel * (st.hs - ah);
                    const float attack = std::clamp ((st.hf - 1.2f * st.hs) / (st.hf + 1.0e-6f), 0.0f, 1.0f);
                    const float crispAdd = crisp * 0.6f * attack * h;
                    y += crispAdd;

                    inPow += matchK * (x * x - inPow);
                    outPow += matchK * (y * y - outPow);
                    // BALANCE: the smoothed, coloured sound, as loud as it came in. ADD: the sound as it came in,
                    // with the warmth (every added harmonic) and the crispness on top - fuller, a little louder.
                    const float mg = mgFrom + mgStep * (float) (i + 1);
                    const float added = x + harm + crispAdd;
                    addPow += matchK * (added * added - addPow);
                    const float out = mode * (y * mg) + (1.0f - mode) * (added * (agFrom + agStep * (float) (i + 1)));
                    ch[c][i] = x + fade * (out - x);
                }
            }
            meterDb += 0.3f * (grMax - meterDb);
        }

    private:
        struct Chan
        {
            std::array<float, numBands> fast {}, slow {}, gr { 1.0f, 1.0f, 1.0f };
            std::array<kit::SoftSat, numBands> sat {};
            std::array<kit::Level, numBands> level {};
            kit::Level levelY;
            kit::SoftSat grain;
            std::array<kit::SoftSat, 2> col {};
            std::array<float, 2> lp {};   // TAPE's top / TRANSFORMER's low, per colour slot
            BiquadState hp;
            float hf = 0.0f, hs = 0.0f;
        };

        /** One colour model on a sample; `slot` 0 / 1 keeps A's and B's state apart. */
        float colour (int model, float x, float lvl, Chan& st, int slot) noexcept
        {
            auto& sat = st.col[(size_t) slot];
            auto& lp = st.lp[(size_t) slot];
            switch (model)
            {
                // (each the curve's own harmonics on top of the sound, driven against its level: see UnitKit)
                case 0: return x + kit::harmonicsAt (sat, x, lvl, 1.1f, 0.25f);                          // TUBE: even-rich
                case 1: { const float y = x + kit::harmonicsAt (sat, x, lvl, 0.9f, 0.0f);                 // TAPE: soft, top rounded
                          lp += tapeLp * (y - lp); return lp; }
                case 2: { lp += ironLp * (x - lp);                                                      // TRANSFORMER: the lows saturate
                          return x + kit::harmonicsAt (sat, lp, lvl, 1.8f, 0.05f); }
                case 3: return x + kit::harmonicsAt (sat, x, lvl, 0.8f, 0.02f);                          // CONSOLE: gentle, odd
                case 4: return x + kit::harmonicsAt (sat, x, lvl, 1.5f, 0.0f);                           // TRANSISTOR: firmer, odd
                default: return x;                                                                  // CRYSTAL: clean
            }
        }

        void target (kit::Glide& g, float v, int n) noexcept { if (! primed) g.set (v); g.target (v, n); }

        void resetDsp() noexcept
        {
            for (auto& s : split) s.reset();
            for (auto& c : chan)
            {
                c = {};
                for (auto& l : c.level) l.setup (sr, 0.15);
                c.levelY.setup (sr, 0.15);
            }
            inPow = outPow = addPow = 1.0e-9f;
            matchGain = addGain = 1.0f;
        }

        double sr = 48000.0;
        std::array<kit::Split3, 2> split {};
        std::array<Chan, 2> chan {};
        std::array<kit::Glide, numBands> velvetG {};
        kit::Glide grainG, crispG, balG, modeG;
        BiquadCoeffs crispHp;
        float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, grAtt = 0, grRel = 0, matchK = 0, tapeLp = 0, ironLp = 0;
        float inPow = 1.0e-9f, outPow = 1.0e-9f, addPow = 1.0e-9f, matchGain = 1.0f, addGain = 1.0f, fade = 0.0f, fadeStep = 0.0f, meterDb = 0.0f;
        bool primed = false;
    };
}
