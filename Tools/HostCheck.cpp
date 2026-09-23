/*  EnhHostCheck - loads the installed ENH Master VST3 the way a host (Carla) does, feeds it noise, moves
    every parameter at random as a user would, and reports what a host would see: blocks of silence
    while there is input, anything that is not a number, and how long each block took against the time
    it lasts (over 100 % is an xrun: the host drops audio).

      EnhHostCheck [plugin.vst3] [seconds] [block] [seed] [--char]
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String path = argc > 1 ? argv[1] : (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName() + "/.vst3/ENH Master.vst3");
    const double seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 30.0;
    const int block = argc > 3 ? juce::String (argv[3]).getIntValue() : 512;
    const int seed = argc > 4 ? juce::String (argv[4]).getIntValue() : 1;
    bool charOn = false;
    for (int i = 1; i < argc; ++i) if (juce::String (argv[i]) == "--char") charOn = true;
    const double sr = 48000.0;

    juce::AudioPluginFormatManager fm;
    fm.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> types;
    fm.getFormat (0)->findAllTypesForFile (types, path);
    if (types.isEmpty()) { std::printf ("no plugin at %s\n", path.toRawUTF8()); return 1; }

    juce::String err;
    auto plugin = fm.createPluginInstance (*types[0], sr, block, err);
    if (plugin == nullptr) { std::printf ("could not load: %s\n", err.toRawUTF8()); return 1; }
    std::printf ("loaded %s %s, latency %d\n", types[0]->name.toRawUTF8(), types[0]->version.toRawUTF8(), plugin->getLatencySamples());

    plugin->setPlayConfigDetails (2, 2, sr, block);
    plugin->prepareToPlay (sr, block);

    auto& params = plugin->getParameters();
    auto byId = [&] (const char* want) -> juce::AudioProcessorParameter*
    {
        for (auto* p : params)
            if (auto* w = dynamic_cast<juce::HostedAudioProcessorParameter*> (p); w != nullptr && w->getParameterID() == want)
                return p;
        return nullptr;
    };
    if (charOn)
        if (auto* p = byId ("charActive")) p->setValueNotifyingHost (1.0f);

    juce::Random rng (seed);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int blocks = (int) (seconds * sr / block);
    int silent = 0, nonFinite = 0, over = 0;
    double worst = 0.0, total = 0.0;
    float a0 = 0, a1 = 0;
    juce::String lastMoves;

    for (int b = 0; b < blocks; ++b)
    {
        // A few parameters move each block (a knob being turned, automation)
        lastMoves.clear();
        for (int k = 0, n = rng.nextInt (3); k < n; ++k)
        {
            auto* p = params[rng.nextInt (params.size())];
            const float v = rng.nextFloat();
            p->setValueNotifyingHost (v);
            if (auto* w = dynamic_cast<juce::HostedAudioProcessorParameter*> (p))
                lastMoves << w->getParameterID() << "=" << juce::String (v, 2) << " ";
        }

        double inE = 0.0;
        for (int i = 0; i < block; ++i)
        {
            const float w = rng.nextFloat() * 2.0f - 1.0f;
            a0 = 0.97f * a0 + 0.3f * w; a1 = 0.6f * a1 + w;
            const float v = 0.15f * (a0 + 0.4f * a1);
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
            inE += (double) v * v;
        }

        const auto t0 = juce::Time::getHighResolutionTicks();
        plugin->processBlock (buf, midi);
        const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
        const double budget = 1000.0 * block / sr;
        total += ms;
        worst = std::max (worst, ms / budget);
        if (ms > budget) ++over;

        double outE = 0.0;
        bool bad = false;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const float y = buf.getSample (c, i);
                if (! std::isfinite (y)) bad = true;
                outE += (double) y * y;
            }
        if (bad) ++nonFinite;
        if (b * block > sr * 2 && inE > 1e-6 && outE == 0.0)
        {
            if (silent < 5) std::printf ("  silent block %d (%.2f s); moved just now: %s\n", b, b * block / sr, lastMoves.toRawUTF8());
            ++silent;
        }
    }

    std::printf ("blocks %d (%d samples): silent %d, non-finite %d, over budget %d; average %.1f %%, worst %.0f %% of real time\n",
                 blocks, block, silent, nonFinite, over, 100.0 * total / (blocks * 1000.0 * block / sr), 100.0 * worst);
    plugin->releaseResources();
    return silent == 0 && nonFinite == 0 ? 0 : 1;
}
