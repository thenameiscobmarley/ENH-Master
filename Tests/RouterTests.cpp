/*  EnhRouterTests: the standalone app's router against the real sound server (Linux, PipeWire/Pulse).

    It plays a *silent* stream of its own with paplay, so nothing is heard, and checks that
      1. chosen apps: the stream moves onto the rack input, and back to where it was on REMOVE,
         and the rack input the router made is gone again,
      2. whole system: the default device becomes the rack input, and goes back on REMOVE,
      3. a crash: a journal left by a process that is gone is undone by the next start,
      4. the watchdog: undoes the journal once the process it watches has gone.
    The default device and the test stream are put back even when a check fails.

    Not run in CI: the build machines have no sound server. Run it by hand:
        build/EnhRouterTests_artefacts/Release/EnhRouterTests
*/

#include <juce_core/juce_core.h>
#include "../Source/Standalone/AudioRouting.h"

#include <thread>
#include <unistd.h>
#include <sys/wait.h>

using namespace pad::routing;

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what.toRawUTF8());
        if (! ok)
            ++failures;
    }

    juce::File makeSilentWav (const juce::File& dir)
    {
        auto f = dir.getChildFile ("silence.wav");
        juce::MemoryOutputStream m;
        const int rate = 48000, seconds = 30, frames = rate * seconds, bytes = frames * 4;
        m.write ("RIFF", 4); m.writeInt (36 + bytes); m.write ("WAVE", 4);
        m.write ("fmt ", 4); m.writeInt (16); m.writeShort (1); m.writeShort (2);
        m.writeInt (rate); m.writeInt (rate * 4); m.writeShort (4); m.writeShort (16);
        m.write ("data", 4); m.writeInt (bytes);
        juce::HeapBlock<char> zeros ((size_t) bytes, true);
        m.write (zeros.get(), (size_t) bytes);
        f.replaceWithData (m.getData(), m.getDataSize());
        return f;
    }

    const App* findApp (const std::vector<App>& apps, const juce::String& key)
    {
        for (auto& a : apps)
            if (a.key == key)
                return &a;
        return nullptr;
    }

    juce::String sinkOfApp (Backend& b, const juce::String& key)
    {
        auto apps = b.apps();
        if (auto* a = findApp (apps, key))
            return b.endpointOfHandle (a->handles.getFirst());
        return {};
    }

    bool sinkExists (Backend& b, const juce::String& id)
    {
        for (auto& e : b.outputs())
            if (e.id == id)
                return true;
        return false;
    }

    /** The sound server removes a sink a moment after it is asked to (longer when the machine is busy). */
    bool sinkGone (Backend& b, const juce::String& id)
    {
        for (int i = 0; i < 40; ++i, juce::Thread::sleep (50))
            if (! sinkExists (b, id))
                return true;
        return false;
    }
}

