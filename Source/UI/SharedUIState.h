#pragma once

#include <juce_core/juce_core.h>
#include "Scene/PanelArtwork.h"

namespace pad
{
    /** Lock-free (or spin-locked) state shared between the message thread
        (input, text) and the OpenGL render thread. */
    struct SharedUIState
    {
        // written by message thread
        std::atomic<float> mouseNdcX { 0.0f }, mouseNdcY { 0.0f };
        std::atomic<bool>  mouseInside { false };
        std::atomic<int>   hoveredControl { -1 };
        std::atomic<int>   activeControl { -1 };
        std::atomic<int>   viewWidth { 1 }, viewHeight { 1 };

        // for render-thread pointer polling (X11): peer window + this view's offset inside it
        std::atomic<juce::uint64> nativeWindow { 0 };
        std::atomic<int>   viewOffsetX { 0 }, viewOffsetY { 0 };
        std::atomic<float> platformScale { 1.0f };
        std::atomic<bool>  dragging { false };
        std::atomic<bool>  calloutAtPointer { true };   // false: loupe locks to the anchor below

        // camera focus: which unit is being looked at, and how far in (0 = the whole rack)
        std::atomic<int>   focusUnit { 0 };
        std::atomic<float> focusTarget { 0.0f };   // message thread asks
        std::atomic<float> focusAmount { 0.0f };   // render thread animates, both threads read

        // written by render thread
        std::atomic<float> parallaxX { 0.0f }, parallaxY { 0.0f };
        std::atomic<bool>  pointerInside { false };
        std::atomic<float> pointerNdcX { 0.0f }, pointerNdcY { 0.0f };   // polled pointer, 1 per frame
        std::atomic<bool>  renderInteraction { false };  // true: clicks/drags handled on the render thread
        std::atomic<int>   renderDragParam { -1 };        // parameter being dragged by the render thread
        std::atomic<double> renderPressMs { -1.0e9 };     // when the render thread last handled a press
        std::atomic<bool>  animating { true };

        // scope overlay hand-off
        juce::SpinLock        overlayLock;
        artwork::RawTexture   overlayPending;     // guarded by overlayLock
        juce::uint32          overlayVersion = 0; // guarded by overlayLock

        // LEVEL & LOUDNESS screen print (its readout line changes) hand-off
        juce::SpinLock        levelLabelsLock;
        artwork::RawTexture   levelLabelsPending; // guarded by levelLabelsLock
        juce::uint32          levelLabelsVersion = 0;

        // hover callout (zoomed text box + leader line) hand-off
        juce::SpinLock        calloutLock;
        artwork::RawTexture   calloutPending;     // guarded by calloutLock
        juce::uint32          calloutVersion = 0; // guarded by calloutLock
        std::atomic<bool>     calloutVisible { false };
        std::atomic<int>      calloutUnit { 0 };
        std::atomic<float>    calloutX { 0.0f }, calloutZ { 0.0f };   // anchor, panel-local
        std::atomic<float>    calloutPixelScale { 2.0f };           // image pixels per logical pixel
        std::atomic<bool>     calloutHasPill { false };             // a name + value pill under the loupe (controls)
    };
}
