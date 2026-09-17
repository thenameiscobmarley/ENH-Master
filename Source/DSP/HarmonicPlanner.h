#pragma once

#include "BandAnalyzer.h"

namespace enh::dsp
{
    /** Decides WHERE harmonic enhancement is useful for the current programme (control rate).

        For every candidate source band it asks two questions:
          - is there real content here?                          (level vs. the loudest band)
          - are its 2nd/3rd harmonic regions comparatively empty? (spectral deficit one to
                                                                   1.6 octaves above it)
        The best-scoring source becomes the centre of an exciter band; the score sets how much
        harmonic energy is worth adding. Two bands are planned:

          depth    sources 70-500 Hz   -> harmonics that give weight/body on headsets
          clarity  sources 600-5000 Hz -> harmonics that give definition and edge

        plus the bass fundamental for SUB's psychoacoustic harmonics. Centres glide in log
        frequency (speed follows ADAPT), so the enhancement follows the material without jumps.

        Coupling with the EQ lift (ADD mode):
          need     how much the harmonics are missing (0 = the region is already full of them).
                   The EQ lifts MORE when need is low, so the exciter still has material to work on.
          rescue   the harmonic region is thinning out right now (fast vs. slow harmonic-to-source
                   ratio at the centre). The exciter is driven by max (need, rescue), so falling
                   harmonics are topped up immediately.
    */
    class HarmonicPlanner
    {
    public:
        struct Band
        {
            float hz = 1000.0f;
            float amount = 0.0f;   // exciter drive: max (need, rescue)
            float need = 0.0f;     // harmonics missing at the planned source (0..1)
            float rescue = 0.0f;   // harmonics currently falling away (0..1)
        };

        void prepare (const BandAnalyzer&);
        void reset();
        void update (const BandAnalyzer&, float speed, float dt) noexcept;

        Band depth { 180.0f, 0.0f }, clarity { 2000.0f, 0.0f };
        float bassHz = 60.0f;

    private:
        struct Range { int lo = 0, hi = 0; };
        struct Tracker { float logHz = 0.0f, ratioFast = 0.0f, ratioSlow = 0.0f; bool primed = false; };
        void plan (const std::array<float, numBands>& level, float loudest, Range, Band&, Tracker&, float speed, float dt) noexcept;

        Range depthRange, clarityRange, bassRange;
        Tracker depthTrack, clarityTrack;
        float bassLog = 0.0f;
        int activeCount = 0;
    };
}
