#include "StudioWall.h"
#include "HardwareRenderer.h"
#include "GeometryFactory.h"
#include "Picking.h"
#include "LimiterDemo.h"
#include "../GlassPanel.h"

using namespace juce::gl;

namespace pad
{
    using gfx::Mat4;
    using gfx::Vec3;
    using namespace layout;

    namespace colours
    {
        constexpr Vec3 chassisBlack { 0.045f, 0.046f, 0.050f };
        constexpr Vec3 ledGreen     { 0.25f, 1.00f, 0.20f };
        constexpr Vec3 ledYellow    { 1.00f, 0.78f, 0.10f };
        constexpr Vec3 ledRed       { 1.00f, 0.12f, 0.06f };
        constexpr Vec3 amber        { 1.00f, 0.55f, 0.15f };
        constexpr Vec3 knobBlack    { 0.030f, 0.030f, 0.033f };
        constexpr Vec3 buttonGrey   { 0.40f, 0.41f, 0.43f };
        constexpr Vec3 seraphPurple { 0.21f, 0.13f, 0.30f };   // deep aubergine enamel
        constexpr Vec3 chrome      { 0.92f, 0.90f, 0.93f };
    }

    static Vec3 mixVec (Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }
    static float saturateUi (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }

    //==============================================================================
    HardwareRenderer::HardwareRenderer (ParameterBridge& b, SharedUIState& s, const enh::dsp::EngineMeters& m,
                                        const UIConfig& c, artwork::TextureSet textures, const enh::dsp::ScopeCurve& sc,
                                        const enh::dsp::ScopeCurve& bsc, const DisplayHistory& dh)
        : bridge (b), shared (s), meters (m), scope (sc), balancerScope (bsc), history (dh), config (c), textureData (std::move (textures))
    {
        seraphModeParam = bridge.indexOf (params::id::seraphMode);

        // Which knobs an auto mode turns (AUTO heaven: REVERB .. SUB; MATCH: OUTPUT), resolved once
        {
            namespace id = params::id;
            const char* chosen[7] { id::haloSpace, id::haloDecay, id::haloShimmer, id::haloTone, id::haloWidth, id::silkAir, id::silkSub };
            for (int i = 0; i < numControls; ++i)
            {
                const std::string_view pid (controls[(size_t) i].paramId);
                autoRole[(size_t) i] = -1;
                for (int k = 0; k < 7; ++k)
                    if (pid == chosen[k])
                        autoRole[(size_t) i] = k;
                if (pid == id::silkOutput)
                    autoRole[(size_t) i] = autoOutputRole;
                autoSpec[(size_t) i] = autoRole[(size_t) i] >= 0 ? params::findSpec (controls[(size_t) i].paramId) : nullptr;
            }
            silkAutoParam = bridge.indexOf (id::silkAuto);
            silkAutoSpec = params::findSpec (id::silkAuto);
            seraphMultiplyParam = bridge.indexOf (id::seraphMultiply);
            seraphMultiplySpec = params::findSpec (id::seraphMultiply);
        }

        for (int i = 0; i < numControls; ++i)
        {
            const auto& def = controls[(size_t) i];
            controlParam[(size_t) i] = bridge.indexOf (def.paramId);
            altParam[(size_t) i] = def.altParamId != nullptr ? bridge.indexOf (def.altParamId) : -1;
            modeParam[(size_t) i] = def.modeParamId != nullptr ? bridge.indexOf (def.modeParamId) : -1;
        }

        footstepControl = controlIndex (params::id::footstep);
        modeControl = controlIndex (params::id::clarityMode);

        // Which printed scale ring each ENH knob wears (by its parameter's range)
        for (int i = 0; i < numControls; ++i)
            if (auto* spec = params::findSpec (controls[(size_t) i].paramId))
                ringScale[(size_t) i] = spec->maxValue <= 3.01f ? 3 : spec->maxValue <= 5.01f ? 5 : 10;
    }

    //==============================================================================
    void HardwareRenderer::GpuModel::upload (const hwk::models::Model& model)
    {
        release();
        for (auto& part : model.parts)
        {
            auto gpu = std::make_unique<Part>();
            gpu->mesh.upload (part.mesh);
            gpu->role = part.role;
            gpu->rotates = part.rotates;
            gpu->brushed = part.brushedRings;
            gpu->colour = part.colour;
            gpu->ridges = part.ridges;
            gpu->ridgesBelowY = part.ridgesBelowY;
            gpu->polish = part.polish;
            parts.push_back (std::move (gpu));
        }
        footprint = model.footprintRadius;
        shadowRadius = model.bodyShadowRadius();
        beakLength = model.beakLength;
        beakHalfWidth = model.beakHalfWidth;
        pivotOffset = model.pivotOffset;
    }

    void HardwareRenderer::GpuModel::release()
    {
        for (auto& p : parts)
            p->mesh.release();
        parts.clear();
    }

    void HardwareRenderer::drawModel (const GpuModel& model, const Mat4& moving, const Mat4& fixed, Vec3 hoverLift,
                                      Vec3 pointerColour, float accentGain, bool litPointer)
    {
        using hwk::models::Role;
        for (auto& part : model.parts)
        {
            const Mat4& m = part->rotates ? moving : fixed;
            switch (part->role)
            {
                case Role::body:
                    use (shaders::plastic).set ("uParams", part->ridges, part->ridgesBelowY, 0.0f, 0.0f);
                    draw (part->mesh, m, part->colour, hoverLift);
                    break;
                case Role::accent:
                    use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                    draw (part->mesh, m, part->colour * accentGain, hoverLift * 0.6f);
                    break;
                case Role::metal:
                    use (shaders::chrome).set ("uParams", part->polish, part->brushed ? 1.0f : 0.0f, 0.0f, 0.0f);
                    draw (part->mesh, m, part->colour, hoverLift * 0.5f);
                    break;
                case Role::pointer:
                    if (! litPointer)   // an inlaid / painted line, as on real knobs
                    {
                        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                        draw (part->mesh, m, part->colour, hoverLift);
                        break;
                    }
                    use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                    draw (part->mesh, m, pointerColour * 0.55f, pointerColour * 0.45f);
                    break;
            }
        }
    }

    void HardwareRenderer::queueGlow (const Mat4& space, float x, float z, float size, Vec3 colour, float intensity) noexcept
    {
        if (intensity < 0.002f || glowCount >= (int) glows.size())   // fades all the way out (at 0.02 it popped off)
            return;
        glows[(size_t) glowCount++] = { space * Mat4::translation ({ x, 0.02f, z }) * Mat4::scale (size, 1.0f, size), colour, intensity };
    }

    int HardwareRenderer::parameterFor (int control) const noexcept
    {
        const auto i = (size_t) control;
        if (altParam[i] >= 0 && modeParam[i] >= 0 && bridge.getNormalised (modeParam[i]) > 0.5f)
            return altParam[i];
        return controlParam[i];
    }

    int HardwareRenderer::detailFor (const CameraRig& cam, const Mat4& panel, float x, float z, float radius, int viewportW) const noexcept
    {
        // Size on screen: project the centre and a point `radius` along the camera's right
        const auto centre = panel.transformPoint ({ x, 0.0f, z });
        float ax = 0, ay = 0, bx = 0, by = 0;
        if (! gfx::projectToNdc (cam.viewProj, centre, ax, ay) || ! gfx::projectToNdc (cam.viewProj, centre + cam.right * radius, bx, by))
            return 0;
        const float pixels = std::hypot (bx - ax, by - ay) * 0.5f * (float) viewportW;
        return std::min (maxDetail, hwk::models::detailForPixels (pixels));
    }

    HardwareRenderer::~HardwareRenderer()
    {
        jassert (! ready); // context must be detached before destruction
    }

