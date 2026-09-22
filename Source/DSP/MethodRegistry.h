#pragma once

#include <array>
#include <string_view>

namespace enh::dsp::methods
{
    /** Every swappable processing method and every knob modifier, in one place. This table drives the
        glass panel (its categories, dropdowns and the text under the pointer), the choice parameters
        (ParameterSpecs builds them from here) and `EnhDspTests --methods-doc`, which writes
        Vault/Reference/Methods.md (a test fails when that page is out of date).

        A stage is one step of a unit's processing, a knob's own processing (its law), an output
        setting or a display setting. Its methods are stored in a choice parameter (`param`, not
        automatable) whose index is the method's position; the first method is the default and is what
        the unit did before methods existed, so old sessions sound exactly the same.

        Rules every method keeps (checked by EnhDspTests --methods): no change to the reported latency;
        stable, finite and under full scale at every sample rate and block size; switching never steps
        the audio (audio-rate methods crossfade over 30 ms, control-rate ones glide through the unit's
        own smoothing). */

    struct Method
    {
        std::string_view shortName;   // 2-3 characters, printed in the panel
        std::string_view fullName;    // what they stand for
        std::string_view measures;    // what it looks at / does
        std::string_view sound;       // how the sound (or the display) changes
        std::string_view cost;        // CPU and latency
    };

    /** Every stage with a parameter, so the DSP can address them by number (KnobValues::methods). */
    enum MethodId
    {
        tideDetector, tideSideChain, tideGain, tideSmoothing, tideMakeup, tideResponseLaw,
        limiterNormal, limiterWidth, limiterKeeper,
        balancerReference, balancerDeadZone, balancerLifts, balancerGuard, balancerKeeper,
        levelerLift, levelerGate, levelerBalance,
        enhancerHarmonics,
        seraphTape, seraphPreDelay, seraphWindow,
        levelGlide,
        outputCeiling,
        displayToneRange, displayDuckHold, displayWaveform,
        numMethodIds,
        fixedStage = -1
    };

    struct Stage
    {
        std::string_view unit;        // the unit's name as printed on the rack
        int unitIndex;                // layout::Unit
        std::string_view category;    // PROCESSING, KNOBS, OUTPUT or DISPLAY
        std::string_view name;        // the setting's title
        std::string_view question;    // what it answers
        std::string_view param;       // the choice parameter ("" = fixed, one method)
        int id;                       // MethodId, or fixedStage
        std::string_view knobParam;   // a knob's own processing: the knob it belongs to ("" = the unit's)
        const Method* methods;
        int numMethods;
    };

