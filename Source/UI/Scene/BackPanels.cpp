#include "BackPanels.h"
#include "DeviceLayout.h"

namespace pad::backs
{
    using namespace layout;

    float plateHalfW()           { return chassisHalfW - 0.05f; }
    float plateHalfH (int unit)  { return unitHalfH (unit) - 0.05f; }
    float plateY()               { return -unitBodyDepth + 0.004f; }

    namespace
    {
        juce::uint32 hashOf (juce::uint32 x)
        {
            x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
            return x;
        }
    }

    //==============================================================================
    /*  The makers. The units came from different companies, and their backs show it: where the mains goes in,
        what kind (an IEC inlet, one with its own switch, or a cord fixed through a grommet), the connectors, the
        vents, the plate or sticker with the serial number, the warning, the screws, the paint, the lettering. */
    const std::array<Maker, numMakers>& makers()
    {
        static const std::array<Maker, numMakers> m {{
            //  name                       made                              steel       ink         dim         accent     face pwrR  outF  trs  mains vents plate warn screws ground badge
            { "ENH AUDIO WORKS",          "HAND BUILT IN THE ENH WORKSHOP", 0xff1c1d20, 0xffe6e6e3, 0xff9a9ca0, 0xffd8b25a, 0, false, false, true,  0, 0, 0, 0, 0, 0, 0 },
            { "HALDEN & ROSS",            "BROADCAST APPARATUS - SHEFFIELD", 0xff4f5d52, 0xffe9e1c8, 0xffb4ad97, 0xffc9a227, 1, true,  false, false, 2, 5, 3, 2, 2, 1, 1 },
            { "NORTHFIELD ELECTRONICS",   "MADE IN U.S.A.",                 0xff3d4b5a, 0xfff1f1ee, 0xffa9b2bc, 0xffe24a3b, 0, true,  true,  true,  1, 1, 1, 1, 0, 0, 2 },
            { "KESTREL PRO AUDIO",        "DESIGNED IN VANCOUVER",          0xff111215, 0xffdedfe2, 0xff7d8086, 0xff3ec7e0, 0, false, false, true,  1, 2, 1, 0, 1, 0, 3 },
            { "VOLTA LABS",               "COSTRUITO A TORINO",             0xffd8d0bc, 0xff1e1d1b, 0xff5c5850, 0xffb02a24, 0, true,  false, false, 0, 1, 2, 3, 0, 1, 0 },
            { "ORBITAL DSP",              "ENGINEERED IN GOTHENBURG",       0xffa4a49b, 0xff141416, 0xff3c3c3e, 0xff2f6fd0, 2, false, true,  false, 0, 3, 1, 2, 1, 0, 2 },
            { "TANNHAUSER GERAETEBAU",    "HERGESTELLT IN HAMBURG",         0xffb9bbb6, 0xff1d1f22, 0xff55585c, 0xff2e5e3a, 0, false, false, true,  0, 0, 0, 3, 2, 1, 0 },
            { "AKAGI DENKI CO., LTD.",    "MADE IN JAPAN",                  0xffc4c5c7, 0xff17181a, 0xff4b4d52, 0xffc8202a, 0, true,  true,  false, 0, 0, 1, 1, 0, 0, 1 },
            { "BRIXTON VALVE CO.",        "HAND WIRED IN LONDON",           0xff161514, 0xffd9c48f, 0xff8f8466, 0xffc9a227, 1, false, false, false, 2, 5, 3, 2, 2, 1, 1 },
            { "SUNSET ANALOG",            "BUILT IN CALIFORNIA",            0xffb8b9bb, 0xff1a1a1c, 0xff55575b, 0xffe0702a, 0, true,  false, true,  1, 1, 2, 0, 1, 0, 2 },
        }};
        return m;
    }

    int makerOf (int unit)
    {
        switch (unit)
        {
            case enhUnit: case powerUnit: return 0;
            case tubeUnit:      return 8;
            case tideUnit:      return 2;
            case lumenUnit:     return 9;
            case limiterUnit:   return 6;
            case levelUnit:     return 5;
            case balancerUnit:  return 1;
            case monitorUnit:   return 3;
            case deepUnit:      return 4;
            case characterUnit: return 7;
            case radarUnit:     return 3;
            case x4Unit:        return 6;
            case velvetUnit:    return 4;
            case takebackUnit:  return 9;
            case scopeUnit:     return 1;
            default:            return (int) (hashOf ((juce::uint32) unit * 2654435761u + 7u) % (juce::uint32) numMakers);
        }
    }

    namespace
    {
        // Sizes in scene units (1U = 0.59 = 44.45 mm): an XLR's panel flange, a 1/4" jack's nut, an IEC inlet's flange
        constexpr float xlrW = 0.30f, xlrH = 0.36f, xlrPitch = 0.42f;
        constexpr float trsR = 0.095f;
        constexpr float iecW = 0.62f, iecH = 0.30f;

        struct Box { float u0, z0, u1, z1; };   // u across as seen from behind, left to right (x = -u); z down

        /** Where everything on a unit's back goes - the one plan the print and the cables both read. */
        struct Plan
        {
            bool audio = true, tall = false;
            float iecU = 0, rowZ = 0;                       // the power row: mains in, fuse, voltage selector, ground
            float fuseU = 0, selU = 0, groundU = 0, switchU = 0;
            float xlrU[4] {}, xlrZ[4] {};                   // IN L, IN R, OUT L, OUT R
            bool trs = false;
            float trsU = 0, trsZ[2] {};                     // TRS IN, OUT (tall units)
            Box warning {}, serial {}, brand {}, vents {};
            bool hasBrand = false, hasVents = false;
        };

        Box mirrored (Box b) { return { -b.u1, b.z0, -b.u0, b.z1 }; }

        Plan planOf (int unit)
        {
            const auto& mk = makers()[(size_t) makerOf (unit)];
            Plan p;
            const float W = plateHalfW(), H = plateHalfH (unit);
            p.audio = unit != powerUnit;
            p.tall = H > 0.40f;

            // (laid out with the mains on the left, as seen from behind; mirrored below for makers who put it right)
            p.rowZ = p.tall ? H - 0.24f : 0.02f;
            p.iecU = -W + 0.17f + 0.5f * iecW;
            p.switchU = p.iecU + 0.5f * iecW + 0.16f;          // (an IEC with its own switch: the rocker beside it)
            p.fuseU = (mk.mains == 1 ? p.switchU + 0.18f : p.iecU) + 0.50f - (mk.mains == 1 ? 0.12f : 0.0f);
            p.selU = p.fuseU + 0.33f;
            p.groundU = p.selU + 0.30f;

            if (! p.tall)
            {
                for (int i = 0; i < 4; ++i) { p.xlrU[i] = W - 0.20f - (float) (3 - i) * xlrPitch; p.xlrZ[i] = 0.03f; }
                const float mid0 = p.groundU + 0.13f, mid1 = p.audio ? p.xlrU[0] - 0.5f * xlrW - 0.08f : W - 0.10f;
                const float half = 0.5f * (mid1 - mid0);
                p.warning = { mid0, -0.18f, mid0 + std::min (0.56f, half - 0.03f), 0.19f };
                p.serial  = { mid1 - std::min (0.60f, half - 0.03f), -0.18f, mid1, 0.19f };
            }
            else
            {
                const float inZ = -H + 0.33f, outZ = inZ + 0.50f;
                for (int i = 0; i < 4; ++i)
                {
                    p.xlrU[i] = W - 0.20f - (float) (1 - (i & 1)) * xlrPitch;
                    p.xlrZ[i] = i < 2 ? inZ : outZ;
                }
                p.trs = p.audio && mk.trs;
                p.trsU = p.xlrU[0] - 0.40f;
                p.trsZ[0] = inZ; p.trsZ[1] = outZ;

                p.warning = { -W + 0.14f, -H + 0.10f, -W + 1.02f, -H + 0.50f };
                p.serial  = { p.warning.u1 + 0.10f, -H + 0.10f, p.warning.u1 + 0.80f, -H + 0.46f };
                const float c0 = p.serial.u1 + 0.16f, c1 = (p.audio ? (p.trs ? p.trsU - trsR : p.xlrU[0] - 0.5f * xlrW) : W) - 0.22f;
                p.hasBrand = c1 - c0 > 0.6f;
                p.brand = { c0, -H + 0.10f, c1, -H + 0.40f };
                p.hasVents = mk.vents != 4;
                p.vents = { p.hasBrand ? c0 : -W + 0.14f, p.hasBrand ? -H + 0.48f : p.warning.z1 + 0.10f,
                            c1, p.rowZ - 0.24f };
                if (p.vents.z1 - p.vents.z0 < 0.12f)
                    p.hasVents = false;
            }
            // Some makers put the outputs before the inputs
            if (mk.outputsFirst && p.audio)
                for (int i = 0; i < 2; ++i)
                {
                    std::swap (p.xlrU[i], p.xlrU[i + 2]);
                    std::swap (p.xlrZ[i], p.xlrZ[i + 2]);
                }
            if (mk.outputsFirst && p.trs)
                std::swap (p.trsZ[0], p.trsZ[1]);
            if (mk.powerRight)
            {
                for (float* u : { &p.iecU, &p.fuseU, &p.selU, &p.groundU, &p.switchU, &p.trsU })
                    *u = -*u;
                for (auto& u : p.xlrU)
                    u = -u;
                // (mirrored, a pair reads R L: put L back on the left)
                for (int i = 0; i < 4; i += 2)
                    if (p.xlrZ[i] == p.xlrZ[i + 1])
                        std::swap (p.xlrU[i], p.xlrU[i + 1]);
                p.warning = mirrored (p.warning);
                p.serial = mirrored (p.serial);
                p.brand = mirrored (p.brand);
                p.vents = mirrored (p.vents);
                if (! p.tall)
                    std::swap (p.warning, p.serial);
            }
            return p;
        }
    }

