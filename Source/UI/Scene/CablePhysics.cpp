#include "CablePhysics.h"
#include <algorithm>
#include <cmath>

namespace pad::geo::rope
{
    namespace
    {
        using hwk::gfx::length;
        using hwk::gfx::normalise;

        constexpr float gravity = 130.0f;   // 9.8 m/s^2 in scene units (1U = 0.59 = 44.45 mm: 13.3 a metre)
        constexpr float frameDt = 1.0f / 60.0f;

        /** The path made smooth (Catmull-Rom through its points), then walked off at `spacing`. */
        std::vector<Vec3> resample (const std::vector<Vec3>& pts, float spacing, int forceCount = 0)
        {
            std::vector<Vec3> dense;
            if (pts.size() < 2) return pts;
            auto at = [&] (int i) { return pts[(size_t) std::clamp (i, 0, (int) pts.size() - 1)]; };
            for (int i = 0; i + 1 < (int) pts.size(); ++i)
            {
                const Vec3 p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
                const int steps = std::max (2, (int) std::ceil (length (p2 - p1) / 0.02f));
                for (int k = 0; k < steps; ++k)
                {
                    const float t = (float) k / (float) steps, t2 = t * t, t3 = t2 * t;
                    dense.push_back ((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f);
                }
            }
            dense.push_back (pts.back());
            float total = 0.0f;
            for (size_t i = 1; i < dense.size(); ++i) total += length (dense[i] - dense[i - 1]);
            const int n = forceCount > 1 ? forceCount : std::max (3, (int) std::ceil (total / spacing) + 1);
            const float step = total / (float) (n - 1);
            std::vector<Vec3> out { dense.front() };
            float walked = 0.0f, next = step;
            for (size_t i = 1; i < dense.size() && (int) out.size() < n - 1; ++i)
            {
                const float seg = length (dense[i] - dense[i - 1]);
                while (seg > 0.0f && walked + seg >= next && (int) out.size() < n - 1)
                {
                    out.push_back (dense[i - 1] + (dense[i] - dense[i - 1]) * ((next - walked) / seg));
                    next += step;
                }
                walked += seg;
            }
            out.push_back (dense.back());
            return out;
        }

        float pathLength (const std::vector<Vec3>& p)
        {
            float t = 0.0f;
            for (size_t i = 1; i < p.size(); ++i) t += length (p[i] - p[i - 1]);
            return t;
        }

        /** A spatial hash of points (buckets of a fixed table, cells `cell` wide). */
        struct Grid
        {
            static constexpr int buckets = 1 << 14;
            float cell = 0.1f;
            std::vector<int> head, next;
            static unsigned key (int x, int y, int z) { return ((unsigned) x * 73856093u ^ (unsigned) y * 19349663u ^ (unsigned) z * 83492791u) & (buckets - 1); }
            int ci (float v) const { return (int) std::floor (v / cell); }
            template <typename Pos> void build (int count, Pos&& pos)
            {
                head.assign (buckets, -1);
                next.assign ((size_t) count, -1);
                for (int i = 0; i < count; ++i)
                {
                    const Vec3 q = pos (i);
                    const unsigned k = key (ci (q.x), ci (q.y), ci (q.z));
                    next[(size_t) i] = head[k];
                    head[k] = i;
                }
            }
            template <typename Fn> void near (const Vec3& q, Fn&& fn) const
            {
                const int x = ci (q.x), y = ci (q.y), z = ci (q.z);
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dz = -1; dz <= 1; ++dz)
                            for (int i = head[key (x + dx, y + dy, z + dz)]; i >= 0; i = next[(size_t) i])
                                fn (i);
            }
        };

        void keepOut (Rope& r, size_t i, const World& w)
        {
            if (r.pinned[i]) return;
            Vec3& q = r.p[i];
            // the shelf (and a little friction on it: what lies there stays)
            if (q.y < w.shelfY + r.r)
            {
                q.y = w.shelfY + r.r;
                Vec3& pv = r.prev[i];
                pv.x += (q.x - pv.x) * 0.6f;
                pv.z += (q.z - pv.z) * 0.6f;
            }
            // the rack's back: nothing nearer the arc's axis than its back plates
            if (w.backRadius > 0.0f && std::abs (q.x) < w.halfW && q.y > w.yMin && q.y < w.yMax)
            {
                const float dy = q.y - w.axisY, dz = q.z - w.axisZ, d = std::sqrt (dy * dy + dz * dz), min = w.backRadius + r.r;
                if (d < min && d > 1.0e-4f)
                {
                    q.y = w.axisY + dy * (min / d);
                    q.z = w.axisZ + dz * (min / d);
                }
            }
        }

