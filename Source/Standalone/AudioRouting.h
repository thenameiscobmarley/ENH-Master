#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

/*  Audio routing for the standalone app: what the router needs from the operating system.

    The router puts the rack *in the path* of audio that is already playing somewhere, and later puts
    everything back where it was. To do that it has to be able to
      - list the playback devices, and read / change which one is the default,
      - list the programs that are playing, and read / change which device each one plays to,
      - (Linux) make a device of its own for the rack to capture, and point its own streams at it.

    Windows: IPolicyConfig for the default device, the per-app device setting that Settings > Sound >
    "App volume and device preferences" uses (the one EarTrumpet uses too), and WASAPI sessions to see
    who is playing (AudioRoutingWindows.cpp).
    Linux: PipeWire / PulseAudio through pactl (AudioRoutingLinux.cpp).
*/
namespace pad::routing
{
    /** A playback device. */
    struct Endpoint
    {
        juce::String id;     // Windows: the MMDevice id; Linux: the sink name
        juce::String name;   // what to show
        bool isDefault = false;
    };

    /** One program that has audio open, with all of its streams. */
    struct App
    {
        juce::String key;           // the same for every stream of one program: its executable, lower case
        juce::String name;          // what to show
        juce::String endpointId;    // the device it plays to now
        bool followsDefault = true; // no device of its own: it plays wherever the default is
        juce::Array<int> handles;   // Windows: process ids; Linux: stream (sink-input) indices
        bool active = false;        // making sound right now
    };

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual std::vector<Endpoint> outputs() = 0;
        virtual juce::String defaultOutput() = 0;
        virtual bool setDefaultOutput (const juce::String& endpointId) = 0;

        /** Programs with audio open, not counting this one. */
        virtual std::vector<App> apps() = 0;

        /** Sends every stream of `app` to `endpointId`, or back to following the default when it is empty.
            `handle` limits it to one of app.handles (-1: all of them). */
        virtual bool moveApp (const App& app, int handle, const juce::String& endpointId) = 0;

        /** Where one handle of an app plays to now ("" = it follows the default). */
        virtual juce::String endpointOfHandle (int handle) = 0;

        /** Linux makes a device of its own for the rack to capture; Windows cannot, and the user picks
            one (a virtual cable, or an output with nothing plugged into it). */
        virtual bool canCreateRackInput() const { return false; }
        virtual Endpoint createRackInput() { return {}; }
        virtual void destroyRackInput (const juce::String& /*token*/) {}
        virtual juce::String rackInputToken() const { return {}; }   // what destroyRackInput needs (the module index)

        /** Once this app's audio is running: point its capture at the rack input and its playback at
            the listening device. Windows opens the devices by id already, so it has nothing to do. */
        virtual bool connectOwnStreams (const juce::String& /*rackInputId*/, const juce::String& /*listenId*/) { return true; }

        /** Whether they are still connected like that. While the rack is in, the bar asks this every
            second and keeps the rack deaf while it is not true: a capture that slipped back to the
            default source would otherwise be a microphone played into the headset. */
        virtual bool ownStreamsConnected (const juce::String& /*rackInputId*/, const juce::String& /*listenId*/) { return true; }

        juce::String getLastError() const { return error; }

    protected:
        juce::String error;
    };

    /** The backend for this platform (never null; on an unsupported one every call fails politely). */
    std::unique_ptr<Backend> createBackend();

    //==============================================================================
    /** What the user asked for. */
    struct Plan
    {
        enum class Source { wholeSystem, chosenApps };

        Source source = Source::wholeSystem;
        juce::String rackInputId;     // "" on Linux = make one
        juce::String listenId;        // where the processed audio goes
        juce::StringArray appKeys;    // for chosenApps
    };

    /** Moves audio into the rack and back out, and remembers - on disk, before touching anything - how
        things were, so that a crash, a kill or a power cut cannot leave the computer's sound pointing
        at a device nobody is listening to. The next start (or the watchdog) puts it back. */
    class Router
    {
    public:
        Router (Backend& backendToUse, juce::File journalFileToUse);

        /** Reroutes. The rack's own audio must already be running (see Backend::connectOwnStreams). */
        bool insert (const Plan& plan, const juce::String& rackInputId, const juce::String& rackInputToken);

        /** Puts everything back, as far as it can, and forgets the journal. */
        bool remove();

        bool isInserted() const { return journal.isObject(); }

        /** While inserted (chosen apps): moves any new stream of a chosen app into the rack, and any
            stream moved earlier that is no longer there back into it. Returns how many moved. */
        int followNewStreams();

        /** A journal left behind by a run that did not finish: undo it. Returns a line for the user,
            or "" if there was nothing to do. */
        juce::String recoverLeftovers();

        juce::String getLastError() const { return error; }
        juce::File getJournalFile() const { return journalFile; }

        /** The journal, as the watchdog and the tests see it. */
        static juce::var readJournal (const juce::File& f);

        /** Undoes a journal (used by remove(), recovery and the watchdog). */
        static bool undo (Backend& backend, const juce::var& journal, juce::String& errorOut);

    private:
        bool save();

        Backend& backend;
        juce::File journalFile;
        juce::var journal;
        juce::String error;
    };

    /** Where the journal lives: next to the app's settings. */
    juce::File defaultJournalFile();

    /** The watchdog: a second copy of the app, started with this argument, that waits for the first
        one to exit and undoes the journal if it is still there (a crash or a kill). */
    inline constexpr const char* watchdogArgument = "--enh-restore-watch";

    /** Runs the watchdog loop for `pid` (returns when that process is gone and the journal handled). */
    void runWatchdog (int pid, const juce::File& journal);

    /** Starts the watchdog for this process. Keep what it returns and ask it isRunning() now and then:
        that is what reaps it once it has finished (it is never killed - outliving us is its job). */
    std::unique_ptr<juce::ChildProcess> startWatchdog (const juce::File& journal);
}
