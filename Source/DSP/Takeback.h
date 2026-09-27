#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "UnitKit.h"

namespace enh::dsp
{
    /** TAKEBACK (BLONDEX, by Texas Studios) - takes back what processing took out of the sound.
        (Designed in the Rack Unit Designer by the owner; the sound built to its panel.)

        Compression and limiting squash the attacks, and codecs and heavy processing dull the top. TAKEBACK
        gives them back:
          - SHARPEN (0 - 10): the attacks restored - a fast level against a slower one finds each attack,
            and it is lifted by up to twice the amount it stood out (9 dB at most);
          - BLUR (0 - 10): the opposite - each attack's first ~10 ms taken down, softer and smoother. The two work against
            each other, like a photo editor's;
          - COLOR (0 - 10): warmth - a valve stage's harmonics, even-rich, under the sound;
          - RAW (0 - 10): a short room bloom under the sound - a handful of early reflections, wet and full
            but gone within 40 ms, so it never washes the sound out;
          - SHINE (0 - 10): the air rebuilt - the 2.5 - 7 kHz band's harmonics, kept only above 9 kHz: new
            top octave from what is there, never a treble lift of the hiss and edge;
          - MIX (0 - 100 %): the takeback against the sound as it came in;
          - AUTO: it measures what was lost - how squashed the sound is (its peaks against its level, over
            ~1.5 s) and how dull (its top octave against its upper mids) - and gives back as much as that:
            a brick-walled master gets the most SHARPEN, a dull one the most SHINE, a fine one little. The
            knobs set the most it may give.
          - POWER.

        The meters: IN dB+ the attacks lifted, IN dB- the attacks rounded off; OUT dB+ / OUT dB- what the
        output is brought up / down by to stay as loud as it came in (over ~1 s). The six LED ladders: how
        hard each knob's section is working. No latency, no allocation; bit-for-bit out while POWER is off. */
    class Takeback
    {
    public:
        static constexpr int numLadders = 6;   // BLUR, SHARPEN, COLOR, RAW, SHINE, MIX

