#pragma once

#include <juce_opengl/juce_opengl.h>

#include "../HardwareKit.h"
#include "../Render/PluginMaterials.h"
#include "../../Parameters/ParameterBridge.h"
#include "../SharedUIState.h"
#include "../../Config/UIConfig.h"
#include "DeviceLayout.h"
#include "PanelArtwork.h"
#include "CameraRig.h"
#include "../../DSP/EngineMeters.h"
#include "../../DSP/SpectrumScope.h"
#include "../DisplayHistory.h"

namespace pad
{
    /** Renders the rack unit on the OpenGL thread. Reads parameters through the
        bridge (atomics only); writes them only for clicks/drags taken from the polled pointer.

        Frame pacing is done here, on the render thread, with continuous
        repainting locked to vsync: every refresh while interacting/animating,
        every second refresh when idle. */
    class HardwareRenderer final : public juce::OpenGLRenderer
    {
    public:
        HardwareRenderer (ParameterBridge&, SharedUIState&, const enh::dsp::EngineMeters&, const UIConfig&,
                          artwork::TextureSet textures, const enh::dsp::ScopeCurve&,
                          const enh::dsp::ScopeCurve& balancerScope, const DisplayHistory&);
        ~HardwareRenderer() override;

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;


    private:
        struct Meshes
        {
            gfx::GpuMesh table, quad,
                         faceEdges, faceTop, displayWalls, displayGlass, displayBezel,
                         earWalls, earFloors, screws, screwSlots,
                         scaleRing, arcRing, led,
                         tubeFaceTop, tubeFaceEdges, tubeEarWalls, tubeEarFloors, tubeScrews, tubeScrewSlots,
                         seraphWalls, seraphGlass, seraphBezel,
                         levelScopeWalls, levelScopeGlass, levelScopeBezel, balancerWalls, balancerGlass, balancerBezel,
                         enhBody, tubeBody, tubeVents, tubeVentWalls, tubeVentFloors, bodyScrews,
                         caseCheeks, caseRails, caseFrontRails, caseRailHoles, caseEdges;

            template <typename Fn> void forEach (Fn&& fn)
            {
                for (auto* m : { &table, &quad,
                                 &faceEdges, &faceTop, &displayWalls, &displayGlass, &displayBezel,
                                 &earWalls, &earFloors, &screws, &screwSlots,
                                 &scaleRing, &arcRing, &led,
                                 &tubeFaceTop, &tubeFaceEdges, &tubeEarWalls, &tubeEarFloors, &tubeScrews, &tubeScrewSlots,
                                 &seraphWalls, &seraphGlass, &seraphBezel,
                                 &levelScopeWalls, &levelScopeGlass, &levelScopeBezel, &balancerWalls, &balancerGlass, &balancerBezel,
                                 &enhBody, &tubeBody, &tubeVents, &tubeVentWalls, &tubeVentFloors, &bodyScrews,
                                 &caseCheeks, &caseRails, &caseFrontRails, &caseRailHoles, &caseEdges })
                    fn (*m);
            }
        };

        /** The shell of an outboard unit (the 1U units, LEVEL & LOUDNESS, MIX BALANCER): its own height,
            ear slots, and holes for its meters and windows. */
        struct OutboardMeshes
        {
            gfx::GpuMesh faceTop, faceEdges, earWalls, earFloors, screws, screwSlots, body;
            template <typename Fn> void forEach (Fn&& fn)
            {
                for (auto* m : { &faceTop, &faceEdges, &earWalls, &earFloors, &screws, &screwSlots, &body })
                    fn (*m);
            }
        };
        std::array<OutboardMeshes, layout::numUnits> outboard;

        /** A HardwareKit model on the GPU: one mesh per part plus its material hints. */
        struct GpuModel
        {
            struct Part
            {
                gfx::GpuMesh mesh;
                hwk::models::Role role = hwk::models::Role::body;
                bool rotates = true, brushed = false;
                gfx::Vec3 colour;
                float ridges = 0.0f, ridgesBelowY = 0.0f, polish = 0.6f;
            };

            std::vector<std::unique_ptr<Part>> parts;
            float footprint = 0.12f;
            float shadowRadius = 0.12f, beakLength = 0.0f, beakHalfWidth = 0.0f, pivotOffset = 0.0f;

            void upload (const hwk::models::Model&);
            void release();
        };

        /** moving = matrix for parts that turn / press; fixed = for the rest. accentGain scales accent colours.
            litPointer: the pointer part glows (lamps, jewels); otherwise it is paint, lit like the knob. */
        float autoTurnedValue (int control, float value) const;
        static constexpr int autoOutputRole = 7;
        std::array<int, layout::numControls> autoRole {};
        std::array<const params::Spec*, layout::numControls> autoSpec {};
        int silkAutoParam = -1, seraphMultiplyParam = -1;
        const params::Spec* silkAutoSpec = nullptr;
        const params::Spec* seraphMultiplySpec = nullptr;

