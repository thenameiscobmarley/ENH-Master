#pragma once

#include <vector>
#include <utility>
#include "ParameterSpecs.h"

/*  Factory presets for the whole rack. Values are in each parameter's own units (as printed on
    the panels); anything a preset does not list goes back to its default, so every preset is a
    complete, predictable state.

    These are the built-in defaults. The plugin and the tests use the local preset file
    (PresetLibrary.h: ~/.config/ENH Master/presets.json on Linux), which is seeded from this list the
    first time the plugin runs and can then be edited and re-tuned without rebuilding.

    Units, bottom to top: ADAPTIVE ENHANCER, UPWARD LEVELER, SPECTRAL LIMITER, ADAPTIVE COMPRESSOR,
    TONE & SPACE. Parameter IDs keep the units' earlier names (tide* = compressor, lumen* = leveler,
    silk / halo / heaven = TONE / SPACE / LOUDNESS on TONE & SPACE).
*/
namespace pad::presets
{
    struct Preset
    {
        juce::String name;
        juce::String purpose;
        std::vector<std::pair<juce::String, float>> values;
    };

    namespace id = params::id;

    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> list {
            { "DEFAULT", "every unit at its default: a balanced starting point", {} },

            { "COMPETITIVE FOOTSTEPS",
              "PvP: steps and detail forward, explosions do not duck them, no reverb smearing position",
              {
                  { id::clarityMode, 0.0f }, { id::clarityNorm, 21.0f }, { id::adaptSpeed, 55.0f },
                  { id::sub, 0.0f }, { id::subBoost, 0.0f }, { id::footstep, 1.0f }, { id::enhStrength, 1.3f },
                  { id::lumenTarget, -22.0f }, { id::lumenResponse, 4.0f },                 // lift quiet detail, steadily
                  { id::spectralRange, 13.0f }, { id::spectralRelease, 110.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 35.0f }, { id::tideResponse, 5.0f },                     // light bus compression: transients are cues
                  { id::seraphMode, 1.0f },                                                // TONE only: no reverb tail
                  { id::silkSmooth, 5.0f }, { id::silkAir, 3.0f }, { id::silkWarmth, 1.0f }, { id::silkBody, 0.0f },
                  { id::silkProtect, 1.0f }, { id::silkTape, 0.0f }, { id::silkAuto, 1.0f },
                  { id::haloWidth, 100.0f }, { id::haloBassMono, 1.0f },                   // stereo cues untouched
                  { id::heavenHold, 10.0f },
              } },

            { "IMMERSIVE GAMES",
              "single-player and cinematic games: fuller low end, space and width, hits still controlled",
              {
                  { id::clarityMode, 1.0f }, { id::clarityAdd, 4.0f }, { id::adaptSpeed, 35.0f },
                  { id::sub, 45.0f }, { id::subBoost, 0.0f }, { id::footstep, 0.0f },
                  { id::lumenTarget, -22.0f }, { id::lumenResponse, 4.0f },               // hits don't pull the detail down
                  { id::spectralRange, 9.0f }, { id::spectralRelease, 240.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 40.0f }, { id::tideResponse, 4.0f },
                  { id::seraphMode, 2.0f },
                  { id::silkSmooth, 4.0f }, { id::silkAir, 4.0f }, { id::silkWarmth, 3.0f }, { id::silkBody, 2.5f },
                  { id::haloWidth, 130.0f }, { id::haloSpace, 2.0f }, { id::haloDecay, 2.4f }, { id::haloShimmer, 1.0f },
                  { id::haloTone, 6.0f }, { id::haloDuck, 1.0f }, { id::haloBassMono, 1.0f },
                  { id::heavenHold, 12.0f }, { id::silkSub, 3.0f },
                  { id::heavenAuto, 1.0f }, { id::heavenAutoAmount, 6.0f },          // it tunes its own space
              } },

            { "NIGHT MODE",
              "quiet listening: small level range, quiet sounds lifted, sudden loud events held down hard",
              {
                  { id::clarityMode, 0.0f }, { id::clarityNorm, 16.0f }, { id::sub, 0.0f }, { id::footstep, 1.0f },
                  { id::lumenTarget, -24.0f }, { id::lumenResponse, 4.5f },
                  { id::spectralRange, 16.0f }, { id::spectralRelease, 200.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 80.0f }, { id::tideResponse, 7.0f },
                  { id::seraphMode, 1.0f },
                  { id::silkSmooth, 5.0f }, { id::silkAir, 2.0f }, { id::silkWarmth, 2.0f }, { id::silkBody, 1.0f },
                  { id::silkAuto, 1.0f }, { id::heavenHold, 20.0f },
              } },

            { "BASS HEAVY, PROTECTED",
              "big low end without the pumping: sub lift on, abnormal bass hits cut where they are",
              {
                  { id::clarityMode, 1.0f }, { id::clarityAdd, 3.0f }, { id::sub, 60.0f }, { id::subBoost, 1.0f },
                  { id::footstep, 1.0f },
                  { id::lumenTarget, -21.0f },
                  { id::spectralRange, 15.0f }, { id::spectralRelease, 170.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 50.0f }, { id::tideResponse, 5.0f },
                  { id::seraphMode, 2.0f },
                  { id::silkBody, 1.0f }, { id::silkWarmth, 2.0f }, { id::silkSub, 4.0f },
                  { id::haloSpace, 1.5f }, { id::haloBassMono, 1.0f }, { id::haloDuck, 1.0f },
              } },

            { "VOICE & STREAMING",
              "speech first: intelligibility, even level, no tail under the voice",
              {
                  { id::clarityMode, 0.0f }, { id::clarityNorm, 22.0f }, { id::adaptSpeed, 50.0f },
                  { id::sub, 0.0f }, { id::footstep, 0.0f },
                  { id::lumenTarget, -20.0f }, { id::lumenResponse, 5.0f },
                  { id::spectralRange, 9.0f }, { id::spectralRelease, 150.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 70.0f }, { id::tideResponse, 6.0f },
                  { id::seraphMode, 1.0f },
                  { id::silkSmooth, 6.0f }, { id::silkAir, 3.0f }, { id::silkWarmth, 2.0f }, { id::silkBody, 3.0f },
                  { id::silkProtect, 0.0f }, { id::heavenHold, 14.0f },
              } },

            { "MUSIC: WARM MASTER",
              "music: tape-like warmth and a touch of room, dynamics kept (no upward leveling)",
              {
                  { id::clarityMode, 1.0f }, { id::clarityAdd, 3.0f }, { id::adaptSpeed, 30.0f },
                  { id::sub, 25.0f }, { id::footstep, 0.0f },
                  { id::lumenActive, 0.0f },                                                // music keeps its dynamics
                  { id::spectralRange, 6.0f }, { id::spectralRelease, 300.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 35.0f }, { id::tideResponse, 3.0f },
                  { id::seraphMode, 2.0f },
                  { id::silkSmooth, 3.0f }, { id::silkAir, 3.0f }, { id::silkWarmth, 4.5f }, { id::silkBody, 3.0f },
                  { id::silkTape, 1.0f },
                  { id::haloWidth, 115.0f }, { id::haloSpace, 1.0f }, { id::haloDecay, 1.6f }, { id::haloShimmer, 0.0f },
                  { id::haloTone, 5.0f }, { id::heavenHold, 12.0f },
              } },

            { "MUSIC: WIDE & AIRY",
              "music: open top end, wide image, a lush modulated hall",
              {
                  { id::clarityMode, 1.0f }, { id::clarityAdd, 4.0f }, { id::adaptSpeed, 35.0f },
                  { id::sub, 20.0f }, { id::footstep, 0.0f },
                  { id::lumenActive, 0.0f },
                  { id::spectralRange, 6.0f }, { id::spectralRelease, 300.0f }, { id::spectralCeiling, 0.0f },
                  { id::tideMix, 30.0f }, { id::tideResponse, 3.0f },
                  { id::seraphMode, 2.0f },
                  { id::silkSmooth, 4.0f }, { id::silkAir, 6.0f }, { id::silkWarmth, 2.0f }, { id::silkBody, 1.5f },
                  { id::haloWidth, 150.0f }, { id::haloSpace, 3.0f }, { id::haloDecay, 3.2f }, { id::haloShimmer, 2.5f },
                  { id::haloTone, 7.0f }, { id::haloMod, 1.0f }, { id::haloDuck, 1.0f }, { id::haloBassMono, 1.0f },
                  { id::silkSub, 2.0f }, { id::heavenAuto, 1.0f }, { id::heavenAutoAmount, 7.0f },
              } },

            { "TRANSPARENT (ALL OUT)",
              "a reference: every unit bypassed, the enhancer at STRENGTH 0 (its subsonic filter and the output safety limiter stay)",
              {
                  { id::enhStrength, 0.0f }, { id::sub, 0.0f }, { id::footstep, 0.0f },
                  { id::lumenActive, 0.0f }, { id::spectralActive, 0.0f }, { id::tideActive, 0.0f }, { id::balActive, 0.0f },
                  { id::seraphMode, 0.0f },
              } },
        };
        return list;
    }

    /** The preset's value for a parameter (its own units), or the parameter's default. */
    inline float valueFor (const Preset& p, const params::Spec& spec)
    {
        for (auto& [pid, v] : p.values)
            if (spec.id == pid)
                return v;
        return spec.defaultValue;
    }
}
