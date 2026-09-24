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
        bool subBoost = false, footstep = false, radarListen = false;
        float radarSens = 6.0f, radarBoost = 6.0f, radarSpace = 4.0f;
        float enhMultiply = 1.0f, enhStrength = 1.0f;

        // SERAPH
        int seraphMode = Seraph::heaven;
        float smooth = 4.0f, air = 4.0f, warmth = 3.0f, body = 2.0f, outputDb = 0.0f;   // 0..10, dB
        bool protect = true, tape = false, autoGain = true;
        float silkSub = 0.0f;                           // 0..10
        bool heavenAuto = false;
        float heavenAutoAmount = 5.0f;                  // 0..10
        float widthPercent = 120.0f, space = 2.5f, decayS = 2.2f, shimmer = 1.5f, tone = 6.0f;
        bool duck = true, bassMono = true, mod = true;
        float heavenHold = 12.0f, heavenLift = 4.0f;    // 0..30 / 0..10
        bool heavenLiftMode = false;
        float seraphMultiply = 1.0f, seraphStrength = 1.0f;

        // ADAPTIVE COMPRESSOR and UPWARD LEVELER: two knobs each
        float tideMixPercent = 60.0f, tideResponse = 5.0f;
        bool tideActive = true;
        std::array<int, methods::numMethodIds> methods {};   // every processing method (MethodRegistry.h), 0 = default

        // DEEP SUB
        float deepDepth = 0.0f, deepHull = 0.0f, deepSize = 5.0f, deepPressure = 0.0f;   // 0..10
        bool deepActive = true;

        // CHARACTER
        float charModelA = 3.0f, charModelB = 4.0f, charBlend = 0.0f, charDrive = 5.0f, charColour = 5.0f;
        bool charActive = false, charGrit = true;
        bool compare = false;   // COMPARE (OUTPUT MONITOR)
        float lumenTargetDb = -18.0f, lumenResponse = 5.0f;
        bool lumenActive = true;

        // SPECTRAL LIMITER: three knobs
        float spectralRangeDb = 9.0f, spectralReleaseMs = 150.0f, spectralCeilingDb = 0.0f;
        bool spectralActive = true;

        // LEVEL, MIX BALANCER
        float levelDb = 0.0f;
        float balAmount = 5.0f, balSpeed = 5.0f, balTilt = 0.0f, balRangeDb = 6.0f, balResolution = 0.0f;
        bool balActive = true;
    };

    /** Every continuous knob, by parameter ID, and where its value goes: the knob modifiers
        (Parameters/KnobModifiers.h) work on these, before the mapping below. */
    struct KnobField { const char* param; float KnobValues::* field; };
    inline constexpr std::array<KnobField, 45> knobFields {{
        { "clarityNorm", &KnobValues::clarityNorm },       { "clarityAdd", &KnobValues::clarityAdd },
        { "adaptSpeed", &KnobValues::adaptPercent },       { "sub", &KnobValues::subPercent },
        { "enhMultiply", &KnobValues::enhMultiply },       { "enhStrength", &KnobValues::enhStrength },
        { "heavenHold", &KnobValues::heavenHold },         { "heavenLift", &KnobValues::heavenLift },
        { "heavenAutoAmount", &KnobValues::heavenAutoAmount },
        { "tideMix", &KnobValues::tideMixPercent },        { "tideResponse", &KnobValues::tideResponse },
        { "lumenTarget", &KnobValues::lumenTargetDb },     { "lumenResponse", &KnobValues::lumenResponse },
        { "spectralRange", &KnobValues::spectralRangeDb }, { "spectralRelease", &KnobValues::spectralReleaseMs },
        { "spectralCeiling", &KnobValues::spectralCeilingDb },
        { "levelGain", &KnobValues::levelDb },
        { "balAmount", &KnobValues::balAmount },           { "balSpeed", &KnobValues::balSpeed },
        { "balTilt", &KnobValues::balTilt },               { "balRange", &KnobValues::balRangeDb },
        { "balResolution", &KnobValues::balResolution },
        { "silkSmooth", &KnobValues::smooth },             { "silkAir", &KnobValues::air },
        { "silkWarmth", &KnobValues::warmth },             { "silkBody", &KnobValues::body },
        { "silkOutput", &KnobValues::outputDb },           { "silkSub", &KnobValues::silkSub },
        { "haloWidth", &KnobValues::widthPercent },        { "haloSpace", &KnobValues::space },
        { "haloDecay", &KnobValues::decayS },              { "haloShimmer", &KnobValues::shimmer },
        { "haloTone", &KnobValues::tone },
        { "seraphMultiply", &KnobValues::seraphMultiply }, { "seraphStrength", &KnobValues::seraphStrength },
        { "deepDepth", &KnobValues::deepDepth },           { "deepHull", &KnobValues::deepHull },
        { "deepSize", &KnobValues::deepSize },             { "deepPressure", &KnobValues::deepPressure },
        { "charBlend", &KnobValues::charBlend },           { "charDrive", &KnobValues::charDrive },
        { "charColour", &KnobValues::charColour },
        { "radarSens", &KnobValues::radarSens },           { "radarBoost", &KnobValues::radarBoost },
        { "radarSpace", &KnobValues::radarSpace },
    }};

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
        p.radar.active = k.footstep;
        p.radar.sensitivity = std::clamp (k.radarSens, 0.0f, 10.0f);
        p.radar.boostDb = std::clamp (k.radarBoost, 0.0f, 12.0f);
        p.radar.space = std::clamp (k.radarSpace, 0.0f, 10.0f);
        p.radar.solo = k.radarListen;
        p.radar.detection = k.methods[(size_t) methods::radarDetection];
        p.radar.room = k.methods[(size_t) methods::radarRoom];
        p.strength = std::clamp (k.enhStrength, 0.0f, maxStrength);

        const float sm = std::clamp (k.seraphMultiply, 0.0f, maxMultiply);
        const float ss = std::clamp (k.seraphStrength, 0.0f, maxStrength);
        auto& s = p.seraph;
        s.mode = k.seraphMode;
        s.silk.smooth   = std::clamp (k.smooth * sm, 0.0f, 30.0f);
        s.silk.air      = std::clamp (k.air * sm, 0.0f, 30.0f);
        s.silk.warmth   = std::clamp (k.warmth * sm, 0.0f, 30.0f);
        s.silk.body     = std::clamp (k.body * sm, 0.0f, 30.0f);
        s.silk.sub      = std::clamp (k.silkSub * sm, 0.0f, 30.0f);
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
        s.heaven.autoHeaven = k.heavenAuto;
        s.heaven.autoAmount = std::clamp (k.heavenAutoAmount / 10.0f, 0.0f, 1.0f);

        // The 1U units have no device master, so their knobs map straight across
        p.tide.mix      = std::clamp (k.tideMixPercent / 100.0f, 0.0f, 1.0f);
        {
            // RESPONSE's own processing method: LIN (the travel as it is) or EXP (more of it at the slow end)
            // RESPONSE's own processing method: LIN (the travel as it is), EXP (more of it at the slow end),
            // LOG (more of it at the fast end)
            const float travel = std::clamp (k.tideResponse / 10.0f, 0.0f, 1.0f);
            const int law = k.methods[(size_t) methods::tideResponseLaw];
            p.tide.response = law == 1 ? (std::pow (8.0f, travel) - 1.0f) / 7.0f
                            : law == 2 ? 1.0f - (std::pow (8.0f, 1.0f - travel) - 1.0f) / 7.0f
                                       : travel;
        }

        // The processing methods (MethodRegistry.h): the engine keeps the whole set, each unit gets its own
        using namespace methods;
        const auto& mt = k.methods;
        p.methods = mt;
        p.tide.detector  = mt[tideDetector];
        p.tide.sideChain = mt[tideSideChain];
        p.tide.gain      = mt[tideGain];
        p.tide.smoothing = mt[tideSmoothing];
        p.tide.makeup    = mt[tideMakeup];
        p.limiter.keeper = mt[limiterKeeper];
        p.balancer.reference = mt[balancerReference];
        p.balancer.deadZone  = mt[balancerDeadZone];
        p.balancer.lifts     = mt[balancerLifts];
        p.balancer.guard     = mt[balancerGuard];
        p.balancer.keeper    = mt[balancerKeeper];
        p.lumen.lift    = mt[levelerLift];
        p.lumen.gate    = mt[levelerGate];
        p.lumen.balance = mt[levelerBalance];
        p.seraph.silk.tapeCurve = mt[seraphTape];
        p.seraph.halo.preDelay  = mt[seraphPreDelay];
        p.seraph.heaven.window  = mt[seraphWindow];

        p.deep.depth    = std::clamp (k.deepDepth / 10.0f, 0.0f, 1.0f);
        p.deep.hull     = std::clamp (k.deepHull / 10.0f, 0.0f, 1.0f);
        p.deep.size     = std::clamp (k.deepSize / 10.0f, 0.0f, 1.0f);
        p.deep.pressure = std::clamp (k.deepPressure / 10.0f, 0.0f, 1.0f);
        p.deep.active   = k.deepActive;
        p.deep.shape    = mt[deepShape];
        p.deep.tracking = mt[deepTracking];
        p.deep.material = mt[deepMaterial];

        p.character.active     = k.charActive;
        p.character.modelA     = std::clamp ((int) std::lround (k.charModelA), 0, (int) Character::numModels - 1);
        p.character.modelB     = std::clamp ((int) std::lround (k.charModelB), 0, (int) Character::numModels - 1);
        p.character.blend      = std::clamp (k.charBlend / 100.0f, 0.0f, 1.0f);
        p.character.drive      = std::clamp (k.charDrive, 0.0f, 10.0f);
        p.character.colour     = std::clamp (k.charColour, 0.0f, 10.0f);
        p.character.components = mt[charComponents];
        p.character.grit       = k.charGrit;
        p.compare              = k.compare;
        p.tide.active   = k.tideActive;
        p.lumen.targetDb = std::clamp (k.lumenTargetDb, -60.0f, 0.0f);
        p.lumen.response = std::clamp (k.lumenResponse / 10.0f, 0.0f, 1.0f);
        p.lumen.active   = k.lumenActive;
        p.limiter.rangeDb   = std::clamp (k.spectralRangeDb, 0.0f, 18.0f);
        p.limiter.releaseMs = std::clamp (k.spectralReleaseMs, 20.0f, 2000.0f);
        p.limiter.ceilingDb = std::clamp (k.spectralCeilingDb, -24.0f, 0.0f);

        p.levelDb = std::clamp (k.levelDb, -24.0f, 12.0f);
        p.balancer.amount  = std::clamp (k.balAmount / 10.0f, 0.0f, 1.0f);
        p.balancer.speed   = std::clamp (k.balSpeed / 10.0f, 0.0f, 1.0f);
        p.balancer.tilt    = std::clamp (k.balTilt / 5.0f, -1.0f, 1.0f);
        p.balancer.rangeDb = std::clamp (k.balRangeDb, 0.0f, 12.0f);
        p.balancer.active  = k.balActive;
        p.balancer.resolution = std::clamp (k.balResolution / 10.0f, 0.0f, 1.0f);
        p.limiter.active    = k.spectralActive;
        return p;
    }
}
