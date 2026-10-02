#include "SiteExport.h"
#include "UnitFace.h"
#include "Scene/SimScreens.h"
#include "Scene/ColourScreens.h"
#include "Scene/RoomScreen.h"
#include "../DSP/units/UnitList.h"

namespace pad::siteexport
{
    using namespace layout;

    namespace
    {
        /** Every unit the gear locker holds, by the key the site knows it by (the newer units: UnitList.h's). */
        std::vector<std::pair<int, juce::String>> units()
        {
            std::vector<std::pair<int, juce::String>> u { { enhUnit, "enhancer" }, { tubeUnit, "tonespace" }, { tideUnit, "compressor" }, { lumenUnit, "leveler" },
                                                          { limiterUnit, "limiter" }, { levelUnit, "level" }, { balancerUnit, "balancer" }, { deepUnit, "deepsub" },
                                                          { characterUnit, "character" }, { radarUnit, "radar" }, { x4Unit, "x4" }, { velvetUnit, "velvet" },
                                                          { takebackUnit, "takeback" }, { scopeUnit, "scope" }, { monitorUnit, "monitor" }, { lunchboxUnit, "lunchbox" } };
            for (int k = 0; k < gen::count; ++k) u.push_back ({ firstGenUnit + k, enh::dsp::units::info[k].key });
            return u;
        }

        void savePng (const juce::Image& img, const juce::File& f)
        {
            f.getParentDirectory().createDirectory();
            juce::PNGImageFormat png;
            if (auto out = f.createOutputStream()) { out->setPosition (0); out->truncate(); png.writeImageToStream (img, *out); }
        }

        juce::Image toImage (const colourscreen::Canvas& cv)
        {
            juce::Image img (juce::Image::RGB, cv.w, cv.h, false);
            juce::Image::BitmapData d (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < cv.h; ++y)
                for (int x = 0; x < cv.w; ++x)
                {
                    const auto* p = &cv.rgba[(size_t) ((y * cv.w + x) * 4)];
                    d.setPixelColour (x, y, juce::Colour (p[0], p[1], p[2]));
                }
            return img;
        }

        /** White-on-black lines and blobs (the simulations' and RAY ROOM's screens) as light on a canvas. */
        void stamp (colourscreen::Canvas& cv, const std::vector<roomscreen::Line>& lines, const std::vector<roomscreen::Blob>& blobs)
        {
            const colourscreen::Rgb white { 1.0f, 1.0f, 1.0f };
            for (const auto& l : lines) cv.line (l.x0, l.y0, l.x1, l.y1, white, l.bright * 1.2f, std::max (1.0f, 0.5f * l.thick));
            for (const auto& b : blobs)
            {
                if (! b.ring) { cv.splat (b.x, b.y, white, b.bright * 1.5f, std::max (1.0f, b.r)); continue; }
                const int steps = std::max (16, (int) (b.r * 6.3f));
                for (int k = 0; k < steps; ++k)
                {
                    const float a0 = 6.2832f * (float) k / (float) steps, a1 = 6.2832f * (float) (k + 1) / (float) steps;
                    cv.line (b.x + std::cos (a0) * b.r, b.y + std::sin (a0) * b.r, b.x + std::cos (a1) * b.r, b.y + std::sin (a1) * b.r, white, b.bright, 1.0f);
                }
            }
        }

        /** RAY ROOM's screen with a made-up state at time t (as the rack draws it with PAD_UI_TEST_DEMO). */
        void roomDemo (float t, float w, float h, std::vector<roomscreen::Line>& lines, std::vector<roomscreen::Blob>& blobs)
        {
            namespace R = enh::dsp::units::room;
            R::State s;
            s.W = R::halfWidth (5.0f); s.D = R::halfDepth (5.0f);
            s.levelL = 0.5f + 0.3f * std::sin (t * 2.1f); s.levelR = 0.5f + 0.3f * std::sin (t * 1.7f + 1.0f);
            s.width = 0.55f + 0.3f * std::sin (t * 0.4f); s.balance = 0.25f * std::sin (t * 0.3f);
            for (int b = 0; b < R::bands; ++b) { s.bandL[(size_t) b] = 0.35f + 0.3f * std::sin (t * (1.1f + 0.3f * (float) b) + (float) b); s.bandR[(size_t) b] = 0.35f + 0.3f * std::sin (t * (0.9f + 0.37f * (float) b) + 2.0f * (float) b); }
            s.dots = 14;
            for (int d = 0; d < s.dots; ++d)
            {
                const float ph = std::fmod (t * (0.35f + 0.05f * (float) d) + 0.13f * (float) d, 1.0f);
                const auto from = R::speaker (d & 1, s.W, s.D);
                const float a = 1.5708f + ((float) ((d * 37) % 19) / 18.0f - 0.5f) * 4.6f + 0.4f * std::sin (ph * 5.0f + (float) d);
                s.dot[(size_t) (d * 4)] = std::clamp (from.x + std::cos (a) * ph * s.W * 1.2f, -s.W, s.W);
                s.dot[(size_t) (d * 4 + 1)] = std::clamp (from.y + std::sin (a) * ph * s.D * 1.6f, -s.D, s.D);
                s.dot[(size_t) (d * 4 + 2)] = d % 4 == 0 ? 1.0f : 0.0f;
                s.dot[(size_t) (d * 4 + 3)] = ph;
            }
            roomscreen::build (s, t, w, h, lines, blobs);
        }
    }

