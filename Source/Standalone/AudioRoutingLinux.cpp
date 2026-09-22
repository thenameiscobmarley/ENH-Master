#include "AudioRouting.h"

#if JUCE_LINUX || JUCE_BSD

#include <algorithm>
#include <map>
#include <unistd.h>

/*  Linux: PipeWire (or PulseAudio) through pactl.

    The rack input is a null sink of our own ("ENH Master rack input"). Whatever plays to it is
    captured from its monitor, processed, and played to the device the user listens on:

        game ──► ENH Master rack input (null sink) ──monitor──► ENH Master ──► headset

    pactl is used rather than libpulse so that nothing extra has to be linked or installed: it ships
    with pipewire-pulse and with PulseAudio. Every call is a short child process on the message
    thread, never on the audio thread.
*/
namespace pad::routing
{
    namespace
    {
        constexpr const char* rackSinkName = "enh_rack_input";

        struct Result
        {
            bool ok = false;
            juce::String output;
        };

        Result pactl (juce::StringArray args)
        {
            args.insert (0, "pactl");

            juce::ChildProcess p;
            if (! p.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
                return { false, "pactl is not installed (it comes with pipewire-pulse)." };

            auto out = p.readAllProcessOutput();
            p.waitForProcessToFinish (5000);
            return { p.getExitCode() == 0, out.trim() };
        }

        juce::var pactlJson (const juce::String& what)
        {
            auto r = pactl ({ "-f", "json", "list", what });
            if (! r.ok)
                return {};

            auto v = juce::JSON::parse (r.output);
            return v.isArray() ? v : juce::var();
        }

        juce::String prop (const juce::var& item, const char* key)
        {
            return item["properties"][key].toString();
        }

        /** Programs that play through ALSA (JUCE apps, many games under Wine) reach PipeWire through
            its ALSA plugin, which names the stream "PipeWire ALSA [program]" and gives no process id.
            Returns "program" for those, "" for anything else. */
        juce::String alsaProgramName (const juce::var& item)
        {
            const auto name = prop (item, "application.name");
            if ((name.startsWith ("PipeWire ALSA [") || name.startsWith ("ALSA plug-in [")) && name.endsWithChar (']'))
                return name.fromFirstOccurrenceOf ("[", false, false).dropLastCharacters (1);
            return {};
        }

        /** One of this app's own streams. */
        bool isOurs (const juce::var& item)
        {
            static const auto pid = juce::String ((int) getpid());
            static const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFileName();
            return prop (item, "application.process.id") == pid || alsaProgramName (item) == exe;
        }
    }

    class LinuxBackend final : public Backend
    {
    public:
        std::vector<Endpoint> outputs() override
        {
            std::vector<Endpoint> found;
            const auto def = defaultOutput();

            if (auto sinks = pactlJson ("sinks"); auto* arr = sinks.getArray())
            {
                for (auto& s : *arr)
                {
                    Endpoint e;
                    e.id = s["name"].toString();
                    e.name = s["description"].toString();
                    if (e.name.isEmpty())
                        e.name = e.id;
                    e.isDefault = e.id == def;
                    found.push_back (e);
                }
            }

            return found;
        }

        juce::String defaultOutput() override
        {
            auto r = pactl ({ "get-default-sink" });
            return r.ok ? r.output : juce::String();
        }

        bool setDefaultOutput (const juce::String& id) override
        {
            auto r = pactl ({ "set-default-sink", id });
            if (! r.ok)
                error = r.output;
            return r.ok;
        }

        std::vector<App> apps() override
        {
            std::vector<App> found;
            auto sinks = sinkNamesByIndex();
            const auto def = defaultOutput();

            if (auto inputs = pactlJson ("sink-inputs"); auto* arr = inputs.getArray())
            {
                for (auto& s : *arr)
                {
                    if (isOurs (s))
                        continue;

                    auto name = prop (s, "application.name");
                    auto binary = prop (s, "application.process.binary");

                    if (auto alsa = alsaProgramName (s); alsa.isNotEmpty())
                        name = binary = alsa;
                    auto key = (binary.isNotEmpty() ? binary : name.isNotEmpty() ? name : prop (s, "node.name")).toLowerCase();

                    if (key.isEmpty())
                        continue;

                    if (name.isEmpty())
                        name = binary.isNotEmpty() ? binary : key;

                    const auto handle = (int) s["index"];
                    const auto sink = sinks[(int) s["sink"]];   // "" if the sink is gone

                    auto it = std::find_if (found.begin(), found.end(), [&] (const App& a) { return a.key == key; });
                    if (it == found.end())
                    {
                        App a;
                        a.key = key;
                        a.name = name;
                        a.endpointId = sink;
                        a.followsDefault = sink == def;
                        found.push_back (a);
                        it = found.end() - 1;
                    }

                    it->handles.add (handle);
                    it->active = it->active || ! (bool) s["corked"];
                    handleSinks[handle] = sink;
                }
            }

            return found;
        }

