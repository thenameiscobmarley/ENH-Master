#include "HardwareRenderer.h"
#include "GeometryFactory.h"
#include "../Render/Shaders.h"

using namespace juce::gl;

namespace pad
{
    using gfx::Mat4;
    using gfx::Vec3;
    using namespace layout;

    namespace colours
    {
        constexpr Vec3 chassisPink  { 1.00f, 0.50f, 0.70f };
        constexpr Vec3 maskPink     { 1.00f, 0.10f, 0.45f };
        constexpr Vec3 stepLime     { 0.45f, 1.00f, 0.15f };
        constexpr Vec3 user         { 1.00f, 0.72f, 0.22f };
        constexpr Vec3 automation   { 0.20f, 0.82f, 1.00f };
        constexpr Vec3 selfTune     { 0.68f, 0.42f, 1.00f };
        constexpr Vec3 neutral      { 0.90f, 0.32f, 0.58f };
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
    HardwareRenderer::HardwareRenderer (ParameterBridge& b, SharedUIState& s, const UIConfig& c,
                                        artwork::RawTexture decal, artwork::RawTexture dial)
        : bridge (b), shared (s), config (c), decalData (std::move (decal)), dialData (std::move (dial))
    {
        for (int i = 0; i < numControls; ++i)
            fixedParam[(size_t) i] = controls[(size_t) i].paramId != nullptr ? bridge.indexOf (controls[(size_t) i].paramId) : -1;

        maskParam = bridge.indexOf (params::id::modeMasking);
        stepParam = bridge.indexOf (params::id::modeFootstep);

        for (int t = 0; t < params::numPdTargets; ++t)
        {
            kpParam[(size_t) t] = bridge.indexOf (params::pdKpId (t));
            kdParam[(size_t) t] = bridge.indexOf (params::pdKdId (t));
        }
    }

    HardwareRenderer::~HardwareRenderer()
    {
        jassert (! ready); // context must be detached before destruction
    }

    int HardwareRenderer::paramIndexForControl (int i) const noexcept
    {
        const auto& c = controls[(size_t) i];
        const auto focus = juce::jlimit (0, params::numPdTargets - 1, shared.pdFocus.load());

        switch (c.binding)
        {
            case Binding::pdKp:  return kpParam[(size_t) focus];
            case Binding::pdKd:  return kdParam[(size_t) focus];
            case Binding::fixed: break;
        }
        return fixedParam[(size_t) i];
    }

