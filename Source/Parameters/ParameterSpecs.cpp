#include "ParameterSpecs.h"
#include "../DSP/MethodRegistry.h"

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
            { id::enhMultiply, "Enhancer Multiply", "MULTIPLY", "x", Kind::continuous, 0.0f, 3.0f,   1.0f,  2 },
            { id::enhStrength, "Enhancer Strength", "STRENGTH", "",  Kind::continuous, 0.0f, 5.0f,   1.0f,  2 },

            { id::tideMix,      "Compressor Mix", "MIX",      " %", Kind::continuous, 0.0f, 100.0f, 60.0f, 0 },
            { id::tideResponse, "Compressor Response", "RESPONSE", "",   Kind::continuous, 0.0f, 10.0f,  5.0f,  1 },
            { id::tideActive,   "Compressor In", "IN",       "",   Kind::toggle, 0.0f, 1.0f, 1.0f, 0, 0.0f, { "Out", "In" } },

            { id::lumenTarget,   "Leveler Target", "TARGET",   " dB", Kind::continuous, -36.0f, -6.0f, -18.0f, 1 },
            { id::lumenResponse, "Leveler Response", "RESPONSE", "",    Kind::continuous, 0.0f, 10.0f, 5.0f, 1 },
            { id::spectralRange,   "Spectral Limiter Range",   "RANGE",   " dB", Kind::continuous, 0.0f, 18.0f, 9.0f, 1 },
            { id::spectralRelease, "Spectral Limiter Release", "RELEASE", " ms", Kind::continuous, 30.0f, 600.0f, 150.0f, 0, 150.0f },
            { id::spectralCeiling, "Spectral Limiter Ceiling", "CEILING", " dB", Kind::continuous, -12.0f, 0.0f, 0.0f, 1 },
            { id::spectralActive,  "Spectral Limiter In",      "IN",      "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0, 0.0f, { "Out", "In" } },

            { id::lumenActive,   "Leveler In", "IN",       "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0, 0.0f, { "Out", "In" } },

            { id::seraphMode,  "Tone & Space Mode", "POWER", "",  Kind::choice,     0.0f, 2.0f,   2.0f,  0, 0.0f, { "Off", "Tone", "Tone + Space" } },
            { id::seraphMultiply, "Tone & Space Multiply", "MULTIPLY", "x", Kind::continuous, 0.0f, 3.0f, 1.0f, 2 },
            { id::seraphStrength, "Tone & Space Strength", "STRENGTH", "",  Kind::continuous, 0.0f, 5.0f, 1.0f, 2 },

            { id::silkSmooth,  "Tone Smooth",     "SMOOTH",  "",    Kind::continuous, 0.0f, 10.0f, 4.0f, 1 },
            { id::silkAir,     "Tone Air",        "AIR",     "",    Kind::continuous, 0.0f, 10.0f, 4.0f, 1 },
            { id::silkWarmth,  "Tone Warmth",     "WARMTH",  "",    Kind::continuous, 0.0f, 10.0f, 3.0f, 1 },
            { id::silkBody,    "Tone Body",       "BODY",    "",    Kind::continuous, 0.0f, 10.0f, 2.0f, 1 },
            { id::silkOutput,  "Tone Output",     "OUTPUT",  " dB", Kind::continuous, -12.0f, 12.0f, 0.0f, 1 },
            { id::silkProtect, "Tone Protect",    "PROTECT", "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::silkTape,    "Tone Tape",       "TAPE",    "",    Kind::toggle, 0.0f, 1.0f, 0.0f, 0 },
            { id::silkAuto,    "Tone Level Match", "MATCH",  "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::silkSub,     "Tone Sub",        "SUB",     "",    Kind::continuous, 0.0f, 10.0f, 0.0f, 1 },

            { id::heavenHold, "Loudness Hold", "LOUDNESS", "",  Kind::continuous, 0.0f, 30.0f, 12.0f, 1 },
            { id::heavenLift, "Loudness Lift", "LOUDNESS", "",  Kind::continuous, 0.0f, 10.0f, 4.0f,  1 },
            { id::heavenMode, "Loudness Mode", "LIFT",   "",  Kind::toggle, 0.0f, 1.0f, 0.0f, 0, 0.0f, { "Hold", "Lift + Hold" } },
            { id::heavenAuto, "Auto Heaven",   "AUTO",   "",  Kind::toggle, 0.0f, 1.0f, 0.0f, 0 },
            { id::heavenAutoAmount, "Auto Heaven Amount", "HEAVEN", "", Kind::continuous, 0.0f, 10.0f, 5.0f, 1 },

            { id::haloWidth,   "Space Width",      "WIDTH",   " %",  Kind::continuous, 0.0f, 200.0f, 120.0f, 0 },
            { id::haloSpace,   "Space Reverb",    "REVERB",   "",    Kind::continuous, 0.0f, 10.0f, 2.5f, 1 },
            { id::haloDecay,   "Space Decay",      "DECAY",   " s",  Kind::continuous, 0.3f, 8.0f, 2.2f, 1, 2.0f },
            { id::haloShimmer, "Space Shimmer",    "SHIMMER", "",    Kind::continuous, 0.0f, 10.0f, 1.5f, 1 },
            { id::haloTone,    "Space Tone",       "TONE",    "",    Kind::continuous, 0.0f, 10.0f, 6.0f, 1 },
            { id::haloDuck,    "Space Duck",       "DUCK",    "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::haloBassMono,"Space Bass Mono",  "BASS MONO","",   Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },
            { id::haloMod,     "Space Mod",        "MOD",     "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0 },

            { id::levelGain,     "Level",           "LEVEL",   " dB", Kind::continuous, -24.0f, 12.0f, 0.0f, 1 },
            { id::loudnessReset, "Loudness Reset",  "RESET",   "",    Kind::toggle, 0.0f, 1.0f, 0.0f, 0, 0.0f, {}, false },

            { id::balAmount, "Balancer Balance", "BALANCE", "",    Kind::continuous, 0.0f, 10.0f, 5.0f, 1 },
            { id::balSpeed,  "Balancer Speed",   "SPEED",   "",    Kind::continuous, 0.0f, 10.0f, 5.0f, 1 },
            { id::balTilt,   "Balancer Tilt",    "TILT",    "",    Kind::continuous, -5.0f, 5.0f, 0.0f, 1 },
            { id::balRange,  "Balancer Range",   "RANGE",   " dB", Kind::continuous, 0.0f, 12.0f, 6.0f, 1 },
            { id::balActive, "Balancer In",      "IN",      "",    Kind::toggle, 0.0f, 1.0f, 1.0f, 0, 0.0f, { "Out", "In" } },
            { id::balResolution, "Balancer Resolution", "RESOLUTION", "", Kind::continuous, 0.0f, 10.0f, 0.0f, 1 },
            { id::deepDepth,    "Deep Sub Depth",    "DEPTH",    "", Kind::continuous, 0.0f, 10.0f, 0.0f, 1 },
            { id::deepHull,     "Deep Sub Hull",     "HULL",     "", Kind::continuous, 0.0f, 10.0f, 0.0f, 1 },
            { id::deepSize,     "Deep Sub Size",     "SIZE",     "", Kind::continuous, 0.0f, 10.0f, 5.0f, 1 },
            { id::deepPressure, "Deep Sub Pressure", "PRESSURE", "", Kind::continuous, 0.0f, 10.0f, 0.0f, 1 },
            { id::deepActive,   "Deep Sub In",       "IN",       "", Kind::toggle, 0.0f, 1.0f, 1.0f, 0, 0.0f, { "Out", "In" } },
            { id::monitorSpeed, "Monitor Speed", "SPEED", "", Kind::continuous, 1.0f, 10.0f, 5.0f, 1, 0.0f, {}, false },

            { id::presetPrev,  "Preset Previous",  "PREV",    "",    Kind::toggle, 0.0f, 1.0f, 0.0f, 0, 0.0f, {}, false },
            { id::presetNext,  "Preset Next",      "NEXT",    "",    Kind::toggle, 0.0f, 1.0f, 0.0f, 0, 0.0f, {}, false },
        };

        // Every processing method's choice (the glass panel), from the registry: not automatable, default =
        // the first method (how the unit sounded before methods existed)
        for (int unit : enh::dsp::methods::unitsInRackOrder)
        {
            const auto list = enh::dsp::methods::stagesForUnit (unit);
            for (int i = 0; i < list.count; ++i)
            {
                const auto& st = list.stages[i];
                if (st.param.empty())
                    continue;
                juce::StringArray texts;
                for (int k = 0; k < st.numMethods; ++k)
                    texts.add (juce::String (st.methods[k].shortName.data(), st.methods[k].shortName.size()));
                const auto unitName = juce::String (st.unit.data(), st.unit.size());
                const auto stageName = juce::String (st.name.data(), st.name.size());
                specs.push_back ({ juce::String (st.param.data(), st.param.size()),
                                   unitName.substring (0, 1) + unitName.substring (1).toLowerCase() + " " + stageName.toLowerCase(),
                                   stageName, "", Kind::choice, 0.0f, (float) (st.numMethods - 1), 0.0f, 0, 0.0f, texts, false });
            }
        }

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
                    juce::AudioParameterBoolAttributes().withAutomatable (spec.automatable).withStringFromValueFunction (
                        [texts] (bool v, int) { return texts[v ? 1 : 0]; })));
            }
            else if (spec.kind == Kind::choice)
            {
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, spec.name, spec.texts, (int) spec.defaultValue,
                                                                          juce::AudioParameterChoiceAttributes().withAutomatable (spec.automatable)));
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