int main()
{
    auto backend = createBackend();
    auto dir = juce::File::createTempFile ("enh-router-test");
    dir.createDirectory();

    const auto originalDefault = backend->defaultOutput();
    std::printf ("Default device now: %s\n", originalDefault.toRawUTF8());

    if (originalDefault.isEmpty())
    {
        std::printf ("No sound server (pactl get-default-sink failed): nothing to test.\n");
        return 0;
    }

    // A silent stream to move about, with a name we can find.
    auto wav = makeSilentWav (dir);
    juce::ChildProcess player;
    player.start (juce::StringArray { "paplay", "--client-name=enh-router-test", wav.getFullPathName() }, 0);

    // paplay is a link to pacat, and the key is the real executable: find it by the name it gave.
    juce::String key;
    for (int i = 0; i < 40 && key.isEmpty(); ++i, juce::Thread::sleep (50))
        for (auto& a : backend->apps())
            if (a.name == "enh-router-test")
                key = a.key;

    {
        std::printf ("1. Chosen apps\n");
        auto apps = backend->apps();
        check (findApp (apps, key) != nullptr, "the test stream is listed as an app");
        const auto before = sinkOfApp (*backend, key);

        auto rack = backend->createRackInput();
        const auto token = backend->rackInputToken();
        check (rack.id.isNotEmpty() && sinkExists (*backend, rack.id), "the router made its rack input");

        Plan plan;
        plan.source = Plan::Source::chosenApps;
        plan.listenId = originalDefault;
        plan.appKeys = { key };

        Router router (*backend, dir.getChildFile ("journal-1.json"));
        const bool inserted = router.insert (plan, rack.id, token);
        check (inserted, "insert " + router.getLastError());
        check (router.getJournalFile().existsAsFile(), "the journal is written");
        // As the app does: every so often, follow (and re-check) the chosen apps' streams
        for (int i = 0; i < 20 && sinkOfApp (*backend, key) != rack.id; ++i, juce::Thread::sleep (100))
            router.followNewStreams();
        check (sinkOfApp (*backend, key) == rack.id, "the stream plays to the rack input (" + sinkOfApp (*backend, key) + ")");

        const bool removed = router.remove();
        check (removed, "remove " + router.getLastError());
        check (sinkOfApp (*backend, key) == before, "the stream is back on " + before + " (" + sinkOfApp (*backend, key) + ")");
        check (sinkGone (*backend, rack.id), "the rack input is gone again");
        check (! router.getJournalFile().existsAsFile(), "the journal is gone");
    }

    {
        std::printf ("2. Whole system\n");
        auto rack = backend->createRackInput();
        Plan plan;
        plan.listenId = originalDefault;

        Router router (*backend, dir.getChildFile ("journal-2.json"));
        const bool inserted = router.insert (plan, rack.id, backend->rackInputToken());
        check (inserted, "insert " + router.getLastError());
        check (backend->defaultOutput() == rack.id, "the default device is the rack input");
        const bool removed = router.remove();
        check (removed, "remove " + router.getLastError());
        check (backend->defaultOutput() == originalDefault, "the default device is back (" + backend->defaultOutput() + ")");
        check (sinkGone (*backend, rack.id), "the rack input is gone again");
    }

    {
        std::printf ("3. Recovery after a crash\n");

        // A process that has already exited stands in for the one that crashed.
        const auto dead = fork();
        if (dead == 0) _exit (0);
        waitpid (dead, nullptr, 0);

        auto rack = backend->createRackInput();
        auto* j = new juce::DynamicObject();
        j->setProperty ("pid", (int) dead);
        j->setProperty ("source", "system");
        j->setProperty ("rackInput", rack.id);
        j->setProperty ("rackInputToken", backend->rackInputToken());
        j->setProperty ("defaultBefore", originalDefault);
        backend->setDefaultOutput (rack.id);

        auto leftover = defaultJournalFile().getSiblingFile ("routing-" + juce::String ((int) dead) + ".json");
        leftover.replaceWithText (juce::JSON::toString (juce::var (j)));

        Router fresh (*backend, defaultJournalFile());
        const auto report = fresh.recoverLeftovers();
        check (report.isNotEmpty(), "the next start noticed: " + report);
        check (backend->defaultOutput() == originalDefault, "the default device is back");
        check (! leftover.existsAsFile(), "the leftover journal is gone");
        check (sinkGone (*backend, rack.id), "the leftover rack input is gone");
    }

    {
        std::printf ("4. Watchdog\n");

        // A child that lives a moment, then goes without cleaning up.
        const auto child = fork();
        if (child == 0) { usleep (600 * 1000); _exit (0); }

        auto rack = backend->createRackInput();
        auto* j = new juce::DynamicObject();
        j->setProperty ("pid", (int) child);
        j->setProperty ("source", "system");
        j->setProperty ("rackInput", rack.id);
        j->setProperty ("rackInputToken", backend->rackInputToken());
        j->setProperty ("defaultBefore", originalDefault);
        backend->setDefaultOutput (rack.id);

        auto journal = dir.getChildFile ("journal-4.json");
        journal.replaceWithText (juce::JSON::toString (juce::var (j)));

        std::thread reaper ([child] { waitpid (child, nullptr, 0); });
        runWatchdog ((int) child, journal);
        reaper.join();

        check (backend->defaultOutput() == originalDefault, "the watchdog put the default device back");
        check (! journal.existsAsFile(), "the watchdog removed the journal");
        check (sinkGone (*backend, rack.id), "the watchdog removed the rack input");
    }

    // Whatever happened above: leave the machine as it was.
    if (backend->defaultOutput() != originalDefault)
        backend->setDefaultOutput (originalDefault);

    player.kill();
    dir.deleteRecursively();

    std::printf (failures == 0 ? "All router checks passed.\n" : "%d router check(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
