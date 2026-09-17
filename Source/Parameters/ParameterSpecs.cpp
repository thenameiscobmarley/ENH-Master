#include "ParameterSpecs.h"

namespace pad::params
{
    static std::vector<Spec> buildSpecs()
    {
        return {
            { id::clarity,    "Clarity",           "CLARITY", "%", Kind::continuous, 0.0f, 100.0f, 50.0f, 1 },
            { id::adaptSpeed, "Adapt Speed",       "ADAPT",   "%", Kind::continuous, 0.0f, 100.0f, 40.0f, 1 },
            { id::sub,        "Sub Enhance",       "SUB",     "%", Kind::continuous, 0.0f, 100.0f, 0.0f,  1 },
            { id::subBoost,   "Sub Boost",         "+BOOST",  "",  Kind::toggle,     0.0f, 1.0f,   0.0f,  0 },
            { id::footstep,   "Footstep Priority", "FOOTSTEP","",  Kind::toggle,     0.0f, 1.0f,   0.0f,  0 },
        };
    }

    const std::vector<Spec>& allSpecs()
    {
        static const std::vector<Spec> specs = buildSpecs();
        return specs;
    }

    const Spec* findSpec (const juce::String& paramId)
    {
        for (auto& spec : allSpecs())
            if (spec.id == paramId)
                return &spec;

        return nullptr;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        for (auto& spec : allSpecs())
        {
            const juce::ParameterID pid { spec.id, 1 };

            if (spec.kind == Kind::toggle)
            {
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    pid, spec.name, spec.defaultValue > 0.5f,
                    juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                        [] (bool v, int) { return juce::String (v ? "On" : "Off"); })));
            }
            else
            {
                const int decimals = spec.decimals;

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, spec.name,
                    juce::NormalisableRange<float> (spec.minValue, spec.maxValue, 0.0f),
                    spec.defaultValue,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (spec.unit)
                        .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); })));
            }
        }

        return layout;
    }
}
