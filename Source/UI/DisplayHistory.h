#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include "../DSP/SpectrumScope.h"

namespace pad
{
    /** Scrolling histories for the LEVEL & LOUDNESS waveform screen and the MIX BALANCER display.
        Filled on the message thread from the audio thread's scope FIFOs and meters; read by the render
        thread. Each column is written before `head` is advanced, so the reader never sees a half one. */
    struct DisplayHistory
    {
        // The rack's input and output: peak per column (the MONITOR's SPEED sets how long a column is)
        static constexpr int waveColumns = 480;
        std::array<std::atomic<float>, waveColumns> wavePeak {};   // output, 0..1
        std::array<std::atomic<float>, waveColumns> waveInPeak {}; // input, 0..1
        std::atomic<int> waveHead { 0 };                           // columns written so far

        // MIX BALANCER: its input and output level (dB) and its deepest cut (dB, <= 0), per ~33 ms
        static constexpr int balColumns = 300;
        std::array<std::atomic<float>, balColumns> balInDb {}, balOutDb {}, balCutDb {};
        std::atomic<int> balHead { 0 };
    };

    /** Message thread: turns the input and output FIFOs into waveform columns (the peak of each). */
    class WaveformReader
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate > 0.0 ? sampleRate : 48000.0; setSecondsAcross (4.8); }

        /** How much audio the screen holds, end to end (the MONITOR's SPEED). */
        void setSecondsAcross (double seconds)
        {
            samplesPerColumn = std::max (1, (int) std::lround (seconds / DisplayHistory::waveColumns * sr));
        }

        void update (const enh::dsp::ScopeFifo& input, const enh::dsp::ScopeFifo& output, DisplayHistory& out)
        {
            // Both FIFOs are written in the same block, so they run level: follow the output's count
            const int write = output.writeIndex.load (std::memory_order_acquire);
            if (last < 0 || write - last > enh::dsp::ScopeFifo::size || write < last)
                last = std::max (0, write - enh::dsp::ScopeFifo::size / 2);

            for (; last < write; ++last)
            {
                const auto k = (size_t) (last & enh::dsp::ScopeFifo::mask);
                peak = std::max (peak, std::abs (output.samples[k]));
                peakIn = std::max (peakIn, std::abs (input.samples[k]));
                if (++count >= samplesPerColumn)
                {
                    const int head = out.waveHead.load (std::memory_order_relaxed);
                    const auto c = (size_t) (head % DisplayHistory::waveColumns);
                    out.wavePeak[c].store (std::min (1.0f, peak), std::memory_order_relaxed);
                    out.waveInPeak[c].store (std::min (1.0f, peakIn), std::memory_order_relaxed);
                    out.waveHead.store (head + 1, std::memory_order_release);
                    peak = peakIn = 0.0f;
                    count = 0;
                }
            }
        }

    private:
        double sr = 48000.0;
        int samplesPerColumn = 480, last = -1, count = 0;
        float peak = 0.0f, peakIn = 0.0f;
    };
}
