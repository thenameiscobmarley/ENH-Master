#include "ParameterBridge.h"
#include "ParameterSpecs.h"

namespace pad
{
    ParameterBridge::ParameterBridge (juce::AudioProcessorValueTreeState& state)
    {
        auto& processor = state.processor;
        const auto& all = processor.getParameters();
        processorIndexToSlot.assign ((size_t) all.size(), -1);

        for (auto& spec : params::allSpecs())
        {
            auto* p = state.getParameter (spec.id);
            jassert (p != nullptr);

            auto slot = std::make_unique<Slot>();
            slot->param = p;

            const int slotIndex = (int) params.size();
            idToSlot[spec.id] = slotIndex;

            if (juce::isPositiveAndBelow (p->getParameterIndex(), (int) processorIndexToSlot.size()))
                processorIndexToSlot[(size_t) p->getParameterIndex()] = slotIndex;

            params.push_back (std::move (slot));
            p->addListener (this);
        }
    }

    ParameterBridge::~ParameterBridge()
    {
        for (auto& slot : params)
            slot->param->removeListener (this);
    }

    int ParameterBridge::indexOf (const juce::String& paramId) const
    {
        auto it = idToSlot.find (paramId);
        return it != idToSlot.end() ? it->second : -1;
    }

    juce::RangedAudioParameter* ParameterBridge::getParameter (int index) const
    {
        return juce::isPositiveAndBelow (index, getNumParameters()) ? params[(size_t) index]->param : nullptr;
    }

    float ParameterBridge::getNormalised (int index) const
    {
        auto* p = getParameter (index);
        return p != nullptr ? p->getValue() : 0.0f;
    }

    float ParameterBridge::getDefaultNormalised (int index) const
    {
        auto* p = getParameter (index);
        return p != nullptr ? p->getDefaultValue() : 0.0f;
    }

    ControlSource ParameterBridge::getLastSource (int index) const
    {
        return juce::isPositiveAndBelow (index, getNumParameters())
                 ? (ControlSource) params[(size_t) index]->lastSource.load()
                 : ControlSource::none;
    }

    juce::uint32 ParameterBridge::getChangeCounter (int index) const
    {
        return juce::isPositiveAndBelow (index, getNumParameters()) ? params[(size_t) index]->counter.load() : 0u;
    }

    void ParameterBridge::beginGesture (int index, ControlSource source)
    {
        if (auto* p = getParameter (index))
        {
            params[(size_t) index]->gestureSource = (int) source;
            p->beginChangeGesture();
        }
    }

    void ParameterBridge::setValueWithSource (int index, float normalised, ControlSource source)
    {
        auto* p = getParameter (index);
        if (p == nullptr)
            return;

        auto& slot = *params[(size_t) index];
        normalised = juce::jlimit (0.0f, 1.0f, normalised);

        slot.pendingSource = (int) source;
        slot.lastOwnValue = normalised;
        p->setValueNotifyingHost (normalised);
        slot.pendingSource = (int) ControlSource::none;
    }

    void ParameterBridge::endGesture (int index)
    {
        if (auto* p = getParameter (index))
        {
            p->endChangeGesture();
            params[(size_t) index]->gestureSource = (int) ControlSource::none;
        }
    }

    void ParameterBridge::parameterValueChanged (int parameterIndex, float newValue)
    {
        if (! juce::isPositiveAndBelow (parameterIndex, (int) processorIndexToSlot.size()))
            return;

        const int slotIndex = processorIndexToSlot[(size_t) parameterIndex];
        if (slotIndex < 0)
            return;

        auto& slot = *params[(size_t) slotIndex];

        auto source = (ControlSource) slot.pendingSource.load();

        if (source == ControlSource::none)
        {
            // A host echo of a value we just wrote shouldn't recolour the control.
            if (std::abs (newValue - slot.lastOwnValue.load()) < 1.0e-5f)
                source = (ControlSource) slot.lastSource.load();
            else if (slot.gestureSource.load() != (int) ControlSource::none)
                source = (ControlSource) slot.gestureSource.load();
            else
                source = ControlSource::hostAutomation;
        }

        slot.lastSource = (int) source;
        slot.counter.fetch_add (1);
    }
}
