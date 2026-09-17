#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <juce_dsp/juce_dsp.h>

namespace enh::dsp
{
    /** Feeds the spectrum analyser without doing any analysis on the audio thread.

        The audio thread only copies samples into a fixed ring buffer (ScopeFifo::push).
        The editor thread pulls a window out of it, windows it, runs the FFT and smooths the
        result (ScopeAnalyser::update), then publishes a log-spaced curve in dB that the GL
        thread reads. Nothing is allocated or locked in push().
    */
    struct ScopeFifo
    {
        static constexpr int size = 4096;     // power of two
        static constexpr int mask = size - 1;

        std::array<float, size> samples {};
        std::atomic<int> writeIndex { 0 };

        /** Audio thread: mono sum of one block. */
        void push (const float* const* data, int numChannels, int numSamples) noexcept
        {
            if (numChannels <= 0 || numSamples <= 0)
                return;

            int w = writeIndex.load (std::memory_order_relaxed);
            const float scale = 1.0f / (float) numChannels;

            for (int i = 0; i < numSamples; ++i)
            {
                float sum = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                    sum += data[c][i];

                samples[(size_t) (w & mask)] = sum * scale;
                ++w;
            }

            writeIndex.store (w, std::memory_order_release);
        }
    };

    /** Log-spaced spectrum in dB, published for the UI. */
    struct ScopeCurve
    {
        static constexpr int numPoints = 192;
        static constexpr float minHz = 20.0f, maxHz = 20000.0f;
        static constexpr float minDb = -90.0f, maxDb = 0.0f;

        std::array<std::atomic<float>, numPoints> inputDb {};
        std::array<std::atomic<float>, numPoints> outputDb {};
        std::array<std::atomic<float>, numPoints> peakDb {};    // decaying peak hold on the output
        std::atomic<float> inputRmsDb { -100.0f }, outputRmsDb { -100.0f };
        std::atomic<bool> active { false };

        static float hzForPoint (int i) noexcept
        {
            const float t = (float) i / (float) (numPoints - 1);
            return minHz * std::pow (maxHz / minHz, t);
        }
    };

    /** Editor thread: turns the two FIFOs into the published curve. */
    class ScopeAnalyser
    {
    public:
        ScopeAnalyser();

        void prepare (double sampleRate);

        /** Call from a timer. dt is used for the decay of the peak-hold and smoothing. */
        void update (const ScopeFifo& input, const ScopeFifo& output, ScopeCurve& out, float dt);

    private:
        void analyse (const ScopeFifo&, std::vector<float>& magsDb);

        static constexpr int order = 11;               // 2048-point FFT
        static constexpr int fftSize = 1 << order;

        juce::dsp::FFT fft { order };
        std::vector<float> window, scratch, inputDb, outputDb, smoothedIn, smoothedOut, peaks;
        std::vector<int> binForPoint;
        double sr = 48000.0;
        bool ready = false;
    };
}