        void lengths (Rope& r)
        {
            const size_t n = r.p.size();
            for (size_t i = 0; i + 1 < n; ++i)
            {
                const float wa = r.pinned[i] ? 0.0f : 1.0f, wb = r.pinned[i + 1] ? 0.0f : 1.0f;
                if (wa + wb == 0.0f) continue;
                const Vec3 d = r.p[i + 1] - r.p[i];
                const float len = length (d);
                if (len < 1.0e-6f) continue;
                const Vec3 corr = d * ((len - r.rest) / (len * (wa + wb)));
                r.p[i] = r.p[i] + corr * wa;
                r.p[i + 1] = r.p[i + 1] - corr * wb;
            }
            // stiffness: a cable doesn't fold - two apart stay at least most of their length apart
            for (size_t i = 0; i + 2 < n; ++i)
            {
                const float wa = r.pinned[i] ? 0.0f : 1.0f, wb = r.pinned[i + 2] ? 0.0f : 1.0f;
                if (wa + wb == 0.0f) continue;
                const Vec3 d = r.p[i + 2] - r.p[i];
                const float len = length (d), min = 1.80f * r.rest;
                if (len >= min || len < 1.0e-6f) continue;
                const Vec3 corr = d * ((len - min) / (len * (wa + wb)) * 0.5f);
                r.p[i] = r.p[i] + corr * wa;
                r.p[i + 2] = r.p[i + 2] - corr * wb;
            }
        }

        struct Ref { int rope, index; };

        /** Pushes apart whatever overlaps: the ropes' particles with each other (not their own near neighbours), and
            with the fixed colliders. */
        void collide (std::vector<Rope*>& ropes, const std::vector<Vec3>& col, const std::vector<float>& colR, const Grid* colGrid)
        {
            std::vector<Ref> refs;
            float maxR = 0.0f;
            for (int k = 0; k < (int) ropes.size(); ++k)
            {
                for (int i = 0; i < (int) ropes[(size_t) k]->p.size(); ++i) refs.push_back ({ k, i });
                maxR = std::max (maxR, ropes[(size_t) k]->r);
            }
            Grid g;
            g.cell = std::max (0.06f, 2.5f * maxR);
            g.build ((int) refs.size(), [&] (int i) { return ropes[(size_t) refs[(size_t) i].rope]->p[(size_t) refs[(size_t) i].index]; });
            for (int a = 0; a < (int) refs.size(); ++a)
            {
                Rope& ra = *ropes[(size_t) refs[(size_t) a].rope];
                const size_t ia = (size_t) refs[(size_t) a].index;
                if (ra.pinned[ia]) continue;
                g.near (ra.p[ia], [&] (int b)
                {
                    if (b <= a && ! ropes[(size_t) refs[(size_t) b].rope]->pinned[(size_t) refs[(size_t) b].index]) return;   // (each free pair once)
                    Rope& rb = *ropes[(size_t) refs[(size_t) b].rope];
                    const size_t ib = (size_t) refs[(size_t) b].index;
                    if (&ra == &rb && (ia > ib ? ia - ib : ib - ia) < 4) return;
                    const Vec3 d = rb.p[ib] - ra.p[ia];
                    const float dist = length (d), min = 1.25f * (ra.r + rb.r);
                    if (dist >= min || dist < 1.0e-6f) return;
                    const float wa = 1.0f, wb = rb.pinned[ib] ? 0.0f : 1.0f;
                    const Vec3 corr = d * ((dist - min) / (dist * (wa + wb)));
                    ra.p[ia] = ra.p[ia] + corr * wa;
                    rb.p[ib] = rb.p[ib] - corr * wb;
                });
                if (colGrid != nullptr)
                    colGrid->near (ra.p[ia], [&] (int c)
                    {
                        const Vec3 d = ra.p[ia] - col[(size_t) c];
                        const float dist = length (d), min = 1.25f * (ra.r + colR[(size_t) c]);
                        if (dist < min && dist > 1.0e-6f)
                            ra.p[ia] = ra.p[ia] + d * ((min - dist) / dist);
                    });
            }
        }

        void integrate (Rope& r, float dt, float damping)
        {
            for (size_t i = 0; i < r.p.size(); ++i)
            {
                if (r.pinned[i]) { r.prev[i] = r.p[i]; continue; }
                const Vec3 v = (r.p[i] - r.prev[i]) * damping;
                r.prev[i] = r.p[i];
                r.p[i] = r.p[i] + v + Vec3 { 0.0f, -gravity * dt * dt, 0.0f };
            }
        }

