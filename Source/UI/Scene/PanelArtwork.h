#pragma once

#include <juce_graphics/juce_graphics.h>

/*  2D artwork rendered with juce::Graphics on the message thread and uploaded
    as small textures:

    - Faceplate decal (RGBA): R = white silkscreen, G = grey section outlines,
                              B = section fields (slightly lighter panel), A = unused
    - Knob scale (R8): fixed printed scale ring around a knob (0-10, or 0-30 for NORM)
    - TONE & SPACE decal (R8) and live display labels (R8)
    - Display overlay (R8): phosphor text, re-rendered only when the text changes
*/
namespace pad::artwork
{
    struct RawTexture
    {
        int width = 0, height = 0, channels = 0;
        std::vector<juce::uint8> pixels;
    };

    /** Everything the renderer uploads once when the GL context is created. */
    struct TextureSet
    {
        RawTexture faceplateDecal, scale10, scale30, scale3, scale5, tubeDecal, seraphLabels;
        RawTexture tideDecal, lumenDecal, limiterDecal;              // the three 1U panels
        RawTexture tideVuFace, lumenVuFace, limiterVuFace[2];        // the printed faces of their VU meters
        RawTexture levelDecal, balancerDecal, monitorDecal;           // LEVEL (1U), MIX BALANCER (4U), MONITOR (3U)
        RawTexture levelVuFace, monitorVuFace[2];                     // INPUT; MOMENTARY and SHORT-TERM
        RawTexture monitorLabels, balancerLabels;                     // print inside their displays
    };

    /** One piece of printed text, in panel-local coordinates of its unit (for the hover callouts). */
    struct TextItem
    {
        int unit = 0;
        float x = 0.0f, z = 0.0f, halfW = 0.0f, halfH = 0.0f;
        juce::String text;
        int control = -1;       // control this text names (label), or -1
        int clarityScale = -1;  // printed CLARITY scale number: 0 = NORM scale, 1 = ADD scale, -1 = always shown
    };

    using TextRegistry = std::vector<TextItem>;

    RawTexture renderFaceplateDecal (int textureWidth, TextRegistry* registry = nullptr);
    RawTexture renderKnobScale (int size, int maxValue = 10);

    /** TONE & SPACE faceplate print (R8 white silkscreen) and the live display's labels (R8). */
    RawTexture renderTubeDecal (int textureWidth, TextRegistry* registry = nullptr);
    RawTexture renderSeraphDisplayLabels (int width, TextRegistry* registry = nullptr);

    /** One of the 1U panels (compressor, leveler, spectral limiter): print, scales with numbers, and its place in the chain. */
    RawTexture renderOneUDecal (int unit, int textureWidth, TextRegistry* registry = nullptr);

    /** Print inside the LEVEL & LOUDNESS waveform screen or the MIX BALANCER display (R8, uv across the window). */
    RawTexture renderWindowLabels (int unit, int width, TextRegistry* registry = nullptr, const juce::String& readout = {});

    /** A VU dial face: arc, ticks, numbers, red zone (green channel) and caption. `meter` picks the
        face where a unit's meters read different things (the limiter: 0 = spectral cut, 1 = broadband). */
    RawTexture renderVuFace (int unit, int width, TextRegistry* registry = nullptr, int meter = 0);

    /** Numbers of the printed scale rings around ENH Master's knobs (they are drawn per knob at render time). */
    void collectKnobScaleText (TextRegistry&);

    /** The zoomed text box shown next to hovered print: a title line and an optional value line. */
    RawTexture renderCallout (const juce::String& title, const juce::String& detail, float pixelScale);

    struct DisplayText
    {
        juce::String title, tag, lineLeft, focusLine;
        juce::String limitLine;   // what the SPECTRAL LIMITER is cutting, while it is

        bool operator== (const DisplayText& o) const
        {
            return title == o.title && tag == o.tag && lineLeft == o.lineLeft && focusLine == o.focusLine && limitLine == o.limitLine;
        }
    };

    inline constexpr int displayOverlayWidth = 1024, displayOverlayHeight = 180;

    RawTexture renderDisplayOverlay (const DisplayText&);
}
