#include "PatchCables.h"
#include "BackPanels.h"
#include "CablePhysics.h"

namespace pad::geo::patch
{
    using gfx::Vec3;
    using gfx::Mat4;
    using namespace layout;
    using namespace hwk::geo;

    namespace
    {
        constexpr float xlrCableR = 0.040f, iecCableR = 0.046f, cordR = 0.022f;
        constexpr float xlrShellR = 0.095f, xlrShellLen = 0.30f, xlrBootLen = 0.20f;
        constexpr float iecLen = 0.30f, iecBootLen = 0.26f;
        constexpr float ttFerruleLen = 0.07f, ttBodyLen = 0.30f;
        constexpr float loomDepth = 0.80f;   // behind the backs, just past the plugs (tied to the lacing bar there)
        constexpr float lacingDepth = 0.72f, lacingX = chassisHalfW + 0.06f;   // the rear lacing bars, one each side

        /** A frame at `at` whose +y runs along `dir` (x stays level). */
        Mat4 along (Vec3 at, Vec3 dir)
        {
            const Vec3 y = normalise (dir);
            const Vec3 x = normalise (cross (std::abs (y.y) < 0.9f ? Vec3 { 0.0f, 1.0f, 0.0f } : Vec3 { 1.0f, 0.0f, 0.0f }, y));
            const Vec3 z = cross (x, y);
            Mat4 m = Mat4::identity();
            m.at (0, 0) = x.x; m.at (1, 0) = x.y; m.at (2, 0) = x.z;
            m.at (0, 1) = y.x; m.at (1, 1) = y.y; m.at (2, 1) = y.z;
            m.at (0, 2) = z.x; m.at (1, 2) = z.y; m.at (2, 2) = z.z;
            m.at (0, 3) = at.x; m.at (1, 3) = at.y; m.at (2, 3) = at.z;
            return m;
        }

        /** A ribbed boot from `r0` at y0 down to a cable of `rc` at y0 + len (outsets from `base`). */
        std::vector<ProfilePoint> boot (float base, float y0, float len, float r0, float rc, int ribs)
        {
            std::vector<ProfilePoint> p;
            for (int k = 0; k <= ribs; ++k)
            {
                const float t = (float) k / (float) ribs, y = y0 + len * t;
                const float r = r0 + (rc + 0.004f - r0) * t * (2.0f - t);
                p.push_back ({ r - base, y });
                if (k < ribs) p.push_back ({ r + 0.005f - base, y + 0.5f * len / (float) ribs });
            }
            return p;
        }

