#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <juce_dsp/juce_dsp.h>
#include "DspMath.h"
#include "LoudnessMeter.h"

namespace enh::dsp
{
    /** CHARACTER: the sound of the hardware the rack would be built from - consoles, tape, valves -
        and two voicings made for games. Two models at once, A and B, with BLEND morphing between them
        and DRIVE setting how hard they are hit (the level is given back afterwards, so DRIVE changes
        the colour, not the loudness).

        Every model is a chain of what the real thing does, not one curve with a different name:

          transformer   the core's flux is the integral of the voltage, and the core saturates on flux,
                        not voltage: so the iron distorts the low end and leaves the top alone, as a
                        real transformer does. Modelled as a leaky integrator -> saturation -> the
                        exact inverse filter, applied to the difference only (at small levels it is a
                        wire, bit for bit in the linear sense).
          amplifier     a biased stage with unity small-signal gain: the bias sets the even/odd
                        balance, a valve's bias sags with the programme (so the colour follows it),
                        an op-amp's output can only move so fast (slew: a faint sheen on steep peaks).
          tape          pre-emphasis -> magnetic saturation -> de-emphasis (so the top saturates
                        first, and loud highs soften the way tape's do), head bump and gap loss.

        All of the non-linear part runs 4x oversampled through polyphase half-band filters (the
        harmonics it makes above half the sample rate are filtered off instead of folding back as
        aliases: -75 dB or better at the default DRIVE; 2x was not enough at full DRIVE); the EQ that
        shapes each model runs
        on the same oversampled signal, on state-variable filters that stay exact at low frequencies.

        OUT is a delay of the same length as the oversampling, so switching IN, or any setting,
        never moves the audio in time; the reported latency is constant. */
    class Character
    {
    public:
        enum Model { clean, british, american, vintage, tape15, tape30, valve, arena, cinema, numModels };

        static constexpr std::array<const char*, numModels> names {
            "MODERN CLEAN", "BRITISH CONSOLE", "AMERICAN CONSOLE", "VINTAGE CONSOLE",
            "TAPE 15 IPS", "TAPE 30 IPS", "VALVE", "ARENA", "CINEMA" };

        struct Settings
        {
            bool active = false;
            int modelA = vintage, modelB = tape15;
            float blend = 0.0f;       // 0 = A only, 1 = B only
            float drive = 5.0f;       // 0 .. 10: -15 .. +15 dB into the models, given back after
            int components = 0;       // COMPONENTS method: 0 matched, 1 subtle, 2 vintage (left and right differ)
            bool grit = true;         // GRIT: DRIVE may grow into distortion (on) or stops short of it (off: clean drive)
            float colour = 5.0f;      // COLOUR 0 .. 10: how much of each model's character - its voicing and its
                                      // harmonics at any level - separate from DRIVE (0 = neither, 5 = default)
        };

        static constexpr int oversamplingStages = 2;   // 4x (2x aliased at full DRIVE: -23 dB)

        void prepare (double sampleRate, int maxBlock, int numChannels)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            chans = std::clamp (numChannels, 1, 2);
            os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) chans, oversamplingStages,
                     juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            os->initProcessing ((size_t) std::max (1, maxBlock));
            latency = (int) std::lround (os->getLatencyInSamples());
            osr = sr * (double) (1 << oversamplingStages);

            for (auto& d : dry)
                d.assign ((size_t) std::max (1, latency), 0.0f);
            setMaxBlock (maxBlock);

