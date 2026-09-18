#pragma once

#include "PanelArtwork.h"
#include <juce_core/juce_core.h>

namespace pad::audit
{
    /** Dev-only (PAD_UI_DUMP_ARTWORK=<dir>): writes every printed panel at full resolution with
        the registered text boxes (green) and the hardware footprints taken from the layout code
        (red) drawn over it, plus clearances.txt listing every piece of print that overlaps, or
        comes closer than a small margin to, hardware, a section border or other print.

        Everything is measured in panel-local units from the same constants the renderer and the
        artwork use, so it checks the real bounds rather than a perspective screenshot. */
    void writeLayoutAudit (const artwork::TextureSet&, const artwork::TextRegistry&, const juce::File& directory);
}
