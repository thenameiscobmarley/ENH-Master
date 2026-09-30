#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>
#include "RoomScreen.h"
#include "../../DSP/units/Sims2.h"

/*  The simulated units' screens (DSP/units/Sims.h), white on black like RAY ROOM's: each draws the object it
    simulates, live, from the state the unit publishes - the record turning under its arm, the rotors in their
    cabinet, the cassette's reels, the echo's tape loop past its heads. Lines and blobs in the screen's pixels,
    as RoomScreen.h: the 3D rack stamps them into its display, the 2D rack draws them with JUCE.
    demo() makes up a state (the screenshot hook, and the rack before the engine has published anything). */
namespace pad::simscreen
{
    using roomscreen::Line;
    using roomscreen::Blob;

    enum class Kind { none, vinyl, rotary, cassette, tapeEcho, valveAmp, speakerCab, radio, pendulum, bounce, sympathy, flyby, tesla, talkBox, lavaLamp };

    /** Each simulation: its unit's key and its POWER switch's parameter id, in Kind's order (after none). */
    struct Entry { const char* key; const char* power; };
    inline constexpr Entry entries[] { { "vinyl", "vdPower" }, { "rotary", "rcPower" }, { "cassette", "ccPower" }, { "tapeecho", "tePower" },
                                       { "valveamp", "vaPower" }, { "speakercab", "skPower" }, { "radio", "raPower" }, { "pendulum", "pdPower" },
                                       { "bounce", "bdPower" }, { "sympathy", "syPower" }, { "flyby", "fyPower" }, { "tesla", "tcPower" },
                                       { "talkbox", "tbPower" }, { "lavalamp", "lvPower" } };

    inline Kind kindOf (std::string_view key) noexcept
    {
        for (size_t i = 0; i < std::size (entries); ++i) if (key == entries[i].key) return (Kind) (i + 1);
        return Kind::none;
    }
    inline const char* powerId (Kind k) noexcept { return k == Kind::none ? "" : entries[(size_t) k - 1].power; }

    namespace detail
    {
        constexpr float tau = 6.2831853f;
        inline float frac (float x) noexcept { return x - std::floor (x); }
        inline void ring (std::vector<Line>& out, float cx, float cy, float r, float thick, float bright, int segs = 0, float a0 = 0.0f, float a1 = tau)
        {
            if (segs <= 0) segs = std::clamp ((int) (r * 0.9f), 16, 96);
            for (int i = 0; i < segs; ++i)
            {
                const float t0 = a0 + (a1 - a0) * (float) i / (float) segs, t1 = a0 + (a1 - a0) * (float) (i + 1) / (float) segs;
                out.push_back ({ cx + r * std::cos (t0), cy + r * std::sin (t0), cx + r * std::cos (t1), cy + r * std::sin (t1), thick, bright });
            }
        }
        inline void box (std::vector<Line>& out, float x0, float y0, float x1, float y1, float thick, float bright)
        {
            out.push_back ({ x0, y0, x1, y0, thick, bright }); out.push_back ({ x1, y0, x1, y1, thick, bright });
            out.push_back ({ x1, y1, x0, y1, thick, bright }); out.push_back ({ x0, y1, x0, y0, thick, bright });
        }
    }

    // --- VINYL DECK: the record from above, its dust turning with it, the arm's stylus working inward ---------------
    inline void vinyl (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float turn = s[0], arm = s[1], level = std::clamp (s[2], 0.0f, 1.0f);
        const int speed = std::clamp ((int) s[3], 0, 2), n = std::clamp ((int) s[4], 0, 24);
        const float R = std::min (0.44f * h, 0.36f * w), cx = 0.40f * w, cy = 0.5f * h;
        // the platter's rim, the record, a few grooves (brighter the louder), the label, the spindle
        ring (L, cx, cy, R * 1.04f, 1.2f * px, 0.35f);
        ring (L, cx, cy, R, 1.8f * px, 0.95f);
        for (int gr = 0; gr < 5; ++gr) ring (L, cx, cy, R * (0.52f + 0.09f * (float) gr), 0.8f * px, 0.16f + 0.2f * level);
        ring (L, cx, cy, R * 0.33f, 1.4f * px, 0.85f);
        B.push_back ({ cx, cy, 2.2f * px, 1.0f, false });
        // the label's print, turning: a bar across it (you see it go round)
        const float a = tau * turn;
        L.push_back ({ cx + std::cos (a) * R * 0.08f, cy + std::sin (a) * R * 0.08f, cx + std::cos (a) * R * 0.28f, cy + std::sin (a) * R * 0.28f, 2.0f * px, 0.9f });
        L.push_back ({ cx - std::cos (a) * R * 0.08f, cy - std::sin (a) * R * 0.08f, cx - std::cos (a) * R * 0.20f, cy - std::sin (a) * R * 0.20f, 1.2f * px, 0.6f });
        // the stylus: on the right, drawn in from the rim toward the label over the side
        const float sa = -0.30f, sr = R * (0.94f - 0.56f * std::clamp (arm, 0.0f, 1.0f));
        const float sx = cx + std::cos (sa) * sr, sy = cy + std::sin (sa) * sr;
        // the dust: fixed on the record, so turning with it - each flashes as the stylus passes it
        for (int i = 0; i < n; ++i)
        {
            const float ang = s[5 + 2 * i], flash = std::clamp (s[6 + 2 * i], 0.0f, 1.0f);
            const float rr = R * (0.40f + 0.55f * frac (ang * 7.31f + 0.17f * (float) i));
            const float da = sa + tau * (ang - turn);
            const float x = cx + std::cos (da) * rr, y = cy + std::sin (da) * rr;
            B.push_back ({ x, y, (1.1f + 1.4f * flash) * px, 0.55f + 0.45f * flash, false });
            if (flash > 0.25f) B.push_back ({ x, y, (3.0f + 7.0f * flash) * px, flash * 0.8f, true });
        }
        // the tonearm: pivot top right, the arm, the headshell square to the groove
        const float pxv = cx + R * 1.22f, pyv = cy - R * 0.80f;
        B.push_back ({ pxv, pyv, 7.0f * px, 0.9f, true });
        B.push_back ({ pxv, pyv, 2.4f * px, 0.9f, false });
        L.push_back ({ pxv, pyv, sx, sy, 2.4f * px, 0.95f });
        const float tx = -std::sin (sa), ty = std::cos (sa);
        L.push_back ({ sx - tx * 7.0f * px, sy - ty * 7.0f * px, sx + tx * 7.0f * px, sy + ty * 7.0f * px, 3.0f * px, 1.0f });
        L.push_back ({ pxv, pyv, pxv + (pxv - sx) * 0.18f, pyv + (pyv - sy) * 0.18f, 4.0f * px, 0.8f });   // (the counterweight)
        B.push_back ({ sx, sy, (2.0f + 4.0f * level) * px, 0.5f + 0.5f * level, true });
        // the speed: three marks on the right, the one it is at lit
        for (int k = 0; k < 3; ++k)
        {
            const float mx = w - 14.0f * px, my = h * (0.62f + 0.11f * (float) k);
            if (k == speed) B.push_back ({ mx, my, 3.6f * px, 1.0f, false });
            else B.push_back ({ mx, my, 3.6f * px, 0.45f, true });
        }
    }

