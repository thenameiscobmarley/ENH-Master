#pragma once

#include "UnitList.h"
#include "Reverbs.h"
#include "TimeFx.h"
#include "Dynamics.h"
#include "Space.h"
#include "Harsh.h"
#include "Exciter.h"
#include "RayRoom.h"
#include "Sims.h"
#include "Sims2.h"

/*  The newer units, made by their place in UnitList.h (Tools/units/gen_units.py writes that list: keep the
    order of this switch the same as its). */
namespace enh::dsp::units
{
    inline std::unique_ptr<RackUnit> make (int k)
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
            default: return nullptr;
        }
    }
    static_assert (count == 33, "Units.h: one case per unit in UnitList.h");
}
