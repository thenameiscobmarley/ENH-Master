#pragma once

#include "RackUnit.h"
#include "../UnitKit.h"

/*  The simulated units: each one a small model of a real object - its sound comes from simulating it, and its
    screen (white on black, UI/Scene/SimScreens.h) shows the simulation live, from the state each publishes
    (displayState; the layouts are documented at each class and read back in SimScreens.h).
      VINYL DECK   a record on a turntable        ROTARY CAB    a rotating-speaker cabinet
      CASSETTE     a cassette deck                TAPE ECHO     a tape-loop echo with three heads */
namespace enh::dsp::units
{
    namespace sim
    {
        struct Rng { unsigned s = 0x2545f491u; float next() noexcept { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; } float bi() noexcept { return 2.0f * next() - 1.0f; } };
        inline float coef (double sr, double seconds) noexcept { return 1.0f - (float) std::exp (-1.0 / (std::max (1.0e-6, seconds) * sr)); }
        inline float lp1K (double sr, double hz) noexcept { return 1.0f - (float) std::exp (-6.283185307 * hz / sr); }
    }

    // ------------------------------------------------------------------------------------------------
    /** VINYL DECK: the sound as if cut to a record and played back. The platter turns (SPEED: 33, 45 or 78):
        an off-centre record bends the pitch once a turn (WOW); dust sits at fixed places round the disc and
        ticks each time the stylus comes round to it, once a turn, as on a real record - DUST sets how much
        (and new specks settle as old ones are worn away); the surface hisses and the motor rumbles; a worn
        stylus breaks up the top (WEAR); AGE narrows the band and folds it toward mono (78s most of all).
        State: [0] turn 0..1, [1] tonearm 0..1 (outside to label), [2] level, [3] speed 0/1/2, [4] n, then n x (angle, flash). */
    class VinylDeck final : public RackUnit
    {
    public:
        static constexpr int maxDust = 24;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 5 + 2 * maxDust) return 0;
            o[0] = turn; o[1] = arm; o[2] = level; o[3] = (float) speedIdx; o[4] = (float) dust.size();
            for (size_t i = 0; i < dust.size(); ++i) { o[5 + 2 * i] = dust[i].angle; o[6 + 2 * i] = dust[i].flash; }
            return 5 + 2 * maxDust;
        }
    private:
        struct Speck { float angle, size, flash; };
        std::array<DelayLine, 2> wow; std::vector<Speck> dust; sim::Rng rng;
        std::array<float, 2> lp {}, hp {}, hissLp {}, hissHp {}, click {};
        float rumble = 0.0f, turn = 0.0f, arm = 0.0f, level = 0.0f; int speedIdx = 0;
        void prepareUnit (double s, int) override { for (auto& d : wow) d.setMax ((int) (0.01 * s) + 8); dust.reserve (maxDust); }
        void resetUnit() override { for (auto& d : wow) d.clear(); dust.clear(); lp = hp = hissLp = hissHp = click = {}; rumble = turn = arm = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            speedIdx = std::clamp ((int) std::lround (p[1]), 0, 2);
            static constexpr float rpm[3] { 33.333f, 45.0f, 78.0f };
            const float rev = rpm[speedIdx] / 60.0f;                       // turns a second
            const float wear = std::clamp (p[2], 0.0f, 10.0f) / 10.0f, dustAmt = std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            const float wowAmt = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, age = std::clamp (p[5], 0.0f, 10.0f) / 10.0f;
            const float mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            // the dust on the record: as many specks as DUST asks for, a few replaced as they wear away
            const int want = (int) std::lround (dustAmt * maxDust);
            while ((int) dust.size() < want) dust.push_back ({ rng.next(), 0.3f + 0.7f * rng.next() * rng.next(), 0.0f });
            while ((int) dust.size() > want) dust.pop_back();
            if (! dust.empty() && rng.next() < 0.02f * (float) n / 512.0f) dust[(size_t) (rng.next() * (float) dust.size()) % dust.size()] = { rng.next(), 0.3f + 0.7f * rng.next(), 0.0f };
            const float top = (speedIdx == 2 ? 5500.0f : 16000.0f) * (1.0f - 0.55f * age) + 1500.0f;
            const float lpK = sim::lp1K (sr, top), hissK1 = sim::lp1K (sr, 9000.0), hissK2 = sim::lp1K (sr, 1500.0);
            const float mono = 0.8f * age * age + (speedIdx == 2 ? 0.6f : 0.0f);
            const float hissAmt = (0.0025f + 0.006f * dustAmt + 0.004f * age) * (speedIdx == 2 ? 2.5f : 1.0f);
            const float step = rev / (float) sr;
            float lv = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float before = turn;
                turn += step; if (turn >= 1.0f) turn -= 1.0f;
                arm = std::min (1.0f, arm + step / (18.0f * 60.0f * rev));   // (a side: about 18 minutes)
                if (arm >= 1.0f) arm = 0.0f;
                // wow: the record off centre - once a turn, and a warp's twice
                const float w = std::sin (6.2832f * turn) + 0.35f * std::sin (12.566f * turn + 1.3f);
                const float d = 2.0f + wowAmt * 0.0022f * (float) sr * (0.5f + 0.5f * w) / (1.0f + (float) speedIdx);
                float x[2] { io[0][i], io[1][i] };
                const float m = 0.5f * (x[0] + x[1]);
                // the dust: a tick each time the stylus passes a speck
                float tick = 0.0f;
                for (auto& s : dust)
                {
                    const bool passed = before <= turn ? (s.angle > before && s.angle <= turn) : (s.angle > before || s.angle <= turn);
                    if (passed) { tick += s.size * (rng.next() < 0.5f ? 1.0f : -1.0f); s.flash = 1.0f; }
                }
                // occasional loose crackle too, with dust
                if (rng.next() < dustAmt * dustAmt * 18.0f / (float) sr) tick += 0.4f * rng.bi();
                rumble += sim::lp1K (sr, 18.0) * (rng.bi() - rumble);
                for (int c = 0; c < 2; ++c)
                {
                    wow[(size_t) c].push (x[c]);
                    float y = wow[(size_t) c].tap (d);
                    y = y + mono * (m - y);
                    // the worn stylus: the top breaks up (only the treble, and only as loud as it is)
                    hp[(size_t) c] += sim::lp1K (sr, 3000.0) * (y - hp[(size_t) c]);
                    const float hi = y - hp[(size_t) c];
                    y += wear * 0.6f * (std::tanh (hi * (1.0f + 6.0f * wear)) / (1.0f + 6.0f * wear) - hi);
                    lp[(size_t) c] += lpK * (y - lp[(size_t) c]); y = lp[(size_t) c];
                    // the surface: hiss (a band), the ticks (sharp, then gone), the rumble
                    const float nz = rng.bi();
                    hissLp[(size_t) c] += hissK1 * (nz - hissLp[(size_t) c]); hissHp[(size_t) c] += hissK2 * (hissLp[(size_t) c] - hissHp[(size_t) c]);
                    click[(size_t) c] = click[(size_t) c] * 0.55f + tick * (c == 0 ? 1.0f : 0.8f);
                    const float surface = hissAmt * (hissLp[(size_t) c] - hissHp[(size_t) c]) * 3.0f + 0.045f * click[(size_t) c] * (0.3f + dustAmt) + 0.004f * rumble * (1.0f + age);
                    y += surface;
                    io[c][i] = x[c] + mix * (y - x[c]);
                }
                lv = std::max (lv, std::abs (m));
            }
            const float fall = std::exp (-(float) n / (0.25f * (float) sr));
            for (auto& s : dust) s.flash *= fall;
            level = std::max (lv, level * fall);
            setMeter (0.2f + 0.8f * std::min (1.0f, level * 2.0f) * mix);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** ROTARY CAB: the rotating-speaker cabinet. The sound splits at 800 Hz: the top goes to a horn, the lows to
        a drum, each turning (SPEED slow or fast; ACCEL: how long each takes to get there - the heavy drum always
        slower than the horn). As they turn, each comes toward and goes away from the two mics (left and right,
        a quarter turn apart): the pitch rises and falls (Doppler) and the level swells and fades; DISTANCE puts
        the mics further off (less of both, more of the room). DRIVE: the valve amplifier inside.
        State: [0] horn turn 0..1, [1] drum turn, [2] horn Hz, [3] drum Hz, [4] top level, [5] low level, [6] fast. */
    class RotaryCab final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 7) return 0;
            o[0] = hornAng; o[1] = drumAng; o[2] = hornHz; o[3] = drumHz; o[4] = lvHi; o[5] = lvLo; o[6] = fastNow ? 1.0f : 0.0f;
            return 7;
        }
    private:
        kit::Split3 split[2]; std::array<DelayLine, 2> hornD, drumD;
        float hornAng = 0.0f, drumAng = 0.0f, hornHz = 0.8f, drumHz = 0.66f, lvHi = 0.0f, lvLo = 0.0f; bool fastNow = false;
        std::array<float, 2> dcX {}, dcY {};
        void prepareUnit (double s, int) override
        {
            for (auto& sp : split) sp.setup (s, 800.0, 12000.0);
            for (auto& d : hornD) d.setMax ((int) (0.004 * s) + 8);
            for (auto& d : drumD) d.setMax ((int) (0.004 * s) + 8);
        }
        void resetUnit() override { for (auto& sp : split) sp.reset(); for (auto& d : hornD) d.clear(); for (auto& d : drumD) d.clear(); hornAng = drumAng = 0.0f; hornHz = 0.8f; drumHz = 0.66f; lvHi = lvLo = 0.0f; dcX = dcY = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            fastNow = p[1] > 0.5f;
            const float accel = std::clamp (p[2], 0.0f, 10.0f) / 10.0f, drive = std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            const float dist = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            const float hornT = fastNow ? 6.7f : 0.8f, drumT = fastNow ? 5.9f : 0.66f;
            // inertia: the horn gets there in ~0.5 - 2 s, the drum takes three times as long
            const float kh = 1.0f - std::exp (-(float) n / ((0.5f + 1.5f * accel) * (float) sr)), kd = 1.0f - std::exp (-(float) n / ((1.5f + 4.5f * accel) * (float) sr));
            hornHz += kh * (hornT - hornHz); drumHz += kd * (drumT - drumHz);
            const float hStep = hornHz / (float) sr, dStep = drumHz / (float) sr;
            const float dop = (1.0f - 0.6f * dist) * 0.00030f * (float) sr, am = 0.45f * (1.0f - 0.55f * dist);
            const float g = 1.0f + 5.0f * drive * drive;
            float hiPk = 0.0f, loPk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                hornAng += hStep; if (hornAng >= 1.0f) hornAng -= 1.0f;
                drumAng += dStep; if (drumAng >= 1.0f) drumAng -= 1.0f;
                // the valve amp, mono (the cabinet is one speaker system)
                float x = 0.5f * (io[0][i] + io[1][i]);
                if (drive > 0.001f) x = std::tanh (x * g) / g * (1.0f + 0.5f * drive);
                float b[3]; split[0].split (x, b);
                const float lo = b[0], hi = b[1] + b[2];
                hornD[0].push (hi); drumD[0].push (lo);
                hiPk = std::max (hiPk, std::abs (hi)); loPk = std::max (loPk, std::abs (lo));
                float out[2];
                for (int c = 0; c < 2; ++c)
                {
                    const float micPhase = c == 0 ? 0.0f : 0.25f;   // the two mics, a quarter turn apart
                    const float ha = 6.2832f * (hornAng + micPhase), da = 6.2832f * (drumAng + micPhase);
                    const float hDelay = 2.0f + dop * (1.0f + std::sin (ha)), dDelay = 2.0f + 0.5f * dop * (1.0f + std::sin (da));
                    const float hAmp = 1.0f - am * (0.5f + 0.5f * std::cos (ha)), dAmp = 1.0f - 0.5f * am * (0.5f + 0.5f * std::cos (da));
                    out[c] = hornD[0].tap (hDelay) * hAmp + drumD[0].tap (dDelay) * dAmp;
                }
                for (int c = 0; c < 2; ++c) io[c][i] = io[c][i] + mix * (out[c] - io[c][i]);
            }
            const float fall = std::exp (-(float) n / (0.2f * (float) sr));
            lvHi = std::max (hiPk, lvHi * fall); lvLo = std::max (loPk, lvLo * fall);
            setMeter (hornHz / 6.7f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** CASSETTE DECK: the sound recorded to cassette and played back. TAPE: type I (warm, saturates early, the
        top rolls off soonest), II (brighter) or IV (metal: the most headroom); the head bump in the lows; WOW
        (slow drift) and FLUTTER (a quick shimmer) in the capstan; HISS; DROPOUTS: now and then the tape lifts
        off the head for a moment (quieter, duller); SATURATE: how hard it is recorded; STOP: the deck stops - the
        tape winds down to a halt (pitch falling) and starts up again when released.
        State: [0] reel turn 0..1, [1] played 0..1 (the tape's side), [2] speed 0..1, [3] level, [4] dropout 0..1, [5] type. */
    class CassetteDeck final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 6) return 0;
            o[0] = turn; o[1] = played; o[2] = speed; o[3] = level; o[4] = drop; o[5] = (float) type;
            return 6;
        }
    private:
        std::vector<float> buf[2]; int w = 0; double readPos = 0.0; bool stopped = false; float speed = 1.0f, xfade = 0.0f;
        std::array<DelayLine, 2> wf; std::array<float, 2> lp {}, bump {}, hissS {};
        std::array<kit::SoftSat, 2> sat {}; std::array<kit::Level, 2> lvl {};
        sim::Rng rng; float wowPh = 0.0f, wowTarget = 0.0f, wowNow = 0.0f, flutPh = 0.0f;
        float turn = 0.0f, played = 0.0f, level = 0.0f, drop = 0.0f, dropTarget = 0.0f; int dropLeft = 0, type = 0;
        void prepareUnit (double s, int) override
        {
            for (auto& b : buf) b.assign ((size_t) (4.0 * s), 0.0f);
            for (auto& d : wf) d.setMax ((int) (0.02 * s) + 8);
            for (auto& l : lvl) l.setup (s, 0.3);
        }
        void resetUnit() override
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            w = 0; readPos = 0.0; stopped = false; speed = 1.0f; xfade = 0.0f;
            for (auto& d : wf) d.clear(); lp = bump = hissS = {}; for (auto& s : sat) s.reset(); for (auto& l : lvl) l.reset();
            wowPh = wowTarget = wowNow = flutPh = 0.0f; turn = played = level = drop = dropTarget = 0.0f; dropLeft = 0;
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            type = std::clamp ((int) std::lround (p[1]), 0, 2);
            const float wowA = std::clamp (p[2], 0.0f, 10.0f) / 10.0f, flutA = std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            const float hissA = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, dropA = std::clamp (p[5], 0.0f, 10.0f) / 10.0f;
            const float satA = std::clamp (p[6], 0.0f, 10.0f) / 10.0f;
            const bool stop = p[7] > 0.5f;
                        static constexpr float topHz[3] { 9000.0f, 12500.0f, 15000.0f }, headroom[3] { 0.9f, 0.75f, 0.55f };
            const float lpK = sim::lp1K (sr, topHz[type]), bumpK = sim::lp1K (sr, 70.0), hissK = sim::lp1K (sr, 6000.0);
            const float drive = (0.4f + 2.2f * satA) * (0.5f + headroom[type]), bias = 0.08f + 0.1f * satA;
            const int nb = (int) buf[0].size();
            float lv = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                // the transport: STOP winds it down (~0.8 s), release starts it up (~0.3 s)
                if (stop && ! stopped) stopped = true;
                speed += (stop ? -1.0f / (0.8f * (float) sr) : 1.0f / (0.3f * (float) sr));
                speed = std::clamp (speed, 0.0f, 1.0f);
                if (! stop && stopped && speed >= 1.0f) { stopped = false; xfade = 1.0f; }
                turn += speed * 0.35f / (float) sr; if (turn >= 1.0f) turn -= 1.0f;   // (a reel: about a turn every 3 s)
                played = std::min (1.0f, played + speed / (float) (sr * 30.0 * 60.0)); if (played >= 1.0f) played = 0.0f;
                // wow: a slow wander; flutter: a quick shimmer
                wowPh += 0.7f / (float) sr; if (wowPh >= 1.0f) { wowPh -= 1.0f; wowTarget = rng.bi(); }
                wowNow += sim::lp1K (sr, 0.8) * (wowTarget - wowNow);
                flutPh += (9.0f + 3.0f * std::sin (wowPh * 6.2832f)) / (float) sr; if (flutPh >= 1.0f) flutPh -= 1.0f;
                const float mod = 3.0f + (0.0030f * wowA * (1.0f + wowNow) + 0.00035f * flutA * (1.0f + std::sin (6.2832f * flutPh))) * (float) sr;
                // dropouts: now and then the tape lifts off the head
                if (dropLeft <= 0 && rng.next() < dropA * dropA * 1.5f / (float) sr) { dropLeft = (int) ((0.02f + 0.1f * rng.next()) * (float) sr); dropTarget = 0.2f + 0.6f * rng.next(); }
                if (dropLeft > 0) --dropLeft;
                drop += sim::lp1K (sr, 60.0) * ((dropLeft > 0 ? dropTarget : 0.0f) - drop);
                const float dropGain = 1.0f - drop, dropLp = 1.0f - 0.8f * drop;
                for (int c = 0; c < 2; ++c) buf[(size_t) c][(size_t) w] = io[c][i];
                w = (w + 1) % nb;
                // (stopping: the read head falls behind the live tape, ever slower; stopped dead, it catches up so
                // it starts again on what is coming in; started, it runs at speed and fades back into the live one)
                if (! stopped && xfade <= 0.0f) readPos = (double) w - 1.0;
                else readPos = speed <= 0.0f ? (double) w - 1.0 : readPos + (double) speed;
                float yv[2];
                for (int c = 0; c < 2; ++c)
                {
                    // the tape (recorded as it came, or - stopping - read ever slower behind)
                    float x;
                    if (stopped || xfade > 0.0f)
                    {
                        double q = std::fmod (readPos + 4.0 * nb, (double) nb); const int i0 = (int) q; const float f = (float) (q - i0);
                        const float a = buf[(size_t) c][(size_t) i0], b = buf[(size_t) c][(size_t) ((i0 + 1) % nb)];
                        const float slow = (a + (b - a) * f) * std::min (1.0f, speed * 4.0f);
                        x = stopped ? slow : io[c][i] + xfade * (slow - io[c][i]);
                    }
                    else x = io[c][i];
                    wf[(size_t) c].push (x);
                    float y = wf[(size_t) c].tap (mod);
                    // recorded: saturated against its level (the colour the same at any volume), the head bump, the top rolling off
                    const float l = lvl[(size_t) c].process (y);
                    y += kit::harmonicsAt (sat[(size_t) c], y, l, drive, bias) * (0.6f + satA);
                    bump[(size_t) c] += bumpK * (y - bump[(size_t) c]);
                    y += 0.25f * bump[(size_t) c];
                    lp[(size_t) c] += lpK * dropLp * (y - lp[(size_t) c]); y = lp[(size_t) c] * dropGain;
                    hissS[(size_t) c] += hissK * (rng.bi() - hissS[(size_t) c]);
                    y += hissA * 0.012f * hissS[(size_t) c] * speed;
                    yv[c] = y;
                }
                if (xfade > 0.0f) xfade = std::max (0.0f, xfade - 1.0f / (0.05f * (float) sr));
                for (int c = 0; c < 2; ++c) { lv = std::max (lv, std::abs (yv[c])); io[c][i] = yv[c]; }
            }
            level = std::max (lv, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (std::min (1.0f, level * 2.0f) * speed);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TAPE ECHO: a loop of tape past a record head and three playback heads (1, 2 and 3 times TIME apart),
        each echo read back where the tape has carried it; what comes back is recorded again (INTENSITY - past
        100 % it runs away, held by the tape's own saturation), each pass through the tape: a little duller,
        warmer, wavering (WOW). HEADS picks which play; TONE tilts what goes round. MIX.
        State: [0] loop phase 0..1, [1] TIME s, [2] heads mask (1, 2, 4), [3] intensity, [4] level, [5] n, then n x (position 0..1, amplitude). */
    class TapeEcho final : public RackUnit
    {
    public:
        static constexpr int maxBlips = 32;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 6 + 2 * maxBlips) return 0;
            o[0] = phase; o[1] = timeS; o[2] = (float) heads; o[3] = inten; o[4] = level; o[5] = (float) nBlips;
            for (int b = 0; b < nBlips; ++b) { o[6 + 2 * b] = blipPos[(size_t) b]; o[7 + 2 * b] = blipAmp[(size_t) b]; }
            return 6 + 2 * maxBlips;
        }
    private:
        std::array<DelayLine, 2> tape; std::array<float, 2> lp {}, hp {}, lowS {}; sim::Rng rng;
        float wowNow = 0.0f, wowT = 0.0f, wowPh = 0.0f, timeS = 0.3f, timeSm = -1.0f, inten = 0.0f, level = 0.0f, phase = 0.0f; int heads = 1;
        std::array<float, maxBlips> blipPos {}, blipAmp {}; int nBlips = 0; float sinceBlip = 0.0f, blockPeak = 0.0f;
        void prepareUnit (double s, int) override { for (auto& d : tape) d.setMax ((int) (2.6 * s) + 16); }
        void resetUnit() override { for (auto& d : tape) d.clear(); lp = hp = lowS = {}; wowNow = wowT = wowPh = 0.0f; timeSm = -1.0f; level = phase = 0.0f; nBlips = 0; sinceBlip = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float t = std::clamp (p[1], 50.0f, 800.0f) / 1000.0f;
            inten = std::clamp (p[2], 0.0f, 110.0f) / 100.0f;
            static constexpr int mask[6] { 1, 2, 4, 3, 6, 7 };
            heads = mask[std::clamp ((int) std::lround (p[3]), 0, 5)];
            const float wowA = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, tone = std::clamp (p[5], -5.0f, 5.0f) / 5.0f, mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            if (timeSm < 0.0f) timeSm = t;
            timeS = t;
            const float tk = 1.0f - std::exp (-(float) n / (0.35f * (float) sr));   // (TIME glides: the motor's speed changing, a pitch sweep)
            timeSm += tk * (t - timeSm);
            const float lpK = sim::lp1K (sr, 4500.0 + 2500.0 * tone), hpK = sim::lp1K (sr, 120.0 - 60.0 * tone);
            int active = 0; for (int h = 0; h < 3; ++h) active += (heads >> h) & 1;
            const float fb = inten * 0.95f / std::max (1, active);
            float pk = 0.0f, inPk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                inPk = std::max (inPk, std::abs (0.5f * (io[0][i] + io[1][i])));
                wowPh += 0.9f / (float) sr; if (wowPh >= 1.0f) { wowPh -= 1.0f; wowT = rng.bi(); }
                wowNow += sim::lp1K (sr, 1.2) * (wowT - wowNow);
                const float wob = 1.0f + 0.004f * wowA * wowNow + 0.0015f * wowA * std::sin (6.2832f * wowPh * 11.0f);
                phase += 1.0f / (timeSm * 3.0f * (float) sr); if (phase >= 1.0f) phase -= 1.0f;
                for (int c = 0; c < 2; ++c)
                {
                    float echo = 0.0f;
                    for (int h = 0; h < 3; ++h)
                        if ((heads >> h) & 1) echo += tape[(size_t) c].tap (std::min ((float) (2.55 * sr), (float) (h + 1) * timeSm * (float) sr * wob));
                    // round the loop: duller, thinner, saturated (it holds itself past 100 %)
                    lp[(size_t) c] += lpK * (echo - lp[(size_t) c]);
                    hp[(size_t) c] += hpK * (lp[(size_t) c] - hp[(size_t) c]);
                    const float loop = std::tanh ((lp[(size_t) c] - hp[(size_t) c]) * fb * 1.2f) / 1.2f;
                    tape[(size_t) c].push (io[c][i] + loop);
                    pk = std::max (pk, std::abs (echo));
                    io[c][i] = io[c][i] + mix * (echo / std::max (1, active) * 1.2f);
                }
            }
            // the echoes as marks on the loop: a new one where the record head is, whenever something was loud
            sinceBlip += (float) n / (float) sr;
            blockPeak = std::max (blockPeak * 0.8f, inPk);
            if (sinceBlip > 0.05f && blockPeak > 0.02f && nBlips < maxBlips)
            {
                blipPos[(size_t) nBlips] = 0.0f; blipAmp[(size_t) nBlips] = std::min (1.0f, blockPeak * 2.0f); ++nBlips; sinceBlip = 0.0f;
            }
            const float move = (float) n / (float) sr / (timeSm * 3.0f);
            for (int b = 0; b < nBlips;)
            {
                blipPos[(size_t) b] += move; blipAmp[(size_t) b] *= 0.992f;
                if (blipPos[(size_t) b] >= 1.0f) { blipPos[(size_t) b] -= 1.0f; blipAmp[(size_t) b] *= inten; }   // (round again: as loud as it is fed back)
                if (blipAmp[(size_t) b] < 0.02f) { blipPos[(size_t) b] = blipPos[(size_t) (nBlips - 1)]; blipAmp[(size_t) b] = blipAmp[(size_t) (nBlips - 1)]; --nBlips; }
                else ++b;
            }
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (std::min (1.0f, level * 2.0f));
        }
    };
}
