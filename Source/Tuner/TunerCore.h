#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include "TunerBank.inc"
#include "TunerMeasured.inc"
#include "TunerVerified.inc"

/*  RACK TUNER's thinking, shared by the plugin (UI/RackTuner.cpp) and the AudioLab (its tunerbank / tunercheck
    commands). No AI, no network: a table of words, and a bank of settings whose effects were MEASURED.

      Words  TunerWords.inc: ~480 words, phrases, genres and uses, each as twelve tags (WARM BRIGHT DEEP PUNCH
             LOUD SPACE WIDE DIRTY SMOOTH VINTAGE MOTION CLARITY). "very", "a little", "no/not/less" bend the next.
      Knobs  what each unit does switched on, and what each knob does turned up, in the same tags - measured by
             the AudioLab through the real engine (TunerMeasured.inc: brightness, lows, width, tail, loudness,
             punch, distortion, movement...), a first guess by name only where nothing was measured.
      Bank   60,000 settings ("vendors"), each one a unit's answer to an intention - a mix of one to three
             words, a strength - its knobs moved along what they were measured to do, so every setting means
             something. Built once from a fixed seed: the same bank everywhere.
      Tune   every unit's settings scored against the words (along them, less for what they add to the side);
             VARIETY 0 takes the best, more and more freely above it; the best-fitting units play (at most two
             that do nearly the same thing). */
namespace pad::tuner
{
    inline constexpr int numTags = bank::numTags;
    using Tags = std::array<float, numTags>;
    enum { WARM, BRIGHT, DEEP, PUNCH, LOUD, SPACE, WIDE, DIRTY, SMOOTH, VINTAGE, MOTION, CLARITY };
    static_assert (verified::numUnits == bank::numUnits, "TunerVerified.inc is out of date: run EnhAudioLab tunerverify");
    /** How far to trust a unit's settings (verified by playing them), and how big they really are. */
    namespace lab { inline bool flag (const char* name) { static std::map<std::string, bool> f; auto it = f.find (name); if (it == f.end()) it = f.emplace (name, std::getenv (name) != nullptr).first; return it->second; } }   // (AudioLab experiments: TUNER_NO_VERIFY, TUNER_NO_GAIN, TUNER_NO_FOCUSED)
    inline float reliabilityOf (int unit) noexcept { return verified::have && ! lab::flag ("TUNER_NO_VERIFY") ? std::clamp (verified::reliability[unit], 0.05f, 1.0f) : 1.0f; }
    /** Per unit and tag: how much of what its settings are said to do was really heard (least squares over its verified
        settings; 0 where it went the other way - so it is never chosen for that). */
    inline float scaleOf (int unit, int tag) noexcept { return verified::have && ! lab::flag ("TUNER_NO_VERIFY") ? std::clamp (verified::scale[unit][tag], 0.0f, 1.5f) : 1.0f; }
    static_assert (measured::numParams == bank::numParams && measured::numUnits == bank::numUnits,
                   "TunerMeasured.inc is out of date: run EnhAudioLab tunerbank (or it writes an empty one)");

    inline const char* tagName (int t) noexcept { return t >= 0 && t < numTags ? bank::tagNames[t] : ""; }

    /** A knob's default position (0..1). */
    inline float defaultNormOf (int param) noexcept
    {
        if (measured::have) return measured::defaultNorm[param];
        const auto& p = bank::params[param];
        return (p.flags & 4) || p.hi <= p.lo ? 0.5f : std::clamp ((p.d - p.lo) / (p.hi - p.lo), 0.0f, 1.0f);
    }
    /** What a knob at position `pos` (0..1) does, against it at its default: measured at five points along its
        travel (knobs are not straight lines), else the guess by name, halved. */
    inline Tags curveAt (int param, float pos) noexcept
    {
        Tags t {};
        pos = std::clamp (pos, 0.0f, 1.0f);
        if (measured::have && measured::known[param])
        {
            const float x = pos * 4.0f; const int i = std::min (3, (int) x); const float f = x - (float) i;
            const float d = std::clamp (measured::defaultNorm[param], 0.0f, 1.0f) * 4.0f; const int di = std::min (3, (int) d); const float df = d - (float) di;
            for (int g = 0; g < numTags; ++g)
            {
                const auto& c = measured::curve[param];
                const float at = c[i][g] + (c[i + 1][g] - c[i][g]) * f, zero = c[di][g] + (c[di + 1][g] - c[di][g]) * df;
                t[(size_t) g] = at - zero;
            }
        }
        else
            for (int g = 0; g < numTags; ++g) t[(size_t) g] = 0.5f * bank::params[param].tags[g] * (pos - defaultNormOf (param));
        return t;
    }
    /** What a unit brings, switched on at its defaults. */
    inline Tags baseOf (int unit) noexcept
    {
        Tags t {};
        for (int g = 0; g < numTags; ++g) t[(size_t) g] = measured::have ? measured::base[unit][g] : bank::units[unit].base[g];
        return t;
    }
    inline float dot (const Tags& a, const Tags& b) noexcept { float s = 0.0f; for (int g = 0; g < numTags; ++g) s += a[(size_t) g] * b[(size_t) g]; return s; }

