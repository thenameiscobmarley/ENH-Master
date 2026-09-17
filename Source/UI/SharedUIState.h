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

        // written by render thread
        std::atomic<float> parallaxX { 0.0f }, parallaxY { 0.0f };
        std::atomic<bool>  pointerInside { false };
        std::atomic<bool>  renderInteraction { false };  // true: clicks/drags handled on the render thread
        std::atomic<int>   renderDragParam { -1 };        // parameter being dragged by the render thread
        std::atomic<double> renderPressMs { -1.0e9 };     // when the render thread last handled a press
        std::atomic<bool>  animating { true };

        // scope overlay hand-off
        juce::SpinLock        overlayLock;
        artwork::RawTexture   overlayPending;     // guarded by overlayLock
        juce::uint32          overlayVersion = 0; // guarded by overlayLock
    };
}