    //==============================================================================
    void HardwareRenderer::newOpenGLContextCreated()
    {
        for (int m = 0; m < shaders::numMaterials; ++m)
        {
            const auto fragment = hwk::shaders::fragmentSource (shaders::materialFor (m));
            juce::String error;

            if (! programs[(size_t) m].build (hwk::shaders::vertex, fragment.toRawUTF8(), error))
            {
                juce::Logger::writeToLog ("ENH Master: shader " + juce::String (m) + " failed: " + error);
                ready = false;
                return;
            }
        }

        meshes.table.upload (geo::caseFloor());
        meshes.wall.upload (geo::backWall());
        {
            const auto baked = studiowall::bake (arcCentreZ - arcRadius - 2.2f, geo::floorHeight(), wallRackCentreY());
            wallTex.upload (baked.data(), studiowall::texW, studiowall::texH, 4, true, 4);
        }
        meshes.quad.upload (geo::unitQuad());
        meshes.enhBody.upload (geo::unitBody (faceHalfH));
        meshes.tubeBody.upload (geo::unitBody (tubeHalfH));
        meshes.tubeVents.upload (geo::unitVents (tubeHalfH));
        meshes.tubeVentWalls.upload (geo::unitVentWalls (tubeHalfH));
        meshes.tubeVentFloors.upload (geo::unitVentFloors (tubeHalfH));
        meshes.bodyScrews.upload (geo::unitBodyScrews (tubeHalfH));
        meshes.caseCheeks.upload (geo::caseCheeks());
        meshes.caseRails.upload (geo::caseRails());
        meshes.caseFrontRails.upload (geo::caseFrontRails());
        meshes.caseRailHoles.upload (geo::caseRailHoles());
        meshes.caseEdges.upload (geo::caseEdges());
        meshes.caseBoards.upload (geo::caseBoards());
        meshes.caseFeet.upload (geo::caseFeet());
        meshes.caseBrass.upload (geo::caseBrass());
        meshes.faceEdges.upload (geo::faceplateEdges());
        meshes.faceTop.upload (geo::faceplateTop());
        meshes.displayWalls.upload (geo::displayWalls());
        meshes.displayGlass.upload (geo::displayGlass());
        meshes.displayBezel.upload (geo::displayBezel());
        meshes.earWalls.upload (geo::earSlotWalls());
        meshes.earFloors.upload (geo::earSlotFloors());
        meshes.screws.upload (geo::screwHeads());
        meshes.screwSlots.upload (geo::screwSlots());
        meshes.scaleRing.upload (geo::knobScaleRing());
        meshes.arcRing.upload (hwk::geo::flatAnnulus (1.0f, 1.16f, 96));
        meshes.tubeFaceTop.upload (geo::tubeFaceTop());
        meshes.tubeFaceEdges.upload (geo::tubeFaceEdges());
        meshes.tubeEarWalls.upload (geo::tubeEarSlotWalls());
        meshes.tubeEarFloors.upload (geo::tubeEarSlotFloors());
        meshes.tubeScrews.upload (geo::tubeScrewHeads());
        meshes.tubeScrewSlots.upload (geo::tubeScrewSlots());
        meshes.seraphWalls.upload (geo::seraphDisplayWalls());
        meshes.seraphGlass.upload (geo::seraphDisplayGlass());
        meshes.seraphBezel.upload (geo::seraphDisplayBezel());

        for (int unit = 0; unit < numUnits; ++unit)
        {
            if (! isOutboard (unit))
                continue;
            auto& o = outboard[(size_t) unit];
            o.faceTop.upload (geo::oneUFaceTop (unit));
            o.faceEdges.upload (geo::oneUFaceEdges (unit));
            o.earWalls.upload (geo::oneUEarWalls (unit));
            o.earFloors.upload (geo::oneUEarFloors (unit));
            o.screws.upload (geo::oneUScrewHeads (unit));
            o.screwSlots.upload (geo::oneUScrewSlots (unit));
            o.body.upload (geo::unitBody (unitHalfH (unit)));
        }
        meshes.monitorWalls.upload (geo::windowWalls (monitorDisplayRect));
        meshes.monitorGlass.upload (geo::windowGlass (monitorDisplayRect));
        meshes.monitorBezel.upload (geo::windowBezel (monitorDisplayRect));
        meshes.balancerWalls.upload (geo::windowWalls (balancerDisplayRect));
        meshes.balancerGlass.upload (geo::windowGlass (balancerDisplayRect));
        meshes.balancerBezel.upload (geo::windowBezel (balancerDisplayRect));
        // VU movements, one model per size (the compressor's wide meter, the leveler's three narrow
        // ones, the limiter's two)
        tideVu.upload (hwk::models::vuMeter (tideVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        lumenVu.upload (hwk::models::vuMeter (lumenVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        limiterVu.upload (hwk::models::vuMeter (limiterVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        deepVu.upload (hwk::models::vuMeter (deepVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        characterVu.upload (hwk::models::vuMeter (characterVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        radarVu.upload (hwk::models::vuMeter (radarVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        levelVu.upload (hwk::models::vuMeter (levelVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));
        monitorVu.upload (hwk::models::vuMeter (monitorVuHalfW, vuHalfH, vuDepth, { 0.075f, 0.075f, 0.08f }));


        // HardwareKit models, every distinct (style, radius, unit accent) at every level of detail
        knobModels.clear();
        struct Key { int style; float radius; int unit; };
        std::vector<Key> built;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            knobModelIndex[(size_t) i].fill (-1);
            if (c.kind != ControlKind::knob && c.kind != ControlKind::selector)
                continue;

            const float r = knobBodyRadius (c);
            // Soft-touch caps (TONE & SPACE masters) in violet; the 1U units' anodised caps muted -
            // petrol, bronze and oxblood, as anodised aluminium comes
            const auto accent = c.unit == tubeUnit    ? Vec3 { 0.30f, 0.26f, 0.40f }
                              : c.unit == tideUnit    ? Vec3 { 0.10f, 0.29f, 0.35f }
                              : c.unit == lumenUnit   ? Vec3 { 0.47f, 0.33f, 0.14f }
                              : c.unit == limiterUnit ? Vec3 { 0.42f, 0.08f, 0.12f }
                              : c.unit == deepUnit    ? Vec3 { 0.07f, 0.13f, 0.26f }   // abyss blue
                              : c.unit == characterUnit ? Vec3 { 0.40f, 0.19f, 0.07f } // cognac
                              : c.unit == radarUnit   ? Vec3 { 0.06f, 0.22f, 0.20f }   // phosphor teal
                                                      : Vec3 { 0.55f, 0.56f, 0.60f };
            int first = -1;
            for (size_t k = 0; k < built.size(); ++k)
                if (built[k].style == (int) c.style && built[k].radius == r && built[k].unit == c.unit)
                    first = (int) k;
            if (first >= 0)
            {
                for (int d = 0; d < numDetail; ++d)
                    knobModelIndex[(size_t) i][(size_t) d] = first * numDetail + d;
                continue;
            }
            for (int d = 0; d < numDetail; ++d)
            {
                auto model = std::make_unique<GpuModel>();
                model->upload (hwk::models::knob (c.style, r, accent, d));
                knobModelIndex[(size_t) i][(size_t) d] = (int) knobModels.size();
                knobModels.push_back (std::move (model));
            }
            built.push_back ({ (int) c.style, r, c.unit });
        }
        for (int d = 0; d < numDetail; ++d)
        {
            for (int s = 0; s < hwk::models::numButtonStyles; ++s)
                buttonModels[(size_t) s][(size_t) d].upload (hwk::models::pushButton ((ButtonStyle) s, buttonHalfW, buttonHalfD, d));
            for (int s = 0; s < hwk::models::numSwitchStyles; ++s)
            {
                const auto model = hwk::models::toggleSwitch ((SwitchStyle) s, d);
                switchModels[(size_t) s][(size_t) d].upload (model);
                switchPivotY[(size_t) s] = model.leverPivotY;
                switchAngle[(size_t) s] = model.leverAngle;
            }
        }

        galleryKnobs.clear();
        if (gallery)
            for (int s = 0; s < hwk::models::numKnobStyles; ++s)
            {
                auto model = std::make_unique<GpuModel>();
                model->upload (hwk::models::knob ((KnobStyle) s, 0.085f, { 0.42f, 0.08f, 0.12f }, 2));
                galleryKnobs.push_back (std::move (model));
            }
        lampModel.upload (hwk::models::jewelLamp());
        meshes.led.upload (hwk::models::ledLens());

        auto upload = [this] (gfx::Texture2D& tex, const artwork::RawTexture& raw)
        {
            tex.upload (raw.pixels.data(), raw.width, raw.height, raw.channels, true, config.anisotropy);
        };
        upload (decalTex, textureData.faceplateDecal);
        upload (scaleTex, textureData.scale10);
        upload (scaleWideTex, textureData.scale30);
        upload (scale3Tex, textureData.scale3);
        upload (scale5Tex, textureData.scale5);
        upload (tubeDecalTex, textureData.tubeDecal);
        upload (seraphLabelTex, textureData.seraphLabels);
        upload (tideDecalTex, textureData.tideDecal);
        upload (lumenDecalTex, textureData.lumenDecal);
        upload (tideLabelTex, textureData.tideVuFace);
        upload (lumenLabelTex, textureData.lumenVuFace);
        upload (limiterDecalTex, textureData.limiterDecal);
        upload (deepDecalTex, textureData.deepDecal);
        upload (deepLabelTex, textureData.deepVuFace);
        upload (characterDecalTex, textureData.characterDecal);
        upload (radarDecalTex, textureData.radarDecal);
        upload (radarVuFaceTex, textureData.radarVuFace);
        upload (characterLabelTex, textureData.characterVuFace);
        upload (limiterLabelTex[0], textureData.limiterVuFace[0]);
        upload (limiterLabelTex[1], textureData.limiterVuFace[1]);
        upload (levelDecalTex, textureData.levelDecal);
        upload (balancerDecalTex, textureData.balancerDecal);
        upload (monitorDecalTex, textureData.monitorDecal);
        upload (levelFaceTex, textureData.levelVuFace);
        upload (monitorFaceTex[0], textureData.monitorVuFace[0]);
        upload (monitorFaceTex[1], textureData.monitorVuFace[1]);
        upload (monitorLabelTex, textureData.monitorLabels);
        upload (balancerLabelTex, textureData.balancerLabels);
        uploadedLevelLabelsVersion = 0;

        const std::vector<juce::uint8> blank ((size_t) (artwork::displayOverlayWidth * artwork::displayOverlayHeight), 0);
        overlayTex.upload (blank.data(), artwork::displayOverlayWidth, artwork::displayOverlayHeight, 1, true, 1);
        uploadedOverlayVersion = 0;

        if (statsEnabled)
        {
            GLint samples = 0, buffers = 0;
            glGetIntegerv (GL_SAMPLES, &samples);
            glGetIntegerv (GL_SAMPLE_BUFFERS, &buffers);
            std::fprintf (stderr, "[enh-stats] framebuffer: %d sample buffer(s), %d samples (config msaa %d)\n", buffers, samples, config.msaaSamples);
        }

        swapInterval = -1;
        lastFrameMs = 0.0;
        ready = true;
    }

    void HardwareRenderer::openGLContextClosing()
    {
        meshes.forEach ([] (gfx::GpuMesh& m) { m.release(); });
        decalTex.release();
        scaleTex.release();
        scaleWideTex.release();
        scale3Tex.release();
        scale5Tex.release();
        tubeDecalTex.release();
        for (auto& model : knobModels)
            model->release();
        knobModels.clear();
        for (auto& style : buttonModels) for (auto& b : style) b.release();
        for (auto& style : switchModels) for (auto& s : style) s.release();
        for (auto& g : galleryKnobs) g->release();
        galleryKnobs.clear();
        lampModel.release();
        tideVu.release();
        lumenVu.release();
        limiterVu.release();
        deepVu.release();
        characterVu.release();
        radarVu.release();
        levelVu.release();
        monitorVu.release();
        for (auto& o : outboard)
            o.forEach ([] (gfx::GpuMesh& m) { m.release(); });
        for (auto* tex : { &levelDecalTex, &balancerDecalTex, &monitorDecalTex, &monitorLabelTex, &balancerLabelTex, &levelFaceTex,
                           &monitorFaceTex[0], &monitorFaceTex[1],
                           &waveTex, &balancerDataTex, &tideDecalTex, &lumenDecalTex, &limiterDecalTex, &tideLabelTex, &lumenLabelTex,
                           &limiterLabelTex[0], &limiterLabelTex[1], &deepDecalTex, &deepLabelTex, &characterDecalTex, &characterLabelTex,
                           &radarDecalTex, &radarVuFaceTex, &wallTex })
            tex->release();
        loupeTarget.release();
        sceneTarget.release();
        blurA.release();
        blurB.release();
        panelTex.release();
        uploadedPanelVersion = 0;
        loupeReady = false;
        seraphLabelTex.release();
        overlayTex.release();
        calloutTex.release();
        uploadedCalloutVersion = 0;
        calloutW = calloutH = 0;

        for (auto& p : programs)
            p.release();

        current = nullptr;
        ready = false;
    }

    //==============================================================================
    /** The analyser curve as a 1 x N strip: one texel per display point. */
    void HardwareRenderer::uploadScope()
    {
        constexpr int n = enh::dsp::ScopeCurve::numPoints;
        scopeScratch.resize ((size_t) n * 4);

        auto encode = [] (float db)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (db - enh::dsp::ScopeCurve::minDb)
                                                        / (enh::dsp::ScopeCurve::maxDb - enh::dsp::ScopeCurve::minDb));
            return (juce::uint8) juce::roundToInt (t * 255.0f);
        };

        for (int i = 0; i < n; ++i)
        {
            auto* px = scopeScratch.data() + (size_t) i * 4;
            px[0] = encode (scope.inputDb[(size_t) i].load (std::memory_order_relaxed));
            px[1] = encode (scope.outputDb[(size_t) i].load (std::memory_order_relaxed));
            px[2] = encode (scope.peakDb[(size_t) i].load (std::memory_order_relaxed));
            px[3] = 255;
        }

        scopeTex.upload (scopeScratch.data(), n, 1, 4, false, 1);
    }

    //==============================================================================
    /** The two new displays' data, rebuilt once a frame from the histories the editor fills. */
    void HardwareRenderer::uploadDisplays (float dt)
    {
        // --- MONITOR: row 0 = the waveform, in and out (newest at the right) with the output's afterimage;
        //     row 1 = the spectrum in and out (the analyser's curve), both 480 across
        {
            constexpr int n = DisplayHistory::waveColumns;
            waveScratch.assign ((size_t) n * 2 * 4, 0);
            const int head = history.waveHead.load (std::memory_order_acquire);
            const float fade = std::exp (-dt / 1.4f);   // the afterimage fades over a second or two

            for (int x = 0; x < n; ++x)
            {
                float out = 0.0f, in = 0.0f;
                if (demoMeters)
                {
                    // A plausible programme for screenshots: a bed with hits every 0.7 s, the hits limited
                    const double t = timeSeconds - (double) (n - 1 - x) * 0.010;
                    const double hit = std::fmod (t, 0.7);
                    const float noise = 0.5f + 0.5f * (float) std::sin (t * 91.0 + std::sin (t * 13.0) * 3.0);
                    in = 0.20f + 0.07f * noise + (hit < 0.25 ? 0.75f * (float) std::exp (-hit * 12.0) : 0.0f);
                    out = std::min (0.30f + 0.08f * noise + (hit < 0.25 ? 0.62f * (float) std::exp (-hit * 12.0) : 0.0f), 0.86f);
                }
                else
                {
                    const int index = head - n + x;
                    if (index >= 0)
                    {
                        out = std::clamp (history.wavePeak[(size_t) (index % n)].load (std::memory_order_relaxed), 0.0f, 1.0f);
                        in = std::clamp (history.waveInPeak[(size_t) (index % n)].load (std::memory_order_relaxed), 0.0f, 1.0f);
                    }
                }
                auto& g = waveGhost[(size_t) x];
                g = std::max (g * fade, out);
                auto* px = waveScratch.data() + (size_t) x * 4;
                px[0] = (juce::uint8) juce::roundToInt (out * 255.0f);
                px[1] = (juce::uint8) juce::roundToInt (g * 255.0f);
                px[2] = (juce::uint8) juce::roundToInt (in * 255.0f);
                px[3] = 255;
            }

            constexpr int points = enh::dsp::ScopeCurve::numPoints;
            auto encode = [] (float db)
            {
                return (juce::uint8) juce::roundToInt (255.0f * std::clamp ((db - enh::dsp::ScopeCurve::minDb)
                                                                           / (enh::dsp::ScopeCurve::maxDb - enh::dsp::ScopeCurve::minDb), 0.0f, 1.0f));
            };
            for (int x = 0; x < n; ++x)
            {
                const float pos = (float) x / (float) (n - 1) * (float) (points - 1);
                const int i0 = std::min (points - 2, (int) pos);
                const float f = pos - (float) i0;
                auto at = [&] (const std::array<std::atomic<float>, points>& a)
                {
                    return a[(size_t) i0].load (std::memory_order_relaxed) * (1.0f - f) + a[(size_t) i0 + 1].load (std::memory_order_relaxed) * f;
                };
                auto* px = waveScratch.data() + ((size_t) n + (size_t) x) * 4;
                px[0] = encode (at (scope.inputDb));
                px[1] = encode (at (scope.outputDb));
                px[3] = 255;
            }
            waveTex.upload (waveScratch.data(), n, 2, 4, false, 1);
        }

        // --- MIX BALANCER: spectrum row and history row, and the six faders ------------------------------
        {
            constexpr int w = DisplayHistory::balColumns;
            balancerScratch.assign ((size_t) w * 4 * 4, 0);
            constexpr int points = enh::dsp::ScopeCurve::numPoints;
            auto encodeSpectrum = [] (float db)
            {
                return (juce::uint8) juce::roundToInt (255.0f * std::clamp ((db - enh::dsp::ScopeCurve::minDb)
                                                                           / (enh::dsp::ScopeCurve::maxDb - enh::dsp::ScopeCurve::minDb), 0.0f, 1.0f));
            };
            auto row = [&] (int r, int x) { return balancerScratch.data() + ((size_t) r * w + (size_t) x) * 4; };
            for (int x = 0; x < w; ++x)
            {
                const float pos = (float) x / (float) (w - 1) * (float) (points - 1);
                const int i0 = std::min (points - 2, (int) pos);
                const float f = pos - (float) i0;
                auto at = [&] (const std::array<std::atomic<float>, points>& a)
                {
                    return a[(size_t) i0].load (std::memory_order_relaxed) * (1.0f - f) + a[(size_t) i0 + 1].load (std::memory_order_relaxed) * f;
                };
                auto* px = row (1, x);
                px[0] = encodeSpectrum (at (balancerScope.inputDb));
                px[1] = encodeSpectrum (at (balancerScope.outputDb));
                px[3] = 255;
            }

            // Grid row: the columns a decade (100 Hz, 1 kHz, 10 kHz) or an octave line runs down, anti-aliased over a column
            for (int x = 0; x < w; ++x)
            {
                auto* px = row (0, x);
                px[3] = 255;
                auto mark = [&] (float hz, int channel)
                {
                    const float c = std::log (hz / 20.0f) / std::log (1000.0f) * (float) (w - 1);
                    const float d = std::abs ((float) x - c);
                    if (d < 1.0f)
                        px[channel] = (juce::uint8) std::max ((int) px[channel], juce::roundToInt ((1.0f - d) * 255.0f));
                };
                for (float hz : { 100.0f, 1000.0f, 10000.0f }) mark (hz, 0);
                for (float hz : { 50.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f }) mark (hz, 1);
            }

            const int head = history.balHead.load (std::memory_order_acquire);
            for (int x = 0; x < w; ++x)
            {
                float in = 0.0f, out = 0.0f, cut = 0.0f;
                if (demoMeters)
                {
                    const double t = timeSeconds - (double) (w - 1 - x) * 0.033;
                    in = 0.62f + 0.12f * (float) std::sin (t * 2.3) + 0.08f * (float) std::sin (t * 7.1);
                    const float jump = std::fmod ((float) t, 3.0f) < 0.6f ? 0.18f : 0.0f;
                    in += jump;
                    out = in - jump * 0.7f;
                    cut = jump * 0.9f;
                }
                else
                {
                    const int index = head - w + x;
                    if (index >= 0)
                    {
                        const auto k = (size_t) (index % w);
                        in = std::clamp ((history.balInDb[k].load (std::memory_order_relaxed) + 60.0f) / 60.0f, 0.0f, 1.0f);
                        out = std::clamp ((history.balOutDb[k].load (std::memory_order_relaxed) + 60.0f) / 60.0f, 0.0f, 1.0f);
                        cut = std::clamp (-history.balCutDb[k].load (std::memory_order_relaxed) / 12.0f, 0.0f, 1.0f);
                    }
                }
                auto* px = row (2, x);
                px[0] = (juce::uint8) juce::roundToInt (in * 255.0f);
                px[1] = (juce::uint8) juce::roundToInt (out * 255.0f);
                px[2] = (juce::uint8) juce::roundToInt (cut * 255.0f);
                px[3] = 255;
            }
            const float k = 1.0f - std::exp (-dt / 0.06f);
            for (int b = 0; b < 6; ++b)
            {
                const float target = demoMeters ? 3.5f * (float) std::sin (timeSeconds * (0.7 + 0.23 * b) + b)
                                                : meters.balanceGainDb[(size_t) b].load (std::memory_order_relaxed);
                balancerBands[(size_t) b] += (target - balancerBands[(size_t) b]) * k;
            }
            for (int f = 0; f < enh::dsp::MixBalancer::numFine; ++f)
            {
                const float target = demoMeters ? 0.0f : meters.balanceFineGainDb[(size_t) f].load (std::memory_order_relaxed);
                balancerFine[(size_t) f] += (target - balancerFine[(size_t) f]) * k;
            }
            balancerCoarse += ((demoMeters ? 1.0f : 1.0f - meters.balanceResolution.load (std::memory_order_relaxed)) - balancerCoarse) * k;

            // The faders as one curve (bells in the middle, shelves at the ends) and each column's band colour
            static constexpr std::array<float, 6> centre { 70.0f, 200.0f, 500.0f, 1300.0f, 3500.0f, 9000.0f };
            static constexpr std::array<std::array<float, 3>, 6> colour {{ { 0.95f, 0.45f, 0.30f }, { 0.95f, 0.70f, 0.25f }, { 0.55f, 0.85f, 0.35f },
                                                                           { 0.30f, 0.80f, 0.85f }, { 0.45f, 0.55f, 1.00f }, { 0.80f, 0.45f, 0.95f } }};
            auto smooth = [] (float a, float b, float x) { const float t = std::clamp ((x - a) / (b - a), 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); };
            for (int x = 0; x < w; ++x)
            {
                const float hz = 20.0f * std::pow (1000.0f, (float) x / (float) (w - 1));
                float gainDb = 0.0f, weight = 0.0f;
                std::array<float, 3> mixed {};
                for (int b = 0; b < 6; ++b)
                {
                    const float d = std::log2 (hz / centre[(size_t) b]);
                    const float shape = b == 0 ? 1.0f - smooth (-0.4f, 0.9f, d) : b == 5 ? smooth (-0.9f, 0.4f, d) : std::exp (-0.5f * d * d / 0.42f);
                    gainDb += balancerBands[(size_t) b] * shape;
                    for (int c = 0; c < 3; ++c) mixed[(size_t) c] += colour[(size_t) b][(size_t) c] * shape;
                    weight += shape;
                }
                // Spectral mode's third-octave faders (narrow bells)
                for (int k = 0; k < enh::dsp::MixBalancer::numFine; ++k)
                {
                    const float d = std::log2 (hz / enh::dsp::MixBalancer::fineHz (k));
                    gainDb += balancerFine[(size_t) k] * std::exp (-0.5f * d * d / 0.022f);
                }
                auto* px = row (3, x);
                px[0] = (juce::uint8) juce::roundToInt (255.0f * std::clamp (0.5f + gainDb / 24.0f, 0.0f, 1.0f));
                for (int c = 0; c < 3; ++c)
                    px[1 + c] = (juce::uint8) juce::roundToInt (255.0f * std::clamp (mixed[(size_t) c] / std::max (weight, 1.0e-3f), 0.0f, 1.0f));
            }
            balancerDataTex.upload (balancerScratch.data(), w, 4, 4, false, 1);
        }
    }

    void HardwareRenderer::uploadLevelLabelsIfChanged()
    {
        std::vector<juce::uint8> pixels;
        int w = 0, h = 0;
        {
            const juce::SpinLock::ScopedLockType lock (shared.levelLabelsLock);
            if (shared.levelLabelsVersion == uploadedLevelLabelsVersion || shared.levelLabelsPending.pixels.empty())
                return;
            std::swap (pixels, shared.levelLabelsPending.pixels);
            w = shared.levelLabelsPending.width;
            h = shared.levelLabelsPending.height;
            uploadedLevelLabelsVersion = shared.levelLabelsVersion;
        }
        if (w > 0 && h > 0 && pixels.size() == (size_t) (w * h))
            monitorLabelTex.upload (pixels.data(), w, h, 1, true, config.anisotropy);
    }

    /** The LEVEL & LOUDNESS waveform screen and the MIX BALANCER display, each in its window. */
    void HardwareRenderer::drawWindows (const Mat4& monitorPanel, const Mat4& balancerPanel)
    {
        const float monitorLamp = unitLamp[(size_t) monitorUnit], balancerPower = unitLamp[(size_t) balancerUnit];

        waveTex.bind (0);
        monitorLabelTex.bind (1);
        auto& wave = use (shaders::waveScreen);
        wave.set ("uTex2", 1);
        const int toneRange = bridge.indexOf ("displayToneRange");
        const int toneChoice = toneRange >= 0 ? juce::roundToInt (bridge.getNormalised (toneRange) * 2.0f) : 0;
        wave.set ("uParams", monitorLamp, toneChoice == 1 ? 6.0f : toneChoice == 2 ? 24.0f : 12.0f, 0.0f, 0.0f);
        draw (meshes.monitorGlass, monitorPanel, {});

        balancerDataTex.bind (0);
        balancerLabelTex.bind (1);
        auto& bal = use (shaders::balancerDisplay);
        bal.set ("uTex2", 1);
        bal.set ("uParams", 0.35f + 0.65f * balancerPower, balancerCoarse, 0.0f, 0.0f);   // .y: the six handles fade as RESOLUTION goes spectral
        bal.setArray ("uBands", balancerBands.data(), (int) balancerBands.size());
        static const std::array<float, 6> handleU = []
        {
            std::array<float, 6> u {};
            const float centre[6] { 70.0f, 200.0f, 500.0f, 1300.0f, 3500.0f, 9000.0f };
            for (size_t k = 0; k < 6; ++k)
                u[k] = 0.02f + 0.96f * std::log (centre[k] / 20.0f) / std::log (1000.0f);
            return u;
        }();
        bal.setArray ("uHandleU", handleU.data(), 6);
        draw (meshes.balancerGlass, balancerPanel, {});

        auto& walls = use (shaders::recess);
        walls.set ("uParams", windowDepth, 0.0f, 0.0f, 0.0f);
        walls.set ("uGlow", Vec3 { 0.05f, 0.045f, 0.03f } * monitorLamp);
        draw (meshes.monitorWalls, monitorPanel, { 0.08f, 0.075f, 0.07f });
        walls.set ("uGlow", Vec3 { 0.05f, 0.045f, 0.03f } * balancerPower);
        draw (meshes.balancerWalls, balancerPanel, { 0.05f, 0.055f, 0.06f });

        use (shaders::chrome).set ("uParams", 0.45f, 0.0f, 0.0f, 0.0f);
        draw (meshes.monitorBezel, monitorPanel, { 0.09f, 0.09f, 0.10f });
        draw (meshes.balancerBezel, balancerPanel, { 0.09f, 0.09f, 0.10f });
    }

    void HardwareRenderer::uploadOverlayIfChanged()
    {
        juce::uint32 version = 0;

        {
            const juce::SpinLock::ScopedLockType lock (shared.overlayLock);

            if (shared.overlayVersion == uploadedOverlayVersion || shared.overlayPending.pixels.empty())
                return;

            std::swap (overlayScratch, shared.overlayPending.pixels);
            version = shared.overlayVersion;
        }

        if (overlayScratch.size() == (size_t) (artwork::displayOverlayWidth * artwork::displayOverlayHeight))
            overlayTex.upload (overlayScratch.data(), artwork::displayOverlayWidth, artwork::displayOverlayHeight, 1, true, 1);

        uploadedOverlayVersion = version;
    }

    void HardwareRenderer::uploadCalloutIfChanged()
    {
        juce::uint32 version = 0;
        int w = 0, h = 0;

        {
            const juce::SpinLock::ScopedLockType lock (shared.calloutLock);
            if (shared.calloutVersion == uploadedCalloutVersion || shared.calloutPending.pixels.empty())
                return;

            std::swap (calloutScratch, shared.calloutPending.pixels);
            w = shared.calloutPending.width;
            h = shared.calloutPending.height;
            version = shared.calloutVersion;
        }

        if (w > 0 && h > 0 && calloutScratch.size() == (size_t) (w * h * 4))
        {
            calloutTex.upload (calloutScratch.data(), w, h, 4, false, 1);
            calloutW = w;
            calloutH = h;
        }

        uploadedCalloutVersion = version;
    }

    void HardwareRenderer::renderLoupeView (const CameraRig& camera, int vw, int vh)
    {
        loupeReady = false;
        float ax = 0.0f, ay = 0.0f;

        // Normally the glass magnifies about the cursor itself: what sits under the pointer stays
        // under the pointer, only bigger, so moving on to the next label walks straight through the
        // lens. While a control is being adjusted (or for the screenshot hook, which has no real
        // pointer) it locks onto the control instead, so the value stays in view as the mouse moves.
        const bool atPointer = shared.calloutAtPointer.load() && ! shared.dragging.load()
                                 && (pointerInside || shared.mouseInside.load());
        if (atPointer)
        {
            ax = pointerNdcX;
            ay = pointerNdcY;
        }
        else
        {
            const auto world = panelToWorld (loupeUnit).transformPoint ({ loupeX, 0.004f, loupeZ });
            if (! gfx::projectToNdc (camera.viewProj, world, ax, ay))
                return;
        }

        const float px = (float) vw / (float) juce::jmax (1, shared.viewWidth.load());
        const float maxRadius = 77.5f * px;   // 1.25x the original 62 px lens
        loupeRadius = maxRadius * (0.55f + 0.45f * loupeAlpha);   // grows out of the panel, shrinks back into it
        const float margin = loupeRadius + 6.0f * px;
        loupeCx = juce::jlimit (margin, std::max (margin, (float) vw - margin), (ax + 1.0f) * 0.5f * (float) vw);
        loupeCy = juce::jlimit (margin, std::max (margin, (float) vh - margin), (ay + 1.0f) * 0.5f * (float) vh);

        // Fixed target size (only the window size changes it), so opening the loupe never reallocates
        const int size = juce::jmax (8, juce::roundToInt (2.0f * maxRadius));
        // Multisampled like the main view, so the magnified scene is anti-aliased too
        if (! loupeTarget.ensureSize (size, size, config.msaaSamples))
            return;

        // Re-render the scene zoomed in around the anchor: real detail, not stretched pixels
        auto zoomed = camera;
        zoomed.viewProj = hwk::fx::Loupe::zoomedViewProj (camera.viewProj, ax, ay, loupeZoom, maxRadius, vw, vh);

        loupeTarget.bind();
        vignette = 0.0f;
        drawScene (zoomed, size, size);
        vignette = 1.0f;
        loupeTarget.resolve();
        gfx::RenderTarget::unbind();
        glViewport (0, 0, vw, vh);
        loupeReady = true;
    }

    /** How much finer than the screen the scene is drawn: config renderScale (1..2), or 1 on auto (0).
        Measured on the UHD 600 this was built on: 4x MSAA at 1x holds ~57 fps on the whole rack, while
        1.25x drops it to ~44 and 1.5x to ~31 - so supersampling is there for GPUs with room to spare. */
    float HardwareRenderer::renderScaleFor (int) const noexcept
    {
        return config.renderScale > 0.0f ? juce::jlimit (1.0f, 2.0f, config.renderScale) : 1.0f;
    }

    void HardwareRenderer::presentScene (int screenW, int screenH)
    {
        glDisable (GL_DEPTH_TEST);
        glDisable (GL_BLEND);
        glDepthMask (GL_FALSE);
        sceneTarget.bindColour (0);
        auto& p = use (shaders::present);
        p.set ("uViewProj", Mat4::identity());
        // Taps a quarter of a screen pixel either side: with 1.5x, they cover the source pixels a screen pixel spans
        const float sx = (float) sceneTarget.getWidth() / (float) std::max (1, screenW), sy = (float) sceneTarget.getHeight() / (float) std::max (1, screenH);
        p.set ("uParams", sx > 1.01f ? 0.25f / (float) screenW : 0.0f, sy > 1.01f ? 0.25f / (float) screenH : 0.0f, 0.0f, 0.0f);
        p.set ("uParams2", 1.0f / (float) std::max (1, screenW), 1.0f / (float) std::max (1, screenH), 0.0f, 0.0f);
        draw (meshes.quad, gfx::screenQuad (screenW, screenH, 0.5f * (float) screenW, 0.5f * (float) screenH,
                                            0.5f * (float) screenW, 0.0f, 0.0f, 0.5f * (float) screenH), {});
        glDepthMask (GL_TRUE);
        glEnable (GL_DEPTH_TEST);
    }

    void HardwareRenderer::drawLoupe (int vw, int vh)
    {
        if (! loupeReady || loupeAlpha < 0.01f)
            return;

        const float px = (float) vw / (float) juce::jmax (1, shared.viewWidth.load());
        const float R = loupeRadius;

        glDisable (GL_DEPTH_TEST);
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask (GL_FALSE);

        // Soft drop shadow and a dark outer rim, like a glass loupe lifted off the panel
        auto& overlay = use (shaders::callout);
        overlay.set ("uViewProj", Mat4::identity());
        overlay.set ("uParams", 0.0f, 0.34f * loupeAlpha, 1.0f, 1.0f);
        draw (meshes.quad, gfx::screenQuad (vw, vh, loupeCx + 3.0f * px, loupeCy - 7.0f * px, R * 1.18f, 0.0f, 0.0f, R * 1.18f), {});
        overlay.set ("uParams", 0.0f, 0.80f * loupeAlpha, 1.0f, 1.0f);
        draw (meshes.quad, gfx::screenQuad (vw, vh, loupeCx, loupeCy, R * 1.05f, 0.0f, 0.0f, R * 1.05f), { 0.05f, 0.05f, 0.06f });

        loupeTarget.bindColour (0);
        auto& lens = use (shaders::lens);
        lens.set ("uViewProj", Mat4::identity());
        lens.set ("uParams", 0.38f, 0.012f, 0.94f, loupeAlpha);   // .z = glass opacity: just barely see-through
        lens.set ("uParams2", loupeCx, loupeCy, R, 0.0f);
        draw (meshes.quad, gfx::screenQuad (vw, vh, loupeCx, loupeCy, R, 0.0f, 0.0f, R), {});

        // Controls: a small name + value pill under the lens
        if (shared.calloutHasPill.load() && calloutW > 0 && calloutTex.isValid())
        {
            const float boxScale = px / std::max (1.0f, shared.calloutPixelScale.load()) * 0.72f;
            const float bw = (float) calloutW * boxScale, bh = (float) calloutH * boxScale;
            float by = loupeCy - R - 10.0f * px - 0.5f * bh;
            if (by - 0.5f * bh < 4.0f * px)
                by = loupeCy + R + 10.0f * px + 0.5f * bh;
            const float bx = juce::jlimit (0.5f * bw + 4.0f * px, std::max (0.5f * bw + 4.0f * px, (float) vw - 0.5f * bw - 4.0f * px), loupeCx);

            calloutTex.bind (0);
            auto& pill = use (shaders::callout);
            pill.set ("uViewProj", Mat4::identity());
            pill.set ("uParams", 1.0f, loupeAlpha, 0.0f, 0.0f);
            draw (meshes.quad, gfx::screenQuad (vw, vh, bx, by, 0.5f * bw, 0.0f, 0.0f, -0.5f * bh), {});
        }

        glDisable (GL_BLEND);
        glDepthMask (GL_TRUE);
        glEnable (GL_DEPTH_TEST);
    }

    //==============================================================================
    // Glass panel
    void HardwareRenderer::updatePanel (float dt)
    {
        // Dev only: PAD_UI_TEST_SLOWMO=<factor> slows the panel's animation down, to look at it frame by frame
        static const float slowmo = juce::jmax (1.0f, juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SLOWMO", "1").getFloatValue());
        dt /= slowmo;
        // The line draws out from the unit first, then the glass opens at its end. Switching units redraws
        // the line from the new one while the glass stays; closing folds both away.
        const int want = shared.panelUnit.load();
        if (want >= 0)
        {
            if (want != panelShown)
            {
                panelShown = want;
                panelLine = 0.0f;
            }
            panelLine = anim::approach (panelLine, 1.0f, 16.0f, dt);
            if (panelLine > 0.55f)
                panelOpen = anim::approach (panelOpen, 1.0f, 14.0f, dt);
        }
        else
        {
            panelOpen = anim::approach (panelOpen, 0.0f, 18.0f, dt);
            panelLine = anim::approach (panelLine, 0.0f, 22.0f, dt);
            if (panelOpen < 0.005f && panelLine < 0.005f)
            {
                panelOpen = panelLine = 0.0f;
                panelShown = -1;
            }
        }
        if (panelShown >= 0)
        {
            if (std::abs (panelOpen - (want >= 0 ? 1.0f : 0.0f)) > 0.002f || panelLine < 0.998f)
                busyUntilMs = juce::Time::getMillisecondCounterHiRes() + 300.0;   // animate at the full frame rate
        }
    }

    void HardwareRenderer::uploadPanelIfChanged()
    {
        juce::uint32 version = 0;
        int w = 0, h = 0;
        juce::Rectangle<float> rect;
        {
            const juce::SpinLock::ScopedLockType lock (shared.panelLock);
            if (shared.panelVersion == uploadedPanelVersion || shared.panelPending.pixels.empty())
                return;
            std::swap (panelScratch, shared.panelPending.pixels);
            w = shared.panelPending.width;
            h = shared.panelPending.height;
            rect = shared.panelPendingRect;
            version = shared.panelVersion;
        }
        panelRect = rect;
        if (w > 0 && h > 0 && panelScratch.size() == (size_t) (w * h * 4))
        {
            panelTex.upload (panelScratch.data(), w, h, 4, false, 1);
            panelTexW = w;
            panelTexH = h;
        }
        uploadedPanelVersion = version;
    }

    /** The scene behind the panel, blurred: down to a quarter of the resolution, then two Gaussian passes
        each way. Only the panel's rectangle (and its glow margin) is touched. Needs the scene in our own
        buffer; returns false when it is not (the glass is then a plain dark frost). */
    bool HardwareRenderer::blurBehindPanel (int w, int h)
    {
        if (! sceneTarget.isValid())
            return false;
        const int sw = sceneTarget.getWidth(), sh = sceneTarget.getHeight();
        const int qw = juce::jmax (1, sw / 4), qh = juce::jmax (1, sh / 4);
        if (! blurA.ensureSize (qw, qh, 0) || ! blurB.ensureSize (qw, qh, 0))
            return false;

        // The panel in quarter-resolution pixels (y up), with a margin for the blur's reach
        const float lw = (float) juce::jmax (1, shared.viewWidth.load()), lh = (float) juce::jmax (1, shared.viewHeight.load());
        const float sx = (float) qw / lw, sy = (float) qh / lh;
        const int margin = 12;
        const int x0 = juce::jlimit (0, qw, (int) std::floor (panelRect.getX() * sx) - margin);
        const int x1 = juce::jlimit (0, qw, (int) std::ceil (panelRect.getRight() * sx) + margin);
        const int y0 = juce::jlimit (0, qh, (int) std::floor ((lh - panelRect.getBottom()) * sy) - margin);
        const int y1 = juce::jlimit (0, qh, (int) std::ceil ((lh - panelRect.getY()) * sy) + margin);
        if (x1 <= x0 || y1 <= y0)
            return false;

        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glDisable (GL_BLEND);
        glEnable (GL_SCISSOR_TEST);
        auto& p = use (shaders::blurPass);
        p.set ("uViewProj", Mat4::identity());
        p.set ("uParams2", 1.0f / (float) qw, 1.0f / (float) qh, 0.0f, 0.0f);
        const auto full = gfx::screenQuad (qw, qh, 0.5f * (float) qw, 0.5f * (float) qh, 0.5f * (float) qw, 0.0f, 0.0f, 0.5f * (float) qh);

        auto pass = [&] (gfx::RenderTarget& into, const gfx::RenderTarget& from, float dx, float dy, float box)
        {
            into.bind();
            glScissor (x0, y0, x1 - x0, y1 - y0);
            from.bindColour (0);
            p.set ("uParams", dx, dy, box, 0.0f);
            draw (meshes.quad, full, {});
        };
        pass (blurA, sceneTarget, 1.0f / (float) sw, 1.0f / (float) sh, 1.0f);   // down to a quarter
        pass (blurB, blurA, 1.0f / (float) qw, 0.0f, 0.0f);                          // Gaussian across
        pass (blurA, blurB, 0.0f, 1.0f / (float) qh, 0.0f);                          // ... and down
        pass (blurB, blurA, 2.0f / (float) qw, 0.0f, 0.0f);                          // again, twice as wide
        pass (blurA, blurB, 0.0f, 2.0f / (float) qh, 0.0f);
        pass (blurB, blurA, 4.0f / (float) qw, 0.0f, 0.0f);                          // and four times: frosted
        pass (blurA, blurB, 0.0f, 4.0f / (float) qh, 0.0f);

        glDisable (GL_SCISSOR_TEST);
        gfx::RenderTarget::unbind();
        glViewport (0, 0, w, h);
        return true;
    }

    /** The line from the unit to the glass: a node on the unit's right edge, out level with it, then an
        elbow up or down to the panel's header. Drawn out along its length as it opens. */
    void HardwareRenderer::drawPanelConnector (const CameraRig& camera, int w, int h)
    {
        if (panelShown < 0 || panelLine < 0.01f)
            return;
        float ax = 0.0f, ay = 0.0f;
        const auto world = panelToWorld (panelShown).transformPoint ({ faceHalfW, 0.01f, 0.0f });
        if (! gfx::projectToNdc (camera.viewProj, world, ax, ay))
            return;
        const float lh = (float) juce::jmax (1, shared.viewHeight.load());
        const float px = (float) w / (float) juce::jmax (1, shared.viewWidth.load());
        const juce::Point<float> a ((ax + 1.0f) * 0.5f * (float) w, (ay + 1.0f) * 0.5f * (float) h);
        if (panelRect.isEmpty())
            return;
        const float panelLeft = panelRect.getX() * px;
        const float headerY = (lh - panelRect.getY() - glass::headerH * 0.5f) * px;
        const juce::Point<float> end (panelLeft, headerY);
        // Out level with the unit, then the elbow: a 45-degree run to the header's height
        const float rise = end.y - a.y;
        const float kneeX = std::max (a.x + 12.0f * px, end.x - std::abs (rise) - 18.0f * px);
        const juce::Point<float> knee (kneeX, a.y), knee2 (kneeX + std::abs (rise), end.y);
        const std::array<juce::Point<float>, 4> pts { a, knee, knee2, end };
        float total = 0.0f;
        for (int i = 0; i < 3; ++i)
            total += pts[(size_t) i].getDistanceFrom (pts[(size_t) i + 1]);
        if (total < 1.0f)
            return;

        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        auto& o = use (shaders::callout);
        o.set ("uViewProj", Mat4::identity());

        auto segment = [&] (juce::Point<float> p0, juce::Point<float> p1, float thickness, float alpha)
        {
            const auto d = p1 - p0;
            const float len = d.getDistanceFromOrigin();
            if (len < 0.5f)
                return;
            const auto c = (p0 + p1) * 0.5f;
            const float ux = d.x / len, uy = d.y / len;
            o.set ("uParams", 0.0f, alpha, 0.0f, 0.0f);
            draw (meshes.quad, gfx::screenQuad (w, h, c.x, c.y, 0.5f * len * ux, 0.5f * len * uy, -0.5f * thickness * uy, 0.5f * thickness * ux), panelColour);
        };

        const float fade = std::max (panelLine * 0.3f, panelOpen) * (shared.panelUnit.load() >= 0 ? 1.0f : panelLine);
        float left = total * panelLine;
        for (int i = 0; i < 3 && left > 0.0f; ++i)
        {
            auto p0 = pts[(size_t) i], p1 = pts[(size_t) i + 1];
            const float len = p0.getDistanceFrom (p1);
            if (len > left)
                p1 = p0 + (p1 - p0) * (left / len);
            left -= len;
            segment (p0, p1, 1.0f * px, 0.75f * fade);   // a hairline of white
        }
        // The node on the unit: a small square with a hairline ring
        const float s = 2.5f * px;
        o.set ("uParams", 0.0f, 0.9f * fade, 0.0f, 0.0f);
        draw (meshes.quad, gfx::screenQuad (w, h, a.x, a.y, s, 0.0f, 0.0f, s), panelColour);

        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable (GL_BLEND);
        glDepthMask (GL_TRUE);
        glEnable (GL_DEPTH_TEST);
    }

    void HardwareRenderer::drawGlassPanel (int w, int h, bool blurred)
    {
        if (panelShown < 0 || panelOpen < 0.01f || panelRect.isEmpty())
            return;
        const float lh = (float) juce::jmax (1, shared.viewHeight.load());
        const float px = (float) w / (float) juce::jmax (1, shared.viewWidth.load());
        const float hw = 0.5f * panelRect.getWidth() * px, hh = 0.5f * panelRect.getHeight() * px;
        const float slide = (1.0f - panelOpen) * 22.0f * px;   // slides in from the right as it opens
        const float cx = panelRect.getX() * px + hw + slide;
        const float cy = (lh - panelRect.getY()) * px - hh;
        const float margin = 40.0f * px;   // room for the shadow

        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (blurred)
            blurA.bindColour (0);
        if (panelTex.isValid())
            panelTex.bind (1);
        auto& g = use (shaders::glassPanel);
        g.set ("uViewProj", Mat4::identity());
        g.set ("uTex2", 1);
        g.set ("uParams", hw, hh, px, panelOpen);
        g.set ("uParams2", hw + margin, hh + margin, blurred ? 1.0f : 0.0f, 0.0f);
        draw (meshes.quad, gfx::screenQuad (w, h, cx, cy, hw + margin, 0.0f, 0.0f, -(hh + margin)), panelColour,
              { 1.0f / (float) w, 1.0f / (float) h, 0.0f });
        glActiveTexture (GL_TEXTURE0);

        glDisable (GL_BLEND);
        glDepthMask (GL_TRUE);
        glEnable (GL_DEPTH_TEST);
    }

    /** Hover outlines, cheap: the control under the pointer gets an inverted hull (its own mesh, slightly
        bigger, front faces culled); otherwise the unit under the pointer gets one silhouette quad over its
        faceplate. The open panel's unit keeps a steady outline in its own colour. */
    void HardwareRenderer::drawOutlines (const CameraRig& cam, int)
    {
        (void) cam;
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask (GL_FALSE);

        if (hullModel != nullptr)
        {
            glEnable (GL_CULL_FACE);
            glCullFace (GL_FRONT);
            auto& hull = use (shaders::outlineHull);
            hull.set ("uParams", 0.0f, 0.0f, 0.0f, 0.8f);
            const auto grow = Mat4::translation ({ 0.0f, -0.003f, 0.0f }) * Mat4::scale (1.06f, 1.03f, 1.06f);
            for (auto& part : hullModel->parts)
                draw (part->mesh, (part->rotates ? hullMoving : hullFixed) * grow, { 1.0f, 1.0f, 1.0f });
            glCullFace (GL_BACK);
            glDisable (GL_CULL_FACE);
        }

        auto frame = [&] (int unit, Vec3 colour, float opacity)
        {
            auto& f = use (shaders::outlineFrame);
            const float hw = faceHalfW + 0.012f, hh = unitHalfH (unit) + 0.012f;
            f.set ("uParams", hw, hh, 1.2f, opacity);
            draw (meshes.quad, panelToWorld (unit) * Mat4::translation ({ 0.0f, 0.006f, 0.0f }) * Mat4::scale (hw, 1.0f, hh), colour);
        };
        const int open = shared.panelUnit.load();
        if (open >= 0)
            frame (open, { 1.0f, 1.0f, 1.0f }, 0.55f);
        const int hovered = shared.hoveredUnit.load();
        if (hovered >= 0 && hovered != open && hullModel == nullptr)
            frame (hovered, { 1.0f, 1.0f, 1.0f }, 0.35f);

        glDepthMask (GL_TRUE);
        glDisable (GL_BLEND);
    }

    void HardwareRenderer::pollPointer() noexcept
    {
        const int w = juce::jmax (1, shared.viewWidth.load()), h = juce::jmax (1, shared.viewHeight.load());
        int wx = 0, wy = 0;

        bool left = false, fine = false;

        if (pointer.query ((unsigned long) shared.nativeWindow.load(), wx, wy, left, fine))
        {
            // Window pixels -> this view's logical coordinates
            const float scale = std::max (0.25f, shared.platformScale.load());
            pointerX = (float) wx / scale - (float) shared.viewOffsetX.load();
            pointerY = (float) wy / scale - (float) shared.viewOffsetY.load();

            pointerInside = pointerX >= 0.0f && pointerY >= 0.0f && pointerX < (float) w && pointerY < (float) h;
            pointerNdcX = juce::jlimit (-1.0f, 1.0f, 2.0f * pointerX / (float) w - 1.0f);
            pointerNdcY = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * pointerY / (float) h);
            leftDown = left;
            fineDrag = fine;
            pointerPolled = true;
        }
        else
        {
            pointerPolled = false;
            leftDown = false;
            // Fall back to the (host-paced) mouse events
            pointerInside = shared.mouseInside.load();
            pointerNdcX = shared.mouseNdcX.load();
            pointerNdcY = shared.mouseNdcY.load();
        }

        shared.pointerInside = pointerInside;
        shared.pointerNdcX = pointerNdcX;
        shared.pointerNdcY = pointerNdcY;
        shared.renderInteraction = pointerPolled;
    }

    void HardwareRenderer::handleInteraction() noexcept
    {
        // Clicks and drags straight from the polled pointer: the parameter changes on the
        // very frame the button moves, instead of when the host delivers the mouse event.
        // (Host gesture begin/end are still sent from the message-thread mouse events.)
        if (! pointerPolled)
        {
            lastLeftDown = false;
            return;
        }

        const bool pressed = leftDown && ! lastLeftDown;
        const bool released = ! leftDown && lastLeftDown;
        lastLeftDown = leftDown;

        const int w = juce::jmax (1, shared.viewWidth.load()), h = juce::jmax (1, shared.viewHeight.load());

        // Presses on the glass panel are the panel's (the message thread handles them): never a knob behind it
        const bool overPanel = shared.pointInPanel (pointerX, pointerY);

        // A press only counts when our window is really the one under the pointer
        if (pressed && pointerInside && ! overPanel && pointer.isTopmostUnderPointer ((unsigned long) shared.nativeWindow.load()))
        {
            const auto cam = CameraRig::build ((float) w / (float) h, parallaxX, parallaxY, { shared.focusUnit.load(), focusAmount });
            const int hit = pickControl (cam, pointerNdcX, pointerNdcY);

            if (hit >= 0)
            {
                shared.renderPressMs = juce::Time::getMillisecondCounterHiRes();
                const int p = parameterFor (hit);

                if (controls[(size_t) hit].kind == ControlKind::button || controls[(size_t) hit].kind == ControlKind::toggle)
                {
                    bridge.setValueWithSource (p, bridge.getNormalised (p) > 0.5f ? 0.0f : 1.0f, ControlSource::user);
                    heldButton = hit;
                }
                else
                {
                    dragControl = hit;
                    dragParam = p;
                    dragValue = bridge.getNormalised (p);
                    shared.activeControl = hit;
                    shared.dragging = true;
                    shared.renderDragParam = p;
                }
            }

            lastPointerX = pointerX;
            lastPointerY = pointerY;
        }
        else if (leftDown && dragParam >= 0)
        {
            const float dx = pointerX - lastPointerX, dy = pointerY - lastPointerY;
            lastPointerX = pointerX;
            lastPointerY = pointerY;

            if (std::abs (dx) + std::abs (dy) > 0.0f)
            {
                const float pixelsForFullRange = fineDrag ? 1400.0f : 260.0f;
                dragValue = juce::jlimit (0.0f, 1.0f, dragValue + (-dy + dx * 0.25f) / pixelsForFullRange);
                bridge.setValueWithSource (dragParam, dragValue, ControlSource::user);
            }
        }

        if (released)
            heldButton = -1;

        if (released && dragParam >= 0)
        {
            dragControl = dragParam = -1;
            shared.activeControl = -1;
            shared.dragging = false;
            shared.renderDragParam = -1;
        }

        if (dragParam < 0 && pointerInside && ! overPanel)
        {
            const auto cam = CameraRig::build ((float) w / (float) h, parallaxX, parallaxY, { shared.focusUnit.load(), focusAmount });
            const int control = pickControl (cam, pointerNdcX, pointerNdcY);
            shared.hoveredControl = control;
            shared.hoveredUnit = control >= 0 ? -1 : pickUnit (cam, pointerNdcX, pointerNdcY);
        }
        else if (dragParam < 0)
        {
            shared.hoveredControl = -1;
            shared.hoveredUnit = -1;
        }
    }

    /** A parameter's value in its own units, and back (the spec's range and skew). */
    static juce::NormalisableRange<float> rangeOf (const pad::params::Spec& s)
    {
        juce::NormalisableRange<float> r (s.minValue, s.maxValue);
        if (s.skewCentre > 0.0f)
            r.setSkewForCentre (s.skewCentre);
        return r;
    }

    /** Where a knob should stand when an auto mode is turning it (normalised), or `value` when none is.
        AUTO heaven moves REVERB, DECAY, SHIMMER, SPACE TONE, WIDTH, AIR and SUB toward what it chose, as far
        as it has blended in; MATCH turns OUTPUT by the level-match gain it is applying. Display only: the
        parameters keep the user's settings, which come back as the auto mode lets go. */
    float HardwareRenderer::autoTurnedValue (int control, float value) const
    {
        const int role = autoRole[(size_t) control];   // resolved once in the constructor
        const auto* spec = autoSpec[(size_t) control];
        if (role < 0 || spec == nullptr)
            return value;

        const auto range = rangeOf (*spec);
        const float user = range.convertFrom0to1 (value);
        auto realOf = [this] (int index, const pad::params::Spec* s)
        {
            return s != nullptr && index >= 0 ? rangeOf (*s).convertFrom0to1 (bridge.getNormalised (index)) : 0.0f;
        };

        float shown = user;
        if (role == autoOutputRole)
        {
            if (realOf (silkAutoParam, silkAutoSpec) > 0.5f)
                shown = user + meters.silkMatchDb.load (std::memory_order_relaxed);
        }
        else
        {
            static constexpr float fromDsp[7] { 10.0f, 1.0f, 10.0f, 10.0f, 100.0f, 1.0f, 1.0f };   // knob units per DSP unit
            const float blend = meters.heavenAutoBlend.load (std::memory_order_relaxed);
            const float multiply = realOf (seraphMultiplyParam, seraphMultiplySpec);
            if (blend < 1.0e-3f || multiply < 0.05f)
                return value;
            const float chosen = meters.heavenAutoChoice[(size_t) role].load (std::memory_order_relaxed) * fromDsp[role] / multiply;
            shown = user + (chosen - user) * blend;
        }
        return range.convertTo0to1 (std::clamp (shown, spec->minValue, spec->maxValue));
    }

    void HardwareRenderer::updateAnimation (float dt)
    {
        const int hovered = shared.hoveredControl.load();
        const int active  = shared.activeControl.load();
        bool busy = false;

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const int p = parameterFor (i);
            const float value = bridge.getNormalised (p);

            if (c.kind == ControlKind::button)
            {
                auto& bt = buttons[(size_t) i];
                bt.update (value > 0.5f, heldButton == i && leftDown, dt);
                busy = busy || ! bt.isIdle();
                continue;
            }

            if (c.kind == ControlKind::toggle)
            {
                auto& tg = toggles[(size_t) i];
                const float a = switchAngle[(size_t) c.switchStyle];
                tg.update (value > 0.5f, -a, a, dt);   // on: a rocker's I end pressed in, a lever up
                busy = busy || ! tg.isIdle();
                continue;
            }

            auto& k = knobs[(size_t) i];
            const auto counter = bridge.getChangeCounter (p);
            bool changed = false;

            if (k.boundParam != p)
            {
                k.boundParam = p;
                k.lastCounter = counter;
                k.source = (int) bridge.getLastSource (p);
            }
            else if (counter != k.lastCounter)
            {
                k.lastCounter = counter;
                changed = true;
            }

            // An auto mode turns the knob itself (not while you are holding it)
            const float shownValue = active == i ? value : autoTurnedValue (i, value);
            const float target = c.kind != ControlKind::selector ? knobAngleForValue (shownValue)
                                : c.unit == characterUnit ? characterSelectorAngle (shownValue) : selectorAngleForValue (shownValue);
            const bool isHovered = (hovered == i || active == i);
            k.update (target, changed, (int) bridge.getLastSource (p), isHovered, dt);
            busy = busy || ! k.isIdle (target, isHovered);
        }

        // SERAPH: power lamp / backlight, live L/R display, glow through the lid
        {
            const bool on = seraphModeParam >= 0 && bridge.getNormalised (seraphModeParam) > 0.25f;
            tubePower = anim::approach (tubePower, on ? 1.0f : 0.0f, on ? 5.0f : 3.0f, dt);   // filament-like warm up

            // Bars rise instantly and fall like a meter (~250 ms); dips follow a little faster
            auto meter = [dt, &busy] (float& shown, float target, float fallRate)
            {
                const float before = shown;
                shown = target > shown ? target : anim::approach (shown, target, fallRate, dt);
                busy = busy || std::abs (shown - before) > 1.0e-3f;
            };

            // Display order: SMOOTH AIR WARMTH BODY TAPE | LEVEL | WIDTH SPACE SHIMMER
            constexpr std::array<int, displayColumns> activityOf { 0, 1, 2, 3, 4, -1, 5, 6, 7 };
            const float level = std::clamp (meters.seraphLevelDb.load (std::memory_order_relaxed) / 9.0f, -1.0f, 1.0f);
            for (int col = 0; col < displayColumns; ++col)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& shown = seraphColumns[(size_t) (col * 2 + ch)];
                    if (activityOf[(size_t) col] < 0)
                    {
                        const float before = shown;
                        shown = anim::approach (shown, level, 6.0f, dt);
                        busy = busy || std::abs (shown - before) > 1.0e-3f;
                        continue;
                    }
                    float db = meters.seraphActivityDb[(size_t) (activityOf[(size_t) col] * 2 + ch)].load (std::memory_order_relaxed);
                    if (demoMeters)   // PAD_UI_TEST_DEMO=1: animated values for screenshots without audio
                        db = -30.0f + 18.0f * (0.5f + 0.5f * std::sin ((float) timeSeconds * 1.3f + (float) col * 0.9f + (float) ch * 0.6f));
                    meter (shown, saturateUi ((db + 42.0f) / 36.0f) * tubePower, 4.0f);   // -42 dB .. -6 dB
                }

            for (size_t k = 0; k < seraphDips.size(); ++k)
            {
                const float before = seraphDips[k];
                const float dipTarget = demoMeters ? -9.0f * std::exp (-std::pow (((float) k - 17.0f) / 1.2f, 2.0f)) - 4.0f * std::exp (-std::pow (((float) k - 7.0f) / 1.0f, 2.0f))
                                                   : meters.silkDipDb[k].load (std::memory_order_relaxed);
                seraphDips[k] = anim::approach (seraphDips[k], dipTarget, 18.0f, dt);
                busy = busy || std::abs (seraphDips[k] - before) > 0.01f;
            }

            const float space = std::max (seraphColumns[14], seraphColumns[15]);
            tubeWarmth = anim::approach (tubeWarmth, tubePower * (0.35f + 0.65f * space), 4.0f, dt);
            busy = busy || std::abs (tubePower - (on ? 1.0f : 0.0f)) > 1.0e-3f;
        }

        // The 1U units: the meter movements, and the backlight behind their dials
        {
            const bool tideOn = bridge.getNormalised (bridge.indexOf (params::id::tideActive)) > 0.5f;
            const bool lumenOn = bridge.getNormalised (bridge.indexOf (params::id::lumenActive)) > 0.5f;
            const bool limiterOn = bridge.getNormalised (bridge.indexOf (params::id::spectralActive)) > 0.5f;
            const bool balancerOn = bridge.getNormalised (bridge.indexOf (params::id::balActive)) > 0.5f;
            const bool deepOn = bridge.getNormalised (bridge.indexOf (params::id::deepActive)) > 0.5f;
            const bool characterOn = bridge.getNormalised (bridge.indexOf (params::id::charActive)) > 0.5f;
            const bool radarOn = bridge.getNormalised (bridge.indexOf (params::id::footstep)) > 0.5f;
            unitLamp[(size_t) radarUnit] = anim::approach (unitLamp[(size_t) radarUnit], radarOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) characterUnit] = anim::approach (unitLamp[(size_t) characterUnit], characterOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) deepUnit] = anim::approach (unitLamp[(size_t) deepUnit], deepOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) tideUnit] = anim::approach (unitLamp[(size_t) tideUnit], tideOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) lumenUnit] = anim::approach (unitLamp[(size_t) lumenUnit], lumenOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) limiterUnit] = anim::approach (unitLamp[(size_t) limiterUnit], limiterOn ? 1.0f : 0.15f, 4.0f, dt);
            unitLamp[(size_t) levelUnit] = anim::approach (unitLamp[(size_t) levelUnit], 1.0f, 4.0f, dt);     // meters: always lit
            unitLamp[(size_t) monitorUnit] = anim::approach (unitLamp[(size_t) monitorUnit], 1.0f, 4.0f, dt);
            unitLamp[(size_t) balancerUnit] = anim::approach (unitLamp[(size_t) balancerUnit], balancerOn ? 1.0f : 0.25f, 4.0f, dt);

            // SPECTRAL LIMITER: the moving cuts as the audio thread published them (or the demo)
            std::array<enh::dsp::SpectralLimiter::Slot, enh::dsp::SpectralLimiter::numSlots> cuts {};
            float broadband = 0.0f;
            if (demoMeters && limiterOn)
                cuts = demoLimiterSlots (timeSeconds);
            else
            {
                for (size_t s = 0; s < cuts.size(); ++s)
                    cuts[s] = { (enh::dsp::SpectralLimiter::Shape) meters.limitShape[s].load (std::memory_order_relaxed),
                                meters.limitHz[s].load (std::memory_order_relaxed), meters.limitOctaves[s].load (std::memory_order_relaxed),
                                limiterOn ? meters.limitDepthDb[s].load (std::memory_order_relaxed) : 0.0f };
                broadband = limiterOn ? meters.limitBroadbandDb.load (std::memory_order_relaxed) : 0.0f;
            }
            float deepest = 0.0f;
            for (auto& c : cuts)
                deepest = std::max (deepest, c.depthDb);

            // On the analyser: the cut at 48 points of its log axis (20 Hz - 20 kHz), instant on, ~80 ms off
            const float fall = 1.0f - std::exp (-dt / 0.08f);
            for (size_t i = 0; i < limitCurve.size(); ++i)
            {
                const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) (limitCurve.size() - 1));
                const float cut = -enh::dsp::SpectralLimiter::responseDb (cuts, hz);
                const float before = limitCurve[i];
                limitCurve[i] = cut > limitCurve[i] ? cut : limitCurve[i] + (cut - limitCurve[i]) * fall;
                busy = busy || std::abs (limitCurve[i] - before) > 0.01f;
            }
            limitBroadband = broadband > limitBroadband ? broadband : limitBroadband + (broadband - limitBroadband) * fall;

            // The compressor reads gain reduction (0..12 dB), the leveler the lift in each band (0..18 dB),
            // the limiter its deepest spectral cut (0..18 dB) and its broadband protection (0..12 dB)
            const float readings[numNeedles] {
                saturateUi (meters.tideGrDb.load() / 12.0f),
                saturateUi (meters.lumenGainDb[0].load() / 18.0f),
                saturateUi (meters.lumenGainDb[1].load() / 18.0f),
                saturateUi (meters.lumenGainDb[2].load() / 18.0f),
                saturateUi (deepest / 18.0f),
                saturateUi (broadband / 12.0f),
                // LEVEL's INPUT: the level going into the rack after the knob, -40 .. 0 dBFS (RMS)
                saturateUi ((demoMeters ? -16.0f + 5.0f * std::sin ((float) timeSeconds * 1.1f) : scope.inputRmsDb.load()) / 40.0f + 1.0f),
                // Loudness: -40 .. 0 LUFS across the dial (the demo swings them through the middle)
                saturateUi ((demoMeters ? -18.0f + 7.0f * std::sin ((float) timeSeconds * 1.7f) : meters.momentaryLufs.load()) / 40.0f + 1.0f),
                saturateUi ((demoMeters ? -20.0f + 2.0f * std::sin ((float) timeSeconds * 0.4f) : meters.shortTermLufs.load()) / 40.0f + 1.0f),
                // DEEP SUB: what it is adding, -40 .. 0 dBFS RMS
                saturateUi ((demoMeters ? -14.0f + 6.0f * std::sin ((float) timeSeconds * 0.9f) : (deepOn ? meters.deepGeneratedDb.load() : -120.0f)) / 40.0f + 1.0f),
                // CHARACTER: the harmonics its models add, against the signal, -60 .. 0 dB
                saturateUi ((demoMeters ? -34.0f + 8.0f * std::sin ((float) timeSeconds * 0.7f) : (characterOn ? meters.charHarmonicsDb.load() : -120.0f)) / 60.0f + 1.0f),
                // FOOTSTEP RADAR: the lift it is giving a step now, 0 .. 18 dB (the demo: a step every half second)
                saturateUi ((demoMeters ? 9.0f * std::pow (std::max (0.0f, std::cos ((float) timeSeconds * 6.2832f)), 8.0f)
                                        : 20.0f * std::log10 (1.0f + 1.5f * meters.radarActivity.load())) / 18.0f),
            };

            for (int i = 0; i < numNeedles; ++i)
            {
                auto& n = needles[(size_t) i];
                const float before = n.angle;
                n.update (hwk::models::vuAngleFor (readings[(size_t) i]), dt);
                busy = busy || std::abs (n.angle - before) > 2.0e-4f || std::abs (n.velocity) > 1.0e-3f;
            }
        }

        // CLARITY scale swap: the printed scale cross-fades and the knob dips as it changes over
        {
            const bool addMode = modeControl >= 0 && bridge.getNormalised (controlParam[(size_t) modeControl]) > 0.5f;
            if (modeBlend < 0.0f)
                modeBlend = addMode ? 1.0f : 0.0f;
            else if (addMode != lastAddMode)
                swapPulse = 1.0f;

            lastAddMode = addMode;
            modeBlend = anim::approach (modeBlend, addMode ? 1.0f : 0.0f, 9.0f, dt);
            swapPulse = std::max (0.0f, swapPulse - dt / 0.35f);
            busy = busy || swapPulse > 0.0f || std::abs (modeBlend - (addMode ? 1.0f : 0.0f)) > 1.0e-3f;
        }

        // Live DSP meters (smoothed for display only)
        const float meterK = 1.0f - std::exp (-dt / 0.06f);
        for (int b = 0; b < enh::dsp::numBands; ++b)
            displayBands[(size_t) b] += (meters.bandGainDb[(size_t) b].load (std::memory_order_relaxed) - displayBands[(size_t) b]) * meterK;

        // PAD_UI_TEST_DEMO=1: music-like meter movement, so the ladders can be looked at without audio
        const float demoT = (float) timeSeconds;
        const float demoPeak = -9.0f + 5.0f * std::sin (demoT * 1.3f) + 3.0f * std::sin (demoT * 7.9f) * std::sin (demoT * 3.1f);
        stepFlash = std::max (demoMeters ? std::max (0.0f, std::sin (demoT * 2.2f)) : meters.footstepConfidence.load (std::memory_order_relaxed),
                              stepFlash * std::exp (-dt / 0.25f));
        activityGlow = anim::approach (activityGlow, demoMeters ? 0.55f + 0.35f * std::sin (demoT * 0.9f)
                                                                : meters.enhancement.load (std::memory_order_relaxed), 6.0f, dt);

        // OUT: peak-meter ballistics - up at once, down at 20 dB a second. The raw peak of each audio
        // block jumps several dB from block to block, and lit straight from it the segments around the
        // level flashed on and off every frame.
        const float outRaw = demoMeters ? demoPeak : meters.outputPeakDb.load (std::memory_order_relaxed);
        outDbShown = outRaw >= outDbShown ? outRaw : std::max (outRaw, outDbShown - 20.0f * dt);
        const float outDb = outDbShown;

        // LED ladders: on over ~15 ms, off over ~70 ms, like real LEDs behind a little diffuser (switched
        // on in a single frame they flashed hard, and their halos popped with them)
        auto ladder = [dt] (std::array<float, ladderSegments>& leds, int segments, auto&& lit)
        {
            for (int k = 0; k < segments; ++k)
            {
                const float target = lit (k) ? 1.0f : 0.0f;
                auto& v = leds[(size_t) k];
                v = anim::approach (v, target, target > v ? 65.0f : 14.0f, dt);
            }
        };
        ladder (outLeds, outLadder.segments, [outDb] (int k) { return outDb >= (float) outLadderDb[(size_t) k]; });
        ladder (enhLeds, enhLadder.segments, [this] (int k) { return activityGlow * (float) enhLadder.segments > (float) k + 0.5f; });
        ladder (detectLeds, detectLadder.segments, [this] (int k) { return stepFlash * (float) detectLadder.segments > (float) k + 0.3f; });

        // Parallax: subtle, and fast enough that it never feels like it is chasing the pointer
        const bool parallaxOn = ! config.reduceMotion && pointerInside;
        auto settle = [&busy, dt] (float& v, float target, float rate)
        {
            v = anim::approach (v, target, rate, dt);
            if (std::abs (v - target) > 1.0e-3f)
                busy = true;
        };

        // The signal-flow pulse travelling up the chain

        // Walking up to the rack, or stepping back from it
        {
            const float before = focusAmount;
            focusAmount = anim::approach (focusAmount, std::clamp (shared.focusTarget.load(), 0.0f, 1.0f), 6.0f, dt);
            shared.focusAmount.store (focusAmount);
            busy = busy || std::abs (focusAmount - before) > 1.0e-4f;
        }

        settle (parallaxX, parallaxOn ? pointerNdcX * config.parallaxAmount : 0.0f, 12.0f);
        settle (parallaxY, parallaxOn ? pointerNdcY * config.parallaxAmount : 0.0f, 12.0f);

        shared.parallaxX = parallaxX;
        shared.parallaxY = parallaxY;
        shared.animating = busy;
    }

