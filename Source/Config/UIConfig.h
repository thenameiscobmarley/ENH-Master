#pragma once

#include <juce_core/juce_core.h>

namespace pad
{
    /** UI / rendering preferences, loaded from
        ~/.config/PvPAdaptiveDynamics/ui-config.json (created with defaults if missing).

        All values are validated and clamped; invalid entries fall back to defaults
        and are reported in `warnings`. The future DSP config loader will live
        beside this one and reuse the same validate-then-apply pattern.
    */
    struct UIConfig
    {
        static constexpr int schemaVersion = 1;

        int   frameRate       = 60;     // active rendering rate (Hz)
        int   idleFrameRate   = 30;     // when nothing is moving and mouse is outside
        int   msaaSamples     = 4;      // 0, 2, 4
        int   anisotropy      = 4;      // 1..8
        int   panelTextureWidth = 2048; // 1024 or 2048 (height is half)
        float parallaxAmount  = 1.0f;   // 0..2
        bool  reduceMotion    = false;  // disables idle sheen + parallax

        juce::StringArray warnings;

        static juce::File getDefaultFile();
        static UIConfig loadOrCreate (const juce::File& file = getDefaultFile());

        juce::var toVar() const;
        static UIConfig fromVar (const juce::var&);
    };
}