    std::vector<BackJack> jacksOf (int unit)
    {
        const auto p = planOf (unit);
        const auto& mk = makers()[(size_t) makerOf (unit)];
        std::vector<BackJack> j;
        j.push_back ({ mk.mains == 2 ? Jack::cord : Jack::iec, 0, -p.iecU, p.rowZ });
        j.push_back ({ Jack::ground, 0, -p.groundU, p.rowZ });
        if (p.audio)
            for (int i = 0; i < 4; ++i)
                j.push_back ({ i < 2 ? Jack::xlrIn : Jack::xlrOut, i & 1, -p.xlrU[i], p.xlrZ[i] });
        if (p.trs)
            for (int i = 0; i < 2; ++i)
                j.push_back ({ i == 0 ? Jack::trsIn : Jack::trsOut, 0, -p.trsU, p.trsZ[i] });
        if (mk.mains == 1)
            j.push_back ({ Jack::rocker, 0, -p.switchU, p.rowZ });
        j.push_back ({ Jack::fuse, 0, -p.fuseU, p.rowZ });
        return j;
    }

    //==============================================================================
    namespace
    {
        juce::Font font (float px, bool bold = false, float tracking = 0.0f, int face = 0)
        {
            auto o = juce::FontOptions().withHeight (px).withKerningFactor (tracking);
            if (face == 1) o = o.withName (juce::Font::getDefaultSerifFontName());
            if (face == 2) o = o.withName (juce::Font::getDefaultMonospacedFontName());
            return juce::Font (bold ? o.withStyle ("Bold") : o);
        }

        /** Draws in scene units: (u, z) to pixels. */
        struct Pen
        {
            juce::Graphics& g;
            float k, W, H;
            int face = 0, screws = 0;   // the maker's lettering (sans, serif, mono) and screw heads (cross, hex, slot)
            float px (float u) const { return (u + W) * k; }
            float pz (float z) const { return (z + H) * k; }
            juce::Rectangle<float> rect (float u0, float z0, float u1, float z1) const { return { px (u0), pz (z0), (u1 - u0) * k, (z1 - z0) * k }; }
            juce::Rectangle<float> rect (const Box& b) const { return rect (b.u0, b.z0, b.u1, b.z1); }
            juce::Rectangle<float> around (float u, float z, float hw, float hh) const { return rect (u - hw, z - hh, u + hw, z + hh); }
            juce::Font f (float sizeU, bool bold = false, float tracking = 0.04f) const { return font (sizeU * k, bold, tracking, face); }

            void text (const juce::String& s, float u, float z, float sizeU, juce::Colour c, bool bold = false,
                       juce::Justification j = juce::Justification::centred, float widthU = 2.0f) const
            {
                g.setColour (c);
                g.setFont (f (sizeU, bold));
                const float w = widthU * k, h = sizeU * k * 1.4f;
                float x = px (u) - 0.5f * w;
                if (j.testFlags (juce::Justification::left)) x = px (u);
                if (j.testFlags (juce::Justification::right)) x = px (u) - w;
                g.drawText (s, juce::Rectangle<float> (x, pz (z) - 0.5f * h, w, h), j, false);
            }

            /** A screw: domed, its drive (cross, hex socket or slot), a soft shadow below-right. */
            void screw (float u, float z, float rU = 0.034f) const
            {
                const float r = rU * k, x = px (u), y = pz (z);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillEllipse (x - r + 0.18f * r, y - r + 0.25f * r, 2 * r, 2 * r);
                const bool black = screws == 1;
                g.setGradientFill ({ juce::Colour (black ? 0xff5a5b60 : 0xffd8d9dc), x - 0.4f * r, y - 0.5f * r,
                                     juce::Colour (black ? 0xff18191b : 0xff55575c), x + 0.7f * r, y + 0.8f * r, true });
                g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
                g.setColour (juce::Colour (0xff1a1b1d));
                const float a = 0.62f * r, t = juce::jmax (1.0f, 0.22f * r);
                if (screws == 1)
                {
                    juce::Path hex;
                    for (int i = 0; i < 6; ++i)
                    {
                        const float an = juce::MathConstants<float>::pi / 3.0f * (float) i;
                        const juce::Point<float> v { x + 0.45f * r * std::cos (an), y + 0.45f * r * std::sin (an) };
                        if (i == 0) hex.startNewSubPath (v); else hex.lineTo (v);
                    }
                    hex.closeSubPath();
                    g.fillPath (hex);
                }
                else if (screws == 2)
                {
                    const float an = (float) (hashOf ((juce::uint32) (x * 7.0f + y * 13.0f)) % 628u) / 100.0f;
                    g.drawLine (x - a * std::cos (an), y - a * std::sin (an), x + a * std::cos (an), y + a * std::sin (an), t);
                }
                else
                {
                    g.drawLine (x - a, y, x + a, y, t);
                    g.drawLine (x, y - a, x, y + a, t);
                }
            }
        };

        /** An XLR panel connector: the nickel flange with its two screws, the black insert, and the three contacts -
            holes and a PUSH latch for a female (input), pins in a shell for a male. A combo female takes a 1/4" plug
            through its middle too. */
        void xlr (const Pen& p, float u, float z, bool female, bool combo = false, bool black = false)
        {
            auto& g = p.g;
            const auto fl = p.around (u, z, 0.5f * xlrW, 0.5f * xlrH);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (fl.translated (0.012f * p.k, 0.016f * p.k), 0.05f * p.k);
            g.setGradientFill ({ juce::Colour (black ? 0xff3a3b3f : 0xffc9cacd), fl.getX(), fl.getY(),
                                 juce::Colour (black ? 0xff0e0e10 : 0xff6e7075), fl.getRight(), fl.getBottom(), false });
            g.fillRoundedRectangle (fl, 0.05f * p.k);
            g.setColour (juce::Colour (0xff3b3c40));
            g.drawRoundedRectangle (fl, 0.05f * p.k, 0.006f * p.k);
            p.screw (u - 0.5f * xlrW + 0.045f, z - 0.5f * xlrH + 0.05f, 0.022f);
            p.screw (u + 0.5f * xlrW - 0.045f, z + 0.5f * xlrH - 0.05f, 0.022f);

            const float r = 0.118f * p.k, cx = p.px (u), cy = p.pz (z + 0.02f);
            g.setGradientFill ({ juce::Colour (0xff2c2d31), cx, cy - r, juce::Colour (0xffa9abb0), cx, cy + r, false });
            g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
            const float ri = r * 0.84f;
            g.setGradientFill ({ juce::Colour (female ? 0xff141416 : 0xff060607), cx, cy - ri, juce::Colour (female ? 0xff2a2a2d : 0xff151517), cx, cy + ri, false });
            g.fillEllipse (cx - ri, cy - ri, 2 * ri, 2 * ri);
            g.setColour (juce::Colour (0xff0b0b0c));
            g.fillRect (cx - 0.11f * r, cy - r * 1.02f, 0.22f * r, 0.3f * r);

            const float pr = 0.17f * r;
            const juce::Point<float> pins[3] { { cx - 0.42f * r, cy - 0.12f * r }, { cx + 0.42f * r, cy - 0.12f * r }, { cx, cy + 0.42f * r } };
            for (int i = 0; i < 3; ++i)
            {
                auto q = pins[i];
                if (female)
                {
                    g.setColour (juce::Colour (0xff5a5b5f));
                    g.fillEllipse (q.x - pr * 1.25f, q.y - pr * 1.25f, pr * 2.5f, pr * 2.5f);
                    g.setColour (juce::Colour (0xff000000));
                    g.fillEllipse (q.x - pr * 0.8f, q.y - pr * 0.8f, pr * 1.6f, pr * 1.6f);
                }
                else
                {
                    g.setGradientFill ({ juce::Colour (0xfff2e3b5), q.x - pr, q.y - pr, juce::Colour (0xff8a7440), q.x + pr, q.y + pr, true });
                    g.fillEllipse (q.x - pr, q.y - pr, 2 * pr, 2 * pr);
                }
            }
            if (female && combo)   // the 1/4" hole through the middle
            {
                g.setColour (juce::Colour (0xff6a6b70));
                g.fillEllipse (cx - 0.30f * r, cy - 0.20f * r, 0.60f * r, 0.60f * r);
                g.setColour (juce::Colour (0xff000000));
                g.fillEllipse (cx - 0.21f * r, cy - 0.11f * r, 0.42f * r, 0.42f * r);
            }
            if (female)
            {
                const auto tab = p.around (u, z - 0.5f * xlrH + 0.035f, 0.045f, 0.022f);
                g.setGradientFill ({ juce::Colour (0xffeeeeef), tab.getX(), tab.getY(), juce::Colour (0xff7d7f84), tab.getX(), tab.getBottom(), false });
                g.fillRoundedRectangle (tab, 0.008f * p.k);
                p.text ("PUSH", u, z - 0.5f * xlrH + 0.075f, 0.024f, juce::Colour (black ? 0xffc8c8c8 : 0xff2a2b2e), true, juce::Justification::centred, 0.2f);
            }
        }