    //==================================================================================================
    // ADAPTIVE COMPRESSOR
    inline constexpr std::array<Method, 3> tideDetectorMethods {{
        { "PKR", "Peak or RMS",
          "The side-chain's instantaneous peak or its RMS (+3 dB, so a steady sine reads the same either way), whichever is higher.",
          "Catches transients as well as sustained loudness: drum hits and gunshots are held in firmly. The original behaviour.",
          "Zero latency. No extra CPU (default)." },
        { "RMS", "Root mean square",
          "Only the side-chain's power, averaged over 50 ms. Short peaks barely register.",
          "Reacts to loudness, not to transients: attacks pass through fuller and punchier (about a fifth less reduction on a hit), and the reduction lingers a little longer after it. Softer on drums, less peak control.",
          "Zero latency. One multiply-add per sample more than PKR." },
        { "KWT", "K-weighted",
          "RMS over 50 ms of the side-chain lifted 4 dB above 1.5 kHz, the way a loudness meter hears it.",
          "Compresses what sounds loud rather than what measures loud: bright, harsh passages are held a little more, dull heavy ones a little less.",
          "Zero latency. One filter per sample more than PKR." },
    }};
    inline constexpr std::array<Method, 3> tideSideChainMethods {{
        { "H90", "High-pass 90 Hz",
          "The detector listens through a 90 Hz, 12 dB/oct high-pass, as engineers set a bus compressor's side-chain filter.",
          "Steady bass does not drive the gain reduction, so it cannot pull the whole mix down and let it swell back. The original setting.",
          "Zero latency (default)." },
        { "H15", "High-pass 150 Hz",
          "The same filter, nearly an octave higher.",
          "Even kick drums and bass-heavy hits barely move it: the mix keeps its low-end punch, the compressor rides the mids and highs.",
          "Zero latency, same CPU. Crossfades over 30 ms." },
        { "FUL", "Full range",
          "No side-chain filter: the detector hears everything.",
          "Bass counts in full: big low end is controlled, at the price of more audible pumping on bass-heavy material.",
          "Zero latency, slightly less CPU. Crossfades over 30 ms." },
    }};
    inline constexpr std::array<Method, 3> tideGainMethods {{
        { "ADT", "Adaptive threshold",
          "Threshold, ratio and knee from the programme: the loud part of the last seconds, crest factor, transient density and spectral tilt.",
          "Compression sits under the music rather than under a fixed number, so it holds the same character on quiet and loud material. The original computer.",
          "Zero latency. Updated at 1.5 kHz (default)." },
        { "SFT", "Soft",
          "The same adaptive threshold, with 60 % of the ratio and a knee 6 dB wider.",
          "Gentler, more transparent levelling: less reduction, and it comes in gradually. Good for music.",
          "Zero latency, same CPU. Crossfades over 30 ms." },
        { "HRD", "Hard",
          "The same adaptive threshold, with 1.5 times the ratio and a third of the knee.",
          "Firm, obvious control: loud moments are clamped, the level stays tightly even. Good for voice and competitive play.",
          "Zero latency, same CPU. Crossfades over 30 ms." },
    }};
    inline constexpr std::array<Method, 3> tideSmoothingMethods {{
        { "DRL", "Dual release",
          "Two followers: a slow one carries the average gain reduction, a fast one takes only what a transient needs beyond it and gives it back in about 50 ms.",
          "A kick dips the mix for a moment instead of pulling the sustained parts down and letting them swell back: about 30 % less pumping than SRL at the same average reduction. The original.",
          "Zero latency. No extra CPU (default)." },
        { "SRL", "Single release",
          "One attack / release follower on the target gain, with the programme-dependent release.",
          "The classic bus-compressor movement: more audible breathing and glue, sustained parts pump with the kick.",
          "Zero latency. Slightly less CPU than DRL. Crossfades over 30 ms." },
        { "OPT", "Opto",
          "One follower whose release slows as the reduction deepens, like an optical cell: quick from light reduction, slow from deep.",
          "Smooth and musical: small moves recover quickly, big ones fade back gently, so heavy moments never snap back.",
          "Zero latency, same CPU as SRL. Crossfades over 30 ms." },
    }};
    inline constexpr std::array<Method, 3> tideMakeupMethods {{
        { "AUT", "Auto 65 %",
          "Gives back 65 % of the average gain reduction, slowly, so MIX is roughly level-matched.",
          "The original balance: compressed and louder-sounding without jumping in level.",
          "No cost (default)." },
        { "FUL", "Full 90 %",
          "Gives back 90 % of the average reduction.",
          "Denser and louder: quiet detail comes up further. The output limiter catches anything that would go over.",
          "No cost. Glides over seconds." },
        { "OFF", "None",
          "No make-up gain: the reduction is heard as it is.",
          "The mix gets quieter where it is compressed. Useful for judging what the compressor really does.",
          "No cost. Glides over seconds." },
    }};
    inline constexpr std::array<Method, 3> tideResponseLawMethods {{
        { "LIN", "Linear",
          "The knob's travel maps straight to RESPONSE: 5 is half way.",
          "The original feel.",
          "No cost (default)." },
        { "EXP", "Exponential",
          "More of the knob's travel is spent at the slow, gentle end; the last third goes quickly to fast.",
          "Finer control over slow, smooth compression; the same range overall.",
          "No cost." },
        { "LOG", "Logarithmic",
          "More of the travel is spent at the fast end; the first third goes quickly from slow.",
          "Finer control over fast, aggressive settings; the same range overall.",
          "No cost." },
    }};

