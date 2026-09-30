#pragma once

#include "RackUnit.h"
#include "RoomScene.h"

/*  RAY ROOM (3U). */
namespace enh::dsp::units
{
    /** RAY ROOM: the sound played into a room, and a little of what a room full of old gear adds.
          - The room: a rectangle seen from above, two speakers at the front, you in the middle (RoomScene.h).
            Each speaker's sound reaches you straight and off every wall - its mirror images across the
            walls, eight each (first and second order), each delayed by its path (343 m/s), quieter by its
            distance and by what each wall it met took (DAMP), from its own direction (panned). Then a small
            four-line tail, as long as the room is big. SPACE: from a small room (4 m) to a hall (25 m).
          - CRACKLE and POPPING: dots thrown from the speakers into the room, more the louder the track,
            wandering off their lines; where one hits a wall, it plays back a moment of what was playing
            (up to 1.5 s ago) as if off a worn tape: a little slow and wavering, saturated, band-limited,
            hissing and grainy - a crackle a few milliseconds long, a pop a tenth of a second.
          - MIX: how much of the room is added (the sound itself always stays).
        Cheap: 16 taps and 4 lines a sample, 12 tape voices at most; no latency. Its screen shows the room,
        the rays and the dots (displayState). */
    class RayRoom final : public RackUnit
    {
    public:
        int displayState (float* out, int max) const noexcept override
        {
            if (max < room::State::size) return 0;
            published.write (out);
            return room::State::size;
        }

    private:
        struct Voice
        {
            bool on = false; bool pop = false;
            double pos = 0.0; float speed = 1.0f, wob = 0.0f, wobRate = 0.0f, drive = 1.0f, gain = 0.0f, gl = 0.0f, gr = 0.0f;
            int left = 0, len = 1; float hp = 0.0f, lp = 0.0f, lpK = 0.3f, hpK = 0.02f; unsigned noise = 1u;
        };
        struct Dot { float x, y, vx, vy, turn, age, life; bool pop; };

        std::array<DelayLine, 2> line;                 // each speaker's sound, for the reflections
        std::array<DelayLine, 4> fdn;
        std::array<float, 4> fdnLp {};
        std::vector<float> hist; int histW = 0;         // the last 2 s (mono): what the tape plays back
        std::array<Voice, 12> voices {};
        std::vector<Dot> dots;
        std::array<std::array<BiquadState, room::bands>, 2> bandSt {};
        std::array<BiquadCoeffs, room::bands> bandC {};
        std::array<std::array<float, room::bands>, 2> bandEnv {};
        std::array<float, 2> earlyLp {};
        float levL = 0.0f, levR = 0.0f, sideE = 0.0f, midE = 0.0f, spaceSm = -1.0f;
        unsigned rng = 0x9e3779b9u;
        room::State published;

        float rand01() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 16777216.0f; }

        void prepareUnit (double s, int) override
        {
            for (auto& l : line) l.setMax ((int) (0.40 * s) + 8);
            for (auto& l : fdn) l.setMax ((int) (0.25 * s) + 8);
            hist.assign ((size_t) (2.0 * s), 0.0f);
            static constexpr float hz[room::bands] { 120.0f, 350.0f, 900.0f, 2200.0f, 5000.0f, 11000.0f };
            for (int b = 0; b < room::bands; ++b) bandC[(size_t) b] = BiquadCoeffs::bandPass (s, std::min (0.42 * s, (double) hz[b]), 1.1);
            dots.reserve (room::maxDots);
        }
        void resetUnit() override
        {
            for (auto& l : line) l.clear();
            for (auto& l : fdn) l.clear();
            fdnLp = {}; earlyLp = {};
            std::fill (hist.begin(), hist.end(), 0.0f); histW = 0;
            voices = {}; dots.clear(); bandSt = {}; bandEnv = {};
            levL = levR = sideE = midE = 0.0f; spaceSm = -1.0f;
        }

