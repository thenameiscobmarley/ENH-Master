#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "BandAnalyzer.h"
#include "TunerMatch.h"
#include "PrecisionEQ.h"
#include "HarmonicPlanner.h"
#include "FootstepRadar.h"
#include "AdaptiveEQ.h"
#include "SubEnhancer.h"
#include "AnalogStage.h"
#include "Seraph.h"
#include "DynamicCompressor.h"
#include "MethodRegistry.h"
#include "DeepSub.h"
#include "SpectralLeveler.h"
#include "SpectralLimiter.h"
#include "SpectrumScope.h"
#include "FinalLimiter.h"
#include "EarGuard.h"
#include <array>
#include "LoudnessTarget.h"
#include "Character.h"
#include "Lunchbox.h"
#include "OutputStage.h"
#include "HeadphoneEQ.h"
#include "HeadphoneCare.h"
#include "ProX4.h"
#include "Velvetizer.h"
#include "Takeback.h"
#include "units/Units.h"
#include "units/CustomUnit.h"
#include "units/Lb500.h"
#include "LoudnessMeter.h"
#include "MixBalancer.h"
#include "EngineMeters.h"

namespace enh::dsp
{
    /** ENH Master signal chain.

        LEVEL (the rack's working level: everything after it hears it, the leveler's target moves with it)
        input ─► analysis: 24 bands + long-term spectrum, FFT tonality,      [feed-forward]
          │                 harmonic planner, sub follower
          │
          └─► source-dependent EQ ─► sub enhancer ─► analog stage ─► UPWARD LEVELER ─► DEEP SUB
                                                     (auto gain, 2x adaptive
                                                      depth/clarity exciters,
                                                      colour, ceiling)
              ─► SPECTRAL LIMITER ─► MIX BALANCER ─► ADAPTIVE COMPRESSOR ─► FOOTSTEP RADAR ─► TONE & SPACE
              ─► CHARACTER ─► LOUDNESS TARGET ─► COMPARE ─► protection ─► output limiter ─► out (loudness meter)

        Control updates run at ~1.5 kHz; audio runs through IIR filters only, so the added
        latency is just the oversampling filters (reported to the host).
        Real-time safe: no allocation or locking in process().
    */
    class EnhEngine
    {
    public:
        struct Parameters
        {
            float normalize = 0.5f;   // 0..1 CLARITY correction strength (both modes)
            float boost = 0.0f;       // 0..1 CLARITY enhancement (ADD mode only)
            float adaptSpeed = 0.4f;  // 0..1
            float sub = 0.0f;         // 0..1
            bool subBoost = false;
            bool footstep = false;                              // (the FOOTSTEP RADAR's IN; radar.active follows it)
            FootstepRadar::Settings radar {};                   // FOOTSTEP RADAR: finds footsteps, lifts them, far ones with space
            float strength = 1.0f;    // ENH STRENGTH: 0 = no effect .. 5 = five times the effect
            SpectralLeveler::Settings lumen {};   // UPWARD LEVELER: lifts quiet material
            SpectralLimiter::Settings limiter {}; // SPECTRAL LIMITER: cuts abnormal spectral excess
            DynamicCompressor::Settings tide {};  // ADAPTIVE COMPRESSOR: adaptive-threshold compressor
            Seraph::Settings seraph {};
            float levelDb = 0.0f;                 // LEVEL: the rack's working level (-24 .. +12 dB), first in the chain
            MixBalancer::Settings balancer {};    // MIX BALANCER: rides six band faders for the balance
            std::array<int, methods::numMethodIds> methods {};   // every processing method (MethodRegistry.h), 0 = default
            DeepSub::Settings deep {};                          // DEEP SUB: sub-harmonic synth and resonant hull
            Character::Settings character {};                   // CHARACTER: consoles, tape, valves (out by default)
            ProX4::Settings x4 {};                              // LATINSPHIEL PRO X4 (designed unit; PWR off by default)
            Velvetizer::Settings velvet {};                     // VELVETIZER (designed unit; POWER off by default)
            Takeback::Settings takeback {};                     // TAKEBACK (designed unit; POWER off by default)
            std::array<float, units::numParams> unitParams {};   // the newer units' parameters, raw (units/UnitList.h; each POWER off by default)
            std::array<float, lbmods::numParams> lbParams {};     // the LUNCHBOX modules' parameters, raw (units/LbList.h; each IN off by default)
            std::array<float, 21> customParams {};                 // the CUSTOM slot: POWER, 16 knobs, 4 switches
            Lunchbox::Settings lunchbox {};                     // LUNCHBOX: CLASS-A EQ, DE-HARSH, CROSSFEED (all out by default)
            bool compare = false;                               // COMPARE: hear the input instead, at the output's loudness
        };

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();
        void process (juce::AudioBuffer<float>&, const Parameters&) noexcept;