    //==================================================================================================
    // SPECTRAL LIMITER
    inline constexpr std::array<Method, 3> limiterNormalMethods {{
        { "P97", "97th percentile",
          "A band's normal is how far above its baseline it went 97 % of the time over the last 10 s.",
          "Kick drums and regular hits are learnt as normal and left alone; only real outliers are cut. The original.",
          "Control rate, no audio cost (default)." },
        { "P90", "90th percentile",
          "Normal is where the band stayed 90 % of the time: a lower bar.",
          "Stricter: more events count as abnormal and are cut, so the balance is held tighter, with more movement.",
          "Control rate, no audio cost." },
        { "P99", "99th percentile",
          "Normal is where the band stayed 99 % of the time: only the very rarest peaks count.",
          "Looser: it steps in only for truly unusual events, and leaves dynamic material almost untouched.",
          "Control rate, no audio cost." },
    }};
    inline constexpr std::array<Method, 3> limiterWidthMethods {{
        { "STD", "Region width",
          "Each cut is as wide as the run of bands that jumped out.",
          "The original: a cut covers the offending region and not much more.",
          "Control rate, no audio cost (default)." },
        { "NAR", "Narrow",
          "Cuts 60 % as wide.",
          "Surgical: less of the surrounding material moves, but a broad event may be only partly caught.",
          "Control rate, no audio cost. Glides." },
        { "WID", "Wide",
          "Cuts 1.6 times as wide.",
          "Smoother and more forgiving: a broad event is caught whole, at the price of touching more of its neighbours.",
          "Control rate, no audio cost. Glides." },
    }};
    inline constexpr std::array<Method, 3> keeperMethods {{
        { "K60", "Keep 60 %",
          "Where a cut takes a band below its usual level, 60 % of the ear-weighted loss is given back to the whole mix (at most 3 dB, within the headroom).",
          "Heavy cuts no longer make the rest of the mix sound quieter. The original setting.",
          "Control rate, no audio cost (default)." },
        { "K90", "Keep 90 %",
          "Gives back 90 % of that loss.",
          "Holds the loudness almost exactly through a cut; the lift can be a little more noticeable.",
          "Control rate, no audio cost. Glides." },
        { "OFF", "Off",
          "No make-up for cuts.",
          "Cuts are heard as they are: the mix dips a little while a region is held down.",
          "No cost. Glides." },
    }};

    //==================================================================================================
    // MIX BALANCER
    inline constexpr std::array<Method, 2> balancerReferenceMethods {{
        { "MED", "Median",
          "A band's jump is measured against the median move of all the bands.",
          "One or two bands jumping never shift the reference, so exactly those are corrected. The original.",
          "Control rate, no audio cost (default)." },
        { "AVG", "Average",
          "Against the average move of all the bands.",
          "Every band pulls the reference a little: corrections are smaller and spread wider, a gentler overall balance.",
          "Control rate, no audio cost. Glides." },
    }};
    inline constexpr std::array<Method, 3> balancerDeadZoneMethods {{
        { "STD", "1.5 dB",
          "Moves smaller than 1.5 dB beyond the mix's own are taken as the programme breathing and left alone.",
          "The original: it only rides real imbalances.",
          "Control rate, no audio cost (default)." },
        { "TGT", "Tight 0.75 dB",
          "A 0.75 dB dead zone.",
          "Rides smaller imbalances too: a steadier tonal balance, more constant movement.",
          "Control rate, no audio cost. Glides." },
        { "LSE", "Loose 3 dB",
          "A 3 dB dead zone.",
          "Steps in only for obvious imbalances: the most natural, the least correction.",
          "Control rate, no audio cost. Glides." },
    }};
    inline constexpr std::array<Method, 3> balancerLiftsMethods {{
        { "HLF", "Half range",
          "A band that drops out is lifted, to half of RANGE, more gently than cuts.",
          "The original: holes are partly filled, never overdone.",
          "Control rate, no audio cost (default)." },
        { "NON", "Cuts only",
          "Bands are only ever cut.",
          "Nothing is ever boosted: the safest, cleanest balance, but holes stay holes.",
          "Control rate, no audio cost. Glides." },
        { "FUL", "Full range",
          "Lifts go as far as cuts, to the full RANGE.",
          "Missing regions are filled in fully: a fuller, more even sound, with more movement.",
          "Control rate, no audio cost. Glides." },
    }};
    inline constexpr std::array<Method, 3> balancerGuardMethods {{
        { "STD", "Guard attacks",
          "A band in a fresh transient (its fast level 2.5 times its average) is not cut yet.",
          "Footsteps and gunshots keep their front edge. The original.",
          "Control rate, no audio cost (default)." },
        { "STR", "Strong guard",
          "The guard trips at 1.8 times the average: more attacks are protected.",
          "Punchier transients; a harsh burst gets a moment longer before it is caught.",
          "Control rate, no audio cost. Glides." },
        { "OFF", "No guard",
          "Attacks are cut like anything else.",
          "The tightest control of sudden bursts, at the price of softened attacks.",
          "Control rate, no audio cost. Glides." },
    }};