        struct Parts
        {
            MeshData xlrShell, xlrBoot, iecBody, iecBoot, ttFerrule, ttBody;
            MeshData xlrBarrel, trsNut, iecInlet, grommet, cordRelief, rockerBody, rockerCap, fuseCap, lug, post;
            Parts()
            {
                // what is mounted on the backs, standing out of them (+y out)
                xlrBarrel = lathe (0.118f, { { -0.012f, 0.0f }, { 0.0f, 0.004f }, { 0.0f, 0.018f }, { -0.014f, 0.024f } }, 28, false);
                trsNut = sweptPolygon (6, 0.095f, { { 0.0f, 0.0f }, { 0.0f, 0.028f }, { -0.012f, 0.034f } }, true);
                trsNut.append (lathe (0.045f, { { 0.0f, 0.034f }, { 0.0f, 0.06f }, { -0.006f, 0.064f } }, 18, false));
                iecInlet = box ({ -0.31f, 0.0f, -0.15f }, { 0.31f, 0.022f, 0.15f });
                grommet = lathe (0.11f, { { -0.012f, 0.0f }, { 0.0f, 0.012f }, { -0.02f, 0.03f }, { -0.045f, 0.036f } }, 24, false);
                cordRelief = lathe (0.06f, boot (0.06f, 0.0f, 0.34f, 0.058f, iecCableR, 6), 18, false);
                rockerBody = box ({ -0.08f, 0.0f, -0.13f }, { 0.08f, 0.02f, 0.13f });
                rockerCap = box ({ -0.06f, 0.0f, -0.11f }, { 0.06f, 0.03f, 0.0f });
                rockerCap.append (box ({ -0.06f, 0.0f, 0.0f }, { 0.06f, 0.05f, 0.11f }));
                Relief knurlCap; knurlCap.count = 24; knurlCap.depth = 0.08f; knurlCap.yFrom = 0.01f; knurlCap.yTo = 0.07f; knurlCap.sharpness = 0.7f;
                fuseCap = lathe (0.07f, { { 0.0f, 0.0f }, { 0.0f, 0.075f }, { -0.012f, 0.085f } }, 24, true, knurlCap);
                lug = sweptPolygon (6, 0.05f, { { 0.0f, 0.0f }, { 0.0f, 0.03f }, { -0.008f, 0.036f } }, true);
                lug.append (lathe (0.022f, { { 0.0f, 0.036f }, { 0.0f, 0.10f }, { -0.006f, 0.106f } }, 14, true));
                Relief knurlPost; knurlPost.count = 20; knurlPost.depth = 0.10f; knurlPost.yFrom = 0.02f; knurlPost.yTo = 0.11f; knurlPost.sharpness = 0.6f;
                post = lathe (0.065f, { { 0.0f, 0.0f }, { 0.0f, 0.12f }, { -0.015f, 0.135f } }, 22, true, knurlPost);

                Relief knurl; knurl.count = 30; knurl.depth = 0.05f; knurl.yFrom = 0.13f; knurl.yTo = 0.24f; knurl.sharpness = 0.9f;
                xlrShell = lathe (xlrShellR, { { -0.018f, 0.0f }, { -0.004f, 0.012f }, { 0.0f, 0.03f }, { 0.0f, xlrShellLen - 0.01f },
                                               { -0.012f, xlrShellLen } }, 28, false, knurl);
                xlrBoot = lathe (xlrShellR, boot (xlrShellR, xlrShellLen, xlrBootLen, xlrShellR - 0.012f, xlrCableR, 6), 20, false);
                iecBody = box ({ -0.17f, 0.0f, -0.12f }, { 0.17f, iecLen, 0.12f });
                iecBoot = lathe (0.09f, boot (0.09f, iecLen, iecBootLen, 0.085f, iecCableR, 5), 18, false);
                ttFerrule = lathe (0.026f, { { 0.0f, 0.0f }, { 0.0f, ttFerruleLen } }, 14, false);
                ttBody = lathe (0.036f, boot (0.036f, ttFerruleLen, ttBodyLen, 0.034f, cordR, 4), 14, false);
            }
        };

        /** Hex-packed places in a loom for its cables (offsets across it: x and depth). */
        std::pair<float, float> loomSlot (int i, float r)
        {
            // packed four across, out from the units' sides (past them, so they never hang over the connectors), then
            // row behind row
            const int across = i % 4, row = i / 4;
            return { (float) across * 2.3f * r + ((row & 1) ? 1.15f * r : 0.0f), (float) row * 2.0f * r };
        }

        /** The shelf's top, in the rack's own (unturned) space: the cables lie on it once the rack is turned round
            (it has risen by then - layout::turnMatrix - so the shelf is that much lower here). */
        float shelfY()
        {
            const float a0 = (-caseOverhang - 0.5f * totalArcLength()) / arcRadius;
            const float floorY = arcCentreY + arcRadius * std::sin (a0) - caseBoardT - 0.05f;
            const Vec3 o { 0.0f, 0.0f, 0.0f };
            const float lift = turnMatrix (1.0f).transformPoint (o).y - Mat4::identity().transformPoint (o).y;
            return floorY - lift;
        }

