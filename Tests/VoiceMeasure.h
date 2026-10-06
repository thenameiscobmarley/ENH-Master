#pragma once
// Measuring a voice (VOCAL IDENTITY PROCESSOR): pitch, formants (LPC), how periodic, how loud. Shared by the tests
// (Tests/NewUnitsTests.h, EnhDspTests --voice) and EnhAudioLab voice, so both measure with the same tools.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace voicemeasure
{
    using Buf = std::vector<float>;
    /** Pitch of a stretch: normalised autocorrelation, parabolic peak, median of 60 ms frames (0: unvoiced). */
    inline double pitchOf (const Buf& x, double sr, double t0, double t1)
    {
        if (sr > 50000.0)   // (measured at 48 kHz at every rate: the same tool, the same reading)
        {
            Buf h (x.size() / 2);
            for (size_t i = 0; i < h.size(); ++i) h[i] = 0.25f * (2 * i > 0 ? x[2 * i - 1] : 0.0f) + 0.5f * x[2 * i] + 0.25f * x[2 * i + 1];
            return pitchOf (h, sr / 2.0, t0, t1);
        }
        std::vector<double> got;
        const int w = (int) (0.06 * sr), lagMin = (int) (sr / 600), lagMax = (int) (sr / 50);
        // (only the speech: frames within 20 dB of the loudest - a recording's quiet room tone between words is not the voice)
        double loudest = 0.0;
        for (int s = (int) (t0 * sr); s + w + lagMax < (int) std::min ((double) x.size(), t1 * sr); s += w / 2)
        { double e = 0.0; for (int i = 0; i < w; i += 4) e += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)]; loudest = std::max (loudest, 4.0 * e); }
        for (int s = (int) (t0 * sr); s + w + lagMax < (int) std::min ((double) x.size(), t1 * sr); s += w / 2)
        {
            double e0 = 0.0; for (int i = 0; i < w; ++i) e0 += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)];
            if (e0 < 1.0e-6 || e0 < 1.0e-2 * loudest) continue;
            std::vector<double> c ((size_t) lagMax + 2, 0.0);
            for (int lag = lagMin - 1; lag <= lagMax + 1; ++lag)
            {
                double xy = 0.0, e1 = 0.0;
                for (int i = 0; i < w; ++i) { xy += (double) x[(size_t) (s + i)] * x[(size_t) (s + i + lag)]; e1 += (double) x[(size_t) (s + i + lag)] * x[(size_t) (s + i + lag)]; }
                c[(size_t) lag] = xy / std::sqrt (e0 * e1 + 1.0e-12);
            }
            // the first strong peak (an octave-safe pick: within 3 % of the best one)
            double best = 0.0; for (int lag = lagMin; lag <= lagMax; ++lag) best = std::max (best, c[(size_t) lag]);
            if (best < 0.75) continue;
            for (int lag = lagMin; lag <= lagMax; ++lag)
                if (c[(size_t) lag] > 0.97 * best && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1])
                {
                    const double a = c[(size_t) lag - 1], b = c[(size_t) lag], d = c[(size_t) lag + 1], den = a - 2 * b + d;
                    const double fr = std::abs (den) > 1e-12 ? 0.5 * (a - d) / den : 0.0;
                    got.push_back (sr / (lag + fr)); break;
                }
        }
        if (got.empty()) return 0.0;
        std::sort (got.begin(), got.end());
        return got[got.size() / 2];
    }

    /** How far a voice's pitch was moved (semitones): frame by frame at the same moments of the original and the
        changed voice (60 ms frames, the best autocorrelation peak in each, both confident), the median of the
        differences. For real speech, where a clip-wide median compares different moments. */
    inline double pitchShiftSt (const Buf& x, const Buf& y, double sr, double t0, double t1)
    {
        const int w = (int) (0.06 * sr), lagMin = (int) (sr / 600), lagMax = (int) (sr / 45);
        auto best = [&] (const Buf& a, int s, double& corr) {
            double e0 = 0.0; for (int i = 0; i < w; i += 2) e0 += (double) a[(size_t) (s + i)] * a[(size_t) (s + i)];
            double bc = 0.0; int bl = 0;
            for (int lag = lagMin; lag <= lagMax; ++lag)
            {
                double xy = 0.0, e1 = 0.0;
                for (int i = 0; i < w; i += 2) { xy += (double) a[(size_t) (s + i)] * a[(size_t) (s + i + lag)]; e1 += (double) a[(size_t) (s + i + lag)] * a[(size_t) (s + i + lag)]; }
                const double c = xy / std::sqrt (e0 * e1 + 1.0e-12);
                if (c > bc) { bc = c; bl = lag; }
            }
            corr = bc; return bl > 0 ? sr / bl : 0.0; };
        double loudest = 0.0;
        const int end = (int) std::min ({ (double) x.size(), (double) y.size(), t1 * sr }) - w - lagMax - 1;
        for (int s = (int) (t0 * sr); s < end; s += w / 2) { double e = 0.0; for (int i = 0; i < w; i += 4) e += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)]; loudest = std::max (loudest, e); }
        std::vector<std::pair<double, double>> both;   // (input pitch, output pitch)
        for (int s = (int) (t0 * sr); s < end; s += w / 2)
        {
            double e = 0.0; for (int i = 0; i < w; i += 4) e += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)];
            if (e < 1.0e-2 * loudest) continue;
            double ca = 0.0, cb = 0.0;
            const double fa = best (x, s, ca), fb = best (y, s, cb);
            if (ca > 0.6 && cb > 0.6 && fa > 0.0 && fb > 0.0) both.push_back ({ fa, fb });
        }
        if (both.empty()) return 0.0;
        // (the speaker's own range only - a tone or hum in the recording, steady between the words, is not the voice)
        std::vector<double> fs; for (auto& p : both) fs.push_back (p.first);
        std::sort (fs.begin(), fs.end());
        const double mid = fs[fs.size() / 2];
        std::vector<double> d;
        for (auto& p : both) if (p.first > 0.55 * mid && p.first < 1.8 * mid) d.push_back (12.0 * std::log2 (p.second / p.first));
        if (d.empty()) return 0.0;
        std::sort (d.begin(), d.end());
        return d[d.size() / 2];
    }

    /** The pitch every 10 ms (30 ms frames): the first strong autocorrelation peak (within 10 % of the best), parabolic;
        with how sure it is (the peak's correlation). */
    struct PitchFrame { double hz = 0.0, conf = 0.0, energy = 0.0; };
    inline std::vector<PitchFrame> pitchTrack (const Buf& x, double sr, double t0, double t1)
    {
        std::vector<PitchFrame> out;
        const int w = (int) (0.03 * sr), lagMin = (int) (sr / 900), lagMax = (int) (sr / 60);
        std::vector<double> cs ((size_t) lagMax + 2);
        for (double t = t0; (t + 0.03) * sr + lagMax + 2 < std::min ((double) x.size(), t1 * sr); t += 0.01)
        {
            const int s = (int) (t * sr);
            PitchFrame f;
            double e0 = 0; for (int i = 0; i < w; ++i) e0 += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)];
            f.energy = e0;
            if (e0 < 1e-9) { out.push_back (f); continue; }
            double top = 0;
            for (int lag = lagMin - 1; lag <= lagMax + 1; ++lag)
            {
                double xy = 0, e1 = 0;
                for (int i = 0; i < w; i += 2) { xy += (double) x[(size_t) (s + i)] * x[(size_t) (s + i + lag)]; e1 += (double) x[(size_t) (s + i + lag)] * x[(size_t) (s + i + lag)]; }
                cs[(size_t) lag] = xy / std::sqrt (0.5 * e0 * e1 + 1e-12);
                if (lag >= lagMin && lag <= lagMax) top = std::max (top, cs[(size_t) lag]);
            }
            for (int lag = lagMin; lag <= lagMax; ++lag)
                if (cs[(size_t) lag] >= 0.9 * top && cs[(size_t) lag] >= cs[(size_t) lag - 1] && cs[(size_t) lag] >= cs[(size_t) lag + 1])
                {
                    const double a = cs[(size_t) lag - 1], b = cs[(size_t) lag], c = cs[(size_t) lag + 1], den = a - 2 * b + c;
                    f.hz = sr / (lag + (std::abs (den) > 1e-12 ? std::clamp (0.5 * (a - c) / den, -0.5, 0.5) : 0.0)); f.conf = b; break;
                }
            out.push_back (f);
        }
        return out;
    }
    /** How "processed" a changed voice sounds, against the original at the same moments:
        offShift - % of voiced frames whose shift strays over 1.5 st from the usual (flicker: a syllable's edge left
        at the old pitch); jitterIn/Out - the median frame-to-frame pitch change (cents) - more out than in is
        roughness the processing added; hfIn/Out - the share of energy over 4 kHz (dB) - harshness. */
    struct Realism { double offShift = 0, jitterIn = 0, jitterOut = 0, hfIn = 0, hfOut = 0; };
    inline Realism realism (const Buf& x, const Buf& y, double sr, double t0, double t1)
    {
        Realism r;
        const auto a = pitchTrack (x, sr, t0, t1), b = pitchTrack (y, sr, t0, t1);
        double loud = 0; for (auto& f : a) loud = std::max (loud, f.energy);
        std::vector<double> sh, ins, ts;
        for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
            if (a[i].energy > 0.01 * loud && a[i].conf > 0.75 && b[i].conf > 0.6 && a[i].hz > 0 && b[i].hz > 0)
                { sh.push_back (12.0 * std::log2 (b[i].hz / a[i].hz)); ins.push_back (a[i].hz); ts.push_back (t0 + 0.01 * (double) i); }
        // (the speaker's own range only: 0.55x .. 1.8x their median pitch - a recording's tones and a fricative's
        //  periodicity are not the voice)
        if (ins.size() > 10)
        {
            auto si = ins; std::sort (si.begin(), si.end());
            const double mid = si[si.size() / 2];
            std::vector<double> sh2, ins2, ts2;
            for (size_t i = 0; i < ins.size(); ++i) if (ins[i] > 0.55 * mid && ins[i] < 1.8 * mid) { sh2.push_back (sh[i]); ins2.push_back (ins[i]); ts2.push_back (ts[i]); }
            sh.swap (sh2); ins.swap (ins2); ts.swap (ts2);
        }
        if (sh.size() > 10)
        {
            auto sorted = sh; std::sort (sorted.begin(), sorted.end());
            const double med = sorted[sorted.size() / 2];
            // (a flicker: a frame off its neighbours' shift - the local median over +-7 frames - by over 1.5 st; the shift
            //  itself may move, with intonation, and that is not counted)
            int off = 0;
            for (size_t i = 0; i < sh.size(); ++i)
            {
                std::vector<double> nb;
                for (size_t j = (i >= 7 ? i - 7 : 0); j < std::min (sh.size(), i + 8); ++j) if (std::abs (ts[j] - ts[i]) < 0.075) nb.push_back (sh[j]);
                std::sort (nb.begin(), nb.end());
                if (std::abs (sh[i] - nb[nb.size() / 2]) > 1.5) ++off;
            }
            (void) med;
            if (std::getenv ("VOICE_REALISM_DUMP") != nullptr) std::printf ("render\n");
            if (std::getenv ("VOICE_REALISM_DUMP") != nullptr)
                for (size_t i = 0; i < sh.size(); ++i) std::printf ("shift %.2f in %.1f t %.2f\n", sh[i], ins[i], ts[i]);
            r.offShift = 100.0 * off / (double) sh.size();
        }
        auto jit = [&] (const std::vector<PitchFrame>& t) {
            std::vector<double> d;
            for (size_t i = 1; i < t.size(); ++i)
                if (t[i].conf > 0.75 && t[i - 1].conf > 0.75 && t[i].energy > 0.01 * loud && t[i].hz > 0 && t[i - 1].hz > 0)
                    if (const double c = std::abs (1200.0 * std::log2 (t[i].hz / t[i - 1].hz)); c < 200.0) d.push_back (c);
            if (d.empty()) return 0.0;
            std::sort (d.begin(), d.end()); return d[d.size() / 2]; };
        r.jitterIn = jit (a); r.jitterOut = jit (b);
        auto hf = [&] (const Buf& v) {   // (a 2nd-order high-pass at 4 kHz)
            const double w0 = 6.283185307 * 4000.0 / sr, cw = std::cos (w0), al = std::sin (w0) / (2 * 0.7071);
            const double b0 = (1 + cw) / 2 / (1 + al), b1 = -(1 + cw) / (1 + al), b2 = b0, a1 = -2 * cw / (1 + al), a2 = (1 - al) / (1 + al);
            double x1 = 0, x2 = 0, y1 = 0, y2 = 0, eh = 0, et = 0;
            for (int i = (int) (t0 * sr); i < (int) std::min ((double) v.size(), t1 * sr); ++i)
            {
                const double xx = v[(size_t) i], yy = b0 * xx + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = xx; y2 = y1; y1 = yy; eh += yy * yy; et += xx * xx;
            }
            return 10.0 * std::log10 ((eh + 1e-20) / (et + 1e-20)); };
        r.hfIn = hf (x); r.hfOut = hf (y);
        return r;
    }

    /** The LPC envelope (order 12, ~11 kHz, averaged over 30 ms frames) of a stretch, in dB, on a log-frequency
        grid from 200 Hz to 4 kHz (240 points). */
    inline std::vector<double> lpcEnvelope (const Buf& x, double sr, double t0, double t1);
    inline double gridHz (int i) { return 200.0 * std::pow (20.0, i / 239.0); }
    /** How far the formants moved, as one ratio: the frequency scale that best lines the output's LPC envelope up
        with the input's (correlation on the log-frequency grid, parabolic peak). */
    inline double envelopeScale (const Buf& in, double sr, double t0, double t1, const Buf& out, double u0, double u1)
    {
        const auto a = lpcEnvelope (in, sr, t0, t1), b = lpcEnvelope (out, sr, u0, u1);
        if (a.empty() || b.empty()) return 0.0;
        auto corr = [&] (int sh) {
            double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0; int n = 0;
            for (int i = 0; i < 240; ++i) { const int j = i + sh; if (j < 0 || j >= 240) continue; sa += a[(size_t) i]; sb += b[(size_t) j]; saa += a[(size_t) i] * a[(size_t) i]; sbb += b[(size_t) j] * b[(size_t) j]; sab += a[(size_t) i] * b[(size_t) j]; ++n; }
            if (n < 120) return -1.0;
            const double cov = sab / n - sa / n * sb / n, va = saa / n - sa / n * sa / n, vb = sbb / n - sb / n * sb / n;
            return cov / std::sqrt (va * vb + 1e-12); };
        int best = 0; double bc = -2.0;
        for (int sh = -60; sh <= 60; ++sh) { const double c = corr (sh); if (c > bc) { bc = c; best = sh; } }
        const double cm = corr (best - 1), cp = corr (best + 1), den = cm - 2 * bc + cp;
        const double fr = std::abs (den) > 1e-12 ? std::clamp (0.5 * (cm - cp) / den, -0.5, 0.5) : 0.0;
        return std::pow (20.0, (best + fr) / 239.0);
    }

    /** How periodic a stretch is (0..1): the mean best normalised autocorrelation of its 60 ms frames over the
        pitch range. Breath, jitter, shimmer and fry all bring it down. */
    inline double periodicity (const Buf& x, double sr, double t0, double t1, int step = 2)
    {
        const int w = (int) (0.06 * sr), lagMin = (int) (sr / 600), lagMax = (int) (sr / 50);
        double sum = 0.0; int n = 0;
        for (int s = (int) (t0 * sr); s + w + lagMax < (int) std::min ((double) x.size(), t1 * sr); s += w / 2)
        {
            double e0 = 0.0; for (int i = 0; i < w; ++i) e0 += (double) x[(size_t) (s + i)] * x[(size_t) (s + i)];
            if (e0 < 1.0e-6) continue;
            double best = 0.0;
            for (int lag = lagMin; lag <= lagMax; lag += step)
            {
                double xy = 0.0, e1 = 0.0;
                for (int i = 0; i < w; i += step) { xy += (double) x[(size_t) (s + i)] * x[(size_t) (s + i + lag)]; e1 += (double) x[(size_t) (s + i + lag)] * x[(size_t) (s + i + lag)]; }
                best = std::max (best, xy / std::sqrt (0.5 * e0 * e1 + 1.0e-12));
            }
            sum += best; ++n;
        }
        return n > 0 ? sum / n : 0.0;
    }

    /** F1 and F2 of a stretch: LPC (order 12) on a copy decimated to ~11 kHz, the envelope's first two peaks,
        averaged over 30 ms frames. */
    inline std::array<double, 2> formantsOf (const Buf& x, double sr, double t0, double t1)
    {
        const int dec = std::max (1, (int) std::lround (sr / 11025.0)); const double fs = sr / dec;
        // decimate: windowed-sinc low-pass at 0.45 fs
        const int taps = 63; std::vector<double> h ((size_t) taps);
        for (int i = 0; i < taps; ++i) { const double m = i - (taps - 1) / 2.0, fc = 0.45 / dec; h[(size_t) i] = (m == 0 ? 2 * fc : std::sin (6.283185307179586 * fc * m) / (3.141592653589793 * m)) * (0.54 - 0.46 * std::cos (6.283185307179586 * i / (taps - 1))); }
        std::vector<double> y;
        for (int i = (int) (t0 * sr); i + taps < (int) std::min ((double) x.size(), t1 * sr); i += dec)
        { double s = 0.0; for (int k = 0; k < taps; ++k) s += h[(size_t) k] * x[(size_t) (i + k)]; y.push_back (s); }
        const int w = (int) (0.03 * fs), order = 12;
        double f1Sum = 0.0, f2Sum = 0.0; int frames = 0;
        for (int s = 0; s + w < (int) y.size(); s += w / 2)
        {
            std::vector<double> fr ((size_t) w);
            for (int i = 0; i < w; ++i) fr[(size_t) i] = (y[(size_t) (s + i)] - (i > 0 ? 0.94 * y[(size_t) (s + i - 1)] : 0.0)) * (0.54 - 0.46 * std::cos (6.283185307179586 * i / (w - 1)));
            std::vector<double> r ((size_t) order + 1, 0.0);
            for (int l = 0; l <= order; ++l) for (int i = l; i < w; ++i) r[(size_t) l] += fr[(size_t) i] * fr[(size_t) (i - l)];
            if (r[0] < 1e-9) continue;
            r[0] *= 1.0001;
            std::vector<double> a ((size_t) order + 1, 0.0), tmp; a[0] = 1.0; double err = r[0];
            for (int i = 1; i <= order; ++i)
            {
                double acc = r[(size_t) i]; for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
                const double k = -acc / err; tmp = a;
                for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
                a[(size_t) i] = k; err *= (1.0 - k * k);
            }
            std::vector<double> env; std::vector<double> hz;
            for (double f = 150.0; f < std::min (4500.0, 0.48 * fs); f += 5.0)
            {
                double re = 0.0, im = 0.0;
                for (int j = 0; j <= order; ++j) { re += a[(size_t) j] * std::cos (6.283185307179586 * f * j / fs); im -= a[(size_t) j] * std::sin (6.283185307179586 * f * j / fs); }
                env.push_back (-10.0 * std::log10 (re * re + im * im + 1e-18)); hz.push_back (f);
            }
            std::vector<double> peaks;
            for (size_t i = 1; i + 1 < env.size(); ++i) if (env[i] > env[i - 1] && env[i] >= env[i + 1]) peaks.push_back (hz[i]);
            if (peaks.size() >= 2) { f1Sum += peaks[0]; f2Sum += peaks[1]; ++frames; }
        }
        return frames > 0 ? std::array<double, 2> { f1Sum / frames, f2Sum / frames } : std::array<double, 2> { 0.0, 0.0 };
    }

    inline std::vector<double> lpcEnvelope (const Buf& x, double sr, double t0, double t1)
    {
        const int dec = std::max (1, (int) std::lround (sr / 11025.0)); const double fs = sr / dec;
        const int taps = 63; std::vector<double> h ((size_t) taps);
        for (int i = 0; i < taps; ++i) { const double m = i - (taps - 1) / 2.0, fc = 0.45 / dec; h[(size_t) i] = (m == 0 ? 2 * fc : std::sin (6.283185307179586 * fc * m) / (3.141592653589793 * m)) * (0.54 - 0.46 * std::cos (6.283185307179586 * i / (taps - 1))); }
        std::vector<double> y;
        for (int i = std::max (0, (int) (t0 * sr)); i + taps < (int) std::min ((double) x.size(), t1 * sr); i += dec)
        { double s = 0.0; for (int k = 0; k < taps; ++k) s += h[(size_t) k] * x[(size_t) (i + k)]; y.push_back (s); }
        const int w = (int) (0.03 * fs), order = 12;
        std::vector<double> sum (240, 0.0); int frames = 0;
        for (int s = 0; s + w < (int) y.size(); s += w / 2)
        {
            std::vector<double> fr ((size_t) w);
            for (int i = 0; i < w; ++i) fr[(size_t) i] = (y[(size_t) (s + i)] - (i > 0 ? 0.94 * y[(size_t) (s + i - 1)] : 0.0)) * (0.54 - 0.46 * std::cos (6.283185307179586 * i / (w - 1)));
            std::vector<double> r ((size_t) order + 1, 0.0);
            for (int l = 0; l <= order; ++l) for (int i = l; i < w; ++i) r[(size_t) l] += fr[(size_t) i] * fr[(size_t) (i - l)];
            if (r[0] < 1e-9) continue;
            r[0] *= 1.0001;
            std::vector<double> a ((size_t) order + 1, 0.0), tmp; a[0] = 1.0; double err = r[0];
            for (int i = 1; i <= order; ++i)
            {
                double acc = r[(size_t) i]; for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
                const double k = -acc / err; tmp = a;
                for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
                a[(size_t) i] = k; err *= (1.0 - k * k);
            }
            for (int g = 0; g < 240; ++g)
            {
                const double f = gridHz (g); double re = 0.0, im = 0.0;
                for (int j = 0; j <= order; ++j) { re += a[(size_t) j] * std::cos (6.283185307179586 * f * j / fs); im -= a[(size_t) j] * std::sin (6.283185307179586 * f * j / fs); }
                sum[(size_t) g] += -10.0 * std::log10 (re * re + im * im + 1e-18) + 10.0 * std::log10 (err + 1e-18);
            }
            ++frames;
        }
        if (frames == 0) return {};
        for (auto& v : sum) v /= frames;
        return sum;
    }

    inline double rmsDb (const Buf& x, double sr, double t0, double t1)
    {
        double s = 0.0; int n = 0;
        for (int i = (int) (t0 * sr); i < (int) std::min ((double) x.size(), t1 * sr); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
        return 10.0 * std::log10 (s / std::max (1, n) + 1e-20);
    }
}
