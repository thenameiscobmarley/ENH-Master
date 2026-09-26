#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace enh::dsp
{
    /** The rack's output amplifier: the last analog stage before the LOUDNESS TARGET, so every unit's
        colour comes out through one piece of hardware and sounds like one signal path.

        Extremely subtle and level-aware: below about -12 dBFS it is a wire (a few thousandths of a
        percent); hot passages meet a soft odd-order curve and a top that closes a little, the way a
        real line amplifier runs out of slew and headroom gently rather than clipping. Its two channels
        differ by a hair, as two real channels do. It only runs while an analog unit is in (the enhancer
        at STRENGTH above 0, CHARACTER, or the LUNCHBOX's EQ): otherwise it is bit-for-bit bypassed, so
        TRANSPARENT stays transparent. Switching fades over 30 ms. Zero latency; real-time safe. */
    class OutputStage
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            envAtt = (float) std::exp (-1.0 / (0.002 * sr));
            envRel = (float) std::exp (-1.0 / (0.080 * sr));
            lpK = 1.0f - (float) std::exp (-2.0 * 3.141592653589793 * 15000.0 / sr);
            fadeStep = 1.0f / (float) std::max (1.0, 0.030 * sr);
            reset();
        }

        void reset()
        {
            env = {};
            lp = {};
            mix = 0.0f;
        }

        void process (float* const* ch, int numChannels, int n, bool active) noexcept
        {
            const float target = active ? 1.0f : 0.0f;
            if (mix == 0.0f && target == 0.0f)
            {
                env = {};
                return;   // bypassed: bit for bit
            }
            const int nc = std::min (numChannels, 2);
            for (int i = 0; i < n; ++i)
            {
                mix = target > mix ? std::min (target, mix + fadeStep) : std::max (target, mix - fadeStep);
                for (int c = 0; c < nc; ++c)
                {
                    const float x = ch[c][i];
                    const float a = std::abs (x);
                    auto& e = env[(size_t) c];
                    e = a > e ? a + envAtt * (e - a) : a + envRel * (e - a);
                    // How hard the stage is being driven: nothing below -12 dBFS, fully at 0 dBFS
                    const float driven = std::clamp ((e - 0.25f) / 0.75f, 0.0f, 1.0f);
                    // A soft odd curve (the right channel's a hair different) ...
                    const float k = (0.004f + 0.022f * driven) * (c == 1 ? 1.03f : 1.0f);
                    const float xc = std::clamp (x, -1.6f, 1.6f);
                    float y = x - k * xc * xc * xc / (1.0f + 0.5f * xc * xc);
                    // ... and a top that closes a little when hot (the stage's slew and bandwidth)
                    auto& l = lp[(size_t) c];
                    l += lpK * (y - l);
                    y += (l - y) * 0.35f * driven;
                    ch[c][i] = x + (y - x) * mix;
                }
            }
        }

    private:
        double sr = 48000.0;
        float envAtt = 0.0f, envRel = 0.0f, lpK = 0.0f, fadeStep = 0.001f, mix = 0.0f;
        std::array<float, 2> env {}, lp {};
    };
}
