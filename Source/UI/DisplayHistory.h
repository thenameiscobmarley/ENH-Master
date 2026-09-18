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
        // The rack's output: peak per 10 ms column, ~4.8 s across the screen
        static constexpr int waveColumns = 480;
        std::array<std::atomic<float>, waveColumns> wavePeak {};   // 0..1
        std::atomic<int> waveHead { 0 };                           // columns written so far

        // MIX BALANCER: its input and output level (dB) and its deepest cut (dB, <= 0), per ~33 ms
        static constexpr int balColumns = 300;
        std::array<std::atomic<float>, balColumns> balInDb {}, balOutDb {}, balCutDb {};
        std::atomic<int> balHead { 0 };
    };

    /** Message thread: turns the output FIFO into waveform columns (peak of each 10 ms). */
    class WaveformReader
    {
    public:
        void prepare (double sampleRate) { samplesPerColumn = std::max (1, (int) std::lround (0.010 * sampleRate)); }

        void update (const enh::dsp::ScopeFifo& fifo, DisplayHistory& out)
        {
            const int write = fifo.writeIndex.load (std::memory_order_acquire);
            if (last < 0 || write - last > enh::dsp::ScopeFifo::size || write < last)
                last = std::max (0, write - enh::dsp::ScopeFifo::size / 2);

            for (; last < write; ++last)
            {
                peak = std::max (peak, std::abs (fifo.samples[(size_t) (last & enh::dsp::ScopeFifo::mask)]));
                if (++count >= samplesPerColumn)
                {
                    const int head = out.waveHead.load (std::memory_order_relaxed);
                    out.wavePeak[(size_t) (head % DisplayHistory::waveColumns)].store (std::min (1.0f, peak), std::memory_order_relaxed);
                    out.waveHead.store (head + 1, std::memory_order_release);
                    peak = 0.0f;
                    count = 0;
                }
            }
        }

    private:
        int samplesPerColumn = 480, last = -1, count = 0;
        float peak = 0.0f;
    };
}