    //==============================================================================
    void HardwareRenderer::newOpenGLContextCreated()
    {
        juce::String error;
        if (! shader.build (shaders::vertex, shaders::fragment, error))
        {
            DBG ("Shader build failed: " << error);
            juce::Logger::writeToLog ("PvPAdaptiveDynamics: shader build failed: " + error);
            ready = false;
            return;
        }

        meshes.table.upload (geo::tablePlane());
        meshes.shadowQuad.upload (geo::horizontalQuad ({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.0f));
        meshes.chassis.upload (geo::chassisBody());
        meshes.ears.upload (geo::rackEars());
        meshes.feet.upload (geo::feet());
        meshes.panelEdges.upload (geo::panelEdges());
        meshes.panelTop.upload (geo::panelTop());
        meshes.screws.upload (geo::screws());
        meshes.flange.upload (geo::knobFlange());
        meshes.capBody.upload (geo::knobCapBody());
        meshes.capInsert.upload (geo::knobCapInsert());
        meshes.pointer.upload (geo::knobPointer());
        meshes.ledRing.upload (geo::flatAnnulus (flangeRadius * 1.04f, flangeRadius * 1.21f, 48));
        meshes.switchWalls.upload (geo::switchWellWalls());
        meshes.switchFloors.upload (geo::switchWellFloors());
        meshes.blade.upload (geo::switchBlade());
        meshes.hub.upload (geo::switchHub());
        meshes.scopeWalls.upload (geo::scopeWalls());
        meshes.scopeGlass.upload (geo::scopeGlass());
        meshes.ventWalls.upload (geo::ventWalls());
        meshes.ventFloors.upload (geo::ventFloors());
        meshes.led.upload (geo::dome (1.0f, 0.55f, 10, 3));

        decalTex.upload (decalData.pixels.data(), decalData.width, decalData.height, decalData.channels, true, config.anisotropy);
        dialTex.upload (dialData.pixels.data(), dialData.width, dialData.height, dialData.channels, true, config.anisotropy);

        const std::vector<juce::uint8> blank ((size_t) (artwork::scopeOverlayWidth * artwork::scopeOverlayHeight), 0);
        overlayTex.upload (blank.data(), artwork::scopeOverlayWidth, artwork::scopeOverlayHeight, 1, true, 1);
        uploadedOverlayVersion = 0;

        if (auto* ctx = juce::OpenGLContext::getCurrentContext())
            ctx->setSwapInterval (1);

        lastFrameMs = 0.0;
        ready = true;
    }

    void HardwareRenderer::openGLContextClosing()
    {
        meshes.forEach ([] (gfx::GpuMesh& m) { m.release(); });
        decalTex.release();
        dialTex.release();
        overlayTex.release();
        shader.release();
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

        if (overlayScratch.size() == (size_t) (artwork::scopeOverlayWidth * artwork::scopeOverlayHeight))
            overlayTex.upload (overlayScratch.data(), artwork::scopeOverlayWidth, artwork::scopeOverlayHeight, 1, true, 1);

        uploadedOverlayVersion = version;
    }

    void HardwareRenderer::updateAnimation (float dt)
    {
        const int hovered = shared.hoveredControl.load();
        const int active  = shared.activeControl.load();
        bool busy = false;

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            const int p = paramIndexForControl (i);
            const float value = bridge.getNormalised (p);

            if (c.kind == ControlKind::toggle)
            {
                auto& sw = switches[(size_t) i];
                sw.update (value > 0.5f, -switchAngle, switchAngle, dt);
                busy = busy || ! sw.isIdle();
                continue;
            }

            auto& k = knobs[(size_t) i];
            const auto counter = bridge.getChangeCounter (p);
            bool changed = false;

            if (k.boundParam != p)
            {
                // Rebinding (e.g. PD focus moved) animates on the same path but isn't "activity".
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

        auto settle = [&busy, dt] (float& v, float target, float rate)
        {
            v = anim::approach (v, target, rate, dt);
            if (std::abs (v - target) > 1.0e-3f)
                busy = true;
        };

        settle (maskGlow, bridge.getNormalised (maskParam) > 0.5f ? 1.0f : 0.0f, 5.0f);
        settle (stepGlow, bridge.getNormalised (stepParam) > 0.5f ? 1.0f : 0.0f, 5.0f);

        const int focus = shared.pdFocus.load();
        for (int t = 0; t < params::numPdTargets; ++t)
            settle (focusGlow[t], t == focus ? 1.0f : 0.0f, 8.0f);

        const bool parallaxOn = ! config.reduceMotion && shared.mouseInside.load();
        const float amount = config.parallaxAmount;
        settle (parallaxX, parallaxOn ? shared.mouseNdcX.load() * amount : 0.0f, 4.5f);
        settle (parallaxY, parallaxOn ? shared.mouseNdcY.load() * amount : 0.0f, 4.5f);

        shared.parallaxX = parallaxX;
        shared.parallaxY = parallaxY;
        shared.animating = busy;
    }

    //==============================================================================
    void HardwareRenderer::renderOpenGL()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = lastFrameMs > 0.0 ? (float) juce::jlimit (0.0, 0.1, (now - lastFrameMs) * 0.001) : 1.0f / 60.0f;
        lastFrameMs = now;
        timeSeconds += dt;

        auto* ctx = juce::OpenGLContext::getCurrentContext();
        const double scale = ctx != nullptr ? ctx->getRenderingScale() : 1.0;
        const int logicalW = juce::jmax (1, shared.viewWidth.load());
        const int logicalH = juce::jmax (1, shared.viewHeight.load());
        const int w = juce::roundToInt (scale * logicalW), h = juce::roundToInt (scale * logicalH);

        glViewport (0, 0, w, h);

        if (! ready)
        {
            glClearColor (0.93f, 0.46f, 0.63f, 1.0f);
            glClear (GL_COLOR_BUFFER_BIT);
            return;
        }

        updateAnimation (dt);
        uploadOverlayIfChanged();

        const auto camera = CameraRig::build ((float) logicalW / (float) logicalH, parallaxX, parallaxY);
        drawScene (camera, w, h);
    }

    void HardwareRenderer::draw (const gfx::GpuMesh& mesh, const Mat4& model, int material, Vec3 base, Vec3 emissive)
    {
        shader.set ("uModel", model);
        shader.set ("uMaterial", material);
        shader.set ("uBaseColor", base);
        shader.set ("uEmissive", emissive);
        mesh.draw();
    }

    void HardwareRenderer::drawShadow (float cx, float y, float cz, float hw, float hd, float radius, float blur, float strength)
    {
        const float pad = blur * 2.0f + 0.02f;
        shader.set ("uShadow", cx, cz, hw, hd);
        shader.set ("uShadowParams", Vec3 { radius, blur, strength });
        draw (meshes.shadowQuad, Mat4::translation ({ cx, y, cz }) * Mat4::scale (hw + pad, 1.0f, hd + pad), shaders::softShadow, {});
    }

    void HardwareRenderer::drawScene (const CameraRig& cam, int vw, int vh)
    {
        const float t = (float) timeSeconds;
        const Vec3 zero {};
        const Vec3 light = cam.lightDir;

        glClearColor (0.03f, 0.022f, 0.026f, 1.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_DEPTH_TEST);
        glDepthFunc (GL_LEQUAL);
        glDepthMask (GL_TRUE);
        glDisable (GL_CULL_FACE);
        glDisable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        shader.use();
        shader.set ("uViewProj", cam.viewProj);
        shader.set ("uCamPos", cam.eye);
        shader.set ("uLightDir", light);
        shader.set ("uTime", t);
        shader.set ("uViewport", (float) vw, (float) vh);
        shader.set ("uDecal", 0);
        shader.set ("uDial", 1);
        shader.set ("uOverlay", 2);
        decalTex.bind (0);
        dialTex.bind (1);
        overlayTex.bind (2);

        shader.set ("uPanelRect", -panelHalfW, -panelHalfD, 2.0f * panelHalfW, 2.0f * panelHalfD);
        shader.set ("uVentRect", ventBlock.cx, ventBlock.cz, ventBlock.hw, ventBlock.hd);
        shader.set ("uRing", 0.0f, 0.0f, 0.0f, flangeRadius);

        // --- state-driven glows (placeholders: tied to UI mode switches only) ---
        const float breath = 0.82f + 0.18f * std::sin (t * 1.7f);
        const Vec3 ventGlow = colours::maskPink * (maskGlow * 0.95f * breath)
                            + colours::stepLime * (stepGlow * 0.60f * breath)
                            + Vec3 { 0.035f, 0.014f, 0.022f };

        auto footPulse = [t] (float phase)
        {
            const float p = std::fmod (t * 0.9f + phase, 1.0f);
            return 0.25f + 0.75f * std::exp (-p * 6.0f);
        };

        const Vec3 glyphL = colours::stepLime * (stepGlow * footPulse (0.0f));
        const Vec3 glyphR = colours::stepLime * (stepGlow * footPulse (0.5f));

        float sheenPhase = 0.0f, sheenStrength = 0.0f;
        if (! config.reduceMotion)
        {
            const float cycle = std::fmod (t, 11.0f) / 7.0f;
            const float eased = juce::jlimit (0.0f, 1.0f, cycle);
            sheenPhase = -3.8f + 7.8f * (eased * eased * (3.0f - 2.0f * eased));
            sheenStrength = cycle <= 1.0f ? (0.20f + 0.08f * std::sin (t * 0.9f)) * std::sin (eased * juce::MathConstants<float>::pi) : 0.0f;
        }
        shader.set ("uSheen", sheenPhase, sheenStrength);

        const auto I = Mat4::identity();

        // --- table + contact shadow ---
        shader.set ("uVentGlow", ventGlow);
        draw (meshes.table, I, shaders::table, zero);

        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);
        drawShadow (-light.x * 0.18f, 0.002f, 0.10f - light.z * 0.18f, earOuterX + 0.02f, bodyHalfD + 0.16f, 0.25f, 0.34f, 0.80f);
        drawShadow (0.0f, 0.003f, 0.0f, bodyHalfW + 0.03f, bodyHalfD + 0.03f, 0.08f, 0.05f, 0.55f);
        glDepthMask (GL_TRUE);
        glDisable (GL_BLEND);

        // --- chassis ---
        shader.set ("uVentGlow", zero);
        draw (meshes.chassis, I, shaders::satinChassis, colours::chassisPink);
        draw (meshes.ears, I, shaders::satinChassis, colours::chassisPink * 0.97f);
        draw (meshes.feet, I, shaders::plastic, { 0.035f, 0.03f, 0.035f });
        draw (meshes.screws, I, shaders::metal, { 0.72f, 0.72f, 0.76f });

        // Front power LED
        draw (meshes.led, Mat4::translation ({ 2.25f, 0.34f, bodyHalfD + 0.004f }) * Mat4::rotationX (0.5f * juce::MathConstants<float>::pi) * Mat4::scale (0.035f),
              shaders::emissive, { 0.2f, 0.05f, 0.1f }, colours::maskPink * 0.9f);

        // --- pearlescent panel ---
        shader.set ("uGlyphL", glyphL);
        shader.set ("uGlyphR", glyphR);
        shader.set ("uVentGlow", ventGlow);
        draw (meshes.panelTop, I, shaders::pearlPanel, zero);
        draw (meshes.panelEdges, I, shaders::pearlPanel, zero);

        // --- oscilloscope window ---
        shader.set ("uVentGlow", zero);
        shader.set ("uRecess", panelTop, scopeDepth);
        draw (meshes.scopeWalls, I, shaders::recess, { 0.10f, 0.10f, 0.11f });
        shader.set ("uScope", std::fmod (t / 3.2f, 1.0f), 1.0f, scopeRect.hw / scopeRect.hd, 0.0f);
        draw (meshes.scopeGlass, I, shaders::scopeGlass, zero);

        // --- vents ---
        shader.set ("uVentGlow", ventGlow);
        shader.set ("uRecess", panelTop, ventDepth);
        draw (meshes.ventWalls, I, shaders::recess, { 0.10f, 0.08f, 0.09f });
        draw (meshes.ventFloors, I, shaders::emissive, { 0.01f, 0.006f, 0.008f });

        // --- switches ---
        shader.set ("uVentGlow", zero);
        shader.set ("uRecess", panelTop, switchWellDepth);
        draw (meshes.switchWalls, I, shaders::recess, { 0.14f, 0.13f, 0.14f });
        draw (meshes.switchFloors, I, shaders::plastic, { 0.025f, 0.022f, 0.025f });

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind != ControlKind::toggle)
                continue;