        /** A tape voice: a moment of what was playing, through a worn tape. */
        void startVoice (bool pop, float pan, float strength)
        {
            for (auto& v : voices)
                if (! v.on)
                {
                    v.on = true; v.pop = pop;
                    const int n = (int) hist.size();
                    const double back = (0.05 + 1.45 * rand01()) * sr;
                    v.pos = std::fmod ((double) histW - back + 4.0 * n, (double) n);
                    v.len = v.left = (int) ((pop ? 0.05 + 0.09 * rand01() : 0.003 + 0.012 * rand01()) * sr);
                    v.speed = 0.90f + 0.12f * rand01();              // a worn transport: a little slow
                    v.wob = 6.2832f * rand01(); v.wobRate = (float) (6.2832 * (4.0 + 5.0 * rand01()) / sr);
                    v.drive = pop ? 3.5f + 3.0f * rand01() : 2.0f + 2.0f * rand01();
                    v.gain = strength * (pop ? 0.9f : 0.55f);
                    const float p = std::clamp (pan, -1.0f, 1.0f) * 0.7854f + 0.7854f;
                    v.gl = std::cos (p); v.gr = std::sin (p);
                    v.hp = v.lp = 0.0f;
                    v.lpK = 1.0f - std::exp (-6.2832f * (pop ? 3500.0f : 6000.0f) / (float) sr);
                    v.hpK = 1.0f - std::exp (-6.2832f * (pop ? 90.0f : 400.0f) / (float) sr);
                    v.noise = rng | 1u;
                    return;
                }
        }

        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float spaceK = std::clamp (p[1], 0.0f, 10.0f), damp = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const float crackle = std::clamp (p[3], 0.0f, 10.0f) / 10.0f, popping = std::clamp (p[4], 0.0f, 10.0f) / 10.0f;
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            // SPACE glides (a room grows; its reflections slide rather than jump)
            if (spaceSm < 0.0f) spaceSm = spaceK;
            spaceSm += (spaceK - spaceSm) * (1.0f - std::exp (-(float) n / (0.25f * (float) sr)));
            const float W = room::halfWidth (spaceSm), D = room::halfDepth (spaceSm);
            const auto lis = room::listener (W, D);

            // The reflections: each speaker's mirror images in the walls (x: -1, 0, 1 rooms across; y the same)
            struct Tap { float delay, gl, gr; };
            std::array<std::array<Tap, 8>, 2> taps {};
            const float wallKeep = 0.85f - 0.45f * damp;
            for (int side = 0; side < 2; ++side)
            {
                const auto s = room::speaker (side, W, D);
                int k = 0;
                for (int i = -1; i <= 1; ++i)
                    for (int j = -1; j <= 1; ++j)
                    {
                        if (i == 0 && j == 0) continue;
                        const float ix = i == 0 ? s.x : (i > 0 ? 2.0f * W - s.x : -2.0f * W - s.x);
                        const float iy = j == 0 ? s.y : (j > 0 ? 2.0f * D - s.y : -2.0f * D - s.y);
                        const float dx = ix - lis.x, dy = iy - lis.y, dist = std::sqrt (dx * dx + dy * dy);
                        const int order = std::abs (i) + std::abs (j);
                        const float g = std::pow (wallKeep, (float) order) * 2.2f / std::max (1.0f, dist);
                        const float pan = std::clamp (dx / std::max (1.0e-3f, dist), -1.0f, 1.0f) * 0.7854f + 0.7854f;
                        taps[(size_t) side][(size_t) k++] = { std::min ((float) (0.38 * sr), dist / 343.0f * (float) sr), g * std::cos (pan), g * std::sin (pan) };
                    }
            }
            // The tail: four lines as long as the room, fading over RT60
            const float scale = W / 6.0f;
            static constexpr float ms[4] { 31.0f, 37.0f, 43.0f, 53.0f };
            const float rt = 0.25f + 2.6f * (spaceSm / 10.0f) * (1.0f - 0.55f * damp);
            std::array<float, 4> fl {}, fg {};
            for (int k = 0; k < 4; ++k) { fl[(size_t) k] = std::min ((float) (0.24 * sr), ms[k] * 0.001f * scale * (float) sr); fg[(size_t) k] = std::pow (10.0f, -3.0f * fl[(size_t) k] / ((float) sr * rt)); }
            const float lpK = 1.0f - std::exp (-6.2832f * (12000.0f - 9500.0f * damp) / (float) sr);

