#include "ApoCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

namespace enh::apo
{
    using namespace enh::shared;

    ApoCore::ApoCore() noexcept
        : table (rackParams())   // built here, never on the audio thread
    {
        defaultParamValues (table, values.data());
        defaultModifiers (modifiers.data());
    }

    bool ApoCore::prepare (double sampleRate, int numChannels, int maxFrames) noexcept
    {
        release();
        if (! (sampleRate >= 8000.0 && sampleRate <= 768000.0) || numChannels < 1 || numChannels > 32 || maxFrames < 1)
            return false;
        try
        {
            if (engine == nullptr)
                engine = std::make_unique<enh::dsp::EnhEngine>();
            rate = sampleRate;
            channels = numChannels;
            engineChannels = std::min (2, numChannels);
            maxBlock = std::min (maxFrames, 8192);

            engine->prepare (rate, maxBlock, engineChannels);
            latency = engine->getLatencySamples();

            for (auto& p : planar)
                p.assign ((size_t) maxBlock, 0.0f);
            extraDelay.assign ((size_t) std::max (0, channels - engineChannels) * (size_t) std::max (0, latency), 0.0f);
            extraPos = 0;

            // Point the engine's buffer view at our channels once here, so the audio thread only
            // re-points it (JUCE keeps up to 32 channel pointers inside the object: no allocation)
            float* chans[2] = { planar[0].data(), planar[1].data() };
            view.setDataToReferTo (chans, engineChannels, maxBlock);
        }
        catch (...)   // bad_alloc: stay in pass-through
        {
            release();
            return false;
        }

        smoothers = {};
        silentInputFrames = quietOutputFrames = 0;
        idle = false;
        failsafe = false;
        scopeRead[scopeInput]       = (std::uint32_t) engine->getInputScope().writeIndex.load();
        scopeRead[scopeOutput]      = (std::uint32_t) engine->getOutputScope().writeIndex.load();
        scopeRead[scopeBalancerIn]  = (std::uint32_t) engine->getBalancerInputScope().writeIndex.load();
        scopeRead[scopeBalancerOut] = (std::uint32_t) engine->getBalancerOutputScope().writeIndex.load();
        prepared = true;
        return true;
    }

    void ApoCore::release() noexcept
    {
        prepared = false;
    }

    void ApoCore::openBridge (const std::string& key, DisplayBridge::Scope scope) noexcept
    {
        try
        {
            if (! bridge.isOpen())
                bridge.open (key, true, scope);
        }
        catch (...) {}
    }

    void ApoCore::useSettings (const ParamSnapshot& saved) noexcept
    {
        if (saved.layoutHash != table.layoutHash || saved.count > (std::uint32_t) kMaxParams)
            return;
        for (int i = 0; i < table.count; ++i)
            values[(size_t) i] = i < (int) saved.count ? saved.values[(size_t) i] : table.defs[(size_t) i].defaultValue;
        modifiers = saved.modifiers;
    }

    //==============================================================================================
    void ApoCore::readParameters() noexcept
    {
        if (! bridge.isOpen())
            return;
        // Bounded: one try and one retry; if the UI is mid-write we keep what we have (next call gets it)
        if (bridge.readParams (incoming, paramSeq, 2) != SeqBlock<ParamSnapshot>::Read::fresh)
            return;
        if (incoming.layoutHash != table.layoutHash || incoming.count > (std::uint32_t) kMaxParams)
            return;   // an app with another parameter list: not ours to use

        const int n = std::min ((int) incoming.count, table.count);
        for (int i = 0; i < n; ++i)
            values[(size_t) i] = incoming.values[(size_t) i];   // sanitised when mapped
        modifiers = incoming.modifiers;
        usingUi = true;

        if (haveResetCount && incoming.loudnessResets != loudnessResets)
            engine->resetLoudness();   // an atomic flag: real-time safe
        loudnessResets = incoming.loudnessResets;
        haveResetCount = true;
    }

    void ApoCore::passThrough (const float* in, float* out, std::uint32_t frames, bool inputSilent) noexcept
    {
        if (inputSilent || in == nullptr || out == nullptr || channels <= 0)
            return;
        if (in != out)
            std::memcpy (out, in, sizeof (float) * (size_t) frames * (size_t) channels);
    }

