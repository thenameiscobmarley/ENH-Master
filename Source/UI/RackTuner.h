#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <random>
#include <vector>
#include "../DSP/UnitMask.h"
#include "../Tuner/TunerCore.h"

class PluginProcessor;

/*  RACK TUNER: the whole rack tuned from a few words - "warm punchy hip-hop master", "huge dreamy space" -
    with no AI and no network. Three parts:

      - the words: a dictionary (RackTuner.cpp) turns each into twelve tags (WARM BRIGHT DEEP PUNCH LOUD SPACE
        WIDE DIRTY SMOOTH VINTAGE MOTION CLARITY); "very", "a little", "no" and "not" bend the next word;
      - the bank: 60,000 settings ("vendors") for the newer units and the LUNCHBOX's modules, made once from
        a fixed seed out of what each unit and knob does (TunerBank.inc, Tools/units/gen_tuner.py) - each has
        its own tags;
      - the tune: every unit's settings scored against the words, one picked per unit - the best at VARIETY 0,
        more and more freely above it (a fresh random draw every time, so the same words give a different
        rack each time), AMOUNT how far from the knobs' defaults it goes. The faceplate's switches say what
        it may touch: CHANGE KNOBS, UNITS ON/OFF, SWAP UNITS (in and out of the locker), LEVEL MATCH.

    Safe on the ears and easy to take back: the knobs glide there over a second; LEVEL MATCH keeps the rack
    as loud as it was (DSP/TunerMatch.h); every tune goes on a history (UNDO); A/B flips between the rack
    before the tune and after it. The screen: the 60,000 as small cubes, the chosen ones lit (ColourScreens). */
namespace pad
{
    class ParameterBridge;

    namespace tuner
    {
        // (the words, the bank and the choosing: Tuner/TunerCore.h)
        /** Suggestions for the glass panel's chips. */
        const juce::StringArray& chipWords();

        /** The bank (built once, on first use, from a fixed seed: the same 60,000 on every machine). */
        int bankSize() noexcept;
        /** Where vendor i's cube sits in the cloud (-1 .. 1), and its colour's hue (0 .. 1). */
        void cubeOf (int i, float& x, float& y, float& z, float& hue) noexcept;
        /** Which unit (TunerBank's units[]) vendor i belongs to. */
        int unitOfVendor (int i) noexcept;

        /** What the screen shows (written on the message thread, read by the renderer). */
        struct View
        {
            static constexpr int maxChosen = 96;
            std::atomic<int> numChosen { 0 };
            std::array<std::atomic<int>, maxChosen> chosen {};
            std::atomic<double> tunedAt { -100.0 };   // juce::Time::getMillisecondCounterHiRes() / 1000
            std::atomic<int> tunes { 0 };
        };
        View& view() noexcept;
    }

    class RackTuner
    {
    public:
        RackTuner (ParameterBridge&, PluginProcessor&);

        void setWords (const juce::String& w)    { words = w.substring (0, 80); }
        const juce::String& getWords() const     { return words; }
        /** Tunes the rack to the words; returns what it did, in a sentence or two. */
        juce::String tune();
        /** The last tune taken back (the one before it restored). */
        juce::String undo();
        /** A/B: true = the rack before the last tune, false = after it. */
        void showBefore (bool before);
        bool hasHistory() const noexcept          { return ! history.empty(); }
        const juce::String& lastReport() const   { return report; }

        /** 30 times a second: the glide, and the faceplate's TUNE / UNDO / A/B switches. */
        void tick (float dt);

        /** Tests: tune with a fixed seed. */
        void setSeedForTests (unsigned s)         { rng.seed (s); }

    private:
        struct Value { int index; float normalised; };
        struct Snapshot { std::vector<Value> values; enh::dsp::UnitMask stored {}; std::uint32_t lbStored = 0; };
        Snapshot capture() const;
        void glideTo (const Snapshot&, float seconds);
        float knob (const char* id) const;       // a tuner control's normalised value
        void setParam (int index, float v);

        ParameterBridge& bridge;
        PluginProcessor& processor;
        juce::String words, report;
        std::mt19937 rng;
        std::vector<std::pair<Snapshot, Snapshot>> history;   // (before, after) of each tune
        bool showingBefore = false;

        struct Glide { int index; float from, to; bool stepped, powerOn, powerOff; };
        std::vector<Glide> glide;
        float glideT = 1.0f, glideLen = 1.0f;
        bool glideLoadsStored = false;
        Snapshot glideTarget;
        bool lastAB = false;
    };
}
