#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParameterBridge.h"
#include "DSP/EnhEngine.h"
#include "DSP/ParameterMapping.h"

/*  ENH Master processor: owns the parameters and the DSP engine (Source/DSP). */
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return JucePlugin_Name; }
    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.1; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getState() noexcept  { return state; }
    pad::ParameterBridge& getBridge() noexcept               { return bridge; }
    const enh::dsp::EngineMeters& getMeters() const noexcept { return engine.getMeters(); }

private:
    juce::AudioProcessorValueTreeState state;
    pad::ParameterBridge bridge;
    enh::dsp::EnhEngine engine;

    std::atomic<float>* enhMultiply = nullptr, *enhStrength = nullptr, *seraphMultiply = nullptr, *seraphStrength = nullptr;
    std::atomic<float>* seraphMode = nullptr,
                      * silkSmooth = nullptr, *silkAir = nullptr, *silkWarmth = nullptr, *silkBody = nullptr, *silkOutput = nullptr,
                      * silkProtect = nullptr, *silkTape = nullptr, *silkAuto = nullptr,
                      * haloWidth = nullptr, *haloSpace = nullptr, *haloDecay = nullptr, *haloShimmer = nullptr, *haloTone = nullptr,
                      * haloDuck = nullptr, *haloBassMono = nullptr, *haloMod = nullptr;
    std::atomic<float>* clarityNorm = nullptr, *clarityAdd = nullptr, *clarityMode = nullptr, *adaptSpeed = nullptr, *sub = nullptr, *subBoost = nullptr, *footstep = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