        /** A cable from a unit's back jack into the loom and down it, under the case. */
        std::vector<Vec3> loomRoute (const std::vector<int>& stack, size_t at, float jx, float jz, float plugLen, float loomX, int slot, float r)
        {
            const auto [dx, dd] = loomSlot (slot, r);
            const int u = stack[at];
            const Mat4 P = panelToWorld (u);
            const float y = backs::plateY();
            const float lx = loomX + (loomX < 0.0f ? -dx : dx), ld = loomDepth + dd;   // (out from the units' sides)
            std::vector<Vec3> pts;
            pts.push_back (P.transformPoint ({ jx, y - plugLen, jz }));
            pts.push_back (P.transformPoint ({ jx, y - plugLen - 0.18f, jz }));   // (straight out of the plug first)
            pts.push_back (P.transformPoint ({ 0.5f * (jx + lx), y - 0.5f * (plugLen + 0.14f + ld), jz + 0.22f }));
            pts.push_back (P.transformPoint ({ lx, y - ld, jz + 0.40f }));
            for (size_t k = at; k-- > 0;)   // down past every unit below it
                pts.push_back (panelToWorld (stack[k]).transformPoint ({ lx, y - ld, 0.0f }));
            const Mat4 B = bayToWorld();
            pts.push_back (B.transformPoint ({ lx, y - ld, 0.0f }));
            pts.push_back (B.transformPoint ({ lx, y - ld + 0.10f, bayHalfH + 0.30f }));
            pts.push_back (B.transformPoint ({ lx, y - ld + 0.55f, bayHalfH + 0.42f }));   // in under the case
            return pts;
        }
    }

    hwk::gfx::Vec3 cordColour (int k)
    {
        static const Vec3 c[numCordColours] { { 0.50f, 0.035f, 0.03f }, { 0.03f, 0.10f, 0.45f }, { 0.55f, 0.40f, 0.02f },
                                              { 0.035f, 0.30f, 0.07f }, { 0.52f, 0.52f, 0.50f } };
        return c[(size_t) (k % numCordColours)];
    }

    static std::vector<Vec3> tubePoints (const rope::Rope& r);

