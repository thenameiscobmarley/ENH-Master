#include "HardwareRenderer.h"
#include "GeometryFactory.h"
#include "Picking.h"

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
        constexpr Vec3 seraphPurple { 0.30f, 0.14f, 0.55f };
        constexpr Vec3 user        { 1.00f, 0.70f, 0.20f };
        constexpr Vec3 automation  { 0.20f, 0.82f, 1.00f };
        constexpr Vec3 selfTune    { 0.68f, 0.42f, 1.00f };
        constexpr Vec3 neutral     { 0.92f, 0.93f, 0.95f };
        constexpr Vec3 chrome      { 0.92f, 0.90f, 0.93f };
    }

    static Vec3 mixVec (Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }
    static float saturateUi (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }

    Vec3 HardwareRenderer::sourceColour (ControlSource s) noexcept
    {
        switch (s)
        {
            case ControlSource::user:           return colours::user;
            case ControlSource::hostAutomation: return colours::automation;
            case ControlSource::selfTune:       return colours::selfTune;
            case ControlSource::none:           break;
        }
        return colours::neutral;
    }

    //==============================================================================
    HardwareRenderer::HardwareRenderer (ParameterBridge& b, SharedUIState& s, const enh::dsp::EngineMeters& m,
                                        const UIConfig& c, artwork::TextureSet textures)
        : bridge (b), shared (s), meters (m), config (c), textureData (std::move (textures))
    {
        seraphModeParam = bridge.indexOf (params::id::seraphMode);

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
                                      Vec3 pointerColour, float accentGain)
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
                    use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                    draw (part->mesh, m, pointerColour * 0.55f, pointerColour * 0.45f);
                    break;
            }
        }
    }

    void HardwareRenderer::queueGlow (const Mat4& space, float x, float z, float size, Vec3 colour, float intensity) noexcept
    {
        if (intensity < 0.02f || glowCount >= (int) glows.size())
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

        meshes.table.upload (geo::tablePlane());
        meshes.quad.upload (geo::unitQuad());
        meshes.chassis.upload (geo::chassisBody());
        meshes.lidTop.upload (geo::lidTop());
        meshes.lidVentWalls.upload (geo::lidVentWalls());
        meshes.lidVentFloors.upload (geo::lidVentFloors());
        meshes.feet.upload (geo::feet());
        meshes.faceEdges.upload (geo::faceplateEdges());
        meshes.faceTop.upload (geo::faceplateTop());
        meshes.displayWalls.upload (geo::displayWalls());
        meshes.displayGlass.upload (geo::displayGlass());
        meshes.displayBezel.upload (geo::displayBezel());
        meshes.earWalls.upload (geo::earSlotWalls());
        meshes.earFloors.upload (geo::earSlotFloors());
        meshes.screws.upload (geo::screwHeads());
        meshes.screwSlots.upload (geo::screwSlots());
        meshes.lidScrews.upload (geo::lidScrews());
        meshes.scaleRing.upload (geo::knobScaleRing());
        meshes.arcRing.upload (hwk::geo::flatAnnulus (1.0f, 1.16f, 96));
        meshes.tubeChassis.upload (geo::tubeChassisBody());
        meshes.tubeFaceTop.upload (geo::tubeFaceTop());
        meshes.tubeFaceEdges.upload (geo::tubeFaceEdges());
        meshes.tubeEarWalls.upload (geo::tubeEarSlotWalls());
        meshes.tubeEarFloors.upload (geo::tubeEarSlotFloors());
        meshes.tubeScrews.upload (geo::tubeScrewHeads());
        meshes.tubeScrewSlots.upload (geo::tubeScrewSlots());
        meshes.seraphWalls.upload (geo::seraphDisplayWalls());
        meshes.seraphGlass.upload (geo::seraphDisplayGlass());
        meshes.seraphBezel.upload (geo::seraphDisplayBezel());

        meshes.tideFaceTop.upload (geo::oneUFaceTop (tideUnit));
        meshes.lumenFaceTop.upload (geo::oneUFaceTop (lumenUnit));
        meshes.oneUFaceEdges.upload (geo::oneUFaceEdges());
        meshes.oneUEarWalls.upload (geo::oneUEarWalls());
        meshes.oneUEarFloors.upload (geo::oneUEarFloors());
        meshes.oneUScrews.upload (geo::oneUScrewHeads());
        meshes.oneUScrewSlots.upload (geo::oneUScrewSlots());
        meshes.tideChassis.upload (geo::oneUChassis (tideCenterY, tideHalfH));
        meshes.lumenChassis.upload (geo::oneUChassis (lumenCenterY, lumenHalfH));

        // VU movements, one model per size (TIDE's wide meter, LUMEN's three narrow ones)
        tideVu.upload (hwk::models::vuMeter (tideVuHalfW, vuHalfH, vuDepth, { 0.09f, 0.26f, 0.42f }));
        lumenVu.upload (hwk::models::vuMeter (lumenVuHalfW, vuHalfH, vuDepth, { 0.14f, 0.13f, 0.12f }));

        meshes.rackRails.upload (geo::rackRails());
        meshes.rackHoleWalls.upload (geo::rackHoleWalls());
        meshes.rackHoleFloors.upload (geo::rackHoleFloors());
        meshes.rackShell.upload (geo::rackShell());
        meshes.rackEdges.upload (geo::rackEdges());
        // HardwareKit models: one GPU model per distinct (style, radius)
        knobModels.clear();
        std::vector<std::pair<int, float>> built;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            knobModelIndex[(size_t) i] = -1;
            if (c.kind != ControlKind::knob && c.kind != ControlKind::selector)
                continue;

            const float r = knobBodyRadius (c);
            const auto accent = c.unit == tubeUnit  ? Vec3 { 0.62f, 0.44f, 1.0f }
                              : c.unit == tideUnit  ? Vec3 { 0.20f, 0.80f, 0.95f }
                              : c.unit == lumenUnit ? Vec3 { 1.00f, 0.72f, 0.22f }
                                                    : Vec3 { 0.55f, 0.56f, 0.60f };
            const auto key = std::make_pair ((int) c.style, r);
            const auto found = std::find (built.begin(), built.end(), key);
            if (found != built.end())
            {
                knobModelIndex[(size_t) i] = (int) (found - built.begin());
                continue;
            }
            auto model = std::make_unique<GpuModel>();
            model->upload (hwk::models::knob (c.style, r, accent));
            knobModelIndex[(size_t) i] = (int) knobModels.size();
            knobModels.push_back (std::move (model));
            built.push_back (key);
        }
        buttonModel.upload (hwk::models::pushButton (buttonHalfW, buttonHalfD));
        toggleBaseModel.upload (hwk::models::batToggleBase());
        toggleLeverModel.upload (hwk::models::batToggleLever());
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

        const std::vector<juce::uint8> blank ((size_t) (artwork::displayOverlayWidth * artwork::displayOverlayHeight), 0);
        overlayTex.upload (blank.data(), artwork::displayOverlayWidth, artwork::displayOverlayHeight, 1, true, 1);
        uploadedOverlayVersion = 0;

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
        buttonModel.release();
        toggleBaseModel.release();
        toggleLeverModel.release();
        lampModel.release();
        tideVu.release();
        lumenVu.release();
        loupeTarget.release();
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
        const float maxRadius = 62.0f * px;
        loupeRadius = maxRadius * (0.55f + 0.45f * loupeAlpha);   // grows out of the panel, shrinks back into it
        const float margin = loupeRadius + 6.0f * px;
        loupeCx = juce::jlimit (margin, std::max (margin, (float) vw - margin), (ax + 1.0f) * 0.5f * (float) vw);
        loupeCy = juce::jlimit (margin, std::max (margin, (float) vh - margin), (ay + 1.0f) * 0.5f * (float) vh);

        // Fixed target size (only the window size changes it), so opening the loupe never reallocates
        const int size = juce::jmax (8, juce::roundToInt (2.0f * maxRadius));
        if (! loupeTarget.ensureSize (size, size))
            return;

        // Re-render the scene zoomed in around the anchor: real detail, not stretched pixels
        auto zoomed = camera;
        zoomed.viewProj = hwk::fx::Loupe::zoomedViewProj (camera.viewProj, ax, ay, loupeZoom, maxRadius, vw, vh);

        loupeTarget.bind();
        vignette = 0.0f;
        drawScene (zoomed, size, size);
        vignette = 1.0f;
        gfx::RenderTarget::unbind();
        glViewport (0, 0, vw, vh);
        loupeReady = true;
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

        // A press only counts when our window is really the one under the pointer
        if (pressed && pointerInside && pointer.isTopmostUnderPointer ((unsigned long) shared.nativeWindow.load()))
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

        if (dragParam < 0 && pointerInside)
        {
            const auto cam = CameraRig::build ((float) w / (float) h, parallaxX, parallaxY, { shared.focusUnit.load(), focusAmount });
            shared.hoveredControl = pickControl (cam, pointerNdcX, pointerNdcY);
        }
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
                tg.update (value > 0.5f, toggleAngle, -toggleAngle, dt);
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

            const float target = c.kind == ControlKind::selector ? selectorAngleForValue (value) : knobAngleForValue (value);
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

        // TIDE and LUMEN: the meter movements, and the backlight behind their dials
        {
            const bool tideOn = bridge.getNormalised (bridge.indexOf (params::id::tideActive)) > 0.5f;
            const bool lumenOn = bridge.getNormalised (bridge.indexOf (params::id::lumenActive)) > 0.5f;
            oneULamp[0] = anim::approach (oneULamp[0], tideOn ? 1.0f : 0.15f, 4.0f, dt);
            oneULamp[1] = anim::approach (oneULamp[1], lumenOn ? 1.0f : 0.15f, 4.0f, dt);

            // TIDE reads gain reduction (0..12 dB), LUMEN reads the lift in each band (0..18 dB)
            const float readings[4] {
                saturateUi (meters.tideGrDb.load() / 12.0f),
                saturateUi (meters.lumenGainDb[0].load() / 18.0f),
                saturateUi (meters.lumenGainDb[1].load() / 18.0f),
                saturateUi (meters.lumenGainDb[2].load() / 18.0f),
            };

            for (int i = 0; i < 4; ++i)
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

        stepFlash = std::max (meters.footstepConfidence.load (std::memory_order_relaxed), stepFlash * std::exp (-dt / 0.25f));
        activityGlow = anim::approach (activityGlow, meters.enhancement.load (std::memory_order_relaxed), 6.0f, dt);

        // LED ladders: instant on, short fade off (like real LED meters with a little lag)
        const float outDb = meters.outputPeakDb.load (std::memory_order_relaxed);
        auto ladder = [dt] (std::array<float, ladderSegments>& leds, int segments, auto&& lit)
        {
            for (int k = 0; k < segments; ++k)
            {
                const float target = lit (k) ? 1.0f : 0.0f;
                auto& v = leds[(size_t) k];
                v = target > v ? target : anim::approach (v, target, 14.0f, dt);
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
        uploadOverlayIfChanged();
        uploadCalloutIfChanged();

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
        loupeZoom = anim::approach (loupeZoom, loupeWanted ? 2.2f : 1.0f, loupeWanted ? 10.0f : 15.0f, dt);
        if (loupeAlpha > 0.01f)
            renderLoupeView (camera, w, h);

        drawScene (camera, w, h);
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
        moving-coil meters behind glass. Both units are the same build, different print. */
    void HardwareRenderer::drawOneU (int unit, const Mat4& panel, Vec3 colour, const gfx::Texture2D& decal,
                                     const gfx::Texture2D& faceTex, const gfx::GpuMesh& faceTop)
    {
        const bool tide = unit == tideUnit;
        const float lamp = oneULamp[(size_t) (tide ? 0 : 1)];
        auto& model = tide ? tideVu : lumenVu;

        // The meters: case, printed face, needle, hub, bezel (the glass comes later, blended)
        for (int i = 0; i < numVus (unit); ++i)
        {
            const auto at = panel * Mat4::translation ({ vuX (unit, i), 0.0f, vuCentreZ });
            const auto& needle = needles[(size_t) (tide ? 0 : 1 + i)];
            // The movement is hinged below the window: turn about that hinge, not the centre
            const auto swing = at * Mat4::translation ({ 0.0f, 0.0f, model.pivotOffset })
                                  * Mat4::rotationY (-needle.angle)
                                  * Mat4::translation ({ 0.0f, 0.0f, -model.pivotOffset });

            for (auto& part : model.parts)
            {
                using hwk::models::Role;
                const Mat4& m = part->rotates ? swing : at;

                switch (part->role)
                {
                    case Role::screen:
                        faceTex.bind (0);
                        use (shaders::vuFace).set ("uParams", 0.0f, lamp, 0.0f, (float) i);
                        draw (part->mesh, m, part->colour);
                        break;
                    case Role::metal:
                        use (shaders::chrome).set ("uParams", part->polish, part->brushed ? 1.0f : 0.0f, 0.0f, 0.0f);
                        draw (part->mesh, m, part->colour);
                        break;
                    case Role::pointer:
                        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                        draw (part->mesh, m, part->colour);
                        break;
                    case Role::glass:
                        break;                       // drawn after everything else
                    case Role::body:
                    case Role::accent:
                        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                        draw (part->mesh, m, part->colour);
                        break;
                }
            }
        }

        auto& recessed = use (shaders::recess);
        recessed.set ("uParams", faceThick, 0.0f, 0.0f, 0.0f);
        recessed.set ("uGlow", Vec3 {});
        draw (meshes.oneUEarWalls, panel, { 0.09f, 0.09f, 0.10f });

        use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
        draw (meshes.oneUScrews, panel, { 0.75f, 0.74f, 0.78f });

        use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.oneUEarFloors, panel, { 0.012f, 0.012f, 0.014f });

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.oneUScrewSlots, panel, { 0.02f, 0.02f, 0.025f });

        // Brushed faceplate with the print engraved into it
        decal.bind (0);
        auto& plate = use (shaders::brushed);
        plate.set ("uParams", -faceHalfW, -oneUHalfH, 2.0f * faceHalfW, 2.0f * oneUHalfH);
        draw (faceTop, panel, colour);
        draw (meshes.oneUFaceEdges, panel, colour * 0.82f);

        use (shaders::chassis);
        draw (tide ? meshes.tideChassis : meshes.lumenChassis, Mat4::identity(), colours::chassisBlack);
    }

    /** The cover glass over a unit's meters, drawn with everything else transparent. */
    void HardwareRenderer::drawVuGlass (int unit, const Mat4& panel)
    {
        auto& model = unit == tideUnit ? tideVu : lumenVu;

        for (int i = 0; i < numVus (unit); ++i)
        {
            const auto at = panel * Mat4::translation ({ vuX (unit, i), 0.0f, vuCentreZ });
            for (auto& part : model.parts)
                if (part->role == hwk::models::Role::glass)
                {
                    use (shaders::vuGlass);
                    draw (part->mesh, at, part->colour);
                }
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
        const auto panelFor = [&] (int unit) -> const Mat4&
        {
            return unit == tubeUnit ? tubePanel : unit == tideUnit ? tidePanel : unit == lumenUnit ? lumenPanel : panel;
        };
        const Mat4 tubeLid = Mat4::translation ({ 0.0f, tubeChassisTop, 0.0f });
        const Mat4 I = Mat4::identity();
        const Mat4 lid = Mat4::translation ({ 0.0f, chassisTop, 0.0f });

        glClearColor (0.03f, 0.022f, 0.026f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LEQUAL);
        glDepthMask (GL_TRUE);
        glDisable (GL_CULL_FACE);
        glDisable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float footOn = footstepControl >= 0 ? buttons[(size_t) footstepControl].led : 0.0f;
        const float breath = 0.90f + 0.10f * std::sin (t * 1.6f);
        const Vec3 ventGlow = colours::amber * ((0.05f + 0.45f * activityGlow) * breath) + colours::ledGreen * (stepFlash * footOn * 0.35f);

        glowCount = 0;

        // =============================================================================
        // Opaque, front to back
        // =============================================================================
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const auto& unitPanel = panelFor (c.unit);
            const auto base = unitPanel * Mat4::translation ({ c.x, 0.0f, c.z });
            const bool hovered = shared.hoveredControl.load() == i || shared.activeControl.load() == i;

            if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
            {
                const auto& k = knobs[(size_t) i];
                const int modelIndex = knobModelIndex[(size_t) i];
                if (modelIndex < 0)
                    continue;

                // A mode-switched knob sinks briefly while its printed scale swaps
                const float dip = altParam[(size_t) i] >= 0 ? -0.035f * std::sin (pi * swapPulse) : 0.0f;
                const auto spin = base * Mat4::translation ({ 0.0f, dip, 0.0f }) * Mat4::rotationY (-k.angle);

                // Pointer: white, tinted by who moved the knob while it moves
                const Vec3 pointer = mixVec ({ 0.94f, 0.94f, 0.96f }, sourceColour ((ControlSource) k.source), 0.55f * k.activity);
                drawModel (*knobModels[(size_t) modelIndex], spin, base, Vec3 { 0.035f, 0.033f, 0.042f } * k.hover, pointer);
            }
            else if (c.kind == ControlKind::toggle)
            {
                const auto& tg = toggles[(size_t) i];
                const Vec3 lift = hovered ? Vec3 { 0.08f, 0.08f, 0.09f } : Vec3 {};
                drawModel (toggleLeverModel, base * Mat4::translation ({ 0.0f, togglePivotY, 0.0f }) * Mat4::rotationX (tg.angle), base, lift, {});
                drawModel (toggleBaseModel, base, base, {}, {});
            }
            else
            {
                const auto& bt = buttons[(size_t) i];
                const auto pressed = base * Mat4::translation ({ 0.0f, -buttonTravel * bt.travel(), 0.0f });
                drawModel (buttonModel, pressed, base, {}, {}, hovered ? 1.12f : 1.0f);

                use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                if (i == modeControl)
                {
                    drawLed (panel, c.x - modeLedDx, c.z + buttonLedDz, colours::ledGreen, 1.0f - bt.led);
                    drawLed (panel, c.x + modeLedDx, c.z + buttonLedDz, colours::ledYellow, bt.led);
                }
                else
                {
                    const bool isFootstep = i == footstepControl;
                    const float on = isFootstep ? bt.led * (0.75f + 0.25f * stepFlash) : bt.led;
                    drawLed (panel, c.x, c.z + buttonLedDz, isFootstep ? colours::ledGreen : colours::ledYellow, on);
                }
            }
        }

        // --- TIDE and LUMEN: the two 1U units ---------------------------------------------------
        drawOneU (tideUnit, tidePanel, Vec3 { 0.62f, 0.635f, 0.66f }, tideDecalTex, tideLabelTex, meshes.tideFaceTop);
        drawOneU (lumenUnit, lumenPanel, Vec3 { 0.60f, 0.605f, 0.62f }, lumenDecalTex, lumenLabelTex, meshes.lumenFaceTop);

        // --- the rack case the units are bolted into ---------------------------------------------
        {
            use (shaders::chassis);
            draw (meshes.rackShell, I, { 0.030f, 0.030f, 0.034f });

            auto& railRecess = use (shaders::recess);
            railRecess.set ("uParams", 0.03f, 0.0f, 0.0f, 0.0f);
            railRecess.set ("uGlow", zero);
            draw (meshes.rackHoleWalls, I, { 0.055f, 0.055f, 0.060f });
            use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
            draw (meshes.rackHoleFloors, I, { 0.010f, 0.010f, 0.012f });

            use (shaders::chrome).set ("uParams", 0.30f, 1.0f, 0.0f, 0.0f);
            draw (meshes.rackRails, I, { 0.42f, 0.43f, 0.45f });
            use (shaders::chrome).set ("uParams", 0.55f, 1.0f, 0.0f, 0.0f);
            draw (meshes.rackEdges, I, { 0.58f, 0.59f, 0.62f });
        }

        // --- SERAPH: VU meters, lamp, screws, faceplate, chassis --------------------------------
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
            vuRecess.set ("uGlow", Vec3 { 0.18f, 0.10f, 0.30f } * tubePower);
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
            queueGlow (tubePanel, lampX, lampZ, 0.26f, { 1.0f, 0.12f, 0.06f }, 0.55f * lampOn);

            use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
            draw (meshes.tubeEarFloors, tubePanel, { 0.012f, 0.01f, 0.014f });

            use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
            draw (meshes.tubeScrewSlots, tubePanel, { 0.02f, 0.02f, 0.025f });

            tubeDecalTex.bind (0);
            auto& paint = use (shaders::paint);
            paint.set ("uParams", -faceHalfW, -tubeHalfH, 2.0f * faceHalfW, 2.0f * tubeHalfH);
            draw (meshes.tubeFaceTop, tubePanel, colours::seraphPurple);
            draw (meshes.tubeFaceEdges, tubePanel, colours::seraphPurple);

            use (shaders::chassis);
            draw (meshes.tubeChassis, I, colours::chassisBlack);
            draw (meshes.lidTop, tubeLid, colours::chassisBlack);

            // Tubes glowing through the lid perforation
            const Vec3 tubeGlow = colours::amber * (0.55f * tubeWarmth * (0.94f + 0.06f * breath));
            auto& tubeVents = use (shaders::recess);
            tubeVents.set ("uParams", lidVentDepth, 0.0f, 0.0f, 0.0f);
            tubeVents.set ("uGlow", tubeGlow);
            draw (meshes.lidVentWalls, tubeLid, { 0.05f, 0.05f, 0.055f });
            auto& tubeVentFloor = use (shaders::emissive);
            tubeVentFloor.set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
            tubeVentFloor.set ("uGlow", tubeGlow);
            draw (meshes.lidVentFloors, tubeLid, { 0.006f, 0.006f, 0.007f });

            use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
            draw (meshes.lidScrews, tubeLid, { 0.40f, 0.40f, 0.42f });
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
        draw (meshes.lidScrews, lid, { 0.40f, 0.40f, 0.42f });
        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.screwSlots, panel, { 0.02f, 0.02f, 0.025f });
        draw (meshes.displayBezel, panel, { 0.03f, 0.031f, 0.035f });

        overlayTex.bind (0);
        auto& display = use (shaders::display);
        display.set ("uParams", 0.0f, 1.0f, displayRect.hw / displayRect.hd, footOn * stepFlash);
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
        face.set ("uParams", -faceHalfW, -faceHalfH, 2.0f * faceHalfW, 2.0f * faceHalfH);
        draw (meshes.faceTop, panel, zero);
        draw (meshes.faceEdges, panel, zero);

        // --- chassis + lid vents, feet, table ------------------------------------------------------
        use (shaders::chassis);
        draw (meshes.chassis, I, colours::chassisBlack);
        draw (meshes.lidTop, lid, colours::chassisBlack);

        auto& ventRecess = use (shaders::recess);
        ventRecess.set ("uParams", lidVentDepth, 0.0f, 0.0f, 0.0f);
        ventRecess.set ("uGlow", ventGlow);
        draw (meshes.lidVentWalls, lid, { 0.05f, 0.05f, 0.055f });

        auto& ventFloor = use (shaders::emissive);
        ventFloor.set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
        ventFloor.set ("uGlow", ventGlow);
        draw (meshes.lidVentFloors, lid, { 0.006f, 0.006f, 0.007f });

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.feet, I, { 0.02f, 0.02f, 0.022f });
        use (shaders::table);
        draw (meshes.table, I, zero);

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
                const int modelIndex = knobModelIndex[(size_t) i];
                if (c.kind != ControlKind::knob || modelIndex < 0)
                    continue;

                const auto& k = knobs[(size_t) i];
                const float show = std::max (k.hover, k.activity);
                if (show < 0.01f)
                    continue;

                const float r = knobModels[(size_t) modelIndex]->footprint * 1.04f;
                const auto& unitPanel = panelFor (c.unit);
                const Vec3 arcColour = c.unit == tubeUnit  ? Vec3 { 0.86f, 0.74f, 1.00f }
                                     : c.unit == tideUnit  ? Vec3 { 0.40f, 0.88f, 1.00f }
                                     : c.unit == lumenUnit ? Vec3 { 1.00f, 0.80f, 0.35f }
                                                           : Vec3 { 1.00f, 0.76f, 0.32f };
                arc.set ("uParams", knobAngleForValue (0.0f), k.angle, 0.9f * show, 0.22f);
                draw (meshes.arcRing, unitPanel * Mat4::translation ({ c.x, 0.0025f, c.z }) * Mat4::scale (r, 1.0f, r), arcColour);
            }
        }

        const float chassisCz = chassisFrontZ - 0.5f * chassisDepth;
        drawShadow (I, -L.x * 0.35f, 0.002f, chassisCz - L.z * 0.35f, chassisHalfW + 0.10f, 0.5f * chassisDepth + 0.12f, 0.25f, 0.40f, 0.75f);
        drawShadow (I, 0.0f, 0.003f, frontZ - 0.02f, faceHalfW + 0.02f, 0.07f, 0.05f, 0.08f, 0.55f);

        const float offX = -L.x, offZ = L.y;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const auto& unitPanel = panelFor (c.unit);

            if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
            {
                const int modelIndex = knobModelIndex[(size_t) i];
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
                drawShadow (unitPanel, c.x + offX * 0.02f, 0.003f, c.z + offZ * 0.02f, 0.042f, 0.042f, 0.042f, 0.02f, 0.5f);
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
            glDepthMask (GL_TRUE);
        }

        // The room: shafts of window light and the dust drifting through them, over the frame
        if (vignette > 0.5f)
        {
            glDisable (GL_DEPTH_TEST);
            glEnable (GL_BLEND);
            glBlendFunc (GL_SRC_ALPHA, GL_ONE);
            auto& sun = use (shaders::sunlight);
            sun.set ("uViewProj", Mat4::identity());
            sun.set ("uParams", 1.0f, (float) vw / (float) juce::jmax (1, vh), 0.0f, 0.0f);
            draw (meshes.quad, gfx::screenQuad (vw, vh, 0.5f * (float) vw, 0.5f * (float) vh, 0.5f * (float) vw, 0.0f, 0.0f, 0.5f * (float) vh), {});
            glEnable (GL_DEPTH_TEST);
        }

        // LED / lamp halos, additive, on top of everything
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
