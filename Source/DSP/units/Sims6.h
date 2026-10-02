#pragma once

#include "Sims3.h"
#include <juce_dsp/juce_dsp.h>

/*  SPECTRAL DETAIL ENHANCER (3.8.0.1): works like the ADAPTIVE ENHANCER - it listens, then lifts - but what it
    listens with is a simulation of hearing itself. */
namespace enh::dsp::units
{
    // ------------------------------------------------------------------------------------------------
    /** SPECTRAL DETAIL ENHANCER: finds what the mix hides and brings it out. The sound is split into 24 bands, a
        critical band each (as the ear's cochlea splits it), and for every band it works out, from the bands
        around it, how loud the ear's masking threshold is there: a loud band hides the quieter ones next to it,
        far more above it than below (the spreading of masking). Whatever is real (above that band's own noise
        floor - hiss is never lifted) but under its threshold is detail the ear doesn't hear: a reverb's tail, a
        breath, a ghost note, the second guitar. That is lifted - by how deep it is hidden, up to DEPTH below the
        threshold; hidden transients (rising faster than their band's slow level) a little more - so it comes
        out from under what masks it.
          DETAIL how much it lifts; DEPTH how far under the masking it reaches; CLARITY eases the low-mid masking
          that buries the presence (1-5 kHz) when it is there; AIR adds harmonics over the top, made from the
          presence it has uncovered; SPEED how quickly it follows; LISTEN plays only what it brings out (the
          difference); MIX. The level is matched to what came in: it brings out, it doesn't make louder.
        Bit for bit out until powered. Stereo: one analysis (the middle), the same lift on both sides - the image
        stays put.
        State: [0..23] each band's level (0..1, -72..0 dB), [24..47] its masking threshold, [48..71] its lift
        (0..1 of 15 dB), [72] how much is being lifted in all (0..1), [73] the clarity cut (0..1). */
    class DetailEnhancer final : public RackUnit
    {
    public:
        static constexpr int bands = 24;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 74) return 0;
            for (int b = 0; b < bands; ++b)
            {
                o[b] = std::clamp ((levelDb[(size_t) b] + 72.0f) / 72.0f, 0.0f, 1.0f);
                o[bands + b] = std::clamp ((thresholdDb[(size_t) b] + 72.0f) / 72.0f, 0.0f, 1.0f);
                o[2 * bands + b] = std::clamp (liftDb[(size_t) b] / 15.0f, 0.0f, 1.0f);
            }
            o[72] = totalLift;
            o[73] = clarityNow;
            return 74;
        }

    private:
        std::array<std::array<sim::Svf, bands>, 2> bank;
        std::array<float, bands> fc {}, envFast {}, envSlow {}, floorDb {}, levelDb {}, thresholdDb {}, targetDb {}, liftDb {}, gain {};
        std::array<sim::Svf, 2> airHp;
        // the hearing model's ears: a 2048-point spectrum of the middle every 512 samples
        static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, hop = 512;
        juce::dsp::FFT fft { fftOrder };
        std::vector<float> ring = std::vector<float> ((size_t) fftSize, 0.0f), work = std::vector<float> ((size_t) (2 * fftSize), 0.0f),
                           window = std::vector<float> ((size_t) fftSize, 0.0f), binDb = std::vector<float> ((size_t) (fftSize / 2 + 1), -120.0f),
                           binBark = std::vector<float> ((size_t) (fftSize / 2 + 1), 0.0f);
        std::vector<int> binBand = std::vector<int> ((size_t) (fftSize / 2 + 1), 0), peaks;
        std::array<float, bands> hiddenDb {};
        int ringPos = 0, sinceHop = 0;
        float makeup = 1.0f, inPow = 1.0e-9f, outPow = 1.0e-9f, totalLift = 0.0f, clarityNow = 0.0f;
        int tick = 0;

        void prepareUnit (double s, int) override
        {
            // the bands: 24, a critical band apart, 70 Hz to 15 kHz; each as wide as the ear's band there (ERB)
            for (int b = 0; b < bands; ++b)
            {
                fc[(size_t) b] = 70.0f * std::pow (15000.0f / 70.0f, (float) b / (float) (bands - 1));
                const float erb = 24.7f * (4.37f * fc[(size_t) b] / 1000.0f + 1.0f);
                for (auto& ch : bank) ch[(size_t) b].set (s, fc[(size_t) b], std::clamp (fc[(size_t) b] / (1.6f * erb), 0.7f, 6.0f));
            }
            for (auto& f : airHp) f.set (s, 7000.0, 0.7);
            for (int i = 0; i < fftSize; ++i)
                window[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) fftSize);
            for (int k = 0; k <= fftSize / 2; ++k)
            {
                const float f = std::max (1.0f, (float) k * (float) s / (float) fftSize);
                binBark[(size_t) k] = 13.0f * std::atan (0.00076f * f) + 3.5f * std::atan ((f / 7500.0f) * (f / 7500.0f));   // (the Bark scale)
                int best = 0;
                for (int b = 1; b < bands; ++b)
                    if (std::abs (std::log (f / fc[(size_t) b])) < std::abs (std::log (f / fc[(size_t) best]))) best = b;
                binBand[(size_t) k] = best;
            }
            resetUnit();
        }

        void resetUnit() override
        {
            for (auto& ch : bank) for (auto& f : ch) f.reset();
            for (auto& f : airHp) f.reset();
            envFast.fill (0.0f); envSlow.fill (0.0f); floorDb.fill (-90.0f); levelDb.fill (-90.0f); thresholdDb.fill (-90.0f);
            targetDb.fill (0.0f); liftDb.fill (0.0f); gain.fill (0.0f);
            makeup = 1.0f; inPow = outPow = 1.0e-9f; totalLift = clarityNow = 0.0f; tick = 0;
            std::fill (ring.begin(), ring.end(), 0.0f); hiddenDb.fill (0.0f); ringPos = sinceHop = 0;
        }

        /** The ear's resolution: the spectrum's peaks (each a partial - a note's harmonic, a breath's band), and for
            each the masking threshold the louder partials around it put over it - spreading up 10 dB a Bark, down 24
            dB, 10 dB under the masker's own level. A peak under its threshold (and above where hiss lives) is hidden:
            each band keeps how deeply hidden what it holds is. */
        void hearSpectrum() noexcept
        {
            for (int i = 0; i < fftSize; ++i)
                work[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)] * window[(size_t) i];
            std::fill (work.begin() + fftSize, work.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (work.data(), true);
            const int half = fftSize / 2;
            for (int k = 0; k <= half; ++k)
                binDb[(size_t) k] = sim::toDb (work[(size_t) k] * 4.0f / (float) fftSize);   // (a sine's amplitude, Hann-windowed)
            peaks.clear();
            for (int k = 2; k < half - 1; ++k)
                if (binDb[(size_t) k] > -100.0f && binDb[(size_t) k] >= binDb[(size_t) k - 1] && binDb[(size_t) k] > binDb[(size_t) k + 1])
                    peaks.push_back (k);
            // (the loudest few hundred are enough to mask with)
            if (peaks.size() > 160)
            {
                std::nth_element (peaks.begin(), peaks.begin() + 160, peaks.end(), [&] (int a, int b) { return binDb[(size_t) a] > binDb[(size_t) b]; });
                peaks.resize (160);
            }
            std::array<float, bands> deepest {};
            for (int k : peaks)
            {
                const float lv = binDb[(size_t) k];
                if (lv < -66.0f) continue;   // (hiss and silence: never)
                float t = -120.0f;
                for (int m : peaks)
                {
                    if (std::abs (m - k) < 3) continue;   // (itself)
                    const float dz = binBark[(size_t) k] - binBark[(size_t) m];
                    t = std::max (t, binDb[(size_t) m] - 10.0f - (dz > 0.0f ? 10.0f * dz : -24.0f * dz));
                }
                const float depth = t - lv;
                if (depth <= 0.0f) continue;
                // (into the band it is in, and the one on its other side - a partial between two bands is in both)
                const int b = binBand[(size_t) k];
                const float f = (float) k * (float) sr / (float) fftSize;
                const int other = std::clamp (f > fc[(size_t) b] ? b + 1 : b - 1, 0, bands - 1);
                deepest[(size_t) b] = std::max (deepest[(size_t) b], depth);
                const float share = std::abs (std::log (f / fc[(size_t) b])) / std::abs (std::log (fc[(size_t) other] / fc[(size_t) b]) + 1.0e-6f);
                deepest[(size_t) other] = std::max (deepest[(size_t) other], depth * std::clamp (2.0f * share, 0.0f, 1.0f));
            }
            hiddenDb = deepest;
        }

        /** The hearing model, at control rate: levels, the masking threshold each band sits under, and the lift. */
        void analyse (float detail, float depth, float clarity, float speedS, int n) noexcept
        {
            for (int b = 0; b < bands; ++b)
            {
                levelDb[(size_t) b] = sim::toDb (envFast[(size_t) b]);
                // its noise floor: follows the quietest it gets at once, rises back only slowly (1 dB a second) - and
                // never above where hiss lives (-66 dBFS): a held note is music, not noise
                floorDb[(size_t) b] = std::min ({ levelDb[(size_t) b], floorDb[(size_t) b] + 1.0f * (float) n / (float) sr, -66.0f });
            }
            // masking: each band spreads its masking over its neighbours - upward 10 dB a band, downward 24 dB -
            // under its own level by the masking offset; the threshold is the loudest of those (and quiet's own)
            for (int b = 0; b < bands; ++b)
            {
                float t = -96.0f;
                for (int j = 0; j < bands; ++j)
                {
                    if (j == b) continue;
                    const float d = (float) (b - j);
                    t = std::max (t, levelDb[(size_t) j] - 6.0f - (d > 0.0f ? 10.0f * d : -24.0f * d));
                }
                thresholdDb[(size_t) b] = t;
            }
            // CLARITY: when the presence is buried and the low-mids are what buries it, ease the low-mids a little
            float presenceHidden = 0.0f, lowMid = -96.0f;
            for (int b = 0; b < bands; ++b)
            {
                if (fc[(size_t) b] > 1200.0f && fc[(size_t) b] < 5000.0f)
                    presenceHidden = std::max ({ presenceHidden, thresholdDb[(size_t) b] - levelDb[(size_t) b], hiddenDb[(size_t) b] });
                if (fc[(size_t) b] > 150.0f && fc[(size_t) b] < 500.0f)
                    lowMid = std::max (lowMid, levelDb[(size_t) b]);
            }
            clarityNow = clarity * std::clamp (presenceHidden / 9.0f, 0.0f, 1.0f) * (lowMid > -50.0f ? 1.0f : 0.0f);
            float total = 0.0f;
            for (int b = 0; b < bands; ++b)
            {
                const float lv = levelDb[(size_t) b];
                const float hidden = std::min (std::max (hiddenDb[(size_t) b], thresholdDb[(size_t) b] - lv), depth);   // how deeply what it holds is masked
                const float real = hiddenDb[(size_t) b] > 0.0f ? 1.0f : std::clamp ((lv - floorDb[(size_t) b] - 6.0f) / 12.0f, 0.0f, 1.0f);   // (not its noise)
                const float audible = std::clamp ((lv + 70.0f) / 20.0f, 0.0f, 1.0f);                     // (not silence)
                const float rising = std::clamp (sim::toDb ((envFast[(size_t) b] + 1.0e-9f) / (envSlow[(size_t) b] + 1.0e-9f)) / 6.0f, 0.0f, 1.0f);
                float lift = detail * 1.25f * hidden * real * audible * (1.0f + 0.5f * rising);
                lift = std::min (lift, 15.0f);
                if (fc[(size_t) b] > 150.0f && fc[(size_t) b] < 500.0f)
                    lift -= clarityNow * 3.5f;
                targetDb[(size_t) b] = lift;
                total += std::max (0.0f, lift);
            }
            totalLift = std::clamp (total / (bands * 3.0f), 0.0f, 1.0f);
            const float k = sim::coef (sr / (double) std::max (1, n), speedS);   // (the gains ease there, per control tick)
            for (int b = 0; b < bands; ++b)
                liftDb[(size_t) b] += k * (targetDb[(size_t) b] - liftDb[(size_t) b]);
        }

        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float detail = sim::knob10 (p[1]), depth = std::clamp (p[2], 0.0f, 24.0f), clarity = sim::knob10 (p[3]);
            const float air = sim::knob10 (p[4]), speed = sim::knob10 (p[5]), mix = sim::mix100 (p[7]);
            const bool listen = p[6] > 0.5f;
            const float speedS = 0.6f - 0.55f * speed;                    // (0.6 s .. 50 ms to follow)
            const float fastUp = sim::coef (sr, 0.002), fastDown = sim::coef (sr, 0.03 + 0.12 * (1.0 - speed));
            const float slowUp = sim::coef (sr, 0.08), slowDown = sim::coef (sr, 0.5);
            constexpr int control = 32;
            std::array<float, bands> g {};
            for (int b = 0; b < bands; ++b) g[(size_t) b] = sim::db (liftDb[(size_t) b]) - 1.0f;
            const float powK = sim::coef (sr, 1.0);
            for (int i = 0; i < n; ++i)
            {
                const float in[2] { io[0][i], io[1][i] };
                ring[(size_t) ringPos] = 0.5f * (in[0] + in[1]);
                ringPos = (ringPos + 1) % fftSize;
                if (++sinceHop >= hop * std::max (1, (int) std::lround (sr / 48000.0)))
                {
                    sinceHop = 0;
                    hearSpectrum();
                }
                float out[2] { in[0], in[1] }, presence[2] {};
                for (int b = 0; b < bands; ++b)
                {
                    float lp, hp, bp[2];
                    for (int c = 0; c < 2; ++c) bp[c] = bank[(size_t) c][(size_t) b].process (in[c], lp, hp);
                    const float m = std::abs (0.5f * (bp[0] + bp[1]));
                    float& ef = envFast[(size_t) b];
                    ef += (m > ef ? fastUp : fastDown) * (m - ef);
                    float& es = envSlow[(size_t) b];
                    es += (m > es ? slowUp : slowDown) * (m - es);
                    // the lift: the band added back in by however much it is to come up (exact passthrough at 0 dB)
                    gain[(size_t) b] += 0.002f * (g[(size_t) b] - gain[(size_t) b]);
                    for (int c = 0; c < 2; ++c)
                    {
                        out[c] += gain[(size_t) b] * bp[c];
                        if (fc[(size_t) b] > 1500.0f && fc[(size_t) b] < 6000.0f && liftDb[(size_t) b] > 0.0f)
                            presence[c] += bp[c] * std::min (1.0f, liftDb[(size_t) b] / 6.0f);
                    }
                }
                // AIR: harmonics over the top, from the presence it uncovered (a soft, even curve, then only its top)
                for (int c = 0; c < 2; ++c)
                {
                    float lp, hp;
                    const float h = presence[c] * std::abs (presence[c]) * 4.0f;
                    airHp[(size_t) c].process (h, lp, hp);
                    out[c] += air * 0.6f * std::tanh (hp);
                }
                // the level matched to what came in (over a second, within +-6 dB)
                inPow += powK * (0.5f * (in[0] * in[0] + in[1] * in[1]) - inPow);
                outPow += powK * (0.5f * (out[0] * out[0] + out[1] * out[1]) - outPow);
                for (int c = 0; c < 2; ++c)
                {
                    const float y = out[c] * makeup;
                    const float wet = listen ? 2.0f * (y - in[c]) : y;
                    io[c][i] = in[c] + mix * (wet - in[c]);
                }
                if (++tick >= control)
                {
                    tick = 0;
                    analyse (detail, depth, clarity, speedS, control);
                    for (int b = 0; b < bands; ++b) g[(size_t) b] = sim::db (liftDb[(size_t) b]) - 1.0f;
                    const float want = std::clamp (std::sqrt ((inPow + 1.0e-12f) / (outPow + 1.0e-12f)), sim::db (-6.0f), sim::db (6.0f));
                    makeup += 0.02f * (want - makeup);
                }
            }
            setMeter (totalLift);
        }
    };
}