        int getLatencySamples() const noexcept { return analog.getLatencySamples() + radar.getLatencySamples() + seraph.getLatencySamples() + character.getLatencySamples() + earGuard.getLatencySamples() + output.getLatencySamples(); }

        /** Where the delay comes from, stage by stage (samples at the prepared rate): for the latency
            checks and the app's readout. The sum is getLatencySamples(). */
        struct LatencyPart { const char* stage; int samples; };
        std::array<LatencyPart, 6> getLatencyBreakdown() const noexcept
        {
            return {{ { "ADAPTIVE ENHANCER EQ (oversampled exciters)", analog.getLatencySamples() },
                      { "FOOTSTEP ENHANCER (look-ahead)", radar.getLatencySamples() },
                      { "TONE & SPACE FINISHER (oversampled TONE)", seraph.getLatencySamples() },
                      { "CONSOLE & TAPE EMULATOR (oversampled models)", character.getLatencySamples() },
                      { "EAR GUARD (look-ahead)", earGuard.getLatencySamples() },
                      { "OUTPUT LIMITER (look-ahead)", output.getLatencySamples() } }};
        }

        /** The loudness meter's RESET (any thread): integrated loudness and true-peak hold start again. */
        void resetLoudness() noexcept { loudnessResetPending.store (true, std::memory_order_relaxed); }
        const MixBalancer& getBalancer() const noexcept { return balancer; }
        const LoudnessMeter& getLoudness() const noexcept { return loudness; }
        const ScopeFifo& getBalancerInputScope() const noexcept { return scopeBalIn; }
        const ScopeFifo& getBalancerOutputScope() const noexcept { return scopeBalOut; }
        const FinalLimiter& getOutputLimiter() const noexcept { return output; }
        const EngineMeters& getMeters() const noexcept { return meters; }
        /** RACK TUNER's level match (any thread): a tune starts (newReference: remember how loud the rack is
            now), or A/B / UNDO changed it (match again to the same reference). TunerMatch.h. */
        void requestTunerMatch (bool newReference) noexcept { (newReference ? tunerCapture : tunerRematch).fetch_add (1, std::memory_order_relaxed); }
        /** THE PATCH BAY (DSP/PatchBay.h; any thread): `muted` - a cord is out of the chain, the rack goes silent
            (faded; back slowly when it is whole again); `noise` 0..1 - a plug half in: a quiet crackle and hum. */
        void setPatch (bool muted, float noise) noexcept { patchMuted.store (muted, std::memory_order_relaxed); patchNoise.store (noise, std::memory_order_relaxed); }
        void patchClick() noexcept { patchClicks.fetch_add (1, std::memory_order_relaxed); }
        /** The order the units after the enhancer run in (THE PATCH BAY's cords; message thread): stage codes, each
            once - see Stage. Stages left out run after, in their usual order. */
        enum Stage : int { stLumen = 0, stDeep, stLimiter, stBalancer, stTide, stRadar, stSeraph, stCharacter, stX4, stVelvet,
                           stTakeback, stCustom, stLunchbox, numCoreStages, stNewer = 100 };   // stNewer + k: newer unit k
        static constexpr int numStages = numCoreStages + units::count;
        void setChainOrder (const std::vector<int>& codes) noexcept
        {
            for (size_t i = 0; i < chainOrder.size(); ++i)
                chainOrder[i].store (i < codes.size() ? codes[i] + 1 : 0, std::memory_order_relaxed);
        }
        /** A loop of cords: what leaves the chain comes round into it again (any thread; a new call clears a runaway). */
        void setFeedback (bool on) noexcept { feedbackOn.store (on, std::memory_order_relaxed); feedbackEpoch.fetch_add (1, std::memory_order_relaxed); }
        /** CUSTOM: the loaded design's chain (message thread; the caller keeps every config alive). */
        void setCustomConfig (const units::CustomConfig* c) noexcept { custom->setConfig (c); }