        void run (std::vector<Rope*>& ropes, const World& w, int steps, float dt, float damping,
                  const std::vector<Vec3>& col, const std::vector<float>& colR)
        {
            Grid cg;
            const Grid* colGrid = nullptr;
            if (! col.empty())
            {
                float maxR = 0.0f;
                for (float r : colR) maxR = std::max (maxR, r);
                cg.cell = std::max (0.06f, 2.5f * maxR);
                cg.build ((int) col.size(), [&] (int i) { return col[(size_t) i]; });
                colGrid = &cg;
            }
            for (int s = 0; s < steps; ++s)
            {
                for (auto* r : ropes) integrate (*r, dt, damping);
                for (int it = 0; it < 4; ++it)
                {
                    for (auto* r : ropes) lengths (*r);
                    if (it % 2 == 1) collide (ropes, col, colR, colGrid);
                    for (auto* r : ropes)
                        for (size_t i = 0; i < r->p.size(); ++i) keepOut (*r, i, w);
                }
            }
        }
    }

    Rope along (const std::vector<Vec3>& path, float r, float spacing, float slack, int pinStart, int pinEnd)
    {
        Rope rope;
        rope.r = r;
        rope.p = resample (path, spacing);
        rope.prev = rope.p;
        rope.rest = rope.natural = pathLength (rope.p) * slack / (float) (rope.p.size() - 1);
        rope.pinned.assign (rope.p.size(), 0);
        for (int i = 0; i < pinStart && i < (int) rope.p.size(); ++i) rope.pinned[(size_t) i] = 1;
        for (int i = 0; i < pinEnd && i < (int) rope.p.size(); ++i) rope.pinned[rope.p.size() - 1 - (size_t) i] = 1;
        return rope;
    }

    Rope between (const std::vector<Vec3>& path, float r, float len, int pinEnds)
    {
        Rope rope;
        rope.r = r;
        const int n = std::max (6, (int) std::ceil (len / (1.5f * r)) + 1);
        rope.p = resample (path, 1.0f, n);
        rope.prev = rope.p;
        rope.rest = rope.natural = len / (float) (rope.p.size() - 1);
        rope.pinned.assign (rope.p.size(), 0);
        for (int i = 0; i < pinEnds && i < (int) rope.p.size(); ++i)
        {
            rope.pinned[(size_t) i] = 1;
            rope.pinned[rope.p.size() - 1 - (size_t) i] = 1;
        }
        return rope;
    }

    void tie (Rope& rope, const std::vector<Vec3>& points)
    {
        for (const auto& q : points)
        {
            size_t best = 0;
            float bd = 1.0e9f;
            for (size_t i = 0; i < rope.p.size(); ++i)
                if (const float d = length (rope.p[i] - q); d < bd) { bd = d; best = i; }
            rope.pinned[best] = 1;
        }
    }

    void settle (std::vector<Rope>& ropes, const World& w, int steps, const std::vector<Vec3>& colliders, const std::vector<float>& colliderR)
    {
        std::vector<Rope*> ptrs;
        for (auto& r : ropes) ptrs.push_back (&r);
        run (ptrs, w, steps, frameDt, 0.94f, colliders, colliderR);
    }

    void step (Rope& rope, const World& w, float dt, const std::vector<Vec3>& colliders, const std::vector<float>& colliderR)
    {
        std::vector<Rope*> one { &rope };
        // (only what is near it, as colliders)
        Vec3 lo = rope.p.front(), hi = lo;
        for (const auto& q : rope.p)
        {
            lo = { std::min (lo.x, q.x), std::min (lo.y, q.y), std::min (lo.z, q.z) };
            hi = { std::max (hi.x, q.x), std::max (hi.y, q.y), std::max (hi.z, q.z) };
        }
        std::vector<Vec3> near;
        std::vector<float> nearR;
        for (size_t i = 0; i < colliders.size(); ++i)
        {
            const auto& c = colliders[i];
            if (c.x > lo.x - 0.6f && c.x < hi.x + 0.6f && c.y > lo.y - 0.6f && c.y < hi.y + 0.6f && c.z > lo.z - 0.6f && c.z < hi.z + 0.6f)
            {
                near.push_back (c);
                nearR.push_back (colliderR[i]);
            }
        }
        const int sub = 2;
        run (one, w, sub, std::min (dt, 0.05f) / (float) sub, 0.992f, near, nearR);
    }
}
