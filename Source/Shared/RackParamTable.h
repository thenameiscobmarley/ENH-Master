#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "DisplayBridge.h"
#include "../DSP/ParameterMapping.h"
#include "../DSP/MethodRegistry.h"

/*  The rack's parameters without juce_audio_processors, for the Windows system effect (Source/Apo),
    which runs inside audiodg.exe and must not pull in the plugin / GUI modules.

    It is pad::params::allSpecs() (Parameters/ParameterSpecs.cpp) - same IDs, same ORDER, same ranges,
    defaults and skew - plus where each value goes in enh::dsp::KnobValues, exactly as
    PluginProcessor::processBlock loads them. EnhDspTests --bridge (Tests/DisplayBridgeTests.h) fails
    when the two drift apart: add / move a parameter in ParameterSpecs.cpp and the same line here.

    The DisplayBridge parameter block carries values by index in this order, and a layout hash of the IDs
    and ranges: an app and an effect with different parameter lists don't use each other's values.

    Everything here treats its input as hostile (it comes from shared memory another process wrote):
    non-finite -> the default, clamped to the range, choices / toggles rounded, methods kept in their
    list, knob-modifier settings snapped to their allowed values. */

namespace enh::shared
{
    enum class ParamKind : int { continuous = 0, toggle = 1, choice = 2 };   // = pad::params::Kind

    using KnobValues = enh::dsp::KnobValues;
    using ApplyFn = void (*) (KnobValues&, float) noexcept;

    struct ParamDef
    {
        std::string_view id;
        ParamKind kind = ParamKind::continuous;
        float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f, skewCentre = 0.0f;
        ApplyFn apply = nullptr;   // where the value goes (nullptr: a method, or not used by the DSP)
        int methodId = -1;         // a processing method's choice: KnobValues::methods[methodId]
    };

    struct KnobRange { float start = 0.0f, end = 1.0f, skew = 1.0f; };

    struct RackParamTable
    {
        std::array<ParamDef, kMaxParams> defs {};
        int count = 0;
        std::uint32_t layoutHash = 0;
        std::array<KnobRange, kMaxModifierKnobs> knobRanges {};   // per enh::dsp::knobFields entry
        bool complete = true;                                     // every knob field found (else modifiers are off)
    };

    static_assert ((int) enh::dsp::knobFields.size() <= kMaxModifierKnobs);
    static_assert (enh::dsp::methods::numModifierKinds == kModifierKinds);

    namespace detail
    {
       #define ENH_F(field) [] (KnobValues& k, float v) noexcept { k.field = v; }
       #define ENH_B(field) [] (KnobValues& k, float v) noexcept { k.field = v > 0.5f; }
       #define ENH_I(field) [] (KnobValues& k, float v) noexcept { k.field = (int) std::lround (v); }
        using K = ParamKind;
        constexpr K cont = K::continuous, tog = K::toggle, cho = K::choice;