        /** A 1/4" jack: hex nut, washer, the dark socket. */
        void trs (const Pen& p, float u, float z)
        {
            auto& g = p.g;
            const float cx = p.px (u), cy = p.pz (z), r = trsR * p.k;
            juce::Path hex;
            for (int i = 0; i < 6; ++i)
            {
                const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + 0.26f;
                const juce::Point<float> v { cx + r * std::cos (a), cy + r * std::sin (a) };
                if (i == 0) hex.startNewSubPath (v); else hex.lineTo (v);
            }
            hex.closeSubPath();
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillPath (hex, juce::AffineTransform::translation (0.010f * p.k, 0.014f * p.k));
            g.setGradientFill ({ juce::Colour (0xffe1e2e5), cx - r, cy - r, juce::Colour (0xff5d5f64), cx + r, cy + r, false });
            g.fillPath (hex);
            g.setColour (juce::Colour (0xff3d3e42));
            g.strokePath (hex, juce::PathStrokeType (0.005f * p.k));
            const float rw = 0.66f * r;
            g.setGradientFill ({ juce::Colour (0xff8f9196), cx, cy - rw, juce::Colour (0xffd5d6d9), cx, cy + rw, false });
            g.fillEllipse (cx - rw, cy - rw, 2 * rw, 2 * rw);
            const float rh = 0.40f * r;
            g.setColour (juce::Colour (0xff050506));
            g.fillEllipse (cx - rh, cy - rh, 2 * rh, 2 * rh);
        }

        /** An IEC C14 inlet: flange with two screws, the black moulding, the three blades in its well. */
        void iec (const Pen& p, float u, float z)
        {
            auto& g = p.g;
            const auto fl = p.around (u, z, 0.5f * iecW, 0.5f * iecH);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (fl.translated (0.012f * p.k, 0.016f * p.k), 0.03f * p.k);
            g.setGradientFill ({ juce::Colour (0xff2f3034), fl.getX(), fl.getY(), juce::Colour (0xff121214), fl.getX(), fl.getBottom(), false });
            g.fillRoundedRectangle (fl, 0.03f * p.k);
            p.screw (u - 0.5f * iecW + 0.06f, z, 0.026f);
            p.screw (u + 0.5f * iecW - 0.06f, z, 0.026f);

            const float hw = 0.165f * p.k, hh = 0.115f * p.k, cut = 0.06f * p.k, cx = p.px (u), cy = p.pz (z);
            juce::Path well;
            well.startNewSubPath (cx - hw, cy - hh);
            well.lineTo (cx + hw, cy - hh);
            well.lineTo (cx + hw, cy + hh - cut);
            well.lineTo (cx + hw - cut, cy + hh);
            well.lineTo (cx - hw + cut, cy + hh);
            well.lineTo (cx - hw, cy + hh - cut);
            well.closeSubPath();
            g.setColour (juce::Colour (0xff050506));
            g.fillPath (well);
            g.setColour (juce::Colour (0xff3a3b3f));
            g.strokePath (well, juce::PathStrokeType (0.006f * p.k));
            g.setColour (juce::Colour (0xffcdbf93));
            const float bw = 0.016f * p.k, bh = 0.055f * p.k;
            g.fillRect (cx - 0.085f * p.k - bw, cy - 0.01f * p.k - bh, 2 * bw, 2 * bh);
            g.fillRect (cx + 0.085f * p.k - bw, cy - 0.01f * p.k - bh, 2 * bw, 2 * bh);
            g.fillRect (cx - bw, cy - 0.055f * p.k - bh * 0.8f, 2 * bw, 1.6f * bh);
        }

        /** A fixed mains cord's way out: a black rubber grommet with its strain relief. */
        void grommet (const Pen& p, float u, float z)
        {
            auto& g = p.g;
            const float cx = p.px (u), cy = p.pz (z), r = 0.11f * p.k;
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillEllipse (cx - r + 0.012f * p.k, cy - r + 0.016f * p.k, 2 * r, 2 * r);
            g.setGradientFill ({ juce::Colour (0xff3a3a3c), cx - r, cy - r, juce::Colour (0xff09090a), cx + r, cy + r, true });
            g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
            g.setColour (juce::Colour (0xff050505));
            g.fillEllipse (cx - 0.55f * r, cy - 0.55f * r, 1.1f * r, 1.1f * r);
        }

        /** A rocker: the mains switch built in beside some makers' inlets (I / O). */
        void rocker (const Pen& p, float u, float z, juce::Colour ink)
        {
            auto& g = p.g;
            const auto b = p.around (u, z, 0.07f, 0.12f);
            g.setColour (juce::Colour (0xff0a0a0b));
            g.fillRoundedRectangle (b.expanded (0.012f * p.k), 0.015f * p.k);
            g.setGradientFill ({ juce::Colour (0xff3c3d41), b.getX(), b.getY(), juce::Colour (0xff141416), b.getX(), b.getBottom(), false });
            g.fillRoundedRectangle (b, 0.01f * p.k);
            g.setColour (juce::Colour (0xffd8d8d8));
            g.setFont (font (0.05f * p.k, true));
            g.drawText ("I", b.withHeight (b.getHeight() * 0.5f), juce::Justification::centred);
            g.drawText ("O", b.withTrimmedTop (b.getHeight() * 0.5f), juce::Justification::centred);
            p.text ("POWER", u, z - 0.17f, 0.03f, ink, true, juce::Justification::centred, 0.4f);
        }

        juce::String modelOf (int unit)
        {
            const juce::String name = unitInfo[(size_t) unit].name;
            juce::String initials;
            for (auto& w : juce::StringArray::fromTokens (name, " -&", ""))
                if (w.isNotEmpty() && juce::CharacterFunctions::isLetter (w[0]))
                    initials << w.substring (0, 1);
            return initials.substring (0, 3) + "-" + juce::String (100 + (int) (hashOf ((juce::uint32) unit + 99u) % 900u));
        }