    Meshes buildBacks()
    {
        static const Parts parts;
        Meshes m;

        std::vector<int> stack;   // the units in the case, bottom up
        for (int u : rackOrder)
            if (isShown (u))
                stack.push_back (u);

        // A tape flag on a cable, just past its plug: a band round it, the tab out to one side, written on
        const auto chain = backs::bay::unitsInOrder();
        auto flag = [&] (const std::vector<Vec3>& route, float r, const Vec3& out, const juce::String& text, bool flip)
        {
            if (route.size() < 6) return;
            // (on a rope's particles: past its plug's straight run, where it has started to hang - there the flag's
            //  face can face out, never edge-on)
            size_t i = 4;
            for (size_t k = 4; k + 2 < route.size() && k < route.size() / 2; ++k)
                if (std::abs (normalise (route[k + 1] - route[k - 1]).y) > 0.6f) { i = k; break; }
            const Vec3 at = route[i];
            Vec3 T = normalise (route[i + 1] - route[i - 1]);
            Vec3 S = normalise (cross (out, T));
            if (flip) { S = S * -1.0f; T = T * -1.0f; }   // (the other side of the cable, its written face still out)
            const Vec3 Y = cross (T, S);
            m.tape.append (tubeAlong ({ at - T * (0.5f * flagWidth), at + T * (0.5f * flagWidth) }, r + 0.006f, 10, 0.05f));
            Mat4 f = Mat4::identity();
            f.at (0, 0) = S.x; f.at (1, 0) = S.y; f.at (2, 0) = S.z;
            f.at (0, 1) = Y.x; f.at (1, 1) = Y.y; f.at (2, 1) = Y.z;
            f.at (0, 2) = T.x; f.at (1, 2) = T.y; f.at (2, 2) = T.z;
            const Vec3 o = at + S * r;
            f.at (0, 3) = o.x; f.at (1, 3) = o.y; f.at (2, 3) = o.z;
            m.flags.push_back ({ f, text, flip });
            // its back: plain tape (the writing is on the front only)
            const float L = flagLength, hw = 0.5f * flagWidth;
            m.tape.append (quad (f.transformPoint ({ L, -0.002f, -hw }), f.transformPoint ({ L, -0.002f, hw }),
                                 f.transformPoint ({ 0.004f, -0.002f, hw }), f.transformPoint ({ 0.004f, -0.002f, -hw })));
        };
        auto shortName = [] (int u)
        {
            auto words = juce::StringArray::fromTokens (juce::String (unitInfo[(size_t) u].name), " -", "");
            words.removeEmptyStrings();
            juce::String n = words[0];
            if (words.size() > 1 && n.length() + words[1].length() <= 9) n << " " << words[1];
            return n;
        };

        // Between the units' backs it is dark (inside the case, nothing lights their lids)
        for (size_t i = 0; i < stack.size(); ++i)
        {
            const int u = stack[i];
            const Mat4 P = panelToWorld (u);
            const float h = unitHalfH (u), y = backs::plateY() - 0.002f, w = chassisHalfW + 0.02f;
            const float z0 = -h - (i + 1 < stack.size() ? rackGap * 1.5f + 0.03f : 0.0f), z1 = -(h - 0.05f);
            m.gaps.append (quad (P.transformPoint ({ w, y, z0 }), P.transformPoint ({ w, y, z1 }), P.transformPoint ({ -w, y, z1 }), P.transformPoint ({ -w, y, z0 })));
            const float b0 = h - 0.05f, b1 = h + (i == 0 ? bayArcLen - 2.0f * bayHalfH : 0.0f) + 0.006f;   // (and under the bottom one, down to the bay)
            m.gaps.append (quad (P.transformPoint ({ w, y, b0 }), P.transformPoint ({ w, y, b1 }), P.transformPoint ({ -w, y, b1 }), P.transformPoint ({ -w, y, b0 })));
        }

        // The rear lacing bars: a steel bar down each side behind the backs, following the case, that the looms are tied
        // to - and a tie round each loom at every unit
        {
            const float y = backs::plateY() - lacingDepth;
            for (float side : { -1.0f, 1.0f })
            {
                std::vector<Vec3> bar;
                for (size_t i = 0; i < stack.size(); ++i)
                {
                    const Mat4 P = panelToWorld (stack[i]);
                    if (i == 0) bar.push_back (P.transformPoint ({ side * lacingX, y, unitHalfH (stack[i]) }));
                    bar.push_back (P.transformPoint ({ side * lacingX, y, 0.0f }));
                    if (i + 1 == stack.size()) bar.push_back (P.transformPoint ({ side * lacingX, y, -unitHalfH (stack[i]) }));
                }
                m.nickel.append (tubeAlong (bar, 0.022f, 8, 0.2f));
                for (size_t i = 0; i < stack.size(); ++i)   // its stand-offs to the case, at every unit
                {
                    const Mat4 P = panelToWorld (stack[i]);
                    m.nickel.append (tubeAlong ({ P.transformPoint ({ side * lacingX, y, 0.0f }), P.transformPoint ({ side * (caseSideX + 0.05f), y + 0.12f, 0.0f }) }, 0.014f, 6, 1.0f));
                }
            }
        }

        // The backs: each unit's connectors, and its XLRs and mains cord into the loom on their side - each cable a rope,
        // tied into its loom at every unit, all of them settled together (they hang, lie on the shelf, and never go
        // through each other or into the rack)
        const float y = backs::plateY();
        int slot[2] {};   // (each loom's next place)
        struct Pending { rope::Rope rope; MeshData* into; juce::String flagText; bool flip; Vec3 out; };
        std::vector<Pending> pending;
        auto cable = [&] (const std::vector<Vec3>& route, float r, MeshData* into, const juce::String& text, bool flip, const Vec3& out)
        {
            auto rp = rope::along (route, r, 2.6f * r, 1.04f, 3, 3);
            // laced along the bar: every bit of it that runs in the loom is held there (only the run from its plug to
            // the loom hangs free)
            for (size_t i = 0; i < rp.p.size(); ++i)
                for (size_t k = 3; k + 1 < route.size(); ++k)
                {
                    const Vec3 a = route[k], ab = route[k + 1] - a;
                    const float t = std::clamp (dot (rp.p[i] - a, ab) / std::max (1.0e-6f, dot (ab, ab)), 0.0f, 1.0f);
                    if (length (rp.p[i] - (a + ab * t)) < 0.07f) { rp.pinned[i] = 1; break; }
                }
            pending.push_back ({ std::move (rp), into, text, flip, out });
        };
        for (size_t i = stack.size(); i-- > 0;)   // top down: the first in sit at the loom's heart
        {
            const int u = stack[i];
            const Mat4 P = panelToWorld (u);
            const Vec3 out = normalise (P.transformPoint ({ 0.0f, -1.0f, 0.0f }) - P.transformPoint ({ 0.0f, 0.0f, 0.0f }));
            for (const auto& j : backs::jacksOf (u))
            {
                const Vec3 at = P.transformPoint ({ j.x, y, j.z });
                const Mat4 F = along (at, out);
                const int side = j.x >= 0.0f ? 1 : 0;
                const float loomX = (side == 1 ? 1.0f : -1.0f) * (lacingX + 0.04f);
                const auto& mk = backs::makers()[(size_t) backs::makerOf (u)];
                switch (j.kind)
                {
                    case backs::Jack::xlrIn:
                    case backs::Jack::xlrOut:
                    {
                        m.nickel.append (parts.xlrBarrel, F);
                        m.nickel.append (parts.xlrShell, F);
                        m.rubber.append (parts.xlrBoot, F);
                        if (j.kind == backs::Jack::xlrOut)   // a female plug: its release tab on top
                        {
                            const Vec3 zAxis { F.at (0, 2), F.at (1, 2), F.at (2, 2) };
                            const float up = zAxis.y >= 0.0f ? 1.0f : -1.0f;
                            m.latches.append (box ({ -0.022f, 0.05f, up > 0 ? xlrShellR - 0.01f : -xlrShellR - 0.012f },
                                                   { 0.022f, 0.16f, up > 0 ? xlrShellR + 0.012f : -xlrShellR + 0.01f }), F);
                        }
                        // the cable: rubber for most; braided for the modern makers, cloth tweed for the old ones
                        const auto route = loomRoute (stack, i, j.x, j.z, xlrShellLen + xlrBootLen - 0.02f, loomX, slot[side]++, xlrCableR);
                        // its flag: its bay number (the column its own jack is in) and the unit's name, in marker
                        const auto it = std::find (chain.begin(), chain.end(), u);
                        const bool isOut = j.kind == backs::Jack::xlrOut;
                        const juce::String lr = j.channel == 0 ? "L" : "R";
                        const juce::String num = it == chain.end() ? juce::String ("-") : juce::String (2 * (int) (it - chain.begin()) + j.channel + 1);
                        MeshData* into = mk.face == 1 ? &m.braided[1] : (mk.screws == 1 || mk.face == 2) ? &m.braided[0] : &m.xlrJackets;
                        cable (route, xlrCableR, into, num + " " + shortName (u) + (isOut ? " OUT " : " IN ") + lr, (slot[side] & 1) != 0, out);
                        break;
                    }
                    case backs::Jack::iec:
                        m.rubber.append (parts.iecInlet, F);
                        m.rubber.append (parts.iecBody, F);
                        m.rubber.append (parts.iecBoot, F);
                        cable (loomRoute (stack, i, j.x, j.z, iecLen + iecBootLen - 0.02f, loomX, slot[side]++, iecCableR), iecCableR,
                               &m.iecJackets, "MAINS  " + shortName (u), false, out);
                        break;
                    case backs::Jack::cord:   // fixed through a grommet: no plug, a long strain relief
                    {
                        m.rubber.append (parts.grommet, F);
                        m.rubber.append (parts.cordRelief, F);
                        cable (loomRoute (stack, i, j.x, j.z, 0.32f, loomX, slot[side]++, iecCableR), iecCableR,
                               mk.face == 1 ? &m.braided[1] : &m.iecJackets, {}, false, out);   // (cloth-covered on the old ones)
                        break;
                    }
                    case backs::Jack::trsIn:
                    case backs::Jack::trsOut:
                        m.nickel.append (parts.trsNut, F);
                        break;
                    case backs::Jack::rocker:
                        m.rubber.append (parts.rockerBody, F);
                        m.rubber.append (parts.rockerCap, F);
                        break;
                    case backs::Jack::fuse:
                        m.rubber.append (parts.fuseCap, F);
                        break;
                    case backs::Jack::ground:
                        if (mk.ground == 1) m.rubber.append (parts.post, F); else m.brass.append (parts.lug, F);
                        break;
                }
            }
        }

        // Let them hang
        {
            std::vector<rope::Rope> ropes;
            for (auto& pd : pending) ropes.push_back (std::move (pd.rope));
            rope::settle (ropes, world(), 70);
            for (size_t k = 0; k < ropes.size(); ++k)
            {
                const auto& rp = ropes[k];
                pending[k].into->append (tubeAlong (tubePoints (rp), rp.r, 6, 1.0e3f));
                if (pending[k].flagText.isNotEmpty())
                    flag (rp.p, rp.r, pending[k].out, pending[k].flagText, pending[k].flip);
                for (const auto& q : rp.p) { m.colliders.push_back (q); m.colliderR.push_back (rp.r); }
            }
        }
        return m;
    }

