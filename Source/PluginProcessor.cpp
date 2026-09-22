#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters/ParameterSpecs.h"
#include "Parameters/PresetLibrary.h"

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
    pad::presets::library();   // load the local preset file now (writing the factory presets there if there is none)

    namespace id = pad::params::id;
    clarityNorm = state.getRawParameterValue (id::clarityNorm);
    clarityAdd  = state.getRawParameterValue (id::clarityAdd);
    clarityMode = state.getRawParameterValue (id::clarityMode);
    adaptSpeed = state.getRawParameterValue (id::adaptSpeed);
    sub        = state.getRawParameterValue (id::sub);
    subBoost   = state.getRawParameterValue (id::subBoost);
    footstep   = state.getRawParameterValue (id::footstep);

    heavenHold    = state.getRawParameterValue (id::heavenHold);
    heavenLift    = state.getRawParameterValue (id::heavenLift);
    heavenMode    = state.getRawParameterValue (id::heavenMode);

    tideMix       = state.getRawParameterValue (id::tideMix);
    tideResponse  = state.getRawParameterValue (id::tideResponse);
    tideActive    = state.getRawParameterValue (id::tideActive);
    // Every processing method's choice (MethodRegistry.h), by its MethodId
    for (int unit : enh::dsp::methods::unitsInRackOrder)
    {
        const auto list = enh::dsp::methods::stagesForUnit (unit);
        for (int i = 0; i < list.count; ++i)
            if (list.stages[i].id >= 0)
                methodParams[(size_t) list.stages[i].id] = state.getRawParameterValue (juce::String (list.stages[i].param.data(), list.stages[i].param.size()));
    }
    // Each modified knob's range, so its modifiers work on the travel (0..1) as the knob is turned
    for (int i = 0; i < pad::KnobModifiers::numKnobs; ++i)
        if (const auto* spec = pad::params::findSpec (enh::dsp::knobFields[(size_t) i].param))
        {
            knobRanges[(size_t) i] = juce::NormalisableRange<float> (spec->minValue, spec->maxValue);
            if (spec->skewCentre > 0.0f)
                knobRanges[(size_t) i].setSkewForCentre (spec->skewCentre);
        }
    lumenTarget   = state.getRawParameterValue (id::lumenTarget);
    lumenResponse = state.getRawParameterValue (id::lumenResponse);
    lumenActive   = state.getRawParameterValue (id::lumenActive);
    spectralRange   = state.getRawParameterValue (id::spectralRange);
    spectralRelease = state.getRawParameterValue (id::spectralRelease);
    spectralCeiling = state.getRawParameterValue (id::spectralCeiling);
    spectralActive  = state.getRawParameterValue (id::spectralActive);
    levelGain       = state.getRawParameterValue (id::levelGain);
    balAmount       = state.getRawParameterValue (id::balAmount);
    balSpeed        = state.getRawParameterValue (id::balSpeed);
    balTilt         = state.getRawParameterValue (id::balTilt);
    balRange        = state.getRawParameterValue (id::balRange);
    balActive       = state.getRawParameterValue (id::balActive);
    balResolution   = state.getRawParameterValue (id::balResolution);

    seraphMode   = state.getRawParameterValue (id::seraphMode);
    enhMultiply    = state.getRawParameterValue (id::enhMultiply);
    enhStrength    = state.getRawParameterValue (id::enhStrength);
    seraphMultiply = state.getRawParameterValue (id::seraphMultiply);
    seraphStrength = state.getRawParameterValue (id::seraphStrength);
    silkSmooth   = state.getRawParameterValue (id::silkSmooth);
    silkAir      = state.getRawParameterValue (id::silkAir);
    silkWarmth   = state.getRawParameterValue (id::silkWarmth);
    silkBody     = state.getRawParameterValue (id::silkBody);
    silkOutput   = state.getRawParameterValue (id::silkOutput);
    silkProtect  = state.getRawParameterValue (id::silkProtect);
    silkTape     = state.getRawParameterValue (id::silkTape);
    silkAuto     = state.getRawParameterValue (id::silkAuto);
    silkSub      = state.getRawParameterValue (id::silkSub);
    heavenAuto   = state.getRawParameterValue (id::heavenAuto);
    heavenAutoAmount = state.getRawParameterValue (id::heavenAutoAmount);
    haloWidth    = state.getRawParameterValue (id::haloWidth);
    haloSpace    = state.getRawParameterValue (id::haloSpace);
    haloDecay    = state.getRawParameterValue (id::haloDecay);
    haloShimmer  = state.getRawParameterValue (id::haloShimmer);
    haloTone     = state.getRawParameterValue (id::haloTone);
    haloDuck     = state.getRawParameterValue (id::haloDuck);
    haloBassMono = state.getRawParameterValue (id::haloBassMono);
    haloMod      = state.getRawParameterValue (id::haloMod);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    knobSmoothers = {};
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

    enh::dsp::KnobValues k;
    k.clarityNorm    = clarityNorm->load();
    k.clarityAdd     = clarityAdd->load();
    k.clarityAddMode = clarityMode->load() > 0.5f;
    k.adaptPercent   = adaptSpeed->load();
    k.subPercent     = sub->load();
    k.subBoost       = subBoost->load() > 0.5f;
    k.footstep       = footstep->load() > 0.5f;
    k.enhMultiply    = enhMultiply->load();
    k.enhStrength    = enhStrength->load();

    k.heavenHold     = heavenHold->load();
    k.heavenLift     = heavenLift->load();
    k.heavenLiftMode = heavenMode->load() > 0.5f;

    k.tideMixPercent = tideMix->load();
    k.tideResponse   = tideResponse->load();
    for (size_t m = 0; m < k.methods.size(); ++m)
        k.methods[m] = methodParams[m] != nullptr ? juce::roundToInt (methodParams[m]->load()) : 0;
    k.tideActive     = tideActive->load() > 0.5f;
    k.lumenTargetDb  = lumenTarget->load();
    k.lumenResponse  = lumenResponse->load();
    k.lumenActive    = lumenActive->load() > 0.5f;
    k.spectralRangeDb   = spectralRange->load();
    k.spectralReleaseMs = spectralRelease->load();
    k.spectralCeilingDb = spectralCeiling->load();
    k.spectralActive    = spectralActive->load() > 0.5f;
    k.levelDb           = levelGain->load();
    k.balAmount         = balAmount->load();
    k.balSpeed          = balSpeed->load();
    k.balTilt           = balTilt->load();
    k.balRangeDb        = balRange->load();
    k.balActive         = balActive->load() > 0.5f;
    k.balResolution     = balResolution->load();

    k.seraphMode     = juce::roundToInt (seraphMode->load());
    k.smooth         = silkSmooth->load();
    k.air            = silkAir->load();
    k.warmth         = silkWarmth->load();
    k.body           = silkBody->load();
    k.outputDb       = silkOutput->load();
    k.protect        = silkProtect->load() > 0.5f;
    k.tape           = silkTape->load() > 0.5f;
    k.autoGain       = silkAuto->load() > 0.5f;
    k.silkSub        = silkSub->load();
    k.heavenAuto     = heavenAuto->load() > 0.5f;
    k.heavenAutoAmount = heavenAutoAmount->load();
    k.widthPercent   = haloWidth->load();
    k.space          = haloSpace->load();
    k.decayS         = haloDecay->load();
    k.shimmer        = haloShimmer->load();
    k.tone           = haloTone->load();
    k.duck           = haloDuck->load() > 0.5f;
    k.bassMono       = haloBassMono->load() > 0.5f;
    k.mod            = haloMod->load() > 0.5f;
    k.seraphMultiply = seraphMultiply->load();
    k.seraphStrength = seraphStrength->load();

    applyKnobModifiers (k, buffer.getNumSamples());   // the host still sees the raw values
    const auto p = enh::dsp::mapKnobs (k);
    engine.process (buffer, p);
}