        /** The shock warning, in the maker's way: 0 a sticker with a red CAUTION band, 1 a yellow sticker with a
            big triangle, 2 printed straight on the steel, 3 a white sticker in two languages. */
        void warningLabel (const Pen& p, const Box& b, int style, juce::Colour ink)
        {
            auto& g = p.g;
            const auto r = p.rect (b);
            const float k = p.k, h = b.z1 - b.z0;
            if (style == 2)   // printed on the steel, no sticker
            {
                g.setColour (ink);
                g.setFont (p.f (0.034f, true));
                g.drawFittedText ("WARNING", r.withHeight (r.getHeight() * 0.25f).toNearestInt(), juce::Justification::centredLeft, 1);
                g.setFont (p.f (0.024f));
                g.drawFittedText ("TO REDUCE THE RISK OF FIRE OR ELECTRIC SHOCK, DO NOT REMOVE THE COVER. "
                                  "NO USER SERVICEABLE PARTS INSIDE. THIS APPARATUS MUST BE EARTHED.",
                                  r.withTrimmedTop (r.getHeight() * 0.27f).toNearestInt(), juce::Justification::topLeft, 5, 0.8f);
                return;
            }
            const juce::Colour paper = style == 1 ? juce::Colour (0xfff2c418) : juce::Colour (0xfff1ede2);
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (r.translated (0.006f * k, 0.008f * k), 0.012f * k);
            g.setColour (paper);
            g.fillRoundedRectangle (r, 0.012f * k);
            float top = r.getY() + 0.04f * h * k;
            if (style == 0)
            {
                const auto band = r.withHeight (0.26f * r.getHeight()).reduced (0.012f * k, 0.012f * k);
                g.setColour (juce::Colour (0xffb51f1a));
                g.fillRect (band);
                g.setColour (juce::Colours::white);
                g.setFont (font (band.getHeight() * 0.78f, true, 0.12f));
                g.drawText ("CAUTION", band, juce::Justification::centred);
                top = band.getBottom() + 0.04f * h * k;
            }
            else if (style == 3)
            {
                g.setColour (juce::Colour (0xff111111));
                g.drawRect (r.reduced (0.012f * k), juce::jmax (1.0f, 0.004f * k));
            }
            const float triH = (style == 1 ? 0.62f : 0.42f) * h * k;
            auto triangle = [&] (float cx, bool flash)
            {
                juce::Path t;
                t.addTriangle (cx, top, cx - 0.58f * triH, top + triH, cx + 0.58f * triH, top + triH);
                g.setColour (style == 1 ? juce::Colour (0xff111111) : juce::Colour (0xfff4c418));
                if (style != 1) g.fillPath (t);
                g.setColour (juce::Colour (0xff111111));
                g.strokePath (t, juce::PathStrokeType (0.08f * triH, juce::PathStrokeType::curved));
                if (flash)
                {
                    juce::Path f;
                    f.startNewSubPath (cx + 0.06f * triH, top + 0.28f * triH);
                    f.lineTo (cx - 0.16f * triH, top + 0.66f * triH);
                    f.lineTo (cx + 0.02f * triH, top + 0.62f * triH);
                    f.lineTo (cx - 0.08f * triH, top + 0.92f * triH);
                    f.lineTo (cx + 0.16f * triH, top + 0.52f * triH);
                    f.lineTo (cx - 0.01f * triH, top + 0.56f * triH);
                    f.closeSubPath();
                    g.fillPath (f);
                }
                else
                {
                    g.fillRoundedRectangle (cx - 0.05f * triH, top + 0.32f * triH, 0.10f * triH, 0.38f * triH, 0.04f * triH);
                    g.fillEllipse (cx - 0.06f * triH, top + 0.76f * triH, 0.12f * triH, 0.12f * triH);
                }
            };
            const bool wide = r.getWidth() > 2.2f * r.getHeight();
            triangle (r.getX() + 0.65f * triH + 0.01f * k, true);
            if (wide && style != 1) triangle (r.getRight() - 0.65f * triH, false);

            auto body = juce::Rectangle<float> (r.getX() + 1.35f * triH, top, r.getWidth() - (wide && style != 1 ? 2.7f : 1.45f) * triH, triH);
            g.setColour (juce::Colour (0xff111111));
            g.setFont (font (body.getHeight() * (style == 1 ? 0.2f : 0.25f), true));
            const juce::String main = style == 1 ? "WARNING\nSHOCK HAZARD\nDO NOT OPEN" : "RISK OF ELECTRIC SHOCK\nDO NOT OPEN";
            g.drawFittedText (main, body.toNearestInt().withTrimmedBottom ((int) ((style == 1 ? 0.2f : 0.42f) * body.getHeight())), juce::Justification::centred, 3);
            g.setFont (font (body.getHeight() * 0.15f));
            const juce::String second = style == 3 ? "ACHTUNG: GEFAHR EINES ELEKTRISCHEN SCHLAGES\nGERAET NICHT OEFFNEN"
                                                   : "AVIS: RISQUE DE CHOC ELECTRIQUE - NE PAS OUVRIR\nNO USER-SERVICEABLE PARTS INSIDE";
            g.drawFittedText (second, body.toNearestInt().withTrimmedTop ((int) ((style == 1 ? 0.8f : 0.58f) * body.getHeight())), juce::Justification::centred, 2);
            g.setFont (font (0.07f * h * k));
            g.drawText ("REFER SERVICING TO QUALIFIED PERSONNEL. TO REDUCE THE RISK OF FIRE, DO NOT EXPOSE TO RAIN.",
                        r.withTop (r.getBottom() - 0.14f * h * k).reduced (0.03f * k, 0.0f), juce::Justification::centred, true);
        }