    //==============================================================================
    void HardwareRenderer::paceFrame (double frameStartMs)
    {
        // Live meters keep the UI "busy" while audio is being processed
        const bool audioActive = meters.enhancement.load (std::memory_order_relaxed) > 0.01f || stepFlash > 0.01f;

        if (pointerInside || audioActive || shared.animating.load() || shared.dragging.load())
            busyUntilMs = frameStartMs + 1000.0;

        const bool busy = frameStartMs < busyUntilMs;
        const int rate = busy ? config.frameRate : config.idleFrameRate;
        const int wanted = rate <= 35 ? 2 : 1;

        if (wanted != swapInterval)
            if (auto* ctx = juce::OpenGLContext::getCurrentContext())
            {
                ctx->setSwapInterval (wanted);
                swapInterval = wanted;
            }

        // Fallback limiter when vsync isn't honoured (or on high-refresh displays)
        const double targetMs = 1000.0 / juce::jmax (1, rate);
        const double sinceLast = frameStartMs - lastFrameMs;

        if (lastFrameMs > 0.0 && sinceLast < targetMs - 3.0)
            juce::Thread::sleep (juce::roundToInt (targetMs - 3.0 - sinceLast));
    }

    void HardwareRenderer::recordStats (double frameStartMs, double renderMs)
    {
        if (lastFrameMs > 0.0)
        {
            const double interval = frameStartMs - lastFrameMs;
            statSum += interval;
            statMax = std::max (statMax, interval);
            statRenderSum += renderMs;
            ++statCount;

            if (interval > 1.5 * (swapInterval > 1 ? 33.3 : 16.7))
                ++statLong;
        }

        if (statStart <= 0.0)
            statStart = frameStartMs;

        if (frameStartMs - statStart > 5000.0 && statCount > 0)
        {
            std::fprintf (stderr, "[enh-stats] swap=%d frames=%d avgInterval=%.2fms maxInterval=%.2fms late=%d avgCpuRender=%.2fms pointer=%s\n",
                          swapInterval, statCount, statSum / statCount, statMax, statLong, statRenderSum / statCount,
                          shared.nativeWindow.load() != 0 ? "polled" : "events");
            statSum = statMax = statRenderSum = 0.0;
            statCount = statLong = 0;
            statStart = frameStartMs;
        }
    }