    // --- ROTARY CAB: the cabinet from above - the horn turning over the drum, the two mics ------------------------
    inline void rotary (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float hornA = s[0], drumA = s[1], hornHz = std::max (0.0f, s[2]), drumHz = std::max (0.0f, s[3]);
        const float hi = std::clamp (s[4] * 2.0f, 0.0f, 1.0f), lo = std::clamp (s[5] * 2.0f, 0.0f, 1.0f);
        const float cx = 0.45f * w, cy = 0.5f * h, R = std::min (0.40f * h, 0.30f * w);
        // the cabinet (square-ish, from above) and its louvres
        box (L, cx - R * 1.25f, cy - R * 1.15f, cx + R * 1.25f, cy + R * 1.15f, 1.6f * px, 0.7f);
        for (int k = 0; k < 6; ++k) { const float y = cy - R * 1.05f + R * 0.42f * (float) k; L.push_back ({ cx + R * 1.12f, y, cx + R * 1.20f, y, 1.0f * px, 0.3f }); }
        // the drum below (seen through the horn): a disc with its baffle's open side - a bright arc - turning
        ring (L, cx, cy, R * 0.92f, 1.2f * px, 0.35f);
        const float blurD = std::min (0.9f, drumHz * 0.05f);   // (fast: the rotor smears)
        for (int gh = 0; gh < 4; ++gh)
        {
            const float back = blurD * (float) gh / 3.0f, bright = (0.35f + 0.55f * lo) * (gh == 0 ? 1.0f : 0.35f);
            const float a = tau * drumA - back;
            ring (L, cx, cy, R * 0.86f, (2.0f + 2.5f * lo) * px, bright, 14, a - 0.9f, a + 0.9f);
        }
        // the horn: two arms (one the dummy), the live one with its flared mouth; ghosts when it is fast
        const float blurH = std::min (1.4f, hornHz * 0.12f);
        for (int gh = 3; gh >= 0; --gh)
        {
            const float a = tau * hornA - blurH * (float) gh / 3.0f;
            const float b = gh == 0 ? 1.0f : 0.25f;
            const float ex = std::cos (a), ey = std::sin (a), nx = -ey, ny = ex;
            const float len = R * 0.78f;
            L.push_back ({ cx - ex * len, cy - ey * len, cx + ex * len, cy + ey * len, 2.0f * px, 0.8f * b });
            const float mx = cx + ex * len, my = cy + ey * len, fl = R * (0.16f + 0.08f * hi);
            L.push_back ({ cx + ex * len * 0.6f + nx * fl * 0.2f, cy + ey * len * 0.6f + ny * fl * 0.2f, mx + nx * fl, my + ny * fl, 1.6f * px, b });
            L.push_back ({ cx + ex * len * 0.6f - nx * fl * 0.2f, cy + ey * len * 0.6f - ny * fl * 0.2f, mx - nx * fl, my - ny * fl, 1.6f * px, b });
            L.push_back ({ mx + nx * fl, my + ny * fl, mx - nx * fl, my - ny * fl, 1.6f * px, b });
            if (gh == 0)   // the sound leaving the mouth: arcs travelling out along it, as loud as the top is
                for (int k = 1; k <= 3; ++k)
                {
                    const float r = len + R * 0.12f * (float) k;
                    ring (L, cx, cy, r, 1.2f * px, hi * (1.0f - 0.28f * (float) k), 6, a - 0.22f, a + 0.22f);
                }
        }
        B.push_back ({ cx, cy, 4.0f * px, 1.0f, false });
        // the two mics, a quarter turn apart: right (L) and top (R), on their stands
        const float mr = R * 1.42f;
        const float mxs[2] { cx + mr, cx }, mys[2] { cy, cy - std::min (mr, 0.5f * h - 6.0f * px) };
        for (int m = 0; m < 2; ++m)
        {
            B.push_back ({ mxs[m], mys[m], 4.2f * px, 0.95f, true });
            B.push_back ({ mxs[m], mys[m], 1.6f * px, 0.95f, false });
        }
        L.push_back ({ mxs[0] + 4.2f * px, cy, mxs[0] + 12.0f * px, cy, 1.4f * px, 0.6f });
        // the speed: how fast the horn is going, a bar along the bottom
        const float bx0 = cx - R * 1.25f, bx1 = cx + R * 1.25f, by = h - 5.0f * px;
        L.push_back ({ bx0, by, bx0 + (bx1 - bx0) * std::clamp (hornHz / 6.7f, 0.0f, 1.0f), by, 2.2f * px, 0.9f });
    }

    // --- CASSETTE DECK: the cassette through its window - the reels (the tape moving from one to the other), the path,
    //     the head; the tape lifting off it in a dropout; everything slowing to a stop with STOP -------------------------
    inline void cassette (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float turn = s[0], played = std::clamp (s[1], 0.0f, 1.0f), speed = std::clamp (s[2], 0.0f, 1.0f);
        const float level = std::clamp (s[3] * 2.0f, 0.0f, 1.0f), drop = std::clamp (s[4], 0.0f, 1.0f);
        const int type = std::clamp ((int) s[5], 0, 2);
        const float sx0 = 0.06f * w, sx1 = 0.94f * w, sy0 = 0.10f * h, sy1 = 0.92f * h;
        // the shell, its window
        box (L, sx0, sy0, sx1, sy1, 1.6f * px, 0.75f);
        box (L, 0.24f * w, 0.24f * h, 0.76f * w, 0.60f * h, 1.0f * px, 0.3f);
        // the reels: the tape's pack on each by how much is played (by area), the hubs turning - the smaller pack faster
        const float rHub = 0.07f * h, rMax = std::min (0.25f * h, 0.17f * w);
        const float fr[2] { 1.0f - played, played };
        const float cxs[2] { 0.32f * w, 0.68f * w }, cy = 0.42f * h;
        float packR[2];
        for (int k = 0; k < 2; ++k)
        {
            packR[k] = std::sqrt (rHub * rHub + (rMax * rMax - rHub * rHub) * (0.08f + 0.92f * fr[k]));
            ring (L, cxs[k], cy, packR[k], 1.4f * px, 0.8f);
            ring (L, cxs[k], cy, packR[k] * 0.8f, 0.8f * px, 0.18f);
            ring (L, cxs[k], cy, rHub, 1.4f * px, 0.95f);
            const float a = tau * turn * (rMax / packR[k]) * 1.6f;
            for (int t = 0; t < 6; ++t)
            {
                const float ta = a + tau * (float) t / 6.0f;
                L.push_back ({ cxs[k] + std::cos (ta) * rHub * 0.45f, cy + std::sin (ta) * rHub * 0.45f, cxs[k] + std::cos (ta) * rHub, cy + std::sin (ta) * rHub, 1.2f * px, 0.9f });
            }
        }
        // the path: off the bottom of the left pack, round the guide, across the head, round the guide, onto the right pack
        const float gy = 0.80f * h, gxl = 0.18f * w, gxr = 0.82f * w, hx = 0.5f * w, headY = 0.86f * h;
        const float lift = drop * 0.05f * h;
        B.push_back ({ gxl, gy, 3.0f * px, 0.7f, true }); B.push_back ({ gxr, gy, 3.0f * px, 0.7f, true });
        L.push_back ({ cxs[0] - packR[0] * 0.7f, cy + packR[0] * 0.7f, gxl, gy + 3.0f * px, 1.4f * px, 0.85f });
        L.push_back ({ gxl, gy + 3.0f * px, hx - 0.07f * w, headY - lift * 0.5f, 1.4f * px, 0.85f });
        L.push_back ({ hx - 0.07f * w, headY - lift * 0.5f, hx + 0.07f * w, headY - lift * 0.5f, 1.4f * px, 0.85f });
        L.push_back ({ hx + 0.07f * w, headY - lift * 0.5f, gxr, gy + 3.0f * px, 1.4f * px, 0.85f });
        L.push_back ({ gxr, gy + 3.0f * px, cxs[1] + packR[1] * 0.7f, cy + packR[1] * 0.7f, 1.4f * px, 0.85f });
        // the head, under the tape: it glows as loud as it plays
        box (L, hx - 0.035f * w, headY + 1.5f * px, hx + 0.035f * w, headY + 0.06f * h, 1.4f * px, 0.9f);
        B.push_back ({ hx, headY + 0.03f * h, (1.5f + 3.0f * level) * px, 0.4f + 0.6f * level * speed, false });
        // the capstan and its roller, turning with the tape
        const float cap = 0.62f * w;
        B.push_back ({ cap, headY + 0.02f * h, 2.5f * px, 0.9f, false });
        ring (L, cap + 7.0f * px, headY + 0.02f * h, 5.0f * px, 1.0f * px, 0.6f);
        // the type: I, II or IV, as that many bars in the top corner
        for (int k = 0; k <= type; ++k)
        {
            const float x = sx1 - (10.0f + 5.0f * (float) k) * px;
            L.push_back ({ x, sy0 + 6.0f * px, x, sy0 + 16.0f * px, 1.8f * px, 0.85f });
        }
        // stopped: a bar across the window
        if (speed < 0.02f) L.push_back ({ 0.44f * w, 0.20f * h, 0.56f * w, 0.20f * h, 3.0f * px, 1.0f });
    }