        struct Settings
        {
            bool power = false, autoOn = true;
            float blur = 0.0f, sharpen = 5.0f, colour = 3.0f, raw = 2.0f, shine = 4.0f;   // 0 .. 10
            float mix = 45.0f;                                                            // 0 .. 100 %
        };
        struct Readout
        {
            float inPlusDb = 0.0f, inMinusDb = 0.0f, outPlusDb = 0.0f, outMinusDb = 0.0f;
            std::array<float, numLadders> ladder {};   // 0 .. 1 each
            float lost = 0.0f, dull = 0.0f;            // what AUTO measured (0 .. 1)
        };

        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            auto pole = [this] (double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); };
            fastAtt = pole (0.0003); slowAtt = pole (0.015); blurAtt = pole (0.010); envRel = pole (0.120);
            gainAtt = pole (0.0002); gainRel = pole (0.008); blurRel = pole (0.015);
            crestPeakRel = pole (1.5); crestK = 1.0f - pole (1.5);
            matchK = 1.0f - pole (1.0);
            shineBand = BiquadCoeffs::bandPass (sr, 4200.0, 0.55);
            shineHp = BiquadCoeffs::highPass (sr, 9000.0, 0.707);
            airHp = BiquadCoeffs::highPass (sr, 9000.0, 0.707);
            mid = BiquadCoeffs::bandPass (sr, 4200.0, 0.55);
            bloomHp = BiquadCoeffs::highPass (sr, 160.0, 0.707);
            bloomLp = BiquadCoeffs::lowPass (sr, 6000.0, 0.707);
            // Early reflections: six taps in 7 - 37 ms, a little different each side (width without a tail)
            static constexpr std::array<std::array<double, numTaps>, 2> ms {{ { 7.1, 11.3, 16.9, 22.7, 29.3, 36.7 },
                                                                            { 8.3, 12.7, 18.1, 24.1, 31.1, 37.9 } }};
            static constexpr std::array<float, numTaps> g { 0.62f, 0.52f, 0.43f, 0.35f, 0.28f, 0.22f };
            float norm = 0.0f;
            for (float x : g) norm += x * x;
            for (int c = 0; c < 2; ++c)
                for (int t = 0; t < numTaps; ++t)
                {
                    taps[(size_t) c][(size_t) t] = std::min (ringSize - 1, (int) std::lround (ms[(size_t) c][(size_t) t] * 0.001 * sr));
                    tapGain[(size_t) t] = g[(size_t) t] / std::sqrt (norm);
                }
            fadeStep = (float) (1.0 / (0.030 * sr));
            for (auto& r : ring)
                r.assign ((size_t) ringSize, 0.0f);   // (here, not while playing; on the heap: the engine lives on
                                                       // callers' stacks in the tests, and 64 KB more broke Windows' 1 MB)
            reset();
        }

        void reset() noexcept
        {
            resetDsp();
            fade = 0.0f;
            primed = false;
            readout = {};
        }

        const Readout& getReadout() const noexcept { return readout; }

        void process (float* const* ch, int numChannels, int n, const Settings& s) noexcept
        {
            const float want = s.power ? 1.0f : 0.0f;
            if (numChannels < 1 || n <= 0 || (fade == 0.0f && want == 0.0f))
            {
                readout = {};
                return;
            }
            if (fade == 0.0f)
                resetDsp();   // coming in: starts clean (the fade covers the start)

            const int nc = std::min (numChannels, 2);
            // AUTO: what was lost scales what is given back (a third of it, even on a sound that lost nothing)
            const float autoSharp = s.autoOn ? 0.6f + 0.4f * lost : 1.0f;
            const float autoShine = s.autoOn ? 0.6f + 0.4f * dull : 1.0f;
            target (sharpG, std::clamp (s.sharpen, 0.0f, 10.0f) / 10.0f * autoSharp, n);
            target (blurG, std::clamp (s.blur, 0.0f, 10.0f) / 10.0f, n);
            target (colourG, std::clamp (s.colour, 0.0f, 10.0f) / 10.0f, n);
            target (rawG, std::clamp (s.raw, 0.0f, 10.0f) / 10.0f, n);
            target (shineG, std::clamp (s.shine, 0.0f, 10.0f) / 10.0f * autoShine, n);
            target (mixG, std::clamp (s.mix, 0.0f, 100.0f) / 100.0f, n);
            primed = true;

            const float mgFrom = matchGain;
            if (inPow > 1.0e-9f && outPow > 1.0e-12f)
                matchGain = std::clamp (std::sqrt (inPow / outPow), 0.5f, 2.0f);   // 6 dB at most either way
            const float mgStep = (matchGain - mgFrom) / (float) n;
            float liftMax = 0.0f, cutMax = 0.0f;
            float colourPow = 0.0f, rawPow = 0.0f, shinePow = 0.0f, sigPow = 1.0e-12f;

            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + fadeStep) : std::max (want, fade - fadeStep);
                const float sharp = sharpG.at (i), blur = blurG.at (i), colour = colourG.at (i), raw = rawG.at (i),
                            shine = shineG.at (i), mix = mixG.at (i);

                // Attacks: one detector for both sides (the image stays put)
                float det = 0.0f;
                for (int c = 0; c < nc; ++c)
                    det = std::max (det, std::abs (ch[c][i]));
                envFast = det > envFast ? det + fastAtt * (envFast - det) : det + envRel * (envFast - det);
                envSlow = det > envSlow ? det + slowAtt * (envSlow - det) : det + envRel * (envSlow - det);
                envBlur = det > envBlur ? det + blurAtt * (envBlur - det) : det + envRel * (envBlur - det);
                auto over = [&] (float slow) { return envFast > 1.0e-5f ? std::clamp (20.0f * std::log10 ((envFast + 1.0e-9f) / (slow + 1.0e-6f)), 0.0f, 24.0f) : 0.0f; };
                // SHARPEN lifts the attack's first few ms; BLUR takes down its first ~10 ms and lets go over
                // ~15 ms (longer, and the loudness match just brings the whole hit back up)
                const float wantDb = std::clamp (2.2f * sharp * over (envSlow) - 0.8f * blur * over (envBlur), -12.0f, 9.0f);
                // (a lift comes fast and goes in ~8 ms; a cut comes fast and lets go over ~30 ms, smoothly)
                const float gk = wantDb > gainDb ? (gainDb < 0.0f ? blurRel : gainAtt) : (gainDb > 0.0f ? gainRel : gainAtt);
                gainDb = wantDb + gk * (gainDb - wantDb);
                liftMax = std::max (liftMax, gainDb);
                cutMax = std::max (cutMax, -gainDb);
                const float tg = dbToGain (gainDb);

                // AUTO's measurements (both sides)
                crestPeak = det > crestPeak ? det : det + crestPeakRel * (crestPeak - det);
                float ms = 0.0f;
                for (int c = 0; c < nc; ++c)
                    ms += ch[c][i] * ch[c][i];
                crestPow += crestK * (ms / (float) nc - crestPow);

                const float mg = mgFrom + mgStep * (float) (i + 1);
                for (int c = 0; c < nc; ++c)
                {
                    auto& st = chan[(size_t) c];
                    const float x = ch[c][i];
                    const float shaped = x * tg;

                    // COLOR: a valve's harmonics, even-rich, under the sound
                    // (driven against its own level: about -30 dB of harmonics at 3, -16 dB at 10, loud or quiet)
                    const float lvl = st.level.process (shaped);
                    const float col = colour * 1.3f * kit::harmonicsAt (st.valve, shaped, lvl, 0.8f + 1.2f * colour, 0.22f);

                    // SHINE: the upper mids' harmonics, only what lands above 9 kHz
                    // (driven at a steady level - the band over its own envelope - so the air it builds follows
                    // the music instead of vanishing when it is quiet; ~15 dB under the band at 10, about where a
                    // bright master's top octave sits under its upper mids)
                    const float band = st.band.process (shineBand, shaped), ab = std::abs (band);
                    st.bandEnv = ab > st.bandEnv ? ab + fastAtt * (st.bandEnv - ab) : ab + envRel * (st.bandEnv - ab);
                    const float airIn = st.bandEnv * st.shineSat.residual (1.2f * band / (st.bandEnv + 1.0e-6f), 0.15f) / 1.2f;
                    const float air = shine * 12.0f * st.shineHp.process (shineHp, airIn);

                    // RAW: a short room bloom
                    auto& rb = ring[(size_t) c];
                    rb[(size_t) st.w] = st.bloomLp.process (bloomLp, st.bloomHp.process (bloomHp, shaped));
                    float er = 0.0f;
                    for (int t = 0; t < numTaps; ++t)
                        er += tapGain[(size_t) t] * rb[(size_t) ((st.w - taps[(size_t) c][(size_t) t]) & (ringSize - 1))];
                    st.w = (st.w + 1) & (ringSize - 1);
                    const float bloom = raw * 0.9f * er;

                    // AUTO: the top octave against the upper mids, as it came in
                    const float hi = st.airHp.process (airHp, x), md = st.mid.process (mid, x);
                    st.hiPow += crestK * (hi * hi - st.hiPow);
                    st.midPow += crestK * (md * md - st.midPow);

                    const float wet = shaped + col + air + bloom;
                    const float y = x + mix * (wet - x);
                    inPow += matchK * (x * x - inPow);
                    outPow += matchK * (y * y - outPow);
                    colourPow += col * col; rawPow += bloom * bloom; shinePow += air * air; sigPow += x * x;
                    ch[c][i] = x + fade * (y * mg - x);
                }
            }

            // AUTO: how squashed (crest 8 dB or less: all of it; 17 dB or more: none) and how dull (top octave
            // 8 dB under the upper mids: fine; 20 dB under: all of it)
            const float crestDb = crestPow > 1.0e-9f ? 20.0f * std::log10 ((crestPeak + 1.0e-9f) / std::sqrt (crestPow)) : 17.0f;
            lost += 0.05f * (std::clamp ((17.0f - crestDb) / 9.0f, 0.0f, 1.0f) - lost);
            float hp = 0.0f, mp = 0.0f;
            for (int c = 0; c < nc; ++c) { hp += chan[(size_t) c].hiPow; mp += chan[(size_t) c].midPow; }
            const float airDb = mp > 1.0e-10f ? 10.0f * std::log10 ((hp + 1.0e-14f) / mp) : -8.0f;
            dull += 0.05f * (std::clamp ((-8.0f - airDb) / 12.0f, 0.0f, 1.0f) - dull);

            // Meters
            const float blockK = 1.0f - std::exp (-(float) n / (0.05f * (float) sr));   // (meters ease over ~50 ms)
            auto ease = [blockK] (float& m, float v) { m += blockK * (v - m); };
            const float matchDb = 20.0f * std::log10 (matchGain);
            ease (readout.inPlusDb, liftMax);
            ease (readout.inMinusDb, cutMax);
            ease (readout.outPlusDb, std::max (0.0f, matchDb));
            ease (readout.outMinusDb, std::max (0.0f, -matchDb));
            auto rel = [sigPow] (float p, float floorDb) { return std::clamp ((10.0f * std::log10 ((p + 1.0e-14f) / sigPow) - floorDb) / 30.0f, 0.0f, 1.0f); };
            const std::array<float, numLadders> lad { cutMax / 6.0f, liftMax / 6.0f, rel (colourPow, -40.0f), rel (rawPow, -36.0f), rel (shinePow, -46.0f),
                                                      mixG.at (n - 1) };
            for (int k = 0; k < numLadders; ++k)
                ease (readout.ladder[(size_t) k], std::clamp (lad[(size_t) k], 0.0f, 1.0f) * fade);
            readout.lost = lost;
            readout.dull = dull;
        }

    private:
        static constexpr int numTaps = 6, ringSize = 8192;   // (40 ms at 192 kHz fits)

        struct Chan
        {
            kit::SoftSat valve, shineSat;
            kit::Level level;
            BiquadState band, shineHp, airHp, mid, bloomHp, bloomLp;
            int w = 0;
            float hiPow = 0.0f, midPow = 0.0f, bandEnv = 0.0f;
        };

        void target (kit::Glide& g, float v, int n) noexcept { if (! primed) g.set (v); g.target (v, n); }

        void resetDsp() noexcept
        {
            for (auto& c : chan) { c = {}; c.level.setup (sr, 0.15); }
            for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
            envFast = envSlow = envBlur = gainDb = crestPeak = crestPow = 0.0f;
            inPow = outPow = 1.0e-9f;
            matchGain = 1.0f;
            lost = dull = 0.0f;
        }

        double sr = 48000.0;
        std::array<Chan, 2> chan {};
        std::array<std::vector<float>, 2> ring { std::vector<float> ((size_t) ringSize), std::vector<float> ((size_t) ringSize) };   // RAW's early reflections
        std::array<std::array<int, numTaps>, 2> taps {};
        std::array<float, numTaps> tapGain {};
        kit::Glide sharpG, blurG, colourG, rawG, shineG, mixG;
        BiquadCoeffs shineBand, shineHp, airHp, mid, bloomHp, bloomLp;
        float fastAtt = 0, slowAtt = 0, blurAtt = 0, blurRel = 0, envRel = 0, gainAtt = 0, gainRel = 0, crestPeakRel = 0, crestK = 0, matchK = 0;
        float envFast = 0, envSlow = 0, envBlur = 0, gainDb = 0, crestPeak = 0, crestPow = 0, lost = 0, dull = 0;
        float inPow = 1.0e-9f, outPow = 1.0e-9f, matchGain = 1.0f, fade = 0.0f, fadeStep = 0.0f;
        bool primed = false;
        Readout readout;
    };
}
