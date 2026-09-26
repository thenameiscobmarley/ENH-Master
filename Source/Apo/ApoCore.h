#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../DSP/EnhEngine.h"
#include "../Shared/DisplayBridge.h"
#include "../Shared/RackParamTable.h"

/*  The system effect's processing, without any Windows / COM code: interleaved float audio in and out,
    the engine, the parameters from the DisplayBridge (or the saved file, or the defaults), the meters
    and scopes back out. The COM object (EnhApo.cpp) is a thin shell around this, so the part that
    touches the sound can be built and tested on Linux (Tests/ApoCoreTests.h, EnhApoCoreCheck).

    Threads:
      prepare / release / openBridge / loadSettings  - the audio engine's configuration thread
                                                       (LockForProcess / UnlockForProcess). Allocate here.
      process                                        - the real-time thread (APOProcess). Never allocates,
                                                       locks, logs, calls the OS beyond a clock read, or
                                                       throws. Any failure: the input passes unchanged.

    Channels: the first two are ENH Master's (stereo, or mono for a mono endpoint). Any others (5.1,
    7.1 ...) pass through a plain delay of the engine's latency, so every channel stays in time.

    Defaults with no app running: the saved settings (ParamSnapshotFile.h) if any, else every parameter
    at its default - the DEFAULT preset. EAR GUARD and the output limiter are part of the engine and are
    always on whatever the parameters say. */

namespace enh::apo
{
    class ApoCore
    {
    public:
        ApoCore() noexcept;
        ~ApoCore() = default;

        /** Configuration thread. False (and pass-through) if anything can't be allocated. */
        bool prepare (double sampleRate, int numChannels, int maxFrames) noexcept;
        void release() noexcept;
        bool isPrepared() const noexcept { return prepared; }

        /** Configuration thread. key "system", Scope::machine on Windows. Harmless if it fails. */
        void openBridge (const std::string& key, enh::shared::DisplayBridge::Scope scope) noexcept;
        void closeBridge() noexcept { bridge.close(); }
        bool isBridgeOpen() const noexcept { return bridge.isOpen(); }

        /** Configuration thread: settings saved by the app (validated; ignored if from another layout). */
        void useSettings (const enh::shared::ParamSnapshot& saved) noexcept;

        int getLatencySamples() const noexcept { return prepared ? latency : 0; }

        /** Real-time. `in` and `out` may be the same buffer (in-place). `inputSilent`: the host marked
            the input silent (its contents are then not read). Returns true when the output is silent and
            was not written (the caller then marks it BUFFER_SILENT). */
        bool process (const float* in, float* out, std::uint32_t frames, bool inputSilent) noexcept;

        /** For tests / status. */
        std::uint32_t processCalls() const noexcept { return calls; }
        bool isIdle() const noexcept { return idle; }
        bool isUsingUi() const noexcept { return usingUi; }
        const enh::dsp::EngineMeters& getMeters() const noexcept { return engine->getMeters(); }

    private:
        void readParameters() noexcept;
        void publish() noexcept;
        void passThrough (const float* in, float* out, std::uint32_t frames, bool inputSilent) noexcept;

        const enh::shared::RackParamTable& table;
        std::unique_ptr<enh::dsp::EnhEngine> engine;
        enh::shared::DisplayBridge bridge;

        // Parameters now (plain values, table order) and the knob modifiers
        std::array<float, enh::shared::kMaxParams> values {};
        std::array<float, enh::shared::kMaxModifierKnobs * enh::shared::kModifierKinds> modifiers {};
        std::array<enh::shared::ModifierSmoother, enh::shared::kMaxModifierKnobs> smoothers {};
        enh::shared::ParamSnapshot incoming {};
        std::uint32_t paramSeq = 0, loudnessResets = 0;
        bool haveResetCount = false, usingUi = false;

        // Audio
        double rate = 48000.0;
        int channels = 0, engineChannels = 0, maxBlock = 0, latency = 0;
        bool prepared = false;
        std::array<std::vector<float>, 2> planar;
        juce::AudioBuffer<float> view;
        std::vector<float> extraDelay;    // (channels - engineChannels) * latency, per channel one ring
        int extraPos = 0;

        // Silence: after 2 s of silent input and an output under -120 dBFS, stop processing (the host
        // gets BUFFER_SILENT) until sound comes back
        std::int64_t silentInputFrames = 0, quietOutputFrames = 0;
        bool idle = false;

        // Publishing
        std::array<std::uint32_t, enh::shared::kNumScopes> scopeRead {};
        std::uint32_t calls = 0;
        std::int64_t framesSincePublish = 0;
        enh::shared::MeterBlock outgoing {};
        bool failsafe = false;
    };
}