    // --- TAPE ECHO: the loop of tape, the record head and the three playback heads, the echoes carried round --------
    inline void tapeEcho (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float phase = s[0], inten = std::clamp (s[3], 0.0f, 1.2f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f);
        const int heads = (int) s[2], n = std::clamp ((int) s[5], 0, 32);
        // the loop: a stadium round the screen; u 0..1 round it, clockwise from the top left
        const float cx = 0.5f * w, cy = 0.5f * h, rr = 0.34f * h, a = std::min (0.30f * w, 0.45f * w - rr);
        const float straight = 2.0f * a, arc = 3.14159265f * rr, per = 2.0f * straight + 2.0f * arc;
        auto at = [&] (float u, float& x, float& y, float& nx, float& ny)
        {
            float d = frac (u) * per;
            if (d < straight) { x = cx - a + d; y = cy - rr; nx = 0.0f; ny = -1.0f; return; }
            d -= straight;
            if (d < arc) { const float t = -1.5708f + d / rr; x = cx + a + std::cos (t) * rr; y = cy + std::sin (t) * rr; nx = std::cos (t); ny = std::sin (t); return; }
            d -= arc;
            if (d < straight) { x = cx + a - d; y = cy + rr; nx = 0.0f; ny = 1.0f; return; }
            d -= straight;
            const float t = 1.5708f + d / rr; x = cx - a + std::cos (t) * rr; y = cy + std::sin (t) * rr; nx = std::cos (t); ny = std::sin (t);
        };
        const int segs = 96;
        for (int i = 0; i < segs; ++i)
        {
            float x0, y0, x1, y1, nx, ny;
            at ((float) i / (float) segs, x0, y0, nx, ny); at ((float) (i + 1) / (float) segs, x1, y1, nx, ny);
            L.push_back ({ x0, y0, x1, y1, 1.8f * px, 0.55f + 0.2f * level });
        }
        // the heads: erase (dim), record, then the three playbacks - lit when HEADS uses them
        auto uOf = [] (float pos) { return 0.08f + 0.80f * pos; };
        auto head = [&] (float u, float bright, bool big)
        {
            float x, y, nx, ny; at (u, x, y, nx, ny);
            const float tx = -ny, ty = nx, hw = (big ? 7.0f : 5.0f) * px, d0 = 3.0f * px, d1 = 13.0f * px;
            const float xa = x + nx * d0, ya = y + ny * d0;
            L.push_back ({ xa - tx * hw, ya - ty * hw, xa + tx * hw, ya + ty * hw, 1.6f * px, bright });
            L.push_back ({ xa + tx * hw, ya + ty * hw, xa + tx * hw + nx * (d1 - d0), ya + ty * hw + ny * (d1 - d0), 1.6f * px, bright });
            L.push_back ({ xa - tx * hw, ya - ty * hw, xa - tx * hw + nx * (d1 - d0), ya - ty * hw + ny * (d1 - d0), 1.6f * px, bright });
            L.push_back ({ xa - tx * hw + nx * (d1 - d0), ya - ty * hw + ny * (d1 - d0), xa + tx * hw + nx * (d1 - d0), ya + ty * hw + ny * (d1 - d0), 1.6f * px, bright });
        };
        head (0.03f, 0.3f, false);   // erase
        head (uOf (0.0f), 1.0f, true);   // record
        for (int k = 0; k < 3; ++k) head (uOf ((float) (k + 1) / 3.0f), (heads >> k) & 1 ? 1.0f : 0.25f, false);
        // the capstan on the bottom run, turning
        {
            float x, y, nx, ny; at (0.62f, x, y, nx, ny);
            const float r = 6.0f * px, ca = tau * phase * 6.0f;
            ring (L, x, y + r + 2.0f * px, r, 1.2f * px, 0.8f);
            L.push_back ({ x, y + r + 2.0f * px, x + std::cos (ca) * r, y + r + 2.0f * px + std::sin (ca) * r, 1.2f * px, 0.8f });
        }
        // the echoes: marks on the tape, carried round; each flares as it passes a playback head that is on
        for (int b = 0; b < n; ++b)
        {
            const float pos = std::clamp (s[6 + 2 * b], 0.0f, 1.0f), amp = std::clamp (s[7 + 2 * b], 0.0f, 1.0f);
            float x, y, nx, ny; at (uOf (pos), x, y, nx, ny);
            B.push_back ({ x, y, (1.5f + 3.0f * amp) * px, 0.35f + 0.65f * amp, false });
            for (int k = 0; k < 3; ++k)
                if (((heads >> k) & 1) && std::abs (pos - (float) (k + 1) / 3.0f) < 0.02f)
                    B.push_back ({ x, y, (6.0f + 8.0f * amp) * px, amp, true });
        }
        // INTENSITY: a bar in the middle - past 100 % it runs away (the bar's end marked)
        const float bx0 = cx - a * 0.6f, bx1 = cx + a * 0.6f;
        L.push_back ({ bx0, cy, bx1, cy, 1.0f * px, 0.25f });
        L.push_back ({ bx0, cy, bx0 + (bx1 - bx0) * std::min (1.0f, inten / 1.1f), cy, 3.0f * px, inten > 1.0f ? 1.0f : 0.8f });
        L.push_back ({ bx0 + (bx1 - bx0) / 1.1f, cy - 5.0f * px, bx0 + (bx1 - bx0) / 1.1f, cy + 5.0f * px, 1.0f * px, 0.6f });
    }


