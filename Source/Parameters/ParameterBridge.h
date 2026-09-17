#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pad
{
    /** Who caused the most recent change of a parameter. The UI uses this only to
        colour the movement — the animation path is identical for every source. */
    enum class ControlSource : int
    {
        none = 0,
        user,           // dragged / clicked in our editor
        hostAutomation, // host automation, generic host sliders, preset recall
        selfTune        // reserved for the future self-tuning engine
    };

    /** Thread-safe view of all parameters plus change-source tracking.

        - Listens to every parameter (callbacks may arrive on the audio thread,
          so the handler only touches atomics).
        - All writes coming from our own code go through setValueWithSource(), so
          a future self-tuner uses exactly the same path as the UI.
    */
    class ParameterBridge : private juce::AudioProcessorParameter::Listener
    {
    public:
        explicit ParameterBridge (juce::AudioProcessorValueTreeState&);
        ~ParameterBridge() override;

        int  getNumParameters() const noexcept                { return (int) params.size(); }
        int  indexOf (const juce::String& paramId) const;
        juce::RangedAudioParameter* getParameter (int index) const;

        float getNormalised (int index) const;
        float getDefaultNormalised (int index) const;

        ControlSource getLastSource (int index) const;
        juce::uint32  getChangeCounter (int index) const;

        // Writes (message thread)
        void beginGesture (int index, ControlSource);
        void setValueWithSource (int index, float normalised, ControlSource);
        void endGesture (int index);

    private:
        struct Slot
        {
            juce::RangedAudioParameter* param = nullptr;
            std::atomic<int>          lastSource { (int) ControlSource::none };
            std::atomic<int>          pendingSource { (int) ControlSource::none };
            std::atomic<int>          gestureSource { (int) ControlSource::none };
            std::atomic<float>        lastOwnValue { -1.0f };
            std::atomic<juce::uint32> counter { 0 };
        };

        void parameterValueChanged (int parameterIndex, float newValue) override;
        void parameterGestureChanged (int, bool) override {}

        std::vector<std::unique_ptr<Slot>> params;
        std::vector<int> processorIndexToSlot;
        std::map<juce::String, int> idToSlot;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterBridge)
    };
}