    /** A rope's particles thinned for its tube (the tube's curve runs smoothly through what is left; the ends kept). */
    static std::vector<Vec3> tubePoints (const rope::Rope& r)
    {
        std::vector<Vec3> t;
        for (size_t i = 0; i < r.p.size(); i += (i < 3 || i + 4 > r.p.size()) ? 1 : 2)
            t.push_back (r.p[i]);
        if (t.back().x != r.p.back().x || t.back().y != r.p.back().y || t.back().z != r.p.back().z) t.push_back (r.p.back());
        return t;
    }

    rope::World world()
    {
        rope::World w;
        w.shelfY = shelfY();
        w.backRadius = arcRadius + unitRecess - backs::plateY() - 0.01f;
        w.axisY = arcCentreY;
        w.axisZ = arcCentreZ;
        w.halfW = chassisHalfW + 0.15f;
        const float a0 = (-caseOverhang - 0.5f * totalArcLength()) / arcRadius, a1 = (caseOverhang + 0.5f * totalArcLength()) / arcRadius;
        w.yMin = arcCentreY + (arcRadius + 1.0f) * std::sin (a0) - 0.3f;
        w.yMax = arcCentreY + arcRadius * std::sin (a1) + 0.3f;
        return w;
    }

    hwk::gfx::Vec3 jackAt (int col, int row)
    {
        return bayToWorld().transformPoint ({ backs::bay::jackX (col), backs::bay::faceY(), backs::bay::rowZ (row) });
    }