    // --- VALVE AMP: two valves - the preamp's and the power stage's - glowing as hard as they work, and the
    //     supply rail sagging under them --------------------------------------------------------------------
    inline void valveAmp (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float glow[2] { std::clamp (s[0], 0.0f, 1.0f), std::clamp (s[1], 0.0f, 1.0f) }, sag = std::clamp (s[2], 0.0f, 1.0f), drive = std::clamp (s[3], 0.0f, 1.0f);
        const float xs[2] { 0.26f * w, 0.54f * w }, tw = 0.09f * w, top = 0.22f * h, bot = 0.80f * h;
        for (int v = 0; v < 2; ++v)
        {
            const float cx = xs[v], g = glow[v];
            // the glass: straight sides, a domed top, the base
            L.push_back ({ cx - tw, bot, cx - tw, top + tw, 1.4f * px, 0.8f }); L.push_back ({ cx + tw, bot, cx + tw, top + tw, 1.4f * px, 0.8f });
            ring (L, cx, top + tw, tw, 1.4f * px, 0.8f, 16, 3.14159f, tau);
            box (L, cx - tw * 1.15f, bot, cx + tw * 1.15f, bot + 0.07f * h, 1.4f * px, 0.7f);
            for (int k = 0; k < 4; ++k) L.push_back ({ cx - tw * 0.6f + tw * 0.4f * (float) k, bot + 0.07f * h, cx - tw * 0.6f + tw * 0.4f * (float) k, bot + 0.11f * h, 1.2f * px, 0.6f });
            // the plate, the grid, the filament glowing
            box (L, cx - tw * 0.55f, top + tw * 0.9f, cx + tw * 0.55f, bot - 0.10f * h, 1.2f * px, 0.55f + 0.3f * g);
            for (int k = 0; k < 7; ++k) { const float y = top + tw * 1.1f + (bot - 0.12f * h - top - tw * 1.1f) * (float) k / 6.0f; L.push_back ({ cx - tw * 0.35f, y, cx + tw * 0.35f, y, 0.8f * px, 0.3f }); }
            const float fy0 = top + tw * 1.2f, fy1 = bot - 0.13f * h;
            for (int k = 0; k < 8; ++k)
            {
                const float y0 = fy0 + (fy1 - fy0) * (float) k / 8.0f, y1 = fy0 + (fy1 - fy0) * (float) (k + 1) / 8.0f;
                L.push_back ({ cx + ((k & 1) ? 0.12f : -0.12f) * tw, y0, cx + ((k & 1) ? -0.12f : 0.12f) * tw, y1, 1.4f * px, 0.45f + 0.55f * g });
            }
            B.push_back ({ cx, 0.5f * (fy0 + fy1), (0.3f + 0.5f * g) * tw, 0.25f + 0.5f * g, false });
            B.push_back ({ cx, 0.5f * (fy0 + fy1), (0.6f + 0.8f * g) * tw, 0.2f + 0.4f * g, true });
        }
        // the supply rail (B+), dipping as it sags, and the drive as a bar under the preamp
        const float rx0 = 0.72f * w, rx1 = 0.94f * w, ry = 0.25f * h;
        L.push_back ({ rx0, ry, 0.5f * (rx0 + rx1), ry + sag * 0.45f * h, 2.0f * px, 1.0f });
        L.push_back ({ 0.5f * (rx0 + rx1), ry + sag * 0.45f * h, rx1, ry, 2.0f * px, 1.0f });
        L.push_back ({ rx0, ry - 6.0f * px, rx0, ry + 6.0f * px, 1.2f * px, 0.6f }); L.push_back ({ rx1, ry - 6.0f * px, rx1, ry + 6.0f * px, 1.2f * px, 0.6f });
        L.push_back ({ rx0, 0.88f * h, rx0 + (rx1 - rx0) * drive, 0.88f * h, 3.0f * px, 0.9f });
        L.push_back ({ rx0, 0.88f * h, rx1, 0.88f * h, 1.0f * px, 0.25f });
    }

    // --- SPEAKER CAB: the speaker from the front (its cone moving), the mic where it is --------------------
    inline void speakerCab (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float cone = std::clamp (s[0], -1.0f, 1.0f), mic = std::clamp (s[1], 0.0f, 1.0f), dist = std::clamp (s[2], 0.0f, 1.0f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f);
        const int size = std::clamp ((int) s[3], 0, 2);
        const float cx = 0.40f * w, cy = 0.5f * h, R = 0.43f * h * (0.8f + 0.1f * (float) size);
        ring (L, cx, cy, R, 2.0f * px, 0.9f);
        for (int k = 0; k < 8; ++k) { const float a = tau * (float) k / 8.0f + 0.39f; B.push_back ({ cx + std::cos (a) * R * 0.95f, cy + std::sin (a) * R * 0.95f, 2.0f * px, 0.6f, true }); }
        ring (L, cx, cy, R * 0.86f, (1.5f + 1.5f * level) * px, 0.7f);
        for (int k = 1; k <= 3; ++k) ring (L, cx, cy, R * (0.78f - 0.14f * (float) k) * (1.0f + 0.03f * cone * (float) k), 1.0f * px, 0.3f + 0.2f * level);
        const float cap = R * 0.24f * (1.0f + 0.10f * cone);
        ring (L, cx, cy, cap, 1.8f * px, 0.95f);
        B.push_back ({ cx, cy, cap * 0.5f, 0.25f + 0.3f * level, false });
        // the mic: from the dust cap out to the edge, smaller the further back it is; its cable off to the right
        const float ma = -0.55f, mr = mic * R * 0.82f, mx = cx + std::cos (ma) * mr, my = cy + std::sin (ma) * mr, ms = R * (0.12f - 0.06f * dist);
        ring (L, mx, my, ms, 2.0f * px, 1.0f);
        B.push_back ({ mx, my, ms * 0.45f, 1.0f, false });
        L.push_back ({ mx + ms, my, w * 0.70f, my - 0.1f * h, 1.4f * px, 0.7f });
        // from the side: the speaker's baffle and the mic, as far off as DISTANCE
        const float sx = 0.80f * w, sy0 = 0.25f * h, sy1 = 0.75f * h;
        L.push_back ({ sx, sy0, sx, sy1, 2.0f * px, 0.8f });
        L.push_back ({ sx, 0.40f * h, sx + 6.0f * px + 4.0f * cone * px, 0.5f * h, 1.4f * px, 0.7f }); L.push_back ({ sx, 0.60f * h, sx + 6.0f * px + 4.0f * cone * px, 0.5f * h, 1.4f * px, 0.7f });
        const float sm = sx + (10.0f + 0.14f * w * dist) * px / px;
        B.push_back ({ sm, 0.5f * h + (mic - 0.5f) * 0.3f * h, 3.0f * px, 1.0f, false });
        L.push_back ({ sm, 0.5f * h + (mic - 0.5f) * 0.3f * h, sm + 0.06f * w, 0.5f * h + (mic - 0.5f) * 0.3f * h, 1.4f * px, 0.8f });
        L.push_back ({ sx - 6.0f * px, 0.88f * h, 0.96f * w, 0.88f * h, 1.0f * px, 0.35f });   // (the floor)
    }

