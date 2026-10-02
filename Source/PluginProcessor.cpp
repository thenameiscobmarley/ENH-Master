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
    radarSens  = state.getRawParameterValue (id::radarSens);
    radarBoost = state.getRawParameterValue (id::radarBoost);
    radarSpace = state.getRawParameterValue (id::radarSpace);
    radarReach = state.getRawParameterValue (id::radarReach);
    radarListen = state.getRawParameterValue (id::radarListen);

    heavenHold    = state.getRawParameterValue (id::heavenHold);
    heavenLift    = state.getRawParameterValue (id::heavenLift);
    heavenMode    = state.getRawParameterValue (id::heavenMode);

    tideMix       = state.getRawParameterValue (id::tideMix);
    tideResponse  = state.getRawParameterValue (id::tideResponse);
    tideActive    = state.getRawParameterValue (id::tideActive);
    deepDepth     = state.getRawParameterValue (id::deepDepth);
    deepHull      = state.getRawParameterValue (id::deepHull);
    deepSize      = state.getRawParameterValue (id::deepSize);
    deepPressure  = state.getRawParameterValue (id::deepPressure);
    deepActive    = state.getRawParameterValue (id::deepActive);
    charModelA    = state.getRawParameterValue (id::charModelA);
    charModelB    = state.getRawParameterValue (id::charModelB);
    charBlend     = state.getRawParameterValue (id::charBlend);
    charDrive     = state.getRawParameterValue (id::charDrive);
    charColour    = state.getRawParameterValue (id::charColour);
    lbEqIn        = state.getRawParameterValue (id::lbEqIn);
    lbHpf         = state.getRawParameterValue (id::lbHpf);
    lbLowFreq     = state.getRawParameterValue (id::lbLowFreq);
    lbLowGain     = state.getRawParameterValue (id::lbLowGain);
    lbMidFreq     = state.getRawParameterValue (id::lbMidFreq);
    lbMidGain     = state.getRawParameterValue (id::lbMidGain);
    lbMidHiQ      = state.getRawParameterValue (id::lbMidHiQ);
    lbHighGain    = state.getRawParameterValue (id::lbHighGain);
    lbIron        = state.getRawParameterValue (id::lbIron);
    lbHarshIn     = state.getRawParameterValue (id::lbHarshIn);
    lbHarshAmount = state.getRawParameterValue (id::lbHarshAmount);
    lbHarshFreq   = state.getRawParameterValue (id::lbHarshFreq);
    lbHarshSpeed  = state.getRawParameterValue (id::lbHarshSpeed);
    lbFeedIn      = state.getRawParameterValue (id::lbFeedIn);
    lbFeedAmount  = state.getRawParameterValue (id::lbFeedAmount);
    for (int i = 0; i < enh::dsp::designed::numParams; ++i)
    {
        const auto& d = enh::dsp::designed::params[(size_t) i];
        designedParams[(size_t) i] = state.getRawParameterValue (juce::String (d.id.data(), d.id.size()));
    }
    charActive    = state.getRawParameterValue (id::charActive);
    abCompare     = state.getRawParameterValue (id::abCompare);
    charGrit      = state.getRawParameterValue (id::charGrit);
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
    k.radarSens      = radarSens->load();
    k.radarBoost     = radarBoost->load();
    k.radarSpace     = radarSpace->load();
    k.radarReach     = radarReach->load();
    k.radarListen    = radarListen->load() > 0.5f;
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
    k.deepDepth      = deepDepth->load();
    k.deepHull       = deepHull->load();
    k.deepSize       = deepSize->load();
    k.deepPressure   = deepPressure->load();
    k.deepActive     = deepActive->load() > 0.5f;
    k.charModelA     = charModelA->load();
    k.charModelB     = charModelB->load();
    k.charBlend      = charBlend->load();
    k.charDrive      = charDrive->load();
    k.charColour     = charColour->load();
    k.lbEqIn         = lbEqIn->load() > 0.5f;
    k.lbHpf          = lbHpf->load();
    k.lbLowFreq      = lbLowFreq->load();
    k.lbLowGain      = lbLowGain->load();
    k.lbMidFreq      = lbMidFreq->load();
    k.lbMidGain      = lbMidGain->load();
    k.lbMidHiQ       = lbMidHiQ->load() > 0.5f;
    k.lbHighGain     = lbHighGain->load();
    k.lbIron         = lbIron->load() > 0.5f;
    k.lbHarshIn      = lbHarshIn->load() > 0.5f;
    k.lbHarshAmount  = lbHarshAmount->load();
    k.lbHarshFreq    = lbHarshFreq->load();
    k.lbHarshSpeed   = lbHarshSpeed->load();
    k.lbFeedIn       = lbFeedIn->load() > 0.5f;
    k.lbFeedAmount   = lbFeedAmount->load();
    for (int i = 0; i < enh::dsp::designed::numParams; ++i)
        if (designedParams[(size_t) i] != nullptr)
            k.designed[(size_t) i] = designedParams[(size_t) i]->load();
    k.charActive     = charActive->load() > 0.5f;
    k.compare        = abCompare->load() > 0.5f;
    k.stored         = storedUnits.load (std::memory_order_relaxed) | patchBypass.load (std::memory_order_relaxed);
    k.lbStored       = storedModules.load (std::memory_order_relaxed);
    k.charGrit       = charGrit->load() > 0.5f;
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
            // THE GEAR LOCKER (a session saved before it existed: the rack as it was, the designed units stored)
            // A unit added to the plugin after the session was saved starts in the locker, like a new one
            enh::dsp::rack::Mask stored { (std::uint64_t) (juce::int64) state.state.getProperty ("lockerStored", (juce::int64) enh::dsp::rack::defaultStored.lo),
                                          (std::uint64_t) (juce::int64) state.state.getProperty ("lockerStoredHi", (juce::int64) enh::dsp::rack::defaultStored.hi) };
            const int knewUnits = (int) state.state.getProperty ("lockerUnits", enh::dsp::rack::takeback);   // (TAKEBACK came after the first locker)
            // CUSTOM is always the last unit, so a newer unit added since moves it along: its place in the
            // locker moves with it (it has been last since it came, in 3.8.0.0 at 70: 71 units)
            if (knewUnits >= 71 && knewUnits < enh::dsp::rack::numUnits)
            {
                const int oldCustom = knewUnits - 1;
                const bool customStored = ((stored >> oldCustom) & 1u) != 0u;
                stored &= ~enh::dsp::rack::bit (oldCustom);
                stored &= ~enh::dsp::rack::bit (enh::dsp::rack::custom);
                if (customStored) stored |= enh::dsp::rack::bit (enh::dsp::rack::custom);
                for (int u = oldCustom; u < enh::dsp::rack::numUnits; ++u)   // (the units new since: in the locker, as new ones start)
                    if (u != enh::dsp::rack::custom) stored |= enh::dsp::rack::defaultStored & enh::dsp::rack::bit (u);
            }
            for (int u = std::max (0, knewUnits); u < enh::dsp::rack::numUnits; ++u)
                if (knewUnits < 71 || u != enh::dsp::rack::custom)   // (CUSTOM's own place was carried over above)
                    stored |= enh::dsp::rack::defaultStored & enh::dsp::rack::bit (u);
            storedUnits.store (stored, std::memory_order_relaxed);
            // The LUNCHBOX's modules the same way (a module added later starts in its locker)
            auto lbStored = (enh::dsp::rack::LbMask) (juce::int64) state.state.getProperty ("lunchboxStored", (juce::int64) enh::dsp::rack::defaultLbStored);
            const int knewModules = (int) state.state.getProperty ("lunchboxModules", enh::dsp::rack::lbFirstGen);
            for (int m = std::max (0, knewModules); m < enh::dsp::rack::lbModules; ++m)
                lbStored |= enh::dsp::rack::defaultLbStored & enh::dsp::rack::lbBit (m);
            storedModules.store (lbStored, std::memory_order_relaxed);
            // CUSTOM: the design it had (its knobs are the session's own)
            setCustomCode (state.state.getProperty ("customCode", juce::String()).toString(), false);
            // THE PATCH BAY: its cords (none: straight through)
            patch = {};
            enh::patch::fromString (state.state.getProperty ("patchCords", juce::String()).toString().toStdString(), patch);
            applyPatch();
            patchVersion.fetch_add (1, std::memory_order_relaxed);
        }
}

