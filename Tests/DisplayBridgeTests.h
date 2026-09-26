// DisplayBridge tests (Source/Shared/DisplayBridge.h): the shared-memory block between the audio side
// (system effect / a plugin instance getting audio) and the UI side. Linux (and any POSIX system).
//
// Self-contained and JUCE-free: runDisplayBridgeTests() counts its own failures in
// bridgetest::failures. In EnhDspTests (--bridge) add them to the tool's count. Standalone:
//     main.cpp:  #include "../Tests/DisplayBridgeTests.h"
//                int main() { runDisplayBridgeTests(); return bridgetest::failures ? 1 : 0; }
//     g++ -std=c++17 -O2 -pthread -I Source main.cpp Source/Shared/DisplayBridge.cpp
//
// When included after the JUCE parameter headers (EnhDspTests: JUCE_AUDIO_PROCESSORS_H_INCLUDED), it
// also checks Shared/RackParamTable.h against pad::params::allSpecs() and the knob modifiers.

#pragma once

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#if ! defined (_WIN32)
 #include <sys/wait.h>
 #include <unistd.h>
#endif
#include "Shared/DisplayBridge.h"

namespace bridgetest
{
    using namespace enh::shared;

    inline int failures = 0;

    inline void expect (bool ok, const std::string& what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (! ok) ++failures;
    }

    inline std::string uniqueKey (const char* what)
    {
       #if defined (_WIN32)
        return std::string ("test-") + what;
       #else
        return std::string ("test-") + what + "-" + std::to_string ((long) getpid());
       #endif
    }

    /** Every field = v (so a torn copy shows as a mix of values). */
    inline ParamSnapshot patternParams (std::uint32_t v)
    {
        ParamSnapshot p;
        p.layoutHash = v;
        p.count = v;
        p.loudnessResets = v;
        p.flags = v;
        for (auto& x : p.values) x = (float) v;
        for (auto& x : p.modifiers) x = (float) v;
        return p;
    }

    inline bool isPattern (const ParamSnapshot& p)
    {
        const std::uint32_t v = p.layoutHash;
        if (p.count != v || p.loudnessResets != v || p.flags != v) return false;
        for (auto x : p.values) if (x != (float) v) return false;
        for (auto x : p.modifiers) if (x != (float) v) return false;
        return true;
    }

    inline MeterBlock patternMeters (std::uint32_t v)
    {
        MeterBlock m;
        m.status.processCalls = v;
        m.status.sampleRate = (float) v;
        m.meters.radarStepsTotal = (std::int32_t) v;
        m.meters.integratedLufs = (float) v;
        m.meters.radarOn = v & 1u;
        for (auto& x : m.meters.bandGainDb) x = (float) v;
        for (auto& x : m.meters.radarStepBoost) x = (float) v;
        for (auto& x : m.meters.radarStepTrack) x = (std::int32_t) v;
        return m;
    }

    inline bool isPattern (const MeterBlock& m)
    {
        const std::uint32_t v = m.status.processCalls;
        if (m.status.sampleRate != (float) v || m.meters.radarStepsTotal != (std::int32_t) v
             || m.meters.integratedLufs != (float) v || m.meters.radarOn != (v & 1u)) return false;
        for (auto x : m.meters.bandGainDb) if (x != (float) v) return false;
        for (auto x : m.meters.radarStepBoost) if (x != (float) v) return false;
        for (auto x : m.meters.radarStepTrack) if (x != (std::int32_t) v) return false;
        return true;
    }

    using PRead = SeqBlock<ParamSnapshot>::Read;
    using MRead = SeqBlock<MeterBlock>::Read;