    // --- RADIO: the dial (the needle off or on the station), what it receives, how strong ------------------
    inline void radio (const float* s, float w, float h, float time, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float tune = std::clamp (s[0], -1.0f, 1.0f), sig = std::clamp (s[1], 0.0f, 1.0f), stat = std::clamp (s[2], 0.0f, 1.0f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f);
        const int band = std::clamp ((int) s[3], 0, 2);
        const float x0 = 0.08f * w, x1 = 0.92f * w, y0 = 0.10f * h, y1 = 0.42f * h;
        box (L, x0, y0, x1, y1, 1.6f * px, 0.8f);
        for (int k = 0; k <= 40; ++k)
        {
            const float x = x0 + (x1 - x0) * (0.05f + 0.9f * (float) k / 40.0f), long_ = k % 5 == 0 ? 0.35f : 0.18f;
            L.push_back ({ x, y1 - (y1 - y0) * long_, x, y1 - 3.0f * px, 1.0f * px, k % 5 == 0 ? 0.7f : 0.35f });
        }
        const float station = x0 + (x1 - x0) * 0.5f;
        L.push_back ({ station - 5.0f * px, y0 + 4.0f * px, station + 5.0f * px, y0 + 4.0f * px, 1.4f * px, 0.6f });
        L.push_back ({ station, y0 + 4.0f * px, station, y0 + 10.0f * px, 1.4f * px, 0.6f });
        const float nx = station + tune * (x1 - x0) * 0.4f;
        L.push_back ({ nx, y0 + 2.0f * px, nx, y1 - 2.0f * px, 3.0f * px, 1.0f });
        // the bands: three marks, the one it is on lit
        for (int k = 0; k < 3; ++k) { const float bx = x0 + 10.0f * px + 14.0f * px * (float) k; B.push_back ({ bx, y0 - 6.0f * px + 0.0f, 3.2f * px, k == band ? 1.0f : 0.4f, k != band }); }
        // what it receives: the programme, clean on the station, buried in static off it
        const float wy = 0.68f * h, wa = 0.14f * h;
        const int segs = 90;
        float lx = x0, ly = wy;
        unsigned rs = (unsigned) (time * 30.0f) * 2654435761u;
        for (int i = 0; i <= segs; ++i)
        {
            const float t = (float) i / (float) segs, x = x0 + (x1 - x0) * t;
            rs = rs * 1664525u + 1013904223u;
            const float noise = ((float) (rs >> 8) / 16777216.0f - 0.5f) * 2.0f;
            const float v = sig * (0.6f + 0.4f * level) * std::sin (t * 23.0f + time * 7.0f) * std::sin (t * 5.0f + time * 1.3f) + stat * 0.7f * noise;
            const float y = wy - wa * std::clamp (v, -1.0f, 1.0f);
            if (i > 0) L.push_back ({ lx, ly, x, y, 1.4f * px, 0.9f });
            lx = x; ly = y;
        }
        // the signal strength: five bars
        for (int k = 0; k < 5; ++k)
        {
            const float bx = x1 - 6.0f * px - 7.0f * px * (float) (4 - k), bh = (4.0f + 3.5f * (float) k) * px;
            L.push_back ({ bx, 0.95f * h, bx, 0.95f * h - bh, 3.0f * px, sig > (float) k / 5.0f ? 1.0f : 0.2f });
        }
    }

    // --- PENDULUM: the pendulum swinging from its pivot, how far it is kept swinging, what it moves ------------
    inline void pendulum (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float th = std::clamp (s[0], -1.4f, 1.4f), amp = std::clamp (s[1], 0.0f, 1.4f), len = std::clamp (s[2], 0.1f, 3.0f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f), kick = std::clamp (s[5], 0.0f, 1.0f);
        const int mode = std::clamp ((int) s[3], 0, 2);
        const float cx = 0.46f * w, cy = 0.10f * h, rod = 0.30f * h + 0.52f * h * (len - 0.1f) / 2.9f;
        L.push_back ({ cx - 0.08f * w, cy, cx + 0.08f * w, cy, 2.0f * px, 0.8f });
        B.push_back ({ cx, cy, 3.0f * px, 1.0f, true });
        ring (L, cx, cy, rod, 1.0f * px, 0.3f, 32, 1.5708f - amp, 1.5708f + amp);   // (the swing it is kept at)
        for (int k = 3; k >= 1; --k)   // (a trail)
        {
            const float a = th * (1.0f - 0.06f * (float) k);
            B.push_back ({ cx + std::sin (a) * rod, cy + std::cos (a) * rod, 4.0f * px, 0.15f * (float) (4 - k) / 3.0f, false });
        }
        const float bx = cx + std::sin (th) * rod, by = cy + std::cos (th) * rod, br = (0.045f * h + 0.015f * h * level);
        L.push_back ({ cx, cy, bx, by, 2.0f * px, 0.95f });
        B.push_back ({ bx, by, br, 1.0f, true });
        B.push_back ({ bx, by, br * 0.55f, 0.9f, false });
        if (kick > 0.05f) B.push_back ({ bx, by, br * (1.4f + kick), kick, true });
        // what it moves
        const float mm = amp > 0.01f ? std::clamp (th / amp, -1.0f, 1.0f) : 0.0f;
        if (mode == 1)
            for (int side = 0; side < 2; ++side)
            {
                const float sx = side == 0 ? 0.06f * w : 0.86f * w, on = side == 0 ? 0.5f - 0.5f * mm : 0.5f + 0.5f * mm;
                box (L, sx, 0.55f * h, sx + 0.08f * w, 0.80f * h, 1.4f * px, 0.4f + 0.6f * on);
                ring (L, sx + 0.04f * w, 0.675f * h, 0.03f * w * (0.6f + 0.4f * on), 1.2f * px, 0.4f + 0.6f * on);
            }
        else if (mode == 0)
            L.push_back ({ 0.08f * w, 0.95f * h, 0.08f * w + 0.84f * w * (1.0f - 0.7f * mm * mm), 0.95f * h, 3.0f * px, 0.9f });
        else
        {
            const float knee = 0.85f * w - 0.25f * w * mm * mm, y = 0.93f * h;
            L.push_back ({ 0.55f * w, y - 0.12f * h, knee, y - 0.12f * h, 1.6f * px, 0.9f });
            L.push_back ({ knee, y - 0.12f * h, knee + 0.08f * w, y, 1.6f * px, 0.9f });
        }
    }

    // --- BOUNCE DELAY: the ball dropped by the last hit, bouncing - each bounce an echo, marked where it lands ----
    inline void bounce (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float age = std::max (0.0f, s[0]), t0 = std::max (0.05f, s[1]), e = std::clamp (s[2], 0.3f, 0.95f), drop = std::clamp (s[3], 0.0f, 1.0f);
        const float gy = 0.86f * h, gx0 = 0.06f * w, gx1 = 0.94f * w, H = 0.72f * h * (0.35f + 0.65f * drop);
        L.push_back ({ gx0, gy, gx1, gy, 2.0f * px, 0.9f });
        for (int k = 0; k < 24; ++k) { const float x = gx0 + (gx1 - gx0) * (float) k / 23.0f; L.push_back ({ x, gy, x - 5.0f * px, gy + 6.0f * px, 1.0f * px, 0.3f }); }
        // the bounces: the fall takes half the first gap (it was dropped), then gaps shrinking by e
        float total = 0.5f * t0, gap = t0; for (int k = 0; k < 40 && gap > 0.012f; ++k) { total += gap; gap *= e; }
        auto xAt = [&] (float t) { return gx0 + 12.0f * px + (gx1 - gx0 - 24.0f * px) * std::min (1.0f, t / total); };
        float bxv = xAt (std::min (age, total)), byv = gy;
        if (age < 0.5f * t0) { const float u = age / (0.5f * t0); byv = gy - H * (1.0f - u * u); }
        else
        {
            float t = 0.5f * t0; gap = t0; float hh = H; int k = 0;
            B.push_back ({ xAt (t), gy, 5.0f * px, age < total ? 0.9f : 0.4f, true });
            while (k < 40 && gap > 0.012f && t + gap < age) { t += gap; gap *= e; hh *= e * e; ++k; B.push_back ({ xAt (t), gy, (5.0f * std::pow (e, (float) k) + 1.5f) * px, 0.4f + 0.5f * std::pow (e, (float) k), true }); }
            if (age < total) { const float u = (age - t) / gap; byv = gy - hh * 4.0f * u * (1.0f - u); }
        }
        const float br = 0.045f * h;
        B.push_back ({ bxv, byv - br, br, 1.0f, true });
        B.push_back ({ bxv, byv - br, br * 0.5f, 0.8f, false });
        // its path so far, dotted
        for (int i = 1; i < 60; ++i)
        {
            const float t = age * (float) i / 60.0f; if (t > total) break;
            float y = gy;
            if (t < 0.5f * t0) { const float u = t / (0.5f * t0); y = gy - H * (1.0f - u * u); }
            else { float tt = 0.5f * t0, g2 = t0, hh = H; int k = 0; while (k < 40 && tt + g2 < t) { tt += g2; g2 *= e; hh *= e * e; ++k; } const float u = (t - tt) / g2; y = gy - hh * 4.0f * u * (1.0f - u); }
            B.push_back ({ xAt (t), y - br, 1.2f * px, 0.35f, false });
        }
    }

