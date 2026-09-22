#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <cmath>
#include "../DSP/MethodRegistry.h"

namespace pad
{
    /** The knobs' modifier settings (DSP/MethodRegistry.h: knobModifiers). They live in the plugin state
        under a `KnobModifiers` child, not as parameters, so the host keeps seeing each knob's raw value
        and old sessions (which have no such child) load with every modifier off.

        Written on the message thread (the panel, a state load); read on the audio thread (atomics). */
    class KnobModifiers
    {
    public:
        static constexpr int numKnobs = (int) enh::dsp::methods::knobModifiers.size();
        static inline const juce::Identifier treeType { "KnobModifiers" };

        static int indexOf (juce::StringRef param) noexcept
        {
            for (int i = 0; i < numKnobs; ++i)
                if (juce::String (enh::dsp::methods::knobModifiers[(size_t) i].param.data(),
                                  enh::dsp::methods::knobModifiers[(size_t) i].param.size()) == juce::String (param))
                    return i;
            return -1;
        }

        static juce::Identifier inputKey (int knob)
        {
            const auto& k = enh::dsp::methods::knobModifiers[(size_t) knob];
            return juce::String (k.param.data(), k.param.size()) + "_in_" + juce::String (k.input->shortName.data(), k.input->shortName.size());
        }

        /** After a state load: take the values from the tree (missing = off). */
        void loadFrom (const juce::ValueTree& root)
        {
            const auto tree = root.getChildWithName (treeType);
            for (int i = 0; i < numKnobs; ++i)
                inputValue[(size_t) i].store (tree.isValid() ? clampChoice (i, (float) tree.getProperty (inputKey (i), 0.0f)) : 0.0f);
        }

        /** The panel: set a knob's input modifier (its value, e.g. a smoothing time in ms). */
        void setInput (juce::ValueTree& root, int knob, float value)
        {
            if (! juce::isPositiveAndBelow (knob, numKnobs))
                return;
            value = clampChoice (knob, value);
            auto tree = root.getOrCreateChildWithName (treeType, nullptr);
            tree.setProperty (inputKey (knob), value, nullptr);
            inputValue[(size_t) knob].store (value);
        }

        float getInput (int knob) const noexcept
        {
            return juce::isPositiveAndBelow (knob, numKnobs) ? inputValue[(size_t) knob].load (std::memory_order_relaxed) : 0.0f;
        }

    private:
        static float clampChoice (int knob, float v) noexcept
        {
            const auto& c = enh::dsp::methods::knobModifiers[(size_t) knob].input->choices;
            return std::isfinite (v) ? juce::jlimit (c.front(), c.back(), v) : 0.0f;
        }

        std::array<std::atomic<float>, numKnobs> inputValue {};
    };

    /** SMO on the audio thread: a one-pole glide toward the knob's value, per block. The coefficient is
        worked out from the block's length in seconds, so the glide takes the same time at any sample
        rate and block size. It is open-loop (knob -> setting), so it cannot oscillate; it would need
        care on knobs that an adaptive loop turns (AUTO, MATCH), which do not have modifiers. */
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
}
