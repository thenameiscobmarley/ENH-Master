#pragma once

#include <algorithm>
#include <array>
#include <cmath>

/*  RACK TUNER's level match: after a tune, the rack is as loud as it was before it, so a tune is heard for
    what it does, never as "louder" (and never jumps up at the ears).

    It compares the rack's input with its output at this point, both slowly averaged (2 s) and a little
    high-passed (so the lowest lows don't rule), which cancels the music's own ups and downs: what is left is
    the rack's gain. When a tune starts (capture) it remembers the output level relative to the input then;
    for the next 8 seconds (the knobs glide, the units settle) it trims the output so that stays the same,
    then holds the trim. A/B and UNDO re-open the window against the same reference (rematch). Trimming is
    gentle (0.8 s), at most 12 dB either way, and waits while the input is silent. Off, the trim eases to 0. */
namespace enh::dsp
{
    class TunerMatch
    {
    public:
        void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
        void reset() noexcept
        {
            msPre = msPost = 1.0e-9f; lpPre = lpPost = 0.0f; trimDb = targetDb = 0.0f; gain = 1.0f;
            windowS = 0.0f; refDb = 0.0f; primed = false;
        }

        /** io: the rack at this point (changed in place); pre: the rack's input for the same samples (nullptr:
            not known this block - it only holds). capture / rematch: counters bumped by the UI. */
        void process (float* const* io, int chans, int n, const float* const* pre, bool enabled, int capture, int rematch) noexcept
        {
            if (n <= 0 || chans <= 0) return;
            const float dt = (float) n / (float) sr;
            const float hk = 1.0f - std::exp (-2.0f * 3.14159265f * 80.0f / (float) sr);
            if (pre != nullptr)
            {
                float sPre = 0.0f, sPost = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float a = chans > 1 ? 0.5f * (pre[0][i] + pre[1][i]) : pre[0][i];
                    const float b = chans > 1 ? 0.5f * (io[0][i] + io[1][i]) : io[0][i];
                    lpPre += hk * (a - lpPre); lpPost += hk * (b - lpPost);
                    const float ha = a - lpPre, hb = b - lpPost;
                    sPre += ha * ha; sPost += hb * hb;
                }
                const float k = 1.0f - std::exp (-dt / 2.0f);
                const float bPre = sPre / (float) n, bPost = sPost / (float) n;
                if (bPre > 1.0e-7f)   // (above about -70 dB: silence teaches it nothing)
                {
                    if (! primed) { msPre = bPre; msPost = bPost; primed = true; }
                    msPre += k * (bPre - msPre); msPost += k * (bPost - msPost);
                }
            }
            const float rackDb = 10.0f * std::log10 ((msPost + 1.0e-12f) / (msPre + 1.0e-12f));   // the rack's gain here, before the trim
            if (capture != seenCapture) { seenCapture = capture; refDb = rackDb + targetDb; windowS = 8.0f; }
            if (rematch != seenRematch) { seenRematch = rematch; windowS = 8.0f; }
            if (! enabled) { targetDb = 0.0f; windowS = 0.0f; }
            else if (windowS > 0.0f)
            {
                windowS -= dt;
                if (primed) targetDb = std::clamp (refDb - rackDb, -12.0f, 12.0f);
            }
            // the trim, eased per sample (0.8 s)
            const float gk = 1.0f - std::exp (-1.0f / (0.8f * (float) sr));
            const float want = std::pow (10.0f, targetDb / 20.0f);
            for (int i = 0; i < n; ++i)
            {
                gain += gk * (want - gain);
                for (int c = 0; c < chans; ++c) io[c][i] *= gain;
            }
            trimDb = 20.0f * std::log10 (std::max (gain, 1.0e-6f));
        }

        float getTrimDb() const noexcept { return trimDb; }
        bool isMatching() const noexcept { return windowS > 0.0f; }

    private:
        double sr = 48000.0;
        float msPre = 1.0e-9f, msPost = 1.0e-9f, lpPre = 0.0f, lpPost = 0.0f;
        float trimDb = 0.0f, targetDb = 0.0f, gain = 1.0f, windowS = 0.0f, refDb = 0.0f;
        bool primed = false;
        int seenCapture = 0, seenRematch = 0;
    };
}
