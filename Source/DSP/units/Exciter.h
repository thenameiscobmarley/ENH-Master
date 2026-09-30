#pragma once

#include "RackUnit.h"
#include "../UnitKit.h"

/*  882i: a phase-aligning exciter and "sonic maximizer" of the 1990s kind, 1U. */
namespace enh::dsp::units
{
    /** 882i. A loudspeaker (and a long chain of processing) delays the lows behind the highs, which smears
        every attack; this lines them up again and then brightens the top where it is quiet.
          - three bands, split flat (Linkwitz-Riley at 150 Hz and 2.4 kHz, all three at one phase);
          - PROCESS: the high band leads - the mids are held back up to 0.5 ms and the lows up to 2.5 ms
            (the attack arrives as one), and the high band is lifted up to +12 dB, less the more top the sound
            already has (a VCA on the high band, driven by how the top stands against the whole): dull
            material gets clarity, bright material is left alone;
          - LO CONTOUR: the low band lifted up to +12 dB (a 25 Hz subsonic filter is always in, as on the hardware);
          - OUTPUT.
        The delays glide when PROCESS moves (no pitch warble on a turn: they move slowly). Zero latency - the
        high band is never delayed. */
    class Maximizer final : public RackUnit
    {
        kit::Split3 split[2];
        DelayLine dLow[2], dMid[2];
        BiquadCoeffs sub; BiquadState subSt[2][2];
        float dLowNow = 0.0f, dMidNow = 0.0f, hiEnv = 0.0f, allEnv = 0.0f, hiGain = 1.0f;
        kit::Glide lo, out;
        bool first = true;
        void prepareUnit (double s, int) override
        {
            for (auto& sp : split) sp.setup (s, 150.0, 2400.0);
            for (int c = 0; c < 2; ++c) { dLow[c].setMax ((int) (0.004 * s) + 8); dMid[c].setMax ((int) (0.002 * s) + 8); }
            sub = BiquadCoeffs::highPass (s, 25.0, 0.7071);
        }
        void resetUnit() override
        {
            for (auto& sp : split) sp.reset();
            for (int c = 0; c < 2; ++c) { dLow[c].clear(); dMid[c].clear(); subSt[c][0].reset(); subSt[c][1].reset(); }
            dLowNow = dMidNow = hiEnv = allEnv = 0.0f; hiGain = 1.0f; first = true;
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float loDb = std::clamp (p[1], 0.0f, 10.0f) * 1.2f, proc = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const float outDb = std::clamp (p[3], -12.0f, 12.0f);
            if (first) { first = false; lo.set (dbToGainF (loDb)); out.set (dbToGainF (outDb)); dLowNow = 0.0025f * proc * (float) sr; dMidNow = 0.0005f * proc * (float) sr; }
            lo.target (dbToGainF (loDb), n); out.target (dbToGainF (outDb), n);
            const float dLowT = 0.0025f * proc * (float) sr, dMidT = 0.0005f * proc * (float) sr;
            const float dk = 1.0f - std::exp (-1.0f / (0.25f * (float) sr));   // (the delays move over ~250 ms)
            const float eA = 1.0f - std::exp (-1.0f / (0.002f * (float) sr)), eR = 1.0f - std::exp (-1.0f / (0.120f * (float) sr));
            const float gk = 1.0f - std::exp (-1.0f / (0.005f * (float) sr));
            float lift = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                dLowNow += dk * (dLowT - dLowNow); dMidNow += dk * (dMidT - dMidNow);
                float b[2][3];
                for (int c = 0; c < 2; ++c) split[c].split (io[c][i], b[c]);
                // the top against the whole (linked): the quieter the top, the more it is lifted
                const float h = std::max (std::abs (b[0][2]), std::abs (b[1][2])), a = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
                hiEnv += (h > hiEnv ? eA : eR) * (h - hiEnv);
                allEnv += (a > allEnv ? eA : eR) * (a - allEnv);
                const float ratioDb = allEnv > 1.0e-6f ? 20.0f * std::log10 (std::max (hiEnv, 1.0e-9f) / allEnv) : 0.0f;
                const float wantDb = 12.0f * proc * std::clamp ((-6.0f - ratioDb) / 18.0f, 0.0f, 1.0f);   // (top -24 dB under the whole: full lift)
                hiGain += gk * (dbToGainF (wantDb) - hiGain);
                lift = std::max (lift, wantDb);
                const float lg = lo.at (i), og = out.at (i);
                for (int c = 0; c < 2; ++c)
                {
                    dLow[c].push (b[c][0]); dMid[c].push (b[c][1]);
                    // (always through the delays - never switched in and out: a sample's shift is inaudible, a jump is not)
                    const float low = dLow[c].tap (dLowNow + 1.0f), mid = dMid[c].tap (dMidNow + 1.0f);
                    float y = low * lg + mid + b[c][2] * hiGain;
                    y = subSt[c][1].process (sub, subSt[c][0].process (sub, y));   // (the subsonic filter: always, as on the hardware)
                    io[c][i] = y * og;
                }
            }
            setMeter (lift / 12.0f + loDb / 30.0f);
        }
    };
}