        bool moveApp (const App& app, int handle, const juce::String& endpointId) override
        {
            const auto target = endpointId.isNotEmpty() ? endpointId : defaultOutput();
            bool ok = true;

            for (auto h : app.handles)
            {
                if (handle >= 0 && h != handle)
                    continue;

                auto r = pactl ({ "move-sink-input", juce::String (h), target });
                if (! r.ok)
                {
                    error = r.output;
                    ok = false;
                }
            }

            return ok;
        }

        juce::String endpointOfHandle (int handle) override
        {
            apps();   // refresh
            return handleSinks[handle];
        }

        bool canCreateRackInput() const override { return true; }

        Endpoint createRackInput() override
        {
            token = {};

            if (! sinkExists (rackSinkName))
            {
                auto r = pactl ({ "load-module", "module-null-sink",
                                  juce::String ("sink_name=") + rackSinkName,
                                  "sink_properties=device.description=\"ENH Master rack input\"",
                                  "channels=2", "channel_map=front-left,front-right" });
                if (! r.ok)
                {
                    error = "Could not make the rack input: " + r.output;
                    return {};
                }

                token = r.output;

                // pipewire-pulse makes the sink a moment after answering.
                for (int i = 0; i < 40 && ! sinkExists (rackSinkName); ++i)
                    juce::Thread::sleep (50);
            }

            return { rackSinkName, "ENH Master rack input", false };
        }

        juce::String rackInputToken() const override { return token; }

        void destroyRackInput (const juce::String& moduleIndex) override
        {
            if (moduleIndex.isNotEmpty())
                pactl ({ "unload-module", moduleIndex });
        }

        bool connectOwnStreams (const juce::String& rackInputId, const juce::String& listenId) override
        {
            // Our streams show up a moment after the device opens, and JUCE may reopen the device
            // (replacing them) while it settles. Keep moving whatever is not where it belongs until
            // everything is, twice in a row.
            int settled = 0;

            for (int attempt = 0; attempt < 40; ++attempt)
            {
                const auto state = ownStreams (rackInputId + ".monitor", listenId);

                if (state.complete())
                {
                    if (++settled >= 2)
                        return true;
                }
                else
                {
                    settled = 0;

                    for (auto h : state.playbackElsewhere)
                        pactl ({ "move-sink-input", juce::String (h), listenId });

                    for (auto h : state.captureElsewhere)
                        pactl ({ "move-source-output", juce::String (h), rackInputId + ".monitor" });
                }

                juce::Thread::sleep (60);
            }

            error = "ENH Master could not connect its own audio to the rack input and your listening device.";
            return false;
        }

        bool ownStreamsConnected (const juce::String& rackInputId, const juce::String& listenId) override
        {
            return ownStreams (rackInputId + ".monitor", listenId).complete();
        }

    private:
        struct OwnStreams
        {
            int playbackCount = 0, captureCount = 0;
            juce::Array<int> playbackElsewhere, captureElsewhere;

            bool complete() const
            {
                return playbackCount > 0 && captureCount > 0 && playbackElsewhere.isEmpty() && captureElsewhere.isEmpty();
            }
        };

        /** Where our streams are, compared with where they should be. */
        static OwnStreams ownStreams (const juce::String& captureSource, const juce::String& playbackSink)
        {
            OwnStreams o;
            std::map<int, juce::String> sourceNames;

            if (auto sources = pactlJson ("sources"); auto* arr = sources.getArray())
                for (auto& x : *arr)
                    sourceNames[(int) x["index"]] = x["name"].toString();

            auto sinks = sinkNamesByIndex();

            if (auto a = pactlJson ("sink-inputs"); auto* arr = a.getArray())
                for (auto& x : *arr)
                    if (isOurs (x))
                    {
                        ++o.playbackCount;
                        if (sinks[(int) x["sink"]] != playbackSink)
                            o.playbackElsewhere.add ((int) x["index"]);
                    }

            if (auto a = pactlJson ("source-outputs"); auto* arr = a.getArray())
                for (auto& x : *arr)
                    if (isOurs (x))
                    {
                        ++o.captureCount;
                        if (sourceNames[(int) x["source"]] != captureSource)
                            o.captureElsewhere.add ((int) x["index"]);
                    }

            return o;
        }

        static bool sinkExists (const juce::String& name)
        {
            if (auto sinks = pactlJson ("sinks"); auto* arr = sinks.getArray())
                for (auto& s : *arr)
                    if (s["name"].toString() == name)
                        return true;

            return false;
        }

        static std::map<int, juce::String> sinkNamesByIndex()
        {
            std::map<int, juce::String> m;
            if (auto sinks = pactlJson ("sinks"); auto* arr = sinks.getArray())
                for (auto& x : *arr)
                    m[(int) x["index"]] = x["name"].toString();
            return m;
        }

        std::map<int, juce::String> handleSinks;
        juce::String token;
    };

    std::unique_ptr<Backend> createBackend() { return std::make_unique<LinuxBackend>(); }
}

#endif