    // --- SYMPATHY: the six strings between nut and bridge, each vibrating as much as it rings -----------------
    inline void sympathy (const float* s, float w, float h, float time, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float x0 = 0.08f * w, x1 = 0.92f * w, y0 = 0.18f * h, y1 = 0.82f * h;
        L.push_back ({ x0, y0 - 0.06f * h, x0, y1 + 0.06f * h, 3.0f * px, 0.9f });
        L.push_back ({ x1, y0 - 0.06f * h, x1, y1 + 0.06f * h, 2.0f * px, 0.9f });
        for (int k = 0; k < 6; ++k)
        {
            const float amp = std::clamp (s[k], 0.0f, 1.0f), hz = std::max (20.0f, s[7 + k]);
            const float y = y0 + (y1 - y0) * (float) k / 5.0f, a = amp * 0.05f * h;
            const float f = 3.0f + 2.0f * std::log2 (hz / 40.0f);   // (seen slowed: lower strings slower)
            const float c = std::cos (tau * f * time), c2 = std::cos (tau * f * 2.0f * time + 1.0f);
            const int segs = 48; float lx = x0, ly = y;
            for (int i = 1; i <= segs; ++i)
            {
                const float t = (float) i / (float) segs, x = x0 + (x1 - x0) * t;
                const float yy = y + a * (std::sin (3.14159f * t) * c + 0.35f * std::sin (6.2832f * t) * c2);
                L.push_back ({ lx, ly, x, yy, (2.6f - 0.3f * (float) k) * px, 0.45f + 0.55f * amp });
                lx = x; ly = yy;
            }
            B.push_back ({ x0 + 4.0f * px, y, 2.0f * px, 0.8f, false });
            B.push_back ({ x1 - 4.0f * px, y, 2.0f * px, 0.8f, false });
        }
        // the level along the bottom
        L.push_back ({ x0, 0.95f * h, x0 + (x1 - x0) * std::clamp (s[6] * 4.0f, 0.0f, 1.0f), 0.95f * h, 2.4f * px, 0.8f });
    }

    // --- FLYBY: its path from above, you in the middle of the bottom, the source on it with the wavefronts it
    //     left behind - bunched up ahead of it, spread out behind (Doppler, seen) ----------------------------------
    inline void flyby (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        namespace U = enh::dsp::units;
        const float px = std::max (1.0f, w / 480.0f);
        const int path = std::clamp ((int) s[2], 0, 2);
        const float d = std::clamp (s[3], 1.0f, 50.0f), u = frac (s[6]), speed = std::clamp (s[7], 1.0f, 100.0f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f);
        // the scale: the path and you fit the screen
        float xmin = -1.0f, xmax = 1.0f, ymax = 1.0f;
        for (int i = 0; i < 64; ++i) { float x, y; U::Flyby::at (path, d, (float) i / 64.0f, x, y); xmin = std::min (xmin, x); xmax = std::max (xmax, x); ymax = std::max (ymax, y); }
        const float sc = std::min (0.88f * w / (xmax - xmin), 0.80f * h / ymax);
        const float ox = 0.5f * w - 0.5f * (xmin + xmax) * sc, oy = 0.90f * h;
        auto X = [&] (float x) { return ox + x * sc; };
        auto Y = [&] (float y) { return oy - y * sc; };
        float lx = 0, ly = 0;
        for (int i = 0; i <= 96; ++i)
        {
            float x, y; U::Flyby::at (path, d, (float) i / 96.0f, x, y);
            if (i > 0 && ! (path == 0 && i == 96)) L.push_back ({ X (lx), Y (ly), X (x), Y (y), 1.0f * px, 0.3f });
            lx = x; ly = y;
        }
        // you
        B.push_back ({ X (0.0f), Y (0.0f), 6.0f * px, 1.0f, true });
        L.push_back ({ X (0.0f), Y (0.0f) - 6.0f * px, X (0.0f), Y (0.0f) - 11.0f * px, 1.4f * px, 1.0f });
        // the wavefronts: one left every 40 ms, as far out as sound has gone since
        const float len = U::Flyby::lengthOf (path, d);
        // (spaced so the first is a twentieth of the screen out: time is slowed for the picture, the source's
        // move between them and the wavefronts' growth slowed alike, so the bunching ahead of it is true)
        const float step = 0.05f * w / (343.0f * sc);
        for (int k = 1; k <= 7; ++k)
        {
            const float dt = step * (float) k, uk = u - dt * speed / len;
            if (path == 0 && uk < 0.0f) break;
            float x, y; U::Flyby::at (path, d, frac (uk), x, y);
            const float r = 343.0f * dt * sc;
            if (r > 1.2f * w) break;
            ring (L, X (x), Y (y), r, 1.0f * px, (0.6f - 0.07f * (float) k) * (0.4f + 0.6f * level));
        }
        float x, y; U::Flyby::at (path, d, u, x, y);
        B.push_back ({ X (x), Y (y), 5.0f * px, 1.0f, false });
        B.push_back ({ X (x), Y (y), 8.0f * px, 0.6f, true });
    }

    // --- TESLA COIL: the coil and its toroid, the arcs striking out of it (as many and as far as it plays) -----
    inline void tesla (const float* s, float w, float h, float time, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float arc = std::clamp (s[0], 0.0f, 1.0f), volt = std::clamp (s[3], 0.0f, 1.0f);
        const float cx = 0.5f * w, ty = 0.40f * h, rx = 0.15f * w, ry = 0.06f * h;
        // the coil: its former and windings, the base
        box (L, cx - 0.05f * w, ty + ry, cx + 0.05f * w, 0.88f * h, 1.4f * px, 0.8f);
        for (float y = ty + ry + 4.0f * px; y < 0.87f * h; y += 3.5f * px) L.push_back ({ cx - 0.05f * w, y, cx + 0.05f * w, y + 1.0f * px, 0.8f * px, 0.35f });
        box (L, cx - 0.12f * w, 0.88f * h, cx + 0.12f * w, 0.95f * h, 1.4f * px, 0.7f);
        // the toroid (an ellipse and its inner edge)
        for (int i = 0; i < 40; ++i)
        {
            const float a0 = tau * (float) i / 40.0f, a1 = tau * (float) (i + 1) / 40.0f;
            L.push_back ({ cx + rx * std::cos (a0), ty + ry * std::sin (a0), cx + rx * std::cos (a1), ty + ry * std::sin (a1), 2.0f * px, 0.9f });
            L.push_back ({ cx + 0.45f * rx * std::cos (a0), ty + 0.3f * ry * std::sin (a0), cx + 0.45f * rx * std::cos (a1), ty + 0.3f * ry * std::sin (a1), 1.0f * px, 0.4f });
        }
        // the arcs: random walks out of the toroid's rim, forking; new every 50 ms
        unsigned rs = (unsigned) s[2] * 7919u + (unsigned) (time * 20.0f) * 2654435761u + 1u;
        auto rnd = [&] { rs = rs * 1664525u + 1013904223u; return (float) (rs >> 8) / 16777216.0f; };
        const int arcs = arc > 0.02f ? 2 + (int) (arc * 7.0f) : 0;
        const float reach = (0.12f + 0.35f * arc * (0.5f + 0.5f * volt)) * w;
        for (int a = 0; a < arcs; ++a)
        {
            const float ang = 3.14159f + 3.14159f * rnd();   // (upward half)
            float x = cx + rx * std::cos (ang), y = ty + ry * std::sin (ang), dir = ang;
            const int steps = 6 + (int) (rnd() * 8.0f);
            const float stepL = reach / (float) steps;
            for (int k = 0; k < steps; ++k)
            {
                dir += (rnd() - 0.5f) * 1.2f;
                const float nx = x + std::cos (dir) * stepL, ny = y + std::sin (dir) * stepL;
                L.push_back ({ x, y, nx, ny, (2.2f - 1.4f * (float) k / (float) steps) * px, arc * (1.0f - 0.5f * (float) k / (float) steps) });
                if (rnd() < 0.25f)   // a fork
                {
                    const float fd = dir + (rnd() - 0.5f) * 1.8f;
                    L.push_back ({ nx, ny, nx + std::cos (fd) * stepL * 0.8f, ny + std::sin (fd) * stepL * 0.8f, 1.0f * px, arc * 0.6f });
                }
                x = nx; y = ny;
            }
            B.push_back ({ x, y, 2.0f * px, arc, false });
        }
        if (arcs > 0) B.push_back ({ cx, ty, rx * 1.1f, 0.25f * arc, true });
    }

