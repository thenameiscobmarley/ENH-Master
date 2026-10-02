#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

/*  THE PATCH BAY's cords, and what they make of the signal (shared by the UI, which draws and moves them, and the
    processor, which keeps them with the session and tells the engine).

    A cord has two ends; each is plugged into a port - a unit's OUT or IN, left or right - or loose (held in the
    hand, or pulled out and hanging). The rack's own ends are RACK IN (its outputs feed the first unit) and
    RACK OUT (its inputs take what the last unit sends). Following the left channel from RACK IN, cord by cord,
    gives the signal's path: the units in it, in order; a unit left out of it is passed by; a path that never
    reaches RACK OUT leaves the rack silent (safely - the engine fades); one that comes back on itself is a loop. */
namespace enh::patch
{
    inline constexpr int rack = -1;    // the rack's own ends: RACK IN (OUT ports), RACK OUT (IN ports)
    inline constexpr int loose = -2;   // not plugged in
    /** A spare jack on the bay (nothing wired behind it): unit spareBase - column. Signal into it goes nowhere. */
    inline constexpr int spareBase = -100;

    enum Port : int8_t { outL = 0, outR = 1, inL = 2, inR = 3 };

    struct End
    {
        int16_t unit = loose;
        int8_t port = outL;
        bool operator== (const End& o) const noexcept { return unit == o.unit && (unit == loose || port == o.port); }
        bool plugged() const noexcept { return unit != loose; }
        bool isOut() const noexcept { return port == outL || port == outR; }
        int channel() const noexcept { return port & 1; }
    };

    struct Cord { End a, b; };   // a: where it came out of (an OUT), b: where it goes in (an IN) - as first patched

    struct State
    {
        std::vector<Cord> cords;
        bool anything = false;   // the master switch: anything into anything (feedback allowed, ear-guarded)
    };

    /** The rack patched straight through: RACK IN -> each unit in `units` (in order) -> RACK OUT, both sides. */
    inline State straightThrough (const std::vector<int>& units)
    {
        State s;
        int prev = rack;
        for (size_t i = 0; i <= units.size(); ++i)
        {
            const int next = i < units.size() ? units[i] : rack;
            for (int ch = 0; ch < 2; ++ch)
                s.cords.push_back ({ { (int16_t) prev, (int8_t) (outL + ch) }, { (int16_t) next, (int8_t) (inL + ch) } });
            prev = next;
        }
        return s;
    }

    /** What a channel's cords make of it. */
    struct Path
    {
        std::vector<int> units;   // in signal order
        bool complete = false;    // it reaches RACK OUT
        bool loop = false;        // it came back to a unit already in it
    };

    /** The cord plugged into a port, and its other end (nullptr / loose: none). */
    inline const End* otherEnd (const State& s, End at) noexcept
    {
        for (auto& c : s.cords)
        {
            if (c.a.plugged() && c.a == at) return &c.b;
            if (c.b.plugged() && c.b == at) return &c.a;
        }
        return nullptr;
    }

    inline Path follow (const State& s, int channel)
    {
        Path p;
        End at { (int16_t) rack, (int8_t) (outL + channel) };
        for (int guard = 0; guard < 256; ++guard)
        {
            const End* to = otherEnd (s, at);
            if (to == nullptr || ! to->plugged())
                return p;                                   // a cord pulled out, or nothing there: silence
            if (to->isOut())
            {
                // out into an out (anything-into-anything): nothing flows that way - treat as broken
                return p;
            }
            if (to->unit <= spareBase)
                return p;                                   // into a spare jack: nowhere
            if (to->unit == rack)
            {
                p.complete = true;
                return p;
            }
            for (int u : p.units)
                if (u == to->unit)
                {
                    p.loop = true;
                    return p;
                }
            p.units.push_back (to->unit);
            at = { to->unit, (int8_t) (outL + to->channel()) };   // the unit's OUT on the same side
        }
        return p;
    }