    hwk::gfx::Mat4 frameAlong (hwk::gfx::Vec3 at, hwk::gfx::Vec3 dir) { return along (at, dir); }

    hwk::gfx::Vec3 bayOut()
    {
        const Mat4 B = bayToWorld();
        return normalise (B.transformPoint ({ 0.0f, -1.0f, 0.0f }) - B.transformPoint ({ 0.0f, 0.0f, 0.0f }));
    }

    hwk::gfx::Vec3 restingBelow (int col, int row, int index)
    {
        Vec3 p = jackAt (col, row) + bayOut() * (0.55f + 0.06f * (float) (index % 3));
        p.y = shelfY() + 0.036f;
        return p;
    }

    namespace
    {
        /** Where a cord's plugs are, which way their cable leaves them, and where it leaves them. */
        struct CordEnds { Vec3 base[2], dir[2], exit[2]; };
        CordEnds endsOf (const CordDraw& c)
        {
            const Vec3 out = bayOut();
            const float plug = ttFerruleLen + ttBodyLen - 0.01f;
            CordEnds e;
            for (int k = 0; k < 2; ++k)
            {
                const auto& end = k == 0 ? c.a : c.b;
                if (end.col >= 0)
                {
                    e.dir[k] = out;
                    e.base[k] = jackAt (end.col, end.row) + out * ((1.0f - std::clamp (end.depth, 0.0f, 1.0f)) * (ttFerruleLen + 0.12f));
                }
                else
                {
                    e.dir[k] = normalise (end.dir);
                    e.base[k] = end.at;
                }
                e.exit[k] = e.base[k] + e.dir[k] * plug;
            }
            return e;
        }
    }

