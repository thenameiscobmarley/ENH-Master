#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#if ENH_2D
 #include "UI/Flat/FlatRackView.h"
#else
 #include "UI/HardwareView.h"
#endif

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
   #if ENH_2D
    pad::FlatRackView view;   // ENH Master 2D: the faceplates, flat - no OpenGL
   #else
    pad::HardwareView view;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