    // --- TALK BOX: the mouth from the front, opening and shaping each vowel; its three formants on the right ------
    inline void talkBox (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const float v = std::clamp (s[0], 0.0f, 4.0f), level = std::clamp (s[4] * 2.0f, 0.0f, 1.0f);
        static constexpr float shape[5][2] { { 0.20f, 0.22f }, { 0.24f, 0.11f }, { 0.26f, 0.05f }, { 0.13f, 0.19f }, { 0.08f, 0.10f } };
        const int a = std::min (3, (int) v); const float t = v - (float) a;
        const float mw = (shape[a][0] + (shape[a + 1][0] - shape[a][0]) * t) * w * 0.9f, mh = (shape[a][1] + (shape[a + 1][1] - shape[a][1]) * t) * h * 1.3f * (0.85f + 0.15f * level);
        const float cx = 0.30f * w, cy = 0.50f * h;
        auto ellipse = [&] (float rx, float ry, float th, float br)
        {
            for (int i = 0; i < 48; ++i)
            {
                const float a0 = tau * (float) i / 48.0f, a1 = tau * (float) (i + 1) / 48.0f;
                L.push_back ({ cx + rx * std::cos (a0), cy + ry * std::sin (a0), cx + rx * std::cos (a1), cy + ry * std::sin (a1), th, br });
            }
        };
        ellipse (mw * 0.5f + 0.05f * w, mh * 0.5f + 0.06f * h, 2.2f * px, 0.9f);   // the lips
        ellipse (mw * 0.5f, std::max (1.5f * px, mh * 0.5f), 1.4f * px, 0.7f);       // the opening
        if (mh > 0.1f * h)   // the teeth, when it is open enough to see them
            L.push_back ({ cx - mw * 0.35f, cy - mh * 0.5f + 4.0f * px, cx + mw * 0.35f, cy - mh * 0.5f + 4.0f * px, 2.0f * px, 0.6f });
        // the vowels along the bottom, the one it is at lit
        for (int k = 0; k < 5; ++k) { const float x = 0.10f * w + 0.40f * w * (float) k / 4.0f; B.push_back ({ x, 0.92f * h, 3.0f * px, 1.0f - std::min (1.0f, std::abs (v - (float) k)) * 0.7f, std::abs (v - (float) k) > 0.5f }); }
        L.push_back ({ 0.10f * w + 0.40f * w * v / 4.0f, 0.86f * h, 0.10f * w + 0.40f * w * v / 4.0f, 0.89f * h, 2.0f * px, 1.0f });
        // the formants: peaks on a log frequency axis (100 Hz - 5 kHz)
        const float fx0 = 0.60f * w, fx1 = 0.95f * w, fy = 0.80f * h, fh = 0.55f * h;
        L.push_back ({ fx0, fy, fx1, fy, 1.0f * px, 0.4f });
        float lx = fx0, ly = fy;
        for (int i = 1; i <= 80; ++i)
        {
            const float tt = (float) i / 80.0f, hz = 100.0f * std::pow (50.0f, tt);
            float mag = 0.0f;
            for (int k = 0; k < 3; ++k) { const float f = std::max (50.0f, s[1 + k]), q = std::log2 (hz / f) * (3.0f + 1.5f * (float) k); mag += (k == 0 ? 1.0f : k == 1 ? 0.7f : 0.4f) / (1.0f + q * q); }
            const float x = fx0 + (fx1 - fx0) * tt, y = fy - fh * std::min (1.0f, mag) * (0.6f + 0.4f * level);
            L.push_back ({ lx, ly, x, y, 1.6f * px, 0.9f });
            lx = x; ly = y;
        }
    }

    // --- LAVA LAMP: the lamp, its wax blobs where they are (warmer: brighter), the heater under them --------
    inline void lavaLamp (const float* s, float w, float h, std::vector<Line>& L, std::vector<Blob>& B)
    {
        using namespace detail;
        const float px = std::max (1.0f, w / 480.0f);
        const int n = std::clamp ((int) s[0], 0, 6);
        const float cx = 0.5f * w, yTop = 0.14f * h, yBot = 0.80f * h, hwTop = 0.07f * w, hwBot = 0.14f * w;
        auto half = [&] (float y) { const float t = (y - yTop) / (yBot - yTop); return hwTop + (hwBot - hwTop) * t; };
        L.push_back ({ cx - hwTop, yTop, cx - hwBot, yBot, 1.8f * px, 0.9f }); L.push_back ({ cx + hwTop, yTop, cx + hwBot, yBot, 1.8f * px, 0.9f });
        // the cap and the base
        L.push_back ({ cx - hwTop, yTop, cx - 0.04f * w, 0.04f * h, 1.6f * px, 0.8f }); L.push_back ({ cx + hwTop, yTop, cx + 0.04f * w, 0.04f * h, 1.6f * px, 0.8f });
        L.push_back ({ cx - 0.04f * w, 0.04f * h, cx + 0.04f * w, 0.04f * h, 1.6f * px, 0.8f });
        L.push_back ({ cx - hwBot, yBot, cx - 0.10f * w, 0.96f * h, 1.6f * px, 0.8f }); L.push_back ({ cx + hwBot, yBot, cx + 0.10f * w, 0.96f * h, 1.6f * px, 0.8f });
        L.push_back ({ cx - 0.10f * w, 0.96f * h, cx + 0.10f * w, 0.96f * h, 1.6f * px, 0.8f });
        L.push_back ({ cx - hwBot, yBot, cx + hwBot, yBot, 1.2f * px, 0.6f });
        B.push_back ({ cx, yBot - 4.0f * px, 0.06f * w, 0.25f, false });   // (the bulb's warmth)
        // the wax (a pool at the bottom, and the blobs)
        L.push_back ({ cx - hwBot * 0.9f, yBot - 5.0f * px, cx + hwBot * 0.9f, yBot - 5.0f * px, 4.0f * px, 0.6f });
        for (int b = 0; b < n; ++b)
        {
            const float bx = std::clamp (s[1 + 4 * b], -1.0f, 1.0f), by = std::clamp (s[2 + 4 * b], 0.0f, 1.0f), br = std::clamp (s[3 + 4 * b], 0.1f, 1.0f), heat = std::clamp (s[4 + 4 * b], 0.0f, 1.0f);
            const float y = yBot - 0.05f * h - (yBot - yTop - 0.10f * h) * by;
            const float r = (0.025f + 0.04f * br) * h;
            const float x = cx + bx * std::max (0.0f, half (y) - r);
            B.push_back ({ x, y, r, 0.35f + 0.55f * heat, false });
            B.push_back ({ x, y, r, 0.9f, true });
        }
    }

