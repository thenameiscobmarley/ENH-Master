#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  Single source of truth for every host-visible parameter.

    PHASE 1: all parameters are UI-facing placeholders. Nothing in the audio path
    reads them. The future DSP backend (Source/DSP) will read the same IDs.
*/
namespace pad::params
{
    namespace id
    {
        // Adaptive core targets (each gets its own PD controller later)
        inline constexpr const char* adaptDepth     = "adaptDepth";
        inline constexpr const char* response       = "response";
        inline constexpr const char* bandLeveling   = "bandLeveling";
        inline constexpr const char* maskDucking    = "maskDucking";
        inline constexpr const char* exciterDrive   = "exciterDrive";
        inline constexpr const char* transientFocus = "transientFocus";
        inline constexpr const char* stepFocus      = "stepFocus";
        inline constexpr const char* outputGain     = "outputGain";

        // Controller / timing
        inline constexpr const char* reactionComp   = "reactionComp";

        // Modes
        inline constexpr const char* modeMasking    = "modeMasking";
        inline constexpr const char* modeFootstep   = "modeFootstep";
    }

    enum class Kind { continuous, toggle };

    struct Spec
    {
        juce::String id, name, shortLabel, unit;
        Kind kind = Kind::continuous;
        float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
        int decimals = 1;
    };

    inline constexpr int numPdTargets = 8;

    /** IDs of the parameters that will each be driven by a PD controller. */
    const std::array<const char*, numPdTargets>& pdTargetIds();
    juce::String pdKpId (int targetIndex);
    juce::String pdKdId (int targetIndex);

    const std::vector<Spec>& allSpecs();
    const Spec* findSpec (const juce::String& paramId);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
