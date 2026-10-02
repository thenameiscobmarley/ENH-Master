#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParameterBridge.h"
#include "DSP/EnhEngine.h"
#include "DSP/ParameterMapping.h"
#include "Parameters/KnobModifiers.h"
#include "Custom/DesignCode.h"
#include "DSP/PatchBay.h"

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

    /** How much a unit delays the sound (ms, at the current rate), and the whole rack's: shown in the
        glass panels. Fixed after prepareToPlay (look-ahead and oversampling), so any thread may read it. */
    double getStageLatencyMs (int stage) const noexcept
    {
        const auto parts = engine.getLatencyBreakdown();
        const double rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        return stage >= 0 && stage < (int) parts.size() ? 1000.0 * parts[(size_t) stage].samples / rate : 0.0;
    }
    double getRackLatencyMs() const noexcept
    {
        const double rate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        return 1000.0 * engine.getLatencySamples() / rate;
    }

    /** CUSTOM: load a Rack Unit Designer share code into the slot (message thread). An empty code clears it.
        setKnobs: turn the slot's knobs to the design's positions (a fresh load; not when a session reopens).
        Returns "" or why the code was refused. */
    juce::String setCustomCode (const juce::String& code, bool setKnobs)
    {
        if (code.isEmpty())
        {
            engine.setCustomConfig (nullptr);
            customCode.clear();
        }
        else
        {
            const auto d = pad::custom::decode (code);
            if (! d.ok) return d.error;
            customConfigs.push_back (std::make_unique<enh::dsp::units::CustomConfig> (d.config));   // (kept: the audio thread may still hold an older one)
            engine.setCustomConfig (customConfigs.back().get());
            customCode = code.removeCharacters (" \t\r\n");
            if (setKnobs)
                for (int i = 0; i < 20; ++i)
                    if (d.slots[(size_t) i].used)
                        if (auto* prm = state.getParameter (i < 16 ? "cuK" + juce::String (i + 1) : "cuS" + juce::String (i - 15)))
                            prm->setValueNotifyingHost (i < 16 ? d.slots[(size_t) i].value / 100.0f : (d.slots[(size_t) i].value > 0.5f ? 1.0f : 0.0f));
        }
        state.state.setProperty ("customCode", customCode, nullptr);
        customVersion.fetch_add (1);
        return {};
    }
    juce::String getCustomCode() const { return customCode; }
    /** RACK TUNER: match the rack's level (a tune starts: newReference; A/B or UNDO: match again). */
    void requestTunerMatch (bool newReference) noexcept { engine.requestTunerMatch (newReference); }
    int getCustomVersion() const noexcept { return customVersion.load(); }

    /** THE GEAR LOCKER: the units out of the rack (a bit per unit, the layout's numbering). Saved with the
        session; the designed units start in it. setStoredUnits: message thread. */
    enh::dsp::rack::Mask getStoredUnits() const noexcept { return storedUnits.load (std::memory_order_relaxed); }
    void setStoredUnits (enh::dsp::rack::Mask mask)
    {
        storedUnits.store (mask, std::memory_order_relaxed);
        state.state.setProperty ("lockerStored", (juce::int64) mask.lo, nullptr);
        state.state.setProperty ("lockerStoredHi", (juce::int64) mask.hi, nullptr);   // (units 64 and on, since 3.8.0.1)
        state.state.setProperty ("lockerUnits", enh::dsp::rack::numUnits, nullptr);   // (units added later start stored)
    }
    /** The LUNCHBOX's own locker: its modules out of the frame (rack::lbBit). Saved with the session. */
    enh::dsp::rack::LbMask getStoredModules() const noexcept { return storedModules.load (std::memory_order_relaxed); }
    void setStoredModules (enh::dsp::rack::LbMask mask)
    {
        mask &= ~enh::dsp::rack::lbBit (enh::dsp::rack::lbOutput);   // (the meter never leaves)
        storedModules.store (mask, std::memory_order_relaxed);
        state.state.setProperty ("lunchboxStored", (juce::int64) mask, nullptr);
        state.state.setProperty ("lunchboxModules", enh::dsp::rack::lbModules, nullptr);
    }

    /** THE PATCH BAY (DSP/PatchBay.h): its cords, saved with the session (message thread). No cords: the rack
        straight through in its own order, as it is until it is first re-patched. A cord out of the chain mutes
        the rack (faded); a unit left out of it is passed by; the newer units run in the order they are patched. */
    const enh::patch::State& getPatch() const noexcept { return patch; }
    void setPatch (const enh::patch::State& s)
    {
        patch = s;
        state.state.setProperty ("patchCords", juce::String (enh::patch::toString (patch)), nullptr);
        applyPatch();
        patchVersion.fetch_add (1, std::memory_order_relaxed);
    }
    int getPatchVersion() const noexcept { return patchVersion.load (std::memory_order_relaxed); }
    bool isPatchMuted() const noexcept { return patchMuted || engine.getMeters().patchRunaway.load (std::memory_order_relaxed); }
    bool isPatchLoop() const noexcept { return patchLoop; }
    bool isPatchRunaway() const noexcept { return engine.getMeters().patchRunaway.load (std::memory_order_relaxed); }
    /** A plug half in (the UI's insertion): a quiet crackle and hum, 0..1 (message thread). */
    void setPatchNoise (float noise) noexcept { patchNoise = noise; engine.setPatch (patchMuted, patchNoise); }
    /** A plug going home in its jack, or pulled: its click, quietly, on the output (any thread). */
    void patchClick() noexcept { engine.patchClick(); }

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

    /** Knob modifiers (message thread to set; see Parameters/KnobModifiers.h). knob: index in
        enh::dsp::knobFields, kind: methods::ModifierKind. */
    void setKnobModifier (int knob, int kind, float value)      { knobModifiers.set (state.state, knob, kind, value); }
    float getKnobModifier (int knob, int kind) const noexcept   { return knobModifiers.get (knob, kind); }
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
    pad::KnobModifiers knobModifiers;
    std::array<pad::KnobSmoother, pad::KnobModifiers::numKnobs> knobSmoothers;   // SMO per knob (audio thread)
    std::array<juce::NormalisableRange<float>, pad::KnobModifiers::numKnobs> knobRanges;
    std::array<std::atomic<float>*, enh::dsp::methods::numMethodIds> methodParams {};
    void applyKnobModifiers (enh::dsp::KnobValues&, int numSamples) noexcept;
    double currentSampleRate = 48000.0;
    std::atomic<juce::uint32> presetLoads { 0 };

    std::atomic<float>* enhMultiply = nullptr, *enhStrength = nullptr, *seraphMultiply = nullptr, *seraphStrength = nullptr;
    std::atomic<float>* heavenHold = nullptr, *heavenLift = nullptr, *heavenMode = nullptr;
    std::atomic<float>* tideMix = nullptr, *tideResponse = nullptr, *tideActive = nullptr;
    std::atomic<float>* deepDepth = nullptr, *deepHull = nullptr, *deepSize = nullptr, *deepPressure = nullptr, *deepActive = nullptr;
    std::atomic<float>* lbEqIn = nullptr, *lbHpf = nullptr, *lbLowFreq = nullptr, *lbLowGain = nullptr, *lbMidFreq = nullptr,
                        *lbMidGain = nullptr, *lbMidHiQ = nullptr, *lbHighGain = nullptr, *lbIron = nullptr, *lbHarshIn = nullptr,
                        *lbHarshAmount = nullptr, *lbHarshFreq = nullptr, *lbHarshSpeed = nullptr, *lbFeedIn = nullptr, *lbFeedAmount = nullptr;
    std::array<std::atomic<float>*, enh::dsp::designed::numParams> designedParams {};   // PRO X4, VELVETIZER
    enh::dsp::AtomicUnitMask storedUnits { enh::dsp::rack::defaultStored };
    enh::patch::State patch;                              // THE PATCH BAY's cords (message thread)
    enh::dsp::AtomicUnitMask patchBypass { 0u };          //   the units its cords leave out (run as if put away)
    bool patchMuted = false, patchLoop = false;
    float patchNoise = 0.0f;
    std::atomic<int> patchVersion { 0 };
    void applyPatch();
    std::atomic<enh::dsp::rack::LbMask> storedModules { enh::dsp::rack::defaultLbStored };
    juce::String customCode;                                                              // CUSTOM: the design loaded (its share code)
    std::vector<std::unique_ptr<enh::dsp::units::CustomConfig>> customConfigs;             //   every chain handed to the engine (kept alive)
    std::atomic<int> customVersion { 0 };                // THE GEAR LOCKER
    std::atomic<float>* charModelA = nullptr, *charModelB = nullptr, *charBlend = nullptr, *charDrive = nullptr, *charColour = nullptr, *charActive = nullptr;
    std::atomic<float>* abCompare = nullptr, *charGrit = nullptr;
    std::atomic<float>* lumenTarget = nullptr, *lumenResponse = nullptr, *lumenActive = nullptr;
    std::atomic<float>* spectralRange = nullptr, *spectralRelease = nullptr, *spectralCeiling = nullptr, *spectralActive = nullptr;
    std::atomic<float>* levelGain = nullptr, *balAmount = nullptr, *balSpeed = nullptr, *balTilt = nullptr, *balRange = nullptr, *balActive = nullptr, *balResolution = nullptr;
    std::atomic<float>* seraphMode = nullptr,
                      * silkSmooth = nullptr, *silkAir = nullptr, *silkWarmth = nullptr, *silkBody = nullptr, *silkOutput = nullptr,
                      * silkProtect = nullptr, *silkTape = nullptr, *silkAuto = nullptr, *silkSub = nullptr,
                      * heavenAuto = nullptr, *heavenAutoAmount = nullptr,
                      * haloWidth = nullptr, *haloSpace = nullptr, *haloDecay = nullptr, *haloShimmer = nullptr, *haloTone = nullptr,
                      * haloDuck = nullptr, *haloBassMono = nullptr, *haloMod = nullptr;
    std::atomic<float>* clarityNorm = nullptr, *clarityAdd = nullptr, *clarityMode = nullptr, *adaptSpeed = nullptr, *sub = nullptr, *subBoost = nullptr, *footstep = nullptr,
                        *radarSens = nullptr, *radarBoost = nullptr, *radarSpace = nullptr, *radarReach = nullptr, *radarListen = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
