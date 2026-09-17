#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DspMath.h"

namespace enh::dsp
{
    /** Short-time FFT on the mono input (~21 ms window, ~5 ms hop), used to tell tonal /
        ringing sounds from noisy ones in the 0.7-8 kHz region.

        Footsteps are short noise bursts: their spectral peaks change from one independent
        window to the next. Crate latches, hinges, lids, metal rattles, UI chimes and voices
        leave peaks that persist. A peak counts as persistent when it is present in the current
        frame, two hops ago and one full window ago (independent samples).

        Real-time safe: buffers are allocated in prepare(); push() and the per-hop analysis
        never allocate.
    */
    class SpectralAnalyzer
    {
    public:
        void prepare (double sampleRate);
        void reset();

        inline void push (float mono) noexcept
        {
            ring[(size_t) writePos] = mono;
            writePos = (writePos + 1) & mask;

            if (--toHop == 0)
            {
                toHop = hop;
                analyse();
            }
        }

        /** Remember the current spectrum as the "before the event" reference. */
        void captureReference() noexcept;

        /** Share of 0.7-8 kHz energy held in persistent peaks, current frame (0..1). */
        float getTonality() const noexcept         { return tonality; }
        /** Decaying maximum of the tonality (~350 ms): tonal activity around now. */
        float getTonalityHold() const noexcept     { return tonalityHold; }
        /** Tonality of only the energy that rose above the captured reference (0..1). */
        float getRisingTonality() const noexcept   { return risingTonality; }
        /** Spectral flatness 0.7-8 kHz (1 = white noise, 0 = pure tones). */
        float getFlatness() const noexcept         { return flatness; }
        int getFrameCount() const noexcept         { return frameCount; }

    private:
        void analyse() noexcept;

        static constexpr int historyFrames = 5;

        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> ring, window, work, reference;
        std::array<std::vector<float>, historyFrames> power;
        std::array<std::vector<uint8_t>, historyFrames> peaks;

        int size = 1024, mask = 1023, hop = 256, toHop = 256, writePos = 0, histPos = 0;
        int binLo = 1, binHi = 2;
        float holdDecay = 0.98f;
        bool referenceValid = false;

        float tonality = 0.0f, tonalityHold = 0.0f, risingTonality = 0.0f, flatness = 1.0f;
        int frameCount = 0;
    };
}