    inline bool isFree (const State& s, End port) noexcept { return otherEnd (s, port) == nullptr; }

    /** Keeps the cords right as units come into the rack and leave it (`units`: those in it, in order): a unit
        taken out is bridged (what went into it goes on to where it went), one put in is patched in just before
        RACK OUT. Returns true if it changed anything. */
    inline bool reconcile (State& s, const std::vector<int>& units)
    {
        if (s.cords.empty())
            return false;
        auto inRack = [&] (int u) { return u < 0 || std::find (units.begin(), units.end(), u) != units.end(); };
        bool changed = false;
        for (bool again = true; again;)
        {
            again = false;
            for (auto& c : s.cords)
            {
                const End* gone = c.a.plugged() && ! inRack (c.a.unit) ? &c.a : c.b.plugged() && ! inRack (c.b.unit) ? &c.b : nullptr;
                if (gone == nullptr)
                    continue;
                const int u = gone->unit;
                for (int ch = 0; ch < 2; ++ch)
                {
                    // into its IN from X, out of its OUT to Y: one cord X -> Y
                    End from {}, to {};
                    for (auto& d : s.cords)
                    {
                        if (d.b.unit == u && d.b.port == inL + ch) from = d.a;
                        if (d.a.unit == u && d.a.port == inL + ch) from = d.b;
                        if (d.a.unit == u && d.a.port == outL + ch) to = d.b;
                        if (d.b.unit == u && d.b.port == outL + ch) to = d.a;
                    }
                    if (from.plugged() && to.plugged())
                        s.cords.push_back ({ from, to });
                }
                s.cords.erase (std::remove_if (s.cords.begin(), s.cords.end(), [u] (const Cord& d) { return d.a.unit == u || d.b.unit == u; }),
                               s.cords.end());
                changed = again = true;
                break;
            }
        }
        for (int u : units)
        {
            bool used = false;
            for (auto& c : s.cords)
                used = used || c.a.unit == u || c.b.unit == u;
            if (used)
                continue;
            for (int ch = 0; ch < 2; ++ch)
            {
                const End rackOut { (int16_t) rack, (int8_t) (inL + ch) };
                for (auto& c : s.cords)
                {
                    End* at = c.b == rackOut ? &c.b : c.a == rackOut ? &c.a : nullptr;
                    if (at == nullptr) continue;
                    *at = { (int16_t) u, (int8_t) (inL + ch) };
                    break;
                }
                s.cords.push_back ({ { (int16_t) u, (int8_t) (outL + ch) }, rackOut });
            }
            changed = true;
        }
        return changed;
    }

    /** Saved with the session: "a.unit,a.port,b.unit,b.port;..." then "|1" when ANYTHING is on. */
    inline std::string toString (const State& s)
    {
        std::string out;
        for (auto& c : s.cords)
            out += std::to_string (c.a.unit) + "," + std::to_string ((int) c.a.port) + "," + std::to_string (c.b.unit) + ","
                 + std::to_string ((int) c.b.port) + ";";
        if (s.anything)
            out += "|1";
        return out;
    }

    inline bool fromString (const std::string& text, State& s)
    {
        State r;
        size_t i = 0;
        while (i < text.size() && text[i] != '|')
        {
            int v[4] {};
            for (int k = 0; k < 4; ++k)
            {
                size_t used = 0;
                try { v[k] = std::stoi (text.substr (i), &used); } catch (...) { return false; }
                i += used;
                if (i < text.size() && (text[i] == ',' || text[i] == ';')) ++i;
            }
            if (v[1] < 0 || v[1] > 3 || v[3] < 0 || v[3] > 3) return false;
            r.cords.push_back ({ { (int16_t) v[0], (int8_t) v[1] }, { (int16_t) v[2], (int8_t) v[3] } });
        }
        r.anything = text.find ("|1") != std::string::npos;
        s = r;
        return true;
    }
}