    //==============================================================================================
    inline void testTwoMappings()
    {
        std::printf ("\nDisplayBridge: two mappings of one name in one process\n");
        const auto key = uniqueKey ("two");
        DisplayBridge::unlinkName (key);

        DisplayBridge ui, audio;
        DisplayBridge probe;
        expect (probe.open (key, false) == DisplayBridge::OpenResult::notFound, "opening a missing block without create: notFound");
        expect (ui.open (key, true) == DisplayBridge::OpenResult::ok && ui.createdIt(), "the first side creates it");
        expect (audio.open (key, true) == DisplayBridge::OpenResult::ok && ! audio.createdIt(), "the second side opens the same block");

        ParamSnapshot got;
        std::uint32_t seq = 0;
        expect (audio.readParams (got, seq) == PRead::empty, "nothing published yet: empty");

        auto p = patternParams (7);
        p.values[3] = -12.5f;
        expect (ui.publishParams (p), "UI publishes parameters");
        expect (audio.readParams (got, seq) == PRead::fresh && got.values[3] == -12.5f && got.count == 7, "audio side reads them");
        expect (audio.readParams (got, seq) == PRead::unchanged, "the same version again: unchanged");

        MeterBlock mb = patternMeters (41), mgot;
        std::uint32_t mseq = 0;
        expect (audio.publishMeters (mb), "audio side publishes meters");
        expect (ui.readMeters (mgot, mseq) == MRead::fresh && isPattern (mgot) && mgot.status.processCalls == 41, "UI reads every meter field");

        // Scope ring: the newest samples, in order, across the wrap
        std::vector<float> ramp (5000);
        for (size_t i = 0; i < ramp.size(); ++i) ramp[i] = (float) i;
        std::uint32_t readIndex = 0;
        audio.pushScope (scopeOutput, ramp.data(), 3000);
        std::vector<float> seen;
        auto sink = [&seen] (const float* s, int n) { seen.insert (seen.end(), s, s + n); };
        ui.readScope (scopeOutput, readIndex, sink);
        bool inOrder = seen.size() == 3000;
        for (size_t i = 0; inOrder && i < seen.size(); ++i) inOrder = seen[i] == (float) i;
        expect (inOrder, "scope: 3000 samples arrive in order");
        audio.pushScope (scopeOutput, ramp.data() + 3000, 2000);
        audio.pushScope (scopeOutput, ramp.data(), 5000);   // 7000 behind the reader: only the newest 4096 are kept
        seen.clear();
        ui.readScope (scopeOutput, readIndex, sink);
        expect (seen.size() == (size_t) kScopeSize && seen.back() == 4999.0f && seen.front() == (float) (5000 - kScopeSize),
                "scope: a reader that fell behind gets the newest 4096, in order");

        ui.close(); audio.close();
        DisplayBridge::unlinkName (key);
        expect (true, "block size " + std::to_string (sizeof (BridgeLayout)) + " bytes");
    }

    //==============================================================================================
    inline void testVersionMismatch()
    {
        std::printf ("\nDisplayBridge: version mismatch is refused\n");
        const auto key = uniqueKey ("ver");
        DisplayBridge::unlinkName (key);
        DisplayBridge a, b;
        expect (a.open (key, true) == DisplayBridge::OpenResult::ok, "created");
        a.overwriteVersionForTest (kBridgeVersion + 1);
        expect (b.open (key, true) == DisplayBridge::OpenResult::versionMismatch && ! b.isOpen(), "a block of another version: versionMismatch, not opened");
        a.overwriteVersionForTest (kBridgeVersion);
        expect (b.open (key, false) == DisplayBridge::OpenResult::ok, "the right version again: opens");
        expect (a.open ("bad/name", true) == DisplayBridge::OpenResult::badName, "a key with '/' is refused");
        expect (! DisplayBridge::isValidKey (std::string (41, 'a')) && DisplayBridge::isValidKey ("system"), "key length / characters");
        a.close(); b.close();
        DisplayBridge::unlinkName (key);
    }

