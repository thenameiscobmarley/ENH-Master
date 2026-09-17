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
        constexpr Vec3 chassisPink { 0.95f, 0.52f, 0.68f };
        constexpr Vec3 pink        { 1.00f, 0.12f, 0.46f };
        constexpr Vec3 lime        { 0.45f, 1.00f, 0.15f };
        constexpr Vec3 user        { 1.00f, 0.70f, 0.20f };
        constexpr Vec3 automation  { 0.20f, 0.82f, 1.00f };
        constexpr Vec3 selfTune    { 0.68f, 0.42f, 1.00f };
        constexpr Vec3 neutral     { 0.95f, 0.88f, 0.92f };
        constexpr Vec3 bakelite    { 0.035f, 0.032f, 0.038f };
        constexpr Vec3 chrome      { 0.92f, 0.90f, 0.93f };
    }

    static Vec3 mixVec (Vec3 a, Vec3 b, float t) noexcept { return a + (b - a) * t; }

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
                                        const UIConfig& c, artwork::RawTexture decal, artwork::RawTexture dial)
        : bridge (b), shared (s), meters (m), config (c), decalData (std::move (decal)), dialData (std::move (dial))
    {
        for (int i = 0; i < numControls; ++i)
            controlParam[(size_t) i] = bridge.indexOf (controls[(size_t) i].paramId);
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
            const auto fragment = juce::String ("#version 150\n#define MATERIAL ") + juce::String (m) + "\n" + shaders::fragmentBody;
            juce::String error;

            if (! programs[(size_t) m].build (shaders::vertex, fragment.toRawUTF8(), error))
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
        meshes.knobBezel.upload (geo::knobBezel());
        meshes.knobSkirt.upload (geo::knobSkirt());
        meshes.knobCap.upload (geo::knobCap());
        meshes.knobInsert.upload (geo::knobCapInsert());
        meshes.indicator.upload (geo::box ({ -0.009f, 0.001f, -indicatorFar }, { 0.009f, 0.006f, -indicatorNear }));
        meshes.switchPlate.upload (geo::switchPlate());
        meshes.switchBushing.upload (geo::switchBushing());
        meshes.switchLever.upload (geo::switchLever());
        meshes.led.upload (geo::dome (1.0f, 0.55f, 12, 3));

        decalTex.upload (decalData.pixels.data(), decalData.width, decalData.height, decalData.channels, true, config.anisotropy);
        dialTex.upload (dialData.pixels.data(), dialData.width, dialData.height, dialData.channels, true, config.anisotropy);

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
        dialTex.release();
        overlayTex.release();

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

        if (pressed && pointerInside)
        {
            const auto cam = CameraRig::build ((float) w / (float) h, parallaxX, parallaxY);
            const int hit = pickControl (cam, pointerNdcX, pointerNdcY);

            if (hit >= 0)
            {
                shared.renderPressMs = juce::Time::getMillisecondCounterHiRes();
                const int p = controlParam[(size_t) hit];

                if (controls[(size_t) hit].kind == ControlKind::toggle)
                {
                    bridge.setValueWithSource (p, bridge.getNormalised (p) > 0.5f ? 0.0f : 1.0f, ControlSource::user);
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

        if (released && dragParam >= 0)
        {
            dragControl = dragParam = -1;
            shared.activeControl = -1;
            shared.dragging = false;
            shared.renderDragParam = -1;
        }

        if (dragParam < 0 && pointerInside)
        {
            const auto cam = CameraRig::build ((float) w / (float) h, parallaxX, parallaxY);
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
            const int p = controlParam[(size_t) i];
            const float value = bridge.getNormalised (p);

            if (c.kind == ControlKind::toggle)
            {
                auto& sw = switches[(size_t) i];
                sw.update (value > 0.5f, -switchAngle, switchAngle, dt);
                switchGlow[(size_t) i] = anim::approach (switchGlow[(size_t) i], value > 0.5f ? 1.0f : 0.0f, 10.0f, dt);
                busy = busy || ! sw.isIdle();
                continue;
            }

            auto& k = knobs[(size_t) i];
            const auto counter = bridge.getChangeCounter (p);
            bool changed = false;

            if (k.boundParam != p)
            {
                k.boundParam = p;
                k.lastCounter = counter;
                k.source = bridge.getLastSource (p);
            }
            else if (counter != k.lastCounter)
            {
                k.lastCounter = counter;
                changed = true;
            }

            const float target = knobAngleForValue (value);
            const bool isHovered = (hovered == i || active == i);
            k.update (target, changed, bridge.getLastSource (p), isHovered, dt);
            busy = busy || ! k.isIdle (target, isHovered);
        }

        // Live DSP meters (smoothed for display only)
        const float meterK = 1.0f - std::exp (-dt / 0.06f);
        for (int b = 0; b < enh::dsp::numBands; ++b)
            displayBands[(size_t) b] += (meters.bandGainDb[(size_t) b].load (std::memory_order_relaxed) - displayBands[(size_t) b]) * meterK;

        stepFlash = std::max (meters.footstepConfidence.load (std::memory_order_relaxed), stepFlash * std::exp (-dt / 0.25f));
        activityGlow = anim::approach (activityGlow, meters.enhancement.load (std::memory_order_relaxed), 6.0f, dt);

        // Parallax: subtle, and fast enough that it never feels like it is chasing the pointer
        const bool parallaxOn = ! config.reduceMotion && pointerInside;
        auto settle = [&busy, dt] (float& v, float target, float rate)
        {
            v = anim::approach (v, target, rate, dt);
            if (std::abs (v - target) > 1.0e-3f)
                busy = true;
        };

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

        const auto camera = CameraRig::build ((float) logicalW / (float) logicalH, parallaxX, parallaxY);
        drawScene (camera, w, h);

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

        const float footOn = switchGlow[4];
        const float breath = 0.90f + 0.10f * std::sin (t * 1.6f);

        // Glyph + vent glow driven by the real DSP: detected footsteps flash lime,
        // enhancement activity glows pink.
        const Vec3 glyph = colours::lime * (footOn * (0.30f + 0.70f * stepFlash));
        const Vec3 ventGlow = colours::pink * ((0.12f + 0.75f * activityGlow) * breath) + colours::lime * (stepFlash * 0.9f);

        float sheenPhase = 0.0f, sheenStrength = 0.0f;
        if (! config.reduceMotion)
        {
            const float cycle = std::fmod (t, 12.0f) / 7.0f;
            const float e = juce::jlimit (0.0f, 1.0f, cycle);
            sheenPhase = -3.2f + 6.4f * (e * e * (3.0f - 2.0f * e));
            sheenStrength = cycle <= 1.0f ? (0.14f + 0.05f * std::sin (t * 0.9f)) * std::sin (e * pi) : 0.0f;
        }

        // =============================================================================
        // Opaque, front to back
        // =============================================================================
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const auto base = panel * Mat4::translation ({ c.x, 0.0f, c.z });

            if (c.kind == ControlKind::knob)
            {
                const auto& k = knobs[(size_t) i];
                const auto scaled = base * Mat4::scale (c.scale);
                const auto spin = base * Mat4::rotationY (-k.angle) * Mat4::scale (c.scale);

                use (shaders::plastic).set ("uParams", 36.0f, capTop - 0.03f, 0.0f, 0.0f);
                draw (meshes.knobCap, spin, colours::bakelite, Vec3 { 0.05f, 0.03f, 0.045f } * k.hover);

                use (shaders::chrome).set ("uParams", 0.55f, 1.0f, 0.0f, 0.0f);
                draw (meshes.knobInsert, spin, { 0.82f, 0.80f, 0.84f });

                dialTex.bind (0);
                use (shaders::skirt).set ("uParams", dialRadius, dialTop, 0.0f, 0.0f);
                draw (meshes.knobSkirt, spin, colours::bakelite, Vec3 { 0.03f, 0.02f, 0.03f } * k.hover);

                use (shaders::chrome).set ("uParams", 0.35f, 0.0f, 0.0f, 0.0f);
                draw (meshes.knobBezel, scaled, { 0.55f, 0.53f, 0.57f });

                // Fixed indicator: tinted by who moved the knob, bright while it moves
                const Vec3 src = sourceColour (k.source);
                const Vec3 lit = mixVec (mixVec ({ 0.10f, 0.09f, 0.10f }, src, 0.25f), src * 1.6f, k.activity);
                use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                draw (meshes.indicator, scaled, lit * 0.6f, lit * 0.6f);
            }
            else
            {
                const auto& sw = switches[(size_t) i];
                const bool hovered = shared.hoveredControl.load() == i;
                const bool isFootstep = i == 4;
                const float glow = switchGlow[(size_t) i];

                use (shaders::chrome).set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
                draw (meshes.switchLever, base * Mat4::translation ({ 0.0f, switchPivotY, 0.0f }) * Mat4::rotationX (sw.angle),
                      hovered ? Vec3 { 1.0f, 0.97f, 1.0f } : colours::chrome);
                draw (meshes.switchBushing, base, colours::chrome);
                use (shaders::chrome).set ("uParams", 0.8f, 0.0f, 0.0f, 0.0f);
                draw (meshes.switchPlate, base, { 0.86f, 0.85f, 0.88f });

                const Vec3 ledColour = isFootstep ? colours::lime : colours::pink;
                const float flash = isFootstep ? glow * (0.55f + 0.45f * stepFlash) : glow;
                use (shaders::emissive).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
                draw (meshes.led, panel * Mat4::translation ({ c.x, 0.0f, c.z + switchLedOffset }) * Mat4::scale (0.034f),
                      mixVec ({ 0.07f, 0.06f, 0.07f }, ledColour * 0.45f, glow), ledColour * flash);
            }
        }

        // --- screws, display ----------------------------------------------------------------
        use (shaders::chrome).set ("uParams", 0.5f, 0.0f, 0.0f, 0.0f);
        draw (meshes.screws, panel, { 0.75f, 0.74f, 0.78f });
        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.displayBezel, panel, { 0.05f, 0.046f, 0.055f });

        overlayTex.bind (0);
        auto& display = use (shaders::display);
        display.set ("uParams", 0.0f, 1.0f, displayRect.hw / displayRect.hd, footOn * stepFlash);
        display.setArray ("uBands", displayBands.data(), enh::dsp::numBands);
        draw (meshes.displayGlass, panel, zero);

        auto& recess = use (shaders::recess);
        recess.set ("uParams", displayDepth, 0.0f, 0.0f, 0.0f);
        recess.set ("uGlow", zero);
        draw (meshes.displayWalls, panel, { 0.08f, 0.08f, 0.09f });
        recess.set ("uParams", faceThick, 0.0f, 0.0f, 0.0f);
        draw (meshes.earWalls, panel, { 0.12f, 0.10f, 0.11f });

        auto& glow = use (shaders::emissive);
        glow.set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.earFloors, panel, { 0.012f, 0.01f, 0.012f });

        // --- faceplate ------------------------------------------------------------------------
        decalTex.bind (0);
        auto& face = use (shaders::faceplate);
        face.set ("uParams", -faceHalfW, -faceHalfH, 2.0f * faceHalfW, 2.0f * faceHalfH);
        face.set ("uParams2", sheenPhase, sheenStrength, 0.0f, 0.0f);
        face.set ("uGlyphL", glyph);
        face.set ("uGlyphR", glyph);
        draw (meshes.faceTop, panel, zero);
        draw (meshes.faceEdges, panel, zero);

        // --- chassis + lid vents, feet, table ------------------------------------------------------
        use (shaders::chassis);
        draw (meshes.chassis, I, colours::chassisPink);
        draw (meshes.lidTop, lid, colours::chassisPink);

        auto& ventRecess = use (shaders::recess);
        ventRecess.set ("uParams", lidVentDepth, 0.0f, 0.0f, 0.0f);
        ventRecess.set ("uGlow", ventGlow);
        draw (meshes.lidVentWalls, lid, { 0.10f, 0.07f, 0.08f });

        auto& ventFloor = use (shaders::emissive);
        ventFloor.set ("uParams", 1.0f, 0.0f, 0.0f, 0.0f);
        ventFloor.set ("uGlow", ventGlow);
        draw (meshes.lidVentFloors, lid, { 0.01f, 0.006f, 0.008f });

        use (shaders::plastic).set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f);
        draw (meshes.feet, I, { 0.03f, 0.028f, 0.03f });
        use (shaders::table);
        draw (meshes.table, I, zero);

        // =============================================================================
        // Blended soft shadows
        // =============================================================================
        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);

        const float chassisCz = chassisFrontZ - 0.5f * chassisDepth;
        drawShadow (I, -L.x * 0.35f, 0.002f, chassisCz - L.z * 0.35f, chassisHalfW + 0.10f, 0.5f * chassisDepth + 0.12f, 0.25f, 0.40f, 0.75f);
        drawShadow (I, 0.0f, 0.003f, frontZ - 0.02f, faceHalfW + 0.02f, 0.07f, 0.05f, 0.08f, 0.55f);

        const float offX = -L.x, offZ = L.y;
        for (auto& c : controls)
        {
            if (c.kind == ControlKind::knob)
            {
                const float r = skirtRadius * c.scale;
                drawShadow (panel, c.x + offX * 0.10f, 0.004f, c.z + offZ * 0.10f, r, r, r, 0.07f, 0.55f);
            }
            else
            {
                drawShadow (panel, c.x + offX * 0.03f, 0.004f, c.z + offZ * 0.03f, switchPlateHalfW, switchPlateHalfD, 0.035f, 0.03f, 0.4f);
            }
        }

        glDepthMask (GL_TRUE);
        glDisable (GL_BLEND);
    }
}