            kWeighting (sr, preCoeffs, rlbCoeffs);
            guardAtt = (float) std::exp (-1.0 / (0.001 * osr));
            guardRel = (float) std::exp (-1.0 / (0.150 * osr));
            colourAtt = 0.0f;   // up at once: a lifted peak never passes the sweet spot, even the first after a pause
            colourRel = (float) std::exp (-1.0 / (0.300 * osr));
            designAll (0);
            reset();
        }

        void reset()
        {
            if (os != nullptr)
                os->reset();
            for (auto& d : dry)
                std::fill (d.begin(), d.end(), 0.0f);
            dryPos = 0;
            for (auto& v : voices)
                v = Voice {};
            wet = 0.0f;
            driveGain = 1.0f;
            dryLevel = wetLevel = 0.0;
            matchDb = 0.0f;
            matchGain = 1.0f;
            dryPeak = wetPeak = 0.0f;
            guardEnv = {};
            colourEnv = {};
            for (auto* f : { &kDryPre, &kDryRlb, &kWetPre, &kWetRlb })
                for (auto& st : *f)
                    st.reset();
            harmonicsDb = -120.0f;
            residualMs = signalMs = 0.0;
        }

        int getLatencySamples() const noexcept { return latency; }

        /** What the models add that was not there (their non-linear parts only), against the signal,
            in dB: -40 is about 1 %, -20 about 10 %. For the unit's meter. */
        float getHarmonicsDb() const noexcept { return harmonicsDb; }

        void process (float* const* ch, int numChannels, int n, const Settings& s) noexcept
        {
            const int nc = std::min (numChannels, chans);
            if (nc <= 0 || n <= 0 || os == nullptr)
                return;

            // While COLOUR glides, work in short steps: the voicing EQs are redesigned at each, and at full
            // COLOUR (eight times the voicing) one redesign per block is a step big enough to click
            if (n > 32 && nc <= 2 && std::abs (std::clamp (s.colour, 0.0f, 10.0f) - colourNow) > 0.01f)
            {
                for (int o = 0; o < n; o += 32)
                {
                    float* part[2] = { ch[0] + o, nc > 1 ? ch[1] + o : nullptr };
                    process (part, nc, std::min (32, n - o), s);
                }
                return;
            }

            // COLOUR glides (~40 ms); the voicing EQs are redesigned as it moves (a few coefficients per
            // model, only while it moves). COLOUR 2.5 is the voicing as designed, 5 twice that - and the top
            // half of the knob opens up: 10 is eight times it, so full COLOUR is unmistakably the model.
            const float blockSeconds = (float) n / (float) sr;
            colourNow += (std::clamp (s.colour, 0.0f, 10.0f) - colourNow) * std::min (1.0f, blockSeconds / 0.040f);
            const float toneWanted = colourNow <= 5.0f ? colourNow / 2.5f : 2.0f + (colourNow - 5.0f) / 5.0f * 6.0f;
            // COMPONENTS changed, or COLOUR moved: the parts are redesigned (no allocation)
            if (s.components != designedComponents || std::abs (toneWanted - designedTone) > 0.004f)
            {
                designedTone = toneWanted;
                designAll (s.components);
            }

            const float wetTarget = s.active ? 1.0f : 0.0f;
            if (wet == 0.0f && wetTarget == 0.0f)
            {
                delayOnly (ch, nc, n);
                harmonicsDb = -120.0f;
                return;
            }

            // Coming back IN from OUT: everything inside restarts clean (the fade-in covers the start);
            // carrying on from where it stopped would step the audio
            if (wet == 0.0f)
            {
                os->reset();
                for (auto& v : voices)
                    v = Voice {};
                dryLevel = wetLevel = 0.0;
                matchDb = 0.0f;
                matchGain = 1.0f;
            }

            assignVoices (s);

            // Everything that moves, moves across the block (30 ms for the voices and IN, 20 ms for DRIVE)
            const float blockS = (float) n / (float) sr;
            const float fade = std::min (1.0f, blockS / 0.030f);
            const float wetFrom = wet;
            wet = approach (wet, wetTarget, fade);

            std::array<float, numVoices> wFrom {}, wTo {};
            for (size_t v = 0; v < voices.size(); ++v)
            {
                wFrom[v] = voices[v].weight;
                voices[v].weight = approach (voices[v].weight, voices[v].target, fade);
                wTo[v] = voices[v].weight;
                // Silent for this whole block and not wanted: free (its state is reset when reused). Freed
                // any earlier - while its fade still ends in this block - it would drop out: a click.
                if (wFrom[v] == 0.0f && wTo[v] == 0.0f && voices[v].target == 0.0f)
                    voices[v].model = -1;
            }

            // How far toward the sweet spot COLOUR takes the music: none at 0, halfway (in level) at 5, and on
            // the top half of the knob past it - up to twice the clean peak at 10: warm, plainly audible
            // harmonics (a few percent), still not DRIVE's grit
            colourDepth = colourNow <= 5.0f ? colourNow / 10.0f : 0.5f + (colourNow - 5.0f) / 5.0f * 1.5f;
            const float driveTarget = dbToGain ((std::clamp (s.drive, 0.0f, 10.0f) - 5.0f) * 3.0f);
            const float driveFrom = driveGain;
            driveGain += (driveTarget - driveGain) * std::min (1.0f, blockS / 0.020f);
            const float driveTo = driveGain;

            // Keep the dry, delayed copy (for IN's crossfade) before the audio is replaced
            std::array<float*, 2> dryOut { scratchDry[0].data(), scratchDry[1].data() };
            const int m = std::min (n, (int) scratchDry[0].size());
            delayInto (ch, nc, m, dryOut);

            juce::dsp::AudioBlock<float> block (ch, (size_t) nc, (size_t) m);
            auto up = os->processSamplesUp (block);
            const int un = (int) up.getNumSamples();
            const float inv = 1.0f / (float) std::max (1, un);

            double residual = 0.0, signal = 0.0;

            for (int c = 0; c < nc; ++c)
            {
                auto* x = up.getChannelPointer ((size_t) c);
                const float trim = c == 1 ? rightDrive : 1.0f;

                for (int i = 0; i < un; ++i)
                {
                    const float t = (float) (i + 1) * inv;
                    const float g = (driveFrom + (driveTo - driveFrom) * t) * trim;
                    const float u = x[i] * g;

                    // GRIT off: follow the peaks going in (1 ms up, 150 ms down); a model is fed no more than
                    // its clean peak, and given the difference back after, so the level stays and the
                    // colour grows only as far as it stays clean
                    const float a = std::abs (u);
                    guardEnv[(size_t) c] = a > guardEnv[(size_t) c] ? a + guardAtt * (guardEnv[(size_t) c] - a)
                                                                    : a + guardRel * (guardEnv[(size_t) c] - a);
                    // COLOUR: the music's peak level (up at once, 300 ms down), for the lift into each
                    // model's sweet spot below
                    colourEnv[(size_t) c] = a > colourEnv[(size_t) c] ? a + colourAtt * (colourEnv[(size_t) c] - a)
                                                                      : a + colourRel * (colourEnv[(size_t) c] - a);

                    float y = 0.0f;
                    for (size_t v = 0; v < voices.size(); ++v)
                    {
                        auto& voice = voices[v];
                        if (voice.model < 0)
                            continue;
                        const float w = wFrom[v] + (wTo[v] - wFrom[v]) * t;
                        if (w <= 0.0f)
                            continue;

                        float res = 0.0f;
                        const auto& k = coeffs[(size_t) c][(size_t) voice.model];
                        // COLOUR's harmonics: the model's non-linear core is fed the music lifted toward the
                        // model's sweet spot (its clean peak, ~1 % THD) and the lift is taken off again after
                        // it, so the character is there at any playback level - quiet music too - and not only
                        // once DRIVE pushes it. Only ever a lift (up to 24 dB): hot music is left to DRIVE.
                        const float sweet = k.cleanPeak * std::min (colourDepth, k.colourHeadroom) * (s.grit ? 1.8f : 1.0f);
                        const float env = colourEnv[(size_t) c];
                        const float lift = env > 1.0e-6f ? std::clamp (sweet / env, 1.0f, 32.0f) : 1.0f;
                        // GRIT off: what reaches the curves never passes the clean peak, lift included - or, on
                        // COLOUR's top half, the colour ceiling it asks for (up to twice it)
                        const float ceiling = k.cleanPeak * std::clamp (colourDepth, 1.0f, k.colourHeadroom);
                        const float hot = guardEnv[(size_t) c] * lift;
                        const float guard = s.grit || hot <= ceiling ? 1.0f : ceiling / hot;
                        y += w * runModel (voice.model, k, voice.state[(size_t) c], u, res, lift * guard);
                        residual += (double) (w * res) * (w * res);
                    }

                    signal += (double) u * u;
                    x[i] = y / g;
                }
            }

            os->processSamplesDown (block);

            // DRIVE changes the colour, not the loudness: what the models took off (hard-driven peaks)
            // or added (a model's tone, its low-end bump) is given back. Measured K-weighted, as loudness
            // is heard (plain RMS let the tape and console bumps read louder than they sound), on 1.5 s
            // levels, followed over 1.5 s, 6 dB at most, and held in silence. The same makes A against B,
            // and IN against OUT, a fair listen.
            {
                double d = 0.0, w = 0.0;
                float dryPk = 0.0f, wetPk = 0.0f;
                for (int c = 0; c < nc; ++c)
                    for (int i = 0; i < m; ++i)
                    {
                        dryPk = std::max (dryPk, std::abs (dryOut[(size_t) c][i]));
                        wetPk = std::max (wetPk, std::abs (ch[c][i]));
                        const float kd = kDryRlb[(size_t) c].process (rlbCoeffs, kDryPre[(size_t) c].process (preCoeffs, dryOut[(size_t) c][i]));
                        const float kw = kWetRlb[(size_t) c].process (rlbCoeffs, kWetPre[(size_t) c].process (preCoeffs, ch[c][i]));
                        d += (double) kd * kd;
                        w += (double) kw * kw;
                    }
                const double norm = 1.0 / (double) (nc * m);
                const double kk = std::exp (-(double) blockS / 1.5);
                dryLevel = kk * dryLevel + (1.0 - kk) * d * norm;
                wetLevel = kk * wetLevel + (1.0 - kk) * w * norm;
                // Peaks, held 300 ms: the match may never lift the colour's peaks more than 1 dB over the
                // input's - it gives loudness back, it does not push the output limiter into distortion
                const float pk = std::exp (-blockS / 0.3f);
                dryPeak = std::max (dryPk, dryPeak * pk);
                wetPeak = std::max (wetPk, wetPeak * pk);
                if (dryLevel > 1.0e-7 && wetLevel > 1.0e-12)
                {
                    float want = std::clamp ((float) (10.0 * std::log10 (dryLevel / wetLevel)), -6.0f, 6.0f);
                    if (wetPeak > 1.0e-6f && dryPeak > 1.0e-6f)
                        want = std::min (want, 20.0f * std::log10 (dryPeak / wetPeak) + 1.0f);
                    matchDb += (want - matchDb) * std::min (1.0f, blockS / 1.5f);
                }
            }
            const float matchFrom = matchGain;
            matchGain = dbToGain (matchDb);

            // IN / OUT: crossfade with the dry copy, which is delayed by exactly the same amount
            for (int c = 0; c < nc; ++c)
            {
                for (int i = 0; i < m; ++i)
                {
                    const float t = (float) (i + 1) / (float) m;
                    const float w = wetFrom + (wet - wetFrom) * t;
                    const float g = matchFrom + (matchGain - matchFrom) * t;
                    ch[c][i] = dryOut[(size_t) c][i] + (ch[c][i] * g - dryOut[(size_t) c][i]) * w;
                }
            }

            // The meter: a 300 ms average of what the models add, against what went in
            const double k = std::exp (-(double) blockS / 0.3);
            residualMs = k * residualMs + (1.0 - k) * residual * inv;
            signalMs = k * signalMs + (1.0 - k) * signal * inv;
            harmonicsDb = signalMs > 1.0e-12 && wet > 0.01f
                              ? (float) std::clamp (10.0 * std::log10 ((residualMs + 1.0e-20) / signalMs), -120.0, 0.0)
                              : -120.0f;
        }

        void setMaxBlock (int maxBlock)
        {
            for (auto& d : scratchDry)
                d.assign ((size_t) std::max (1, maxBlock), 0.0f);
        }

    private:
        //==============================================================================
        static constexpr int numVoices = 4;   // A, B, and the two they may be fading from

        /** One model's parts, designed for the oversampled rate (and for one channel's tolerances). */
        struct Coeffs
        {
            std::array<SvfEqCoeffs, 2> pre {};     // before the non-linear part (tape: pre-emphasis)
            std::array<SvfEqCoeffs, 5> post {};    // after it
            int numPre = 0, numPost = 0;
            SvfCoeffs highPass {}, lowPass {};
            bool hasHighPass = false, hasLowPass = false;

            float inputTrim = 1.0f;                // where this model's 0 VU sits against its curves
            float cleanPeak = 1.0f;                // GRIT off: the most that may go in (about 1 % THD for this model)
            float colourHeadroom = 2.0f;           // how far past it full COLOUR may take it (warm, a few percent, never grit)

            float fluxR = 0.0f;                    // transformer: leaky integrator pole (0 = none)
            float fluxLimit = 1.0f, fluxBias = 0.0f;
            float flux2R = 0.0f, flux2Limit = 1.0f; // a second (output) transformer
            float fluxInvLimit2 = 1.0f, fluxBiasSat = 0.0f, flux2InvLimit2 = 1.0f;   // worked out once, in design()

            int amp = 0;                           // 0 none, 1 biased stage, 2 tape curve, 3 clean ceiling
            float ampK = 1.0f, ampBias = 0.0f, ampLimit = 1.0f;
            float sag = 0.0f, envAtt = 0.0f, envRel = 0.0f;   // valve: bias follows the programme
            float slew = 0.0f;                     // op-amp: most a sample may move (0 = no limit)

            // --- Behaviour over time, frequency and transients (each model its own) -------------------
            float even = 0.0f;                     // a static tilt of the curve: 2nd (and 4th) beside the odd ones
            float transientDrive = 0.0f;           // how much harder a transient hits the core than the sustain
            float thermal = 0.0f;                  // heat: long hard driving shifts the bias (even content) a little
            float coupling = 0.0f;                 // coupling capacitor: big lows charge it, the operating point
                                                   // moves, and recovers over ~250 ms (blocking-type squash)
            float hysteresis = 0.0f;               // iron: the core's magnetisation lags its drive (a loop, not a curve)
            float fluxMemory = 0.0f;               // iron: a core pushed hard saturates sooner for a while after
            float bandwidthLoss = 0.0f;            // driven hard, the stage's top end closes a little (0..1)
            float lowEmphasisDb = 0.0f;            // lows fed hotter into the core, cut back after: thick, compressed lows
            SvfEqCoeffs emphPre {}, emphPost {};   // (designed from lowEmphasisDb: exact mirrors)
        };

        struct State
        {
            std::array<SvfEqState, 2> pre {};
            std::array<SvfEqState, 5> post {};
            SvfState highPass {}, lowPass {};
            float flux = 0.0f, fluxRes = 0.0f, flux2 = 0.0f, flux2Res = 0.0f;
            float env = 0.0f, slewed = 0.0f;
            float dcX = 0.0f, dcY = 0.0f;
            float fast = 0.0f, slow = 0.0f;        // transient detection: the drive's peak, fast and slow
            float heat = 0.0f;                     // thermal memory (seconds)
            float couple = 0.0f;                   // the coupling capacitor's charge
            float mag = 0.0f;                      // iron: the lagging magnetisation
            float fluxPeak = 0.0f;                 // iron: how hard the core has been pushed lately
            float bw = 0.0f;                       // level-dependent bandwidth: the low-passed output
            SvfEqState emphPre {}, emphPost {};
        };

        struct Voice
        {
            int model = -1;
            float weight = 0.0f, target = 0.0f;
            std::array<State, 2> state {};
        };

        static float approach (float v, float target, float step) noexcept
        {
            return v < target ? std::min (target, v + step) : std::max (target, v - step);
        }

        /** A and B get their shares; a model no longer wanted fades out while its replacement fades in. */
        void assignVoices (const Settings& s) noexcept
        {
            const int a = std::clamp (s.modelA, 0, numModels - 1), b = std::clamp (s.modelB, 0, numModels - 1);
            const float blend = std::clamp (s.blend, 0.0f, 1.0f);

            for (auto& v : voices)
                v.target = 0.0f;

            auto want = [&] (int model, float share)
            {
                if (share <= 0.0f)
                    return;
                for (auto& v : voices)
                    if (v.model == model)
                    {
                        v.target += share;
                        return;
                    }
                // A free voice, else the quietest one
                Voice* pick = nullptr;
                for (auto& v : voices)
                    if (v.model < 0) { pick = &v; break; }
                if (pick == nullptr)
                {
                    pick = &voices[0];
                    for (auto& v : voices)
                        if (v.weight < pick->weight)
                            pick = &v;
                }
                *pick = Voice {};
                pick->model = model;
                pick->target = share;
            };

            want (a, 1.0f - blend);
            want (b, blend);
        }

        //==============================================================================
        /** tanh by Lambert's continued fraction (7/6): within 2e-7 of std::tanh up to |x| = 4.97, where
            it meets +-1, at a fraction of the cost. Four of these run per sample, four times over. */
        static inline float fastTanh (float x) noexcept
        {
            if (x > 4.97f) return 1.0f;
            if (x < -4.97f) return -1.0f;
            const float x2 = x * x;
            return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
                     / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
        }

        static inline float fluxStage (float x, float& flux, float& res, float r, float invLimit2, float bias, float biasSat,
                                       float* mag = nullptr, float* peak = nullptr, float hysteresis = 0.0f, float memory = 0.0f,
                                       float peakK = 0.0f, float peakAttK = 0.0f) noexcept
        {
            // Flux = the voltage integrated (leaky, unity at DC); the iron saturates on flux; what the
            // saturation takes away is turned back into voltage by the exact inverse of the integrator.
            flux = r * flux + (1.0f - r) * x;
            // Magnetic memory: a core driven hard lately saturates a little sooner, and recovers (~400 ms)
            if (peak != nullptr)
            {
                // (up over ~5 ms, not at once: a sudden change of the curve, through the integrator's
                // inverse, would be a click)
                const float a = std::abs (flux);
                *peak = a > *peak ? a + peakAttK * (*peak - a) : *peak * peakK;
                invLimit2 *= 1.0f + memory * std::min (4.0f, *peak * *peak * invLimit2);
            }
            const float f = flux + bias;
            const float sat = f / std::sqrt (1.0f + f * f * invLimit2);
            float d = (sat - biasSat) - flux;
            // Hysteresis: the magnetisation lags its drive, so the saturation traces a loop (only the part
            // the iron takes away lags: at small levels it is still a wire)
            if (mag != nullptr && hysteresis > 0.0f)
            {
                *mag += (d - *mag) * (1.0f - hysteresis);
                d = *mag;
            }
            const float out = (d - r * res) / (1.0f - r);
            res = d;
            return x + out;
        }

        static inline float biasedStage (float x, float k, float bias) noexcept
        {
            // Unity slope at rest whatever the bias; the bias tilts it (even harmonics)
            const float tb = fastTanh (k * bias);
            const float slope = k * (1.0f - tb * tb);
            return (fastTanh (k * (x + bias)) - tb) / slope;
        }

        static inline float tapeCurve (float x, float limit) noexcept
        {
            // Magnetic saturation: a soft knee that bends harder than tanh near the top,
            // x / (1 + a^3)^(1/3) - one cube root instead of two powers
            const float a = std::abs (x) / limit;
            return x / std::cbrt (1.0f + a * a * a);
        }

        /** nlGain: the gain into the non-linear core (COLOUR's lift, GRIT off's guard), taken off again
            right after it - only the core sees it, so the voicing EQ around it is never modulated. */
        inline float runModel (int model, const Coeffs& k, State& s, float u, float& residual, float nlGain) const noexcept
        {
            juce::ignoreUnused (model);
            float x = u * k.inputTrim;

            for (int i = 0; i < k.numPre; ++i)
                x = s.pre[(size_t) i].process (k.pre[(size_t) i], x);

            if (k.lowEmphasisDb != 0.0f)
                x = s.emphPre.process (k.emphPre, x);   // lows in hotter (cut back after the core)

            const float beforeNl = x;

            // A transient hits the core harder than the sustain behind it (each model by its own amount): the
            // drive's peak followed fast and slow, and how far the fast one stands above the slow
            const float drive = std::abs (x * nlGain);
            s.fast = drive > s.fast ? drive + tAtt * (s.fast - drive) : drive + tRelFast * (s.fast - drive);
            s.slow = drive > s.slow ? drive + tAttSlow * (s.slow - drive) : drive + tRelSlow * (s.slow - drive);   // a peak too, 10 ms up
            const float trans = std::clamp ((s.fast - 1.15f * s.slow) / (s.fast + 1.0e-6f), 0.0f, 1.0f);
            const float g = nlGain * (1.0f + k.transientDrive * trans);
            x *= g;

            // Heat (seconds) and the coupling capacitor (charged by big positive swings, ~250 ms to recover):
            // both move the operating point a little - the colour follows what the music has been doing
            s.heat += (x * x - s.heat) * heatK;
            const float hn = s.heat / (4.0f * k.cleanPeak * k.cleanPeak);   // (how hot, against the model's clean point)
            const float warm = k.thermal * 2.0f * hn / (1.0f + hn);       // grows with heat, eases toward its limit
            if (k.coupling > 0.0f)
                s.couple = std::max (s.couple * coupleK, (x - 0.8f * k.cleanPeak) * 0.02f);
            const float shift = warm + k.coupling * std::min (0.3f, s.couple / k.cleanPeak);

            if (k.fluxR > 0.0f)
                x = fluxStage (x, s.flux, s.fluxRes, k.fluxR, k.fluxInvLimit2, k.fluxBias, k.fluxBiasSat,
                               &s.mag, &s.fluxPeak, k.hysteresis, k.fluxMemory, fluxPeakK, fluxAttK);

            // A tilt of the curve: even harmonics beside the odd ones (the static part, and the part heat and
            // the capacitor add). x + e x^2 is linear at small levels, so quiet music stays clean
            const float e = k.even + (k.amp == 1 ? 0.0f : shift);
            if (e != 0.0f)
                x += e * x * x;

            switch (k.amp)
            {
                case 1:
                {
                    float bias = k.ampBias + shift;
                    if (k.sag > 0.0f)
                    {
                        const float a = std::abs (x);
                        s.env = (a > s.env ? k.envAtt : k.envRel) * (s.env - a) + a;
                        bias += k.sag * s.env;
                    }
                    x = biasedStage (x, k.ampK, std::min (0.5f, bias));   // sags, but never past the curve's reach
                    break;
                }
                case 2: x = tapeCurve (x, k.ampLimit); break;
                case 3: x = k.ampLimit * fastTanh (x / k.ampLimit); break;
                default: break;
            }

            // Driven hard, the stage's top closes a little (its bandwidth follows the level): quiet music
            // passes untouched, hot passages lose some glare above ~12 kHz
            if (k.bandwidthLoss > 0.0f)
            {
                s.bw += bwK * (x - s.bw);
                const float driven = k.bandwidthLoss * std::clamp (s.slow / (1.5f * k.cleanPeak) - 0.35f, 0.0f, 1.0f);
                x += (s.bw - x) * driven;
            }

            if (k.slew > 0.0f)
            {
                // An output that can only move so fast: smooth, so it only rounds the steepest edges
                s.slewed += k.slew * fastTanh ((x - s.slewed) / k.slew);
                x = s.slewed;
            }

            if (k.flux2R > 0.0f)
                x = fluxStage (x, s.flux2, s.flux2Res, k.flux2R, k.flux2InvLimit2, 0.0f, 0.0f);

            // The biased and tilted stages make a little DC; take it off (4 Hz)
            if (k.amp == 1 || k.fluxBias != 0.0f || e != 0.0f || k.even != 0.0f || k.thermal > 0.0f || k.coupling > 0.0f)
            {
                const float y = x - s.dcX + dcCoeff * s.dcY;
                s.dcX = x;
                s.dcY = y;
                x = y;
            }

            x /= g;
            residual = x - beforeNl;
            if (k.lowEmphasisDb != 0.0f)
                x = s.emphPost.process (k.emphPost, x);

            for (int i = 0; i < k.numPost; ++i)
                x = s.post[(size_t) i].process (k.post[(size_t) i], x);
            if (k.hasHighPass)
                x = s.highPass.process (k.highPass, x).high;
            if (k.hasLowPass)
                x = s.lowPass.process (k.lowPass, x).low;

            return x / k.inputTrim;
        }

        //==============================================================================
        /** Every model, for both channels. `components` > 0 makes the right channel's parts a little
            different, as two channels of real hardware are: a touch more drive, corners a little off. */
        void designAll (int components) noexcept
        {
            designedComponents = components;
            // Even "matched" channels are not identical, as no two channels of real hardware are: a hair of
            // difference in drive, corners and bias (inaudible as width, it keeps the two sides alive)
            const float spread = components == 1 ? 1.0f : components == 2 ? 3.0f : 0.12f;
            rightDrive = dbToGain (0.4f * spread);
            dcCoeff = (float) std::exp (-2.0 * 3.141592653589793 * 4.0 / osr);
            auto pole = [this] (double seconds) { return (float) std::exp (-1.0 / (seconds * osr)); };
            tAtt = pole (0.0003); tRelFast = pole (0.015); tAttSlow = pole (0.010); tRelSlow = pole (0.120);
            heatK = 1.0f - pole (2.0);
            coupleK = pole (0.25);
            fluxPeakK = pole (0.40);
            fluxAttK = pole (0.005);
            bwK = 1.0f - (float) std::exp (-2.0 * 3.141592653589793 * 12000.0 / osr);

            for (int c = 0; c < 2; ++c)
            {
                const double f = c == 1 ? 1.0 + 0.025 * spread : 1.0;       // corner frequencies
                const float b = c == 1 ? 1.0f + (spread < 0.5f ? 0.04f : 0.12f) * spread : 1.0f;   // bias
                for (int m = 0; m < numModels; ++m)
                    coeffs[(size_t) c][(size_t) m] = design (m, f, b, designedTone);
            }
        }

        /** tone: how far each model's voicing EQ goes (COLOUR; 1 = the voicing as designed, at COLOUR 2.5).
            Only the voicing scales: tape's de-emphasis must stay the exact mirror of its pre-emphasis. */
        Coeffs design (int model, double f, float biasScale, float tone) const noexcept
        {
            Coeffs k;
            const double o = osr;
            auto r = [&] (double hz) { return (float) std::exp (-2.0 * 3.141592653589793 * hz * f / o); };
            auto post = [&] (SvfEqCoeffs c) { if (k.numPost < (int) k.post.size()) k.post[(size_t) k.numPost++] = c; };
            auto dB = [tone] (double gain) { return gain * (double) tone; };   // a voicing gain, at this COLOUR

            switch (model)
            {
                case clean:
                    k.cleanPeak = 1.40f;
                    // A very high, very soft ceiling: nothing at working levels, a clean round-off when hit hard
                    k.amp = 3; k.ampLimit = 1.25f; k.inputTrim = 0.22f;
                    break;

                case british:
                    k.cleanPeak = 0.60f;
                    // Op-amp console: odd and quiet until pushed, a faint slew sheen, crisp and tight
                    k.inputTrim = 0.60f;
                    k.amp = 3; k.ampLimit = 1.05f;
                    k.slew = 0.22f;
                    post (SvfEqCoeffs::highShelf (o, 9500.0 * f, 0.6, dB (0.6)));
                    post (SvfEqCoeffs::bell (o, 180.0 * f, 0.7, dB (-0.3)));
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 12.0 * f, 0.6);
                    break;

                case american:
                    k.cleanPeak = 0.22f;
                    // Discrete op-amp into an output transformer: punchy, even and odd together
                    k.inputTrim = 0.75f;
                    k.amp = 1; k.ampK = 1.25f; k.ampBias = 0.08f * biasScale;
                    k.flux2R = r (14.0); k.flux2Limit = 0.30f;
                    post (SvfEqCoeffs::bell (o, 110.0 * f, 0.8, dB (0.6)));
                    post (SvfEqCoeffs::bell (o, 3000.0 * f, 0.6, dB (0.35)));
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 14.0 * f, 0.6);
                    break;

                case vintage:
                    k.cleanPeak = 0.18f;
                    // Class-A with input and output transformers: weight, even harmonics, a silky top
                    k.inputTrim = 0.80f;
                    k.fluxR = r (10.0); k.fluxLimit = 0.16f; k.fluxBias = 0.008f * biasScale;
                    k.amp = 1; k.ampK = 0.95f; k.ampBias = 0.12f * biasScale;
                    k.flux2R = r (16.0); k.flux2Limit = 0.35f;
                    post (SvfEqCoeffs::lowShelf (o, 75.0 * f, 0.6, dB (1.0)));   // the input iron softens the lows it drives: the shelf makes up for it
                    post (SvfEqCoeffs::bell (o, 1600.0 * f, 0.5, dB (0.25)));
                    k.hasLowPass = true; k.lowPass = SvfCoeffs::make (o, 23000.0 * f, 0.55);
                    break;

                case tape15:
                    k.cleanPeak = 0.40f;
                    // 15 ips: pre-emphasis, saturation, de-emphasis; head bump at 50 Hz, gap loss up top
                    k.inputTrim = 0.90f;
                    k.pre[0] = SvfEqCoeffs::highShelf (o, 3200.0 * f, 0.6, 6.0); k.numPre = 1;
                    k.amp = 2; k.ampLimit = 0.95f;
                    post (SvfEqCoeffs::highShelf (o, 3200.0 * f, 0.6, -6.0));
                    post (SvfEqCoeffs::bell (o, 52.0 * f, 1.1, dB (1.3)));
                    post (SvfEqCoeffs::bell (o, 24.0 * f, 1.3, dB (-0.9)));
                    k.hasLowPass = true; k.lowPass = SvfCoeffs::make (o, 17500.0 * f, 0.55);
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 12.0 * f, 0.6);
                    break;

                case tape30:
                    k.cleanPeak = 0.60f;
                    // 30 ips: more headroom up top, the bump higher and gentler, a wider, cleaner tape
                    k.inputTrim = 0.70f;
                    k.pre[0] = SvfEqCoeffs::highShelf (o, 6000.0 * f, 0.6, 4.0); k.numPre = 1;
                    k.amp = 2; k.ampLimit = 1.05f;
                    post (SvfEqCoeffs::highShelf (o, 6000.0 * f, 0.6, -4.0));
                    post (SvfEqCoeffs::bell (o, 92.0 * f, 1.0, dB (0.9)));
                    post (SvfEqCoeffs::bell (o, 42.0 * f, 1.2, dB (-0.5)));
                    k.hasLowPass = true; k.lowPass = SvfCoeffs::make (o, 22000.0 * f, 0.6);
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 10.0 * f, 0.6);
                    break;

                case valve:
                    k.cleanPeak = 0.13f;
                    // A triode whose bias sags with the programme: rich second harmonic that grows with
                    // the music, a little glue, a softened top
                    k.inputTrim = 0.40f;
                    k.amp = 1; k.ampK = 1.5f; k.ampBias = 0.16f * biasScale;
                    k.sag = 0.25f; k.envAtt = (float) std::exp (-1.0 / (0.010 * o)); k.envRel = (float) std::exp (-1.0 / (0.150 * o));
                    k.flux2R = r (18.0); k.flux2Limit = 0.40f;
                    post (SvfEqCoeffs::bell (o, 220.0 * f, 0.7, dB (0.3)));
                    k.hasLowPass = true; k.lowPass = SvfCoeffs::make (o, 20000.0 * f, 0.5);
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 15.0 * f, 0.6);
                    break;

                case arena:
                    k.cleanPeak = 0.65f;
                    k.colourHeadroom = 1.3f;   // its odd-order curve bites sooner past the clean peak
                    // Made for games: tight low end, a clear upper mid for steps and callouts, clean
                    // peaks - impact without mud
                    k.amp = 3; k.ampLimit = 1.1f; k.inputTrim = 0.55f;
                    post (SvfEqCoeffs::bell (o, 280.0 * f, 0.8, dB (-0.9)));
                    post (SvfEqCoeffs::bell (o, 3200.0 * f, 0.8, dB (1.1)));
                    post (SvfEqCoeffs::highShelf (o, 11000.0 * f, 0.6, dB (0.5)));
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 24.0 * f, 0.7);
                    break;

                case cinema:
                    k.cleanPeak = 0.40f;
                    // Made for films and big games: weight underneath, a smooth top, density from
                    // gentle iron and a warm stage
                    k.inputTrim = 0.70f;
                    k.fluxR = r (12.0); k.fluxLimit = 0.22f;
                    k.amp = 1; k.ampK = 0.8f; k.ampBias = 0.10f * biasScale;
                    post (SvfEqCoeffs::lowShelf (o, 65.0 * f, 0.6, dB (1.0)));
                    post (SvfEqCoeffs::highShelf (o, 9000.0 * f, 0.6, dB (-0.6)));
                    k.hasHighPass = true; k.highPass = SvfCoeffs::make (o, 16.0 * f, 0.6);
                    break;

                default: break;
            }

            // How each model behaves over time, frequency and transients - its own, not a different amount of
            // the same thing:     even   transient thermal coupling hyster. memory bwLoss lowEmph
            struct B { float even, trans, thermal, coupling, hyst, mem, bw, lowDb; };
            static constexpr std::array<B, numModels> behaviour {{
                { 0.000f, 0.05f, 0.000f, 0.00f, 0.0f, 0.0f, 0.10f, 0.0f },   // clean: all but a wire
                { 0.010f, 0.15f, 0.010f, 0.00f, 0.0f, 0.0f, 0.25f, 0.0f },   // british op-amp console: controlled
                { 0.000f, 0.30f, 0.000f, 0.00f, 0.4f, 0.3f, 0.20f, 1.5f },   // american: punchy, iron after it
                { 0.000f, 0.20f, 0.030f, 0.22f, 0.5f, 0.35f, 0.35f, 1.0f },   // vintage class A: iron both ends, rounded peaks
                { 0.020f, 0.40f, 0.000f, 0.00f, 0.0f, 0.0f, 0.50f, 0.0f },   // tape 15: softens peaks progressively
                { 0.015f, 0.30f, 0.000f, 0.00f, 0.0f, 0.0f, 0.30f, 0.0f },   // tape 30: the same, gentler
                { 0.000f, 0.35f, 0.025f, 0.25f, 0.0f, 0.0f, 0.30f, 1.5f },   // valve: soft compression, blocking, heat
                { 0.000f, 0.10f, 0.000f, 0.00f, 0.0f, 0.0f, 0.10f, 0.0f },   // arena: clean, tight
                { 0.000f, 0.25f, 0.020f, 0.10f, 0.4f, 0.4f, 0.30f, 2.0f },   // cinema: weight, iron, a smooth top
            }};
            const auto& b = behaviour[(size_t) std::clamp (model, 0, numModels - 1)];
            k.even = b.even; k.transientDrive = b.trans; k.thermal = b.thermal * biasScale; k.coupling = b.coupling;
            k.hysteresis = b.hyst; k.fluxMemory = b.mem; k.bandwidthLoss = b.bw; k.lowEmphasisDb = b.lowDb;
            if (k.lowEmphasisDb != 0.0f)
            {
                k.emphPre = SvfEqCoeffs::lowShelf (o, 140.0 * f, 0.6, k.lowEmphasisDb);
                k.emphPost = SvfEqCoeffs::lowShelf (o, 140.0 * f, 0.6, -k.lowEmphasisDb);
            }

            k.fluxInvLimit2 = 1.0f / (k.fluxLimit * k.fluxLimit);
            k.fluxBiasSat = k.fluxBias / std::sqrt (1.0f + k.fluxBias * k.fluxBias * k.fluxInvLimit2);
            k.flux2InvLimit2 = 1.0f / (k.flux2Limit * k.flux2Limit);
            return k;
        }

        //==============================================================================
        void delayOnly (float* const* ch, int nc, int n) noexcept
        {
            if (latency <= 0)
                return;
            const int len = (int) dry[0].size();
            for (int c = 0; c < nc; ++c)
            {
                int p = dryPos;
                for (int i = 0; i < n; ++i)
                {
                    const float v = dry[(size_t) c][(size_t) p];
                    dry[(size_t) c][(size_t) p] = ch[c][i];
                    ch[c][i] = v;
                    if (++p == len) p = 0;
                }
            }
            dryPos = (dryPos + n) % len;
        }

        void delayInto (float* const* ch, int nc, int n, std::array<float*, 2>& out) noexcept
        {
            const int len = (int) dry[0].size();
            for (int c = 0; c < nc; ++c)
            {
                int p = dryPos;
                for (int i = 0; i < n; ++i)
                {
                    if (latency > 0)
                    {
                        out[(size_t) c][i] = dry[(size_t) c][(size_t) p];
                        dry[(size_t) c][(size_t) p] = ch[c][i];
                        if (++p == len) p = 0;
                    }
                    else
                    {
                        out[(size_t) c][i] = ch[c][i];
                    }
                }
            }
            if (latency > 0)
                dryPos = (dryPos + n) % len;
        }

        double sr = 48000.0, osr = 192000.0;
        int chans = 2, latency = 0;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        std::array<std::vector<float>, 2> dry, scratchDry;
        int dryPos = 0;

        std::array<std::array<Coeffs, numModels>, 2> coeffs {};
        int designedComponents = -1;
        float rightDrive = 1.0f, dcCoeff = 0.9999f;
        float tAtt = 0.0f, tRelFast = 0.0f, tAttSlow = 0.0f, tRelSlow = 0.0f, heatK = 0.0f, coupleK = 0.0f, fluxPeakK = 0.0f, fluxAttK = 0.0f, bwK = 0.0f;

        std::array<Voice, numVoices> voices {};
        float wet = 0.0f, driveGain = 1.0f;
        double dryLevel = 0.0, wetLevel = 0.0;
        float matchDb = 0.0f, matchGain = 1.0f, dryPeak = 0.0f, wetPeak = 0.0f;
        std::array<float, 2> guardEnv {};
        float guardAtt = 0.0f, guardRel = 0.0f;
        std::array<float, 2> colourEnv {};                 // COLOUR: the music's level, per channel
        float colourAtt = 0.0f, colourRel = 0.0f;
        float colourNow = 5.0f, colourDepth = 0.5f;        // COLOUR, glided; how far toward the sweet spot
        float designedTone = 2.0f;                         // the voicing the coefficients were designed at
        BiquadCoeffs preCoeffs, rlbCoeffs;   // K-weighting, for the loudness match
        std::array<BiquadState, 2> kDryPre {}, kDryRlb {}, kWetPre {}, kWetRlb {};
        float harmonicsDb = -120.0f;
        double residualMs = 0.0, signalMs = 0.0;
    };
}