    //==============================================================================================
    inline void testHeartbeat()
    {
        std::printf ("\nDisplayBridge: heartbeats\n");
        const auto key = uniqueKey ("beat");
        DisplayBridge::unlinkName (key);
        DisplayBridge a, b;
        a.open (key, true);
        b.open (key, true);
        using Role = DisplayBridge::Role;

        expect (! b.isAlive (Role::audio, 1000, 5000), "no beat yet: not alive");
        a.beat (Role::audio, 5000);
        expect (b.isAlive (Role::audio, 1000, 5500), "beat 500 ms ago: alive (1 s timeout)");
        expect (b.isAlive (Role::audio, 1000, 6000), "beat exactly 1 s ago: alive");
        expect (! b.isAlive (Role::audio, 1000, 6001), "beat 1001 ms ago: timed out");
        expect (! b.isAlive (Role::ui, 1000, 5500), "the other role's beat is separate");
        a.beat (Role::audio, 0xfffffe00u);   // the 32-bit clock about to wrap
        expect (b.isAlive (Role::audio, 1000, 0x00000100u), "wrap-safe: alive across the 32-bit wrap");
        expect (! b.isAlive (Role::audio, 1000, 0x00000500u), "wrap-safe: times out across the wrap");
        expect (b.beatCount (Role::audio) == 2, "beat counter counts");

        // The real clock
        a.beat (Role::ui);
        expect (b.isAlive (Role::ui, 200), "real clock: alive right after a beat");
        std::this_thread::sleep_for (std::chrono::milliseconds (250));
        expect (! b.isAlive (Role::ui, 200), "real clock: timed out after 250 ms (200 ms timeout)");
        a.close(); b.close();
        DisplayBridge::unlinkName (key);
    }