        void drawModel (const GpuModel&, const gfx::Mat4& moving, const gfx::Mat4& fixed, gfx::Vec3 hoverLift,
                        gfx::Vec3 pointerColour, float accentGain = 1.0f, bool litPointer = true);

        void renderLoupeView (const CameraRig&, int viewportW, int viewportH);
        void drawLoupe (int viewportW, int viewportH);
        void queueGlow (const gfx::Mat4& space, float x, float z, float size, gfx::Vec3 colour, float intensity) noexcept;

        void updateAnimation (float dt);
        void paceFrame (double frameStartMs);
        void drawScene (const CameraRig&, int viewportW, int viewportH);
        void drawLed (const gfx::Mat4& panel, float x, float z, gfx::Vec3 colour, float on);

        gfx::ShaderProgram& use (int material);
        void draw (const gfx::GpuMesh&, const gfx::Mat4& model, gfx::Vec3 base, gfx::Vec3 emissive = {});
        void drawShadow (const gfx::Mat4& space, float cx, float y, float cz, float hw, float hd,
                         float radius, float blur, float strength);
        void uploadOverlayIfChanged();
        void uploadCalloutIfChanged();
        void pollPointer() noexcept;
        void handleInteraction() noexcept;
        void recordStats (double frameStartMs, double renderMs);
        int parameterFor (int control) const noexcept;

        ParameterBridge& bridge;
        SharedUIState& shared;
        const enh::dsp::EngineMeters& meters;
        const enh::dsp::ScopeCurve& scope;
        const enh::dsp::ScopeCurve& balancerScope;
        const DisplayHistory& history;

        // LEVEL & LOUDNESS waveform (R = the band now, G = its afterimage) and the MIX BALANCER's data
        // (spectrum row + history row), rebuilt every frame from the histories
        gfx::Texture2D waveTex, balancerDataTex;
        std::vector<juce::uint8> waveScratch, balancerScratch;
        std::array<float, DisplayHistory::waveColumns> waveGhost {};
        std::array<float, 6> balancerBands {};
        void uploadDisplays (float dt);
        void uploadLevelLabelsIfChanged();
        juce::uint32 uploadedLevelLabelsVersion = 0;

        /** The analyser curve, uploaded once a frame as a strip: R = input, G = output, B = peak. */
        gfx::Texture2D scopeTex;
        std::vector<juce::uint8> scopeScratch;
        void uploadScope();
        PointerPoller pointer;
        const UIConfig config;

        artwork::TextureSet textureData;
        std::vector<juce::uint8> overlayScratch;
        juce::uint32 uploadedOverlayVersion = 0;

        std::array<gfx::ShaderProgram, shaders::numMaterials> programs;
        std::array<juce::uint32, shaders::numMaterials> programFrame {};
        gfx::ShaderProgram* current = nullptr;
        juce::uint32 frameIndex = 1;
        CameraRig frameCamera;
        int frameW = 1, frameH = 1;
        float vignette = 1.0f;   // 0 while rendering the zoomed loupe view

        gfx::Texture2D decalTex, scaleTex, scaleWideTex, scale3Tex, scale5Tex, tubeDecalTex, seraphLabelTex, overlayTex, calloutTex;
        gfx::Texture2D tideDecalTex, lumenDecalTex, limiterDecalTex, tideLabelTex, lumenLabelTex;
        std::array<gfx::Texture2D, 2> limiterLabelTex;   // SPECTRAL and BROADBAND faces
        gfx::Texture2D levelDecalTex, balancerDecalTex, levelLabelTex, balancerLabelTex;
        std::array<gfx::Texture2D, 2> levelFaceTex;      // MOMENTARY and SHORT-TERM faces

        /** A VU movement: the needle has mass, so it swings toward the reading and overshoots
            a little, the way a real moving coil does. */
        struct Needle
        {
            float angle = 0.0f, velocity = 0.0f;
            void update (float target, float dt) noexcept;
        };

        // [0] compressor, [1..3] leveler low / mid / high, [4..5] limiter spectral / broadband,
        // [6..7] loudness momentary / short-term (layout::firstNeedle)
        std::array<Needle, layout::numNeedles> needles {};
        std::array<float, layout::numUnits> unitLamp {};   // backlight per outboard unit, on with IN (or always)

        GpuModel tideVu, lumenVu, limiterVu, levelVu;      // HardwareKit VU models, one per size
        GpuModel& vuModelFor (int unit) noexcept
        {
            return unit == layout::tideUnit ? tideVu : unit == layout::lumenUnit ? lumenVu : unit == layout::levelUnit ? levelVu : limiterVu;
        }

        /** An outboard unit. faces: the dial print per meter (one texture shared by all of a unit's meters, or one each). */
        void drawOneU (int unit, const gfx::Mat4& panel, gfx::Vec3 colour, const gfx::Texture2D& decal,
                       std::initializer_list<const gfx::Texture2D*> faces);
        void drawWindows (const gfx::Mat4& levelPanel, const gfx::Mat4& balancerPanel);
        void drawVuGlass (int unit, const gfx::Mat4& panel);

