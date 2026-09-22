#pragma once

#include <array>
#include <string_view>

namespace enh::dsp::methods
{
    /** Every swappable processing method, in one place. This table drives the glass panel's dropdowns
        and hover tooltips, and `EnhDspTests --methods-doc`, which writes Vault/Reference/Methods.md
        (a test fails if that file is out of date).

        A stage is one step of a unit's processing (how it measures, how it calculates the gain, how
        it smooths it) or one knob's own processing. Its methods are stored as a choice parameter
        (`param`, not automatable) whose choice index is the method's `index`; the first method is the
        default and is what the unit did before methods existed, so old sessions sound the same.

        Rules every method keeps (checked by EnhDspTests): zero change to the reported latency; stable,
        finite and under full scale at every sample rate and block size; a switch crossfades (30 ms). */

    struct Method
    {
        std::string_view shortName;   // 3 letters, printed in the dropdown
        std::string_view fullName;    // what the letters stand for
        std::string_view measures;    // what it looks at / does
        std::string_view sound;       // how the sound changes
        std::string_view cost;        // CPU and latency
    };

    struct Stage
    {
        std::string_view unit;        // the unit's name as printed on the rack
        std::string_view name;        // the dropdown's title
        std::string_view question;    // what the stage answers, under the title
        std::string_view param;       // the choice parameter holding the method ("" = fixed, one method)
        const Method* methods;
        int numMethods;
    };

    // --- ADAPTIVE COMPRESSOR ---------------------------------------------------------------------------
    inline constexpr std::array<Method, 2> compressorDetector {{
        { "PKR", "Peak or RMS",
          "The side-chain's instantaneous peak or its RMS (+3 dB, so a steady sine reads the same either way), "
          "whichever is higher.",
          "Catches transients as well as sustained loudness: drum hits and gunshots are held in firmly. "
          "The compressor's original behaviour.",
          "Zero latency. No extra CPU (default)." },
        { "RMS", "Root mean square",
          "Only the side-chain's power, averaged over 50 ms. Short peaks barely register.",
          "Reacts to loudness, not to transients: attacks pass through fuller and punchier (about a fifth "
          "less reduction on a hit), and the reduction lingers a little longer after it. Softer on drums, "
          "less peak control.",
          "Zero latency. One multiply-add per sample more than PKR." },
    }};

    inline constexpr std::array<Method, 1> compressorGain {{
        { "ADT", "Adaptive threshold",
          "Threshold, ratio and knee from the programme: the loud part of the last seconds, crest factor, "
          "transient density and spectral tilt. Peaky material gets a higher threshold and a gentler ratio.",
          "Compression sits under the music rather than under a fixed number, so it holds the same "
          "character on quiet and loud material.",
          "Zero latency. Updated at 1.5 kHz." },
    }};

    inline constexpr std::array<Method, 2> compressorSmoothing {{
        { "DRL", "Dual release",
          "Two followers: a slow one carries the average gain reduction, a fast one takes only what a "
          "transient needs beyond it and gives it back in about 50 ms.",
          "A kick dips the mix for a moment instead of pulling the sustained parts down and letting them "
          "swell back: about 30 % less pumping than SRL at the same average reduction.",
          "Zero latency. No extra CPU (default)." },
        { "SRL", "Single release",
          "One attack / release follower on the target gain, with the programme-dependent release.",
          "The classic bus-compressor movement: more audible breathing and glue, sustained parts pump "
          "with the kick.",
          "Zero latency. Slightly less CPU than DRL." },
    }};

    // RESPONSE knob: how the knob's travel maps to the speed and depth it sets
    inline constexpr std::array<Method, 2> compressorResponseLaw {{
        { "LIN", "Linear",
          "The knob's travel maps straight to RESPONSE: 5 is half way.",
          "The original feel.",
          "No cost (default)." },
        { "EXP", "Exponential",
          "More of the knob's travel is spent at the slow, gentle end; the last third goes quickly to fast.",
          "Finer control over slow, smooth compression; the same range overall.",
          "No cost." },
    }};

    inline constexpr std::array<Stage, 4> compressorStages {{
        { "ADAPTIVE COMPRESSOR", "DETECTOR", "How it measures the level", "tideDetector",
          compressorDetector.data(), (int) compressorDetector.size() },
        { "ADAPTIVE COMPRESSOR", "GAIN", "How it calculates the reduction", "",
          compressorGain.data(), (int) compressorGain.size() },
        { "ADAPTIVE COMPRESSOR", "SMOOTHING", "How the reduction moves", "tideSmoothing",
          compressorSmoothing.data(), (int) compressorSmoothing.size() },
        { "ADAPTIVE COMPRESSOR", "RESPONSE LAW", "How the RESPONSE knob maps", "tideResponseLaw",
          compressorResponseLaw.data(), (int) compressorResponseLaw.size() },
    }};

    // --- knob modifiers ---------------------------------------------------------------------------------
    /** A modifier sits on a knob's input side (between the knob and its processing) or output side. It is
        stored in the plugin state, not as a parameter: the host always sees the knob's raw value. */
    struct Modifier
    {
        std::string_view shortName, fullName, does, sound, cost;
        std::array<float, 4> choices;         // the dropdown's values; choices[0] = off
        std::string_view unit;
    };

    inline constexpr Modifier smoothingModifier {
        "SMO", "Smoothing",
        "Glides the knob's value toward where it was set, one-pole, with this time constant. The same at "
        "every sample rate.",
        "Automation and fast knob moves turn into gradual changes instead of steps.",
        "No latency on the audio; the knob's effect lags by about the time constant.",
        { 0.0f, 50.0f, 250.0f, 1000.0f }, "ms" };

    /** The knobs that have modifiers so far (the pilot): param ID, and which modifiers each side has. */
    struct KnobModifiers { std::string_view param; const Modifier* input; };
    inline constexpr std::array<KnobModifiers, 1> knobModifiers {{
        { "tideResponse", &smoothingModifier },
    }};

    // --- lookup -----------------------------------------------------------------------------------------
    /** The stages of a unit (by its layout::Unit index), or an empty span. Only the compressor so far. */
    inline constexpr int compressorUnitIndex = 2;   // layout::tideUnit

    struct StageList { const Stage* stages; int count; };
    inline constexpr StageList stagesForUnit (int unit) noexcept
    {
        return unit == compressorUnitIndex ? StageList { compressorStages.data(), (int) compressorStages.size() }
                                           : StageList { nullptr, 0 };
    }
}