void PluginProcessor::applyPatch()
{
    namespace rk = enh::dsp::rack;
    namespace un = enh::dsp::units;
    using E = enh::dsp::EnhEngine;
    std::vector<int> order;
    bool loop = false;
    if (patch.cords.empty())
    {
        patchBypass.store (rk::Mask {}, std::memory_order_relaxed);
        patchMuted = false;
    }
    else
    {
        const auto left = enh::patch::follow (patch, 0), right = enh::patch::follow (patch, 1);
        // A loop of cords is feedback (ear-guarded in the engine); a cord out of the chain is silence
        loop = left.loop || right.loop;
        patchMuted = ! ((left.complete || left.loop) && (right.complete || right.loop));
        // Every unit the signal does not go through is passed by (POWER and the OUTPUT MONITOR always run)
        rk::Mask inPath {};
        for (const auto* path : { &left, &right })
            for (int u : path->units)
                if (u >= 0 && u < rk::numUnits)
                    inPath |= rk::bit (u);
        rk::Mask bypass {};
        for (int u = 0; u < rk::numUnits; ++u)
            if (u != rk::power && u != rk::monitor && ((inPath >> u) & 1u) == 0u)
                bypass |= rk::bit (u);
        patchBypass.store (bypass, std::memory_order_relaxed);
        // The units after the enhancer run in the order the left side's cords take them
        for (int u : left.units)
        {
            int code = -1;
            switch (u)
            {
                case rk::leveler:   code = E::stLumen; break;
                case rk::deepSub:   code = E::stDeep; break;
                case rk::limiter:   code = E::stLimiter; break;
                case rk::balancer:  code = E::stBalancer; break;
                case rk::compressor:code = E::stTide; break;
                case rk::radar:     code = E::stRadar; break;
                case rk::toneSpace: code = E::stSeraph; break;
                case rk::character: code = E::stCharacter; break;
                case rk::x4:        code = E::stX4; break;
                case rk::velvet:    code = E::stVelvet; break;
                case rk::takeback:  code = E::stTakeback; break;
                case rk::custom:    code = E::stCustom; break;
                case rk::lunchbox:  code = E::stLunchbox; break;
                default:
                    if (u >= un::firstUnit && u < un::firstUnit + un::count) code = E::stNewer + (u - un::firstUnit);
                    break;
            }
            if (code >= 0)
                order.push_back (code);
        }
    }
    engine.setChainOrder (order);
    engine.setFeedback (loop);
    patchLoop = loop;
    engine.setPatch (patchMuted, patchNoise);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