        // In pad::params::buildSpecs() order: { id, kind, min, max, default, skewCentre, where }
        inline const ParamDef literalParams[] = {
            { "clarityNorm",   cont, 0.0f, 30.0f, 15.0f, 0.0f, ENH_F (clarityNorm) },
            { "clarityAdd",    cont, 0.0f, 10.0f, 3.0f, 0.0f, ENH_F (clarityAdd) },
            { "clarityMode",   tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (clarityAddMode) },
            { "adaptSpeed",    cont, 0.0f, 100.0f, 40.0f, 0.0f, ENH_F (adaptPercent) },
            { "sub",           cont, 0.0f, 100.0f, 0.0f, 0.0f, ENH_F (subPercent) },
            { "subBoost",      tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (subBoost) },
            { "footstep",      tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (footstep) },
            { "radarSens",     cont, 0.0f, 10.0f, 6.0f, 0.0f, ENH_F (radarSens) },
            { "radarBoost",    cont, 0.0f, 34.0f, 6.0f, 0.0f, ENH_F (radarBoost) },
            { "radarSpace",    cont, 0.0f, 10.0f, 4.0f, 0.0f, ENH_F (radarSpace) },
            { "radarListen",   tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (radarListen) },
            { "enhMultiply",   cont, 0.0f, 3.0f, 1.0f, 0.0f, ENH_F (enhMultiply) },
            { "enhStrength",   cont, 0.0f, 5.0f, 1.0f, 0.0f, ENH_F (enhStrength) },

            { "tideMix",       cont, 0.0f, 100.0f, 60.0f, 0.0f, ENH_F (tideMixPercent) },
            { "tideResponse",  cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (tideResponse) },
            { "tideActive",    tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (tideActive) },

            { "lumenTarget",     cont, -36.0f, -6.0f, -18.0f, 0.0f, ENH_F (lumenTargetDb) },
            { "lumenResponse",   cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (lumenResponse) },
            { "spectralRange",   cont, 0.0f, 18.0f, 9.0f, 0.0f, ENH_F (spectralRangeDb) },
            { "spectralRelease", cont, 30.0f, 600.0f, 150.0f, 150.0f, ENH_F (spectralReleaseMs) },
            { "spectralCeiling", cont, -12.0f, 0.0f, 0.0f, 0.0f, ENH_F (spectralCeilingDb) },
            { "spectralActive",  tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (spectralActive) },

            { "lumenActive",   tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (lumenActive) },

            { "seraphMode",     cho,  0.0f, 2.0f, 2.0f, 0.0f, ENH_I (seraphMode) },
            { "seraphMultiply", cont, 0.0f, 3.0f, 1.0f, 0.0f, ENH_F (seraphMultiply) },
            { "seraphStrength", cont, 0.0f, 5.0f, 1.0f, 0.0f, ENH_F (seraphStrength) },

            { "silkSmooth",    cont, 0.0f, 10.0f, 4.0f, 0.0f, ENH_F (smooth) },
            { "silkAir",       cont, 0.0f, 10.0f, 4.0f, 0.0f, ENH_F (air) },
            { "silkWarmth",    cont, 0.0f, 10.0f, 3.0f, 0.0f, ENH_F (warmth) },
            { "silkBody",      cont, 0.0f, 10.0f, 2.0f, 0.0f, ENH_F (body) },
            { "silkOutput",    cont, -12.0f, 12.0f, 0.0f, 0.0f, ENH_F (outputDb) },
            { "silkProtect",   tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (protect) },
            { "silkTape",      tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (tape) },
            { "silkAuto",      tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (autoGain) },
            { "silkSub",       cont, 0.0f, 10.0f, 0.0f, 0.0f, ENH_F (silkSub) },

            { "heavenHold",       cont, 0.0f, 30.0f, 12.0f, 0.0f, ENH_F (heavenHold) },
            { "heavenLift",       cont, 0.0f, 10.0f, 4.0f, 0.0f, ENH_F (heavenLift) },
            { "heavenMode",       tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (heavenLiftMode) },
            { "heavenAuto",       tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (heavenAuto) },
            { "heavenAutoAmount", cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (heavenAutoAmount) },

            { "haloWidth",     cont, 0.0f, 200.0f, 120.0f, 0.0f, ENH_F (widthPercent) },
            { "haloSpace",     cont, 0.0f, 10.0f, 2.5f, 0.0f, ENH_F (space) },
            { "haloDecay",     cont, 0.3f, 8.0f, 2.2f, 2.0f, ENH_F (decayS) },
            { "haloShimmer",   cont, 0.0f, 10.0f, 1.5f, 0.0f, ENH_F (shimmer) },
            { "haloTone",      cont, 0.0f, 10.0f, 6.0f, 0.0f, ENH_F (tone) },
            { "haloDuck",      tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (duck) },
            { "haloBassMono",  tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (bassMono) },
            { "haloMod",       tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (mod) },

            { "levelGain",     cont, -24.0f, 12.0f, 0.0f, 0.0f, ENH_F (levelDb) },
            { "loudnessReset", tog,  0.0f, 1.0f, 0.0f, 0.0f, nullptr },   // momentary: ParamSnapshot::loudnessResets instead

            { "balAmount",     cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (balAmount) },
            { "balSpeed",      cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (balSpeed) },
            { "balTilt",       cont, -5.0f, 5.0f, 0.0f, 0.0f, ENH_F (balTilt) },
            { "balRange",      cont, 0.0f, 12.0f, 6.0f, 0.0f, ENH_F (balRangeDb) },
            { "balActive",     tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (balActive) },
            { "balResolution", cont, 0.0f, 10.0f, 0.0f, 0.0f, ENH_F (balResolution) },
            { "deepDepth",     cont, 0.0f, 10.0f, 0.0f, 0.0f, ENH_F (deepDepth) },
            { "deepHull",      cont, 0.0f, 10.0f, 0.0f, 0.0f, ENH_F (deepHull) },
            { "deepSize",      cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (deepSize) },
            { "deepPressure",  cont, 0.0f, 10.0f, 0.0f, 0.0f, ENH_F (deepPressure) },
            { "deepActive",    tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (deepActive) },
            { "charModelA",    cho,  0.0f, 8.0f, 3.0f, 0.0f, ENH_F (charModelA) },
            { "charModelB",    cho,  0.0f, 8.0f, 4.0f, 0.0f, ENH_F (charModelB) },
            { "charBlend",     cont, 0.0f, 100.0f, 0.0f, 0.0f, ENH_F (charBlend) },
            { "charDrive",     cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (charDrive) },
            { "charColour",    cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (charColour) },
            { "charActive",    tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (charActive) },
            { "charGrit",      tog,  0.0f, 1.0f, 1.0f, 0.0f, ENH_B (charGrit) },
            { "abCompare",     tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (compare) },
            { "monitorSpeed",  cont, 1.0f, 10.0f, 5.0f, 0.0f, nullptr },   // display only
            { "lbEqIn",        tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (lbEqIn) },
            { "lbHpf",         cho,  0.0f, 4.0f, 0.0f, 0.0f, ENH_F (lbHpf) },
            { "lbLowFreq",     cho,  0.0f, 3.0f, 1.0f, 0.0f, ENH_F (lbLowFreq) },
            { "lbLowGain",     cont, -16.0f, 16.0f, 0.0f, 0.0f, ENH_F (lbLowGain) },
            { "lbMidFreq",     cho,  0.0f, 5.0f, 2.0f, 0.0f, ENH_F (lbMidFreq) },
            { "lbMidGain",     cont, -18.0f, 18.0f, 0.0f, 0.0f, ENH_F (lbMidGain) },
            { "lbMidHiQ",      tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (lbMidHiQ) },
            { "lbHighGain",    cont, -16.0f, 16.0f, 0.0f, 0.0f, ENH_F (lbHighGain) },
            { "lbIron",        tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (lbIron) },
            { "lbHarshIn",     tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (lbHarshIn) },
            { "lbHarshAmount", cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (lbHarshAmount) },
            { "lbHarshFreq",   cho,  0.0f, 2.0f, 1.0f, 0.0f, ENH_F (lbHarshFreq) },
            { "lbHarshSpeed",  cont, 10.0f, 200.0f, 30.0f, 0.0f, ENH_F (lbHarshSpeed) },
            { "lbFeedIn",      tog,  0.0f, 1.0f, 0.0f, 0.0f, ENH_B (lbFeedIn) },
            { "lbFeedAmount",  cont, 0.0f, 10.0f, 5.0f, 0.0f, ENH_F (lbFeedAmount) },

            { "presetPrev",    tog,  0.0f, 1.0f, 0.0f, 0.0f, nullptr },   // UI only
            { "presetNext",    tog,  0.0f, 1.0f, 0.0f, 0.0f, nullptr },
        };
       #undef ENH_F
       #undef ENH_B
       #undef ENH_I

