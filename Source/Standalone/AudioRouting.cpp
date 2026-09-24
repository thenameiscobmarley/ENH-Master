#include "AudioRouting.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <cerrno>
 #include <csignal>
 #include <unistd.h>
#endif

namespace pad::routing
{
    namespace
    {
        int currentPid()
        {
           #if JUCE_WINDOWS
            return (int) GetCurrentProcessId();
           #else
            return (int) getpid();
           #endif
        }

        bool isProcessAlive (int pid)
        {
            if (pid <= 0)
                return false;

           #if JUCE_WINDOWS
            auto h = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, (DWORD) pid);
            if (h == nullptr)
                return GetLastError() == ERROR_ACCESS_DENIED;   // there, but not ours to look at

            const bool alive = WaitForSingleObject (h, 0) == WAIT_TIMEOUT;
            CloseHandle (h);
            return alive;
           #else
            return kill ((pid_t) pid, 0) == 0 || errno == EPERM;
           #endif
        }

        juce::File journalFolder()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ENH Master");
        }
    }

    juce::File defaultJournalFile()
    {
        return journalFolder().getChildFile ("routing-" + juce::String (currentPid()) + ".json");
    }

    //==============================================================================
    Router::Router (Backend& backendToUse, juce::File journalFileToUse)
        : backend (backendToUse), journalFile (std::move (journalFileToUse))
    {
    }

    juce::var Router::readJournal (const juce::File& f)
    {
        if (! f.existsAsFile())
            return {};

        auto v = juce::JSON::parse (f.loadFileAsString());
        return v.isObject() ? v : juce::var();
    }

    bool Router::save()
    {
        journalFile.getParentDirectory().createDirectory();

        // Write-then-rename, so a crash mid-write leaves the old journal rather than half a new one.
        auto temp = journalFile.getSiblingFile (journalFile.getFileName() + ".tmp");
        if (! temp.replaceWithText (juce::JSON::toString (journal)) || ! temp.moveFileTo (journalFile))
        {
            error = "Could not write " + journalFile.getFullPathName() + ", so nothing was rerouted.";
            return false;
        }

        return true;
    }

    bool Router::insert (const Plan& plan, const juce::String& rackInputId, const juce::String& rackInputToken)
    {
        error = {};

        if (isInserted())
            return true;

        if (rackInputId.isEmpty() || rackInputId == plan.listenId)
        {
            error = "The rack input and the device you listen on must be two different devices.";
            return false;
        }

        auto* j = new juce::DynamicObject();
        journal = juce::var (j);
        j->setProperty ("version", 1);
        j->setProperty ("pid", currentPid());
        j->setProperty ("source", plan.source == Plan::Source::wholeSystem ? "system" : "apps");
        j->setProperty ("rackInput", rackInputId);
        j->setProperty ("rackInputToken", rackInputToken);
        j->setProperty ("listen", plan.listenId);
        j->setProperty ("appKeys", juce::var (juce::Array<juce::var> (plan.appKeys.begin(), plan.appKeys.size())));
        j->setProperty ("moved", juce::var (juce::Array<juce::var>()));

        if (plan.source == Plan::Source::wholeSystem)
        {
            const auto before = backend.defaultOutput();
            j->setProperty ("defaultBefore", before);

            if (! save())
            {
                journal = juce::var();
                return false;
            }

            if (before != rackInputId && ! backend.setDefaultOutput (rackInputId))
            {
                error = "Could not make the rack input the default device: " + backend.getLastError();
                remove();
                return false;
            }

            return true;
        }

        if (! save())
        {
            journal = juce::var();
            return false;
        }

        followNewStreams();

        if (j->getProperty ("moved").size() == 0)
            error = "None of the chosen apps is playing yet. They will be moved into the rack as soon as they start.";

        return true;
    }

    int Router::followNewStreams()
    {
        if (! isInserted() || journal["source"].toString() != "apps")
            return 0;

        auto* moved = journal.getDynamicObject()->getProperty ("moved").getArray();
        const auto rackInput = journal["rackInput"].toString();
        const auto keys = journal["appKeys"];

        auto alreadyMoved = [&] (int handle)
        {
            for (auto& m : *moved)
                if ((int) m["handle"] == handle)
                    return true;
            return false;
        };

        int count = 0;

        for (auto& app : backend.apps())
        {
            bool chosen = false;
            for (auto& k : *keys.getArray())
                chosen = chosen || k.toString() == app.key;

            if (! chosen)
                continue;

            for (auto handle : app.handles)
            {
                if (alreadyMoved (handle))
                {
                    // Moved, but not there (a move straight after the rack input was made can be lost,
                    // and a program can move its own stream back): move it again. Already written down.
                    if (backend.endpointOfHandle (handle) != rackInput && backend.moveApp (app, handle, rackInput))
                        ++count;
                    continue;
                }

                // Record first, then move: the journal must never be behind what was done.
                auto* m = new juce::DynamicObject();
                m->setProperty ("key", app.key);
                m->setProperty ("name", app.name);
                m->setProperty ("handle", handle);
                m->setProperty ("before", backend.endpointOfHandle (handle));
                moved->add (juce::var (m));

                if (! save())
                    return count;

                if (backend.moveApp (app, handle, rackInput))
                    ++count;
                else
                    error = "Could not move " + app.name + ": " + backend.getLastError();
            }
        }

        return count;
    }

    bool Router::undo (Backend& backend, const juce::var& journal, juce::String& errorOut)
    {
        bool ok = true;

        auto note = [&] (const juce::String& what)
        {
            ok = false;
            errorOut << (errorOut.isEmpty() ? "" : "\n") << what;
        };

        // The default device first, so anything that follows it is back on it before its own
        // streams move.
        if (journal.hasProperty ("defaultBefore"))
        {
            const auto before = journal["defaultBefore"].toString();

            if (before.isNotEmpty() && backend.defaultOutput() != before && ! backend.setDefaultOutput (before))
                note ("Could not set the default device back: " + backend.getLastError());
        }

        if (auto* moved = journal["moved"].getArray())
        {
            // The streams that are gone need nothing doing; the ones still there go back.
            juce::Array<int> live;
            std::vector<App> apps = backend.apps();

            for (auto& a : apps)
                live.addArray (a.handles);

            for (auto& m : *moved)
            {
                const auto handle = (int) m["handle"];

                if (! live.contains (handle))
                    continue;

                for (auto& a : apps)
                    if (a.handles.contains (handle))
                        if (! backend.moveApp (a, handle, m["before"].toString()))
                            note ("Could not move " + m["name"].toString() + " back: " + backend.getLastError());
            }
        }

        const auto token = journal["rackInputToken"].toString();
        if (token.isNotEmpty())
            backend.destroyRackInput (token);

        return ok;
    }

    bool Router::remove()
    {
        error = {};

        if (! isInserted())
            return true;

        juce::String undoError;
        const auto ok = undo (backend, journal, undoError);

        journal = juce::var();
        journalFile.deleteFile();

        if (! ok)
            error = undoError;

        return ok;
    }

    juce::String Router::recoverLeftovers()
    {
        juce::String report;

        for (auto& f : journalFolder().findChildFiles (juce::File::findFiles, false, "routing-*.json"))
        {
            if (f == journalFile)
                continue;

            auto j = readJournal (f);

            if (j.isObject() && isProcessAlive ((int) j["pid"]))
                continue;   // another copy of the app is running with the rack in: leave it alone

            juce::String undoError;
            if (j.isObject())
                undo (backend, j, undoError);

            f.deleteFile();
            report = undoError.isEmpty() ? "The last session did not close cleanly. Its audio routing has been put back."
                                         : "The last session did not close cleanly. Putting its routing back: " + undoError;
        }

        return report;
    }

    //==============================================================================
    void runWatchdog (int pid, const juce::File& journal)
    {
        while (journal.existsAsFile() && isProcessAlive (pid))
            juce::Thread::sleep (400);

        // Still there: the app went without removing the rack. Put the sound back.
        if (auto j = Router::readJournal (journal); j.isObject())
        {
            auto backend = createBackend();
            juce::String ignored;
            Router::undo (*backend, j, ignored);
            journal.deleteFile();
        }
    }

    std::unique_ptr<juce::ChildProcess> startWatchdog (const juce::File& journal)
    {
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

        auto watchdog = std::make_unique<juce::ChildProcess>();
        if (! watchdog->start (juce::StringArray { exe.getFullPathName(), watchdogArgument,
                                                   juce::String (currentPid()), journal.getFullPathName() },
                               0))
            return {};

        return watchdog;
    }
}