/** Each knob through its modifiers (SMO, CRV, LIM), on its travel. A knob with all three off is left
    exactly as it is. */
void PluginProcessor::applyKnobModifiers (enh::dsp::KnobValues& k, int numSamples) noexcept
{
    using namespace enh::dsp::methods;
    for (int i = 0; i < pad::KnobModifiers::numKnobs; ++i)
    {
        float& v = k.*(enh::dsp::knobFields[(size_t) i].field);
        v = pad::applyKnobModifiers (v, knobRanges[(size_t) i], knobSmoothers[(size_t) i],
                                     knobModifiers.get (i, modifierSmoothing), juce::roundToInt (knobModifiers.get (i, modifierCurve)),
                                     knobModifiers.get (i, modifierRange), numSamples, currentSampleRate);
    }
}

int PluginProcessor::getNumPrograms()
{
    return (int) pad::presets::library()->size();
}

const juce::String PluginProcessor::getProgramName (int index)
{
    const auto list = pad::presets::library();
    return juce::isPositiveAndBelow (index, (int) list->size()) ? (*list)[(size_t) index].name : juce::String();
}

void PluginProcessor::setCurrentProgram (int index)
{
    // The local preset file (re-read when it has changed), so presets can be tuned without a rebuild
    const auto list = pad::presets::library();
    if (! juce::isPositiveAndBelow (index, (int) list->size()))
        return;

    // Every parameter goes to the preset's value or its default, as a host-visible change
    const auto& preset = (*list)[(size_t) index];
    for (auto& spec : pad::params::allSpecs())
    {
        if (! spec.automatable)
            continue;
        if (auto* param = state.getParameter (spec.id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (pad::presets::valueFor (preset, spec)));
            param->endChangeGesture();
        }
    }

    currentPreset = index;
    state.state.setProperty ("preset", index, nullptr);
    ++presetLoads;
}

void PluginProcessor::stepPreset (int delta)
{
    const int n = getNumPrograms();
    setCurrentProgram (((currentPreset.load() + delta) % n + n) % n);
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
        {
            state.replaceState (juce::ValueTree::fromXml (*xml));

            // A session saved before a parameter existed has no value for it, and JUCE would keep whatever
            // this instance had: put the method choices (not automatable, so not in presets either) back
            // to their defaults, which is how the unit sounded then
            for (auto& spec : pad::params::allSpecs())
            {
                if (spec.automatable || spec.kind != pad::params::Kind::choice)
                    continue;
                bool saved = false;
                for (auto* child : xml->getChildIterator())
                    saved = saved || child->getStringAttribute ("id") == spec.id;
                if (! saved)
                    if (auto* param = state.getParameter (spec.id))
                        param->setValueNotifyingHost (param->convertTo0to1 (spec.defaultValue));
            }
            knobModifiers.loadFrom (state.state);
            currentPreset = juce::jlimit (0, getNumPrograms() - 1, (int) state.state.getProperty ("preset", 0));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