    //==================================================================================================
    // UPWARD LEVELER
    inline constexpr std::array<Method, 3> levelerLiftMethods {{
        { "STD", "Standard lift",
          "The most each band may lift: 9 dB low, 18 dB mid, 14 dB high.",
          "The original amount of detail recovery.",
          "Control rate, no audio cost (default)." },
        { "GEN", "Gentle",
          "60 % of those limits.",
          "Quiet detail comes up less: more natural dynamics, less noise brought forward.",
          "Control rate, no audio cost. Glides (slew-limited)." },
        { "BIG", "Big",
          "1.3 times those limits.",
          "The quietest detail comes right up: maximum audibility, flatter dynamics.",
          "Control rate, no audio cost. Glides (slew-limited)." },
    }};
    inline constexpr std::array<Method, 3> levelerGateMethods {{
        { "STD", "Gate -58 dBFS",
          "Nothing below about -58 dBFS, or without enough movement in it, is lifted: hiss and room tone stay down.",
          "The original balance between detail and noise.",
          "Control rate, no audio cost (default)." },
        { "SNS", "Sensitive -66",
          "The gate opens 8 dB lower.",
          "Even fainter sounds are lifted; in a noisy source more of the noise comes up too.",
          "Control rate, no audio cost. Glides (slew-limited)." },
        { "STR", "Strict -50",
          "The gate opens 8 dB higher.",
          "Only clearly audible material is lifted: the cleanest, for noisy sources.",
          "Control rate, no audio cost. Glides (slew-limited)." },
    }};
    inline constexpr std::array<Method, 3> levelerBalanceMethods {{
        { "STD", "Voiced",
          "Each band aims a little differently: low 4 dB under the target, high 1.5 dB under.",
          "Lifts favour the midrange, where detail and footsteps live. The original.",
          "Control rate, no audio cost (default)." },
        { "MID", "Mid focus",
          "Low 7 dB under, high 3 dB under.",
          "Even more of the lift goes to the midrange: voices and steps forward, boom and hiss left alone.",
          "Control rate, no audio cost. Glides (slew-limited)." },
        { "FLT", "Flat",
          "Every band aims at the target itself.",
          "Lows and highs come up as much as the mids: fuller and brighter, less focused.",
          "Control rate, no audio cost. Glides (slew-limited)." },
    }};

    //==================================================================================================
    // ADAPTIVE ENHANCER
    inline constexpr std::array<Method, 3> enhancerHarmonicsMethods {{
        { "CHB", "Chebyshev 2 + 3",
          "The DEPTH and CLARITY exciters generate 2nd and 3rd harmonics in the original mix (body mostly even, definition even and odd).",
          "The original colour: warm body, clear definition.",
          "Zero latency, 2x oversampled (default)." },
        { "EVN", "Even",
          "Mostly 2nd harmonic, very little 3rd.",
          "Warmer, rounder, tube-like: detail becomes fuller rather than sharper.",
          "Zero latency, same CPU. Glides over 30 ms (an exact crossfade)." },
        { "ODD", "Odd",
          "Mostly 3rd harmonic, less 2nd.",
          "Edgier and more forward, tape-and-transistor-like: definition cuts through a dense mix.",
          "Zero latency, same CPU. Glides over 30 ms (an exact crossfade)." },
    }};