            float grainAct = 0.0f;
            const float bandK = 1.0f - std::exp (-1.0f / (0.08f * (float) sr)), levK = 1.0f - std::exp (-1.0f / (0.12f * (float) sr));
            const int nh = (int) hist.size();
            for (int i = 0; i < n; ++i)
            {
                const float xl = io[0][i], xr = io[1][i];
                line[0].push (xl); line[1].push (xr);
                hist[(size_t) histW] = 0.5f * (xl + xr); histW = (histW + 1) % nh;
                // early reflections
                float el = 0.0f, er = 0.0f;
                for (int side = 0; side < 2; ++side)
                    for (const auto& t : taps[(size_t) side])
                    {
                        const float v = line[(size_t) side].tap (t.delay + 1.0f);
                        el += v * t.gl; er += v * t.gr;
                    }
                earlyLp[0] += lpK * (el - earlyLp[0]); earlyLp[1] += lpK * (er - earlyLp[1]);
                el = earlyLp[0]; er = earlyLp[1];
                // the tail (a Hadamard mix of four lines, damped)
                std::array<float, 4> o {};
                for (int k = 0; k < 4; ++k) { o[(size_t) k] = fdn[(size_t) k].tap (fl[(size_t) k]); fdnLp[(size_t) k] += lpK * (o[(size_t) k] - fdnLp[(size_t) k]); o[(size_t) k] = fdnLp[(size_t) k]; }
                const float h0 = 0.5f * (o[0] + o[1] + o[2] + o[3]), h1 = 0.5f * (o[0] - o[1] + o[2] - o[3]);
                const float h2 = 0.5f * (o[0] + o[1] - o[2] - o[3]), h3 = 0.5f * (o[0] - o[1] - o[2] + o[3]);
                const float inj = 0.35f * (el + er);
                fdn[0].push (inj + fg[0] * h0); fdn[1].push (inj + fg[1] * h1); fdn[2].push (inj + fg[2] * h2); fdn[3].push (inj + fg[3] * h3);
                const float tl = 0.5f * (o[0] + o[2]), tr = 0.5f * (o[1] + o[3]);
                // the tape voices
                float gl = 0.0f, gr = 0.0f;
                for (auto& v : voices)
                {
                    if (! v.on) continue;
                    const double q = v.pos; const int i0 = (int) q; const float f = (float) (q - i0);
                    const float a = hist[(size_t) (i0 % nh)], b = hist[(size_t) ((i0 + 1) % nh)];
                    float s = a + (b - a) * f;
                    v.wob += v.wobRate;
                    v.pos += v.speed * (1.0f + 0.015f * std::sin (v.wob));
                    if (v.pos >= nh) v.pos -= nh;
                    s = std::tanh (s * v.drive) / std::tanh (v.drive);                  // saturated
                    v.hp += v.hpK * (s - v.hp); s -= v.hp;                             // thin at the bottom
                    v.lp += v.lpK * (s - v.lp); s = v.lp;                              // dull at the top
                    v.noise = v.noise * 1664525u + 1013904223u;
                    const float nz = (float) (v.noise >> 8) / 8388608.0f - 1.0f;
                    const float ph = 1.0f - (float) v.left / (float) v.len;
                    const float env = std::min (1.0f, ph * (v.pop ? 40.0f : 8.0f)) * std::exp (-ph * (v.pop ? 3.5f : 2.0f));
                    s += nz * (v.pop ? 0.06f : 0.18f) * (std::abs (nz) > 0.985f ? 6.0f : 1.0f);   // hiss and grain
                    const float y = s * env * v.gain;
                    gl += y * v.gl; gr += y * v.gr;
                    grainAct += std::abs (y);
                    if (--v.left <= 0) v.on = false;
                }
                io[0][i] = xl + mix * (el + 0.7f * tl) + gl;
                io[1][i] = xr + mix * (er + 0.7f * tr) + gr;
                // what the screen shows: each side's bands, levels, width and balance
                for (int b = 0; b < room::bands; ++b)
                {
                    const float ul = bandSt[0][(size_t) b].process (bandC[(size_t) b], xl), ur = bandSt[1][(size_t) b].process (bandC[(size_t) b], xr);
                    bandEnv[0][(size_t) b] += bandK * (ul * ul - bandEnv[0][(size_t) b]);
                    bandEnv[1][(size_t) b] += bandK * (ur * ur - bandEnv[1][(size_t) b]);
                }
                levL += levK * (xl * xl - levL); levR += levK * (xr * xr - levR);
                const float m = 0.5f * (xl + xr), sd = 0.5f * (xl - xr);
                midE += levK * (m * m - midE); sideE += levK * (sd * sd - sideE);
            }

