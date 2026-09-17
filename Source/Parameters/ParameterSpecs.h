#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  Single source of truth for every host-visible parameter.

    Reduced control set while DSP work begins (full set: git tag
    backup/phase1-top-panel-full-controls). Nothing in the audio path reads
    these yet; the future DSP backend (Source/DSP) will read the same IDs.
*/
namespace pad::params
{
    namespace id
    {
        inline constexpr const char* clarity      = "clarity";      // depth / detail / clarity amount
        inline constexpr const char* adaptSpeed   = "adaptSpeed";   // how fast the auto-adjustment follows the material
        inline constexpr const char* modeFootstep = "modeFootstep"; // footstep-priority mode
    }

    enum class Kind { continuous, toggle };

    struct Spec
    {
        juce::String id, name, shortLabel, unit;
        Kind kind = Kind::continuous;
        float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
        int decimals = 1;
    };

    const std::vector<Spec>& allSpecs();
    const Spec* findSpec (const juce::String& paramId);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