    //==================================================================================================
    // TONE & SPACE
    inline constexpr std::array<Method, 3> seraphTapeMethods {{
        { "TNH", "Tanh",
          "TAPE's saturation curve: a hyperbolic tangent after 5 dB of treble pre-emphasis.",
          "The original: harsh transients rounded, a gentle, even softness.",
          "Zero latency (default)." },
        { "ATN", "Arctangent",
          "A gentler curve that keeps rising instead of flattening.",
          "Softer saturation: rounds less, keeps more of the attack, more open.",
          "Zero latency, same CPU. Crossfades over 30 ms." },
        { "CUB", "Cubic",
          "A cubic soft clipper: clean up to the knee, then a firm rounding.",
          "Cleaner below the knee, thicker above: louder moments get the character, quiet ones stay clean.",
          "Zero latency, less CPU. Crossfades over 30 ms." },
    }};
    inline constexpr std::array<Method, 3> seraphPreDelayMethods {{
        { "P18", "18 ms",
          "The reverb's pre-delay: how long after the sound its space begins.",
          "The original: the dry sound stays clear, the space follows closely.",
          "Zero latency (default)." },
        { "P08", "8 ms",
          "A short pre-delay.",
          "The space hugs the sound: more intimate, more blended, a smaller-feeling room.",
          "Zero latency. Crossfades over 30 ms." },
        { "P35", "35 ms",
          "A long pre-delay.",
          "The sound stands clear in front of a bigger space: more depth and separation.",
          "Zero latency. Crossfades over 30 ms." },
    }};
    inline constexpr std::array<Method, 3> seraphWindowMethods {{
        { "W2S", "2 s window",
          "LOUDNESS measures K-weighted level over about 2 s before it moves.",
          "The original: steady, no pumping with the bass.",
          "Control rate, no audio cost (default)." },
        { "W1S", "1 s window",
          "Measures over about 1 s.",
          "Follows level changes faster: holds the level tighter, may breathe a little on sparse material.",
          "Control rate, no audio cost. Glides." },
        { "W4S", "4 s window",
          "Measures over about 4 s.",
          "The steadiest: level changes are followed slowly and almost inaudibly.",
          "Control rate, no audio cost. Glides." },
    }};

    //==================================================================================================
    // LEVEL CONTROL
    inline constexpr std::array<Method, 3> levelGlideMethods {{
        { "STD", "20 ms glide",
          "LEVEL moves to where it is set over about 20 ms.",
          "The original: knob moves and automation are smooth, but immediate.",
          "No cost (default)." },
        { "FST", "5 ms glide",
          "Over about 5 ms.",
          "Snappier level changes for automation that should land exactly on the beat.",
          "No cost." },
        { "SLW", "150 ms glide",
          "Over about 150 ms.",
          "Level changes fade in gently: rides and automation never jump.",
          "No cost." },
    }};

    //==================================================================================================
    // OUTPUT MONITOR (the output limiter, and the display)
    inline constexpr std::array<Method, 3> outputCeilingMethods {{
        { "0.0", "0.0 dBFS",
          "The output limiter's ceiling: nothing leaves above full scale.",
          "The original: as loud as possible without clipping.",
          "3 ms lookahead, unchanged (default)." },
        { "0.3", "-0.3 dBFS",
          "Ceiling 0.3 dB under full scale.",
          "A little safety for converters and players that clip slightly early.",
          "3 ms lookahead, unchanged. Glides with the limiter's release." },
        { "1.0", "-1.0 dBFS",
          "Ceiling 1 dB under full scale, the usual target for streaming and encoded files.",
          "Room for lossy encoding and sample-rate conversion to overshoot without clipping.",
          "3 ms lookahead, unchanged. Glides with the limiter's release." },
    }};
    inline constexpr std::array<Method, 3> displayToneRangeMethods {{
        { "12", "+-12 dB",
          "The red tone-change curve spans +-12 dB.",
          "The original scale.",
          "Display only (default)." },
        { "6", "+-6 dB",
          "It spans +-6 dB: small changes are drawn twice as large.",
          "Subtle tone changes become easy to see.",
          "Display only." },
        { "24", "+-24 dB",
          "It spans +-24 dB.",
          "Big moves stay on the card.",
          "Display only." },
    }};
    inline constexpr std::array<Method, 3> displayDuckHoldMethods {{
        { "1.5", "1.5 s",
          "The DUCK readout holds the deepest duck for 1.5 s.",
          "The original: short ducks can be read.",
          "Display only (default)." },
        { "0.5", "0.5 s",
          "Holds it for 0.5 s.",
          "Follows the ducks closely, changes often.",
          "Display only." },
        { "4.0", "4 s",
          "Holds it for 4 s.",
          "Easy to read even brief ducks; slower to show the next one.",
          "Display only." },
    }};
    inline constexpr std::array<Method, 2> displayWaveformMethods {{
        { "PK", "Peak",
          "Each waveform column shows the loudest sample in it.",
          "The original: every hit and its reduction are visible.",
          "Display only (default)." },
        { "RMS", "RMS",
          "Each column shows the column's RMS level (+3 dB, so a steady sine reads the same as PK).",
          "Shows loudness rather than peaks: how the rack changes the body of the sound.",
          "Display only." },
    }};

