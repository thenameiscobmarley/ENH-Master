#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);

    // Host-driven resizing only (no corner component overlapping the GL surface).
    setResizable (true, false);
    setResizeLimits (420, 220, 2560, 1440);
    setSize (1000, 480);

    // Dev-only: PAD_UI_TEST_SIZE="640x340" to check small layouts without a resizable host.
    const auto testSize = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SIZE", {});
    if (testSize.containsChar ('x'))
        setSize (testSize.upToFirstOccurrenceOf ("x", false, false).getIntValue(),
                 testSize.fromFirstOccurrenceOf ("x", false, false).getIntValue());
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff080607));
}

void PluginEditor::resized()
{
    view.setBounds (getLocalBounds());
}