            // The dots: thrown from the speakers as loud as the track is, wandering; at a wall, a tape voice
            const float dt = (float) n / (float) sr;
            const float loud = std::sqrt (0.5f * (levL + levR)) * 1.41f;
            const float drive = std::clamp (std::sqrt (loud) * 1.6f, 0.0f, 1.5f);
            const float crackleRate = crackle * crackle * 26.0f * drive, popRate = popping * popping * 3.2f * drive;
            for (int pop = 0; pop < 2; ++pop)
            {
                const float expected = (pop ? popRate : crackleRate) * dt;
                if ((int) dots.size() < room::maxDots && rand01() < expected)
                {
                    const auto s = room::speaker (rand01() < 0.5f ? 0 : 1, W, D);
                    const float a = 1.5708f + (rand01() - 0.5f) * 5.2f;   // into the room, any way but straight back
                    const float speed = (pop ? 3.5f : 6.0f) * (0.7f + 0.6f * rand01()) * (W / 6.0f);
                    dots.push_back ({ s.x, s.y, std::cos (a) * speed, std::sin (a) * speed, (rand01() - 0.5f) * 1.6f, 0.0f, 2.5f, pop == 1 });
                }
            }
            for (size_t k = 0; k < dots.size();)
            {
                auto& d = dots[k];
                // veer: the line bends a little, randomly (more the louder)
                const float turn = (d.turn + (rand01() - 0.5f) * 3.0f * drive) * dt;
                const float c = std::cos (turn), s = std::sin (turn);
                const float vx = d.vx * c - d.vy * s, vy = d.vx * s + d.vy * c;
                d.vx = vx; d.vy = vy;
                d.x += d.vx * dt; d.y += d.vy * dt; d.age += dt;
                const bool hit = std::abs (d.x) >= W || std::abs (d.y) >= D;
                if (hit || d.age > d.life)
                {
                    if (hit) startVoice (d.pop, d.x / W, std::clamp (0.4f + 0.8f * loud, 0.2f, 1.2f) * (d.pop ? popping : crackle));
                    dots[k] = dots.back(); dots.pop_back();
                }
                else ++k;
            }

            // Published for the screen (read by the message thread through the meters)
            auto& st = published;
            st.W = W; st.D = D;
            st.levelL = std::sqrt (levL) * 1.41f; st.levelR = std::sqrt (levR) * 1.41f;
            st.width = std::clamp (std::sqrt (sideE / std::max (1.0e-9f, midE + sideE)) * 1.6f, 0.0f, 1.0f);
            st.balance = std::clamp ((std::sqrt (levR) - std::sqrt (levL)) / std::max (1.0e-5f, std::sqrt (levR) + std::sqrt (levL)), -1.0f, 1.0f);
            for (int b = 0; b < room::bands; ++b)
            {
                st.bandL[(size_t) b] = std::clamp (std::sqrt (bandEnv[0][(size_t) b]) * 5.0f, 0.0f, 1.0f);
                st.bandR[(size_t) b] = std::clamp (std::sqrt (bandEnv[1][(size_t) b]) * 5.0f, 0.0f, 1.0f);
            }
            st.dots = (int) dots.size();
            for (int k = 0; k < st.dots; ++k)
            {
                const auto& d = dots[(size_t) k];
                st.dot[(size_t) (k * 4)] = d.x; st.dot[(size_t) (k * 4 + 1)] = d.y; st.dot[(size_t) (k * 4 + 2)] = d.pop ? 1.0f : 0.0f; st.dot[(size_t) (k * 4 + 3)] = d.age / d.life;
            }
            setMeter (mix * 0.6f * std::min (1.0f, loud * 2.0f) + std::min (0.6f, grainAct / (float) n * 30.0f));
        }
    };
}