        inline RackParamTable buildTable() noexcept
        {
            RackParamTable t;
            for (const auto& d : literalParams)
                if (t.count < kMaxParams)
                    t.defs[(size_t) t.count++] = d;

            // Then every processing method's choice, as ParameterSpecs adds them (rack order, each stage
            // that has a parameter; default = the first method)
            for (int unit : enh::dsp::methods::unitsInRackOrder)
            {
                const auto list = enh::dsp::methods::stagesForUnit (unit);
                for (int i = 0; i < list.count; ++i)
                {
                    const auto& st = list.stages[i];
                    if (st.param.empty())
                        continue;
                    if (t.count >= kMaxParams)
                    {
                        t.complete = false;
                        continue;
                    }
                    ParamDef d;
                    d.id = st.param;
                    d.kind = ParamKind::choice;
                    d.minValue = 0.0f;
                    d.maxValue = (float) (st.numMethods - 1);
                    d.defaultValue = 0.0f;
                    d.methodId = st.id >= 0 && st.id < enh::dsp::methods::numMethodIds ? st.id : -1;
                    t.defs[(size_t) t.count++] = d;
                }
            }

            std::uint32_t h = kFnvSeed;
            for (int i = 0; i < t.count; ++i)
                h = hashParam (h, t.defs[(size_t) i].id.data(), t.defs[(size_t) i].id.size(),
                               t.defs[(size_t) i].minValue, t.defs[(size_t) i].maxValue);
            t.layoutHash = h;

            // Each modified knob's travel, as juce::NormalisableRange (with setSkewForCentre) has it
            for (size_t k = 0; k < enh::dsp::knobFields.size(); ++k)
            {
                const std::string_view name (enh::dsp::knobFields[k].param);
                bool found = false;
                for (int i = 0; i < t.count && ! found; ++i)
                {
                    const auto& d = t.defs[(size_t) i];
                    if (d.id != name)
                        continue;
                    found = true;
                    auto& r = t.knobRanges[k];
                    r.start = d.minValue;
                    r.end = d.maxValue;
                    r.skew = d.skewCentre > 0.0f ? std::log (0.5f) / std::log ((d.skewCentre - d.minValue) / (d.maxValue - d.minValue)) : 1.0f;
                }
                t.complete = t.complete && found;
            }
            return t;
        }
    }

