#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

/*  Single source of truth for every host-visible parameter.
    The DSP engine (Source/DSP) and the UI both address parameters by these IDs.
    Earlier control sets are preserved in git tags (see README).
*/
namespace pad::params
{
    namespace id
    {
        inline constexpr const char* clarityNorm = "clarityNorm"; // CLARITY in NORM mode: normalise only (0-30)
        inline constexpr const char* clarityAdd  = "clarityAdd";  // CLARITY in ADD mode: add + normalise (0-10)
        inline constexpr const char* clarityMode = "clarityMode"; // off = NORM, on = ADD + NORM
        inline constexpr const char* adaptSpeed = "adaptSpeed";  // how fast adaptive gains follow the material
        inline constexpr const char* sub        = "sub";         // sub-bass enhance amount
        inline constexpr const char* subBoost   = "subBoost";    // + BOOST
        inline constexpr const char* footstep   = "footstep";    // footstep priority mode
        inline constexpr const char* enhMultiply = "enhMultiply"; // multiplies every ENH Master knob (0-3x)
        inline constexpr const char* enhStrength = "enhStrength"; // how hard ENH Master's processing hits (0-5)

        // TIDE - 1U dynamic compressor: two controls, everything else derived from the audio
        inline constexpr const char* tideMix      = "tideMix";       // wet / dry (%)
        inline constexpr const char* tideResponse = "tideResponse";  // how fast it reacts and adapts (0-10)
        inline constexpr const char* tideActive   = "tideActive";    // IN / OUT (hardware bypass)

        // LUMEN - 1U spectral leveler: lifts quiet material toward a target
        inline constexpr const char* lumenTarget   = "lumenTarget";   // target level (dBFS)
        inline constexpr const char* lumenResponse = "lumenResponse"; // how fast it follows (0-10)
        inline constexpr const char* lumenActive   = "lumenActive";   // IN / OUT

        // SERAPH (the upper, purple unit): OFF / SILK / HEAVEN
        inline constexpr const char* seraphMode   = "seraphMode";
        inline constexpr const char* seraphMultiply = "seraphMultiply"; // multiplies every SERAPH knob except OUTPUT (0-3x)
        inline constexpr const char* seraphStrength = "seraphStrength"; // how hard SERAPH's processing hits (0-5)

        // SILK - tone & texture
        inline constexpr const char* silkSmooth   = "silkSmooth";    // adaptive resonance smoothing
        inline constexpr const char* silkAir      = "silkAir";       // adaptive air + generated highs
        inline constexpr const char* silkWarmth   = "silkWarmth";    // low-mid harmonics + triode
        inline constexpr const char* silkBody     = "silkBody";      // low-mid fullness when thin
        inline constexpr const char* silkOutput   = "silkOutput";    // output trim (dB)
        inline constexpr const char* silkProtect  = "silkProtect";   // keep attacks / footstep band intact
        inline constexpr const char* silkTape     = "silkTape";      // pre-emphasised tape softening
        inline constexpr const char* silkAuto     = "silkAuto";      // loudness-matched output

        // HEAVEN - what SERAPH does with the level of the sound it makes
        inline constexpr const char* heavenHold = "heavenHold";   // STABLE: hold the level (0-30)
        inline constexpr const char* heavenLift = "heavenLift";   // LIFT + STABLE: add, then hold (0-10)
        inline constexpr const char* heavenMode = "heavenMode";   // off = STABLE, on = LIFT + STABLE

        // HALO - space & width
        inline constexpr const char* haloWidth    = "haloWidth";     // stereo width (%)
        inline constexpr const char* haloSpace    = "haloSpace";     // reverb amount
        inline constexpr const char* haloDecay    = "haloDecay";     // reverb decay (s)
        inline constexpr const char* haloShimmer  = "haloShimmer";   // octave-up shimmer
        inline constexpr const char* haloTone     = "haloTone";      // dark .. airy tail
        inline constexpr const char* haloDuck     = "haloDuck";      // tail ducks under the programme
        inline constexpr const char* haloBassMono = "haloBassMono";  // mono below 120 Hz
        inline constexpr const char* haloMod      = "haloMod";       // tail modulation
    }

    enum class Kind { continuous, toggle, choice };

    struct Spec
    {
        juce::String id, name, shortLabel, unit;
        Kind kind = Kind::continuous;
        float minValue = 0.0f, maxValue = 1.0f, defaultValue = 0.0f;
        int decimals = 1;
        float skewCentre = 0.0f;                 // > 0: value at the middle of the knob travel
        juce::StringArray texts {};              // toggle: { off, on }; choice: the choices
    };

    const std::vector<Spec>& allSpecs();
    const Spec* findSpec (const juce::String& paramId);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