    // ------------------------------------------------------------------------------------------------------------
    // Words
    inline juce::String squash (const juce::String& w) { return w.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789"); }

    struct Dictionary
    {
        std::map<juce::String, Tags> words;
        std::vector<Tags> intents;   // (every word's tags: what the bank's settings are made to answer)
        Dictionary()
        {
            struct Row { const char* word; const char* tags; };
            static const Row rows[] {
                #include "TunerWords.inc"
            };
            static const char letters[] = "wbdplsxgmvoc";
            for (const auto& r : rows)
            {
                Tags t {};
                juce::StringArray parts; parts.addTokens (r.tags, " ", {});
                for (const auto& p : parts)
                    if (const char* at = std::strchr (letters, p[0]); at != nullptr && p.length() > 1)
                        t[(size_t) (at - letters)] = p.substring (1).getFloatValue();
                const auto key = squash (r.word);
                if (words.find (key) == words.end()) { words[key] = t; intents.push_back (t); }
            }
        }
    };
    inline const Dictionary& dictionary() { static const Dictionary d; return d; }
    inline int numWords() { return (int) dictionary().words.size(); }

    /** The words -> what is wanted (each tag -1.5 .. 1.5), and the words it knew. Phrases of up to three words
        are looked up first ("dark ambient", "space station", "hip hop"). */
    inline Tags parse (const juce::String& text, juce::StringArray* known = nullptr)
    {
        Tags out {};
        juce::StringArray raw; raw.addTokens (text.replace ("&", "and"), " ,;./!?-_", "\"");
        raw.removeEmptyStrings();
        const auto& d = dictionary().words;
        float bend = 1.0f;
        for (int i = 0; i < raw.size(); ++i)
        {
            const auto w = squash (raw[i]);
            if (w == "very" || w == "super" || w == "extra" || w == "really" || w == "insanely" || w == "insane" || w == "ultra" || w == "so" || w == "extremely" || w == "mega") { bend *= 1.5f; continue; }
            if (w == "little" || w == "slightly" || w == "bit" || w == "somewhat" || w == "touch" || w == "hint" || w == "tad" || w == "kinda" || w == "mildly") { bend *= 0.5f; continue; }
            if (w == "no" || w == "not" || w == "less" || w == "without" || w == "non" || w == "never" || w == "anti" || w == "zero") { bend *= -0.8f; continue; }
            if (w == "more") { bend *= 1.2f; continue; }
            if (w == "a" || w == "an" || w == "the" || w == "and" || w == "with" || w == "for" || w == "of" || w == "like" || w == "sound" || w == "sounding" || w == "make" || w == "it" || w == "me" || w == "my" || w == "some" || w == "lots") continue;
            auto found = d.end(); int used = 1;
            for (int n = std::min (3, raw.size() - i); n >= 1 && found == d.end(); --n)
            {
                juce::String key;
                for (int k = 0; k < n; ++k) key << squash (raw[i + k]);
                if ((found = d.find (key)) != d.end()) used = n;
            }
            if (found == d.end() && w.endsWith ("er")) found = d.find (w.dropLastCharacters (2));   // (warmer, brighter)
            if (found == d.end() && w.endsWith ("s")) found = d.find (w.dropLastCharacters (1));
            if (found == d.end() && w.endsWith ("y")) found = d.find (w.dropLastCharacters (1));    // (punchy -> punch is there too)
            if (found == d.end()) { bend = 1.0f; continue; }
            for (int t = 0; t < numTags; ++t) out[(size_t) t] += bend * found->second[(size_t) t];
            if (known != nullptr)
            {
                juce::String shown; for (int k = 0; k < used; ++k) shown << (k ? " " : "") << raw[i + k].toLowerCase();
                known->addIfNotAlreadyThere ((bend < 0.0f ? "no " : bend > 1.01f ? "very " : bend < 0.99f ? "a little " : "") + shown);
            }
            i += used - 1;
            bend = 1.0f;
        }
        for (auto& v : out) v = std::clamp (v, -1.5f, 1.5f);
        return out;
    }

    // ------------------------------------------------------------------------------------------------------------
    // The bank
    struct Bank
    {
        static constexpr int target = 60000;
        std::vector<int> unitOf, firstValue;     // per vendor
        std::vector<float> values;               // per vendor, its unit's knobs: knobs a normalised offset from the default;
                                                 // switches / choices -1 (left) or the position to set
        std::vector<Tags> tags;                  // per vendor: what it does (its unit on, its knobs where it puts them)
        std::vector<std::array<float, 4>> cube;  // per vendor: x, y, z, hue (the screen)
        std::vector<int> unitFirst, unitCount;   // per unit
        Bank()
        {
            std::mt19937 r (0x7E57u);
            std::normal_distribution<float> gauss (0.0f, 1.0f);
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            const auto& intents = dictionary().intents;
            int weights = 0;
            for (const auto& u : bank::units) weights += 3 + u.numParams;
            int made = 0;
            const float golden = 2.39996323f;
            for (int ui = 0; ui < bank::numUnits; ++ui)
            {
                const auto& u = bank::units[ui];
                const int n = ui == bank::numUnits - 1 ? target - made : (int) std::lround ((double) target * (3 + u.numParams) / weights);
                unitFirst.push_back (made); unitCount.push_back (n);
                const auto base = baseOf (ui);
                // each knob's effect at nine points along its travel (from the curves), made once per unit
                std::vector<std::array<Tags, 9>> grid ((size_t) u.numParams);
                for (int k = 0; k < u.numParams; ++k) for (int q = 0; q < 9; ++q) grid[(size_t) k][(size_t) q] = curveAt (u.firstParam + k, (float) q / 8.0f);
                // its cluster in the cloud: a point on a sphere; its colour warm .. cool by what it does
                const float fy = 1.0f - 2.0f * ((float) ui + 0.5f) / (float) bank::numUnits, rad = std::sqrt (std::max (0.0f, 1.0f - fy * fy)), th = golden * (float) ui;
                const float cx = 0.78f * rad * std::cos (th), cy = 0.78f * fy, cz = 0.78f * rad * std::sin (th);
                const float warmth = base[WARM] + 0.6f * base[VINTAGE] + 0.5f * base[DIRTY] - base[BRIGHT] - 0.6f * base[SPACE] - 0.5f * base[WIDE];
                const float hue = std::fmod (0.62f - 0.28f * std::tanh (warmth) + 1.0f, 1.0f);
                const int side = (int) std::ceil (std::cbrt ((double) n));
                for (int v = 0; v < n; ++v)
                {
                    // the intention it answers. The first 72: each tag alone, either way, at three strengths (so every unit has
                    // its clearest, strongest settings); the rest: one to three words (or a single tag), a strength
                    Tags want {};
                    float strength, focus;
                    if (v < 72 && v < n && ! lab::flag ("TUNER_NO_FOCUSED"))
                    {
                        want[(size_t) (v % 12)] = (v / 12) % 2 == 0 ? 1.0f : -1.0f;
                        strength = v < 24 ? 1.0f : v < 48 ? 0.66f : 0.33f; focus = 1.0f;
                    }
                    else
                    {
                        const int parts = 1 + (int) (uni (r) * 2.99f);
                        for (int q = 0; q < parts; ++q)
                        {
                            const float w = 0.4f + 0.8f * uni (r);
                            if (uni (r) < 0.25f) want[(size_t) (int) (uni (r) * 11.999f)] += (uni (r) < 0.5f ? -1.0f : 1.0f) * w;
                            else { const auto& t = intents[(size_t) (uni (r) * (float) (intents.size() - 1))]; for (int g = 0; g < numTags; ++g) want[(size_t) g] += w * t[(size_t) g]; }
                        }
                        strength = 0.25f + 0.75f * uni (r); focus = 0.75f + 0.25f * uni (r);   // (focus: how far it follows the intention, not chance)
                    }
                    const float wn = std::sqrt (dot (want, want)) + 1.0e-6f;
                    for (auto& x : want) x /= wn;
                    unitOf.push_back (ui); firstValue.push_back ((int) values.size());
                    // Only the one to three knobs that serve the intention best move (each was measured with the others at
                    // their defaults: a setting that moves few of them does what the measurements say)
                    Tags t {};
                    std::vector<std::pair<float, int>> gainK;   // (how much each knob could help, at its best place)
                    std::vector<int> bestQs ((size_t) u.numParams, -1);
                    for (int k = 0; k < u.numParams; ++k)
                    {
                        const auto& p = bank::params[u.firstParam + k];
                        if (p.flags & 1) continue;
                        float bestV = 0.0f;
                        for (int q = 0; q < 9; ++q)
                        {
                            if (p.kind == 1 && q != 0 && q != 8) continue;
                            const float fit = dot (grid[(size_t) k][(size_t) q], want) - 0.10f * dot (grid[(size_t) k][(size_t) q], grid[(size_t) k][(size_t) q]);
                            if (fit > bestV) { bestV = fit; bestQs[(size_t) k] = q; }
                        }
                        if (bestQs[(size_t) k] >= 0) gainK.push_back ({ bestV * (0.85f + 0.3f * uni (r)), k });
                    }
                    std::sort (gainK.begin(), gainK.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
                    const int most = (v < 72 ? 2 : 3) + u.numParams / 8;   // (a big unit - TONE & SPACE, PRO X4 - moves more of its knobs)
                    const int moving = std::min ((int) gainK.size(), 1 + (int) (uni (r) * ((float) most - 0.01f)));
                    std::vector<bool> moves ((size_t) u.numParams, false);
                    for (int m = 0; m < moving; ++m) moves[(size_t) gainK[(size_t) m].second] = true;
                    for (int k = 0; k < u.numParams; ++k)
                    {
                        const auto& p = bank::params[u.firstParam + k];
                        const float def = defaultNormOf (u.firstParam + k);
                        float val = p.kind == 1 ? -1.0f : 0.0f;   // (knobs and choices: an offset, 0 = left; switches: -1 = left)
                        if (moves[(size_t) k])
                        {
                            const int bestQ = bestQs[(size_t) k];
                            if (p.kind != 1)   // (a knob, or a choice - its position snapped to one of its choices)
                            {
                                const float aim = (float) bestQ / 8.0f;
                                float pos = std::clamp (def + strength * focus * (aim - def) + 0.08f * (1.0f - focus) * gauss (r), 0.0f, 1.0f);
                                if (p.kind == 2) { const float steps = std::max (1.0f, p.hi - p.lo); pos = std::round (pos * steps) / steps; }
                                val = pos - def;
                                const auto at = curveAt (u.firstParam + k, pos);
                                for (int g = 0; g < numTags; ++g) t[(size_t) g] += at[(size_t) g];
                            }
                            else if (p.kind == 1)
                            {
                                val = bestQ == 8 ? 1.0f : 0.0f;
                                const auto at = curveAt (u.firstParam + k, val);
                                for (int g = 0; g < numTags; ++g) t[(size_t) g] += at[(size_t) g];
                            }
                        }
                        values.push_back (val);
                    }
                    for (int g = 0; g < numTags; ++g) t[(size_t) g] += base[(size_t) g];
                    tags.push_back (t);   // (as measured, knob by knob: calibrated by what was heard when chosen - calibrated())
                    const int ix = v % side, iy = (v / side) % side, iz = v / (side * side);
                    const float step = 0.11f / (float) std::max (1, side - 1), h = 0.055f;
                    cube.push_back ({ cx - h + step * (float) ix, cy - h + step * (float) iy, cz - h + step * (float) iz, hue });
                }
                made += n;
            }
        }
    };
    inline const Bank& theBank() { static const Bank b; return b; }
    /** Was vendor v played through the rack (each unit's 24 strongest settings)? Then its tags are what was heard. */
    inline bool heardFor (int vendor) noexcept
    {
        const auto& b = theBank();
        const int k = vendor - b.unitFirst[(size_t) b.unitOf[(size_t) vendor]];
        return verified::have && ! lab::flag ("TUNER_NO_VERIFY") && k >= 0 && k < 24 && b.unitCount[(size_t) b.unitOf[(size_t) vendor]] > 24;
    }
    /** Vendor v's tags as they come out: heard (the 24 strongest of each unit), else the bank's scaled per tag by its
        unit's verification. */
    inline Tags calibrated (int vendor) noexcept
    {
        const auto& b = theBank();
        const int u = b.unitOf[(size_t) vendor];
        if (heardFor (vendor)) { Tags h {}; const int k = vendor - b.unitFirst[(size_t) u]; for (int g = 0; g < numTags; ++g) h[(size_t) g] = verified::heard[u][k][g]; return h; }
        Tags t = b.tags[(size_t) vendor];
        for (int g = 0; g < numTags; ++g) t[(size_t) g] *= scaleOf (u, g);
        // and never more, either way, than the unit's strongest settings were heard to do (they were all played)
        if (verified::have && ! lab::flag ("TUNER_NO_VERIFY") && b.unitCount[(size_t) u] > 24)
        {
            static const auto range = []
            {
                std::vector<std::array<std::array<float, 2>, numTags>> r ((size_t) bank::numUnits);
                for (int uu = 0; uu < bank::numUnits; ++uu)
                    for (int g = 0; g < numTags; ++g)
                    {
                        float lo = 0.0f, hi = 0.0f;
                        for (int k = 0; k < 24; ++k) { lo = std::min (lo, verified::heard[uu][k][g]); hi = std::max (hi, verified::heard[uu][k][g]); }
                        r[(size_t) uu][(size_t) g] = { 1.2f * lo, 1.2f * hi };
                    }
                return r;
            }();
            for (int g = 0; g < numTags; ++g) t[(size_t) g] = std::clamp (t[(size_t) g], range[(size_t) u][(size_t) g][0], range[(size_t) u][(size_t) g][1]);
        }
        return t;
    }

    // ------------------------------------------------------------------------------------------------------------
    // The tune: which setting for each unit, and which units play
    struct Pick { int unit = -1, vendor = -1; float score = 0.0f; bool play = false; };

    /** One unit's best setting (VARIETY 0) or a setting drawn more freely, for what is still wanted (`goal`).
        Score: along the goal (as far as it asks, no further), less what it adds off to the side. */
    inline Pick bestFor (int ui, const Tags& goal, float variety, std::mt19937& rng)
    {
        const auto& b = theBank();
        const float norm = std::sqrt (dot (goal, goal));
        Pick pk { ui, -1, -1.0e9f, false };
        if (norm < 1.0e-4f) return pk;
        Tags dir {}; for (int g = 0; g < numTags; ++g) dir[(size_t) g] = goal[(size_t) g] / norm;
        const int first = b.unitFirst[(size_t) ui], n = b.unitCount[(size_t) ui];
        std::vector<float> score ((size_t) n);
        float best = -1.0e9f;
        for (int v = 0; v < n; ++v)
        {
            const auto t = calibrated (first + v);
            const float along = dot (dir, t);
            // what it does that was not asked: on qualities not in the words, the wrong way on ones that are, or
            // past how far they ask (doing one asked thing cleanly is not "off" - it need not do all of them at once)
            float off = 0.0f;
            for (int g = 0; g < numTags; ++g)
            {
                const float w = goal[(size_t) g], x = t[(size_t) g];
                if (std::abs (w) < 0.1f) off += x * x;
                else if (x * w < 0.0f) off += 2.0f * x * x;
                else if (std::abs (x) > std::abs (w)) off += 0.5f * (std::abs (x) - std::abs (w)) * (std::abs (x) - std::abs (w));
            }
            const float rel = heardFor (first + v) ? 1.0f : reliabilityOf (ui);   // (heard for real: trusted; else a unit whose settings did not do what they said counts for less)
            score[(size_t) v] = rel * std::min (along, norm) - 0.25f * std::max (0.0f, along - norm) - (0.8f + 0.8f * (1.0f - rel)) * off;
            best = std::max (best, score[(size_t) v]);
        }
        int chosen = (int) (std::max_element (score.begin(), score.end()) - score.begin());
        if (variety > 0.001f && best > 0.0f)
        {
            // VARIETY: any of the settings nearly as good as the best (within 15 % of it, up to 50 % at VARIETY 10),
            // the better ones more often - a different take each time, never a worse fit than that
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            const float floor = best * (1.0f - (0.15f + 0.35f * variety));
            double total = 0.0; std::vector<double> w ((size_t) n, 0.0);
            for (int v = 0; v < n; ++v) if (score[(size_t) v] >= floor) total += (w[(size_t) v] = score[(size_t) v] - floor + 0.02 * best);
            double at = uni (rng) * total;
            for (int v = 0; v < n; ++v) { if (w[(size_t) v] <= 0.0) continue; at -= w[(size_t) v]; if (at <= 0.0) { chosen = v; break; } }
        }
        pk.vendor = first + chosen; pk.score = score[(size_t) chosen];
        return pk;
    }

    /** available(unit): may it be tuned (in the rack, or may come in). The units are chosen one after another,
        each for what the words still want once the ones before it have done their part - so they complement
        each other (a warm one, a punchy one, a deep one - not four punchy ones). Returns every available unit's
        pick, those that play first; `score` is against the whole of the words. */
    template <typename Available>
    std::vector<Pick> choose (const Tags& want, float variety, std::mt19937& rng, Available&& available)
    {
        std::vector<Pick> picks;
        const float norm0 = std::sqrt (dot (want, want));
        if (norm0 < 0.05f) return picks;
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        std::vector<int> units;
        for (int ui = 0; ui < bank::numUnits; ++ui) if (available (ui)) units.push_back (ui);
        std::vector<bool> taken ((size_t) bank::numUnits, false);
        const int maxOn = 4 + (int) std::lround (3.0f * variety);   // (4 at VARIETY 0: the clearest fits; up to 7)
        Tags left = want;
        std::vector<Tags> bases;
        for (int step = 0; step < maxOn; ++step)
        {
            Pick best; best.score = -1.0e9f;
            for (int ui : units)
            {
                if (taken[(size_t) ui]) continue;
                int alike = 0;   // (at most two of a kind)
                const auto bu = baseOf (ui);
                for (const auto& bq : bases) if (dot (bu, bq) / std::sqrt (dot (bu, bu) * dot (bq, bq) + 1.0e-9f) > 0.85f) ++alike;
                if (alike >= 2) continue;
                auto pk = bestFor (ui, left, variety, rng);
                pk.score *= 1.0f + 0.4f * variety * (uni (rng) - 0.5f);   // (VARIETY: which of the units that fit about as well comes first)
                if (pk.score > best.score) best = pk;
            }
            // worth it: it does a clear part of what is left (and the words were not already met)
            if (best.vendor < 0 || best.score < std::max (0.04f, 0.05f * norm0)) break;
            best.play = true;
            taken[(size_t) best.unit] = true;
            bases.push_back (baseOf (best.unit));
            const auto t = calibrated (best.vendor);
            for (int g = 0; g < numTags; ++g) left[(size_t) g] -= t[(size_t) g];
            best.score = bestFor (best.unit, want, 0.0f, rng).score;   // (its fit to the whole, for the report and the rack's own units)
            picks.push_back (best);
        }
        for (int ui : units)
            if (! taken[(size_t) ui]) picks.push_back (bestFor (ui, want, variety, rng));
        return picks;
    }

    /** Where vendor v puts knob k of its unit (normalised), from the knob's default and now; AMOUNT scales how far
        (1 = as the setting was made). -1: leave it. */
    inline float targetOf (int vendor, int k, float defaultNorm, float amount) noexcept
    {
        const auto& b = theBank();
        const auto& p = bank::params[bank::units[b.unitOf[(size_t) vendor]].firstParam + k];
        const float val = b.values[(size_t) (b.firstValue[(size_t) vendor] + k)];
        if (p.flags & 1) return -1.0f;
        if (p.kind == 0) return std::clamp (defaultNorm + val * std::clamp (amount, 0.0f, 1.5f), 0.0f, 1.0f);
        if (p.kind == 2)   // (a choice: as far along as AMOUNT takes it, on one of its choices)
        {
            if (std::abs (val) < 1.0e-4f) return -1.0f;
            const float steps = std::max (1.0f, p.hi - p.lo);
            return std::round (std::clamp (defaultNorm + val * std::clamp (amount, 0.0f, 1.5f), 0.0f, 1.0f) * steps) / steps;
        }
        if (val < 0.0f) return -1.0f;
        return val;
    }
}