    /** The screen of unit `k` from its state `s` (w x h pixels) at `time` s. */
    inline void build (Kind k, const float* s, float w, float h, float time, std::vector<Line>& lines, std::vector<Blob>& blobs)
    {
        lines.clear(); blobs.clear();
        switch (k)
        {
            case Kind::vinyl:    vinyl (s, w, h, lines, blobs); break;
            case Kind::rotary:   rotary (s, w, h, lines, blobs); break;
            case Kind::cassette: cassette (s, w, h, lines, blobs); break;
            case Kind::tapeEcho: tapeEcho (s, w, h, lines, blobs); break;
            case Kind::valveAmp: valveAmp (s, w, h, lines, blobs); break;
            case Kind::speakerCab: speakerCab (s, w, h, lines, blobs); break;
            case Kind::radio:    radio (s, w, h, time, lines, blobs); break;
            case Kind::pendulum: pendulum (s, w, h, lines, blobs); break;
            case Kind::bounce:   bounce (s, w, h, lines, blobs); break;
            case Kind::sympathy: sympathy (s, w, h, time, lines, blobs); break;
            case Kind::flyby:    flyby (s, w, h, lines, blobs); break;
            case Kind::tesla:    tesla (s, w, h, time, lines, blobs); break;
            case Kind::talkBox:  talkBox (s, w, h, lines, blobs); break;
            case Kind::lavaLamp: lavaLamp (s, w, h, lines, blobs); break;
            case Kind::none: break;
        }
    }

    /** A made-up state at time t, in the layouts Sims.h publishes (out: at least 192 floats). */
    inline void demo (Kind k, float t, float* o)
    {
        using detail::frac;
        std::fill (o, o + 192, 0.0f);
        switch (k)
        {
            case Kind::vinyl:
                o[0] = frac (t * 0.5556f); o[1] = frac (t * 0.02f); o[2] = 0.5f + 0.3f * std::sin (t * 2.0f); o[3] = 0.0f; o[4] = 12.0f;
                for (int i = 0; i < 12; ++i)
                {
                    const float ang = frac (0.37f * (float) i + 0.11f);
                    o[5 + 2 * i] = ang;
                    const float since = frac (o[0] - ang);   // (turns since the stylus passed it)
                    o[6 + 2 * i] = std::exp (-since * 4.0f);
                }
                break;
            case Kind::rotary:
            {
                const bool fast = std::fmod (t, 12.0f) > 6.0f;
                o[0] = frac (t * (fast ? 6.7f : 0.8f)); o[1] = frac (t * (fast ? 5.9f : 0.66f)); o[2] = fast ? 6.7f : 0.8f; o[3] = fast ? 5.9f : 0.66f;
                o[4] = 0.3f + 0.15f * std::sin (t * 3.0f); o[5] = 0.3f + 0.1f * std::sin (t * 1.3f); o[6] = fast ? 1.0f : 0.0f;
                break;
            }
            case Kind::cassette:
            {
                const float c = std::fmod (t, 10.0f);
                const float sp = c < 7.0f ? 1.0f : c < 8.0f ? 1.0f - (c - 7.0f) : c < 9.0f ? 0.0f : c - 9.0f;
                o[0] = frac (t * 0.35f); o[1] = 0.3f + 0.01f * t; o[2] = sp; o[3] = 0.3f + 0.15f * std::sin (t * 2.3f);
                o[4] = std::max (0.0f, std::sin (t * 0.7f) - 0.9f) * 6.0f; o[5] = 1.0f;
                break;
            }
            case Kind::tapeEcho:
            {
                o[0] = frac (t * 1.1f); o[1] = 0.3f; o[2] = 7.0f; o[3] = 0.55f; o[4] = 0.4f; o[5] = 8.0f;
                for (int b = 0; b < 8; ++b)
                {
                    const float pos = frac (t * 1.1f + 0.125f * (float) b);
                    o[6 + 2 * b] = pos; o[7 + 2 * b] = 0.9f * std::pow (0.55f, pos * 3.0f) * (b % 3 == 0 ? 1.0f : 0.6f);
                }
                break;
            }
            case Kind::valveAmp: o[0] = 0.5f + 0.4f * std::sin (t * 2.0f); o[1] = 0.5f + 0.4f * std::sin (t * 1.7f + 1.0f); o[2] = 0.3f + 0.25f * std::sin (t * 1.1f); o[3] = 0.4f; o[4] = 0.3f; break;
            case Kind::speakerCab: o[0] = std::sin (t * 9.0f) * 0.7f; o[1] = 0.5f + 0.4f * std::sin (t * 0.3f); o[2] = 0.3f; o[3] = 1.0f; o[4] = 0.3f + 0.2f * std::sin (t * 3.0f); break;
            case Kind::radio: o[0] = 0.4f * std::sin (t * 0.25f); o[1] = 1.0f - 1.6f * o[0] * o[0]; o[2] = 0.2f + 0.8f * std::abs (o[0]); o[3] = 0.0f; o[4] = 0.3f; o[5] = 1.0f; break;
            case Kind::pendulum: o[1] = 0.7f; o[2] = 1.0f; o[0] = 0.7f * std::sin (t * 3.13f); o[3] = 1.0f; o[4] = 0.3f; o[5] = std::max (0.0f, std::sin (t * 1.5f) - 0.9f) * 8.0f; break;
            case Kind::bounce: o[0] = std::fmod (t, 4.0f); o[1] = 0.45f; o[2] = 0.72f; o[3] = 0.8f; o[4] = 0.3f; break;
            case Kind::sympathy:
                for (int k = 0; k < 6; ++k) { o[k] = 0.5f + 0.45f * std::sin (t * (0.7f + 0.2f * (float) k) + (float) k); o[7 + k] = 82.4f * std::pow (2.0f, (float) k * 7.0f / 12.0f); }
                o[6] = 0.1f; break;
            case Kind::flyby: o[2] = 1.0f; o[3] = 8.0f; o[6] = frac (t * 30.0f / enh::dsp::units::Flyby::lengthOf (1, 8.0f)); o[7] = 30.0f; o[4] = 0.4f;
                enh::dsp::units::Flyby::at (1, 8.0f, o[6], o[0], o[1]); break;
            case Kind::tesla: o[0] = std::max (0.0f, std::sin (t * 1.9f)) * (0.6f + 0.4f * std::sin (t * 7.0f)); o[1] = 0.3f; o[2] = (float) ((int) (t * 20.0f) % 1000); o[3] = 0.6f; break;
            case Kind::talkBox: o[0] = 2.0f + 2.0f * std::sin (t * 0.8f); o[1] = 600.0f + 200.0f * std::sin (t * 0.8f); o[2] = 1500.0f + 700.0f * std::sin (t * 0.8f + 1.0f); o[3] = 2600.0f; o[4] = 0.3f; o[5] = 0.5f; break;
            case Kind::lavaLamp:
                o[0] = 4.0f;
                for (int b = 0; b < 4; ++b) { o[1 + 4 * b] = 0.5f * std::sin (t * 0.1f + (float) b * 2.0f); o[2 + 4 * b] = 0.5f + 0.45f * std::sin (t * (0.12f + 0.03f * (float) b) + (float) b * 1.7f); o[3 + 4 * b] = 0.4f + 0.15f * (float) b; o[4 + 4 * b] = 0.5f + 0.5f * std::cos (t * 0.12f + (float) b); }
                break;
            case Kind::none: break;
        }
    }
}
