#pragma once

#include <juce_opengl/juce_opengl.h>

#include "../Render/GLResources.h"
#include "../Render/Shaders.h"
#include "../Controls/ControlAnimation.h"
#include "../SharedUIState.h"
#include "../../Config/UIConfig.h"
#include "DeviceLayout.h"
#include "PanelArtwork.h"
#include "CameraRig.h"

namespace pad
{
    /** Renders the rack unit on the OpenGL thread. Reads parameters through the
        bridge (atomics only) and never writes them.

        Frame pacing is done here, on the render thread, with continuous
        repainting locked to vsync: every refresh while interacting/animating,
        every second refresh when idle. */
    class HardwareRenderer final : public juce::OpenGLRenderer
    {
    public:
        HardwareRenderer (ParameterBridge&, SharedUIState&, const UIConfig&,
                          artwork::RawTexture faceplateDecal, artwork::RawTexture knobDial);
        ~HardwareRenderer() override;

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        static gfx::Vec3 sourceColour (ControlSource) noexcept;

    private:
        struct Meshes
        {
            gfx::GpuMesh table, quad, chassis, lidScrews, feet,
                         faceEdges, faceTop, displayWalls, displayGlass, displayBezel,
                         ventWalls, ventFloors, earWalls, earFloors, screws, handles,
                         knobBezel, knobSkirt, knobCap, knobInsert, indicator,
                         switchPlate, switchBushing, switchLever, led;

            template <typename Fn> void forEach (Fn&& fn)
            {
                for (auto* m : { &table, &quad, &chassis, &lidScrews, &feet,
                                 &faceEdges, &faceTop, &displayWalls, &displayGlass, &displayBezel,
                                 &ventWalls, &ventFloors, &earWalls, &earFloors, &screws, &handles,
                                 &knobBezel, &knobSkirt, &knobCap, &knobInsert, &indicator,
                                 &switchPlate, &switchBushing, &switchLever, &led })
                    fn (*m);
            }
        };

        void updateAnimation (float dt);
        void paceFrame (double frameStartMs);
        void drawScene (const CameraRig&, int viewportW, int viewportH);

        gfx::ShaderProgram& use (int material);
        void draw (const gfx::GpuMesh&, const gfx::Mat4& model, gfx::Vec3 base, gfx::Vec3 emissive = {});
        void drawShadow (const gfx::Mat4& space, float cx, float y, float cz, float hw, float hd,
                         float radius, float blur, float strength);
        void uploadOverlayIfChanged();
        void recordStats (double frameStartMs, double renderMs);

        ParameterBridge& bridge;
        SharedUIState& shared;
        const UIConfig config;

        artwork::RawTexture decalData, dialData;
        std::vector<juce::uint8> overlayScratch;
        juce::uint32 uploadedOverlayVersion = 0;

        std::array<gfx::ShaderProgram, shaders::numMaterials> programs;
        std::array<juce::uint32, shaders::numMaterials> programFrame {};
        gfx::ShaderProgram* current = nullptr;
        juce::uint32 frameIndex = 1;
        CameraRig frameCamera;
        int frameW = 1, frameH = 1;

        gfx::Texture2D decalTex, dialTex, overlayTex;
        Meshes meshes;
        bool ready = false;

        std::array<int, layout::numControls> controlParam {};
        std::array<anim::KnobAnimator, layout::numControls> knobs {};
        std::array<anim::SwitchAnimator, layout::numControls> switches {};

        float parallaxX = 0.0f, parallaxY = 0.0f, stepGlow = 0.0f;
        double lastFrameMs = 0.0, timeSeconds = 0.0, busyUntilMs = 0.0;
        int swapInterval = -1;

        const bool statsEnabled = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_STATS", {}).isNotEmpty();
        double statStart = 0.0, statSum = 0.0, statMax = 0.0, statRenderSum = 0.0;
        int statCount = 0, statLong = 0;

        JUCE_DECLARE_NON_COPYABLE (HardwareRenderer)
    };
}
