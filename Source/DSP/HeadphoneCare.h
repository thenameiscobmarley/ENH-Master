#pragma once

#include <array>
#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** LONG SESSIONS (OUTPUT MONITOR): against listening fatigue.

        What tires the ear over an hour is not one loud sound but a forward balance: too much of the
        sound in 2 - 6 kHz, where the ear is most sensitive, against the body of it (200 Hz - 2 kHz). This
        follows that balance over several seconds and, when it is more forward than a well-balanced
        mix (pink noise sits at about -3 dB; most finished music at -6 to -10 dB), eases a broad bell at
        3.4 kHz and a gentle shelf above 7 kHz down - by at most 1.5 dB (GENTLE) or 3 dB (CARE).

        It moves over tens of seconds, so it is never heard moving: no pumping, no dulling of a
        gunshot or a hi-hat, only a balance that stops leaning forward. Material that is already
        balanced is left exactly as it is. Two SVF EQ bands per channel (their gain can glide without
        clicks), no latency. */
    class SessionCare
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            presHp = BiquadCoeffs::highPass (sr, 2000.0, 0.707);
            presLp = BiquadCoeffs::lowPass (sr, 6000.0, 0.707);
            bodyHp = BiquadCoeffs::highPass (sr, 200.0, 0.707);
            bodyLp = BiquadCoeffs::lowPass (sr, 2000.0, 0.707);
            reset();
        }

        void reset() noexcept
        {
            for (auto& s : detect) s.reset();
            for (auto& c : eq) for (auto& s : c) s.reset();
            presPow = bodyPow = 0.0f;
            cutDb = appliedDb = 0.0f;
            design (0.0f);
        }

        float getReductionDb() const noexcept { return appliedDb; }

        /** mode: 0 off, 1 gentle (1.5 dB at most), 2 care (3 dB). */
        void process (float* const* ch, int numChannels, int n, int mode) noexcept
        {
            if (mode == 0 && appliedDb == 0.0f)
                return;
            const int nc = std::min (numChannels, 2);
            const float maxDb = mode == 2 ? 3.0f : mode == 1 ? 1.5f : 0.0f;

            // The balance, over ~3 s, of the mid signal
            const float a = 1.0f - std::exp (-1.0f / (3.0f * (float) sr));
            for (int i = 0; i < n; ++i)
            {
                const float m = nc > 1 ? 0.5f * (ch[0][i] + ch[1][i]) : ch[0][i];
                const float p = detect[1].process (presLp, detect[0].process (presHp, m));
                const float b = detect[3].process (bodyLp, detect[2].process (bodyHp, m));
                presPow += a * (p * p - presPow);
                bodyPow += a * (b * b - bodyPow);
            }
            // How far past a balanced mix (-6 dB) it leans, half of that taken off; nothing in silence
            const float blockS = (float) n / (float) sr;
            float want = 0.0f;
            if (bodyPow > 1.0e-7f && mode > 0)
            {
                const float ratioDb = 10.0f * std::log10 ((presPow + 1.0e-12f) / bodyPow);
                want = std::clamp ((ratioDb + 6.0f) * 0.5f, 0.0f, maxDb);
            }
            // Tens of seconds each way (and let go at once when switched off: over 2 s)
            const float tau = mode == 0 ? 2.0f : want > cutDb ? 20.0f : 30.0f;
            cutDb += (want - cutDb) * (1.0f - std::exp (-blockS / tau));
            if (mode == 0 && cutDb < 0.01f)
                cutDb = 0.0f;
            if (std::abs (cutDb - appliedDb) > 0.005f || (cutDb == 0.0f && appliedDb != 0.0f))
                design (cutDb);
            if (appliedDb == 0.0f)
                return;
            for (int c = 0; c < nc; ++c)
                for (int i = 0; i < n; ++i)
                    ch[c][i] = eq[(size_t) c][1].process (shelf, eq[(size_t) c][0].process (bell, ch[c][i]));
        }

    private:
        void design (float db) noexcept
        {
            appliedDb = db;
            bell = SvfEqCoeffs::bell (sr, 3400.0, 0.8, -db);
            shelf = SvfEqCoeffs::highShelf (sr, 7000.0, 0.7, -0.6 * db);
        }

        double sr = 48000.0;
        BiquadCoeffs presHp, presLp, bodyHp, bodyLp;
        std::array<BiquadState, 4> detect {};
        float presPow = 0.0f, bodyPow = 0.0f, cutDb = 0.0f, appliedDb = 0.0f;
        SvfEqCoeffs bell, shelf;
        std::array<std::array<SvfEqState, 2>, 2> eq {};
    };

    /** HEADPHONE ROOM (OUTPUT MONITOR): the sound in front of you instead of inside your head.

        On headphones each ear hears only its own channel, with no room: the brain places that inside the
        head. A real room adds early reflections, each arriving at the far ear a little later and duller
        (the head's shadow). This adds eight such reflections per ear from a small room (NEAR) or a
        larger one (ROOM), 14 - 28 dB down, high-passed so they never thicken the bass, with a hair
        different timing for each ear so they decorrelate. The direct sound is untouched, so direction
        and the timing of footsteps stay exactly where they were.

        Delay lines and a few one-pole filters: next to no CPU, no latency. Fades in and out over 30 ms. */
    class HeadphoneRoom
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate;
            size = (int) std::ceil (0.035 * sr) + 4;
            for (auto& d : line) d.assign ((size_t) size, 0.0f);
            shadowK = 1.0f - (float) std::exp (-2.0 * pi * 2600.0 / sr);   // the head's shadow on the far ear
            lowCutK = (float) std::exp (-2.0 * pi * 220.0 / sr);           // reflections above ~220 Hz only
            reset();
        }

        void reset() noexcept
        {
            for (auto& d : line) std::fill (d.begin(), d.end(), 0.0f);
            pos = 0;
            shadow = {};
            lowX = lowY = {};
            fade = 0.0f;
        }

        /** mode: 0 off, 1 near (a small room), 2 room (a larger one). */
        void process (float* const* ch, int numChannels, int n, int mode) noexcept
        {
            // A change of room fades the old one out first, then the new one in: never a jump
            if (mode > 0 && fade == 0.0f)
                active = mode;
            const float want = mode > 0 && mode == active ? 1.0f : 0.0f;
            if ((fade == 0.0f && want == 0.0f) || numChannels < 2 || size == 0)
                return;
            // ms, dB, 1 = from the far side (reaches this ear through the head's shadow)
            struct Tap { float ms, db; int far; };
            // (the same-side reflections a few dB under the far-side ones: arriving unshadowed, they comb the
            // sound more - EnhAudioLab's resonance figure went up 0.6 dB with them level)
            static constexpr std::array<Tap, 8> near {{ { 2.3f, -18.0f, 0 }, { 3.1f, -15.0f, 1 }, { 5.7f, -17.0f, 1 }, { 6.9f, -21.0f, 0 },
                                                        { 9.4f, -20.0f, 1 }, { 12.1f, -25.0f, 0 }, { 15.8f, -24.0f, 1 }, { 19.5f, -28.0f, 0 } }};
            static constexpr std::array<Tap, 8> room {{ { 4.1f, -16.0f, 0 }, { 5.3f, -14.0f, 1 }, { 8.9f, -16.0f, 1 }, { 11.6f, -20.0f, 0 },
                                                        { 15.2f, -19.0f, 1 }, { 19.7f, -24.0f, 0 }, { 24.3f, -23.0f, 1 }, { 28.9f, -27.0f, 0 } }};
            const auto& taps = active == 2 ? room : near;
            std::array<std::array<int, 8>, 2> delay {};
            std::array<float, 8> gain {};
            float power = 1.0f;
            for (size_t t = 0; t < taps.size(); ++t)
            {
                gain[t] = dbToGain (taps[t].db);
                power += gain[t] * gain[t];
                for (int e = 0; e < 2; ++e)   // the right ear's room a hair larger: the two ears decorrelate
                    delay[(size_t) e][t] = std::min (size - 1, (int) std::lround (taps[t].ms * (e == 0 ? 1.0f : 1.07f) * 0.001 * sr));
            }
            const float norm = 1.0f / std::sqrt (power);   // as loud with the room as without
            const float step = (float) (1.0 / (0.030 * sr));

            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + step) : std::max (want, fade - step);
                line[0][(size_t) pos] = ch[0][i];
                line[1][(size_t) pos] = ch[1][i];
                for (int e = 0; e < 2; ++e)
                {
                    float same = 0.0f, other = 0.0f;
                    for (size_t t = 0; t < taps.size(); ++t)
                    {
                        const int src = taps[t].far ? 1 - e : e;
                        const int at = (pos - delay[(size_t) e][t] + size) % size;
                        (taps[t].far ? other : same) += gain[t] * line[(size_t) src][(size_t) at];
                    }
                    shadow[(size_t) e] += shadowK * (other - shadow[(size_t) e]);
                    const float refl = same + shadow[(size_t) e];
                    // one-pole high-pass: the room never thickens the bass
                    const float hp = lowCutK * (lowY[(size_t) e] + refl - lowX[(size_t) e]);
                    lowX[(size_t) e] = refl;
                    lowY[(size_t) e] = hp;
                    const float x = ch[e][i];
                    ch[e][i] = x + fade * ((x + hp) * norm - x);
                }
                pos = (pos + 1) % size;
            }
        }

    private:
        double sr = 48000.0;
        int size = 0, pos = 0, active = 1;
        std::array<std::vector<float>, 2> line;
        std::array<float, 2> shadow {}, lowX {}, lowY {};
        float shadowK = 0.3f, lowCutK = 0.97f, fade = 0.0f;
    };
}
