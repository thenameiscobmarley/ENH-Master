#include "RackTuner.h"
#include "../PluginProcessor.h"
#include "../Parameters/ParameterBridge.h"
#include "Scene/DeviceLayout.h"
#include "Scene/ColourScreens.h"
#include <cmath>
#include <map>

namespace pad
{
    namespace tuner
    {
        const juce::StringArray& chipWords()
        {
            static const juce::StringArray w { "warm", "bright", "dark", "deep", "punchy", "loud", "spacious", "wide", "gritty", "clean", "smooth", "vintage",
                                               "dreamy", "clear", "hip-hop", "edm", "rock", "lo-fi", "ambient", "jazz", "podcast", "gaming", "master", "very", "a little", "no" };
            return w;
        }
        int bankSize() noexcept { return (int) theBank().unitOf.size(); }
        void cubeOf (int i, float& x, float& y, float& z, float& hue) noexcept
        {
            const auto& b = theBank();
            if (i < 0 || i >= (int) b.cube.size()) { x = y = z = hue = 0.0f; return; }
            x = b.cube[(size_t) i][0]; y = b.cube[(size_t) i][1]; z = b.cube[(size_t) i][2]; hue = b.cube[(size_t) i][3];
        }
        int unitOfVendor (int i) noexcept { const auto& b = theBank(); return i >= 0 && i < (int) b.unitOf.size() ? b.unitOf[(size_t) i] : -1; }
        View& view() noexcept { static View v; return v; }
    }

