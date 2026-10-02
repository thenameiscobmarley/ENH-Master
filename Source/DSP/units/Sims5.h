#pragma once

#include "RackUnit.h"

/*  RACK TUNER: tunes the whole rack from words (UI/RackTuner.h does the choosing, on the message thread; its
    screen is drawn from what it chose). Its own sound passes untouched: what it does to the level - the rack
    matched to how loud it was before a tune - happens at the end of the chain (DSP/TunerMatch.h), where the
    whole rack's change can be heard. Its LED ladder shows how far that match is trimming. */
namespace enh::dsp::units
{
    class RackTunerUnit final : public RackUnit
    {
    private:
        void prepareUnit (double, int) override {}
        void resetUnit() override {}
        void render (float* const*, int, const float*) noexcept override {}   // (the engine sets its meter: TunerMatch)
    };
}
