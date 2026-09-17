#include "ParameterSpecs.h"

namespace pad::params
{
    static std::vector<Spec> buildSpecs()
    {
        std::vector<Spec> specs {
            { id::clarityNorm, "Clarity (Norm)",   "CLARITY", "",  Kind::continuous, 0.0f, 30.0f,  15.0f, 1 },
            { id::clarityAdd,  "Clarity (Add)",    "CLARITY", "",  Kind::continuous, 0.0f, 10.0f,  3.0f,  1 },
            { id::clarityMode, "Clarity Mode",     "MODE",    "",  Kind::toggle,     0.0f, 1.0f,   0.0f,  0, 0.0f, { "Norm", "Add + Norm" } },
            { id::adaptSpeed, "Adapt Speed",       "ADAPT",   "%", Kind::continuous, 0.0f, 100.0f, 40.0f, 1 },
            { id::sub,        "Sub Enhance",       "SUB",     "%", Kind::continuous, 0.0f, 100.0f, 0.0f,  1 },
            { id::subBoost,   "Sub Boost",         "+BOOST",  "",  Kind::toggle,     0.0f, 1.0f,   0.0f,  0 },
            { id::footstep,   "Footstep Priority", "FOOTSTEP","",  Kind::toggle,     0.0f, 1.0f,   0.0f,  0 },
            { id::enhMultiply, "ENH Multiply",     "MULTIPLY", "x", Kind::continuous, 0.0f, 3.0f,   1.0f,  2 },
            { id::enhStrength, "ENH Strength",     "STRENGTH", "",  Kind::continuous, 0.0f, 5.0f,   1.0f,  2 },

            { id::seraphMode,  "Seraph Mode",     "POWER",   "",  Kind::choice,     0.0f, 2.0f,   2.0f,  0, 0.0f, { "Off", "Silk", "Heaven" } },
            { id::seraphMultiply, "Seraph Multiply", "MULTIPLY", "x", Kind::continuous, 0.0f, 3.0f, 1.0f, 2 },
            { id::seraphStrength, "Seraph Strength", "STRENGTH", "",  Kind::continuous, 0.0f, 5.0f, 1.0f, 2 },

            { id::silkSmooth,  "Silk Smooth",     "SMOOTH",  "",    Kind::continuous, 0.0f, 10.0f, 4.0f, 1 },
            { id::silkAir,     "Silk Air",        "AIR",     "",    Kind::continuous, 0.0f, 10.0f, 4.0f, 1 },
            { id::silkWarmth,  "Silk Warmth",     "WARMTH",  "",    Kind::continuous, 0.0f, 10.0f, 3.0f, 1 },
            { id::silkBody,    "Silk Body",       "BODY",    "",    Kind::continuous, 0.0f, 10.0f, 2.0f, 1 },
            { id::silkOutput,  "Silk Output",     "OUTPUT",  " dB", Kind::continuous, -12.0f, 12.0f, 0.0f, 1 },
            { id::silkProtect, "Silk Protect",    "PROTECT", "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::silkTape,    "Silk Tape",       "TAPE",    "",    Kind::toggle, 0.0f, 1.0f, 0.0f, 0 },
            { id::silkAuto,    "Silk Auto Gain",  "AUTO",    "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },

            { id::haloWidth,   "Halo Width",      "WIDTH",   " %",  Kind::continuous, 0.0f, 200.0f, 120.0f, 0 },
            { id::haloSpace,   "Halo Space",      "SPACE",   "",    Kind::continuous, 0.0f, 10.0f, 2.5f, 1 },
            { id::haloDecay,   "Halo Decay",      "DECAY",   " s",  Kind::continuous, 0.3f, 8.0f, 2.2f, 1, 2.0f },
            { id::haloShimmer, "Halo Shimmer",    "SHIMMER", "",    Kind::continuous, 0.0f, 10.0f, 1.5f, 1 },
            { id::haloTone,    "Halo Tone",       "TONE",    "",    Kind::continuous, 0.0f, 10.0f, 6.0f, 1 },
            { id::haloDuck,    "Halo Duck",       "DUCK",    "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::haloBassMono,"Halo Bass Mono",  "BASS MONO","",   Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::haloMod,     "Halo Mod",        "MOD",     "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
        };

        return specs;
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
                const auto texts = spec.texts.size() == 2 ? spec.texts : juce::StringArray { "Off", "On" };
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    pid, spec.name, spec.defaultValue > 0.5f,
                    juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                        [texts] (bool v, int) { return texts[v ? 1 : 0]; })));
            }
            else if (spec.kind == Kind::choice)
            {
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, spec.name, spec.texts, (int) spec.defaultValue));
            }
            else
            {
                const int decimals = spec.decimals;
                juce::NormalisableRange<float> range (spec.minValue, spec.maxValue, 0.0f);
                if (spec.skewCentre > 0.0f)
                    range.setSkewForCentre (spec.skewCentre);

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, spec.name,
                    range,
                    spec.defaultValue,
                    juce::AudioParameterFloatAttributes()
                        .withLabel (spec.unit)
                        .withStringFromValueFunction ([decimals] (float v, int) { return juce::String (v, decimals); })));
            }
        }

        return layout;
    }
}
