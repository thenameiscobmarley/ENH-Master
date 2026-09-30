#pragma once

#include <array>
#include <vector>
#include <juce_core/juce_core.h>
#include "../DSP/units/CustomUnit.h"

/*  A Rack Unit Designer share code ("ENH2-xxxx-xxxx-...", or a link with #d=) read here, the way the
    designer reads it: base 62 -> bytes (a leading 1 kept any leading zeros) -> raw deflate -> JSON. Then
    rebuilt from scratch the way designer.js's sanitize() does: only known part types and fields, every
    number clamped, text short and printable - a code is untrusted input. Out comes what the CUSTOM slot
    needs: its look (name, colour), its parts laid on a 2U panel (the designed-unit Print format's fields),
    where its knob and switch slots go, and its sound (a CustomConfig). */
namespace pad::custom
{
    struct Part
    {
        char kind = 'L';
        float x = 0, z = 0, w = 0, h = 0, size = 0;
        juce::String text, param, colour;
        int align = 0, steps = 10, nums = 0;
        float sweep = 270.0f;
    };
    struct Slot { bool used = false; float x = 0, z = 0, size = 1.0f; juce::String label; float value = 50.0f; };

    struct Design
    {
        bool ok = false;
        juce::String error, name, model, sub;
        float plate[3] { 0.05f, 0.05f, 0.055f };
        std::vector<Part> parts;
        std::array<Slot, 20> slots {};   // 16 knobs, then 4 switches
        enh::dsp::units::CustomConfig config;
    };

    namespace detail
    {
        inline juce::String text (const juce::var& v, int max)
        {
            juce::String s = v.toString(), out;
            for (auto c : s)   // letters, digits and a little punctuation - never markup
                if (juce::CharacterFunctions::isLetterOrDigit (c) || juce::String (" .,:;!?&%+-/'()#|_").containsChar (c))
                    out += c;
            return out.substring (0, max);
        }
        inline float num (const juce::var& v, float lo, float hi, float d)
        {
            if (! (v.isDouble() || v.isInt() || v.isInt64())) return d;
            const double x = (double) v;
            return std::isfinite (x) ? (float) std::clamp (x, (double) lo, (double) hi) : d;
        }
        inline bool colourOk (const juce::String& s) { return s.length() == 7 && s[0] == '#' && s.substring (1).containsOnly ("0123456789abcdefABCDEF"); }
        inline float lin (int c) { return std::pow ((float) c / 255.0f, 2.2f); }
    }

