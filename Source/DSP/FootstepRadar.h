#pragma once

#include <array>
#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** FOOTSTEP RADAR - finds footsteps in any game, however quiet, and brings them forward: far ones
        get a little room around them and more lift, near ones a small dry lift.

        What a footstep is, whatever the game, the surface or the shoe:
          - an impact: its energy arrives within a few milliseconds and falls away within tens of them
            (a heel strike, often followed 30-90 ms later by the toe);
          - a noise burst, not a tone: its zero crossings are irregular (a kick drum, a voice, a beep and a
            ringing latch are regular);
          - one of a sequence: a walker's steps come at a steady cadence (0.2 - 1 s apart), from the same
            place, with the same sound - the same shoes on the same floor.

        So it listens in six bands (thump 90 Hz, body 250, low-mid 630, click 1.6 k, scuff 3.8 k, air 8 k),
        each against its own background, which is what lets a step 40 dB down under a quiet ambience - or
        one that stands out in a single band under a loud mix - be found. Every onset is an event, judged
        at 50 ms on its attack, decay, tonality, loudness and spread, and against up to four walkers it
        is following. A walker that has been stepping steadily makes its next step expected: around
        that moment the detector listens more closely, so even a very quiet step in a sequence is taken,
        and its lift starts with the step's first millisecond (the audio waits 2.5 ms for the decision).

        Distance: from the step's level, how much of its top end the air has taken, and how much room
        tail follows it. Far steps get more lift and some space (a pre-delayed room, sent from the step
        alone); near steps a small dry lift. The lift is in the step's own bands, so nothing else in
        the mix moves.

        Rejected: sustained or swelling sounds (voices, music), tonal impacts (kicks, beeps, latches),
        rapid repeats of the same sound (automatic fire, rattles), and loud broadband bangs (gunshots,
        explosions).

        Real-time safe: fixed state, no allocation in process().
    */
    class FootstepRadar
    {
    public:
        static constexpr int numBands = 6;
        static constexpr int maxTracks = 4;
        static constexpr int recentCapacity = 32;

        struct Settings
        {
            bool active = false;
            float sensitivity = 6.0f;   // SENSITIVITY 0 .. 10
            float boostDb = 6.0f;       // BOOST 0 .. 12 dB, for a far, quiet step (near ones get less)
            float space = 4.0f;         // SPACE 0 .. 10: how much room a far step is given
            bool solo = false;          // LISTEN: only what the radar adds (to hear what it finds)
            int detection = 0;          // DETECTION method: 0 standard, 1 sensitive, 2 strict
            int room = 0;               // ROOM method: 0 room, 1 hall, 2 open air
        };

        /** A confirmed step (for the display, the meters and the tests). */
        struct Step
        {
            double time = 0.0;          // seconds since prepare / reset (at its onset)
            float pan = 0.0f;           // -1 left .. +1 right
            float rear = 0.0f;          // 0 .. 1: how much it sounds from behind (out of phase)
            float distance = 0.0f;      // 0 near .. 1 far
            float confidence = 0.0f;    // 0 .. 1
            float boostDb = 0.0f;       // the lift it was given
            float levelDb = -120.0f;    // its peak (dBFS)
            int track = -1;             // the walker it belongs to
        };

        /** A walker being followed. */
        struct Track
        {
            bool active = false;
            std::array<float, numBands> print {};   // its sound: excess over background per band, normalised
            float pan = 0.0f, rear = 0.0f, distance = 0.0f;
            float period = 0.0f;                    // seconds between steps (0 = not known yet)
            float confidence = 0.0f;
            float loudestDb = -120.0f;
            double last = -10.0;
            int steps = 0, id = 0;
        };

        /** Every event and what was decided (tests; filled only when `log` is set). */
        struct Decision
        {
            double time;
            bool accepted;
            float probability, base, match, attackMs, decayDb, excessDb, tonal, levelDb, spread, distance;
            float durationMs, body, rate, music;
            float ring = 1.0f, bang = 1.0f, rapid = 1.0f, narrow = 1.0f, impact = 0.0f;   // the factors, for tuning
            std::array<float, 6> bandAtt {}, bandDec {}, bandP {}, bandTon {}, bandImp {};
            float grid = 0.0f;
            int withdrawn = 0;   // 1 sustained, 2 a kick (at the 130 ms check)
        };
        std::vector<Decision>* log = nullptr;

        void prepare (double sampleRate, int maxBlock);
        void reset();
        /** key: what to listen to (the rack's input, with its dynamics as they came), or null for the audio itself. */
        void process (float* const* ch, int numChannels, int numSamples, const Settings&, const float* const* key = nullptr) noexcept;

        int getLatencySamples() const noexcept { return lookahead; }

        // --- readouts ------------------------------------------------------------------------------
        float getActivity() const noexcept                         { return activity; }     // 0 .. 1: a step is being lifted now
        int getAcceptedCount() const noexcept                      { return accepted; }
        int getEventCount() const noexcept                         { return events; }
        const std::array<Track, maxTracks>& getTracks() const noexcept { return tracks; }
        const std::array<float, numBands>& getBandExcessDb() const noexcept { return excessNow; }
        float getOnsetStrength() const noexcept                    { return onsetNow; }
        float getThreshold() const noexcept                        { return thresholdNow; }
        /** Confirmed steps: a ring of the last recentCapacity; `stepsTotal` counts every one ever. */
        const std::array<Step, recentCapacity>& getRecent() const noexcept { return recent; }
        int getStepsTotal() const noexcept                         { return stepsTotal; }
        double getClock() const noexcept                           { return clock; }
        float getMusicality() const noexcept                       { return musicality; }
        float getPeriodicity() const noexcept                      { return periodicity; }
        const std::array<float, numBands>& getBackgroundDb() const noexcept { return background; }
        int getPhase() const noexcept { return newest >= 0 ? (int) ev[(size_t) newest].phase : 0; }
        double getLastOnset() const noexcept { return lastOnset; }
        const std::array<float, numBands>& getFlickerDb() const noexcept { return deviation; }

        static constexpr std::array<float, numBands> bandHz { 90.0f, 250.0f, 630.0f, 1600.0f, 3800.0f, 8000.0f };

    private:
        void controlTick (const Settings&) noexcept;
        struct Event;
        void startEvent (Event&, const Settings&) noexcept;
        void decide (Event&, const Settings&) noexcept;
        void confirm (Event&, bool early = false) noexcept;
        void release (Event&) noexcept;
        float trackMatch (const Track&, const Event&, double t, int& expectedK) const noexcept;
        float expectedNow (double t) const noexcept;   // 0..1: a confident walker is due to step now
        void setLift (Event&, float boostDb, float send) noexcept;

        double sr = 48000.0;
        int lookahead = 120, tickInterval = 48, tickCountdown = 48;
        float tickDt = 0.001f;
        double clock = 0.0;

        // Analysis (undelayed)
        std::array<SvfCoeffs, numBands> bandCoeffs {};
        std::array<std::array<SvfState, numBands>, 2> analysis {};
        // Three envelopes of each band's power: fast (onsets: up in 0.5 ms, down over 12), steady (levels,
        // background, the step's print: 6 ms either way - noise flickers less through it) and short (how
        // long the impact really lasts: down over 1.5 ms)
        std::array<float, numBands> fast {}, med {}, shortEnv {}, tickPeak {}, tickShort {}, accL {}, accR {}, accX {};
        float fastAtk = 0.0f, fastRel = 0.0f, medK = 0.0f, shortAtk = 0.0f, shortRel = 0.0f;
        std::array<float, numBands> shortDb {};
        float tickInPeak = 0.0f;
        int tickSamples = 0;

        // Zero crossings (tonality) in the four lower bands, while an event is being watched
        static constexpr int zcBands = 4;
        std::array<float, zcBands> zcPrev {};
        std::array<long long, zcBands> zcLast {};
        std::array<long long, zcBands> zcPrevInterval {};
        std::array<double, zcBands> winSum {}, winSq {};    // the same, over 50 ms windows between events (music)
        std::array<int, zcBands> winCount {};
        int winTicks = 0;
        // Pitch in the low end (music, voices): the low bands summed, averaged down to ~4 kHz, and every
        // 50 ms the strongest normalised autocorrelation at 50 - 500 Hz
        static constexpr int pitchLength = 192;
        std::array<float, pitchLength> pitchBuf {};
        int pitchPos = 0, pitchDecim = 12, pitchCount = 0;
        float pitchAcc = 0.0f;
        float periodicity = 0.0f;
        long long sampleCounter = 0;

        // Control rate (1 kHz)
        static constexpr int historyTicks = 8;
        std::array<std::array<float, historyTicks>, numBands> levelHistory {};
        int historyPos = 0;
        std::array<float, numBands> levelDb {}, background {}, deviation {}, excessNow {};
        // The background by minimum statistics: each band's 50 ms windows over the last 2 s; the background
        // is their 20th percentile (steps can fill most of the windows and not move it), the flicker margin
        // is measured in the quiet windows only
        static constexpr int bgWindows = 40, bgWindowTicks = 50;
        std::array<std::array<float, bgWindows>, numBands> windowMean {}, windowFlicker {};
        std::array<double, numBands> windowPower {};
        std::array<float, numBands> windowJitter {}, lastTickDb {};
        int windowTick = 0, windowPos = 0, windowsFilled = 0;
        void updateBackground() noexcept;
        bool weakSoFar (const Event&) const noexcept;
        float pitchStrength (double& energy) const noexcept;
        float onsetNow = 0.0f, thresholdNow = 0.0f;
        float musicality = 0.0f;           // sustained tonal content between events (music): 0 .. 1
        double lastOnset = -10.0;
        // How busy the scene is: the onsets of the last 3 s (rain, rattles, gravel under many feet)
        static constexpr int onsetMemory = 64;
        std::array<double, onsetMemory> onsetTimes {};
        int onsetHead = 0;
        float onsetRate() const noexcept;

        // The events being watched: several at once, so one walker's step landing while another's is still
        // being judged (two walkers, a heel under a syllable) gets its own judgement
        enum class Phase { idle, watching, lifted, done };
        static constexpr int preTicks = 32;                                  // the fast envelope's last 32 ms, always (rise times)
        std::array<std::array<float, preTicks>, numBands> preShort {};       // (fast envelope: up at once, down over 12 ms -
                                                                             //  it follows a swell, and ignores a buzz's flicker)
        int prePos = 0;
        static constexpr int eventTicks = 64;                                // the first 64 ms, per band
        struct Event
        {
            Phase phase = Phase::idle;
            double eventStart = -10.0;
            float eventOnset = 0.0f, provisional = 0.0f, eventProbability = 0.0f;
            std::array<float, numBands> eventPeak {}, eventBase {}, eventAt45 {};
            std::array<int, numBands> eventAboveTicks {};     // ticks within 10 dB of the band's peak: how long the impact lasts
            std::array<float, numBands> eventShortPeak {}, eventShortAt45 {};
            std::array<std::array<float, eventTicks>, numBands> eventFastHistory {};
            std::array<float, numBands> eventFastPeak {};
            std::array<std::array<float, preTicks>, numBands> eventPre {};    // the fast envelope as it was when the event began
            std::array<std::array<float, eventTicks>, numBands> eventShortHistory {};
            int eventTickCount = 0;
            std::array<double, numBands> eventPeakTime {};
            double eventL = 0.0, eventR = 0.0, eventX = 0.0;
            float eventInPeak = 0.0f, eventTailSum = 0.0f;
            int eventTailTicks = 0, eventBestTrack = -1, eventExpectedK = 1, eventBestBand = 0;
            float eventMatch = 0.0f, eventBaseScore = 0.0f, eventDistance = 0.0f, eventPan = 0.0f, eventRear = 0.0f;
            bool eventSustained = false;
            float rhythmPeriod = 0.0f;                   // accepted on its rhythm: the period of the faint steps before it
            std::array<float, numBands> eventBefore {};      // each band's level just before the event (dB)
            std::array<double, zcBands> zcSum {}, zcSq {}, zcJitter {};
            std::array<int, zcBands> zcCount {};
            // Its lift (the lift heard is the most any event asks of each band)
            std::array<float, numBands> liftTarget {};
            float sendTarget = 0.0f, holdS = 0.0f;
        };
        // Faint events that were not taken on their own: three alike in a steady rhythm are a walker
        // (track before detect - how a buried walk is found)
        struct Faint { double time = -100.0; std::array<float, numBands> print {}; float pan = 0.0f; };
        static constexpr int faintMemory = 12;
        std::array<Faint, faintMemory> faint {};
        int faintHead = 0;
        float rhythmOf (const Event&, const std::array<float, numBands>& print) const noexcept;   // its period, or 0
        static constexpr int maxEvents = 4;
        std::array<Event, maxEvents> ev {};
        int newest = -1;                    // the event that began last
        double lastEventTime = -10.0;
        float lastEventPan = 0.0f;
        bool lastEventBody = false;
        int lastEventTrack = -1;
        float lastEventGrid = 0.0f;   // how surely the last impact-like event was a drum machine's
        // Every event's time and print (the last 8 s or so): a drum machine plays on a grid exact to the
        // sample; footsteps, set off by animations and the game's audio blocks, wobble by milliseconds
        struct Heard { double time = -100.0; std::array<float, numBands> print {}; };
        static constexpr int heardMemory = 64;
        std::array<Heard, heardMemory> heard {};
        int heardHead = 0;
        float machineGrid (const Event&, const std::array<float, numBands>& print) const noexcept;   // 0..1
        std::array<float, numBands> riseNow {};          // each band's rise over the last 6 ms (as the onset counts it)
        std::array<float, numBands> lastEventPrint {};   // (of the last impact-like event)
        // Zero-crossing intervals of this tick (added into every event being watched)
        std::array<double, zcBands> tickZcSum {}, tickZcSq {}, tickZcJitter {};
        std::array<int, zcBands> tickZcCount {};
        bool anyWatching = false, anyActive = false;

        // Walkers
        std::array<Track, maxTracks> tracks {};
        int nextTrackId = 1;

        // Lift (control targets, glided per sample)
        std::array<float, numBands> liftTarget {}, lift {};
        float sendTarget = 0.0f, send = 0.0f, glide = 0.0f;
        float activity = 0.0f;
        void combineLifts() noexcept;

        // Audio path: the lookahead delay, the lift's band filters, the room
        std::array<std::vector<float>, 2> delay;
        int delayPos = 0;
        std::array<std::array<SvfState, numBands>, 2> liftFilters {};

        struct Room
        {
            static constexpr int lines = 4;
            std::array<std::vector<float>, lines> line;
            std::array<int, lines> length {}, pos {};
            std::array<float, lines> damp {};
            std::vector<float> pre;
            int prePos = 0, preSamples = 0;
            float feedback = 0.6f, dampK = 0.3f;
            int designedRoom = -1;
        } room;
        void designRoom (int kind) noexcept;
        inline void roomProcess (float in, float& outL, float& outR) noexcept;

        // Readouts
        int accepted = 0, events = 0, stepsTotal = 0;
        std::array<Step, recentCapacity> recent {};
    };
}
