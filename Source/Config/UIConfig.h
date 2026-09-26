#pragma once

#include <juce_core/juce_core.h>

namespace pad
{
    /** UI / rendering preferences, loaded from
        ~/.config/ENHMaster/ui-config.json (created with defaults if missing).

        All values are validated and clamped; invalid entries fall back to defaults
        and are reported in `warnings`. The future DSP config loader will live
        beside this one and reuse the same validate-then-apply pattern.
    */
    struct UIConfig
    {
        static constexpr int schemaVersion = 2;   // 2: panel print up to 4096 px (was 2048)

        int   frameRate       = 60;     // active rendering rate (Hz)
        int   idleFrameRate   = 30;     // when nothing is moving and mouse is outside
        int   msaaSamples     = 4;      // 0, 2, 4, 8 (main view and loupe)
        int   anisotropy      = 4;      // 1..8
        int   maxDetail       = 3;      // 0..3: finest level of detail the models may use up close
        int   panelTextureWidth = 4096; // 1024, 2048 or 4096: printed panels (mipmapped: the GPU picks the
                                        // resolution the camera needs, so print sharpens as you walk up)
        float renderScale     = 0.0f;   // supersampling: 0 = auto (1x), or 1..2 (draw finer than the screen, filter down)
        float parallaxAmount  = 1.0f;   // 0..2
        bool  reduceMotion    = false;  // disables idle sheen + parallax
        float wetCoat         = 1.0f;   // 0..1: the clear coat's strength (the wet look); 0 saves ~2-4 ms a frame on a weak GPU
        bool  simpleView      = false;  // SIMPLE: only the units you use are in the case (right-click the rack).
                                        // A new config starts SIMPLE; one written before it existed stays FULL

        /** Remember the view (right-click menu): rewrites only that key in the file. */
        static void saveSimpleView (bool simple, const juce::File& file = getDefaultFile());

        juce::StringArray warnings;

        static juce::File getDefaultFile();
        static UIConfig loadOrCreate (const juce::File& file = getDefaultFile());

        juce::var toVar() const;
        static UIConfig fromVar (const juce::var&);
    };
}
