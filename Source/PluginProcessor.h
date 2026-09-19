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

    // The rack presets (the local preset file, see Parameters/PresetLibrary.h) as host programs
    int getNumPrograms() override;
    int getCurrentProgram() override                         { return currentPreset.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    /** The loudness meter's RESET button (any thread). */
    void resetLoudness() noexcept { engine.resetLoudness(); }

    /** Loads the next / previous preset (message thread); the PRESET buttons call this. */
    void stepPreset (int delta);
    /** Bumped on every preset load, so the editor can show the name. */
    juce::uint32 getPresetLoadCount() const noexcept         { return presetLoads.load(); }

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getState() noexcept  { return state; }
    pad::ParameterBridge& getBridge() noexcept               { return bridge; }
    const enh::dsp::EngineMeters& getMeters() const noexcept { return engine.getMeters(); }

    /** Analyser taps. The audio thread only copies samples into these; the editor runs the FFT. */
    const enh::dsp::ScopeFifo& getInputScope() const noexcept { return engine.getInputScope(); }
    const enh::dsp::ScopeFifo& getOutputScope() const noexcept { return engine.getOutputScope(); }
    const enh::dsp::ScopeFifo& getBalancerInputScope() const noexcept  { return engine.getBalancerInputScope(); }
    const enh::dsp::ScopeFifo& getBalancerOutputScope() const noexcept { return engine.getBalancerOutputScope(); }

private:
    juce::AudioProcessorValueTreeState state;
    pad::ParameterBridge bridge;
    enh::dsp::EnhEngine engine;
    std::atomic<int> currentPreset { 0 };
    std::atomic<juce::uint32> presetLoads { 0 };

    std::atomic<float>* enhMultiply = nullptr, *enhStrength = nullptr, *seraphMultiply = nullptr, *seraphStrength = nullptr;
    std::atomic<float>* heavenHold = nullptr, *heavenLift = nullptr, *heavenMode = nullptr;
    std::atomic<float>* tideMix = nullptr, *tideResponse = nullptr, *tideActive = nullptr;
    std::atomic<float>* lumenTarget = nullptr, *lumenResponse = nullptr, *lumenActive = nullptr;
    std::atomic<float>* spectralRange = nullptr, *spectralRelease = nullptr, *spectralCeiling = nullptr, *spectralActive = nullptr;
    std::atomic<float>* levelGain = nullptr, *balAmount = nullptr, *balSpeed = nullptr, *balTilt = nullptr, *balRange = nullptr, *balActive = nullptr, *balResolution = nullptr;
    std::atomic<float>* seraphMode = nullptr,
                      * silkSmooth = nullptr, *silkAir = nullptr, *silkWarmth = nullptr, *silkBody = nullptr, *silkOutput = nullptr,
                      * silkProtect = nullptr, *silkTape = nullptr, *silkAuto = nullptr, *silkSub = nullptr,
                      * heavenAuto = nullptr, *heavenAutoAmount = nullptr,
                      * haloWidth = nullptr, *haloSpace = nullptr, *haloDecay = nullptr, *haloShimmer = nullptr, *haloTone = nullptr,
                      * haloDuck = nullptr, *haloBassMono = nullptr, *haloMod = nullptr;
    std::atomic<float>* clarityNorm = nullptr, *clarityAdd = nullptr, *clarityMode = nullptr, *adaptSpeed = nullptr, *sub = nullptr, *subBoost = nullptr, *footstep = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