    void write (const juce::File& dir)
    {
        dir.createDirectory();
        // the faceplates: 1400 px across (the site scales them down; a hover shows them bigger)
        for (const auto& [unit, key] : units())
            savePng (face::render (unit, 1400.0f / (2.0f * unitHalfW (unit)), true), dir.getChildFile ("faces/" + key + ".png"));

        // the screens: 48 frames, 1.6 s at 30 a second, 480 x 276 (a screen's shape)
        constexpr int frames = 48, W = 480, H = 276;
        for (int k = 0; k < gen::count; ++k)
        {
            const juce::String key = enh::dsp::units::info[k].key;
            const auto sim = simscreen::kindOf (key.toStdString());
            const auto col = colourscreen::kindOf (key.toStdString());
            const bool room = key == "rayroom";
            if (sim == simscreen::Kind::none && col == colourscreen::Kind::none && ! room) continue;
            colourscreen::Canvas cv; cv.resize (W, H);
            colourscreen::CubeState cube;
            std::array<float, 192> st {};
            std::vector<roomscreen::Line> lines; std::vector<roomscreen::Blob> blobs;
            for (int f = -12; f < frames; ++f)   // (12 frames first unkept: trails build up, the loop starts settled)
            {
                const float t = 2.0f + (float) f / 30.0f;
                if (col != colourscreen::Kind::none)
                {
                    colourscreen::demo (col, t, st.data());
                    if (col == colourscreen::Kind::chroma) colourscreen::chroma (cv, st.data(), t);
                    else if (col == colourscreen::Kind::tuner) colourscreen::tunerCloud (cv, t);
                    else colourscreen::hypercube (cv, cube, st.data(), t, 1.0f / 30.0f);
                }
                else
                {
                    cv.fade (0.35f);   // (a phosphor's short afterglow, as the rack's screens have)
                    if (room) roomDemo (t, (float) W, (float) H, lines, blobs);
                    else { simscreen::demo (sim, t, st.data()); simscreen::build (sim, st.data(), (float) W, (float) H, t, lines, blobs); }
                    stamp (cv, lines, blobs);
                }
                if (f < 0) continue;
                cv.toRgba();
                if (key == "hypercube" && f == 0 && juce::SystemStats::getEnvironmentVariable ("PAD_UI_HC_SHEET", {}).isNotEmpty())
                {
                    // (dev: a contact sheet - 16 moments, 2.5 s apart, of 40 s of made-up music: does it vary, does it read?)
                    colourscreen::Canvas c2; c2.resize (W, H); colourscreen::CubeState cs; std::array<float, 192> s2 {};
                    juce::Image sheet (juce::Image::RGB, W * 4, H * 4, true);
                    for (int fr = 0; fr < 40 * 30; ++fr)
                    {
                        const float tt = (float) fr / 30.0f;
                        colourscreen::demo (col, tt, s2.data());
                        if (tt > 20.0f && tt < 25.0f) s2[26] = 0.05f;   // (a quiet moment: home to the cube)
                        colourscreen::hypercube (c2, cs, s2.data(), tt, 1.0f / 30.0f);
                        if (fr % 75 == 74)
                        {
                            c2.toRgba();
                            const int q = fr / 75; juce::Graphics g (sheet);
                            g.drawImageAt (toImage (c2), (q % 4) * W, (q / 4) * H);
                        }
                    }
                    savePng (sheet, dir.getChildFile ("hypercube-sheet.png"));
                }
                savePng (toImage (cv), dir.getChildFile ("screens/" + key + "/f" + juce::String (f).paddedLeft ('0', 2) + ".png"));
            }
        }
    }
}
