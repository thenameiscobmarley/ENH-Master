#pragma once

#include "Sims3.h"

/*  Two units with colour screens (UI/Scene/ColourScreens.h draws them):
      CHROMA SPACE  a master of space and tone - a lush, modulated eight-line space with an octave shimmer when
                    pushed, a tone stage from warm to airy - and CHOP: pieces of what just played, captured in
                    memory only (nothing is ever written anywhere), played back on the track's own hits and
                    retuned to follow its melody
      HYPERCUBE     a visualiser: the sound's bands, its stereo trace and a fingerprint of the track, published for a
                    cube drawn as a scope draws, warped by what plays (it passes the sound untouched) */
namespace enh::dsp::units
{
    // ------------------------------------------------------------------------------------------------
    /** CHROMA SPACE. SPACE: size, length and (past 6) an octave shimmer in the tail; TONE: -10 warm and dark
        (lows up, warmth in them) to +10 bright and airy (top up, air over it) - on the whole sound; WIDTH: the
        space's width; CHOP: how loud the chops play (0: none captured); SIZE: each chop's length; MIX: the space.
        Chops: when the track hits, the next SIZE of it is copied into a bank of six (in memory, overwritten as
        it goes - never saved); on each later hit (or when the melody moves a semitone) one of them plays,
        resampled from the pitch it was captured at to the pitch playing now.
        State: [0] 48, [1..144] 48 x (hue, warmth, level) oldest first; [145] chops n, [146..169] 6 x (age s,
        length s, hue, playing 0..1); [170] hue now, [171] warmth now, [172] space, [173] tone -1..1, [174] chop. */
    class ChromaSpace final : public RackUnit
    {
    public:
        static constexpr int lines = 8, bankSize = 6, hist = 48;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 175) return 0;
            o[0] = (float) hist;
            for (int i = 0; i < hist; ++i) { const int k = (histHead + i) % hist; o[1 + 3 * i] = hHue[(size_t) k]; o[2 + 3 * i] = hWarm[(size_t) k]; o[3 + 3 * i] = hLevel[(size_t) k]; }
            int n = 0;
            for (int c = 0; c < bankSize; ++c)
                if (bank[(size_t) c].length > 0)
                {
                    o[146 + 4 * n] = bank[(size_t) c].age; o[147 + 4 * n] = (float) bank[(size_t) c].length / (float) sr;
                    o[148 + 4 * n] = bank[(size_t) c].hue; o[149 + 4 * n] = bank[(size_t) c].flash; ++n;
                }
            o[145] = (float) n;
            o[170] = hueNow; o[171] = warmNow; o[172] = spaceN; o[173] = toneN; o[174] = chopN;
            return 175;
        }

    private:
        struct Chop { std::vector<float> audio; int length = 0; float pitch = 0.0f, hue = 0.0f, age = 0.0f, flash = 0.0f; };
        struct Voice { int chop = -1; double pos = 0.0; float ratio = 1.0f, pan = 0.0f; bool on = false, reverse = false; int stutterLen = 0, stutterLeft = 0; };
        // the space
        std::array<DelayLine, lines> fdn; std::array<float, lines> damp {}, lfo {}, lenNow {}; std::array<float, 2> pre {}; bool lenSet = false;
        DelayLine preL, preR, shim; float shimPh = 0.0f;
        // the tone
        std::array<float, 2> toneLp {}, airHp {}; std::array<kit::SoftSat, 2> warmSat {}, airSat {}; std::array<kit::Level, 2> lvl {};
        // the chops (all in memory)
        std::vector<float> ring; int ringW = 0; std::array<Chop, bankSize> bank; int nextSlot = 0, capturing = -1, captureFrom = 0, captureLen = 0;
        std::array<Voice, 2> voices; int nextVoice = 0, nextChop = 0;
        sim::Env fast, slow; float refractory = 0.0f, lastPlayedPitch = 0.0f;
        // the timing: the hits' tempo (for the grid and AUTO LENGTH), how busy it is, when a chop last played
        std::array<double, 8> hitTimes {}; int hitCount = 0; double clock = 0.0; float beat = 0.0f, busy = 0.0f;
        float sinceHit = 1.0e9f, sinceTrigger = 1.0e9f, gridPhase = 0.0f, followRatioNote = 0.0f; sim::Rng rng;
        sim::Env act, actSlow; float spaceDrift = 0.0f;
        // the pitch (every ~40 ms, on a quarter-rate copy)
        std::vector<float> dec; int decW = 0, decAcc = 0; float decSum = 0.0f, pitchNow = 0.0f, pitchConf = 0.0f; int sinceTrack = 0;
        // the colour
        float hiMs = 0.0f, allMs = 1.0e-9f; std::array<float, 2> centLp {};
        std::array<float, hist> hHue {}, hWarm {}, hLevel {}; int histHead = 0; float histT = 0.0f, lvPeak = 0.0f;
        float hueNow = 0.0f, warmNow = 0.5f, spaceN = 0.0f, toneN = 0.0f, chopN = 0.0f;

        static constexpr float baseMs[lines] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.9f, 73.1f };

        void prepareUnit (double s, int) override
        {
            for (auto& d : fdn) d.setMax ((int) (0.17 * s) + 16);
            preL.setMax ((int) (0.06 * s) + 8); preR.setMax ((int) (0.06 * s) + 8); shim.setMax ((int) (0.12 * s) + 8);
            for (auto& l : lvl) l.setup (s, 0.3);
            ring.assign ((size_t) (4.0 * s), 0.0f);
            for (auto& c : bank) c.audio.assign ((size_t) (0.62 * s), 0.0f);
            dec.assign (2048, 0.0f);
            fast.setup (s, 0.001, 0.03); slow.setup (s, 0.03, 0.3);
            act.setup (s, 0.02, 0.15); actSlow.setup (s, 1.0, 3.0);
            for (int i = 0; i < lines; ++i) lfo[(size_t) i] = 0.13f * (float) i;
        }
        void resetUnit() override
        {
            for (auto& d : fdn) d.clear(); preL.clear(); preR.clear(); shim.clear();
            damp = {}; pre = {}; lenSet = false; toneLp = airHp = {}; centLp = {}; for (auto& x : warmSat) x.reset(); for (auto& x : airSat) x.reset(); for (auto& l : lvl) l.reset();
            std::fill (ring.begin(), ring.end(), 0.0f); ringW = 0;
            for (auto& c : bank) { c.length = 0; c.flash = 0.0f; }
            for (auto& v : voices) v.on = false;
            capturing = -1; fast.v = slow.v = 0.0f; refractory = 0.0f; pitchNow = pitchConf = 0.0f; decW = decAcc = 0; decSum = 0.0f;
            hHue = hWarm = hLevel = {}; hiMs = 0.0f; allMs = 1.0e-9f; lvPeak = 0.0f;
            hitCount = 0; clock = 0.0; beat = 0.0f; busy = 0.0f; sinceHit = sinceTrigger = 1.0e9f; gridPhase = 0.0f; act.v = actSlow.v = 0.0f; spaceDrift = 0.0f;
        }

        /** The pitch of the last ~85 ms: normalised autocorrelation on the quarter-rate copy (70 Hz - 1 kHz). */
        void trackPitch() noexcept
        {
            const float dsr = (float) sr / 4.0f;
            const int win = 512, lo = std::max (2, (int) (dsr / 1000.0f)), hi = std::min (700, (int) (dsr / 70.0f));
            const int n = (int) dec.size();
            auto at = [&] (int back) { return dec[(size_t) ((decW - 1 - back + 4 * n) % n)]; };
            float e0 = 0.0f; for (int i = 0; i < win; ++i) e0 += at (i) * at (i);
            if (e0 < 1.0e-6f) { pitchConf *= 0.8f; return; }
            float best = 0.0f; int bestLag = 0; float rPrev = 0.0f, rBest[3] {};
            for (int lag = lo; lag <= hi; ++lag)
            {
                float c = 0.0f, e1 = 0.0f;
                for (int i = 0; i < win; i += 2) { const float b = at (i + lag); c += at (i) * b; e1 += b * b; }
                const float r = c / std::sqrt (0.5f * e0 * e1 + 1.0e-12f);
                if (r > best && r > rPrev) { best = r; bestLag = lag; }
                rPrev = r;
            }
            (void) rBest;
            if (best > 0.55f && bestLag > 0) { const float f = dsr / (float) bestLag; pitchNow = pitchConf > 0.3f && std::abs (std::log2 (f / std::max (1.0f, pitchNow))) < 0.04f ? pitchNow + 0.5f * (f - pitchNow) : f; pitchConf = best; }
            else pitchConf *= 0.85f;
        }

        /** The note playing now, snapped to the nearest semitone (the chops follow it exactly; they glide as it slides). */
        float noteNow() const noexcept
        {
            if (pitchConf <= 0.4f || pitchNow <= 0.0f) return 0.0f;
            return 440.0f * std::pow (2.0f, std::round (12.0f * std::log2 (pitchNow / 440.0f)) / 12.0f);
        }

        /** A chop plays: the next in the bank, retuned to the note now; forwards, reversed or stuttered (how
            often the tricks come: `variety`). */
        void trigger (float variety) noexcept
        {
            int have = 0; for (auto& c : bank) have += c.length > 0 ? 1 : 0;
            if (have == 0 || chopN <= 0.0f) return;
            for (int tries = 0; tries < bankSize; ++tries) { nextChop = (nextChop + 1) % bankSize; if (bank[(size_t) nextChop].length > 0) break; }
            auto& c = bank[(size_t) nextChop];
            auto& v = voices[(size_t) nextVoice]; nextVoice = (nextVoice + 1) % 2;
            v.chop = nextChop; v.pos = 0.0; v.on = true; v.pan = (nextVoice == 0 ? -0.45f : 0.45f);
            const float note = noteNow();
            v.ratio = note > 0.0f && c.pitch > 0.0f ? std::clamp (note / c.pitch, 0.5f, 2.0f) : 1.0f;   // (retuned to the note playing now)
            const float roll = rng.next();
            v.reverse = roll < 0.45f * variety;
            v.stutterLen = 0; v.stutterLeft = 0;
            if (! v.reverse && roll < 0.80f * variety)   // (a stutter: its first eighth repeated two to four times)
            {
                v.stutterLen = std::max (64, c.length / 8);
                v.stutterLeft = 2 + (int) (rng.next() * 3.0f);
            }
            c.flash = 1.0f;
            lastPlayedPitch = pitchNow;
        }

        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float spaceKnob = sim::knob10 (p[1]), tone = std::clamp (p[2], -10.0f, 10.0f) / 10.0f, width = sim::knob10 (p[3]);
            const float chop = sim::knob10 (p[4]), sizeS = std::clamp (p[5], 60.0f, 600.0f) / 1000.0f, variety = sim::knob10 (p[6]);
            const float autoAmt = sim::knob10 (p[7]), mix = sim::mix100 (p[8]);
            const bool autoTime = p[9] > 0.5f, autoTricks = p[10] > 0.5f, autoLen = p[11] > 0.5f, autoSpace = p[12] > 0.5f;
            // AUTO SPACE: it opens in the gaps and draws in when the track is busy (as far as AUTO AMT says)
            const float drift = autoSpace ? std::clamp ((actSlow.v > 1.0e-4f ? (act.v < 0.35f * actSlow.v ? 0.25f : act.v > 1.4f * actSlow.v ? -0.2f : 0.0f) : 0.0f) * autoAmt, -0.2f, 0.25f) : 0.0f;
            spaceDrift += (1.0f - std::exp (-(float) n / (0.6f * (float) sr))) * (drift - spaceDrift);
            const float space = std::clamp (spaceKnob + spaceDrift, 0.0f, 1.0f);
            spaceN = space; toneN = tone; chopN = chop;
            // the space's shape: 0 - 5 a room that sits in a mix; 6 - 10 it grows into a vast, shimmering tail
            const float lowPart = std::min (1.0f, space / 0.5f), highPart = std::max (0.0f, (space - 0.5f) / 0.5f);
            const float scale = 0.45f + 0.45f * lowPart + 1.1f * highPart;                      // (line lengths: 0.45 .. 0.9 .. 2.0)
            const float rt60 = 0.35f + 1.9f * lowPart * lowPart + 7.5f * highPart * highPart;   // (about 2.3 s at 5, ~12 s at 10 with the shimmer: it always dies away)
            const float dampK = sim::lp1K (sr, 2500.0 + 9000.0 * (0.5 + 0.5 * tone) + 3000.0 * (1.0 - space) + 5000.0 * highPart);   // (the vast end: a brighter, longer air)
            std::array<float, lines> len {}, g {};
            for (int i = 0; i < lines; ++i) { len[(size_t) i] = baseMs[i] * 0.001f * scale * (float) sr; g[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / (rt60 * (float) sr)); }
            const float shimAmt = highPart > 0.2f ? (highPart - 0.2f) / 0.8f * 0.32f : 0.0f, keepAmt = std::sqrt (1.0f - shimAmt * shimAmt);
            const float modDepth = (0.0005f + 0.0006f * lowPart + 0.0014f * highPart) * (float) sr;
            // the tricks: VARIETY, or - AUTO TRICKS - more of them the busier the track is
            const float tricks = autoTricks ? autoAmt * (0.25f + 0.75f * busy) : variety;
            // the chop's length: SIZE, or - AUTO LENGTH - an eighth note at the track's tempo
            const float lenS = autoLen && beat > 0.0f ? std::clamp (0.5f * beat, 0.06f, 0.6f) : sizeS;
            followRatioNote = noteNow();
            const float wid = 0.4f + 1.2f * width;
            // the tone: a tilt about 900 Hz, warmth into the lows or air over the top
            const float tk = sim::lp1K (sr, 900.0), ak = sim::lp1K (sr, 6000.0);
            const float gLow = sim::db (-4.5f * tone), gHigh = sim::db (4.5f * tone);
            const float warm = std::max (0.0f, -tone), airy = std::max (0.0f, tone);
            const int chopLen = std::clamp ((int) (lenS * (float) sr), 64, (int) bank[0].audio.size());
            const float minGap = std::max (0.18f * (float) sr, 1.5f * (float) chopLen);   // (never busy: a chop plays at most every so often)
            const int rn = (int) ring.size();
            const float hk = sim::lp1K (sr, 2000.0), mk = sim::coef (sr, 0.15);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float xl = io[0][i], xr = io[1][i], m = 0.5f * (xl + xr);
                // --- what it is hearing: the ring (memory only), the quarter-rate copy for the pitch, the colour
                ring[(size_t) ringW] = m; ringW = (ringW + 1) % rn;
                decSum += m; if (++decAcc >= 4) { dec[(size_t) decW] = decSum * 0.25f; decW = (decW + 1) % (int) dec.size(); decAcc = 0; decSum = 0.0f; }
                centLp[0] += hk * (m - centLp[0]); const float hiPart = m - centLp[0];
                hiMs += mk * (hiPart * hiPart - hiMs); allMs += mk * (m * m - allMs);
                lvPeak = std::max (lvPeak, std::abs (m));
                // --- the chops: a hit starts a capture (if it is on), the capture finished goes to the bank
                const float f = fast.process (m), sl = slow.process (m);
                const bool hit = refractory <= 0.0f && f > 1.8f * sl && f > 0.02f;
                if (hit) refractory = 0.12f * (float) sr; else refractory -= 1.0f;
                act.process (m); actSlow.process (m);
                clock += 1.0; sinceHit += 1.0f; sinceTrigger += 1.0f;
                if (hit)
                {
                    hitTimes[(size_t) (hitCount++ % 8)] = clock / sr; sinceHit = 0.0f; gridPhase = 0.0f;
                    // the tempo: the median gap between the last hits, folded into 0.35 - 0.8 s a beat
                    std::array<float, 7> gaps {}; int ng = 0;
                    for (int k = 1; k < std::min (hitCount, 8); ++k)
                    {
                        const double a = hitTimes[(size_t) ((hitCount - k) % 8)], b = hitTimes[(size_t) ((hitCount - k - 1) % 8)];
                        const float gp = (float) (a - b); if (gp > 0.12f && gp < 1.5f && ng < (int) gaps.size()) gaps[(size_t) ng++] = gp;
                    }
                    if (ng >= 3)
                    {
                        std::sort (gaps.begin(), gaps.begin() + ng);
                        float bt = gaps[(size_t) (ng / 2)];
                        while (bt < 0.35f) bt *= 2.0f; while (bt > 0.8f) bt *= 0.5f;
                        beat = beat > 0.0f ? beat + 0.3f * (bt - beat) : bt;
                    }
                    int recent = 0; for (int k = 0; k < std::min (hitCount, 8); ++k) recent += clock / sr - hitTimes[(size_t) k] < 2.0 ? 1 : 0;
                    busy = std::clamp ((float) recent / 8.0f, 0.0f, 1.0f);
                }
                if (chop > 0.0f)
                {
                    if (hit && capturing < 0) { capturing = nextSlot; nextSlot = (nextSlot + 1) % bankSize; captureFrom = (ringW - 1 - (int) (0.01 * sr) + rn) % rn; captureLen = 0; }   // (from just before the hit was heard)
                    if (capturing >= 0 && ++captureLen >= chopLen)
                    {
                        auto& c = bank[(size_t) capturing];
                        for (int k = 0; k < chopLen; ++k) c.audio[(size_t) k] = ring[(size_t) ((captureFrom + k) % rn)];
                        c.length = chopLen; c.pitch = pitchConf > 0.4f ? pitchNow : 0.0f; c.age = 0.0f; c.hue = hueNow;
                        capturing = -1;
                    }
                    // when one plays: on the track's hits (not every one); AUTO TIMING - also on its beat grid (eighths,
                    // sixteenths when it is busy) through the gaps between hits, more of them with AUTO AMT
                    if (sinceTrigger >= minGap)
                    {
                        bool fire = false;
                        if (hit) fire = rng.next() < (autoTime ? 0.3f + 0.4f * autoAmt * (1.0f - 0.5f * busy) : 0.5f);
                        else if (autoTime && beat > 0.0f && sinceHit > 0.75f * beat * (float) sr)
                        {
                            const float grid = beat * (float) sr * (busy > 0.6f && autoAmt > 0.6f ? 0.25f : 0.5f);
                            gridPhase += 1.0f;
                            if (gridPhase >= grid) { gridPhase -= grid; fire = rng.next() < 0.25f + 0.4f * autoAmt; }
                        }
                        if (fire) { trigger (tricks); sinceTrigger = 0.0f; }
                    }
                }
                // --- the voices: each chop resampled, a quick fade in and out
                float vl = 0.0f, vr = 0.0f;
                for (auto& v : voices)
                {
                    if (! v.on) continue;
                    const auto& c = bank[(size_t) v.chop];
                    // a stutter: its first slice again, and again
                    if (v.stutterLeft > 0 && v.pos >= (double) v.stutterLen) { v.pos -= (double) v.stutterLen; --v.stutterLeft; }
                    const int i0 = (int) v.pos; const float fr = (float) (v.pos - (double) i0);
                    if (i0 + 1 >= c.length) { v.on = false; continue; }
                    const int a0 = v.reverse ? c.length - 1 - i0 : i0, a1 = v.reverse ? std::max (0, a0 - 1) : a0 + 1;   // (reversed: read from its end)
                    const float s = c.audio[(size_t) a0] + fr * (c.audio[(size_t) a1] - c.audio[(size_t) a0]);
                    const float t = (float) v.pos / (float) c.length;
                    float env = std::min ({ 1.0f, t * 40.0f, (1.0f - t) * 12.0f });
                    if (v.stutterLeft > 0) { const float ts = (float) v.pos / (float) v.stutterLen; env = std::min ({ env, ts * 25.0f + 0.02f, (1.0f - ts) * 25.0f }); }
                    // following the melody: the note now (snapped), glided to over about 10 ms
                    if (followRatioNote > 0.0f && c.pitch > 0.0f) v.ratio += 0.0021f * (std::clamp (followRatioNote / c.pitch, 0.5f, 2.0f) - v.ratio);
                    const float y = s * env * chop * 0.9f;
                    vl += y * (1.0f - v.pan); vr += y * (1.0f + v.pan);
                    v.pos += (double) v.ratio;
                }
                // --- the space: pre-delay, eight modulated lines, mixed (Hadamard), damped, shimmered
                preL.push (xl + 0.5f * vl); preR.push (xr + 0.5f * vr);
                const float il = preL.tap (0.022f * (float) sr), ir = preR.tap (0.029f * (float) sr);
                std::array<float, lines> y {};
                for (int k = 0; k < lines; ++k)
                {
                    lfo[(size_t) k] += (0.07f + 0.05f * (float) k) / (float) sr; if (lfo[(size_t) k] >= 1.0f) lfo[(size_t) k] -= 1.0f;
                    if (! lenSet) lenNow[(size_t) k] = len[(size_t) k];
                    lenNow[(size_t) k] += std::clamp (len[(size_t) k] - lenNow[(size_t) k], -0.4f, 0.4f);   // (SPACE moved: the lines stretch, never jump)
                    const float d = lenNow[(size_t) k] + modDepth * (1.0f + std::sin (6.2832f * lfo[(size_t) k]));
                    float v = fdn[(size_t) k].tap (d);
                    damp[(size_t) k] += dampK * (v - damp[(size_t) k]);
                    y[(size_t) k] = damp[(size_t) k] * g[(size_t) k];
                }
                lenSet = true;
                // Hadamard 8 (fast Walsh-Hadamard, energy kept)
                for (int h = 1; h < lines; h <<= 1)
                    for (int a = 0; a < lines; a += h << 1)
                        for (int b = a; b < a + h; ++b) { const float u = y[(size_t) b], w = y[(size_t) (b + h)]; y[(size_t) b] = u + w; y[(size_t) (b + h)] = u - w; }
                for (auto& v : y) v *= 0.35355339f;
                // the shimmer: line 0's return, an octave up (two grains crossfading), fed back in
                float sh = 0.0f;
                if (shimAmt > 0.0f)
                {
                    shim.push (y[0]);
                    shimPh += 1.0f / (0.05f * (float) sr); if (shimPh >= 1.0f) shimPh -= 1.0f;
                    const float win = 0.05f * (float) sr;
                    for (int gi = 0; gi < 2; ++gi) { const float ph = std::fmod (shimPh + 0.5f * (float) gi, 1.0f); sh += std::sin (3.14159f * ph) * shim.tap (2.0f + win * (1.0f - ph)); }
                    sh *= 0.7071f;   // (two grains a half apart: their windows sum to as much as 1.41)
                }
                // the shimmer takes the place of part of what recirculates (never added on top: the loop's gain stays
                // under 1), and anything loud in the tank is eased (it can never run away)
                for (int k = 0; k < lines; ++k)
                {
                    float v = (k < 2 ? 0.94f * (y[(size_t) k] * keepAmt + shimAmt * sh) : y[(size_t) k]) + ((k & 1) ? ir : il) * ((k & 2) ? -0.5f : 0.5f);   // (a power-keeping crossfade, a little lost each pass: the tail always falls)
                    v = v / (1.0f + 0.3f * v * v);
                    fdn[(size_t) k].push (v);
                }
                float wl = (y[0] + y[2] + y[4] + y[6]) * 0.6f, wr = (y[1] + y[3] + y[5] + y[7]) * 0.6f;
                const float wm = 0.5f * (wl + wr), ws = 0.5f * (wl - wr) * wid;
                wl = wm + ws; wr = wm - ws;
                // --- together, then the tone over all of it
                float outs[2] { xl + mix * wl + 0.6f * vl, xr + mix * wr + 0.6f * vr };
                for (int c = 0; c < 2; ++c)
                {
                    float o = outs[c];
                    toneLp[(size_t) c] += tk * (o - toneLp[(size_t) c]);
                    const float lo = toneLp[(size_t) c], hi2 = o - lo;
                    o = lo * gLow + hi2 * gHigh;
                    const float lv = lvl[(size_t) c].process (o);
                    if (warm > 0.0f) o += warm * 0.8f * kit::harmonicsAt (warmSat[(size_t) c], lo, lv, 0.9f, 0.2f);
                    if (airy > 0.0f)
                    {
                        airHp[(size_t) c] += ak * (o - airHp[(size_t) c]);
                        o += airy * 0.7f * kit::harmonicsAt (airSat[(size_t) c], o - airHp[(size_t) c], lv, 1.1f, 0.0f);
                    }
                    io[c][i] = o;
                    pk = std::max (pk, std::abs (o));
                }
            }
            // the pitch, about every 40 ms; a melody moving a semitone plays the next chop even without a hit
            sinceTrack += n;
            if (sinceTrack >= (int) (0.04 * sr))
            {
                sinceTrack = 0; trackPitch();
                if (chop > 0.0f && autoTime && sinceTrigger >= minGap && pitchConf > 0.6f && lastPlayedPitch > 0.0f && std::abs (12.0f * std::log2 (pitchNow / lastPlayedPitch)) > 0.9f)
                { trigger (tricks); sinceTrigger = 0.0f; }
            }
            // the colour: the note (its pitch class round the colour wheel) and warm to cool (its brightness)
            const float bright = std::sqrt (hiMs / (allMs + 1.0e-12f));
            warmNow += 0.2f * (std::clamp (bright * 2.2f, 0.0f, 1.0f) - warmNow);
            if (pitchConf > 0.4f && pitchNow > 0.0f) { const float pc = std::fmod (12.0f * std::log2 (pitchNow / 440.0f) + 69.0f + 1200.0f, 12.0f); hueNow = pc / 12.0f; }
            const float dt = (float) n / (float) sr;
            for (auto& c : bank) { c.age += dt; c.flash *= std::exp (-dt / 0.3f); }
            for (const auto& v : voices) if (v.on) bank[(size_t) v.chop].flash = std::max (bank[(size_t) v.chop].flash, 0.8f);
            histT += dt;
            if (histT >= 1.0f / 30.0f)
            {
                histT = 0.0f;
                hHue[(size_t) histHead] = hueNow; hWarm[(size_t) histHead] = warmNow; hLevel[(size_t) histHead] = std::min (1.0f, lvPeak * 2.5f);
                histHead = (histHead + 1) % hist; lvPeak = 0.0f;
            }
            setMeter (std::min (1.0f, pk));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** HYPERCUBE: a visualiser - it passes the sound untouched and publishes what it hears: 16 bands, a
        fingerprint of the track (its long-term balance, so no two tracks warp the cube alike), the hits, the
        brightness, the level, the stereo trace. Its knobs shape only the picture (the screen reads them back).
        State: [0..15] bands, [16..23] fingerprint, [24] hit, [25] brightness, [26] level, [27] correlation,
        [28] 64, [29..156] 64 x (L, R), [157] detail, [158] warp, [159] spin, [160] colour, [161] trails. */
    class Hypercube final : public RackUnit
    {
    public:
        static constexpr int bands = 16, xy = 64;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 167) return 0;
            for (int b = 0; b < bands; ++b) o[b] = band[(size_t) b];
            for (int k = 0; k < 8; ++k) o[16 + k] = print[(size_t) k];
            o[24] = hitN; o[25] = brightN; o[26] = levelN; o[27] = corrN; o[28] = (float) xy;
            for (int i = 0; i < xy; ++i) { o[29 + 2 * i] = trace[(size_t) (2 * i)]; o[30 + 2 * i] = trace[(size_t) (2 * i + 1)]; }
            for (int k = 0; k < 8; ++k) o[157 + k] = knobs[(size_t) k];   // MORPH REACT PALETTE BEAM (0..1), then their AUTOs (0 / 1)
            o[165] = hardN; o[166] = hitRate;
            return 167;
        }
    private:
        std::array<sim::Svf, bands> f; std::array<float, bands> env {}, band {}; std::array<float, 8> slowAvg {}, print {};
        std::array<float, 2 * xy> trace {}; std::array<float, 8> knobs {};
        float hardN = 0.0f, hitRate = 0.0f;   // (the music's vibe: 0 calm .. 1 hard - hits a second, brightness, loudness)
        sim::Env fast, slow; float hitN = 0.0f, brightN = 0.0f, levelN = 0.0f, corrN = 1.0f, lr = 0.0f, ll = 1.0e-9f, rrr = 1.0e-9f, hiMs = 0.0f, allMs = 1.0e-9f, hp = 0.0f;
        void prepareUnit (double s, int) override
        {
            for (int b = 0; b < bands; ++b) f[(size_t) b].set (s, 40.0 * std::pow (400.0, b / 15.0), 3.0);
            fast.setup (s, 0.001, 0.04); slow.setup (s, 0.04, 0.4);
        }
        void resetUnit() override { for (auto& x : f) x.reset(); env = band = {}; slowAvg = print = {}; trace = {}; fast.v = slow.v = 0.0f; hitN = 0.0f; hardN = hitRate = 0.0f; hiMs = 0.0f; allMs = 1.0e-9f; hp = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            for (int k = 0; k < 4; ++k) knobs[(size_t) k] = sim::knob10 (p[1 + k]);
            for (int k = 0; k < 4; ++k) knobs[(size_t) (4 + k)] = p[5 + k] > 0.5f ? 1.0f : 0.0f;
            const float ek = sim::coef (sr, 0.06), ck = sim::coef (sr, 0.2), hk = sim::lp1K (sr, 3000.0);
            float hitMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = io[0][i], r = io[1][i], m = 0.5f * (l + r);
                float lo, hi;
                for (int b = 0; b < bands; ++b) { const float v = f[(size_t) b].process (m, lo, hi); env[(size_t) b] += ek * (std::abs (v) - env[(size_t) b]); }
                const float fa = fast.process (m), sl = slow.process (m);
                hitMax = std::max (hitMax, sim::clamp01 ((fa - sl) / (sl + 1.0e-3f)));
                lr += ck * (l * r - lr); ll += ck * (l * l - ll); rrr += ck * (r * r - rrr);
                hp += hk * (m - hp); hiMs += ck * ((m - hp) * (m - hp) - hiMs); allMs += ck * (m * m - allMs);
            }
            // (the sound itself passes untouched)
            const float dt = (float) n / (float) sr, sk = 1.0f - std::exp (-dt / 8.0f);
            for (int b = 0; b < bands; ++b) band[(size_t) b] = sim::clamp01 ((sim::toDb (env[(size_t) b] + 1.0e-6f) + 60.0f) / 55.0f);
            for (int k = 0; k < 8; ++k)
            {
                slowAvg[(size_t) k] += sk * (0.5f * (band[(size_t) (2 * k)] + band[(size_t) (2 * k + 1)]) - slowAvg[(size_t) k]);
            }
            float mean = 0.0f; for (float v : slowAvg) mean += v; mean /= 8.0f;
            for (int k = 0; k < 8; ++k) print[(size_t) k] = std::clamp ((slowAvg[(size_t) k] - mean) * 4.0f, -1.0f, 1.0f);
            // hits a second, slowly (a block with a fresh hit counts once), and from them, brightness and level: the vibe
            hitRate += (1.0f - std::exp (-dt / 2.5f)) * ((hitMax > 0.5f && hitN < 0.5f ? 1.0f / dt : 0.0f) - hitRate);
            hitN = std::max (hitMax, hitN * std::exp (-dt / 0.15f));
            brightN = sim::clamp01 (std::sqrt (hiMs / (allMs + 1.0e-12f)) * 2.0f);
            levelN = sim::clamp01 ((10.0f * std::log10 (allMs + 1.0e-12f) + 50.0f) / 45.0f);
            corrN = std::clamp (lr / std::sqrt (ll * rrr + 1.0e-12f), -1.0f, 1.0f);
            hardN += (1.0f - std::exp (-dt / 1.5f)) * (sim::clamp01 (0.18f * hitRate + 0.45f * brightN + 0.35f * levelN - 0.15f) - hardN);
            // the stereo trace: the block's last samples, every other one (a scope's beam, continuous)
            for (int i = 0; i < xy; ++i) { const int s = std::clamp (n - 2 * xy + 2 * i, 0, n - 1); trace[(size_t) (2 * i)] = io[0][s]; trace[(size_t) (2 * i + 1)] = io[1][s]; }
            setMeter (levelN);
        }
    };
}