    //==============================================================================================
    inline void testConcurrent()
    {
        std::printf ("\nDisplayBridge: seqlock under concurrent writes\n");
        const auto key = uniqueKey ("race");
        DisplayBridge::unlinkName (key);
        DisplayBridge w1, w2, r;
        w1.open (key, true); w2.open (key, true); r.open (key, true);

        std::atomic<bool> stop { false };
        std::atomic<long> writes { 0 }, skipped { 0 };
        auto writer = [&] (DisplayBridge& b, std::uint32_t base)
        {
            for (std::uint32_t v = base; ! stop.load (std::memory_order_relaxed); v += 2)
            {
                const bool okP = b.publishParams (patternParams (v));
                const bool okM = b.publishMeters (patternMeters (v));
                (okP ? writes : skipped).fetch_add (1);
                (okM ? writes : skipped).fetch_add (1);
            }
        };
        std::thread t1 (writer, std::ref (w1), 1u), t2 (writer, std::ref (w2), 2u);   // two writers: the CAS keeps them apart

        long fresh = 0, busy = 0, torn = 0, boundedCalls = 0, unboundedOk = 0;
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds (1500);
        while (std::chrono::steady_clock::now() < until)
        {
            ParamSnapshot p; std::uint32_t ps = 0;
            // The audio thread's way: at most one retry
            const auto res = r.readParams (p, ps, 2);
            ++boundedCalls;
            if (res == PRead::fresh) { ++fresh; if (! isPattern (p)) ++torn; }
            else if (res == PRead::busy) ++busy;

            MeterBlock m; std::uint32_t ms = 0;
            if (r.readMeters (m, ms, 1000) == MRead::fresh) { ++unboundedOk; if (! isPattern (m)) ++torn; }
        }
        stop = true;
        t1.join(); t2.join();

        expect (torn == 0, "no torn snapshot accepted (" + std::to_string (fresh + unboundedOk) + " reads, "
                             + std::to_string (writes.load()) + " writes, " + std::to_string (skipped.load()) + " skipped by the CAS)");
        expect (fresh > 0 && unboundedOk > 0, "readers do get through while writers hammer the block");
        expect (true, "bounded (audio) reads: " + std::to_string (boundedCalls) + " calls, " + std::to_string (busy)
                        + " gave up after one retry (the caller keeps its last values)");

        // At a realistic rate (the UI publishes ~30 times a second, the audio side reads every ~10 ms;
        // here 1000 writes/s against a read every 0.2 ms) the bounded read almost always gets through
        {
            DisplayBridge w, rd;
            w.open (key, true); rd.open (key, true);
            std::atomic<bool> done { false };
            std::thread t ([&] { for (std::uint32_t v = 1; ! done; ++v) { w.publishParams (patternParams (v)); std::this_thread::sleep_for (std::chrono::milliseconds (1)); } });
            long calls = 0, gaveUp = 0, bad = 0;
            ParamSnapshot p; std::uint32_t s = 0;
            for (int i = 0; i < 3000; ++i)
            {
                const auto res = rd.readParams (p, s, 2);
                ++calls;
                if (res == PRead::busy) ++gaveUp;
                if (res == PRead::fresh && ! isPattern (p)) ++bad;
                std::this_thread::sleep_for (std::chrono::microseconds (200));
            }
            done = true;
            t.join();
            expect (bad == 0 && gaveUp * 20 < calls, "realistic rate: " + std::to_string (gaveUp) + " of " + std::to_string (calls)
                                                    + " bounded reads gave up (must be < 5 %), none torn");
        }

        // A writer frozen mid-write (odd sequence, e.g. a process killed inside write()): other writers
        // skip instead of waiting, and a bounded reader gives up after its attempts instead of spinning
        {
            auto blk = std::make_unique<SeqBlock<ParamSnapshot>>();
            blk->seq.store (0);
            for (auto& w : blk->words) w.store (0);
            expect (blk->write (patternParams (5)), "a free block takes a write");
            blk->seq.fetch_add (1);   // freeze it "mid-write"
            ParamSnapshot p; std::uint32_t s = 0;
            const auto t0 = std::chrono::steady_clock::now();
            const bool skipped = ! blk->write (patternParams (6));
            const auto res = blk->read (p, s, 2);
            const auto us = std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::steady_clock::now() - t0).count();
            expect (skipped && res == PRead::busy && us < 1000,
                    "block stuck mid-write: the writer skips, the audio-side read returns busy at once (" + std::to_string (us) + " us)");
        }
        w1.close(); w2.close(); r.close();
        DisplayBridge::unlinkName (key);
    }

    //==============================================================================================
   #if ! defined (_WIN32)
    inline void testForkedChild()
    {
        std::printf ("\nDisplayBridge: another process (fork)\n");
        const auto key = uniqueKey ("fork");
        DisplayBridge::unlinkName (key);
        std::fflush (stdout);

        const pid_t child = fork();
        if (child == 0)
        {
            // Child = the audio side: creates the block (or opens it), publishes meters and scope,
            // beats, then waits for parameters from the parent and checks them
            DisplayBridge b;
            int tries = 0;
            while (b.open (key, true) != DisplayBridge::OpenResult::ok && tries++ < 100)
                std::this_thread::sleep_for (std::chrono::milliseconds (5));
            if (! b.isOpen()) _exit (10);
            b.publishMeters (patternMeters (1234));
            float s[64];
            for (int i = 0; i < 64; ++i) s[i] = 0.5f + (float) i;
            b.pushScope (scopeInput, s, 64);
            b.beat (DisplayBridge::Role::audio);

            ParamSnapshot p; std::uint32_t seq = 0;
            for (int i = 0; i < 2000; ++i)
            {
                b.beat (DisplayBridge::Role::audio);
                if (b.readParams (p, seq) == PRead::fresh)
                    _exit (isPattern (p) && p.count == 99 ? 0 : 11);
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
            _exit (12);
        }

        DisplayBridge b;
        int tries = 0;
        while (b.open (key, true) != DisplayBridge::OpenResult::ok && tries++ < 100)
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        expect (b.isOpen(), "parent opens the block");

        // Wait for the child's meters
        MeterBlock m; std::uint32_t ms = 0;
        bool gotMeters = false;
        for (int i = 0; i < 2000 && ! gotMeters; ++i)
        {
            gotMeters = b.readMeters (m, ms) == MRead::fresh;
            if (! gotMeters) std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        expect (gotMeters && isPattern (m) && m.status.processCalls == 1234, "parent reads the child's meters");
        expect (b.isAlive (DisplayBridge::Role::audio, 1000), "parent sees the child's heartbeat");
        std::uint32_t ri = 0; float first = -1.0f; int n = 0;
        n = b.readScope (scopeInput, ri, [&first] (const float* s, int k) { if (first < 0.0f && k > 0) first = s[0]; });
        expect (n == 64 && first == 0.5f, "parent reads the child's scope samples");

        expect (b.publishParams (patternParams (99)), "parent publishes parameters");
        int status = 0;
        waitpid (child, &status, 0);
        expect (WIFEXITED (status) && WEXITSTATUS (status) == 0,
                "child read the parent's parameters intact (child exit " + std::to_string (WIFEXITED (status) ? WEXITSTATUS (status) : -1) + ")");

        // After the child has gone, its heartbeat times out
        std::this_thread::sleep_for (std::chrono::milliseconds (300));
        expect (! b.isAlive (DisplayBridge::Role::audio, 250), "the child's heartbeat times out after it exits");
        b.close();
        DisplayBridge::unlinkName (key);
    }
   #endif

    //==============================================================================================
    inline void testMeterRoundTrip()
    {
        std::printf ("\nMeterSnapshot: EngineMeters -> snapshot -> EngineMeters\n");
        auto em = std::make_unique<enh::dsp::EngineMeters>();
        em->bandGainDb[5] = 3.25f;
        em->limitShape[2] = 2;
        em->radarTrackId[3] = -7;
        em->comparing = true;
        em->protectionTripped = true;
        em->radarStepsTotal = 321;
        em->radarStepBoost[31] = 9.5f;
        em->radarOn = true;
        em->silkDipDb[27] = -4.0f;
        MeterSnapshot s;
        captureMeters (*em, s);
        auto back = std::make_unique<enh::dsp::EngineMeters>();
        applyMeters (s, *back);
        expect (back->bandGainDb[5] == 3.25f && back->limitShape[2] == 2 && back->radarTrackId[3] == -7
                 && back->comparing && back->protectionTripped && back->radarStepsTotal == 321
                 && back->radarStepBoost[31] == 9.5f && back->radarOn && back->silkDipDb[27] == -4.0f,
                "floats, ints and bools survive the round trip (first, middle and last fields)");
        expect (sizeof (MeterSnapshot) % 4 == 0, "snapshot is whole 32-bit words (" + std::to_string (sizeof (MeterSnapshot)) + " bytes)");
    }
}

#if defined (JUCE_AUDIO_PROCESSORS_H_INCLUDED)
 #include "Shared/RackParamTable.h"
 #include "Parameters/ParameterSpecs.h"
 #include "Parameters/KnobModifiers.h"

namespace bridgetest
{
    /** The JUCE-free parameter table the system effect uses must be pad::params::allSpecs(), in the
        same order, with the same ranges and defaults; its knob modifiers must match the plugin's. */
    inline void testRackParamTable()
    {
        std::printf ("\nRackParamTable: the system effect's parameter table vs ParameterSpecs\n");
        const auto& table = enh::shared::rackParams();
        const auto& specs = pad::params::allSpecs();
        expect ((int) specs.size() == table.count, "same number of parameters (" + std::to_string (specs.size()) + ")");
        int wrong = 0;
        std::uint32_t h = enh::shared::kFnvSeed;
        for (int i = 0; i < (int) specs.size() && i < table.count; ++i)
        {
            const auto& s = specs[(size_t) i];
            const auto& d = table.defs[(size_t) i];
            const bool same = s.id == juce::String (d.id.data(), d.id.size()) && s.minValue == d.minValue && s.maxValue == d.maxValue
                              && s.defaultValue == d.defaultValue && s.skewCentre == d.skewCentre
                              && (int) s.kind == (int) d.kind;
            if (! same && wrong++ < 5)
                std::printf ("    index %d: spec %s, table %.*s\n", i, s.id.toRawUTF8(), (int) d.id.size(), d.id.data());
            const auto id = s.id.toStdString();
            h = enh::shared::hashParam (h, id.c_str(), id.size(), s.minValue, s.maxValue);
        }
        expect (wrong == 0, "every parameter: same ID, order, range, default, skew and kind");
        expect (h == table.layoutHash, "layout hash from the specs == the table's");

        // Modifiers: the effect's JUCE-free copy against pad::applyKnobModifiers, every knob, every setting
        int modWrong = 0, cases = 0;
        for (int knob = 0; knob < pad::KnobModifiers::numKnobs; ++knob)
        {
            const auto* spec = pad::params::findSpec (enh::dsp::knobFields[(size_t) knob].param);
            if (spec == nullptr) { ++modWrong; continue; }
            juce::NormalisableRange<float> range (spec->minValue, spec->maxValue);
            if (spec->skewCentre > 0.0f) range.setSkewForCentre (spec->skewCentre);
            for (int curve = 0; curve < 4; ++curve)
                for (float lim : { 100.0f, 75.0f, 50.0f, 25.0f })
                    for (float smo : { 0.0f, 50.0f })
                        for (float t : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
                        {
                            const float v = range.convertFrom0to1 (t);
                            pad::KnobSmoother js; enh::shared::ModifierSmoother ms;
                            float a = 0, b = 0;
                            for (int blk = 0; blk < 3; ++blk)   // a few blocks, so SMO glides
                            {
                                const float target = blk == 0 ? v : range.convertFrom0to1 (1.0f - t);
                                a = pad::applyKnobModifiers (target, range, js, smo, curve, lim, 480, 48000.0);
                                b = enh::shared::applyKnobModifier (table, knob, target, ms, smo, curve, lim, 480, 48000.0);
                            }
                            ++cases;
                            if (std::abs (a - b) > 1.0e-4f * std::max (1.0f, std::abs (a)) && modWrong++ < 5)
                                std::printf ("    knob %s curve %d lim %.0f smo %.0f t %.2f: plugin %f, effect %f\n",
                                             enh::dsp::knobFields[(size_t) knob].param, curve, lim, smo, t, a, b);
                        }
        }
        expect (modWrong == 0, "knob modifiers: the effect's copy matches the plugin's (" + std::to_string (cases) + " cases)");

        // Defaults: the table's defaults map to the same engine settings as a fresh plugin (KnobValues{})
        std::array<float, enh::shared::kMaxParams> values {};
        for (int i = 0; i < table.count; ++i) values[(size_t) i] = table.defs[(size_t) i].defaultValue;
        enh::dsp::KnobValues fromTable;
        enh::shared::toKnobValues (table, values.data(), table.count, fromTable);
        const auto a = enh::dsp::mapKnobs (fromTable), b = enh::dsp::mapKnobs (enh::dsp::KnobValues {});
        expect (a.normalize == b.normalize && a.levelDb == b.levelDb && a.seraph.mode == b.seraph.mode
                 && a.tide.mix == b.tide.mix && a.lumen.targetDb == b.lumen.targetDb && a.limiter.releaseMs == b.limiter.releaseMs
                 && a.seraph.halo.decayS == b.seraph.halo.decayS && a.methods == b.methods && a.character.modelB == b.character.modelB,
                "the defaults give the engine the same settings as a fresh plugin");

        // Hostile values: NaN, infinities and out-of-range numbers are clamped, methods stay in their list
        for (int i = 0; i < table.count; ++i) values[(size_t) i] = (i % 3 == 0) ? std::numeric_limits<float>::quiet_NaN()
                                                                   : (i % 3 == 1) ? 1.0e30f : -1.0e30f;
        enh::dsp::KnobValues hostile;
        enh::shared::toKnobValues (table, values.data(), table.count, hostile);
        bool methodsOk = true;
        for (int unit : enh::dsp::methods::unitsInRackOrder)
        {
            const auto list = enh::dsp::methods::stagesForUnit (unit);
            for (int s = 0; s < list.count; ++s)
                if (list.stages[s].id >= 0)
                    methodsOk = methodsOk && hostile.methods[(size_t) list.stages[s].id] >= 0
                                          && hostile.methods[(size_t) list.stages[s].id] < list.stages[s].numMethods;
        }
        expect (methodsOk && std::isfinite (hostile.levelDb) && hostile.levelDb <= 12.0f && hostile.levelDb >= -24.0f
                 && std::isfinite (hostile.clarityNorm) && hostile.clarityNorm <= 30.0f,
                "hostile values from shared memory are clamped (methods stay in their lists)");
    }
}
#endif

static void runDisplayBridgeTests()
{
    bridgetest::testMeterRoundTrip();
    bridgetest::testTwoMappings();
    bridgetest::testVersionMismatch();
    bridgetest::testHeartbeat();
    bridgetest::testConcurrent();
   #if ! defined (_WIN32)
    bridgetest::testForkedChild();
   #endif
   #if defined (JUCE_AUDIO_PROCESSORS_H_INCLUDED)
    bridgetest::testRackParamTable();
   #endif
    std::printf ("\nDisplayBridge: %s (%d failure%s)\n", bridgetest::failures == 0 ? "ALL PASSED" : "FAILURES",
                 bridgetest::failures, bridgetest::failures == 1 ? "" : "s");
}