    // ----------------------------------------------------------------------------------------------------
    // The screen: the cloud of 60,000 (ColourScreens.h declares it)
    void colourscreen::tunerCloud (Canvas& cv, float time)
    {
        const float W = (float) cv.w, H = (float) cv.h, px = std::max (1.0f, W / 448.0f);
        cv.fade (0.30f);
        const int n = tuner::bankSize();
        auto& v = tuner::view();
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        float age = (float) (now - v.tunedAt.load (std::memory_order_relaxed));
        std::array<int, tuner::View::maxChosen> chosen {};
        int numChosen = std::min (v.numChosen.load (std::memory_order_relaxed), tuner::View::maxChosen);
        for (int k = 0; k < numChosen; ++k) chosen[(size_t) k] = v.chosen[(size_t) k].load (std::memory_order_relaxed);
        if (numChosen == 0)   // (not tuned yet: it shows what a tune looks like, a new one every six seconds)
        {
            const int round = (int) std::floor (time / 6.0f);
            numChosen = 9;
            for (int k = 0; k < numChosen; ++k) chosen[(size_t) k] = (int) (((unsigned) round * 7919u + (unsigned) k * 104729u + 17u) % (unsigned) std::max (1, n));
            age = std::fmod (time, 6.0f);
        }
        const float yaw = time * 0.13f, pitch = 0.42f + 0.08f * std::sin (time * 0.05f);
        const float cyw = std::cos (yaw), syw = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
        const float scale = H * 2.15f;
        auto project = [&] (float x, float y, float z, float& sx, float& sy, float& depth, float& yr)
        {
            const float x1 = cyw * x + syw * z, z1 = -syw * x + cyw * z;
            const float y1 = cp * y - sp * z1, z2 = sp * y + cp * z1;
            const float persp = 1.0f / (2.6f + z2);
            sx = 0.5f * W + x1 * persp * scale; sy = 0.5f * H - y1 * persp * scale; depth = z2; yr = y1;
        };
        // the sweep: a plane of light passing down through the cloud as a tune looks through it
        const float sweepY = age < 1.2f ? 1.1f - 2.2f * (age / 1.2f) : 99.0f;
        const unsigned twinkle = (unsigned) (time * 4.0f);
        // (positions and colours made once: 60,000 cubes a frame, 30 frames a second, on modest graphics)
        struct Cloud { std::vector<float> x, y, z; std::vector<Rgb> c; };
        static const Cloud cloud = []
        {
            Cloud cl;
            const int m = tuner::bankSize();
            cl.x.resize ((size_t) m); cl.y.resize ((size_t) m); cl.z.resize ((size_t) m); cl.c.resize ((size_t) m);
            for (int i = 0; i < m; ++i) { float hue; tuner::cubeOf (i, cl.x[(size_t) i], cl.y[(size_t) i], cl.z[(size_t) i], hue); cl.c[(size_t) i] = hsv (hue, 0.62f, 1.0f); }
            return cl;
        }();
        const float k0 = 0.5f * W, k1 = 0.5f * H;
        const bool sweeping = sweepY < 50.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = cloud.x[(size_t) i], y = cloud.y[(size_t) i], z = cloud.z[(size_t) i];
            const float x1 = cyw * x + syw * z, z1 = -syw * x + cyw * z;
            const float yr = cp * y - sp * z1, depth = sp * y + cp * z1;
            const float persp = scale / (2.6f + depth);
            float b = 0.07f + 0.07f * (1.0f - 0.5f * (depth + 1.0f));   // (dimmer each: six times as many)
            if (sweeping && std::abs (yr - sweepY) < 0.07f) b += 0.7f * (1.0f - std::abs (yr - sweepY) / 0.07f);
            if (((unsigned) i * 2654435761u + twinkle * 40503u) % 4999u == 0u) b += 0.6f;
            cv.dot ((int) (k0 + x1 * persp + 0.5f), (int) (k1 - yr * persp + 0.5f), cloud.c[(size_t) i], b);
        }
        // the chosen: each lights in turn after the sweep, a small cube round it, a beam down to the rack
        static constexpr int edges[12][2] { { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 }, { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (int k = 0; k < numChosen; ++k)
        {
            const float at = 1.0f + 0.06f * (float) k;
            if (age < at) continue;
            const float lit = std::min (1.0f, (age - at) / 0.25f), pulse = 0.85f + 0.15f * std::sin (time * 3.1f + (float) k * 0.9f);
            float x, y, z, hue, sx, sy, depth, yr;
            tuner::cubeOf (chosen[(size_t) k], x, y, z, hue);
            project (x, y, z, sx, sy, depth, yr);
            const float h = 0.026f * (1.0f + 1.5f * std::max (0.0f, 1.0f - (age - at) / 0.4f));   // (it lands large, then settles)
            std::array<std::array<float, 2>, 8> c2 {};
            for (int q = 0; q < 8; ++q)
            {
                float cx, cy, cd, cyr;
                project (x + ((q & 1) ? h : -h), y + ((q & 2) ? h : -h), z + ((q & 4) ? h : -h), cx, cy, cd, cyr);
                c2[(size_t) q] = { cx, cy };
            }
            const auto col = hsv (hue, 0.30f, 1.0f);
            for (const auto& e : edges)
                cv.line (c2[(size_t) e[0]][0], c2[(size_t) e[0]][1], c2[(size_t) e[1]][0], c2[(size_t) e[1]][1], col, 1.3f * lit * pulse, px * 0.8f);
            cv.splat (sx, sy, col, 1.6f * lit * pulse, 1.4f * px);
            const float f = (age - at) / 1.1f;   // the beam: from the cube to the rack below, then gone
            if (f < 1.6f)
            {
                const float tx = 0.5f * W + (sx - 0.5f * W) * 0.25f, ty = H - 2.0f;
                const float head = std::min (1.0f, f), tail = std::max (0.0f, f - 0.6f);
                cv.line (sx + (tx - sx) * tail, sy + (ty - sy) * tail, sx + (tx - sx) * head, sy + (ty - sy) * head, hsv (hue, 0.5f, 1.0f), 0.9f * (1.0f - std::max (0.0f, f - 1.0f) / 0.6f), px);
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------
    RackTuner::RackTuner (ParameterBridge& b, PluginProcessor& p) : bridge (b), processor (p), rng (std::random_device{}()) {}

    float RackTuner::knob (const char* id) const
    {
        const int i = bridge.indexOf (id);
        return i >= 0 ? bridge.getNormalised (i) : 0.0f;
    }

    void RackTuner::setParam (int index, float v)
    {
        if (index < 0) return;
        bridge.setValueWithSource (index, juce::jlimit (0.0f, 1.0f, v), ControlSource::selfTune);
    }

    RackTuner::Snapshot RackTuner::capture() const
    {
        Snapshot s;
        for (const auto& u : tuner::bank::units)
        {
            if (const int pw = bridge.indexOf (u.power); pw >= 0) s.values.push_back ({ pw, bridge.getNormalised (pw) });
            for (int k = 0; k < u.numParams; ++k)
                if (const int i = bridge.indexOf (tuner::bank::params[u.firstParam + k].id); i >= 0)
                    s.values.push_back ({ i, bridge.getNormalised (i) });
        }
        s.stored = processor.getStoredUnits();
        s.lbStored = processor.getStoredModules();
        return s;
    }

    namespace
    {
        /** The rack's own units in the layout's numbering (the bank knows them by key). */
        int coreUnitOf (const juce::String& k)
        {
            return k == "enhancer" ? layout::enhUnit : k == "tone" ? layout::tubeUnit : k == "compressor" ? layout::tideUnit : k == "leveler" ? layout::lumenUnit
                 : k == "limiter" ? layout::limiterUnit : k == "balancer" ? layout::balancerUnit : k == "deepsub" ? layout::deepUnit : k == "character" ? layout::characterUnit
                 : k == "radar" ? layout::radarUnit : k == "x4" ? layout::x4Unit : k == "velvet" ? layout::velvetUnit : k == "takeback" ? layout::takebackUnit : -1;
        }
    }

    juce::String RackTuner::tune()
    {
        namespace T = tuner; namespace B = tuner::bank;
        juce::StringArray known;
        const auto want = T::parse (words, &known);
        if (std::sqrt (T::dot (want, want)) < 0.05f)
            return report = "Type what you want first - a few words like \"warm punchy hip-hop\", or click the words below.";
        const float variety = knob ("tnVariety"), amount = 10.0f * knob ("tnAmount") / 7.0f;
        const bool mayKnobs = knob ("tnKnobs") > 0.5f, mayPower = knob ("tnOnOff") > 0.5f, maySwap = knob ("tnSwap") > 0.5f;

        const auto before = capture();
        auto stored = processor.getStoredUnits();
        auto lbStored = processor.getStoredModules();
        // where a unit lives: the rack (its unit number) or the LUNCHBOX (its module number)
        auto rackUnitOf = [] (const B::Unit& u) { return u.genIndex >= 0 ? layout::firstGenUnit + u.genIndex : u.lbModule >= 0 ? -1 : coreUnitOf (u.key); };
        // the rack's own eight never leave it (the designed units and the radar can, like the newer ones)
        auto isCore = [&] (const B::Unit& u)
        {
            const int ru = rackUnitOf (u);
            return ru == layout::enhUnit || ru == layout::tubeUnit || ru == layout::tideUnit || ru == layout::lumenUnit || ru == layout::limiterUnit
                || ru == layout::balancerUnit || ru == layout::deepUnit || ru == layout::characterUnit;
        };
        auto isIn = [&] (const B::Unit& u)
        {
            if (u.lbModule >= 0) return ((lbStored >> u.lbModule) & 1u) == 0u && ((stored >> layout::lunchboxUnit) & 1u) == 0u;
            const int ru = rackUnitOf (u);
            return ru >= 0 && ((stored >> ru) & 1u) == 0u;
        };
        // Choose, then see what fits: a unit that would have to come in but has no room is left out and the choice made
        // again without it (so the rest are chosen around what can really play). SWAP UNITS: what doesn't help goes
        // back to the locker first (making room), then what does comes in, best first.
        std::vector<bool> noRoom ((size_t) B::numUnits, false);
        std::vector<T::Pick> picks;
        const auto storedAtStart = stored; const auto lbAtStart = lbStored;
        int swappedIn = 0, swappedOut = 0, leftOut = 0;
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            stored = storedAtStart; lbStored = lbAtStart;
            auto rngCopy = rng;   // (the same draw each attempt: only what fits changes)
            picks = T::choose (want, variety, rngCopy, [&] (int ui) { const auto& u = B::units[ui]; return ! noRoom[(size_t) ui] && (isIn (u) || (maySwap && ! isCore (u))); });
            swappedIn = swappedOut = 0;
            for (auto& pk : picks)
            {
                const auto& u = B::units[pk.unit];
                if (pk.play || isCore (u) || ! isIn (u) || ! maySwap) continue;
                if (u.lbModule >= 0) lbStored |= enh::dsp::rack::lbBit (u.lbModule);
                else stored |= enh::dsp::rack::bit (rackUnitOf (u));
                ++swappedOut;
            }
            bool again = false;
            for (auto& pk : picks)
            {
                const auto& u = B::units[pk.unit];
                if (! pk.play || isIn (u)) continue;
                bool fits = false;
                if (u.lbModule < 0)
                {
                    const auto next = stored & ~enh::dsp::rack::bit (rackUnitOf (u));
                    if ((fits = layout::usedU (next) <= layout::rackCapacityU)) stored = next;
                }
                else
                {
                    const auto next = lbStored & ~enh::dsp::rack::lbBit (u.lbModule);
                    if ((fits = layout::lb::slotsFor (next) <= layout::lbSlots)) lbStored = next;
                }
                if (fits) ++swappedIn; else { noRoom[(size_t) pk.unit] = true; ++leftOut; again = true; }
            }
            if (! again) { rng = rngCopy; break; }
        }

        if (std::none_of (picks.begin(), picks.end(), [] (const T::Pick& p) { return p.play; }))   // (nothing can: change nothing)
            return report = "Nothing in the rack can do that" + juce::String (leftOut > 0 ? " - the units that could are in the locker and there is no room: store one, or turn on SWAP UNITS." : ": try other words.");

        // Each unit: on with its setting if it plays, off if it doesn't (the rack's own: only if it would go against
        // the words, and it can be switched)
        std::vector<Glide> moves;
        int changedKnobs = 0, turnedOn = 0, turnedOff = 0, shown = 0;
        juce::StringArray names;
        auto& view = T::view();
        for (const auto& pk : picks)
        {
            const auto& u = B::units[pk.unit];
            const int pw = bridge.indexOf (u.power);
            const bool powered = pw < 0 || bridge.getNormalised (pw) > 0.5f;
            if (pk.play)
            {
                names.add (juce::String (u.name));
                if (shown < T::View::maxChosen) view.chosen[(size_t) shown++].store (pk.vendor, std::memory_order_relaxed);
                if (mayPower && ! powered && pw >= 0) { moves.push_back ({ pw, 0.0f, 1.0f, true, true, false }); ++turnedOn; }
                if (mayKnobs && (powered || mayPower))
                    for (int k = 0; k < u.numParams; ++k)
                    {
                        const auto& p = B::params[u.firstParam + k];
                        const int idx = bridge.indexOf (p.id);
                        if (idx < 0) continue;
                        const float to = T::targetOf (pk.vendor, k, bridge.getDefaultNormalised (idx), amount), now = bridge.getNormalised (idx);
                        if (to >= 0.0f && std::abs (to - now) > 1.0e-4f) { moves.push_back ({ idx, now, to, p.kind != 0, false, false }); ++changedKnobs; }
                    }
            }
            else if (pw >= 0 && powered && mayPower && (isCore (u) ? pk.score < -0.05f : (isIn (u) || maySwap)))
            {
                moves.push_back ({ pw, 1.0f, 0.0f, true, false, true }); ++turnedOff;
            }
        }
        view.numChosen.store (shown, std::memory_order_relaxed);
        view.tunedAt.store (juce::Time::getMillisecondCounterHiRes() * 0.001, std::memory_order_relaxed);
        view.tunes.fetch_add (1, std::memory_order_relaxed);

        // Level first (the reference is the rack as it is now), then the glide, then the history
        if (knob ("tnMatch") > 0.5f) processor.requestTunerMatch (true);
        Snapshot after = before;
        for (auto& v : after.values)
            for (const auto& m : moves) if (m.index == v.index) v.normalised = m.to;
        after.stored = stored; after.lbStored = lbStored;
        if (stored != before.stored) processor.setStoredUnits (stored);
        if (lbStored != before.lbStored) processor.setStoredModules (lbStored);
        glide = moves; glideT = 0.0f; glideLen = 1.0f; glideLoadsStored = false;
        for (auto& g : glide) bridge.beginGesture (g.index, ControlSource::selfTune);
        history.push_back ({ before, after });
        if (history.size() > 32) history.erase (history.begin());
        showingBefore = false;
        if (const int ab = bridge.indexOf ("tnAB"); ab >= 0 && bridge.getNormalised (ab) > 0.5f) { setParam (ab, 0.0f); lastAB = false; }

        // What it understood (the words' strongest tags), then what it did
        std::vector<int> order (T::numTags); for (int g = 0; g < T::numTags; ++g) order[(size_t) g] = g;
        std::sort (order.begin(), order.end(), [&] (int a, int b) { return std::abs (want[(size_t) a]) > std::abs (want[(size_t) b]); });
        juce::StringArray aims;
        for (int q = 0; q < 3 && std::abs (want[(size_t) order[(size_t) q]]) > 0.15f; ++q)
            aims.add ((want[(size_t) order[(size_t) q]] < 0.0f ? "less " : "more ") + juce::String (T::tagName (order[(size_t) q])).toLowerCase());
        juce::String r;
        r << "\"" << known.joinIntoString (", ") << "\" = " << aims.joinIntoString (", ") << ". " << names.size() << (names.size() == 1 ? " unit" : " units");
        if (names.size() > 0) r << ": " << names.joinIntoString (", ");
        r << ". " << changedKnobs << " knobs moved";
        if (turnedOn + turnedOff > 0) r << ", " << turnedOn << " on, " << turnedOff << " off";
        if (swappedIn + swappedOut > 0) r << ", " << swappedIn << " in, " << swappedOut << " away";
        r << ".";
        if (leftOut > 0) r << " (" << leftOut << (leftOut == 1 ? " unit" : " units") << " from the locker had no room in the rack: store one to let it bring more in.)";
        return report = r;
    }

    void RackTuner::glideTo (const Snapshot& s, float seconds)
    {
        glide.clear();
        for (const auto& v : s.values)
        {
            const float now = bridge.getNormalised (v.index);
            if (std::abs (now - v.normalised) < 1.0e-4f) continue;
            const auto* p = bridge.getParameter (v.index);
            const bool stepped = p != nullptr && p->getNumSteps() <= 12;
            glide.push_back ({ v.index, now, v.normalised, stepped, stepped && v.normalised > now, stepped && v.normalised < now });
            bridge.beginGesture (v.index, ControlSource::selfTune);
        }
        glideTarget = s; glideLoadsStored = true;
        if (s.stored != processor.getStoredUnits() && (s.stored & ~processor.getStoredUnits()) == enh::dsp::rack::Mask {})   // (units coming in: now, so they are heard gliding)
            processor.setStoredUnits (s.stored);
        glideT = 0.0f; glideLen = seconds;
    }

    juce::String RackTuner::undo()
    {
        if (history.empty()) return report = "Nothing to undo yet.";
        const auto before = history.back().first;
        history.pop_back();
        if (knob ("tnMatch") > 0.5f) processor.requestTunerMatch (false);
        glideTo (before, 0.6f);
        showingBefore = false;
        return report = "Back to how the rack was before that tune." + juce::String (history.empty() ? "" : " UNDO again for the one before.");
    }

    void RackTuner::showBefore (bool before)
    {
        if (history.empty() || before == showingBefore) return;
        showingBefore = before;
        if (knob ("tnMatch") > 0.5f) processor.requestTunerMatch (false);
        glideTo (before ? history.back().first : history.back().second, 0.15f);
        report = before ? "A: the rack before the tune." : "B: the rack after the tune.";
    }

    void RackTuner::tick (float dt)
    {
        // The faceplate's buttons (TUNE and UNDO spring back; A/B stays where it is put)
        // (they work with the unit off too: its POWER only switches the level match)
        const int tIdx = bridge.indexOf ("tnTune"), uIdx = bridge.indexOf ("tnUndo"), abIdx = bridge.indexOf ("tnAB");
        const bool t = tIdx >= 0 && bridge.getNormalised (tIdx) > 0.5f, u = uIdx >= 0 && bridge.getNormalised (uIdx) > 0.5f, ab = abIdx >= 0 && bridge.getNormalised (abIdx) > 0.5f;
        if (t) { tune(); setParam (tIdx, 0.0f); }
        if (u) { undo(); setParam (uIdx, 0.0f); }
        if (ab != lastAB) showBefore (ab);
        lastAB = ab;

        // The glide: continuous knobs ease there; switches and choices change half way (a unit coming on
        // switches on at the start, one going off at the end, so nothing cuts out mid-way)
        if (glideT >= 1.0f) return;
        glideT = std::min (1.0f, glideT + dt / std::max (0.05f, glideLen));
        const float e = glideT * glideT * (3.0f - 2.0f * glideT);
        for (const auto& g : glide)
        {
            float v = g.from + (g.to - g.from) * e;
            if (g.stepped) v = g.powerOn ? g.to : g.powerOff ? (glideT >= 1.0f ? g.to : g.from) : (glideT >= 0.5f ? g.to : g.from);
            setParam (g.index, v);
        }
        if (glideT >= 1.0f)
        {
            for (const auto& g : glide) bridge.endGesture (g.index);
            glide.clear();
            if (glideLoadsStored)
            {
                if (glideTarget.stored != processor.getStoredUnits()) processor.setStoredUnits (glideTarget.stored);
                if (glideTarget.lbStored != processor.getStoredModules()) processor.setStoredModules (glideTarget.lbStored);
                glideLoadsStored = false;
            }
        }
    }
}