        /** The rating plate: 0 a brushed metal plate, 1 a paper sticker with a barcode, 2 printed on the steel,
            3 an engraved brass plate riveted on. */
        void serialPlate (const Pen& p, const Box& b, int unit, const Maker& mk)
        {
            auto& g = p.g;
            const auto r = p.rect (b);
            const float k = p.k;
            const int style = mk.plate;
            juce::Colour ink (0xff1a1b1d);
            if (style != 2)
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillRoundedRectangle (r.translated (0.006f * k, 0.008f * k), 0.01f * k);
            }
            if (style == 0)
                g.setGradientFill ({ juce::Colour (0xffd9dadc), r.getX(), r.getY(), juce::Colour (0xff9b9da1), r.getRight(), r.getBottom(), false });
            else if (style == 1)
                g.setColour (juce::Colour (0xfff7f6f2));
            else if (style == 3)
                g.setGradientFill ({ juce::Colour (0xffe6c675), r.getX(), r.getY(), juce::Colour (0xff8c6a24), r.getRight(), r.getBottom(), false });
            if (style != 2)
                g.fillRoundedRectangle (r, style == 1 ? 0.004f * k : 0.01f * k);
            else
                ink = juce::Colour (mk.ink);
            if (style == 0 || style == 3)
                for (int i = 0; i < 26; ++i)   // brushed: faint streaks along it
                {
                    const float y = r.getY() + r.getHeight() * ((float) (hashOf ((juce::uint32) (unit * 64 + i)) % 1000u) / 1000.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.10f));
                    g.drawHorizontalLine ((int) y, r.getX() + 2.0f, r.getRight() - 2.0f);
                }
            if (style == 3)   // rivets
                for (float fx : { 0.03f, 0.97f })
                    for (float fy : { 0.12f, 0.88f })
                    {
                        const float cx = r.getX() + fx * r.getWidth(), cy = r.getY() + fy * r.getHeight(), rr = 0.012f * k;
                        g.setGradientFill ({ juce::Colour (0xfff0e0b0), cx - rr, cy - rr, juce::Colour (0xff6b5020), cx + rr, cy + rr, true });
                        g.fillEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr);
                    }
            const float lh = r.getHeight() / 7.0f;
            const float inset = style == 3 ? 0.05f : 0.03f;
            auto line = [&] (int row, const juce::String& s, bool bold, float size = 0.72f)
            {
                g.setColour (ink);
                g.setFont (font (lh * size, bold, 0.0f, p.face));
                g.drawText (s, juce::Rectangle<float> (r.getX() + inset * k, r.getY() + lh * (0.35f + (float) row), r.getWidth() - 2.0f * inset * k, lh),
                            juce::Justification::centredLeft, true);
            };
            const auto serial = juce::String::toHexString ((int) (hashOf ((juce::uint32) unit * 7919u + 3u) & 0xffffffu)).toUpperCase().paddedLeft ('0', 6);
            line (0, mk.name, true, 0.86f);
            line (1, "MODEL  " + modelOf (unit), false);
            line (2, "SERIAL No.  " + serial.substring (0, 2) + "-" + serial.substring (2), false);
            line (3, mk.powerRight ? "120V~ 60Hz  30W" : "100-240V~  50/60Hz  25W", false);
            line (4, "FUSE  T500mAL 250V", false);
            line (5, mk.made, false, 0.60f);

            if (style == 1)   // a barcode down its right
            {
                const float bx = r.getRight() - 0.03f * k - lh * 2.2f, by = r.getY() + lh * 1.3f;
                juce::uint32 h = hashOf ((juce::uint32) unit * 31u + 5u);
                for (float x = bx; x < bx + lh * 2.2f; )
                {
                    h = hashOf (h);
                    const float w = (float) (1 + (h % 3u));
                    if ((h >> 4) & 1u) { g.setColour (ink); g.fillRect (x, by, w, lh * 2.2f); }
                    x += w + 1.0f;
                }
            }
            // the marks along the bottom-right: a conformity ring and the crossed-out bin
            const float m = lh * 1.15f, mx = r.getRight() - 0.03f * k - m, my = r.getBottom() - 0.02f * k - m;
            g.setColour (ink);
            g.drawRect (juce::Rectangle<float> (mx + m * 0.22f, my + m * 0.18f, m * 0.56f, m * 0.72f), juce::jmax (1.0f, m * 0.07f));
            g.drawLine (mx + m * 0.1f, my + m * 0.05f, mx + m * 0.9f, my + m * 0.95f, juce::jmax (1.0f, m * 0.07f));
            g.drawLine (mx + m * 0.9f, my + m * 0.05f, mx + m * 0.1f, my + m * 0.95f, juce::jmax (1.0f, m * 0.07f));
            g.drawEllipse (juce::Rectangle<float> (mx - m * 1.15f, my + m * 0.1f, m * 0.85f, m * 0.85f), juce::jmax (1.0f, m * 0.08f));
            g.setFont (font (m * 0.42f, true));
            g.drawText (juce::String (mk.name).substring (0, 3), juce::Rectangle<float> (mx - m * 1.15f, my + m * 0.1f, m * 0.85f, m * 0.85f), juce::Justification::centred);
        }

        /** Air: 0 pressed slots, 1 a grid of round holes, 2 hex mesh, 3 a fan behind a wire grille, 5 louvres. */
        void vents (const Pen& p, const Box& b, int style)
        {
            auto& g = p.g;
            const float k = p.k;
            const auto v = p.rect (b);
            if (style == 3)
            {
                const float r = juce::jmin (v.getHeight(), v.getWidth()) * 0.46f, cx = v.getCentreX(), cy = v.getCentreY();
                g.setColour (juce::Colour (0xff050506));
                g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
                g.setColour (juce::Colour (0xff2a2b2e));   // the fan's blades, dim behind
                for (int i = 0; i < 7; ++i)
                {
                    juce::Path bl;
                    bl.addPieSegment (cx - 0.9f * r, cy - 0.9f * r, 1.8f * r, 1.8f * r, (float) i * 0.8976f, (float) i * 0.8976f + 0.5f, 0.3f);
                    g.fillPath (bl);
                }
                g.setColour (juce::Colour (0xffb8b9bc));   // the wire grille
                for (float f : { 0.25f, 0.5f, 0.75f, 1.0f })
                    g.drawEllipse (cx - f * r, cy - f * r, 2 * f * r, 2 * f * r, juce::jmax (1.0f, 0.006f * k));
                g.drawLine (cx - r, cy, cx + r, cy, juce::jmax (1.0f, 0.006f * k));
                g.drawLine (cx, cy - r, cx, cy + r, juce::jmax (1.0f, 0.006f * k));
                for (float sx : { -1.0f, 1.0f })
                    for (float sy : { -1.0f, 1.0f })
                        p.screw ((cx + sx * r * 0.9f) / k - p.W, (cy + sy * r * 0.9f) / k - p.H, 0.022f);
                return;
            }
            if (style == 1 || style == 2)
            {
                const float pitch = (style == 1 ? 0.07f : 0.06f) * k, rr = (style == 1 ? 0.021f : 0.024f) * k;
                for (float y = v.getY() + pitch * 0.5f, row = 0; y < v.getBottom() - rr; y += pitch * (style == 2 ? 0.866f : 1.0f), ++row)
                    for (float x = v.getX() + pitch * 0.5f + ((int) row % 2 ? pitch * 0.5f : 0.0f); x < v.getRight() - rr; x += pitch)
                    {
                        g.setColour (juce::Colour (0xff040405));
                        if (style == 1)
                            g.fillEllipse (x - rr, y - rr, 2 * rr, 2 * rr);
                        else
                        {
                            juce::Path hex;
                            for (int i = 0; i < 6; ++i)
                            {
                                const float an = juce::MathConstants<float>::pi / 3.0f * (float) i + 0.5236f;
                                const juce::Point<float> q { x + rr * std::cos (an), y + rr * std::sin (an) };
                                if (i == 0) hex.startNewSubPath (q); else hex.lineTo (q);
                            }
                            hex.closeSubPath();
                            g.fillPath (hex);
                        }
                    }
                return;
            }
            // slots, or louvres (wider, with a lit lip over each)
            const bool louvre = style == 5;
            const float slotW = (louvre ? 0.60f : 0.30f) * k, slotH = (louvre ? 0.030f : 0.035f) * k, gapU = 0.08f * k, gapZ = (louvre ? 0.05f : 0.035f) * k;
            const int cols = juce::jmax (1, (int) ((v.getWidth() + gapU) / (slotW + gapU)));
            const int rows = juce::jmax (1, (int) ((v.getHeight() + gapZ) / (slotH + gapZ)));
            const float x0 = v.getCentreX() - 0.5f * ((float) cols * (slotW + gapU) - gapU);
            const float y0 = v.getCentreY() - 0.5f * ((float) rows * (slotH + gapZ) - gapZ);
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                {
                    const juce::Rectangle<float> s (x0 + (float) c * (slotW + gapU), y0 + (float) r * (slotH + gapZ), slotW, slotH);
                    if (louvre)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.16f));
                        g.fillRoundedRectangle (s.translated (0, -0.012f * k).withHeight (0.012f * k), 0.006f * k);
                    }
                    g.setColour (juce::Colours::white.withAlpha (0.10f));
                    g.fillRoundedRectangle (s.translated (0, 0.004f * k), 0.5f * slotH);
                    g.setColour (juce::Colour (0xff050506));
                    g.fillRoundedRectangle (s, 0.5f * slotH);
                }
        }

        /** The maker's badge: 0 a box, 1 an oval, 2 a wordmark over a rule, 3 a diamond. */
        void badge (const Pen& p, juce::Rectangle<float> b, const Maker& mk, int unit)
        {
            auto& g = p.g;
            const juce::Colour ink (mk.ink), steel (mk.steel), accent (mk.accent);
            const juce::String initials = [&]
            {
                juce::String s;
                for (auto& w : juce::StringArray::fromTokens (mk.name, " &.,", ""))
                    if (w.isNotEmpty() && s.length() < 3 && w != "CO" && w != "LTD") s << w.substring (0, 1);
                return s;
            }();
            juce::Rectangle<float> logo = b.withWidth (b.getHeight() * 1.5f);
            switch (mk.badge)
            {
                case 1:
                    g.setColour (accent);
                    g.fillEllipse (logo);
                    g.setColour (steel);
                    g.drawEllipse (logo.reduced (0.012f * p.k), juce::jmax (1.0f, 0.006f * p.k));
                    g.setFont (p.f (b.getHeight() / p.k * 0.42f, true, 0.06f));
                    g.drawText (initials, logo, juce::Justification::centred);
                    break;
                case 2:
                    logo = b.withWidth (0.0f);
                    break;
                case 3:
                {
                    juce::Path d;
                    const auto c = logo.getCentre();
                    const float rr = logo.getHeight() * 0.55f;
                    d.addQuadrilateral (c.x, c.y - rr, c.x + rr, c.y, c.x, c.y + rr, c.x - rr, c.y);
                    g.setColour (accent);
                    g.fillPath (d);
                    g.setColour (steel);
                    g.setFont (p.f (b.getHeight() / p.k * 0.30f, true));
                    g.drawText (initials.substring (0, 1), logo, juce::Justification::centred);
                    break;
                }
                default:
                    g.setColour (ink);
                    g.fillRoundedRectangle (logo, 0.03f * p.k);
                    g.setColour (steel);
                    g.setFont (p.f (b.getHeight() / p.k * 0.50f, true, 0.06f));
                    g.drawText (initials, logo, juce::Justification::centred);
                    break;
            }
            const auto txt = b.withTrimmedLeft (logo.getWidth() + (mk.badge == 2 ? 0.0f : 0.08f * p.k));
            g.setColour (ink);
            g.setFont (p.f (b.getHeight() / p.k * 0.30f, true, 0.06f));
            g.drawFittedText (mk.name, txt.withHeight (b.getHeight() * 0.40f).toNearestInt(), juce::Justification::centredLeft, 1);
            if (mk.badge == 2)
            {
                g.setColour (accent);
                g.fillRect (txt.getX(), txt.getY() + b.getHeight() * 0.42f, txt.getWidth() * 0.85f, juce::jmax (2.0f, 0.012f * p.k));
            }
            g.setFont (p.f (b.getHeight() / p.k * 0.24f, true, 0.04f));
            g.drawFittedText (unitInfo[(size_t) unit].name, txt.withTrimmedTop ((int) (b.getHeight() * 0.48f)).withHeight (b.getHeight() * 0.28f).toNearestInt(),
                              juce::Justification::centredLeft, 1);
            g.setColour (juce::Colour (mk.dim));
            g.setFont (p.f (b.getHeight() / p.k * 0.15f, false, 0.04f));
            g.drawFittedText ("MODEL " + modelOf (unit) + "   -   " + juce::String (mk.made),
                              txt.withTrimmedTop ((int) (b.getHeight() * 0.78f)).toNearestInt(), juce::Justification::centredLeft, 1);
        }
    }

    namespace
    {
        /** An image copied out RGBA (opaque); mirrored left to right for a plate that is read from behind. */
        artwork::RawTexture toRaw (const juce::Image& img, bool mirror)
        {
            const int w = img.getWidth(), h = img.getHeight();
            artwork::RawTexture raw;
            raw.width = w; raw.height = h; raw.channels = 4;
            raw.pixels.resize ((size_t) (w * h * 4));
            const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const auto c = bd.getPixelColour (mirror ? w - 1 - x : x, y);
                    auto* d = raw.pixels.data() + ((size_t) y * (size_t) w + (size_t) x) * 4;
                    d[0] = c.getRed(); d[1] = c.getGreen(); d[2] = c.getBlue(); d[3] = 255;
                }
            return raw;
        }
    }

    artwork::RawTexture renderBack (int unit, int textureWidth)
    {
        const float W = plateHalfW(), H = plateHalfH (unit);
        const float k = (float) textureWidth / (2.0f * W);
        const int texH = juce::jmax (8, (int) std::lround (2.0f * H * k));
        juce::Image img (juce::Image::ARGB, textureWidth, texH, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            const auto& mk = makers()[(size_t) makerOf (unit)];
            Pen p { g, k, W, H, mk.face, mk.screws };
            const auto plan = planOf (unit);
            const juce::Colour steel (mk.steel), ink (mk.ink), dim (mk.dim), accent (mk.accent);
            const bool german = mk.warning == 3;

            // the steel: its colour, hammertone or crackle for the old makers, handling marks and fine scratches
            g.fillAll (steel);
            juce::Random rng ((juce::int64) unit * 31 + 5);
            if (mk.face == 1)
                for (int i = 0; i < 2600; ++i)
                {
                    const float x = rng.nextFloat() * (float) textureWidth, y = rng.nextFloat() * (float) texH, rr = (0.004f + 0.012f * rng.nextFloat()) * k;
                    g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
                    g.fillEllipse (x - rr, y - rr, 2 * rr, 2 * rr);
                }
            for (int i = 0; i < 160; ++i)
            {
                const float x = rng.nextFloat() * (float) textureWidth, y = rng.nextFloat() * (float) texH, len = (0.02f + 0.12f * rng.nextFloat()) * k;
                const float a = rng.nextFloat() * 3.14159f;
                g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f + 0.04f * rng.nextFloat()));
                g.drawLine (x, y, x + len * std::cos (a), y + len * std::sin (a), 1.0f);
            }
            if (makerOf (unit) == 9)   // SUNSET ANALOG's orange stripe along the top
            {
                g.setColour (accent);
                g.fillRect (0.0f, p.pz (-H + 0.075f), (float) textureWidth, 0.018f * k);
            }
            g.setColour (steel.darker (0.35f));
            g.drawRect (juce::Rectangle<float> (0, 0, (float) textureWidth, (float) texH), 0.014f * k);
            for (float u : { -W + 0.07f, W - 0.07f, -0.5f * W, 0.5f * W, 0.0f })
            {
                p.screw (u, -H + 0.045f);
                if (H > 0.3f || std::abs (u) > W - 0.1f)
                    p.screw (u, H - 0.045f);
            }

            // power: the way the mains comes in (an inlet, one with a switch, a cord through a grommet), the fuse,
            // the voltage selector and the ground, printed round them
            if (mk.mains == 2)
            {
                grommet (p, plan.iecU, plan.rowZ);
                p.text (german ? "NETZ ~" : "MAINS", plan.iecU, plan.rowZ - 0.16f, 0.04f, ink, true, juce::Justification::centred, 0.6f);
            }
            else
            {
                iec (p, plan.iecU, plan.rowZ);
                p.text (german ? "NETZ / AC IN" : "AC INPUT", plan.iecU, plan.rowZ - 0.5f * iecH - 0.03f, 0.04f, ink, true, juce::Justification::centred, 0.7f);
                if (mk.mains == 1)
                    rocker (p, plan.switchU, plan.rowZ, ink);
            }
            {
                const float r = 0.10f * k, cx = p.px (plan.fuseU), cy = p.pz (plan.rowZ);
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillEllipse (cx - r + 0.012f * k, cy - r + 0.016f * k, 2 * r, 2 * r);
                g.setGradientFill ({ juce::Colour (0xff3a3b3f), cx, cy - r, juce::Colour (0xff0d0d0f), cx, cy + r, false });
                g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
                g.setColour (juce::Colour (0xff1e1f22));
                g.fillEllipse (cx - 0.7f * r, cy - 0.7f * r, 1.4f * r, 1.4f * r);
                g.setColour (juce::Colour (0xff050506));
                g.fillRect (cx - 0.55f * r, cy - 0.09f * r, 1.1f * r, 0.18f * r);
                p.text (german ? "SICHERUNG" : "FUSE", plan.fuseU, plan.rowZ - 0.15f, 0.036f, ink, true, juce::Justification::centred, 0.5f);
                p.text ("T500mAL", plan.fuseU, plan.rowZ + 0.14f, 0.032f, dim, false, juce::Justification::centred, 0.4f);
            }
            {
                const auto win = p.around (plan.selU, plan.rowZ, 0.10f, 0.065f);
                g.setColour (juce::Colour (0xff0a0a0b));
                g.fillRoundedRectangle (win.expanded (0.012f * k), 0.01f * k);
                g.setColour (juce::Colour (0xffd6d0bf));
                g.fillRect (win.reduced (0.012f * k));
                g.setColour (juce::Colour (0xff141414));
                g.setFont (font (win.getHeight() * 0.55f, true));
                g.drawText (mk.powerRight ? "115V" : "230V", win, juce::Justification::centred);
                p.text ("VOLTAGE", plan.selU, plan.rowZ - 0.12f, 0.032f, ink, true, juce::Justification::centred, 0.4f);
                p.text ("SELECT", plan.selU, plan.rowZ + 0.12f, 0.032f, dim, false, juce::Justification::centred, 0.4f);
            }
            {
                const float r = 0.055f * k, cx = p.px (plan.groundU), cy = p.pz (plan.rowZ);
                if (mk.ground == 1)   // a binding post: a knurled black knob
                {
                    g.setGradientFill ({ juce::Colour (0xff4a4b50), cx - r, cy - r, juce::Colour (0xff0a0a0b), cx + r, cy + r, false });
                    g.fillEllipse (cx - 1.2f * r, cy - 1.2f * r, 2.4f * r, 2.4f * r);
                    g.setColour (juce::Colour (0xff6a6b70));
                    for (int i = 0; i < 16; ++i)
                    {
                        const float an = (float) i * 0.3927f;
                        g.drawLine (cx + 0.9f * r * std::cos (an), cy + 0.9f * r * std::sin (an), cx + 1.2f * r * std::cos (an), cy + 1.2f * r * std::sin (an), 1.0f);
                    }
                }
                else
                {
                    g.setGradientFill ({ juce::Colour (0xfff0d79a), cx - r, cy - r, juce::Colour (0xff7a5e22), cx + r, cy + r, false });
                    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
                    g.setColour (juce::Colour (0xff4d3a12));
                    g.drawEllipse (cx - 0.5f * r, cy - 0.5f * r, r, r, 0.004f * k);
                }
                const float sx = cx, sy = p.pz (plan.rowZ + 0.11f), s = 0.03f * k;
                g.setColour (ink);
                g.drawLine (sx, sy - s, sx, sy, 0.004f * k);
                g.drawLine (sx - s, sy, sx + s, sy, 0.004f * k);
                g.drawLine (sx - 0.65f * s, sy + 0.35f * s, sx + 0.65f * s, sy + 0.35f * s, 0.004f * k);
                g.drawLine (sx - 0.3f * s, sy + 0.7f * s, sx + 0.3f * s, sy + 0.7f * s, 0.004f * k);
                p.text ("GND", plan.groundU, plan.rowZ - 0.11f, 0.032f, ink, true, juce::Justification::centred, 0.3f);
            }

            // audio: the XLRs, labelled; the 1/4" jacks on tall units
            if (plan.audio)
            {
                const bool combo = mk.mains == 1 && makerOf (unit) == 3;   // KESTREL: combo inputs
                for (int i = 0; i < 4; ++i)
                    xlr (p, plan.xlrU[i], plan.xlrZ[i], i < 2, combo, mk.steel < 0xff303030u);
                for (int pair = 0; pair < 2; ++pair)
                {
                    const float uL = plan.xlrU[pair * 2], uR = plan.xlrU[pair * 2 + 1], z = plan.xlrZ[pair * 2];
                    const float labelZ = plan.tall ? z - 0.5f * xlrH - 0.06f : z + 0.5f * xlrH;
                    const juce::String name = pair == 0 ? (german ? "EINGANG / INPUT" : "INPUT") : (german ? "AUSGANG / OUTPUT" : "OUTPUT");
                    p.text (name, 0.5f * (uL + uR), labelZ, 0.040f, ink, true, juce::Justification::centred, 0.8f);
                    const float sL = uL < uR ? -1.0f : 1.0f;
                    p.text ("L", uL + sL * (0.5f * xlrW + 0.035f), z, 0.040f, ink, true, juce::Justification::centred, 0.1f);
                    p.text ("R", uR - sL * (0.5f * xlrW + 0.035f), z, 0.040f, ink, true, juce::Justification::centred, 0.1f);
                }
                if (plan.tall)
                {
                    p.text ("BALANCED +4 dBu  -  PIN 2 HOT", 0.5f * (plan.xlrU[2] + plan.xlrU[3]), plan.xlrZ[2] + 0.5f * xlrH + 0.05f, 0.028f,
                            dim, false, juce::Justification::centred, 1.0f);
                    if (plan.trs)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            trs (p, plan.trsU, plan.trsZ[i]);
                            p.text (i == (mk.outputsFirst ? 1 : 0) ? "LINK IN" : "LINK OUT", plan.trsU, plan.trsZ[i] - 0.15f, 0.034f, ink, true, juce::Justification::centred, 0.4f);
                        }
                        p.text ("UNBAL -10", plan.trsU, std::max (plan.trsZ[0], plan.trsZ[1]) + 0.15f, 0.026f, dim, false, juce::Justification::centred, 0.4f);
                    }
                }
            }
            else
            {
                const float u = mk.powerRight ? -(W - 0.6f) : W - 0.6f;
                const auto btn = p.around (u, plan.rowZ, 0.09f, 0.09f);
                g.setColour (juce::Colour (0xff0b0b0c));
                g.fillEllipse (btn.expanded (0.02f * k));
                g.setGradientFill ({ juce::Colour (0xffd9d9d6), btn.getX(), btn.getY(), juce::Colour (0xff77787a), btn.getRight(), btn.getBottom(), false });
                g.fillEllipse (btn);
                p.text ("CIRCUIT BREAKER  15A", u, plan.rowZ - 0.16f, 0.036f, ink, true, juce::Justification::centred, 0.9f);
                p.text ("PUSH TO RESET", u, plan.rowZ + 0.16f, 0.03f, dim, false, juce::Justification::centred, 0.9f);
            }

            warningLabel (p, plan.warning, mk.warning, ink);
            serialPlate (p, plan.serial, unit, mk);
            if (plan.hasBrand)
                badge (p, p.rect (plan.brand), mk, unit);
            if (plan.hasVents)
                vents (p, plan.vents, mk.vents);
        }
        return toRaw (img, true);   // mirrored: the plate is read from behind
    }

    //==============================================================================
    namespace bay
    {
        float halfW()  { return faceHalfW; }
        float faceY()  { return plateY() - 0.002f; }
        static constexpr float pitch = 0.094f;   // between columns (a 96-point TT bay across the 19 inches)
        float jackX (int column) { return -(((float) column - 0.5f * (float) (columns - 1)) * pitch); }
        float rowZ (int row) { return row == 0 ? -0.075f : 0.105f; }

        std::vector<int> unitsInOrder()
        {
            // (the LUNCHBOX, beside the rack, has its pair too: after CUSTOM, where it runs)
            std::vector<int> v;
            for (int u : rackOrder)
            {
                if (u == scopeUnit && ! isStored (lunchboxUnit) && (int) v.size() < rackColumn / 2)
                    v.push_back (lunchboxUnit);
                if (! isStored (u) && u != powerUnit && (int) v.size() < rackColumn / 2)
                    v.push_back (u);
            }
            return v;
        }

        bool jackOf (const std::vector<int>& chain, enh::patch::End e, int& col, int& row)
        {
            if (! e.plugged())
                return false;
            row = e.isOut() ? 0 : 1;
            if (e.unit <= enh::patch::spareBase)
                col = enh::patch::spareBase - e.unit;
            else if (e.unit == enh::patch::rack)
                col = rackColumn + e.channel();
            else
            {
                const auto it = std::find (chain.begin(), chain.end(), (int) e.unit);
                if (it == chain.end())
                    return false;
                col = 2 * (int) (it - chain.begin()) + e.channel();
            }
            return col >= 0 && col < columns;
        }

        bool canGoInto (const enh::patch::State& s, const std::vector<int>& chain, int cord, int end, int col, int row)
        {
            if (cord < 0 || cord >= (int) s.cords.size() || ! enh::patch::isFree (s, portAt (chain, col, row)))
                return false;
            if (s.anything)
                return true;
            const auto& c = s.cords[(size_t) cord];
            const auto& other = end == 0 ? c.b : c.a;
            return ! other.plugged() || (other.isOut() ? row == 1 : row == 0);
        }

        enh::patch::End portAt (const std::vector<int>& chain, int col, int row)
        {
            const int ch = col & 1;
            const auto port = (int8_t) ((row == 0 ? enh::patch::outL : enh::patch::inL) + ch);
            if (col >= rackColumn)
                return { (int16_t) enh::patch::rack, port };
            const size_t i = (size_t) (col / 2);
            if (i < chain.size())
                return { (int16_t) chain[i], port };
            return { (int16_t) (enh::patch::spareBase - col), port };
        }

        /** A unit's short name for the scribble strip: its first word, or two short ones. */
        static juce::String scribble (int unit)
        {
            auto words = juce::StringArray::fromTokens (juce::String (unitInfo[(size_t) unit].name), " -", "");
            words.removeEmptyStrings();
            juce::String s = words[0];
            if (words.size() > 1 && s.length() + words[1].length() <= 7)
                s << " " << words[1];
            return s.substring (0, 8);
        }

        artwork::RawTexture renderFace (int textureWidth) { return toRaw (renderFaceImage (textureWidth), true); }

        juce::Image renderFaceImage (int textureWidth)
        {
            const float W = halfW(), H = bayHalfH;
            const float k = (float) textureWidth / (2.0f * W);
            const int texH = (int) std::lround (2.0f * H * k);
            juce::Image img (juce::Image::ARGB, textureWidth, texH, true, juce::SoftwareImageType());
            {
                juce::Graphics g (img);
                const Pen p { g, k, W, H };
                // black anodised, faint brushing; the ears with their slots
                g.fillAll (juce::Colour (0xff141518));
                juce::Random rng (77);
                for (int i = 0; i < 400; ++i)
                {
                    const float y = rng.nextFloat() * (float) texH;
                    g.setColour (juce::Colours::white.withAlpha (0.015f + 0.02f * rng.nextFloat()));
                    g.drawHorizontalLine ((int) y, rng.nextFloat() * (float) textureWidth * 0.5f, (float) textureWidth * (0.5f + 0.5f * rng.nextFloat()));
                }
                for (float side : { -1.0f, 1.0f })
                {
                    const float u = side * (W - 0.09f);
                    for (float z : { -H + 0.13f, H - 0.13f })
                    {
                        g.setColour (juce::Colour (0xff050506));
                        g.fillRoundedRectangle (p.around (u, z, 0.05f, 0.03f), 0.03f * k);
                        p.screw (u, z, 0.028f);
                    }
                }

                const auto chain = unitsInOrder();
                const juce::Colour ink (0xffeceae4), dim (0xff8f9196);
                auto colU = [] (int c) { return -jackX (c); };   // seen from behind

                // the scribble strips: white, above the top row and below the bottom one, the names in marker
                for (int r = 0; r < 2; ++r)
                {
                    const float z = r == 0 ? rowZ (0) - 0.105f : rowZ (1) + 0.105f;
                    const auto strip = p.rect (colU (0) - 0.5f * pitch, z - 0.032f, colU (columns - 1) + 0.5f * pitch, z + 0.032f);
                    g.setColour (juce::Colour (0xfff3f1ea));
                    g.fillRect (strip);
                    g.setColour (juce::Colour (0xffb9b7b0));
                    for (int c = 0; c <= columns; c += 2)
                    {
                        const float x = p.px (colU (0) - 0.5f * pitch + (float) c * pitch);
                        g.drawVerticalLine ((int) x, strip.getY(), strip.getBottom());
                    }
                    for (size_t i = 0; i < chain.size(); ++i)
                    {
                        const float u = colU ((int) i * 2) + 0.5f * pitch;
                        const juce::String name = scribble (chain[i]);
                        g.setColour (juce::Colour (0xff1d2a6b));   // blue marker
                        g.setFont (font (0.034f * k, true));
                        g.drawFittedText (name, p.around (u, z, pitch - 0.006f, 0.03f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
                    }
                    g.setColour (juce::Colour (0xffa3201b));   // the rack's own ends, in red marker
                    g.setFont (font (0.034f * k, true));
                    g.drawFittedText (r == 0 ? "RACK IN" : "RACK OUT", p.around (colU (rackColumn) + 0.5f * pitch, z, pitch - 0.006f, 0.03f).toNearestInt(),
                                      juce::Justification::centred, 1, 0.6f);
                }
                // OUT / IN beside the rows, L R under the columns, numbers between the rows
                p.text ("OUT", colU (0) - 0.5f * pitch - 0.10f, rowZ (0), 0.034f, ink, true, juce::Justification::centred, 0.2f);
                p.text ("IN", colU (0) - 0.5f * pitch - 0.10f, rowZ (1), 0.034f, ink, true, juce::Justification::centred, 0.2f);
                for (int c = 0; c < columns; ++c)
                {
                    const float u = colU (c), zm = 0.5f * (rowZ (0) + rowZ (1));
                    p.text (juce::String (c + 1), u, zm, 0.026f, dim, false, juce::Justification::centred, pitch);
                    // the bantam jack: a nickel bush with a dark bore, a shadow under it
                    for (int r = 0; r < 2; ++r)
                    {
                        const float cx = p.px (u), cy = p.pz (rowZ (r)), ro = 0.030f * k, ri = 0.016f * k;
                        g.setColour (juce::Colours::black.withAlpha (0.5f));
                        g.fillEllipse (cx - ro + 0.004f * k, cy - ro + 0.006f * k, 2 * ro, 2 * ro);
                        g.setGradientFill ({ juce::Colour (0xffe4e5e8), cx - ro, cy - ro, juce::Colour (0xff5a5c61), cx + ro, cy + ro, false });
                        g.fillEllipse (cx - ro, cy - ro, 2 * ro, 2 * ro);
                        g.setColour (juce::Colour (0xff020203));
                        g.fillEllipse (cx - ri, cy - ri, 2 * ri, 2 * ri);
                    }
                }
                p.text ("ENH  TT-96  PATCH BAY", W - 0.36f, H - 0.035f, 0.026f, dim, true, juce::Justification::centred, 0.7f);
                // the lamp's bezel and its legend (the lamp itself is lit by the renderer)
                {
                    const float cx = p.px (-lampX), cy = p.pz (lampZ), r = 0.040f * k;
                    g.setGradientFill ({ juce::Colour (0xffd8d9dc), cx - r, cy - r, juce::Colour (0xff4a4c50), cx + r, cy + r, false });
                    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
                    p.text ("RED: CHAIN OPEN - SILENT    AMBER: LOOP", -lampX + 0.07f, lampZ, 0.028f, ink, true, juce::Justification::centredLeft, 1.4f);
                    p.text ("PATCH", -lampX - 0.07f, lampZ, 0.028f, dim, true, juce::Justification::centredRight, 0.5f);
                }
            }
            return img;
        }

        artwork::RawTexture renderFront (int textureWidth)
        {
            const float W = halfW(), H = bayHalfH;
            const float k = (float) textureWidth / (2.0f * W);
            const int texH = (int) std::lround (2.0f * H * k);
            juce::Image img (juce::Image::ARGB, textureWidth, texH, true, juce::SoftwareImageType());
            {
                juce::Graphics g (img);
                const Pen p { g, k, W, H };
                g.fillAll (juce::Colour (0xff17181b));
                juce::Random rng (91);
                for (int i = 0; i < 400; ++i)
                {
                    const float y = rng.nextFloat() * (float) texH;
                    g.setColour (juce::Colours::white.withAlpha (0.015f + 0.02f * rng.nextFloat()));
                    g.drawHorizontalLine ((int) y, rng.nextFloat() * (float) textureWidth * 0.5f, (float) textureWidth * (0.5f + 0.5f * rng.nextFloat()));
                }
                for (float side : { -1.0f, 1.0f })
                    for (float z : { -H + 0.13f, H - 0.13f })
                        p.screw (side * (W - 0.09f), z, 0.028f);
                // a field of round vent holes, and the print
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 30; ++c)
                    {
                        const float u = -1.1f + (float) c * 0.075f + ((r & 1) ? 0.0375f : 0.0f), z = -0.11f + (float) r * 0.075f;
                        const float cx = p.px (u), cy = p.pz (z), rr = 0.022f * k;
                        g.setColour (juce::Colour (0xff030304));
                        g.fillEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr);
                    }
                p.text ("ENH", -1.85f, -0.02f, 0.11f, juce::Colour (0xffe8e6e0), true, juce::Justification::centred, 0.6f);
                p.text ("PATCH BAY", -1.85f, 0.09f, 0.04f, juce::Colour (0xff9a9ca0), true, juce::Justification::centred, 0.6f);
                p.text ("96-POINT TT  -  JACKS AT THE BACK", 1.55f, -0.03f, 0.036f, juce::Colour (0xffc9c7c1), true, juce::Justification::centred, 1.5f);
                p.text ("TURN THE RACK ROUND TO PATCH  (RIGHT-CLICK, OR DRAG ON EMPTY SPACE)", 1.55f, 0.05f, 0.026f, juce::Colour (0xff8f9196), false, juce::Justification::centred, 1.8f);
            }
            return toRaw (img, false);
        }
    }

    artwork::RawTexture renderMasterPlate (int textureWidth, float W, float H, float leverX)
    {
        const float k = (float) textureWidth / (2.0f * W);
        const int texH = (int) std::lround (2.0f * H * k);
        juce::Image img (juce::Image::ARGB, textureWidth, texH, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            const Pen p { g, k, W, H };
            g.fillAll (juce::Colour (0xff121316));
            g.setColour (juce::Colour (0xff2a2b2f));
            g.drawRect (juce::Rectangle<float> (0, 0, (float) textureWidth, (float) texH), 0.012f * k);
            for (float u : { -W + 0.05f, W - 0.05f })
                for (float z : { -H + 0.05f, H - 0.05f })
                    p.screw (u, z, 0.024f);
            const juce::Colour ink (0xffeceae4), dim (0xff8f9196), amber (0xfff0b030);
            p.text ("MASTER", -0.20f, -0.14f, 0.07f, ink, true, juce::Justification::centred, 0.8f);
            p.text ("ANYTHING INTO ANYTHING", -0.20f, -0.04f, 0.040f, amber, true, juce::Justification::centred, 0.9f);
            p.text ("ANY PLUG INTO ANY JACK - OUT INTO OUT, LOOPS", -0.20f, 0.04f, 0.024f, dim, false, juce::Justification::centred, 0.9f);
            p.text ("FEEDBACK EAR-GUARDED: A RUNAWAY LOOP MUTES", -0.20f, 0.09f, 0.024f, dim, false, juce::Justification::centred, 0.9f);
            p.text ("ON", leverX, -0.17f, 0.04f, ink, true, juce::Justification::centred, 0.2f);
            p.text ("OFF", leverX, 0.17f, 0.04f, ink, true, juce::Justification::centred, 0.2f);
            // the lever's escutcheon: a ring round where it comes through
            const float cx = p.px (leverX), cy = p.pz (0.0f), r = 0.075f * k;
            g.setColour (juce::Colour (0xff3a3b40));
            g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 0.008f * k);
        }
        return toRaw (img, false);
    }

    artwork::RawTexture renderFlagAtlas (const std::vector<juce::String>& texts, int cellW, int cellH, int cols)
    {
        const int rows = juce::jmax (1, ((int) texts.size() + cols - 1) / cols);
        juce::Image img (juce::Image::ARGB, cellW * cols, cellH * rows, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            g.fillAll (juce::Colour (0xffe9e2cc));
            juce::Random rng (17);
            for (size_t i = 0; i < texts.size(); ++i)
            {
                const auto cell = juce::Rectangle<float> ((float) ((int) i % cols * cellW), (float) ((int) i / cols * cellH), (float) cellW, (float) cellH);
                // masking tape: creamy, a little uneven, its crepe in faint lines
                g.setColour (juce::Colour (0xffece4cb).withMultipliedBrightness (0.94f + 0.08f * rng.nextFloat()));
                g.fillRect (cell);
                for (int l = 0; l < 40; ++l)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.025f));
                    const float x = cell.getX() + rng.nextFloat() * cell.getWidth();
                    g.drawLine (x, cell.getY(), x + (rng.nextFloat() - 0.5f) * 6.0f, cell.getBottom(), 1.0f);
                }
                // the writing: a marker's hand, leaning, its size and slant a little different each time
                g.setColour (rng.nextFloat() < 0.7f ? juce::Colour (0xff15161c) : juce::Colour (0xff142a7a));
                g.setFont (juce::Font (juce::FontOptions().withHeight ((float) cellH * (0.52f + 0.08f * rng.nextFloat())).withStyle ("Bold Italic")));
                const auto t = juce::AffineTransform::rotation ((rng.nextFloat() - 0.5f) * 0.05f, cell.getCentreX(), cell.getCentreY());
                juce::GlyphArrangement ga;
                ga.addFittedText (g.getCurrentFont(), texts[i], cell.getX() + 0.06f * (float) cellW, cell.getY(), 0.88f * (float) cellW, (float) cellH,
                                  juce::Justification::centredLeft, 1, 0.7f);
                ga.draw (g, t);
            }
        }
        return toRaw (img, false);
    }
}
