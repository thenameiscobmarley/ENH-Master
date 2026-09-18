#pragma once

#include "EnhEngine.h"

namespace enh::dsp
{
    /** Raw knob values exactly as the host / panel holds them (their own units), plus each device's
        MULTIPLY and STRENGTH. Kept free of JUCE parameters so the tests can exercise the mapping. */
    struct KnobValues
    {
        // ENH Master
        float clarityNorm = 15.0f, clarityAdd = 3.0f;   // 0..30 / 0..10
        bool clarityAddMode = false;
        float adaptPercent = 40.0f, subPercent = 0.0f;
        bool subBoost = false, footstep = false;
        float enhMultiply = 1.0f, enhStrength = 1.0f;

        // SERAPH
        int seraphMode = Seraph::heaven;
        float smooth = 4.0f, air = 4.0f, warmth = 3.0f, body = 2.0f, outputDb = 0.0f;   // 0..10, dB
        bool protect = true, tape = false, autoGain = true;
        float widthPercent = 120.0f, space = 2.5f, decayS = 2.2f, shimmer = 1.5f, tone = 6.0f;
        bool duck = true, bassMono = true, mod = true;
        float heavenHold = 12.0f, heavenLift = 4.0f;    // 0..30 / 0..10
        bool heavenLiftMode = false;
        float seraphMultiply = 1.0f, seraphStrength = 1.0f;

        // ADAPTIVE COMPRESSOR and UPWARD LEVELER: two knobs each
        float tideMixPercent = 60.0f, tideResponse = 5.0f;
        bool tideActive = true;
        float lumenTargetDb = -18.0f, lumenResponse = 5.0f;
        bool lumenActive = true;

        // SPECTRAL LIMITER: three knobs
        float spectralRangeDb = 9.0f, spectralReleaseMs = 150.0f, spectralCeilingDb = -3.0f;
        bool spectralActive = true;
    };

    inline constexpr float maxMultiply = 3.0f, maxStrength = 5.0f;

    /** MULTIPLY scales every knob on its device (never the gain: SERAPH's OUTPUT stays as set) before the
        engine sees it, so 1.5x makes CLARITY 20 behave as 30. Values may go past a knob's printed end;
        each is clamped to what the processing can safely take. STRENGTH is passed through: it scales
        how hard the processing hits, inside the engine. */
    inline EnhEngine::Parameters mapKnobs (const KnobValues& k) noexcept
    {
        EnhEngine::Parameters p;

        const float m = std::clamp (k.enhMultiply, 0.0f, maxMultiply);
        if (k.clarityAddMode)
        {
            p.normalize = std::clamp (k.clarityAdd * m / 10.0f, 0.0f, 3.0f);
            p.boost = p.normalize;
        }
        else
        {
            p.normalize = std::clamp (k.clarityNorm * m / 30.0f, 0.0f, 3.0f);
            p.boost = 0.0f;
        }
        p.adaptSpeed = std::clamp (k.adaptPercent * m / 100.0f, 0.0f, 1.5f);
        p.sub = std::clamp (k.subPercent * m / 100.0f, 0.0f, 3.0f);
        p.subBoost = k.subBoost;
        p.footstep = k.footstep;
        p.strength = std::clamp (k.enhStrength, 0.0f, maxStrength);

        const float sm = std::clamp (k.seraphMultiply, 0.0f, maxMultiply);
        const float ss = std::clamp (k.seraphStrength, 0.0f, maxStrength);
        auto& s = p.seraph;
        s.mode = k.seraphMode;
        s.silk.smooth   = std::clamp (k.smooth * sm, 0.0f, 30.0f);
        s.silk.air      = std::clamp (k.air * sm, 0.0f, 30.0f);
        s.silk.warmth   = std::clamp (k.warmth * sm, 0.0f, 30.0f);
        s.silk.body     = std::clamp (k.body * sm, 0.0f, 30.0f);
        s.silk.outputDb = k.outputDb;                            // gain: not multiplied
        s.silk.protect  = k.protect;
        s.silk.tape     = k.tape;
        s.silk.autoGain = k.autoGain;
        s.silk.strength = ss;
        s.halo.width    = std::clamp (k.widthPercent / 100.0f * sm, 0.0f, 6.0f);
        s.halo.space    = std::clamp (k.space / 10.0f * sm, 0.0f, 3.0f);
        s.halo.decayS   = std::clamp (k.decayS * sm, 0.1f, 24.0f);
        s.halo.shimmer  = std::clamp (k.shimmer / 10.0f * sm, 0.0f, 3.0f);
        s.halo.tone     = std::clamp (k.tone / 10.0f * sm, 0.0f, 1.0f);
        s.halo.duck     = k.duck;
        s.halo.bassMono = k.bassMono;
        s.halo.mod      = k.mod;
        s.halo.strength = ss;

        // HEAVEN: one knob with two printed scales, like CLARITY on ENH Master
        s.heaven.liftMode = k.heavenLiftMode;
        s.heaven.amount   = k.heavenLiftMode ? std::clamp (k.heavenLift * sm / 10.0f, 0.0f, 3.0f)
                                             : std::clamp (k.heavenHold * sm / 30.0f, 0.0f, 3.0f);
        s.heaven.lift     = std::clamp (k.heavenLift * sm / 10.0f, 0.0f, 1.0f);
        s.heaven.strength = ss;

        // The 1U units have no device master, so their knobs map straight across
        p.tide.mix      = std::clamp (k.tideMixPercent / 100.0f, 0.0f, 1.0f);
        p.tide.response = std::clamp (k.tideResponse / 10.0f, 0.0f, 1.0f);
        p.tide.active   = k.tideActive;
        p.lumen.targetDb = std::clamp (k.lumenTargetDb, -60.0f, 0.0f);
        p.lumen.response = std::clamp (k.lumenResponse / 10.0f, 0.0f, 1.0f);
        p.lumen.active   = k.lumenActive;
        p.limiter.rangeDb   = std::clamp (k.spectralRangeDb, 0.0f, 18.0f);
        p.limiter.releaseMs = std::clamp (k.spectralReleaseMs, 20.0f, 2000.0f);
        p.limiter.ceilingDb = std::clamp (k.spectralCeilingDb, -24.0f, 0.0f);
        p.limiter.active    = k.spectralActive;
        return p;
    }
}
