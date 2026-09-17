#include "ParameterSpecs.h"

namespace pad::params
{
    const std::array<const char*, numPdTargets>& pdTargetIds()
    {
        static const std::array<const char*, numPdTargets> ids {
            id::adaptDepth, id::response, id::bandLeveling, id::maskDucking,
            id::exciterDrive, id::transientFocus, id::stepFocus, id::outputGain
        };
        return ids;
    }

    juce::String pdKpId (int t) { return "pdKp_" + juce::String (pdTargetIds()[(size_t) t]); }
    juce::String pdKdId (int t) { return "pdKd_" + juce::String (pdTargetIds()[(size_t) t]); }

    static std::vector<Spec> buildSpecs()
    {
        std::vector<Spec> s;

        auto cont = [&s] (const char* pid, const char* name, const char* shortLabel, const char* unit,
                          float lo, float hi, float def, int decimals)
        {
            s.push_back ({ pid, name, shortLabel, unit, Kind::continuous, lo, hi, def, decimals });
        };

        cont (id::adaptDepth,     "Adapt Depth",     "DEPTH",      "%",  0.0f,   100.0f, 50.0f, 1);
        cont (id::response,       "Response",        "RESPONSE",   "%",  0.0f,   100.0f, 40.0f, 1);
        cont (id::bandLeveling,   "Band Leveling",   "LEVELING",   "%",  0.0f,   100.0f, 35.0f, 1);
        cont (id::maskDucking,    "Mask Ducking",    "MASK DUCK",  "dB", 0.0f,   24.0f,  6.0f,  1);
        cont (id::exciterDrive,   "Exciter Drive",   "EXCITE",     "%",  0.0f,   100.0f, 25.0f, 1);
        cont (id::transientFocus, "Transient Focus", "TRANSIENT",  "%",  0.0f,   100.0f, 50.0f, 1);
        cont (id::stepFocus,      "Step Focus",      "STEP FOCUS", "dB", 0.0f,   12.0f,  4.0f,  1);
        cont (id::outputGain,     "Output",          "OUTPUT",     "dB", -18.0f, 6.0f,   0.0f,  1);

        cont (id::reactionComp,   "Reaction Compensation", "REACT COMP", "ms", 0.0f, 40.0f, 8.0f, 1);

        for (int t = 0; t < numPdTargets; ++t)
        {
            auto* target = pdTargetIds()[(size_t) t];
            auto targetName = juce::String();
            for (auto& existing : s)
                if (existing.id == target)
                    targetName = existing.name;

            s.push_back ({ pdKpId (t), "PD Kp - " + targetName, "Kp", "", Kind::continuous, 0.0f, 10.0f, 1.0f, 2 });
            s.push_back ({ pdKdId (t), "PD Kd - " + targetName, "Kd", "", Kind::continuous, 0.0f, 2.0f,  0.2f, 3 });
        }

        s.push_back ({ id::modeMasking,  "Masking Reduction Mode", "MASKING",  "", Kind::toggle, 0.0f, 1.0f, 0.0f, 0 });
        s.push_back ({ id::modeFootstep, "Footstep Priority Mode", "FOOTSTEP", "", Kind::toggle, 0.0f, 1.0f, 0.0f, 0 });

        return s;
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
                const auto unit = spec.unit;

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, spec.name,
                    juce::NormalisableRange<float> (spec.minValue, spec.maxValue, 0.0f),
                    spec.defaultValue,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (unit)
                        .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); })));
            }
        }

        return layout;
    }
}