    void HardwareRenderer::renderOpenGL()
    {
        double now = juce::Time::getMillisecondCounterHiRes();

        if (ready)
        {
            pollPointer();
            handleInteraction();
            paceFrame (now);
            now = juce::Time::getMillisecondCounterHiRes();
        }

        const float dt = lastFrameMs > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastFrameMs) * 0.001) : 1.0f / 60.0f;
        timeSeconds += dt;

        auto* ctx = juce::OpenGLContext::getCurrentContext();
        const double scale = ctx != nullptr ? ctx->getRenderingScale() : 1.0;
        const int logicalW = juce::jmax (1, shared.viewWidth.load());
        const int logicalH = juce::jmax (1, shared.viewHeight.load());
        const int w = juce::roundToInt (scale * logicalW), h = juce::roundToInt (scale * logicalH);

        glViewport (0, 0, w, h);

        if (! ready)
        {
            glClearColor (0.95f, 0.52f, 0.68f, 1.0f);
            glClear (GL_COLOR_BUFFER_BIT);
            lastFrameMs = now;
            return;
        }

        updateAnimation (dt);
        updatePanel (dt);
        uploadPanelIfChanged();
        uploadOverlayIfChanged();
        uploadCalloutIfChanged();
        uploadLevelLabelsIfChanged();
        uploadScope();          // once a frame, however many views are drawn (the loupe draws the scene twice)
        uploadDisplays (dt);

        const auto camera = CameraRig::build ((float) logicalW / (float) logicalH, parallaxX, parallaxY,
                                             { shared.focusUnit.load(), focusAmount });

        // Loupe: fades and zooms in over hovered print, keeps its last anchor while fading out
        const bool loupeWanted = shared.calloutVisible.load();
        if (loupeWanted)
        {
            loupeUnit = shared.calloutUnit.load();
            loupeX = shared.calloutX.load();
            loupeZ = shared.calloutZ.load();
        }
        // Opening: zoom and fade together. Closing: the glass zooms back out first (fast), the
        // fade trails it (slow), so it reads as zooming out and then being gone.
        loupeAlpha = anim::approach (loupeAlpha, loupeWanted ? 1.0f : 0.0f, loupeWanted ? 17.0f : 6.5f, dt);
        loupeZoom = anim::approach (loupeZoom, loupeWanted ? 1.8f : 1.0f, loupeWanted ? 10.0f : 15.0f, dt);   // gentler than 2.2x
        if (loupeAlpha > 0.01f)
            renderLoupeView (camera, w, h);

        // The scene into our own multisampled buffer, supersampled on small windows, then filtered down to
        // the screen. If that buffer cannot be had, straight to the screen as before.
        const float ss = renderScaleFor (logicalW);
        const int sw = juce::roundToInt ((float) w * ss), sh = juce::roundToInt ((float) h * ss);
        if (sceneTarget.ensureSize (sw, sh, config.msaaSamples))
        {
            sceneTarget.bind();
            drawScene (camera, sw, sh);
            sceneTarget.resolve();          // leaves the host's framebuffer bound
            glViewport (0, 0, w, h);
            presentScene (w, h);
        }
        else
        {
            drawScene (camera, w, h);
        }

        // The glass panel over the finished frame: the line from its unit, then the frosted glass
        if (panelShown >= 0)
        {
            drawPanelConnector (camera, w, h);
            if (panelOpen > 0.01f)
                drawGlassPanel (w, h, blurBehindPanel (w, h));
        }
        drawLoupe (w, h);

        if (statsEnabled)
            recordStats (now, juce::Time::getMillisecondCounterHiRes() - now);

        lastFrameMs = now;
    }

    //==============================================================================
    gfx::ShaderProgram& HardwareRenderer::use (int material)
    {
        auto& program = programs[(size_t) material];

        if (current != &program)
        {
            program.use();
            current = &program;
        }

        if (programFrame[(size_t) material] != frameIndex)
        {
            programFrame[(size_t) material] = frameIndex;
            program.set ("uViewProj", frameCamera.viewProj);
            program.set ("uCamPos", frameCamera.eye);
            program.set ("uLightDir", frameCamera.lightDir);
            program.set ("uTime", (float) timeSeconds);
            program.set ("uViewport", (float) frameW, (float) frameH);
            program.set ("uVignette", vignette);
            program.set ("uTex", 0);
        }

        return program;
    }

    void HardwareRenderer::draw (const gfx::GpuMesh& mesh, const Mat4& model, Vec3 base, Vec3 emissive)
    {
        jassert (current != nullptr);
        current->set ("uModel", model);
        current->set ("uBaseColor", base);
        current->set ("uEmissive", emissive);
        mesh.draw();
    }

    void HardwareRenderer::drawShadow (const Mat4& space, float cx, float y, float cz, float hw, float hd,
                                       float radius, float blur, float strength)
    {
        const float sx = hw + blur * 2.0f + 0.02f, sz = hd + blur * 2.0f + 0.02f;
        auto& p = use (shaders::shadow);
        p.set ("uParams", sx, sz, hw, hd);
        p.set ("uParams2", radius, blur, strength, 0.0f);
        draw (meshes.quad, space * Mat4::translation ({ cx, y, cz }) * Mat4::scale (sx, 1.0f, sz), {});
    }

    void HardwareRenderer::drawLed (const Mat4& panel, float x, float z, Vec3 colour, float on)
    {
        // Lens tinted even when off, like real coloured LEDs; lit ones also get a soft halo
        draw (meshes.led, panel * Mat4::translation ({ x, 0.0f, z }) * Mat4::scale (ledRadius, ledRadius, ledRadius),
              mixVec (colour * 0.07f, colour * 0.35f, on), colour * (1.25f * on));
        queueGlow (panel, x, z, ledRadius * 4.0f, colour, 0.42f * on);
    }

    void HardwareRenderer::Needle::update (float target, float dt) noexcept
    {
        // A moving coil: spring toward the reading, damped, so it settles like the real thing
        const float stiffness = 240.0f, damping = 21.0f;
        const float step = std::min (dt, 0.02f);
        velocity += (stiffness * (target - angle) - damping * velocity) * step;
        angle += velocity * step;
    }

    /** One of the 1U units: brushed plate, engraved print, knobs in a bordered section, and
        moving-coil meters behind glass. All three are the same build, different print. */
    void HardwareRenderer::drawOneU (int unit, const Mat4& panel, Vec3 colour, const gfx::Texture2D& decal,
                                     std::initializer_list<const gfx::Texture2D*> faces)
    {
        const float lamp = unitLamp[(size_t) unit];
        auto& model = vuModelFor (unit);
        auto& shell = outboard[(size_t) unit];
        const float halfH = unitHalfH (unit);

        /*  The meters, grouped by material rather than by meter: every program switch costs a
            uniform upload, and the leveler has three of these side by side. */
        using hwk::models::Role;

        auto matrixFor = [&] (int i, bool moving)
        {
            const auto at = panel * Mat4::translation ({ vuX (unit, i), 0.0f, vuZ (unit, i) });
            if (! moving)
                return at;

            // The movement is hinged below the window: turn about that hinge, not the centre
            const auto& needle = needles[(size_t) (firstNeedle (unit) + i)];
            return at * Mat4::translation ({ 0.0f, 0.0f, model.pivotOffset })
                      * Mat4::rotationY (-needle.angle)
                      * Mat4::translation ({ 0.0f, 0.0f, -model.pivotOffset });
        };

        auto drawRole = [&] (Role role)
        {
            for (auto& part : model.parts)
            {
                if (part->role != role)
                    continue;

                for (int i = 0; i < numVus (unit); ++i)
                    draw (part->mesh, matrixFor (i, part->rotates), part->colour);
            }
        };

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        drawRole (Role::body);

        // Dial faces: one print shared by every meter, or one each
        use (shaders::vuFace).set ("uParams", 0.0f, lamp, 0.0f, 0.0f);
        if (faces.size() == 1)
        {
            (*faces.begin())->bind (0);
            drawRole (Role::screen);
        }
        else
        {
            int i = 0;
            for (auto* face : faces)
            {
                face->bind (0);
                for (auto& part : model.parts)
                    if (part->role == Role::screen && i < numVus (unit))
                        draw (part->mesh, matrixFor (i, part->rotates), part->colour);
                ++i;
            }
        }

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        drawRole (Role::pointer);

        use (shaders::chrome).set ("uParams", 0.55f, 0.0f, 0.0f, 0.0f);
        drawRole (Role::metal);

        auto& recessed = use (shaders::recess);
        recessed.set ("uParams", faceThick, 0.0f, 0.0f, 0.0f);
        recessed.set ("uGlow", Vec3 {});
        draw (shell.earWalls, panel, { 0.09f, 0.09f, 0.10f });

        use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
        draw (shell.screws, panel, { 0.75f, 0.74f, 0.78f });

        use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (shell.earFloors, panel, { 0.012f, 0.012f, 0.014f });

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (shell.screwSlots, panel, { 0.02f, 0.02f, 0.025f });

        // Brushed faceplate with the print engraved into it
        decal.bind (0);
        auto& plate = use (shaders::brushed);
        plate.set ("uParams", -faceHalfW, -halfH, 2.0f * faceHalfW, 2.0f * halfH);
        plate.set ("uParams2", 0.0f, 0.0f, 0.0f, (float) unit);      // .w seeds this unit's wear
        plate.setArray ("uWear", hwk::shaders::wearUniforms ((float) unit + 7.0f).data(), 15);
        draw (shell.faceTop, panel, colour);
        draw (shell.faceEdges, panel, colour * 0.82f);

        use (shaders::chassis);
        draw (shell.body, panel, colours::chassisBlack);
        use (shaders::chrome).set ("uParams", 0.42f, 0.0f, 0.0f, 0.0f);
        draw (meshes.bodyScrews, panel * Mat4::translation ({ 0.0f, 0.0f, 0.012f - halfH }), { 0.42f, 0.42f, 0.44f });
    }

    /** The cover glass over a unit's meters, drawn with everything else transparent. */
    void HardwareRenderer::drawVuGlass (int unit, const Mat4& panel)
    {
        auto& model = vuModelFor (unit);
        use (shaders::vuGlass);

        for (auto& part : model.parts)
        {
            if (part->role != hwk::models::Role::glass)
                continue;

            for (int i = 0; i < numVus (unit); ++i)
                draw (part->mesh, panel * Mat4::translation ({ vuX (unit, i), 0.0f, vuZ (unit, i) }), part->colour);
        }
    }

    void HardwareRenderer::drawScene (const CameraRig& cam, int vw, int vh)
    {
        ++frameIndex;
        frameCamera = cam;
        frameW = vw;
        frameH = vh;
        current = nullptr;

        const float t = (float) timeSeconds;
        const Vec3 zero {};
        const Vec3 L = cam.lightDir;
        const Mat4 panel = panelToWorld();
        const Mat4 tubePanel = panelToWorld (tubeUnit);
        const Mat4 tidePanel = panelToWorld (tideUnit);
        const Mat4 lumenPanel = panelToWorld (lumenUnit);
        const Mat4 limiterPanel = panelToWorld (limiterUnit);
        const Mat4 deepPanel = panelToWorld (deepUnit);
        const Mat4 characterPanel = panelToWorld (characterUnit);
        const Mat4 radarPanel = panelToWorld (radarUnit);
        const Mat4 levelPanel = panelToWorld (levelUnit);
        const Mat4 balancerPanel = panelToWorld (balancerUnit);
        const Mat4 monitorPanel = panelToWorld (monitorUnit);
        const auto panelFor = [&] (int unit) -> const Mat4&
        {
            return unit == tubeUnit ? tubePanel : unit == tideUnit ? tidePanel : unit == lumenUnit ? lumenPanel
                 : unit == limiterUnit ? limiterPanel : unit == deepUnit ? deepPanel : unit == characterUnit ? characterPanel : unit == levelUnit ? levelPanel : unit == balancerUnit ? balancerPanel
                 : unit == radarUnit ? radarPanel
                 : unit == monitorUnit ? monitorPanel : panel;
        };

        const Mat4 I = Mat4::identity();


        glClearColor (0.03f, 0.022f, 0.026f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LEQUAL);
        glDepthMask (GL_TRUE);
        glDisable (GL_CULL_FACE);
        glDisable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float footOn = std::clamp ((unitLamp[(size_t) radarUnit] - 0.15f) / 0.85f, 0.0f, 1.0f);   // the FOOTSTEP RADAR is IN
        const float breath = 0.90f + 0.10f * std::sin (t * 1.6f);
        const Vec3 ventGlow = colours::amber * ((0.05f + 0.45f * activityGlow) * breath) + colours::ledGreen * (stepFlash * footOn * 0.35f);

        glowCount = 0;

        // =============================================================================
        // Opaque, front to back
        // =============================================================================
        if (gallery)
        {
            // Every knob style in order, 10 across; then the switch and button styles on the last row
            const int n = (int) galleryKnobs.size();
            for (int s = 0; s < n; ++s)
            {
                const float x = -2.20f + 0.49f * (float) (s % 10), z = -0.72f + 0.33f * (float) (s / 10);
                const auto at = panel * Mat4::translation ({ x, 0.05f, z }) * Mat4::rotationY (-0.6f);
                drawModel (*galleryKnobs[(size_t) s], at, at, {}, { 0.94f, 0.94f, 0.96f });
            }
            for (int s = 0; s < hwk::models::numSwitchStyles; ++s)
            {
                const auto at = panel * Mat4::translation ({ -2.20f + 0.40f * (float) s, 0.05f, 0.95f });
                drawModel (switchModels[(size_t) s][2], at * Mat4::translation ({ 0.0f, switchPivotY[(size_t) s], 0.0f }) * Mat4::rotationX (-switchAngle[(size_t) s]),
                           at, {}, { 0.93f, 0.93f, 0.95f });
            }
            for (int s = 0; s < hwk::models::numButtonStyles; ++s)
            {
                const auto at = panel * Mat4::translation ({ 0.0f + 0.42f * (float) s, 0.05f, 0.95f });
                drawModel (buttonModels[(size_t) s][2], at, at, {}, {});
            }
        }

        hullModel = nullptr;
        int hullControl = shared.hoveredControl.load();
        if (testHoverControl.isNotEmpty())
            for (int i = 0; i < numControls; ++i)
                if (testHoverControl == controls[(size_t) i].paramId)
                    hullControl = i;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (gallery && c.unit == enhUnit)
                continue;
            const auto& unitPanel = panelFor (c.unit);
            const auto base = unitPanel * Mat4::translation ({ c.x, 0.0f, c.z });
            const bool hovered = shared.hoveredControl.load() == i || shared.activeControl.load() == i;
            auto captureHull = [&] (const GpuModel& model, const Mat4& moving)
            {
                if (i == hullControl)
                {
                    hullModel = &model;
                    hullMoving = moving;
                    hullFixed = base;
                }
            };

            if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
            {
                const auto& k = knobs[(size_t) i];
                const int detail = detailFor (cam, unitPanel, c.x, c.z, knobBodyRadius (c) * 1.3f, vw);
                const int modelIndex = knobModelIndex[(size_t) i][(size_t) detail];
                if (modelIndex < 0)
                    continue;

                // A mode-switched knob sinks briefly while its printed scale swaps
                const float dip = altParam[(size_t) i] >= 0 ? -0.035f * std::sin (pi * swapPulse) : 0.0f;
                const auto spin = base * Mat4::translation ({ 0.0f, dip, 0.0f }) * Mat4::rotationY (-k.angle);

                // Pointer: the model's own paint (white on dark knobs, black on metal); hover lifts it slightly
                drawModel (*knobModels[(size_t) modelIndex], spin, base, Vec3 { 0.025f, 0.025f, 0.027f } * k.hover,
                           {}, 1.0f, false);
                captureHull (*knobModels[(size_t) modelIndex], spin);
            }
            else if (c.kind == ControlKind::toggle)
            {
                // I / O rocker: the paddle and its marks rock about the pivot; the bezel stays put
                const auto& tg = toggles[(size_t) i];
                const Vec3 lift = hovered ? Vec3 { 0.05f, 0.05f, 0.06f } : Vec3 {};
                const auto style = (size_t) c.switchStyle;
                const int detail = detailFor (cam, unitPanel, c.x, c.z, switchOutline (c.switchStyle).halfD, vw);
                const auto rocked = base * Mat4::translation ({ 0.0f, switchPivotY[style], 0.0f }) * Mat4::rotationX (tg.angle);
                drawModel (switchModels[style][(size_t) detail], rocked, base, lift, { 0.93f, 0.93f, 0.95f });
                captureHull (switchModels[style][(size_t) detail], rocked);
            }
            else
            {
                const auto& bt = buttons[(size_t) i];
                const auto pressed = base * Mat4::translation ({ 0.0f, -buttonTravel * bt.travel(), 0.0f });
                const int detail = detailFor (cam, unitPanel, c.x, c.z, buttonHalfW, vw);
                drawModel (buttonModels[(size_t) c.buttonStyle][(size_t) detail], pressed, base, {}, {}, hovered ? 1.12f : 1.0f);
                captureHull (buttonModels[(size_t) c.buttonStyle][(size_t) detail], pressed);

                // Each button's LED on its own unit's panel (the LIFT button's used to be drawn on the
                // enhancer's panel, where it landed inside FOOTSTEP)
                use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                if (! hasLed (c))
                    continue;
                if (i == modeControl)
                {
                    drawLed (unitPanel, c.x - modeLedDx, c.z + buttonLedDz, colours::ledGreen, 1.0f - bt.led);
                    drawLed (unitPanel, c.x + modeLedDx, c.z + buttonLedDz, colours::ledYellow, bt.led);
                }
                else
                {
                    const auto [ledDx, ledDz] = ledOffset (c);
                    drawLed (unitPanel, c.x + ledDx, c.z + ledDz, colours::ledYellow, bt.led);
                }
            }
        }

        // --- the 1U units: compressor and leveler in natural aluminium, the limiter anodised steel-blue
        drawOneU (tideUnit, tidePanel, Vec3 { 0.62f, 0.635f, 0.66f }, tideDecalTex, { &tideLabelTex });
        drawOneU (lumenUnit, lumenPanel, Vec3 { 0.60f, 0.605f, 0.62f }, lumenDecalTex, { &lumenLabelTex });
        drawOneU (limiterUnit, limiterPanel, Vec3 { 0.46f, 0.52f, 0.60f }, limiterDecalTex,
                  { &limiterLabelTex[0], &limiterLabelTex[1] });
        drawOneU (deepUnit, deepPanel, Vec3 { 0.30f, 0.36f, 0.44f }, deepDecalTex, { &deepLabelTex });   // blued steel
        drawOneU (characterUnit, characterPanel, Vec3 { 0.58f, 0.53f, 0.45f }, characterDecalTex, { &characterLabelTex });   // champagne anodised
        drawOneU (radarUnit, radarPanel, Vec3 { 0.27f, 0.30f, 0.21f }, radarDecalTex, { &radarVuFaceTex });   // CHARACTER's sister in olive drab anodised

        // --- LEVEL & LOUDNESS in natural aluminium; the MIX BALANCER in dark graphite, around its display
        drawOneU (levelUnit, levelPanel, Vec3 { 0.64f, 0.645f, 0.66f }, levelDecalTex, { &levelFaceTex });
        drawOneU (balancerUnit, balancerPanel, Vec3 { 0.25f, 0.26f, 0.285f }, balancerDecalTex, {});
        drawOneU (monitorUnit, monitorPanel, Vec3 { 0.62f, 0.625f, 0.64f }, monitorDecalTex, { &monitorFaceTex[0], &monitorFaceTex[1] });
        drawWindows (monitorPanel, balancerPanel);

        // --- TONE & SPACE: display, lamp, screws, faceplate, chassis ----------------------------
        {
            seraphLabelTex.bind (0);
            auto& live = use (shaders::seraphDisplay);
            live.set ("uParams", tubePower, displayDipsU0, displayDipsU1, displayColsU0);
            live.set ("uParams2", displayColsU1, 0.0f, 0.0f, 0.0f);
            live.setArray ("uDips", seraphDips.data(), (int) seraphDips.size());
            live.setArray ("uActivity", seraphColumns.data(), (int) seraphColumns.size());
            draw (meshes.seraphGlass, tubePanel, zero);

            auto& vuRecess = use (shaders::recess);
            vuRecess.set ("uParams", seraphDisplayDepth, 0.0f, 0.0f, 0.0f);
            vuRecess.set ("uGlow", Vec3 { 0.06f, 0.05f, 0.08f } * tubePower);
            draw (meshes.seraphWalls, tubePanel, { 0.06f, 0.05f, 0.08f });
            vuRecess.set ("uParams", faceThick, 0.0f, 0.0f, 0.0f);
            vuRecess.set ("uGlow", zero);
            draw (meshes.tubeEarWalls, tubePanel, { 0.10f, 0.08f, 0.14f });

            use (shaders::chrome).set ("uParams", 0.75f, 0.0f, 0.0f, 0.0f);
            draw (meshes.seraphBezel, tubePanel, { 0.82f, 0.81f, 0.85f });
            use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
            draw (meshes.tubeScrews, tubePanel, { 0.75f, 0.74f, 0.78f });

            // Jewel lamp (HardwareKit model): the jewel is the "pointer" part, lit by POWER
            const auto lampAt = tubePanel * Mat4::translation ({ lampX, 0.0f, lampZ });
            const float lampOn = tubePower * (0.92f + 0.08f * breath);
            drawModel (lampModel, lampAt, lampAt, {}, Vec3 { 0.45f, 0.05f, 0.04f } + Vec3 { 2.4f, 0.18f, 0.09f } * lampOn);
            queueGlow (tubePanel, lampX, lampZ, 0.14f, { 1.0f, 0.12f, 0.06f }, 0.30f * lampOn);

            use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
            draw (meshes.tubeEarFloors, tubePanel, { 0.012f, 0.01f, 0.014f });

            use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
            draw (meshes.tubeScrewSlots, tubePanel, { 0.02f, 0.02f, 0.025f });

            tubeDecalTex.bind (0);
            auto& paint = use (shaders::paint);
            paint.set ("uParams", -faceHalfW, -tubeHalfH, 2.0f * faceHalfW, 2.0f * tubeHalfH);
            paint.set ("uParams2", 0.0f, 0.0f, 0.0f, (float) tubeUnit);
            paint.setArray ("uWear", hwk::shaders::wearUniforms ((float) tubeUnit + 3.0f).data(), 15);
            draw (meshes.tubeFaceTop, tubePanel, colours::seraphPurple);
            draw (meshes.tubeFaceEdges, tubePanel, colours::seraphPurple);

            use (shaders::chassis);
            draw (meshes.tubeBody, tubePanel, colours::chassisBlack);

            // The tubes glowing out through the vents in the top of the body
            const Vec3 tubeGlow = colours::amber * (0.55f * tubeWarmth * (0.94f + 0.06f * breath));
            const auto ventFace = tubePanel * Mat4::translation ({ 0.0f, -0.004f, -tubeHalfH });
            draw (meshes.tubeVents, ventFace, colours::chassisBlack);
            auto& tubeVents = use (shaders::recess);
            tubeVents.set ("uParams", lidVentDepth, 0.0f, 0.0f, 0.0f);
            tubeVents.set ("uGlow", tubeGlow);
            draw (meshes.tubeVentWalls, ventFace, { 0.05f, 0.05f, 0.055f });
            auto& tubeVentFloor = use (shaders::emissive);
            tubeVentFloor.set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
            tubeVentFloor.set ("uGlow", tubeGlow);
            draw (meshes.tubeVentFloors, ventFace, { 0.006f, 0.006f, 0.007f });

            use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
            draw (meshes.bodyScrews, tubePanel * Mat4::translation ({ 0.0f, 0.0f, 0.012f - tubeHalfH }), { 0.42f, 0.42f, 0.44f });
        }

        // --- LED ladders ------------------------------------------------------------------------
        use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        for (int k = 0; k < outLadder.segments; ++k)
        {
            const Vec3 colour = k >= 11 ? colours::ledRed : k >= 8 ? colours::ledYellow : colours::ledGreen;
            drawLed (panel, outLadder.x, ladderLedZ (k), colour, outLeds[(size_t) k]);
        }
        for (int k = 0; k < enhLadder.segments; ++k)
            drawLed (panel, enhLadder.x, ladderLedZ (k), k >= 9 ? colours::ledYellow : colours::ledGreen, enhLeds[(size_t) k]);
        for (int k = 0; k < detectLadder.segments; ++k)
            drawLed (panel, detectLadder.x, ladderLedZ (k), k >= detectLadder.segments - 1 ? colours::ledYellow : colours::ledGreen,
                     detectLeds[(size_t) k] * footOn);

        drawLed (panel, powerLedX, powerLedZ, colours::ledRed, 0.85f * breath);

        // --- screws, display ----------------------------------------------------------------
        use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
        draw (meshes.screws, panel, { 0.75f, 0.74f, 0.78f });

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.screwSlots, panel, { 0.02f, 0.02f, 0.025f });
        draw (meshes.displayBezel, panel, { 0.03f, 0.031f, 0.035f });

        // Analyser: the spectrum strip on unit 0, the printed graticule on unit 1
        scopeTex.bind (0);
        overlayTex.bind (1);
        auto& display = use (shaders::display);
        display.set ("uTex2", 1);
        display.set ("uParams", 0.0f, 1.0f, limitBroadband, footOn * stepFlash);
        display.setArray ("uLimit", limitCurve.data(), (int) limitCurve.size());
        // Show the top 78 dB of the strip's range, and +/- 12 dB of EQ against it
        display.set ("uParams2", 0.135f, 1.0f, 24.0f, 1.0f);
        display.setArray ("uBands", displayBands.data(), enh::dsp::numBands);
        draw (meshes.displayGlass, panel, zero);

        auto& recess = use (shaders::recess);
        recess.set ("uParams", displayDepth, 0.0f, 0.0f, 0.0f);
        recess.set ("uGlow", zero);
        draw (meshes.displayWalls, panel, { 0.05f, 0.05f, 0.06f });
        recess.set ("uParams", faceThick, 0.0f, 0.0f, 0.0f);
        draw (meshes.earWalls, panel, { 0.10f, 0.10f, 0.11f });

        auto& glow = use (shaders::emissive);
        glow.set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.earFloors, panel, { 0.012f, 0.01f, 0.012f });

        // --- faceplate ------------------------------------------------------------------------
        decalTex.bind (0);
        auto& face = use (shaders::faceplate);
        face.set ("uParams2", 0.0f, 0.0f, 0.0f, (float) enhUnit);
        face.setArray ("uWear", hwk::shaders::wearUniforms ((float) enhUnit).data(), 15);
        face.set ("uParams", -faceHalfW, -faceHalfH, 2.0f * faceHalfW, 2.0f * faceHalfH);
        draw (meshes.faceTop, panel, zero);
        draw (meshes.faceEdges, panel, zero);

        // --- ENH Master's body, then the case it all sits in ----------------------------------
        use (shaders::chassis);
        draw (meshes.enhBody, panel, colours::chassisBlack);
        use (shaders::chrome).set ("uParams", 0.42f, 0.0f, 0.0f, 0.0f);
        draw (meshes.bodyScrews, panel * Mat4::translation ({ 0.0f, 0.0f, 0.012f - faceHalfH }), { 0.42f, 0.42f, 0.44f });
        (void) ventGlow;

        // The case: walnut cheeks (grain along the arc), crown and plinth (grain across), a dark-stained
        // back board, brass corners, turned feet; the steel mounting rails with their square holes
        const Vec3 walnutTone { 0.34f, 0.20f, 0.11f };
        auto& wood = use (shaders::wood);
        wood.set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.caseCheeks, I, walnutTone);
        draw (meshes.caseRails, I, walnutTone * 0.30f);
        wood.set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.caseBoards, I, walnutTone * 0.92f);
        use (shaders::chrome).set ("uParams", 0.12f, 1.0f, 0.0f, 0.0f);   // brass, brushed satin: it lights like metal, not a mirror of the dark room
        draw (meshes.caseBrass, I, { 1.00f, 0.76f, 0.38f });
        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.caseFeet, I, { 0.035f, 0.033f, 0.032f });
        use (shaders::chrome).set ("uParams", 0.22f, 0.0f, 0.0f, 0.0f);   // zinc-plated steel, satin
        draw (meshes.caseFrontRails, I, { 0.52f, 0.53f, 0.55f });
        use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.caseRailHoles, I, { 0.010f, 0.010f, 0.012f });

        use (shaders::table);
        draw (meshes.table, I, zero);

        // The wall behind, last of the opaque surfaces: only the pixels the rack and the floor leave are shaded
        wallTex.bind (0);
        use (shaders::studioWall).set ("uParams", studiowall::x0, geo::floorHeight(), studiowall::width, studiowall::height);
        draw (meshes.wall, I, zero);

        // =============================================================================
        // Blended: printed knob scales, then soft shadows
        // =============================================================================
        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);

        scaleWideTex.bind (1);
        auto& print = use (shaders::print);
        print.set ("uTex2", 1);
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind != ControlKind::knob || c.unit != enhUnit)
                continue;

            // NORM prints the 0-30 scale in white, ADD the 0-10 scale in amber; masters wear 0-3x / 0-5
            const bool swappable = altParam[(size_t) i] >= 0;
            const int scale = ringScale[(size_t) i];
            (scale == 3 ? scale3Tex : scale == 5 ? scale5Tex : scaleTex).bind (0);
            print.set ("uParams", scaleOuter, 0.0f, swappable ? 1.0f - modeBlend : 0.0f, swappable ? modeBlend : 0.0f);
            draw (meshes.scaleRing, panel * Mat4::translation ({ c.x, 0.0015f, c.z }) * Mat4::scale (c.size, 1.0f, c.size), zero);
        }
        scaleTex.bind (0);

        // Value arcs: a thin lit arc from zero to the pointer while a knob is hovered or moving
        {
            auto& arc = use (shaders::valueArc);
            for (int i = 0; i < numControls; ++i)
            {
                const auto& c = controls[(size_t) i];
                const int modelIndex = knobModelIndex[(size_t) i][1];
                if (c.kind != ControlKind::knob || modelIndex < 0)
                    continue;

                const auto& k = knobs[(size_t) i];
                const float show = std::max (k.hover, k.activity);
                if (show < 0.01f)
                    continue;

                const float r = knobModels[(size_t) modelIndex]->footprint * 1.04f;
                const auto& unitPanel = panelFor (c.unit);
                const Vec3 arcColour { 0.86f, 0.83f, 0.76f };   // faint, neutral: a hint of where the value sits
                arc.set ("uParams", knobAngleForValue (0.0f), k.angle, 0.40f * show, 0.22f);
                draw (meshes.arcRing, unitPanel * Mat4::translation ({ c.x, 0.0025f, c.z }) * Mat4::scale (r, 1.0f, r), arcColour);
            }
        }

        // Where each faceplate's ears press on the rails: a contact shadow along both edges
        for (int u = 0; u < numUnits; ++u)
            for (float side : { -1.0f, 1.0f })
                drawShadow (panelFor (u), side * (faceHalfW + 0.004f) - L.x * 0.02f, -railFront + 0.0012f, 0.0f,
                            0.012f, unitHalfH (u) - 0.01f, 0.01f, 0.03f, 0.55f);

        // Each unit lays a soft shadow on the panel of the one below it, inside the case
        for (int u = 0; u < numUnits; ++u)
        {
            const auto shadowSpace = panelFor (u);
            drawShadow (shadowSpace, -L.x * 0.06f, 0.0015f, -unitHalfH (u) - rackGap * 0.35f,
                        faceHalfW * 0.98f, rackGap * 0.55f, 0.05f, rackGap * 0.7f, 0.55f);
        }

        const float offX = -L.x, offZ = L.y;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const auto& unitPanel = panelFor (c.unit);

            if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
            {
                const int modelIndex = knobModelIndex[(size_t) i][1];
                const auto* model = modelIndex >= 0 ? knobModels[(size_t) modelIndex].get() : nullptr;
                const float r = model != nullptr ? model->shadowRadius : knobFlange;
                const float sx = c.x + offX * 0.05f, sz = c.z + offZ * 0.05f;
                drawShadow (unitPanel, sx, 0.003f, sz, r, r, r, 0.035f, 0.6f);

                // Pointer knobs (chicken head): the beak casts its own shadow, turning with the knob
                if (model != nullptr && model->beakLength > 0.0f)
                {
                    const float half = 0.5f * (model->beakLength + r * 0.4f);
                    const auto space = unitPanel * Mat4::translation ({ sx, 0.0f, sz })
                                                 * Mat4::rotationY (-knobs[(size_t) i].angle);
                    drawShadow (space, 0.0f, 0.003f, -half + r * 0.35f, model->beakHalfWidth, half,
                                model->beakHalfWidth, 0.030f, 0.55f);
                }
            }
            else if (c.kind == ControlKind::toggle)
                drawShadow (unitPanel, c.x + offX * 0.012f, 0.003f, c.z + offZ * 0.012f, switchOutline (c.switchStyle).halfW + 0.004f,
                            std::min (switchOutline (c.switchStyle).halfD, 0.10f) + 0.004f, 0.014f, 0.014f, 0.45f);
            else
                drawShadow (unitPanel, c.x + offX * 0.015f, 0.003f, c.z + offZ * 0.015f, buttonHalfW + 0.014f, buttonHalfD + 0.014f, 0.016f, 0.015f, 0.5f);
        }

        // Cover glass over the meters, blended over what is behind it
        {
            glEnable (GL_BLEND);
            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask (GL_FALSE);
            drawVuGlass (tideUnit, tidePanel);
            drawVuGlass (lumenUnit, lumenPanel);
            drawVuGlass (limiterUnit, limiterPanel);
            drawVuGlass (deepUnit, deepPanel);
            drawVuGlass (characterUnit, characterPanel);
            drawVuGlass (radarUnit, radarPanel);
            drawVuGlass (levelUnit, levelPanel);
            drawVuGlass (monitorUnit, monitorPanel);
            glDepthMask (GL_TRUE);
        }

        drawOutlines (cam, vw);

        // (The window's shafts in the air used to be a full-screen pass here, ~1.8 ms a frame on an integrated
        // GPU; their haze is baked into the wall now - StudioWall.h - and the hardware carries the light itself.)

        // LED / lamp halos, additive, on top of everything. Blending is switched on here, not inherited:
        // the loupe's pass has no window light before this, and the outlines leave blending off, so the
        // halos were drawn opaque there - every lit LED a solid square of its colour under the lens.
        // They never write depth: they all lie on one plane over the panel, and where two overlapped, the
        // second failed the depth test against the first in strips (dark bands and hard edges through a
        // lit ladder, moving as the LEDs changed). Solid parts in front still hide them.
        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE);
        auto& halo = use (shaders::glow);
        for (int g = 0; g < glowCount; ++g)
        {
            const auto& item = glows[(size_t) g];
            halo.set ("uParams", item.intensity, 3.2f, 0.0f, 0.0f);
            draw (meshes.quad, item.model, item.colour);
        }
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDepthMask (GL_TRUE);
        glDisable (GL_BLEND);
    }
}