    inline Design decode (juce::String code)
    {
        using namespace detail;
        Design d;
        code = code.fromLastOccurrenceOf ("#d=", false, false).isNotEmpty() ? code.fromLastOccurrenceOf ("#d=", false, false) : code;
        code = code.removeCharacters (" \t\r\n");
        if (code.length() > 24000) { d.error = "That code is too long to be a design."; return d; }
        if (! code.startsWith ("ENH2")) { d.error = "That is not a design code (they start with ENH2-)."; return d; }
        const auto digits = code.substring (4).removeCharacters ("-");
        if (digits.isEmpty() || ! digits.containsOnly ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")) { d.error = "That code has a character a design code never has."; return d; }
        // base 62 -> bytes (little-endian while building)
        static const juce::String b62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::vector<juce::uint8> le;
        for (auto ch : digits)
        {
            unsigned carry = (unsigned) b62.indexOfChar (ch);
            for (auto& b : le) { const unsigned v = (unsigned) b * 62u + carry; b = (juce::uint8) (v & 255u); carry = v >> 8; }
            while (carry) { le.push_back ((juce::uint8) (carry & 255u)); carry >>= 8; }
        }
        std::vector<juce::uint8> bytes (le.rbegin(), le.rend());
        if (bytes.empty() || bytes[0] != 1) { d.error = "That code is damaged (a character is missing or wrong)."; return d; }
        juce::MemoryInputStream raw (bytes.data() + 1, bytes.size() - 1, false);
        juce::GZIPDecompressorInputStream gz (&raw, false, juce::GZIPDecompressorInputStream::deflateFormat);
        juce::MemoryBlock json;
        char buf[4096];
        for (;;) { const int got = gz.read (buf, sizeof (buf)); if (got <= 0) break; json.append (buf, (size_t) got); if (json.getSize() > 120000) { d.error = "That code unpacks to too much to be a design."; return d; } }
        const auto root = juce::JSON::parse (json.toString());
        if (! root.isObject()) { d.error = "That code is damaged (it doesn't unpack)."; return d; }

        // --- the unit
        const auto u = root["u"];
        d.name = text (u["name"], 40); d.model = text (u["model"], 24); d.sub = text (u["sub"], 40);
        if (d.name.isEmpty()) d.name = "MY UNIT";
        if (d.model.isEmpty()) d.model = "EM-X";
        const auto col = u["colour"].toString();
        if (colourOk (col)) { const auto c = juce::Colour::fromString ("ff" + col.substring (1)); d.plate[0] = lin (c.getRed()); d.plate[1] = lin (c.getGreen()); d.plate[2] = lin (c.getBlue()); }
        else { d.plate[0] = lin (0x16); d.plate[1] = lin (0x17); d.plate[2] = lin (0x1a); }
        const float H = 44.45f * std::round (num (u["height"], 1, 6, 1));
        // fitted to the slot's 2U, the same scale across and down (a taller design is shrunk to fit)
        const float s = std::min (1.0f, 88.9f / H), sx = 5.0f / 482.6f * s, sz = 1.18f / 88.9f * s;
        auto X = [&] (float x) { return (x - 241.3f) * sx; };
        auto Z = [&] (float y) { return (y - 0.5f * H) * sz; };

        // --- the sound: blocks in the designer's order of parameters
        static const std::array<std::pair<const char*, std::array<const char*, 7>>, 9> blockKeys {{
            { "eq", { "low", "lowf", "mid", "midf", "q", "high", "highf" } }, { "filter", { "mode", "freq", "q" } }, { "drive", { "drive", "shape", "tone", "mix" } },
            { "comp", { "threshold", "ratio", "attack", "release", "makeup", "mix" } }, { "exciter", { "freq", "amount" } }, { "delay", { "time", "feedback", "tone", "mix" } },
            { "room", { "size", "damp", "predelay", "mix" } }, { "width", { "width" } }, { "gain", { "gain" } } }};
        static const std::array<std::array<float, 7>, 9> defaults {{ { 0, 120, 0, 1200, 0.9f, 0, 8000 }, { 0, 8000, 0.7f }, { 9, 0, 9000, 60 }, { -18, 3, 10, 150, 4, 100 },
                                                                   { 4000, 30 }, { 350, 35, 5000, 25 }, { 1.8f, 40, 15, 22 }, { 120 }, { 0 } }};
        const auto chain = root["x"]["chain"];
        if (auto* arr = chain.getArray())
            for (const auto& b : *arr)
            {
                if (d.config.count >= enh::dsp::units::CustomConfig::maxBlocks) break;
                const auto type = b["b"].toString();
                int t = -1;
                for (int k = 0; k < 9; ++k) if (type == blockKeys[(size_t) k].first) t = k;
                if (t < 0) continue;
                auto& cb = d.config.blocks[(size_t) d.config.count++];
                cb.type = (enh::dsp::units::CustomConfig::Block) t;
                cb.on = ! b.hasProperty ("on") || (bool) b["on"];
                const auto ranges = enh::dsp::units::rangesOf (cb.type);
                for (int k = 0; k < 7 && blockKeys[(size_t) t].second[(size_t) k] != nullptr; ++k)
                    cb.p[(size_t) k] = num (b["p"][blockKeys[(size_t) t].second[(size_t) k]], ranges[(size_t) k].lo, ranges[(size_t) k].hi, defaults[(size_t) t][(size_t) k]);
            }
        auto wire = [&] (const juce::String& ctl) -> enh::dsp::units::CustomConfig::Wire
        {
            const auto blockIdx = ctl.upToFirstOccurrenceOf (".", false, false), key = ctl.fromFirstOccurrenceOf (".", false, false);
            if (! blockIdx.containsOnly ("01234567") || blockIdx.isEmpty()) return {};
            const int bi = blockIdx.getIntValue();
            if (bi >= d.config.count) return {};
            if (key == "on") return { bi, -1 };
            const auto& keys = blockKeys[(size_t) d.config.blocks[(size_t) bi].type].second;
            for (int k = 0; k < 7 && keys[(size_t) k] != nullptr; ++k) if (key == keys[(size_t) k]) return { bi, k };
            return {};
        };

        // --- the parts (short keys, as the share code packs them), onto the slot's panel
        struct TypeDef { const char* type; float w, h; };
        static const std::array<TypeDef, 17> types {{ { "knob", 22, 22 }, { "toggle", 10, 18 }, { "button", 12, 10 }, { "led", 4, 4 }, { "vu", 64, 36 }, { "ladder", 6, 40 },
            { "display", 90, 30 }, { "label", 40, 8 }, { "box", 90, 34 }, { "line", 60, 1 }, { "jack", 14, 14 }, { "screw", 5, 5 }, { "vent", 40, 16 },
            { "slider", 12, 60 }, { "selector", 22, 22 }, { "lamp", 10, 10 }, { "plate", 60, 14 } }};
        int knobs = 0, switches = 0;
        if (auto* arr = root["p"].getArray())
            for (int i = 0; i < std::min (arr->size(), 150); ++i)
            {
                const auto& p = arr->getReference (i);
                const auto type = p["t"].toString();
                const TypeDef* td = nullptr;
                for (const auto& t : types) if (type == t.type) td = &t;
                if (td == nullptr) continue;
                const float w = num (p["w"], 1, 482.6f, td->w), h = num (p["h"], 0.5f, 177.8f, td->h);
                const float x = X (num (p["x"], 0, 482.6f, 241.3f)), z = Z (num (p["y"], 0, H, 0.5f * H));
                const auto label = text (p["l"], 40);
                Part q; q.x = x; q.z = z; q.w = w * sx; q.h = h * sz; q.text = label;
                if (type == "knob" || type == "selector" || type == "slider")
                {
                    const float r = 0.5f * std::min (w, type == "slider" ? w * 1.6f : h) * sx;
                    if (knobs < 16)
                    {
                        auto& sl = d.slots[(size_t) knobs];
                        sl.used = true; sl.x = x; sl.z = z; sl.size = std::clamp (r / 0.105f, 0.4f, 2.0f); sl.label = label.isNotEmpty() ? label : "KNOB " + juce::String (knobs + 1);
                        if (type == "selector")
                        {
                            const int stops = std::max (2, juce::StringArray::fromTokens (text (p["so"], 130), "|", "").size());
                            sl.value = num (p["v"], 0, 11, 0) / (float) (stops - 1) * 100.0f;
                        }
                        else sl.value = num (p["v"], 0, 100, 50);
                        d.config.wires[(size_t) knobs] = wire (p["cl"].toString());
                        q.kind = 'K'; q.w = r; q.param = "cuK" + juce::String (knobs + 1);
                        q.steps = (int) num (p["st"], 2, 20, 10); q.nums = p["nu"].toString() == "all" ? 1 : p["nu"].toString() == "none" ? 2 : 0;
                        q.sweep = num (p["sw"], 180, 330, 270);
                        if (p.hasProperty ("c") && ! (bool) p["c"]) q.nums = 3;
                        ++knobs;
                    }
                    else q.kind = 'L', q.size = 2.4f * sz;
                }
                else if (type == "toggle" || type == "button")
                {
                    if (switches < 4)
                    {
                        auto& sl = d.slots[(size_t) (16 + switches)];
                        sl.used = true; sl.x = x; sl.z = z; sl.label = label.isNotEmpty() ? label : "SWITCH " + juce::String (switches + 1);
                        sl.value = p.hasProperty ("o") && ! (bool) p["o"] ? 0.0f : 1.0f;
                        d.config.wires[(size_t) (16 + switches)] = wire (p["cl"].toString());
                        q.kind = 'T'; q.param = "cuS" + juce::String (switches + 1);
                        ++switches;
                    }
                    else q.kind = 'L', q.size = 2.4f * sz;
                }
                else if (type == "led" || type == "lamp")
                {
                    q.kind = 'E'; q.w = 0.5f * std::min (w, 10.0f) * sx;
                    const auto c = p["k"].toString(); q.param = colourOk (c) ? c : juce::String ("#46e070");
                }
                else if (type == "ladder")
                {
                    const int segs = (int) num (p["g"], 3, 12, 10);
                    for (int k = 0; k < segs; ++k)
                    {
                        Part e = q; e.kind = 'E'; e.w = 0.4f * std::min (w, 8.0f) * sx; e.z = z + (0.5f * h - (k + 0.5f) * h / (float) segs) * sz;
                        e.param = k >= segs - 1 ? "#ff3b30" : k >= segs - 3 ? "#ffcc33" : "#46e070";
                        d.parts.push_back (e);
                    }
                    continue;
                }
                else if (type == "label") { q.kind = 'L'; q.size = num (p["z"], 2, 20, 5) * sz; q.align = p["e"].toString() == "left" ? -1 : p["e"].toString() == "right" ? 1 : 0; }
                else if (type == "box" || type == "plate") { q.kind = 'B'; q.size = 3.0f * sx; }
                else if (type == "line") q.kind = 'N';
                else if (type == "vent") { q.kind = 'V'; q.steps = (int) num (p["u"], 2, 30, 6); }
                else if (type == "screw") { q.kind = 'S'; q.w = 0.5f * w * sx; }
                else if (type == "jack") { q.kind = 'J'; q.w = 0.5f * w * sx; const auto st = p["s"].toString(); q.align = st.startsWith ("xlr") || st == "combo" ? 1 : 0; }
                else if (type == "display" || type == "vu")
                {
                    q.kind = 'D'; const auto c = p["k"].toString(); q.param = colourOk (c) ? c : juce::String ("#56c8f5");
                }
                d.parts.push_back (q);
            }
        d.ok = true;
        return d;
    }
}
