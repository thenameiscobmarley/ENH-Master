#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/ParameterSpecs.h"

namespace
{
    const juce::Identifier stateType { "EnhMaster" };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, stateType, pad::params::createLayout()),
      bridge (state)
{
    namespace id = pad::params::id;
    clarity    = state.getRawParameterValue (id::clarity);
    adaptSpeed = state.getRawParameterValue (id::adaptSpeed);
    sub        = state.getRawParameterValue (id::sub);
    subBoost   = state.getRawParameterValue (id::subBoost);
    footstep   = state.getRawParameterValue (id::footstep);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatencySamples());
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    enh::dsp::EnhEngine::Parameters p;
    p.clarity    = clarity->load() / 100.0f;
    p.adaptSpeed = adaptSpeed->load() / 100.0f;
    p.sub        = sub->load() / 100.0f;
    p.subBoost   = subBoost->load() > 0.5f;
    p.footstep   = footstep->load() > 0.5f;

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
