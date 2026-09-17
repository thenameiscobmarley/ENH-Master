#pragma once

#include <juce_opengl/juce_opengl.h>

#include "../Render/GLResources.h"
#include "../Controls/ControlAnimation.h"
#include "../SharedUIState.h"
#include "../../Config/UIConfig.h"
#include "DeviceLayout.h"
#include "PanelArtwork.h"
#include "CameraRig.h"

namespace pad
{
    /** Renders the 3D hardware scene on the OpenGL thread. Reads parameters
        through the bridge (atomics only) and never writes them. */
    class HardwareRenderer final : public juce::OpenGLRenderer
    {
    public:
        HardwareRenderer (ParameterBridge&, SharedUIState&, const UIConfig&,
                          artwork::RawTexture panelDecal, artwork::RawTexture knobDial);
        ~HardwareRenderer() override;

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        static gfx::Vec3 sourceColour (ControlSource) noexcept;

    private:
        struct Meshes
        {
            gfx::GpuMesh table, shadowQuad, chassis, ears, feet, panelEdges, panelTop, screws,
                         flange, capBody, capInsert, pointer, ledRing,
                         switchWalls, switchFloors, blade, hub,
                         scopeWalls, scopeGlass, ventWalls, ventFloors, led;

            template <typename Fn> void forEach (Fn&& fn)
            {
                for (auto* m : { &table, &shadowQuad, &chassis, &ears, &feet, &panelEdges, &panelTop, &screws,
                                 &flange, &capBody, &capInsert, &pointer, &ledRing,
                                 &switchWalls, &switchFloors, &blade, &hub,
                                 &scopeWalls, &scopeGlass, &ventWalls, &ventFloors, &led })
                    fn (*m);
            }
        };

        void updateAnimation (float dt);
        void drawScene (const CameraRig&, int viewportW, int viewportH);
        void draw (const gfx::GpuMesh&, const gfx::Mat4& model, int material, gfx::Vec3 base, gfx::Vec3 emissive = {});
        void drawShadow (float cx, float y, float cz, float hw, float hd, float radius, float blur, float strength);
        void uploadOverlayIfChanged();

        int paramIndexForControl (int controlIndex) const noexcept;

        ParameterBridge& bridge;
        SharedUIState& shared;
        const UIConfig config;

        artwork::RawTexture decalData, dialData;
        std::vector<juce::uint8> overlayScratch;
        juce::uint32 uploadedOverlayVersion = 0;

        gfx::ShaderProgram shader;
        gfx::Texture2D decalTex, dialTex, overlayTex;
        Meshes meshes;
        bool ready = false;

        std::array<int, layout::numControls> fixedParam {};
        std::array<int, params::numPdTargets> kpParam {}, kdParam {};
        int maskParam = -1, stepParam = -1;

        std::array<anim::KnobAnimator, layout::numControls> knobs {};
        std::array<anim::SwitchAnimator, layout::numControls> switches {};

        float parallaxX = 0.0f, parallaxY = 0.0f;
        float maskGlow = 0.0f, stepGlow = 0.0f, focusGlow[params::numPdTargets] {};
        double lastFrameMs = 0.0, timeSeconds = 0.0;

        JUCE_DECLARE_NON_COPYABLE (HardwareRenderer)
    };
}
