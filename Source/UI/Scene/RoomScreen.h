#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "../../DSP/units/RoomScene.h"

/*  RAY ROOM's screen, white on black: the room from above - its walls, the two speakers, you - the rays of
    sound bouncing off the walls (thick where their band is loud, reaching further the bigger the room and the
    louder the track), and the crackle and the pops in flight. Worked out here as lines and blobs in the
    screen's pixels; the 3D rack stamps them into its display's glow, the 2D rack draws them with JUCE. */
namespace pad::roomscreen
{
    struct Line { float x0, y0, x1, y1, thick, bright; };
    struct Blob { float x, y, r, bright; bool ring; };

    inline void build (const enh::dsp::units::room::State& s, float time, float w, float h, std::vector<Line>& lines, std::vector<Blob>& blobs)
    {
        namespace R = enh::dsp::units::room;
        lines.clear(); blobs.clear();
        // a fixed scale: the biggest room fills the screen, a small one sits small in its middle
        const float scale = std::min (w * 0.47f / R::maxHalfW, h * 0.45f / (R::maxHalfW * R::aspect));
        const float cx = 0.5f * w, cy = 0.5f * h;
        auto X = [&] (float x) { return cx + x * scale; };
        auto Y = [&] (float y) { return cy + y * scale; };
        const float px = std::max (1.0f, w / 480.0f);   // (line widths grow with the screen's resolution)

        // the walls
        const float W = s.W, D = s.D;
        lines.push_back ({ X (-W), Y (-D), X (W), Y (-D), 2.0f * px, 0.9f });
        lines.push_back ({ X (W), Y (-D), X (W), Y (D), 2.0f * px, 0.9f });
        lines.push_back ({ X (W), Y (D), X (-W), Y (D), 2.0f * px, 0.9f });
        lines.push_back ({ X (-W), Y (D), X (-W), Y (-D), 2.0f * px, 0.9f });

        // the rays
        static thread_local std::vector<R::Stroke> strokes;
        R::rays (s, time, strokes);
        for (const auto& st : strokes)
            lines.push_back ({ X (st.x0), Y (st.y0), X (st.x1), Y (st.y1), (0.8f + 3.2f * st.thick) * px, st.bright * 0.85f });

        // the speakers (a box, the driver in it, pulsing with its side's level) and you
        for (int side = 0; side < 2; ++side)
        {
            const auto sp = R::speaker (side, W, D);
            const float half = 0.45f * scale * std::max (0.6f, W / 6.0f) * 0.5f;
            const float x0 = X (sp.x) - half, x1 = X (sp.x) + half, y0 = Y (sp.y) - half, y1 = Y (sp.y) + half;
            lines.push_back ({ x0, y0, x1, y0, 1.6f * px, 1.0f }); lines.push_back ({ x1, y0, x1, y1, 1.6f * px, 1.0f });
            lines.push_back ({ x1, y1, x0, y1, 1.6f * px, 1.0f }); lines.push_back ({ x0, y1, x0, y0, 1.6f * px, 1.0f });
            const float lv = std::clamp (side == 0 ? s.levelL : s.levelR, 0.0f, 1.0f);
            blobs.push_back ({ X (sp.x), Y (sp.y), half * (0.45f + 0.25f * lv), 1.0f, true });
        }
        const auto me = R::listener (W, D);
        const float head = std::max (3.0f * px, 0.35f * scale);
        blobs.push_back ({ X (me.x), Y (me.y), head, 0.95f, true });
        lines.push_back ({ X (me.x), Y (me.y) - head, X (me.x), Y (me.y) - head * 1.6f, 1.4f * px, 0.95f });   // (facing the speakers)

        // the crackle (small, bright) and the pops (bigger, with a ring), fading as they age
        for (int d = 0; d < s.dots; ++d)
        {
            const float x = s.dot[(size_t) (d * 4)], y = s.dot[(size_t) (d * 4 + 1)], kind = s.dot[(size_t) (d * 4 + 2)], age = s.dot[(size_t) (d * 4 + 3)];
            const float b = std::clamp (1.0f - 0.6f * age, 0.2f, 1.0f);
            if (kind > 0.5f) { blobs.push_back ({ X (x), Y (y), 3.2f * px, b, false }); blobs.push_back ({ X (x), Y (y), 6.0f * px, b * 0.6f, true }); }
            else blobs.push_back ({ X (x), Y (y), 1.6f * px, b, false });
        }
    }
}
