#pragma once

// CLARITY's precision layer (PrecisionEQ): it must find narrow resonances and holes with the right
// frequency and width, leave a flat spectrum alone, and actually take a resonance down.
// Included by EnhDspTests.cpp (uses its check(), Pink and dbfs()). EnhDspTests --precision runs it alone.
#include "../Source/DSP/PrecisionEQ.h"

namespace precisiontests
{
    using namespace enh::dsp;

    struct Found { bool any = false; PrecisionEQ::Band band {}; };

    /** Runs `seconds` of pink noise, optionally through two peaking filters (the "room"), into a
        PrecisionEQ; returns its bands, and (if asked) the output's level in a narrow band around hz. */
    inline std::array<PrecisionEQ::Band, PrecisionEQ::maxBands> run (double sr, bool coloured, int bands, double seconds,
                                                                      float measureHz = 0.0f, float* inDb = nullptr, float* outDb = nullptr)
    {
        PrecisionEQ eq;
        eq.prepare (sr, 1);
        juce::Random rng (5);
        Pink pink;
        BiquadState r1, r2, mIn, mOut;
        const auto ring = BiquadCoeffs::peaking (sr, 2300.0, 8.0, 10.0);   // a narrow resonance: +10 dB, Q 8
        const auto hole = BiquadCoeffs::peaking (sr, 700.0, 5.0, -9.0);    // a narrow hole: -9 dB, Q 5
        const auto meas = BiquadCoeffs::bandPass (sr, measureHz > 0.0f ? measureHz : 1000.0, 12.0);
        const int control = 32;
        std::vector<float> block ((size_t) control);
        double eIn = 0.0, eOut = 0.0;
        const int total = (int) (seconds * sr), measureFrom = total - (int) (2.0 * sr);

        for (int pos = 0; pos < total; pos += control)
        {
            for (int i = 0; i < control; ++i)
            {
                float x = pink.next (rng) * 0.3f;
                if (coloured)
                    x = r2.process (hole, r1.process (ring, x));
                block[(size_t) i] = x;
                eq.push (x);
            }
            eq.update ({ bands, 0.5f, 1.0f, 3.0f }, (float) control / (float) sr);
            if (pos >= measureFrom && measureHz > 0.0f)
                for (int i = 0; i < control; ++i) { const float v = mIn.process (meas, block[(size_t) i]); eIn += (double) v * v; }
            float* ch[1] { block.data() };
            eq.process (ch, 1, 0, control);
            if (pos >= measureFrom && measureHz > 0.0f)
                for (int i = 0; i < control; ++i) { const float v = mOut.process (meas, block[(size_t) i]); eOut += (double) v * v; }
        }
        if (inDb != nullptr)  *inDb = 10.0f * (float) std::log10 (eIn + 1.0e-20);
        if (outDb != nullptr) *outDb = 10.0f * (float) std::log10 (eOut + 1.0e-20);
        return eq.getBands();
    }

    inline Found near (const std::array<PrecisionEQ::Band, PrecisionEQ::maxBands>& bands, float hz, float octaves)
    {
        Found f;
        for (auto& b : bands)
            if (std::abs (b.gainDb) > 0.3f && std::abs (std::log2 (b.hz / hz)) < octaves
                 && (! f.any || std::abs (b.gainDb) > std::abs (f.band.gainDb)))
                f = { true, b };
        return f;
    }

    inline void runPrecisionTests (double sr)
    {
        std::printf ("\n== CLARITY precision bands (moving bells with their own Q) ==\n");

        float inDb = 0.0f, outDb = 0.0f;
        const auto coloured = run (sr, true, 8, 12.0, 2300.0f, &inDb, &outDb);
        for (auto& b : coloured)
            if (std::abs (b.gainDb) > 0.3f)
                std::printf ("  band: %7.1f Hz  Q %5.2f  %+5.1f dB\n", b.hz, b.q, b.gainDb);
        const auto ring = near (coloured, 2300.0f, 1.0f / 12.0f);
        const auto hole = near (coloured, 700.0f, 1.0f / 8.0f);
        std::printf ("  the resonance at 2.3 kHz, measured narrowly: %.1f dB taken off\n", inDb - outDb);
        check (ring.any && ring.band.gainDb < -2.0f, "a narrow resonance (2.3 kHz, +10 dB, Q 8) gets a cut within 1/12 octave of it");
        check (ring.any && ring.band.q > 3.0f, "... and a narrow one (Q " + juce::String (ring.band.q, 1) + ", more than 3), not a broad dip");
        check (inDb - outDb > 3.0f, "the resonance is really taken down (by more than 3 dB)");
        check (hole.any && hole.band.gainDb > 0.5f, "a narrow hole (700 Hz, -9 dB) gets a gentle lift");

        const auto flat = run (sr, false, 8, 8.0);
        float most = 0.0f;
        for (auto& b : flat) most = std::max (most, std::abs (b.gainDb));
        check (most < 1.5f, "plain pink noise is left alone (largest band " + juce::String (most, 2) + " dB, under 1.5)");

        // A steady tone (a held note, a hum, a test tone) on pink noise: content, not a resonance
        {
            PrecisionEQ eq;
            eq.prepare (sr, 1);
            juce::Random rng (9);
            Pink pk;
            std::vector<float> block (32);
            double ph = 0.0;
            for (int pos = 0; pos < (int) (8.0 * sr); pos += 32)
            {
                for (int i = 0; i < 32; ++i)
                {
                    ph += 1000.0 / sr;
                    block[(size_t) i] = pk.next (rng) * 0.05f + 0.3f * (float) std::sin (6.283185307179586 * ph);
                    eq.push (block[(size_t) i]);
                }
                eq.update ({ 8, 0.5f, 1.0f, 3.0f }, 32.0f / (float) sr);
            }
            const auto tone = near (eq.getBands(), 1000.0f, 1.0f / 6.0f);
            check (! tone.any, "a steady 1 kHz tone is content: no precision band on it or beside it"
                               + (tone.any ? juce::String (" (") + juce::String (tone.band.gainDb, 1) + " dB at " + juce::String (tone.band.hz, 0) + " Hz)" : juce::String()));
        }

        const auto off = run (sr, true, 0, 4.0);
        float any = 0.0f;
        for (auto& b : off) any = std::max (any, std::abs (b.gainDb));
        check (any < 1.0e-3f, "PRECISION: OFF moves no band at all");

        const auto four = run (sr, true, 4, 6.0);
        int active = 0;
        for (int k = 4; k < PrecisionEQ::maxBands; ++k) active += std::abs (four[(size_t) k].gainDb) > 1.0e-3f ? 1 : 0;
        check (active == 0, "PRECISION: 4 uses at most four bands");
    }
}