    bool ApoCore::process (const float* in, float* out, std::uint32_t frames, bool inputSilent) noexcept
    {
        if (frames == 0)
            return inputSilent;
        if (! prepared || failsafe || in == nullptr || out == nullptr)
        {
            passThrough (in, out, frames, inputSilent);
            return inputSilent;
        }

        ++calls;
        readParameters();

        // Silence: keep processing (tails, reverb, the look-ahead) until the output has died away
        if (inputSilent)
            silentInputFrames += frames;
        else
            silentInputFrames = 0, idle = false;

        if (inputSilent && silentInputFrames > (std::int64_t) (2.0 * rate) && quietOutputFrames > (std::int64_t) (2.0 * rate))
        {
            idle = true;
            if (bridge.isOpen())
            {
                bridge.beat (DisplayBridge::Role::audio);
                framesSincePublish += frames;
                if (framesSincePublish >= (std::int64_t) (rate / 30.0))
                    publish();
            }
            return true;
        }

        const int extra = channels - engineChannels;
        float peak = 0.0f;

        for (std::uint32_t start = 0; start < frames;)
        {
            const int n = (int) std::min<std::uint32_t> ((std::uint32_t) maxBlock, frames - start);
            const float* src = in + (size_t) start * (size_t) channels;
            float* dst = out + (size_t) start * (size_t) channels;

            // De-interleave ENH Master's channels (zeros for a silent input)
            for (int c = 0; c < engineChannels; ++c)
            {
                float* p = planar[(size_t) c].data();
                if (inputSilent)
                    std::fill (p, p + n, 0.0f);
                else
                    for (int i = 0; i < n; ++i)
                        p[i] = src[(size_t) i * (size_t) channels + (size_t) c];
            }

            // The other channels: a plain delay of the engine's latency, so they stay in time.
            // Read-then-write per sample, so in-place buffers are fine.
            if (extra > 0)
            {
                for (int i = 0; i < n; ++i)
                {
                    for (int e = 0; e < extra; ++e)
                    {
                        const size_t at = (size_t) i * (size_t) channels + (size_t) (engineChannels + e);
                        const float x = inputSilent ? 0.0f : src[at];
                        if (latency > 0)
                        {
                            float& slot = extraDelay[(size_t) e * (size_t) latency + (size_t) extraPos];
                            dst[at] = slot;
                            slot = x;
                        }
                        else
                        {
                            dst[at] = x;
                        }
                    }
                    if (latency > 0 && ++extraPos >= latency)
                        extraPos = 0;
                }
            }

            // The parameters, as the plugin maps them (knob modifiers per chunk, like per host block)
            enh::dsp::KnobValues k;
            toKnobValues (table, values.data(), table.count, k);
            applyKnobModifiers (table, k, modifiers.data(), smoothers, n, rate);
            const auto params = enh::dsp::mapKnobs (k);

            float* chans[2] = { planar[0].data(), planar[1].data() };
            view.setDataToReferTo (chans, engineChannels, n);   // <= 2 channels: no allocation
            engine->process (view, params);

            // Back out, with a last guard: nothing that isn't a number, nothing past full scale
            for (int c = 0; c < engineChannels; ++c)
            {
                const float* p = planar[(size_t) c].data();
                for (int i = 0; i < n; ++i)
                {
                    float y = p[i];
                    if (! (std::abs (y) <= 1.0f))
                        y = std::isfinite (y) ? std::clamp (y, -1.0f, 1.0f) : 0.0f;
                    peak = std::max (peak, std::abs (y));
                    dst[(size_t) i * (size_t) channels + (size_t) c] = y;
                }
            }
            start += (std::uint32_t) n;
        }

        if (peak < 1.0e-6f)
            quietOutputFrames += frames;
        else
            quietOutputFrames = 0;

        if (bridge.isOpen())
        {
            bridge.beat (DisplayBridge::Role::audio);
            framesSincePublish += frames;
            if (framesSincePublish >= (std::int64_t) (rate / 60.0))   // ~60 times a second is plenty for a display
                publish();
        }
        return false;
    }

    void ApoCore::publish() noexcept
    {
        framesSincePublish = 0;

        auto& st = outgoing.status;
        st.sampleRate = (float) rate;
        st.latencySamples = latency;
        st.channels = channels;
        st.maxFrames = maxBlock;
        st.processCalls = calls;
        st.flags = statusSystemApo | (prepared ? statusRunning : 0u) | (failsafe ? statusFailsafe : 0u)
                 | (idle ? statusIdle : 0u) | (usingUi ? statusUsingUi : 0u);
        captureMeters (engine->getMeters(), outgoing.meters);
        bridge.publishMeters (outgoing);   // skipped if another writer holds the block: next time

        // The four analyser taps: whatever the engine wrote since last time (at most one ring's worth)
        const enh::dsp::ScopeFifo* fifos[kNumScopes] = { &engine->getInputScope(), &engine->getOutputScope(),
                                                         &engine->getBalancerInputScope(), &engine->getBalancerOutputScope() };
        static_assert (enh::dsp::ScopeFifo::size == kScopeSize, "ScopeFifo and the bridge's scope ring must be the same size");
        for (int s = 0; s < kNumScopes; ++s)
        {
            const auto& f = *fifos[s];
            const std::uint32_t w = (std::uint32_t) f.writeIndex.load (std::memory_order_acquire);
            std::uint32_t from = scopeRead[(size_t) s];
            std::uint32_t count = w - from;
            if (count > (std::uint32_t) kScopeSize)
                from = w - (std::uint32_t) kScopeSize, count = (std::uint32_t) kScopeSize;
            while (count > 0)
            {
                const std::uint32_t at = from & (std::uint32_t) enh::dsp::ScopeFifo::mask;
                const std::uint32_t run = std::min (count, (std::uint32_t) kScopeSize - at);   // up to the ring's end
                bridge.pushScope (s, f.samples.data() + at, (int) run);
                from += run;
                count -= run;
            }
            scopeRead[(size_t) s] = w;
        }
    }
}