    //==================================================================================================
    // The stages of each unit, in the order the panel lists them
    inline constexpr std::array<Stage, 6> compressorStages {{
        { "ADAPTIVE COMPRESSOR", 2, "PROCESSING", "DETECTOR", "How it measures the level", "tideDetector", tideDetector, "", tideDetectorMethods.data(), 3 },
        { "ADAPTIVE COMPRESSOR", 2, "PROCESSING", "SIDE-CHAIN", "What the detector hears", "tideSideChain", tideSideChain, "", tideSideChainMethods.data(), 3 },
        { "ADAPTIVE COMPRESSOR", 2, "PROCESSING", "GAIN", "How it calculates the reduction", "tideGain", tideGain, "", tideGainMethods.data(), 3 },
        { "ADAPTIVE COMPRESSOR", 2, "PROCESSING", "SMOOTHING", "How the reduction moves", "tideSmoothing", tideSmoothing, "", tideSmoothingMethods.data(), 3 },
        { "ADAPTIVE COMPRESSOR", 2, "PROCESSING", "MAKE-UP", "How much level it gives back", "tideMakeup", tideMakeup, "", tideMakeupMethods.data(), 3 },
        { "ADAPTIVE COMPRESSOR", 2, "KNOBS", "LAW", "How the knob's travel maps", "tideResponseLaw", tideResponseLaw, "tideResponse", tideResponseLawMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 3> limiterStages {{
        { "SPECTRAL LIMITER", 4, "PROCESSING", "NORMAL", "What counts as normal for a band", "limiterNormal", limiterNormal, "", limiterNormalMethods.data(), 3 },
        { "SPECTRAL LIMITER", 4, "PROCESSING", "CUT WIDTH", "How wide each cut is", "limiterWidth", limiterWidth, "", limiterWidthMethods.data(), 3 },
        { "SPECTRAL LIMITER", 4, "PROCESSING", "LOUDNESS KEEPER", "What it gives back while cutting", "limiterKeeper", limiterKeeper, "", keeperMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 5> balancerStages {{
        { "MIX BALANCER", 6, "PROCESSING", "REFERENCE", "What a band's jump is measured against", "balancerReference", balancerReference, "", balancerReferenceMethods.data(), 2 },
        { "MIX BALANCER", 6, "PROCESSING", "DEAD ZONE", "How big a move is ignored", "balancerDeadZone", balancerDeadZone, "", balancerDeadZoneMethods.data(), 3 },
        { "MIX BALANCER", 6, "PROCESSING", "LIFTS", "What it does with a band that drops out", "balancerLifts", balancerLifts, "", balancerLiftsMethods.data(), 3 },
        { "MIX BALANCER", 6, "PROCESSING", "ATTACK GUARD", "How it treats a fresh transient", "balancerGuard", balancerGuard, "", balancerGuardMethods.data(), 3 },
        { "MIX BALANCER", 6, "PROCESSING", "LOUDNESS KEEPER", "What it gives back while cutting", "balancerKeeper", balancerKeeper, "", keeperMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 3> levelerStages {{
        { "UPWARD LEVELER", 3, "PROCESSING", "LIFT", "How far quiet material may come up", "levelerLift", levelerLift, "", levelerLiftMethods.data(), 3 },
        { "UPWARD LEVELER", 3, "PROCESSING", "GATE", "What is too quiet to lift", "levelerGate", levelerGate, "", levelerGateMethods.data(), 3 },
        { "UPWARD LEVELER", 3, "PROCESSING", "BAND BALANCE", "Where the lift goes", "levelerBalance", levelerBalance, "", levelerBalanceMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 1> enhancerStages {{
        { "ADAPTIVE ENHANCER", 0, "PROCESSING", "HARMONICS", "What the exciters generate", "enhancerHarmonics", enhancerHarmonics, "", enhancerHarmonicsMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 3> seraphStages {{
        { "TONE & SPACE", 1, "PROCESSING", "TAPE CURVE", "How TAPE saturates", "seraphTape", seraphTape, "", seraphTapeMethods.data(), 3 },
        { "TONE & SPACE", 1, "PROCESSING", "PRE-DELAY", "When the space begins", "seraphPreDelay", seraphPreDelay, "", seraphPreDelayMethods.data(), 3 },
        { "TONE & SPACE", 1, "PROCESSING", "LOUDNESS WINDOW", "How long LOUDNESS listens", "seraphWindow", seraphWindow, "", seraphWindowMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 1> levelStages {{
        { "LEVEL CONTROL", 5, "PROCESSING", "GLIDE", "How fast LEVEL moves", "levelGlide", levelGlide, "", levelGlideMethods.data(), 3 },
    }};
    inline constexpr std::array<Stage, 4> monitorStages {{
        { "OUTPUT MONITOR", 7, "OUTPUT", "CEILING", "The output limiter's ceiling", "outputCeiling", outputCeiling, "", outputCeilingMethods.data(), 3 },
        { "OUTPUT MONITOR", 7, "DISPLAY", "TONE RANGE", "The tone-change curve's scale", "displayToneRange", displayToneRange, "", displayToneRangeMethods.data(), 3 },
        { "OUTPUT MONITOR", 7, "DISPLAY", "DUCK HOLD", "How long DUCK holds a reading", "displayDuckHold", displayDuckHold, "", displayDuckHoldMethods.data(), 3 },
        { "OUTPUT MONITOR", 7, "DISPLAY", "WAVEFORM", "What each waveform column shows", "displayWaveform", displayWaveform, "", displayWaveformMethods.data(), 2 },
    }};

    struct StageList { const Stage* stages; int count; };

    /** The stages of a unit (by its layout::Unit index). */
    inline constexpr StageList stagesForUnit (int unit) noexcept
    {
        switch (unit)
        {
            case 0: return { enhancerStages.data(), (int) enhancerStages.size() };
            case 1: return { seraphStages.data(), (int) seraphStages.size() };
            case 2: return { compressorStages.data(), (int) compressorStages.size() };
            case 3: return { levelerStages.data(), (int) levelerStages.size() };
            case 4: return { limiterStages.data(), (int) limiterStages.size() };
            case 5: return { levelStages.data(), (int) levelStages.size() };
            case 6: return { balancerStages.data(), (int) balancerStages.size() };
            case 7: return { monitorStages.data(), (int) monitorStages.size() };
            default: return { nullptr, 0 };
        }
    }

    /** The units bottom to top (signal order), for the parameters and the reference page. */
    inline constexpr std::array<int, 8> unitsInRackOrder { 5, 0, 3, 4, 6, 2, 1, 7 };

    //==================================================================================================
    // Knob modifiers: on every knob's input side, between the knob and its processing. Stored in the
    // plugin state, not as parameters: the host always sees the knob's raw value. All off = the knob as
    // it always was (skipped entirely, so the output is unchanged to the last bit).
    struct Modifier
    {
        std::string_view shortName, fullName, does, sound, cost;
        std::array<float, 4> choices;                 // stored values; choices[0] = off
        std::array<std::string_view, 4> labels;       // what the panel prints for each
    };

    enum ModifierKind { modifierSmoothing = 0, modifierCurve, modifierRange, numModifierKinds };

    inline constexpr std::array<Modifier, numModifierKinds> modifiers {{
        { "SMO", "Smoothing",
          "Glides the knob's value toward where it was set, one-pole, with this time constant. The same at every sample rate and block size.",
          "Automation and fast knob moves become gradual changes instead of steps.",
          "No latency on the audio; the knob's effect lags by about the time constant.",
          { 0.0f, 50.0f, 250.0f, 1000.0f }, { "OFF", "50 ms", "250 ms", "1 s" } },
        { "CRV", "Curve",
          "Reshapes the knob's travel before it reaches the processing: LOW spends more travel at the bottom of the range (squared), HIGH more at the top (square root), S at both ends (smoothstep).",
          "Finer control where you need it; the knob still covers the same range end to end.",
          "No cost.",
          { 0.0f, 1.0f, 2.0f, 3.0f }, { "LINEAR", "LOW", "HIGH", "S" } },
        { "LIM", "Range",
          "Limits how far the knob reaches: its full travel covers only the bottom part of its range.",
          "A safety net for live use and presets: the setting can never be pushed past the limit, and the knob gets finer.",
          "No cost.",
          { 100.0f, 75.0f, 50.0f, 25.0f }, { "FULL", "75 %", "50 %", "25 %" } },
    }};
}
