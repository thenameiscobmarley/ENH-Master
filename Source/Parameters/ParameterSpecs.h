#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  Single source of truth for every host-visible parameter.
    The DSP engine (Source/DSP) and the UI both address parameters by these IDs.
    Earlier control sets are preserved in git tags (see README).
*/
namespace pad::params
{
    namespace id
    {
        inline constexpr const char* clarity    = "clarity";     // depth / detail / clarity amount
        inline constexpr const char* adaptSpeed = "adaptSpeed";  // how fast adaptive gains follow the material
        inline constexpr const char* sub        = "sub";         // sub-bass enhance amount
        inline constexpr const char* subBoost   = "subBoost";    // + BOOST
        inline constexpr const char* footstep   = "footstep";    // footstep priority mode
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
