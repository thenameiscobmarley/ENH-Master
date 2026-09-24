#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** CLARITY's precision layer: up to eight moving bells, each with its own frequency, width (Q) and
        depth, on top of the 24-band adaptive EQ.

        The 24-band EQ has fixed bands of one width (Q 1.6, almost an octave), so it can only draw broad,
        smooth shapes. A single ringing resonance a sixth of an octave wide, a narrow hole, two build-ups
        close together: it cannot shape them without touching everything around them. This layer can:

          analysis  a 4096-point FFT of the input (hop 2048, ~43 ms at 48 kHz) folded onto a 1/24-octave
                    grid from 40 Hz to 16 kHz, integrated over the same time as the 24-band EQ (ADAPT),
                    only while there is sound.
          features  that fine spectrum against itself smoothed over a third of an octave: what sticks up
                    is a resonance, what sinks is a hole. Where the spectrum has lines (a bin 4x stronger than the
                    bins 3 either side: notes, hums, test tones, a regular click's comb) it is content, not a
                    room, and no feature is taken near them - neither the lines nor the gaps between them. Each one's width is measured where it falls to
                    half its height, and becomes the bell's Q - narrow ringing gets a narrow dip, a broad
                    build-up a wide one. Holes are only filled where there is real content.
          bands     the strongest features (up to 8, never closer than a sixth of an octave) are followed
                    by moving bells: a band keeps following its feature as it drifts (frequency and Q glide
                    in the log domain), fades in as a feature appears and out as it goes.
          limits    cuts up to 8 dB, lifts up to 3 dB, both scaled by CLARITY's correction and STRENGTH;
                    250 Hz - 5 kHz (footsteps, voices): a wide cut at most 4 dB, a narrow
                    notch (Q 6 and up) the full 8.

        Zero latency (TPT state-variable bells, modulation-safe, coefficients at control rate).
        Real-time safe after prepare(). It replaces an FFT analyser left from the old footstep detector
        that ran every 256 samples for nothing, so it costs no more CPU than before. */
    class PrecisionEQ
    {
    public:
        static constexpr int maxBands = 8;

        struct Band
        {
            float hz = 1000.0f, q = 1.0f, gainDb = 0.0f;
        };

        struct Settings
        {
            int bands = 8;             // 8, 4 or 0 (the PRECISION method)
            float normalize = 0.5f;    // CLARITY correction strength 0..1
            float strength = 1.0f;     // the device's STRENGTH
            float integrationS = 3.0f; // how long the fine spectrum is averaged (follows ADAPT)
        };

        void prepare (double sampleRate, int maxChannels);
        void reset();

        /** Audio thread, the mono input: analysis. */
        inline void push (float mono) noexcept
        {
            ring[(size_t) writePos] = mono;
            writePos = (writePos + 1) & fftMask;
            if (--toHop == 0)
            {
                toHop = hop;
                analysePending = true;
            }
        }

        /** Control rate: fold a pending FFT frame in, pick the features, glide the bands. */
        void update (const Settings&, float dt) noexcept;

        void process (float* const* channels, int numChannels, int startSample, int numSamples) noexcept;

        const std::array<Band, maxBands>& getBands() const noexcept { return bands; }

        /** A bell's response (dB) at hz: the same shape the display draws. */
        static float bellDb (const Band& b, float hz) noexcept;

        // The fine grid (tests)
        static constexpr int gridPoints = 208;   // 1/24 octave, 40 Hz .. 16 kHz
        static float gridHz (int i) noexcept;
        const std::array<float, gridPoints>& getFineSpectrumDb() const noexcept { return longTermDb; }

    private:
        void analyseFrame() noexcept;
        void findFeatures (const Settings&) noexcept;

        static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, fftMask = fftSize - 1;
        double sr = 48000.0;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> ring, window, work;
        int writePos = 0, hop = 2048, toHop = 2048;
        bool analysePending = false, haveSpectrum = false;
        float frameDt = 0.043f, integrationS = 3.0f;

        // Grid: for each point, the FFT bins it averages (its own bin range, or the two nearest bins
        // interpolated where the grid is finer than the FFT)
        std::array<int, gridPoints> binLo {}, binHi {};
        std::array<float, gridPoints> binFrac {};
        std::array<float, gridPoints> longTermPow {}, longTermDb {};
        std::vector<float> longTermBins;            // the same average, bin by bin (lines: notes, hums, tones)
        std::array<bool, gridPoints> lineNear {};   // a spectral line in or near this grid point's range
        int framesIn = 0;

        struct Target { float hz, q, gainDb, strength; };
        std::array<Target, maxBands> targets {};
        int numTargets = 0;
        bool targetsFresh = false;

        std::array<Band, maxBands> bands {};
        std::array<float, maxBands> wantDb {}, wantHz {}, wantQ {};   // what each band is heading to
        std::array<bool, maxBands> following {};      // assigned to a feature this frame
        std::array<SvfCoeffs, maxBands> coeffs {};
        std::array<float, maxBands> lift {};          // A^2 - 1: how much band-pass is added (negative cuts)
        std::array<std::array<SvfState, maxBands>, 2> states {};
        int channels = 2;
    };
}
