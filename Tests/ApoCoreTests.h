// The system effect's processing core (Source/Apo/ApoCore.h) on Linux: everything the Windows APO
// does to the sound, without COM. Include after DisplayBridgeTests.h (uses bridgetest::expect).
// Needs the engine sources (ENH_DSP_SOURCES) and Source/Apo/ApoCore.cpp. Run: EnhDspTests --bridge

#pragma once

#include <cmath>
#include <memory>
#include <random>
#include <vector>
#include "Apo/ApoCore.h"

namespace apotest
{
    using bridgetest::expect;
    using namespace enh::shared;

    struct Noise
    {
        std::mt19937 rng { 1234 };
        std::uniform_real_distribution<float> d { -1.0f, 1.0f };
        float next() { return d (rng); }
    };

    /** Interleaved programme: noise at about -20 dBFS on the first two channels, others silent. */
    inline void fill (std::vector<float>& buf, int channels, int frames, Noise& n, float gain = 0.1f)
    {
        buf.assign ((size_t) channels * (size_t) frames, 0.0f);
        for (int i = 0; i < frames; ++i)
            for (int c = 0; c < std::min (2, channels); ++c)
                buf[(size_t) i * (size_t) channels + (size_t) c] = n.next() * gain;
    }

    inline bool allFiniteAndUnderFullScale (const std::vector<float>& b)
    {
        for (float x : b)
            if (! std::isfinite (x) || std::abs (x) > 1.0f)
                return false;
        return true;
    }