        /** For tests: footsteps the radar has found since reset, and how much it is lifting now (0..1). */
        int getFootstepEventCount() const noexcept { return radar.getStepsTotal(); }
        float getFootstepConfidence() const noexcept { return radar.getActivity(); }
        const FootstepRadar& getRadar() const noexcept { return radar; }
        void setRadarLog (std::vector<FootstepRadar::Decision>* log) noexcept { radar.log = log; }   // tests (reserve first)
        const HarmonicPlanner& getHarmonicPlan() const noexcept { return planner; }
        const AdaptiveEQ& getEQ() const noexcept { return eq; }
        const Seraph& getSeraph() const noexcept { return seraph; }
        const DynamicCompressor& getCompressor() const noexcept { return tide; }
        const SpectralLeveler& getLeveler() const noexcept { return lumen; }
        const SpectralLimiter& getLimiter() const noexcept { return limiter; }

        /** Analyser taps: the audio thread only copies samples in, the editor does the FFT. */
        const ScopeFifo& getInputScope() const noexcept { return scopeIn; }
        const ScopeFifo& getOutputScope() const noexcept { return scopeOut; }

    private:
        /** STEREO (mid/side) around one unit: mode 0 runs it on left and right as ever; 1 on the middle
            only, 2 on the sides only. The part it does not work on waits `latency` samples (the unit's
            own) in `keep`, so the two meet again in time. */
        struct KeepDelay { std::vector<float> line; int pos = 0; };
        template <typename Fn>
        void inStereoMode (float* const* chunk, int chans, int n, int mode, int latency, KeepDelay& keep, Fn&& run) noexcept;

        /** COMPARE: the input, delayed to line up and brought to the loudness of the output - before the
            output limiter, so it is looked after like everything else. `start`: where in the block. */
        void compareStage (float* const* chunk, int chans, int n, int start, bool on) noexcept;

        void controlTick (const Parameters&) noexcept;
        void processChunk (juce::AudioBuffer<float>&, int start, int n, const Parameters&) noexcept;

        BandAnalyzer analyzer;
        PrecisionEQ precision;   // CLARITY's precision layer: moving bells with their own Q
        FootstepRadar radar;
        void publishRadar() noexcept;   // its readouts to the meters, for the display
        HarmonicPlanner planner;
        AdaptiveEQ eq;
        SubEnhancer sub;
        AnalogStage analog;
        SpectralLeveler lumen;
        SpectralLimiter limiter;
        DynamicCompressor tide;
        Seraph seraph;
        ScopeFifo scopeIn, scopeOut, scopeBalIn, scopeBalOut;
        MixBalancer balancer;
        DeepSub deep;
        float footstepRecentS = 0.0f;   // time left since the last footstep the enhancer lifted (for the balancer)
        LoudnessMeter loudness;
        std::atomic<bool> loudnessResetPending { false };
        float levelGain = 1.0f, levelDbNow = 0.0f;
        EngineMeters meters;

