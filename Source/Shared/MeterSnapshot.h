#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include "../DSP/EngineMeters.h"

/*  Every value in enh::dsp::EngineMeters, as plain numbers, so it can be copied between processes
    (Shared/DisplayBridge.h). JUCE-free.

    ENH_METER_FIELDS lists each field once, in EngineMeters' declaration order:
        X (name)            one float / int / bool
        A (name, count)     an array of them
    The static_assert below compares the size of a plain mirror (same members, same order, plain types)
    with EngineMeters itself: a field added to EngineMeters and not here fails the build. When it
    does, add the field here in the same place, and bump DisplayBridge's kVersion. */

#define ENH_METER_FIELDS(X, A)                                                                          \
    A (bandGainDb, enh::dsp::numBands)                                                                  \
    X (footstepConfidence) X (earGuardDb) X (earGuardUsualLufs)                                         \
    A (precisionHz, 8) A (precisionQ, 8) A (precisionDb, 8)                                             \
    X (enhancement) X (subLiftDb) X (autoGainDb) X (outputPeakDb)                                       \
    X (tideGrDb) X (tideThresholdDb) X (tideRatio) X (tideInputDb) X (tideOutputDb) X (tideAdaptivity)  \
    A (lumenGainDb, 3) A (lumenLevelDb, 3) X (lumenTotalDb) X (lumenActivity)                           \
    A (limitHz, 3) A (limitOctaves, 3) A (limitDepthDb, 3) A (limitShape, 3)                            \
    X (limitDeepestDb) X (limitBroadbandDb) X (limitMakeupDb)                                           \
    X (silkSmoothingDb) X (haloDb) X (silkBlend) X (haloBlend)                                          \
    A (seraphActivityDb, 16) X (seraphLevelDb)                                                          \
    X (heavenAutoBlend) A (heavenAutoChoice, 7) X (silkMatchDb)                                         \
    X (momentaryLufs) X (shortTermLufs) X (integratedLufs) X (truePeakDb) X (levelDb)                   \
    A (balanceGainDb, 6) A (balanceLevelDb, 6) A (balanceFineGainDb, 28)                                \
    X (balanceResolution) X (balanceMakeupDb)                                                           \
    X (deepGeneratedDb) X (deepPitchHz) X (deepConfidence)                                              \
    A (outputRegionCutDb, 4) X (outputLimitDb) X (targetGainDb) X (lunchboxHarshDb) X (lunchboxPeak)    \
    X (charHarmonicsDb) X (compareGainDb) X (comparing) X (protectionTripped)                           \
    A (silkDipDb, 28)                                                                                   \
    X (radarLiftDb) X (radarActivity) X (radarOnset) X (radarThreshold) X (radarMusicality)             \
    A (radarExcessDb, 6)                                                                                \
    A (radarTrackPan, 4) A (radarTrackDistance, 4) A (radarTrackConfidence, 4) A (radarTrackPeriod, 4)  \
    A (radarTrackId, 4)                                                                                 \
    A (radarStepTime, 32) A (radarStepPan, 32) A (radarStepRear, 32) A (radarStepDistance, 32)          \
    A (radarStepConfidence, 32) A (radarStepBoost, 32)                                                  \
    A (radarStepTrack, 32)                                                                              \
    X (radarStepsTotal) X (radarClock) X (radarOn)

namespace enh::shared
{
    namespace detail
    {
        template <typename T> struct Unwrap                   { using type = T; };
        template <typename T> struct Unwrap<std::atomic<T>>   { using type = T; };
        template <typename T, std::size_t N> struct Unwrap<std::array<std::atomic<T>, N>> { using type = std::array<T, N>; };

        template <typename M> using PlainOf = typename Unwrap<std::remove_cv_t<M>>::type;

        // The same members as EngineMeters, in the same order, as plain types: its size must match
        struct MeterMirror
        {
           #define ENH_MIRROR_X(name)        PlainOf<decltype (enh::dsp::EngineMeters::name)> name;
           #define ENH_MIRROR_A(name, count) PlainOf<decltype (enh::dsp::EngineMeters::name)> name;
            ENH_METER_FIELDS (ENH_MIRROR_X, ENH_MIRROR_A)
           #undef ENH_MIRROR_X
           #undef ENH_MIRROR_A
        };

        static_assert (sizeof (MeterMirror) == sizeof (enh::dsp::EngineMeters),
                       "EngineMeters changed: update ENH_METER_FIELDS in Shared/MeterSnapshot.h (and DisplayBridge kVersion)");

        // Fixed-width storage for the snapshot: floats stay float, ints become int32, bools uint32
        template <typename T> struct Wire                   { using type = float; };
        template <> struct Wire<int>                        { using type = std::int32_t; };
        template <> struct Wire<bool>                       { using type = std::uint32_t; };
        template <typename T, std::size_t N> struct Wire<std::array<T, N>> { using type = std::array<typename Wire<T>::type, N>; };
    }

    /** The meters as plain, fixed-width values (every member 4 bytes wide, so the struct has no padding
        and is the same in every process and compiler). */
    struct MeterSnapshot
    {
       #define ENH_SNAP_X(name)        typename detail::Wire<detail::PlainOf<decltype (enh::dsp::EngineMeters::name)>>::type name {};
       #define ENH_SNAP_A(name, count) typename detail::Wire<detail::PlainOf<decltype (enh::dsp::EngineMeters::name)>>::type name {};
        ENH_METER_FIELDS (ENH_SNAP_X, ENH_SNAP_A)
       #undef ENH_SNAP_X
       #undef ENH_SNAP_A
    };

    static_assert (std::is_trivially_copyable_v<MeterSnapshot>);
    static_assert (sizeof (MeterSnapshot) % 4 == 0);

    /** Reads every meter (relaxed loads: any thread, never blocks). */
    inline void captureMeters (const enh::dsp::EngineMeters& m, MeterSnapshot& s) noexcept
    {
       #define ENH_CAP_X(name) s.name = static_cast<decltype (s.name)> (m.name.load (std::memory_order_relaxed));
       #define ENH_CAP_A(name, count)                                                                       \
            for (std::size_t i = 0; i < s.name.size(); ++i)                                                 \
                s.name[i] = static_cast<typename decltype (s.name)::value_type> (m.name[i].load (std::memory_order_relaxed));
        ENH_METER_FIELDS (ENH_CAP_X, ENH_CAP_A)
       #undef ENH_CAP_X
       #undef ENH_CAP_A
    }

    /** Writes every meter (relaxed stores). radarStepsTotal goes last with release order, as the engine
        publishes it (the display reads the total first, then the steps it counts). */
    inline void applyMeters (const MeterSnapshot& s, enh::dsp::EngineMeters& m) noexcept
    {
        using detail::PlainOf;
       #define ENH_APP_X(name) m.name.store (static_cast<PlainOf<decltype (m.name)>> (s.name), std::memory_order_relaxed);
       #define ENH_APP_A(name, count)                                                                       \
            for (std::size_t i = 0; i < s.name.size(); ++i)                                                 \
                m.name[i].store (static_cast<typename PlainOf<decltype (m.name)>::value_type> (s.name[i]), std::memory_order_relaxed);
        ENH_METER_FIELDS (ENH_APP_X, ENH_APP_A)
       #undef ENH_APP_X
       #undef ENH_APP_A
        m.radarStepsTotal.store (s.radarStepsTotal, std::memory_order_release);
    }
}
