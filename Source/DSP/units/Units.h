#pragma once

#include "UnitList.h"
#include "../DesignedUnits.h"
#include "Reverbs.h"
#include "TimeFx.h"
#include "Dynamics.h"
#include "Space.h"
#include "Harsh.h"
#include "Exciter.h"
#include "RayRoom.h"
#include "Sims.h"
#include "Sims2.h"
#include "Sims3.h"
#include "Sims4.h"
#include "Sims5.h"
#include "Sims6.h"
#include "Sims7.h"

/*  The newer units, made by their place in UnitList.h (Tools/units/gen_units.py writes that list: keep the
    order of this switch the same as its). */
namespace enh::dsp::units
{
    inline std::unique_ptr<RackUnit> makeUnit (int k)
    {
        switch (k)
        {
            case 0:  return std::make_unique<Shimmer>();
            case 1:  return std::make_unique<Plate>();
            case 2:  return std::make_unique<Spring>();
            case 3:  return std::make_unique<GrainCloud>();
            case 4:  return std::make_unique<AtrTape>();
            case 5:  return std::make_unique<T4Opto>();
            case 6:  return std::make_unique<VariMu>();
            case 7:  return std::make_unique<DynamicEq>();
            case 8:  return std::make_unique<Shuffler>();
            case 9:  return std::make_unique<PhaseRotator>();
            case 10: return std::make_unique<BodeShifter>();
            case 11: return std::make_unique<Harmonizer>();
            case 12: return std::make_unique<BbdEnsemble>();
            case 13: return std::make_unique<Robovox>();
            case 14: return std::make_unique<SubMaxx>();
            case 15: return std::make_unique<Overclip>();
            case 16: return std::make_unique<DeHarsh>();
            case 17: return std::make_unique<Maximizer>();
            case 18: return std::make_unique<RayRoom>();
            case 19: return std::make_unique<VinylDeck>();
            case 20: return std::make_unique<RotaryCab>();
            case 21: return std::make_unique<CassetteDeck>();
            case 22: return std::make_unique<TapeEcho>();
            case 23: return std::make_unique<ValveAmp>();
            case 24: return std::make_unique<SpeakerCab>();
            case 25: return std::make_unique<Radio>();
            case 26: return std::make_unique<Pendulum>();
            case 27: return std::make_unique<BounceDelay>();
            case 28: return std::make_unique<Sympathy>();
            case 29: return std::make_unique<Flyby>();
            case 30: return std::make_unique<TeslaCoil>();
            case 31: return std::make_unique<TalkBox>();
            case 32: return std::make_unique<LavaLamp>();
            case 33: return std::make_unique<ClarityLens>();
            case 34: return std::make_unique<SubDriver>();
            case 35: return std::make_unique<VinylCutter>();
            case 36: return std::make_unique<CarTest>();
            case 37: return std::make_unique<PhoneCheck>();
            case 38: return std::make_unique<ClubSystem>();
            case 39: return std::make_unique<Pressure>();
            case 40: return std::make_unique<Balance>();
            case 41: return std::make_unique<StereoField>();
            case 42: return std::make_unique<Sonar>();
            case 43: return std::make_unique<Seismograph>();
            case 44: return std::make_unique<Prism>();
            case 45: return std::make_unique<Furnace>();
            case 46: return std::make_unique<Dither>();
            case 47: return std::make_unique<Rider>();
            case 48: return std::make_unique<Compass>();
            case 49: return std::make_unique<Suspension>();
            case 50: return std::make_unique<Skyline>();
            case 51: return std::make_unique<Hourglass>();
            case 52: return std::make_unique<Aurora>();
            case 53: return std::make_unique<ChromaSpace>();
            case 54: return std::make_unique<Hypercube>();
            case 55: return std::make_unique<RackTunerUnit>();
            case 56: return std::make_unique<DetailEnhancer>();
            case 57: return std::make_unique<VoiceIdentity>();
            case 58: return std::make_unique<PitchCorrector>();
            case 59: return std::make_unique<VocalStation>();
            default: return nullptr;
        }
    }
    /** Unit k, its knob count set (so its knobs glide: RackUnit), and its MULTIPLY and STRENGTH if it has them. */
    inline std::unique_ptr<RackUnit> make (int k)
    {
        auto u = makeUnit (k);
        if (u == nullptr) return u;
        u->setParamCount (info[k].numParams);
        int mul = -1, str = -1;
        std::vector<unsigned char> scaled ((size_t) info[k].numParams, 0);
        std::vector<float> lo ((size_t) info[k].numParams, 0.0f), hi ((size_t) info[k].numParams, 0.0f);
        for (int i = 0; i < info[k].numParams; ++i)
        {
            const auto& row = designed::params[(size_t) (info[k].firstParam + i)];
            lo[(size_t) i] = row.minValue; hi[(size_t) i] = row.maxValue;
            if (row.label == "MULTIPLY") { mul = i; continue; }
            if (row.label == "STRENGTH") { str = i; continue; }
            // (its amounts: knobs, but not its mix, output, gain, a threshold or ceiling, a frequency or a time)
            const bool amount = row.kind == 0 && i > 0 && row.label.find ("MIX") == std::string_view::npos && row.label.find ("OUTPUT") == std::string_view::npos
                             && row.label.find ("GAIN") == std::string_view::npos && row.label.find ("THRESH") == std::string_view::npos
                             && row.label.find ("CEILING") == std::string_view::npos && row.label.find ("VOLUME") == std::string_view::npos
                             && row.label.find ("TARGET") == std::string_view::npos
                             && row.label.find ("SMOOTH") == std::string_view::npos
                             && row.unit.find ("Hz") == std::string_view::npos && row.unit.find ("ms") == std::string_view::npos
                             && row.unit != " s" && row.unit.find ("st") == std::string_view::npos && row.unit.find ("ct") == std::string_view::npos;
            scaled[(size_t) i] = amount ? 1 : 0;
            // (VOCAL IDENTITY PROCESSOR: MULTIPLY scales the source's textures, STRENGTH the whole move - both inside the unit)
            if (std::string_view (info[k].key) == "voice" || std::string_view (info[k].key) == "vocalstation")
                scaled[(size_t) i] = 0;   // (it scales them itself, with the character's own: VoiceIdentity::render)
        }
        if (mul >= 0 && str >= 0)
            u->setModifiers (mul, str, std::move (scaled), std::move (lo), std::move (hi));
        return u;
    }
    /** RACK TUNER's place among the newer units (the engine trims the rack's level for it). */
    inline constexpr int indexOfKey (const char* key) noexcept
    {
        for (int k = 0; k < count; ++k)
        {
            const char* a = info[k].key; const char* b = key;
            while (*a != 0 && *a == *b) { ++a; ++b; }
            if (*a == 0 && *b == 0) return k;
        }
        return -1;
    }
    inline constexpr int tunerIndex = indexOfKey ("tuner");
    static_assert (tunerIndex >= 0, "Units.h: RACK TUNER is in UnitList.h");
    static_assert (count == 60, "Units.h: one case per unit in UnitList.h");
}