        /** The one output limiter: lookahead, holds through a bass cycle, never wobbles inside one. */
        Character character;     // CHARACTER, after TONE & SPACE
        Lunchbox lunchbox;       // LUNCHBOX, after CHARACTER (zero latency)
        OutputStage outputStage; // the rack's output amplifier: one analog signal path (only while an analog unit is in)
        TunerMatch tunerMatch;   // RACK TUNER: the rack kept as loud after a tune as before it
        std::atomic<bool> patchMuted { false };
        std::atomic<float> patchNoise { 0.0f };
        float patchGain = 1.0f, humPhase = 0.0f, crackle = 0.0f, clickEnv = 0.0f, clickPhase = 0.0f;
        std::atomic<int> patchClicks { 0 };
        int clicksDone = 0;
        juce::uint32 crackleSeed = 0x9e3779b9u;
        std::array<std::atomic<int>, numStages> chainOrder {};   // each stage code + 1 (0: the usual order)
        std::atomic<bool> feedbackOn { false };
        std::atomic<int> feedbackEpoch { 0 };
        int feedbackEpochSeen = 0, fbPos = 0;
        std::array<std::vector<float>, 2> fbRing;
        std::array<float, 2> fbHp {}, fbLp {};
        float fbLevel = 0.0f, fbInLevel = 0.0f, runawayS = 0.0f;
        bool runaway = false;
        void runStage (int code, float* const* chunk, int chans, int n, int start, const Parameters& p) noexcept;
        void patchStage (float* const* chunk, int chans, int n) noexcept;
        std::atomic<int> tunerCapture { 0 }, tunerRematch { 0 };

        std::array<std::vector<float>, 3> msScratch;   // mid/side: the part worked on (two channels), the part kept
        std::array<KeepDelay, 6> keepDelays;            // one per unit with a STEREO setting

        std::array<std::vector<float>, 2> compareIn, compareLine;   // the block as it came in; the latency delay
        int comparePos = 0, compareLen = 0;
        BiquadCoeffs compareKPre, compareKRlb;                        // K-weighting, as loudness is heard
        std::array<BiquadState, 2> compareInPre {}, compareInRlb {}, compareOutPre {}, compareOutRlb {};
        double compareInLevel = 0.0, compareOutLevel = 0.0;
        float compareDb = 0.0f, compareGain = 1.0f, compareMix = 0.0f;
        bool compareReady = false;   // this block's input was kept (hosts may send more than COMPARE can hold)

        // Speaker and headset protection, always on: DC and sub-sonic (8 Hz high-pass, before the output
        // limiter), a fade-in after any start or reset, and a last check on what leaves
        SvfCoeffs protectHp {};
        std::array<SvfState, 2> protectState {};
        float startGain = 0.0f, startStep = 1.0f;
        ProX4 x4;                // LATINSPHIEL PRO X4 (after CHARACTER)
        Velvetizer velvet;       // VELVETIZER (after the PRO X4)
        Takeback takeback;       // TAKEBACK (after the VELVETIZER)
        std::vector<std::unique_ptr<units::RackUnit>> newer;   // the newer units, in rack order (on the heap)
        std::vector<std::unique_ptr<units::RackUnit>> lbNewer; // the LUNCHBOX's 500-series modules (units/LbList.h)
        std::array<float, 192> displayScratch {};              // (a unit's picture state, on its way to the meters)
        void runLbModules (float* const* chunk, int chans, int n, const Parameters& p, bool pre) noexcept;
        std::unique_ptr<units::CustomUnit> custom = std::make_unique<units::CustomUnit>();   // CUSTOM (a design's chain)
        SessionCare sessionCare; // LONG SESSIONS: a forward balance eased over tens of seconds
        HeadphoneRoom room;      // HEADPHONE ROOM: early reflections, out of the head
        HeadphoneEQ headphones;  // HEADPHONES: the listener's headphones corrected (after COMPARE: A and B alike)
        LoudnessTarget target;   // LOUDNESS TARGET, just before the output limiter
        FinalLimiter output;
        EarGuard earGuard;   // always on: no sudden jump far over how loud it has been

        double sampleRate = 48000.0;
        int maxBlock = 512;
        int controlInterval = 32, samplesToTick = 32, planCountdown = 0;
        float controlDt = 1.0f / 1500.0f;

        float strength = 1.0f, normalize = 0.5f, boost = 0.0f, speed = 0.4f, subAmount = 0.0f, transient = 0.0f;
    };
}