    /** Built once (thread-safe static); call it outside the audio thread first (the effect does, in
        its constructor). */
    inline const RackParamTable& rackParams() noexcept
    {
        static const RackParamTable table = detail::buildTable();
        return table;
    }

    /** A value from shared memory, made safe for its parameter. */
    inline float sanitizeParam (const ParamDef& d, float v) noexcept
    {
        if (! std::isfinite (v))
            v = d.defaultValue;
        v = std::clamp (v, d.minValue, d.maxValue);
        if (d.kind == ParamKind::toggle)
            return v > 0.5f ? 1.0f : 0.0f;
        if (d.kind == ParamKind::choice)
            return (float) std::lround (v);
        return v;
    }

    inline void defaultParamValues (const RackParamTable& t, float* out) noexcept
    {
        for (int i = 0; i < t.count; ++i)
            out[i] = t.defs[(size_t) i].defaultValue;
    }

    /** Values by index (table order) -> KnobValues, as PluginProcessor::processBlock loads them.
        Parameters past `count` keep their defaults. Real-time safe. */
    inline void toKnobValues (const RackParamTable& t, const float* values, int count, KnobValues& out) noexcept
    {
        out = KnobValues {};
        for (int i = 0; i < t.count; ++i)
        {
            const auto& d = t.defs[(size_t) i];
            const float v = sanitizeParam (d, i < count ? values[i] : d.defaultValue);
            if (d.apply != nullptr)
                d.apply (out, v);
            else if (d.methodId >= 0)
                out.methods[(size_t) d.methodId] = (int) std::lround (v);
        }
    }

