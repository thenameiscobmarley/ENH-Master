#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <cmath>
#include "../DSP/MethodRegistry.h"
#include "../DSP/ParameterMapping.h"

namespace pad
{
    /** The knobs' modifiers (DSP/MethodRegistry.h: modifiers) for every continuous knob
        (DSP/ParameterMapping.h: knobFields). They live in the plugin state under a `KnobModifiers` child,
        not as parameters, so the host keeps seeing each knob's raw value, and old sessions (which have no
        such child) load with every modifier off.

        Written on the message thread (the panel, a state load); read on the audio thread (atomics). */
    class KnobModifiers
    {
    public:
        static constexpr int numKnobs = (int) enh::dsp::knobFields.size();
        static constexpr int numKinds = enh::dsp::methods::numModifierKinds;
        static inline const juce::Identifier treeType { "KnobModifiers" };

        KnobModifiers() { resetAll(); }

        static int indexOf (juce::StringRef param) noexcept
        {
            for (int i = 0; i < numKnobs; ++i)
                if (juce::String (enh::dsp::knobFields[(size_t) i].param) == juce::String (param))
                    return i;
            return -1;
        }

        static const enh::dsp::methods::Modifier& modifier (int kind) noexcept { return enh::dsp::methods::modifiers[(size_t) kind]; }
        static float offValue (int kind) noexcept                               { return modifier (kind).choices[0]; }

        static juce::Identifier key (int knob, int kind)
        {
            const auto& m = modifier (kind);
            return juce::String (enh::dsp::knobFields[(size_t) knob].param) + "_in_" + juce::String (m.shortName.data(), m.shortName.size());
        }

        /** After a state load: take the values from the tree (missing = off). */
        void loadFrom (const juce::ValueTree& root)
        {
            const auto tree = root.getChildWithName (treeType);
            for (int i = 0; i < numKnobs; ++i)
                for (int k = 0; k < numKinds; ++k)
                    value (i, k).store (tree.isValid() ? clampChoice (k, (float) tree.getProperty (key (i, k), offValue (k))) : offValue (k));
        }

        /** The panel: set one modifier of one knob. */
        void set (juce::ValueTree& root, int knob, int kind, float v)
        {
            if (! juce::isPositiveAndBelow (knob, numKnobs) || ! juce::isPositiveAndBelow (kind, numKinds))
                return;
            v = clampChoice (kind, v);
            auto tree = root.getOrCreateChildWithName (treeType, nullptr);
            if (v == offValue (kind))
                tree.removeProperty (key (knob, kind), nullptr);   // off is the absence of a setting
            else
                tree.setProperty (key (knob, kind), v, nullptr);
            value (knob, kind).store (v);
        }

        float get (int knob, int kind) const noexcept
        {
            return juce::isPositiveAndBelow (knob, numKnobs) && juce::isPositiveAndBelow (kind, numKinds)
                       ? values[(size_t) (knob * numKinds + kind)].load (std::memory_order_relaxed) : 0.0f;
        }

        bool anyOn (int knob) const noexcept
        {
            for (int k = 0; k < numKinds; ++k)
                if (get (knob, k) != offValue (k))
                    return true;
            return false;
        }

    private:
        std::atomic<float>& value (int knob, int kind) noexcept { return values[(size_t) (knob * numKinds + kind)]; }

        void resetAll() noexcept
        {
            for (int i = 0; i < numKnobs; ++i)
                for (int k = 0; k < numKinds; ++k)
                    value (i, k).store (offValue (k));
        }

        static float clampChoice (int kind, float v) noexcept
        {
            // Only the listed values are valid: snap to the nearest
            const auto& c = modifier (kind).choices;
            if (! std::isfinite (v))
                return c[0];
            float best = c[0];
            for (float x : c)
                if (std::abs (x - v) < std::abs (best - v))
                    best = x;
            return best;
        }

        std::array<std::atomic<float>, (size_t) (numKnobs * numKinds)> values;
    };

    /** SMO on the audio thread: a one-pole glide toward the knob's value, per block. The coefficient is
        worked out from the block's length in seconds, so the glide takes the same time at any sample
        rate and block size. It is open-loop (knob -> setting), so it cannot oscillate. The knobs that an
        adaptive loop turns on screen (AUTO, MATCH) only show what the DSP is doing: the DSP does not read
        them back, so a modifier can never sit inside a feedback loop. */
    struct KnobSmoother
    {
        float value = 0.0f;
        bool primed = false;

        float process (float target, float timeMs, int numSamples, double sampleRate) noexcept
        {
            if (! primed || timeMs <= 0.0f || sampleRate <= 0.0)
            {
                value = target;
                primed = true;
                return value;
            }
            const double seconds = numSamples / sampleRate;
            const float k = 1.0f - (float) std::exp (-seconds / (timeMs * 0.001));
            value += (target - value) * k;
            return value;
        }
    };

    /** CRV: the knob's travel (0..1) reshaped, same ends. */
    inline float knobCurve (int curve, float t) noexcept;

    /** One knob through its modifiers, on its travel (the range's 0..1): SMO, then CRV, then LIM. Returns
        the value unchanged, to the bit, when all three are off. */
    inline float applyKnobModifiers (float value, const juce::NormalisableRange<float>& range, KnobSmoother& smoother,
                                     float smoothMs, int curve, float rangePercent, int numSamples, double sampleRate) noexcept
    {
        if (smoothMs <= 0.0f && curve == 0 && rangePercent >= 100.0f)
        {
            smoother.primed = false;   // starts from where the knob is, next time
            return value;
        }
        float t = range.convertTo0to1 (juce::jlimit (range.start, range.end, value));
        t = smoother.process (t, smoothMs, numSamples, sampleRate);
        t = knobCurve (curve, t) * (rangePercent / 100.0f);
        return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, t));
    }

    inline float knobCurve (int curve, float t) noexcept
    {
        switch (curve)
        {
            case 1:  return t * t;                              // LOW: more travel at the bottom
            case 2:  return std::sqrt (std::max (0.0f, t));     // HIGH: more travel at the top
            case 3:  return t * t * (3.0f - 2.0f * t);          // S: more at both ends
            default: return t;
        }
    }
}