            const auto& sw = switches[(size_t) i];
            const bool isMask = std::strcmp (c.paramId, params::id::modeMasking) == 0;
            const float glow = isMask ? maskGlow : stepGlow;
            const Vec3 ledColour = isMask ? colours::maskPink : colours::stepLime;
            const bool hovered = shared.hoveredControl.load() == i;

            const auto pivot = Mat4::translation ({ c.x, switchPivotY, c.z });
            draw (meshes.hub, pivot, shaders::metal, { 0.36f, 0.35f, 0.38f });
            draw (meshes.blade, pivot * Mat4::rotationX (sw.angle), shaders::metal,
                  hovered ? Vec3 { 0.95f, 0.93f, 0.97f } : Vec3 { 0.84f, 0.83f, 0.87f });

            draw (meshes.led, Mat4::translation ({ c.x, panelTop, c.z + 0.275f }) * Mat4::scale (0.03f),
                  shaders::emissive, mixVec ({ 0.10f, 0.05f, 0.06f }, ledColour * 0.5f, glow), ledColour * (glow * breath));
        }

        // --- knob shadows + LED value rings (blended) ---
        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind == ControlKind::toggle)
                continue;

            const float r = flangeRadius * c.scale;
            drawShadow (c.x - light.x * 0.07f * c.scale, panelTop + 0.0012f, c.z - light.z * 0.07f * c.scale,
                        r * 1.02f, r * 1.02f, r * 1.02f, 0.06f * c.scale, 0.45f);
        }

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind == ControlKind::toggle)
                continue;

            const auto& k = knobs[(size_t) i];
            shader.set ("uRing", k.angle, k.activity, k.hover, flangeRadius);
            shader.set ("uRingColor", sourceColour (k.source));
            draw (meshes.ledRing, Mat4::translation ({ c.x, panelTop + 0.002f, c.z }) * Mat4::scale (c.scale), shaders::ledRing, zero);
        }

        glDepthMask (GL_TRUE);
        glDisable (GL_BLEND);

        // --- knobs ---
        shader.set ("uRing", 0.0f, 0.0f, 0.0f, flangeRadius);

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind == ControlKind::toggle)
                continue;

            const auto& k = knobs[(size_t) i];
            const auto base = Mat4::translation ({ c.x, panelTop, c.z });
            const auto cap = base * Mat4::rotationY (-k.angle) * Mat4::scale (c.scale);

            // Static flange (never rotates)
            draw (meshes.flange, base * Mat4::scale (c.scale), shaders::knobFlange, { 0.065f, 0.062f, 0.07f });

            // Rotating cap
            draw (meshes.capBody, cap, shaders::plastic, { 0.045f, 0.043f, 0.05f }, Vec3 { 0.05f, 0.03f, 0.045f } * k.hover);
            draw (meshes.capInsert, cap, shaders::metal, { 0.80f, 0.79f, 0.83f });

            const Vec3 src = sourceColour (k.source);
            const Vec3 pointerCol = mixVec ({ 0.92f, 0.90f, 0.93f }, src, 0.35f + 0.65f * k.activity) * (0.9f + 0.9f * k.activity);
            draw (meshes.pointer, cap, shaders::emissive, pointerCol * 0.6f, pointerCol * 0.4f);

            // PD focus LED for adaptive targets
            if (c.kind == ControlKind::knob && c.pdTarget >= 0)
            {
                const float f = focusGlow[c.pdTarget];
                draw (meshes.led, Mat4::translation ({ c.x + 0.285f, panelTop, c.z - 0.285f }) * Mat4::scale (0.022f),
                      shaders::emissive, mixVec ({ 0.12f, 0.10f, 0.12f }, colours::maskPink * 0.5f, f), colours::maskPink * (f * 0.9f));
            }
        }

        // --- legend LEDs ---
        const Vec3 legend[3] { colours::user, colours::automation, colours::selfTune };
        for (int i = 0; i < 3; ++i)
            draw (meshes.led, Mat4::translation ({ legendX[i], panelTop, legendZ }) * Mat4::scale (0.024f),
                  shaders::emissive, legend[i] * 0.45f, legend[i] * 0.55f);
    }
}