    rope::Rope cordRope (const CordDraw& c, int k)
    {
        const auto e = endsOf (c);
        const Vec3 out = bayOut(), down { 0.0f, -1.0f, 0.0f };
        const float span = length (e.exit[1] - e.exit[0]);
        const float lift = 0.02f * (float) (k % 4);
        const std::vector<Vec3> path { e.exit[0], e.exit[0] + e.dir[0] * 0.12f,
                                       (e.exit[0] + e.exit[1]) * 0.5f + out * (0.12f + lift + 0.03f * span) + down * (0.22f + 0.13f * span),
                                       e.exit[1] + e.dir[1] * 0.12f, e.exit[1] };
        auto r = rope::between (path, cordR, std::max (0.7f, span * 1.12f + 0.5f), 3);
        pinCord (r, c);
        return r;
    }

    void pinCord (rope::Rope& r, const CordDraw& c)
    {
        // each plug holds its cable straight for its first few particles; a cord pulled taut gives a little
        const auto e = endsOf (c);
        const size_t n = r.p.size();
        const float span = length (e.exit[1] - e.exit[0]);
        r.rest = std::max (r.natural, (span + 0.25f) / (float) (n - 1));
        for (size_t i = 0; i < 3 && i < n; ++i)
        {
            r.p[i] = r.prev[i] = e.exit[0] + e.dir[0] * (r.rest * (float) i);
            r.p[n - 1 - i] = r.prev[n - 1 - i] = e.exit[1] + e.dir[1] * (r.rest * (float) i);
        }
    }

    void cordMeshes (const rope::Rope& r, const CordDraw& c, Meshes& m)
    {
        static const Parts parts;
        const auto e = endsOf (c);
        for (int k = 0; k < 2; ++k)
        {
            const Mat4 F = along (e.base[k], e.dir[k]);
            m.nickel.append (parts.ttFerrule, F);
            m.rubber.append (parts.ttBody, F);
        }
        m.cordJackets[(size_t) (c.colour % numCordColours)].append (tubeAlong (tubePoints (r), r.r, 6, 1.0e3f));
    }

    void buildCords (const std::vector<CordDraw>& cords, Meshes& m, const std::vector<Vec3>& colliders, const std::vector<float>& colliderR,
                     std::vector<rope::Rope>& ropes)
    {
        // each cord from where it hung before when it can (the same cord, its plugs where they were), else fresh
        std::vector<rope::Rope> next;
        bool fresh = false;
        for (size_t k = 0; k < cords.size(); ++k)
        {
            auto r = cordRope (cords[k], (int) k);
            if (k < ropes.size() && ropes[k].p.size() == r.p.size())
            {
                r.p = ropes[k].p;
                r.prev = r.p;
                pinCord (r, cords[k]);
            }
            else
                fresh = true;
            next.push_back (std::move (r));
        }
        // (only the backs' cables near the cords can touch them)
        Vec3 lo { 1.0e9f, 1.0e9f, 1.0e9f }, hi { -1.0e9f, -1.0e9f, -1.0e9f };
        for (const auto& r : next)
            for (const auto& q : r.p)
            {
                lo = { std::min (lo.x, q.x), std::min (lo.y, q.y), std::min (lo.z, q.z) };
                hi = { std::max (hi.x, q.x), std::max (hi.y, q.y), std::max (hi.z, q.z) };
            }
        std::vector<Vec3> near;
        std::vector<float> nearR;
        for (size_t i = 0; i < colliders.size(); ++i)
        {
            const auto& c = colliders[i];
            if (c.x > lo.x - 0.5f && c.x < hi.x + 0.5f && c.y > lo.y - 0.5f && c.y < hi.y + 0.5f && c.z > lo.z - 0.5f && c.z < hi.z + 0.5f)
            {
                near.push_back (c);
                nearR.push_back (colliderR[i]);
            }
        }
        rope::settle (next, world(), fresh ? 80 : 35, near, nearR);
        for (size_t k = 0; k < cords.size(); ++k)
            cordMeshes (next[k], cords[k], m);
        ropes = std::move (next);
    }