        // Models from HardwareKit, each at every level of detail: one per distinct knob (style, radius,
        // accent); shared button and rocker models. Each frame a control is drawn at the level its size on
        // screen calls for (detailFor), so walking up to the rack or zooming the loupe brings in finer
        // tessellation and the carved grip detail.
        static constexpr int numDetail = hwk::models::numDetailLevels;
        std::vector<std::unique_ptr<GpuModel>> knobModels;
        std::array<std::array<int, numDetail>, layout::numControls> knobModelIndex {};
        std::array<int, layout::numControls> ringScale {};
        std::array<std::array<GpuModel, numDetail>, hwk::models::numButtonStyles> buttonModels;
        std::array<std::array<GpuModel, numDetail>, hwk::models::numSwitchStyles> switchModels;
        std::array<float, hwk::models::numSwitchStyles> switchPivotY {}, switchAngle {};

        // Dev only (PAD_UI_TEST_GALLERY=1): every knob, switch and button style laid out on the enhancer
        const bool gallery = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_GALLERY", {}).isNotEmpty();
        std::vector<std::unique_ptr<GpuModel>> galleryKnobs;
        GpuModel lampModel;

        /** Level of detail for something of `radius` at panel (x, z) of `panel`, seen by `cam` in a viewport `viewportW` wide. */
        int detailFor (const CameraRig& cam, const gfx::Mat4& panel, float x, float z, float radius, int viewportW) const noexcept;

        // Fisheye loupe over hovered print
        gfx::RenderTarget loupeTarget;
        float loupeAlpha = 0.0f, loupeZoom = 1.8f, loupeRadius = 90.0f, loupeCx = 0.0f, loupeCy = 0.0f;
        int loupeUnit = 0;
        float loupeX = 0.0f, loupeZ = 0.0f;
        bool loupeReady = false;

        // Additive LED / lamp halos, collected while drawing and composited at the end
        struct Glow { gfx::Mat4 model; gfx::Vec3 colour; float intensity; };
        std::array<Glow, 96> glows {};
        int glowCount = 0;
        std::vector<juce::uint8> calloutScratch;
        juce::uint32 uploadedCalloutVersion = 0;
        int calloutW = 0, calloutH = 0;
        Meshes meshes;
        bool ready = false;

        // Parameter indices per control: primary, alternative (mode-switched knob), mode button
        std::array<int, layout::numControls> controlParam {}, altParam {}, modeParam {};
        std::array<anim::KnobAnimator, layout::numControls> knobs {};
        std::array<anim::ButtonAnimator, layout::numControls> buttons {};
        std::array<anim::ToggleAnimator, layout::numControls> toggles {};

        // SERAPH: live display values, lamp, backlight and lid glow
        std::array<float, 28> seraphDips {};       // smoothed for display (dB)
        std::array<float, 18> seraphColumns {};    // per column x channel, 0..1 (LEVEL: -1..1)
        float tubePower = 0.0f, tubeWarmth = 0.0f;
        int seraphModeParam = -1;
        int footstepControl = -1, modeControl = -1, heldButton = -1;
        float modeBlend = -1.0f, swapPulse = 0.0f;   // 0 = NORM scale, 1 = ADD scale
        bool lastAddMode = false;

        // LED ladders (smoothed segment brightness, bottom to top)
        std::array<float, layout::ladderSegments> outLeds {}, enhLeds {}, detectLeds {};

        float parallaxX = 0.0f, parallaxY = 0.0f;
        float focusAmount = 0.0f;      // animated toward shared.focusTarget
        std::array<float, enh::dsp::numBands> displayBands {};
        std::array<float, 48> limitCurve {};       // SPECTRAL LIMITER cut on the analyser's axis (dB), smoothed
        float limitBroadband = 0.0f;
        float stepFlash = 0.0f, activityGlow = 0.0f;
        bool pointerInside = false, pointerPolled = false, leftDown = false, lastLeftDown = false, fineDrag = false;
        float pointerNdcX = 0.0f, pointerNdcY = 0.0f, pointerX = 0.0f, pointerY = 0.0f, lastPointerX = 0.0f, lastPointerY = 0.0f;
        int dragControl = -1, dragParam = -1;
        float dragValue = 0.0f;
        double lastFrameMs = 0.0, timeSeconds = 0.0, busyUntilMs = 0.0;
        int swapInterval = -1;

        const bool demoMeters = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_DEMO", {}).isNotEmpty();
        // Finest level of detail allowed (config maxDetail; PAD_UI_TEST_MAX_DETAIL overrides it, dev only)
        const int maxDetail = juce::jlimit (0, 3, juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_MAX_DETAIL", {}).isNotEmpty()
                                                     ? juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_MAX_DETAIL", {}).getIntValue()
                                                     : config.maxDetail);
        const bool statsEnabled = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_STATS", {}).isNotEmpty();
        double statStart = 0.0, statSum = 0.0, statMax = 0.0, statRenderSum = 0.0;
        int statCount = 0, statLong = 0;

        JUCE_DECLARE_NON_COPYABLE (HardwareRenderer)
    };
}