    inline void runApoCoreTests()
    {
        std::printf ("\nApoCore: the system effect's processing, without Windows\n");
        const double sr = 48000.0;
        const int frames = 480;   // 10 ms, what the Windows audio engine usually asks for

        // Stereo, in place
        {
            auto core = std::make_unique<enh::apo::ApoCore>();
            expect (core->prepare (sr, 2, frames), "prepares at 48 kHz stereo, 480 frames");
            const double ms = 1000.0 * core->getLatencySamples() / sr;
            expect (ms > 5.0 && ms < 20.0, "reports the engine's latency: " + std::to_string (ms) + " ms (" + std::to_string (core->getLatencySamples()) + " samples)");
            Noise n;
            std::vector<float> buf;
            bool ok = true, silentOut = false;
            double energy = 0.0;
            for (int b = 0; b < 300; ++b)   // 3 s
            {
                fill (buf, 2, frames, n);
                silentOut = core->process (buf.data(), buf.data(), (std::uint32_t) frames, false) || silentOut;
                ok = ok && allFiniteAndUnderFullScale (buf);
                if (b > 100) for (float x : buf) energy += (double) x * x;
            }
            expect (ok && ! silentOut && energy > 0.0, "3 s of noise in place: finite, under full scale, not silent");

            // Out of place, odd frame counts, and more than maxFrames in one call (split into chunks)
            std::vector<float> in, out;
            ok = true;
            for (int len : { 1, 7, 441, 480, 1000, 4096 })
            {
                fill (in, 2, len, n);
                out.assign (in.size(), 12345.0f);
                core->process (in.data(), out.data(), (std::uint32_t) len, false);
                ok = ok && allFiniteAndUnderFullScale (out);
            }
            expect (ok, "out of place, 1 / 7 / 441 / 480 / 1000 / 4096 frames per call: every sample written, finite");

            // Silent input: keeps running through the tail, then goes idle (BUFFER_SILENT, no processing)
            int callsToIdle = -1;
            for (int b = 0; b < 1000 && callsToIdle < 0; ++b)
                if (core->process (buf.data(), buf.data(), (std::uint32_t) frames, true))
                    callsToIdle = b;
            expect (callsToIdle >= 200 && callsToIdle < 1000, "silent input: goes idle after the tail (" + std::to_string (callsToIdle * 10) + " ms)");
            fill (buf, 2, frames, n);
            expect (! core->process (buf.data(), buf.data(), (std::uint32_t) frames, false) && ! core->isIdle(), "sound again: processing resumes at once");
        }

        // 5.1: channels 3..6 pass through a delay of exactly the latency; 1..2 are ENH Master's
        {
            auto core = std::make_unique<enh::apo::ApoCore>();
            core->prepare (sr, 6, frames);
            const int lat = core->getLatencySamples();
            std::vector<float> buf ((size_t) 6 * frames, 0.0f);
            buf[4] = 0.5f;   // frame 0, channel 5: an impulse
            int foundAt = -1;
            for (int b = 0; b < 10 && foundAt < 0; ++b)
            {
                core->process (buf.data(), buf.data(), (std::uint32_t) frames, false);
                for (int i = 0; i < frames; ++i)
                    if (buf[(size_t) i * 6 + 4] == 0.5f) foundAt = b * frames + i;
                std::fill (buf.begin(), buf.end(), 0.0f);
            }
            expect (foundAt == lat, "5.1: the other channels are delayed by exactly the latency (" + std::to_string (foundAt) + " = " + std::to_string (lat) + ")");
        }

        // Mono endpoint
        {
            auto core = std::make_unique<enh::apo::ApoCore>();
            expect (core->prepare (44100.0, 1, 441), "prepares mono at 44.1 kHz");
            Noise n;
            std::vector<float> buf;
            bool ok = true;
            for (int b = 0; b < 100; ++b) { fill (buf, 1, 441, n); core->process (buf.data(), buf.data(), 441, false); ok = ok && allFiniteAndUnderFullScale (buf); }
            expect (ok, "mono: finite, under full scale");
        }

        // Not prepared (e.g. allocation failed): the input passes unchanged
        {
            auto core = std::make_unique<enh::apo::ApoCore>();
            expect (! core->prepare (1.0, 2, frames), "an impossible format is refused (stays in pass-through)");
            std::vector<float> in ((size_t) 2 * frames), out ((size_t) 2 * frames, 0.0f);
            Noise n; for (auto& x : in) x = n.next();
            core->process (in.data(), out.data(), (std::uint32_t) frames, false);
            expect (in == out, "pass-through is bit-exact");
        }

        // With the display: parameters in, meters and scopes out, over a DisplayBridge
        {
            const auto key = bridgetest::uniqueKey ("apo");
            DisplayBridge::unlinkName (key);
            auto core = std::make_unique<enh::apo::ApoCore>();
            core->prepare (sr, 2, frames);
            core->openBridge (key, DisplayBridge::Scope::session);
            expect (core->isBridgeOpen(), "the core creates its bridge");

            DisplayBridge ui;
            ui.open (key, true);
            const auto& table = rackParams();
            ParamSnapshot p;
            p.layoutHash = table.layoutHash;
            p.count = (std::uint32_t) table.count;
            defaultParamValues (table, p.values.data());
            defaultModifiers (p.modifiers.data());
            int levelIndex = -1;
            for (int i = 0; i < table.count; ++i) if (table.defs[(size_t) i].id == "levelGain") levelIndex = i;
            p.values[(size_t) levelIndex] = -12.0f;
            ui.publishParams (p);
            ui.beat (DisplayBridge::Role::ui);

            Noise n;
            std::vector<float> buf;
            for (int b = 0; b < 100; ++b) { fill (buf, 2, frames, n); core->process (buf.data(), buf.data(), (std::uint32_t) frames, false); }
            expect (core->isUsingUi(), "takes the UI's parameters");
            expect (std::abs (core->getMeters().levelDb.load() + 12.0f) < 0.1f, "LEVEL from the UI reaches the engine (" + std::to_string (core->getMeters().levelDb.load()) + " dB)");

            MeterBlock m; std::uint32_t ms = 0;
            expect (ui.readMeters (m, ms) == SeqBlock<MeterBlock>::Read::fresh && (m.status.flags & statusRunning) && (m.status.flags & statusUsingUi)
                     && m.status.latencySamples == core->getLatencySamples() && std::abs (m.meters.levelDb + 12.0f) < 0.1f,
                    "the UI reads the effect's meters and status");
            expect (ui.isAlive (DisplayBridge::Role::audio, 1000), "the UI sees the effect's heartbeat");
            std::uint32_t ri = 0; int got = 0;
            got = ui.readScope (scopeOutput, ri, [] (const float*, int) {});
            expect (got > 1000, "the UI gets the output scope (" + std::to_string (got) + " samples)");

            // Hostile values from shared memory
            ParamSnapshot bad = p;
            for (int i = 0; i < table.count; ++i) bad.values[(size_t) i] = (i & 1) ? std::numeric_limits<float>::quiet_NaN() : 1.0e30f;
            for (auto& x : bad.modifiers) x = -1.0e30f;
            ui.publishParams (bad);
            bool ok = true;
            for (int b = 0; b < 200; ++b) { fill (buf, 2, frames, n); core->process (buf.data(), buf.data(), (std::uint32_t) frames, false); ok = ok && allFiniteAndUnderFullScale (buf); }
            expect (ok, "NaN / 1e30 parameters and modifiers from the bridge: output stays finite and under full scale");

            // Another app build (different parameter layout): ignored
            ParamSnapshot other = p;
            other.layoutHash ^= 1u;
            other.values[(size_t) levelIndex] = 6.0f;
            ui.publishParams (p);   // back to -12 first
            for (int b = 0; b < 20; ++b) core->process (buf.data(), buf.data(), (std::uint32_t) frames, false);
            ui.publishParams (other);
            for (int b = 0; b < 100; ++b) { fill (buf, 2, frames, n); core->process (buf.data(), buf.data(), (std::uint32_t) frames, false); }
            expect (std::abs (core->getMeters().levelDb.load() + 12.0f) < 0.1f, "a snapshot with another layout hash is ignored");

            ui.close();
            core->closeBridge();
            DisplayBridge::unlinkName (key);
        }
    }
}