    //==============================================================================================
    // Knob modifiers (SMO, CRV, LIM): the same maths as pad::applyKnobModifiers (Parameters/
    // KnobModifiers.h) and juce::NormalisableRange, without JUCE. Checked against them in the tests.

    struct ModifierSmoother
    {
        float value = 0.0f;
        bool primed = false;
    };

    inline float snapModifier (int kind, float v) noexcept
    {
        const auto& c = enh::dsp::methods::modifiers[(size_t) kind].choices;
        if (! std::isfinite (v))
            return c[0];
        float best = c[0];
        for (float x : c)
            if (std::abs (x - v) < std::abs (best - v))
                best = x;
        return best;
    }

    inline float applyKnobModifier (const RackParamTable& t, int knob, float value, ModifierSmoother& sm,
                                    float smoothMs, int curve, float rangePercent, int numSamples, double sampleRate) noexcept
    {
        if (smoothMs <= 0.0f && curve == 0 && rangePercent >= 100.0f)
        {
            sm.primed = false;
            return value;
        }
        const auto& r = t.knobRanges[(size_t) knob];
        auto clamp01 = [] (float x) { return std::clamp (x, 0.0f, 1.0f); };

        // convertTo0to1 (jlimit (start, end, value))
        float tt = clamp01 ((std::clamp (value, r.start, r.end) - r.start) / (r.end - r.start));
        if (r.skew != 1.0f)
            tt = std::pow (tt, r.skew);

        // SMO (pad::KnobSmoother)
        if (! sm.primed || smoothMs <= 0.0f || sampleRate <= 0.0)
        {
            sm.value = tt;
            sm.primed = true;
        }
        else
        {
            const double seconds = numSamples / sampleRate;
            const float k = 1.0f - (float) std::exp (-seconds / (smoothMs * 0.001));
            sm.value += (tt - sm.value) * k;
        }
        tt = sm.value;

        // CRV, then LIM
        switch (curve)
        {
            case 1:  tt = tt * tt; break;
            case 2:  tt = std::sqrt (std::max (0.0f, tt)); break;
            case 3:  tt = tt * tt * (3.0f - 2.0f * tt); break;
            default: break;
        }
        tt *= rangePercent / 100.0f;

        // convertFrom0to1 (jlimit (0, 1, t))
        float p = clamp01 (clamp01 (tt));
        if (r.skew != 1.0f && p > 0.0f)
            p = std::exp (std::log (p) / r.skew);
        return r.start + (r.end - r.start) * p;
    }

    /** Every knob through its modifiers. `modifiers` is ParamSnapshot::modifiers (untrusted: snapped). */
    inline void applyKnobModifiers (const RackParamTable& t, KnobValues& k, const float* modifiers,
                                    std::array<ModifierSmoother, kMaxModifierKnobs>& smoothers,
                                    int numSamples, double sampleRate) noexcept
    {
        if (! t.complete)
            return;
        using namespace enh::dsp::methods;
        for (size_t i = 0; i < enh::dsp::knobFields.size(); ++i)
        {
            const float* m = modifiers + i * (size_t) kModifierKinds;
            float& v = k.*(enh::dsp::knobFields[i].field);
            v = applyKnobModifier (t, (int) i, v, smoothers[i],
                                   snapModifier (modifierSmoothing, m[modifierSmoothing]),
                                   (int) snapModifier (modifierCurve, m[modifierCurve]),
                                   snapModifier (modifierRange, m[modifierRange]), numSamples, sampleRate);
        }
    }

    /** All modifiers off (what the effect uses until a UI sends its own). */
    inline void defaultModifiers (float* modifiers) noexcept
    {
        for (int i = 0; i < kMaxModifierKnobs; ++i)
            for (int kind = 0; kind < kModifierKinds; ++kind)
                modifiers[i * kModifierKinds + kind] = enh::dsp::methods::modifiers[(size_t) kind].choices[0];
    }
}