    void stepCord (rope::Rope& r, const CordDraw& c, float dt, const std::vector<Vec3>& colliders, const std::vector<float>& colliderR)
    {
        pinCord (r, c);
        rope::step (r, world(), dt, colliders, colliderR);
    }

    hwk::gfx::Mat4 masterFrame()
    {
        // Standing upright on the crown's back edge (the crown leans with the arc; the plate stands plumb), facing
        // straight back - toward you when the rack is turned round
        const float s1 = totalArcLength() + caseOverhang;
        const float a = (s1 - 0.5f * totalArcLength()) / arcRadius;
        const Vec3 T { 0.0f, std::cos (a), std::sin (a) };
        const float rOut = arcRadius - (-caseDepth + 0.10f);
        const Vec3 base = Vec3 { 0.0f, arcCentreY + rOut * std::sin (a), arcCentreZ - rOut * std::cos (a) } + T * caseBoardT;
        const Vec3 at = base + Vec3 { 0.0f, masterHalfH, 0.0f };
        Mat4 m = Mat4::identity();
        m.at (0, 0) = -1.0f; m.at (1, 0) = 0.0f;  m.at (2, 0) = 0.0f;    // x across
        m.at (0, 1) = 0.0f;  m.at (1, 1) = 0.0f;  m.at (2, 1) = -1.0f;   // y out of its face: the back
        m.at (0, 2) = 0.0f;  m.at (1, 2) = -1.0f; m.at (2, 2) = 0.0f;    // z down it
        m.at (0, 3) = at.x;  m.at (1, 3) = at.y;  m.at (2, 3) = at.z;
        return m;
    }

    MeshData masterPlate()
    {
        // a folded steel stand: the plate, and a foot screwed to the crown
        MeshData m;
        m.append (box ({ -masterHalfW, -0.05f, -masterHalfH }, { masterHalfW, 0.0f, masterHalfH }));
        m.append (box ({ -masterHalfW, -0.05f, masterHalfH - 0.02f }, { masterHalfW, 0.16f, masterHalfH + 0.0f }));
        return m;
    }

    MeshData masterFace()
    {
        return quad ({ -masterHalfW, 0.001f, -masterHalfH }, { -masterHalfW, 0.001f, masterHalfH }, { masterHalfW, 0.001f, masterHalfH }, { masterHalfW, 0.001f, -masterHalfH });
    }

    MeshData masterNut()
    {
        MeshData m = sweptPolygon (6, 0.055f, { { 0.0f, 0.0f }, { 0.0f, 0.03f }, { -0.006f, 0.036f } }, true);
        m.append (lathe (0.032f, { { 0.0f, 0.03f }, { 0.0f, 0.07f }, { -0.004f, 0.075f } }, 20, true));
        MeshData out;
        out.append (m, Mat4::translation ({ masterLeverX, 0.0f, 0.0f }));
        return out;
    }

    MeshData masterLever (bool on)
    {
        // the bat: a tapered rod with a ball on the end, thrown 28 degrees up (ON) or down
        MeshData bat = lathe (0.020f, { { 0.0f, 0.0f }, { 0.0f, 0.02f }, { -0.006f, 0.20f }, { -0.006f, 0.205f } }, 16, false);
        bat.append (lathe (0.030f, { { -0.030f, 0.19f }, { -0.012f, 0.197f }, { -0.002f, 0.215f }, { -0.002f, 0.235f }, { -0.012f, 0.253f }, { -0.030f, 0.26f } }, 16, true));
        MeshData out;
        out.append (bat, Mat4::translation ({ masterLeverX, 0.06f, 0.0f }) * Mat4::rotationX ((on ? -1.0f : 1.0f) * 0.49f));
        return out;
    }
}
