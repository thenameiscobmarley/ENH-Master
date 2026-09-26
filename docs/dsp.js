/* ENH Master site: the plugin's own maths, ported line by line from Source/DSP for the demos.
   PrecisionEQ.cpp (CLARITY precision bands), EarGuard.h, Character.h, FootstepRadar.cpp.
   Pure functions, no DOM: demos.js draws what these return. */
(function (G) {
  "use strict";
  var PI = Math.PI;
  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
  function clampHz(sr, hz) { return clamp(hz, 5, 0.49 * sr); }          // BiquadCoeffs::clampHz
  function dbToGain(db) { return Math.pow(10, db / 20); }
  function log2(x) { return Math.log(x) / Math.LN2; }

  /* ================= CLARITY precision bands (PrecisionEQ.cpp) ================= */
  var SR = 48000, FFT = 4096, HALF = FFT / 2, BIN = SR / FFT, GRID = 208;
  function gridHz(i) { return 40 * Math.pow(2, i / 24); }

  /** BiquadCoeffs::peaking (the test's "room": a +10 dB, Q 8 resonance at 2.3 kHz), and its |H|^2 at f. */
  function peaking(sr, f0, q, db) {
    var A = Math.pow(10, db / 40), w = 2 * PI * clampHz(sr, f0) / sr, c = Math.cos(w), a = Math.sin(w) / (2 * q);
    return { sr: sr, b0: 1 + a * A, b1: -2 * c, b2: 1 - a * A, a0: 1 + a / A, a1: -2 * c, a2: 1 - a / A };
  }
  function mag2(p, f) {
    var W = 2 * PI * f / p.sr, cw = Math.cos(W), sw = Math.sin(W), c2 = 2 * cw * cw - 1, s2 = 2 * sw * cw;
    var nr = p.b0 + p.b1 * cw + p.b2 * c2, ni = -(p.b1 * sw + p.b2 * s2), dr = p.a0 + p.a1 * cw + p.a2 * c2, di = -(p.a1 * sw + p.a2 * s2);
    return (nr * nr + ni * ni) / (dr * dr + di * di);
  }
  function peakingMag2(sr, f0, q, db, f) { return mag2(peaking(sr, f0, q, db), f); }
  function peakingDb(f0, q, db, f) { return 10 * Math.log10(peakingMag2(SR, f0, q, db, f)); }

  /** The precision band as the plugin runs it: x + (A^2 - 1) * band-pass of a TPT SVF at Q*A.
      A TPT SVF is the bilinear transform of the analogue filter (prewarped at its centre), so this
      is its exact response: s = j tan(pi f / sr) / tan(pi hz / sr). */
  function bandDb(b, f) {
    if (Math.abs(b.gainDb) < 1e-6) return 0;
    var A = Math.pow(10, b.gainDb / 40), k = 1 / (b.q * A);
    var W = Math.tan(PI * clampHz(SR, f) / SR) / Math.tan(PI * clampHz(SR, b.hz) / SR);
    var re = 1 - W * W, num = re * re + Math.pow(k * A * A * W, 2), den = re * re + Math.pow(k * W, 2);
    return 10 * Math.log10(num / den);
  }
  /** PrecisionEQ::bellDb: the Gaussian the plugin's display draws (for the width marker). */
  function bellOctaves(q) { return 2 / 0.693147 * Math.asinh(1 / (2 * Math.max(0.1, q))); }

  // Hann window's power response, a quarter bin apart over +-3 bins (the FFT's smear of a spectrum)
  var HK = [], HKsum = 0;
  (function () {
    function D(x) { return Math.abs(x) < 1e-9 ? 1 : Math.sin(PI * x) / (PI * x); }
    for (var x = -3; x <= 3.0001; x += 0.25) {
      var w = 0.5 * D(x) + 0.25 * D(x - 1) + 0.25 * D(x + 1);
      HK.push([x, w * w]); HKsum += w * w;
    }
  })();
  // prepare(): for each grid point, the bins it averages or the two it interpolates
  var gLo = new Int32Array(GRID), gHi = new Int32Array(GRID), gFrac = new Float64Array(GRID);
  (function () {
    for (var i = 0; i < GRID; i++) {
      var f = gridHz(i), lo = f * Math.pow(2, -1 / 48) / BIN, hi = f * Math.pow(2, 1 / 48) / BIN;
      var blo = Math.ceil(lo), bhi = Math.floor(hi);
      if (bhi >= blo) { gLo[i] = clamp(blo, 1, HALF - 1); gHi[i] = clamp(bhi, 1, HALF - 1); gFrac[i] = -1; }
      else { var b = f / BIN; gLo[i] = clamp(Math.floor(b), 1, HALF - 2); gHi[i] = gLo[i] + 1; gFrac[i] = b - Math.floor(b); }
    }
  })();

  /** What the precision layer settles on for pink noise through one resonance (or hole).
      ring: {hz, q, db}; clarity: the CLARITY knob 0..30; strength: STRENGTH (1 = as shipped). */
  function precisionBands(ring, clarity, strength, maxBands) {
    var wanted = maxBands == null ? 8 : maxBands;
    // Long-term spectrum, bin by bin: pink noise (power 1/f) through the resonance, seen through the Hann window
    var bins = new Float64Array(HALF), room = peaking(SR, ring.hz, ring.q, ring.db);
    for (var bb = 1; bb < HALF; bb++) {
      var s = 0;
      for (var h = 0; h < HK.length; h++) {
        var f = (bb + HK[h][0]) * BIN;
        if (f <= 1) continue;
        s += HK[h][1] * mag2(room, f) / f;
      }
      bins[bb] = s / HKsum;
    }
    // Folded onto the 1/24-octave grid exactly as analyseFrame() does
    var L = new Float64Array(GRID);
    for (var i = 0; i < GRID; i++) {
      var p;
      if (gFrac[i] < 0) { var sum = 0; for (var b = gLo[i]; b <= gHi[i]; b++) sum += bins[b]; p = sum / (gHi[i] - gLo[i] + 1); }
      else { var pa = bins[gLo[i]], pb = bins[gHi[i]]; p = pa + (pb - pa) * gFrac[i]; }
      L[i] = 10 * Math.log10(p + 1e-20);
    }

    // ---- findFeatures() ----
    var radius = 18, sigma = 6;
    function gaussian(inp, out) {
      for (var i = 0; i < GRID; i++) {
        var sum = 0, ws = 0;
        for (var d = -radius; d <= radius; d++) {
          var j = clamp(i + d, 0, GRID - 1), w = Math.exp(-0.5 * d * d / (sigma * sigma));
          sum += w * inp[j]; ws += w;
        }
        out[i] = sum / ws;
      }
    }
    var smooth = new Float64Array(GRID), clipped = new Float64Array(GRID);
    gaussian(L, smooth);
    for (var pass = 0; pass < 3; pass++) {
      for (i = 0; i < GRID; i++) clipped[i] = clamp(L[i], smooth[i] - 1, smooth[i] + 1);
      gaussian(clipped, smooth);
    }
    var top = -200; for (i = 0; i < GRID; i++) top = Math.max(top, L[i]);
    var contentFloor = top - 55;
    var r = new Float64Array(GRID);
    for (i = 0; i < GRID; i++) r[i] = L[i] - smooth[i];
    var normalize = clamp(clarity / 30, 0, 3);                       // ParameterMapping: CLARITY / 30
    var scale = clamp(normalize / 0.5, 0, 2) * clamp(strength, 0, 2);
    var result = { bands: [], L: L, smooth: smooth, residual: r, scale: scale, why: "" };
    if (wanted === 0 || scale <= 0.01) { result.why = "off"; return result; }

    // Spectral lines (notes, hums, tones): a bin 4x stronger than the bins 3 either side
    var lineBin = new Uint8Array(HALF), floorPow = Math.pow(10, contentFloor / 10), lineNear = new Uint8Array(GRID);
    for (b = 4; b < HALF - 4; b++) lineBin[b] = bins[b] > floorPow && bins[b] > 2 * (bins[b - 3] + bins[b + 3]) ? 1 : 0;
    for (i = 0; i < GRID; i++) {
      var fi = gridHz(i), reach = Math.max(fi * (Math.pow(2, 1 / 12) - 1), 3 * BIN);
      var b0 = Math.max(1, Math.floor((fi - reach) / BIN)), b1 = Math.min(HALF - 1, Math.floor((fi + reach) / BIN) + 1), any = false;
      for (b = b0; b <= b1 && !any; b++) any = lineBin[b] !== 0;
      lineNear[i] = any ? 1 : 0;
    }
    // A peak about as narrow as one tone's line is content, not a resonance
    var lineHz = 4 * BIN, toneAt = new Uint8Array(GRID);
    for (i = 1; i < GRID - 1; i++) {
      if (!(r[i] > 2.5 && r[i] >= r[i - 1] && r[i] > r[i + 1])) continue;
      var lo = i, hi = i;
      while (lo > 0 && r[lo - 1] > r[i] - 3) --lo;
      while (hi < GRID - 1 && r[hi + 1] > r[i] - 3) ++hi;
      var widthHz = gridHz(hi) * Math.pow(2, 1 / 48) - gridHz(lo) * Math.pow(2, -1 / 48);
      if (widthHz < lineHz || hi - lo <= 1)
        for (var j = Math.max(0, i - 4); j <= Math.min(GRID - 1, i + 4); j++) toneAt[j] = 1;
    }

    var cands = [];
    for (i = 2; i < GRID - 2; i++) {
      var v = r[i];
      var peak = v > 2.5 && v >= r[i - 1] && v > r[i + 1];
      var dip = v < -3.5 && v <= r[i - 1] && v < r[i + 1] && smooth[i] > contentFloor;
      if (!peak && !dip) continue;
      if (toneAt[i] || lineNear[i]) continue;
      if (L[i] < contentFloor && peak) continue;
      var half = 0.5 * v;
      var edge = function (dir) {
        var j = i;
        while (j + dir > 0 && j + dir < GRID - 1 && (v > 0 ? r[j + dir] > half : r[j + dir] < half)) j += dir;
        var a = r[j], bq = r[clamp(j + dir, 0, GRID - 1)];
        var t = Math.abs(a - bq) > 1e-6 ? clamp((a - half) / (a - bq), 0, 1) : 0;
        return j + dir * t;
      };
      var bw = clamp((edge(1) - edge(-1)) / 24, 1 / 24, 1.2);
      var pw = Math.pow(2, bw);
      var q = clamp(Math.sqrt(pw) / (pw - 1), 0.8, 16);
      var ra = r[i - 1], rc = r[i + 1], denom = ra - 2 * v + rc;
      var off = Math.abs(denom) > 1e-6 ? clamp(0.5 * (ra - rc) / denom, -0.5, 0.5) : 0;
      var hz = 40 * Math.pow(2, (i + off) / 24);
      var gain = peak ? -Math.min(0.85 * (v - 1), 8) : Math.min(0.5 * (-v - 2), 3);
      gain *= scale;
      var limited = false;
      if (peak && hz > 250 && hz < 5000) {
        var floorCut = -4 - 4 * clamp((q - 2) / 4, 0, 1);
        if (gain < floorCut) { gain = floorCut; limited = true; }
      }
      gain = clamp(gain, -8, 3);
      if (Math.abs(gain) < 0.5) continue;
      cands.push({ hz: hz, q: q, gainDb: gain, strength: Math.abs(gain), limited: limited, feature: v });
    }
    cands.sort(function (x, y) { return y.strength - x.strength; });
    for (var k = 0; k < cands.length && result.bands.length < wanted; k++) {
      var clear = true;
      for (var t = 0; t < result.bands.length; t++)
        clear = clear && Math.abs(log2(cands[k].hz / result.bands[t].hz)) >= 1 / 6;
      if (clear) result.bands.push(cands[k]);
    }
    if (!result.bands.length) {
      // Why nothing was placed, at the point that sticks out most (for the demo's caption)
      var mi = 0;
      for (i = 0; i < GRID; i++) if (Math.abs(r[i]) > Math.abs(r[mi])) mi = i;
      result.why = toneAt[mi] || lineNear[mi] ? "tone" : (r[mi] > 0 ? r[mi] <= 2.5 : r[mi] >= -3.5) ? "shallow" : "small";
    }
    return result;
  }

  /* ================= EAR GUARD (EarGuard.h), run on the signal's K-weighted power ================= */
  /** A quiet scene at `quietLufs`, then a blast `blastDb` louder for `blastS` seconds, from `startS`.
      Steady noise: its K-weighted power per sample is a constant in each stretch, which is all the
      guard ever looks at. Returns traces (dB against the quiet level) and the test's measurements. */
  function earGuard(o) {
    var sr = 48000, E = 16;
    var jumpDb = clamp(o.jumpDb, 6, 24), maxReductionDb = 30;
    var look = E * Math.max(2, Math.round(0.005 * sr / E));
    var winB = Math.max(4, Math.round(0.4 * sr / E));
    var attackK = Math.exp(-1 / (0.0008 * sr)), releaseK = Math.exp(-1 / (0.06 * sr));
    var every = E / sr;
    var usualUpK = Math.exp(-every / 10), usualDownK = Math.exp(-every / 30), learnK = Math.exp(-every / 0.3);
    var learnDecisions = Math.floor(2 / every), maxRise = 0.75 * every;
    var kDelay = new Float64Array(look), outBlocks = new Float64Array(winB), inBlocks = new Float64Array(winB);
    var outSum = 0, inSum = 0, aheadPower = 0, blockOut = 0, blockIn = 0;
    var delayPos = 0, blockPos = 0, inBlock = 0, usualDb = -200, gain = 1, targetGain = 1;
    var untilDecision = E, learning = 0, heard = false;
    var windowSamples = winB * E, floorGain = Math.pow(10, -maxReductionDb / 20);

    function decide() {
      var momentaryDb = 10 * Math.log10(Math.max(inSum, 0) / windowSamples + 1e-20) - 0.691;
      if (momentaryDb > -70) {
        if (!heard) { heard = true; usualDb = momentaryDb; learning = learnDecisions; }
        if (learning > 0) {
          --learning;
          usualDb = momentaryDb > usualDb ? learnK * usualDb + (1 - learnK) * momentaryDb : usualDb;
          targetGain = 1; return;
        }
        if (momentaryDb > usualDb - 20) {
          var kk = momentaryDb > usualDb ? usualUpK : usualDownK;
          var next = kk * usualDb + (1 - kk) * momentaryDb;
          if (targetGain < 0.99) next = Math.min(next, usualDb + maxRise);
          usualDb = next;
        }
      }
      if (!heard) { targetGain = 1; return; }
      var allowed = Math.pow(10, (usualDb + jumpDb + 0.691) / 10) * windowSamples;
      var leaving = 0;
      for (var j = 0, n = look / E; j < n; ++j) leaving += outBlocks[(blockPos + j) % winB];
      var room = allowed - Math.max(0, outSum - leaving);
      var g2 = room / Math.max(aheadPower, 1e-20);
      // Spend it evenly (EarGuard.h, 3.7.13.13): at most 8 dB over the allowed level after a quiet
      // stretch, less as the window fills; never more than 1 dB under it (no deep dips to echo)
      var used = Math.min(1, Math.max(0, outSum / Math.max(allowed, 1e-20)));
      var burst = 8 * Math.min(1, Math.max(0, (1 - used) / 0.7));
      var levelDb = usualDb + jumpDb + 0.691, ahead = Math.max(aheadPower, 1e-20);
      var atLevel = Math.pow(10, (levelDb - 1) / 10) * look / ahead;
      var cap = Math.pow(10, (levelDb + burst) / 10) * look / ahead;
      g2 = Math.min(cap, Math.max(g2, atLevel));
      targetGain = g2 >= 1 ? 1 : Math.max(floorGain, Math.sqrt(Math.max(g2, 0)));
    }

    var quietP = Math.pow(10, (o.quietLufs + 0.691) / 10), blastP = quietP * Math.pow(10, o.blastDb / 10);
    var start = Math.round(o.startS * sr), stop = start + Math.max(1, Math.round(o.blastS * sr));
    var total = Math.round((o.startS + o.afterS) * sr);
    // traces: from startS - viewBeforeS, in buckets
    var from = start - Math.round(o.viewBeforeS * sr), buckets = o.buckets, per = (total - from) / buckets;
    var tIn = new Float64Array(buckets), tOut = new Float64Array(buckets), tMom = new Float64Array(buckets), tLim = new Float64Array(buckets), tGr = new Float64Array(buckets);
    for (var z = 0; z < buckets; z++) { tIn[z] = 0; tOut[z] = 0; tGr[z] = 1; }
    // output power history for the measurements (per 16-sample block, heard time)
    var nb = Math.ceil(total / E), outHist = new Float64Array(nb), inHist = new Float64Array(nb);

    for (var s = 0; s < total; s++) {
      var power = s >= start && s < stop ? blastP : quietP;
      var heardPower = kDelay[delayPos];
      aheadPower += power - heardPower;
      kDelay[delayPos] = power;
      if (--untilDecision <= 0) { untilDecision = E; decide(); }
      var k = targetGain < gain ? attackK : releaseK;
      gain = k * gain + (1 - k) * targetGain;
      if (++delayPos >= look) delayPos = 0;
      var outP = heardPower * gain * gain;
      blockOut += outP; blockIn += heardPower;
      outHist[(s / E) | 0] += outP; inHist[(s / E) | 0] += heardPower;
      if (++inBlock >= E) {
        inBlock = 0;
        outSum += blockOut - outBlocks[blockPos]; inSum += blockIn - inBlocks[blockPos];
        outBlocks[blockPos] = blockOut; inBlocks[blockPos] = blockIn;
        blockOut = blockIn = 0;
        if (++blockPos >= winB) {
          blockPos = 0; outSum = inSum = 0;
          for (var jj = 0; jj < winB; jj++) { outSum += outBlocks[jj]; inSum += inBlocks[jj]; }
          aheadPower = 0; for (var d = 0; d < look; d++) aheadPower += kDelay[d];
        }
      }
      if (s >= from) {
        var bi = Math.min(buckets - 1, ((s - from) / per) | 0);
        if (heardPower > tIn[bi]) tIn[bi] = heardPower;
        if (outP > tOut[bi]) tOut[bi] = outP;
        if (gain < tGr[bi]) tGr[bi] = gain;
        tMom[bi] = outSum; tLim[bi] = usualDb;
      }
    }
    // to dB against the quiet level
    var qDb = 10 * Math.log10(quietP), mostReduction = 0;
    for (z = 0; z < buckets; z++) {
      tIn[z] = 10 * Math.log10(Math.max(tIn[z], 1e-30)) - qDb;
      tOut[z] = 10 * Math.log10(Math.max(tOut[z], 1e-30)) - qDb;
      tGr[z] = -20 * Math.log10(Math.max(tGr[z], 1e-6));
      if (tGr[z] > mostReduction) mostReduction = tGr[z];
      tMom[z] = 10 * Math.log10(Math.max(tMom[z], 1e-30) / windowSamples) - qDb;
      tLim[z] = tLim[z] + jumpDb + 0.691 - qDb;
    }
    // The test's measurements (MasteringTests.h): the 2 s before, and the loudest 400 ms of the first 2 s after
    function win(h, a, n) { var e = 0; for (var i = a; i < a + n; i++) e += h[i]; return 10 * Math.log10(e / (n * E) / quietP + 1e-30); }
    var bs = start / E | 0, w = 0.4 * sr / E | 0;
    var before = win(outHist, bs - (2 * sr / E | 0), 2 * sr / E | 0);
    var after = -200, inAfter = -200;
    for (var a = bs; a + w < bs + (2 * sr / E | 0); a += w / 4 | 0) {
      after = Math.max(after, win(outHist, a, w));
      inAfter = Math.max(inAfter, win(inHist, a, w));
    }
    return { tIn: tIn, tOut: tOut, tMom: tMom, tLim: tLim, tGr: tGr, before: before,
             outOver: after - before, inOver: inAfter - before, mostReduction: mostReduction,
             latencyMs: look / sr * 1000 };
  }

  /* ================= FOOTSTEP RADAR (FootstepRadar.cpp, the lift for an accepted step) ================= */
  function smoothRamp(e0, e1, x) { var t = clamp((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t); }
  /** boostDb: BOOST 0..12, space: SPACE 0..10, peakDb: the step's peak (dBFS), probability 0..1, distance 0..1.
      Returns the lift in its strongest band (dB) and the room send (0..0.9). */
  function radarLift(boostDb, space, peakDb, probability, distance) {
    var quiet = clamp((-peakDb - 10) / 25, 0.25, 1);
    var boost = clamp(boostDb, 0, 12) * Math.min(1, 0.4 + 0.7 * probability) * (0.35 + 0.65 * distance) * quiet;
    var send = clamp(space, 0, 10) / 10 * smoothRamp(0.2, 0.9, distance) * 0.9;
    return { liftDb: clamp(boost, 0, 15), send: send, holdS: 0.09 + 0.08 * distance, quiet: quiet };
  }

  /* ================= CHARACTER (Character.h) ================= */
  var MODELS = ["MODERN CLEAN", "BRITISH CONSOLE", "AMERICAN CONSOLE", "VINTAGE CONSOLE",
                "TAPE 15 IPS", "TAPE 30 IPS", "VALVE", "ARENA", "CINEMA"];
  // DspMath.h: TPT state-variable EQ (bell, shelves) and the plain SVF
  function svfEq(a1, a2, a3, m0, m1, m2) { return { a1: a1, a2: a2, a3: a3, m0: m0, m1: m1, m2: m2 }; }
  function eqBell(sr, hz, q, db) {
    var A = Math.pow(10, db / 40), g = Math.tan(PI * clampHz(sr, hz) / sr), k = 1 / (q * A), a1 = 1 / (1 + g * (g + k));
    return svfEq(a1, g * a1, g * g * a1, 1, k * (A * A - 1), 0);
  }
  function eqHighShelf(sr, hz, q, db) {
    var A = Math.pow(10, db / 40), g = Math.tan(PI * clampHz(sr, hz) / sr) * Math.sqrt(A), k = 1 / q, a1 = 1 / (1 + g * (g + k));
    return svfEq(a1, g * a1, g * g * a1, A * A, k * (1 - A) * A, 1 - A * A);
  }
  function eqLowShelf(sr, hz, q, db) {
    var A = Math.pow(10, db / 40), g = Math.tan(PI * clampHz(sr, hz) / sr) / Math.sqrt(A), k = 1 / q, a1 = 1 / (1 + g * (g + k));
    return svfEq(a1, g * a1, g * g * a1, 1, k * (A - 1), A * A - 1);
  }
  function svf(sr, hz, q) {
    var g = Math.tan(PI * clampHz(sr, hz) / sr), kk = 1 / q, a1 = 1 / (1 + g * (g + kk));
    return { a1: a1, a2: g * a1, a3: g * g * a1, k: kk };
  }
  function fastTanh(x) {
    if (x > 4.97) return 1; if (x < -4.97) return -1;
    var x2 = x * x;
    return x * (135135 + x2 * (17325 + x2 * (378 + x2))) / (135135 + x2 * (62370 + x2 * (3150 + x2 * 28)));
  }

  /** Character::design() for one model (COMPONENTS matched: f = 1, bias x1) at the oversampled rate. */
  function design(model, o, tone) {
    var k = { pre: [], post: [], hp: null, lp: null, inputTrim: 1, cleanPeak: 1, colourHeadroom: 2,
              fluxR: 0, fluxLimit: 1, fluxBias: 0, flux2R: 0, flux2Limit: 1, amp: 0, ampK: 1, ampBias: 0, ampLimit: 1,
              sag: 0, envAtt: 0, envRel: 0, slew: 0 };
    function r(hz) { return Math.exp(-2 * PI * hz / o); }
    function dB(g) { return g * tone; }
    switch (model) {
      case 0: k.cleanPeak = 1.40; k.amp = 3; k.ampLimit = 1.25; k.inputTrim = 0.22; break;
      case 1: k.cleanPeak = 0.60; k.inputTrim = 0.60; k.amp = 3; k.ampLimit = 1.05; k.slew = 0.22;
        k.post.push(eqHighShelf(o, 9500, 0.6, dB(0.6)), eqBell(o, 180, 0.7, dB(-0.3))); k.hp = svf(o, 12, 0.6); break;
      case 2: k.cleanPeak = 0.22; k.inputTrim = 0.75; k.amp = 1; k.ampK = 1.25; k.ampBias = 0.08;
        k.flux2R = r(14); k.flux2Limit = 0.30;
        k.post.push(eqBell(o, 110, 0.8, dB(0.6)), eqBell(o, 3000, 0.6, dB(0.35))); k.hp = svf(o, 14, 0.6); break;
      case 3: k.cleanPeak = 0.18; k.inputTrim = 0.80; k.fluxR = r(10); k.fluxLimit = 0.16; k.fluxBias = 0.008;
        k.amp = 1; k.ampK = 0.95; k.ampBias = 0.12; k.flux2R = r(16); k.flux2Limit = 0.35;
        k.post.push(eqLowShelf(o, 75, 0.6, dB(1.0)), eqBell(o, 1600, 0.5, dB(0.25))); k.lp = svf(o, 23000, 0.55); break;
      case 4: k.cleanPeak = 0.40; k.inputTrim = 0.90; k.pre.push(eqHighShelf(o, 3200, 0.6, 6)); k.amp = 2; k.ampLimit = 0.95;
        k.post.push(eqHighShelf(o, 3200, 0.6, -6), eqBell(o, 52, 1.1, dB(1.3)), eqBell(o, 24, 1.3, dB(-0.9)));
        k.lp = svf(o, 17500, 0.55); k.hp = svf(o, 12, 0.6); break;
      case 5: k.cleanPeak = 0.60; k.inputTrim = 0.70; k.pre.push(eqHighShelf(o, 6000, 0.6, 4)); k.amp = 2; k.ampLimit = 1.05;
        k.post.push(eqHighShelf(o, 6000, 0.6, -4), eqBell(o, 92, 1.0, dB(0.9)), eqBell(o, 42, 1.2, dB(-0.5)));
        k.lp = svf(o, 22000, 0.6); k.hp = svf(o, 10, 0.6); break;
      case 6: k.cleanPeak = 0.13; k.inputTrim = 0.40; k.amp = 1; k.ampK = 1.5; k.ampBias = 0.16;
        k.sag = 0.25; k.envAtt = Math.exp(-1 / (0.010 * o)); k.envRel = Math.exp(-1 / (0.150 * o));
        k.flux2R = r(18); k.flux2Limit = 0.40; k.post.push(eqBell(o, 220, 0.7, dB(0.3)));
        k.lp = svf(o, 20000, 0.5); k.hp = svf(o, 15, 0.6); break;
      case 7: k.cleanPeak = 0.65; k.colourHeadroom = 1.3; k.amp = 3; k.ampLimit = 1.1; k.inputTrim = 0.55;
        k.post.push(eqBell(o, 280, 0.8, dB(-0.9)), eqBell(o, 3200, 0.8, dB(1.1)), eqHighShelf(o, 11000, 0.6, dB(0.5)));
        k.hp = svf(o, 24, 0.7); break;
      case 8: k.cleanPeak = 0.40; k.inputTrim = 0.70; k.fluxR = r(12); k.fluxLimit = 0.22;
        k.amp = 1; k.ampK = 0.8; k.ampBias = 0.10;
        k.post.push(eqLowShelf(o, 65, 0.6, dB(1.0)), eqHighShelf(o, 9000, 0.6, dB(-0.6))); k.hp = svf(o, 16, 0.6); break;
    }
    k.fluxInvLimit2 = 1 / (k.fluxLimit * k.fluxLimit);
    k.fluxBiasSat = k.fluxBias / Math.sqrt(1 + k.fluxBias * k.fluxBias * k.fluxInvLimit2);
    k.flux2InvLimit2 = 1 / (k.flux2Limit * k.flux2Limit);
    return k;
  }

  /** A sine through one model, as Character::process runs it on the 4x oversampled signal.
      Returns harmonic amplitudes against the fundamental and THD (%), as the unit tests measure them. */
  function characterSine(set) {
    var sr = 48000, o = sr * 4, hz = set.hz || 1000;
    var colour = clamp(set.colour, 0, 10);
    var tone = colour <= 5 ? colour / 2.5 : 2 + (colour - 5) / 5 * 6;
    var colourDepth = colour <= 5 ? colour / 10 : 0.5 + (colour - 5) / 5 * 1.5;
    var g = dbToGain((clamp(set.drive, 0, 10) - 5) * 3);
    var k = design(set.model, o, tone), grit = !!set.grit;
    var guardAtt = Math.exp(-1 / (0.001 * o)), guardRel = Math.exp(-1 / (0.150 * o)), colourRel = Math.exp(-1 / (0.300 * o));
    var dcCoeff = Math.exp(-2 * PI * 4 / o);
    var pre = k.pre.map(function () { return [0, 0]; }), post = k.post.map(function () { return [0, 0]; });
    var hpS = [0, 0], lpS = [0, 0];
    var st = { flux: 0, fluxRes: 0, flux2: 0, flux2Res: 0, env: 0, slewed: 0, dcX: 0, dcY: 0 };
    var guardEnv = 0, colourEnv = 0, amp = dbToGain(set.levelDb);
    var period = o / hz, settle = Math.round(o * 0.25), N = Math.round(period * 8), total = settle + N;
    var out = new Float64Array(N);
    function eqp(c, s, v0) {
      var v3 = v0 - s[1], v1 = c.a1 * s[0] + c.a2 * v3, v2 = s[1] + c.a2 * s[0] + c.a3 * v3;
      s[0] = 2 * v1 - s[0]; s[1] = 2 * v2 - s[1];
      return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }
    function svfp(c, s, v0, which) {
      var v3 = v0 - s[1], v1 = c.a1 * s[0] + c.a2 * v3, v2 = s[1] + c.a2 * s[0] + c.a3 * v3;
      s[0] = 2 * v1 - s[0]; s[1] = 2 * v2 - s[1];
      return which ? v0 - c.k * v1 - v2 : v2;   // high : low
    }
    function fluxStage(x, which, r, invLimit2, bias, biasSat) {
      var fl = which ? "flux2" : "flux", rs = which ? "flux2Res" : "fluxRes";
      st[fl] = r * st[fl] + (1 - r) * x;
      var f = st[fl] + bias, sat = f / Math.sqrt(1 + f * f * invLimit2), d = (sat - biasSat) - st[fl];
      var out = (d - r * st[rs]) / (1 - r);
      st[rs] = d;
      return x + out;
    }
    for (var n = 0; n < total; n++) {
      var u = amp * Math.sin(2 * PI * hz * n / o) * g;
      var a = Math.abs(u);
      guardEnv = a > guardEnv ? a + guardAtt * (guardEnv - a) : a + guardRel * (guardEnv - a);
      colourEnv = a > colourEnv ? a : a + colourRel * (colourEnv - a);
      var sweet = k.cleanPeak * Math.min(colourDepth, k.colourHeadroom) * (grit ? 1.8 : 1);
      var lift = colourEnv > 1e-6 ? clamp(sweet / colourEnv, 1, 32) : 1;
      var ceiling = k.cleanPeak * clamp(colourDepth, 1, k.colourHeadroom);
      var hot = guardEnv * lift, guard = grit || hot <= ceiling ? 1 : ceiling / hot;
      var nl = lift * guard;
      // runModel()
      var x = u * k.inputTrim, i;
      for (i = 0; i < k.pre.length; i++) x = eqp(k.pre[i], pre[i], x);
      x *= nl;
      if (k.fluxR > 0) x = fluxStage(x, 0, k.fluxR, k.fluxInvLimit2, k.fluxBias, k.fluxBiasSat);
      if (k.amp === 1) {
        var bias = k.ampBias;
        if (k.sag > 0) { var ax = Math.abs(x); st.env = (ax > st.env ? k.envAtt : k.envRel) * (st.env - ax) + ax; bias = Math.min(0.5, bias + k.sag * st.env); }
        var tb = fastTanh(k.ampK * bias), slope = k.ampK * (1 - tb * tb);
        x = (fastTanh(k.ampK * (x + bias)) - tb) / slope;
      } else if (k.amp === 2) { var aa = Math.abs(x) / k.ampLimit; x = x / Math.cbrt(1 + aa * aa * aa); }
      else if (k.amp === 3) x = k.ampLimit * fastTanh(x / k.ampLimit);
      if (k.slew > 0) { st.slewed += k.slew * fastTanh((x - st.slewed) / k.slew); x = st.slewed; }
      if (k.flux2R > 0) x = fluxStage(x, 1, k.flux2R, k.flux2InvLimit2, 0, 0);
      if (k.amp === 1 || k.fluxBias !== 0) { var y = x - st.dcX + dcCoeff * st.dcY; st.dcX = x; st.dcY = y; x = y; }
      x /= nl;
      for (i = 0; i < k.post.length; i++) x = eqp(k.post[i], post[i], x);
      if (k.hp) x = svfp(k.hp, hpS, x, 1);
      if (k.lp) x = svfp(k.lp, lpS, x, 0);
      x /= k.inputTrim;
      if (n >= settle) out[n - settle] = x / g;
    }
    // Least-squares fit of the fundamental and each harmonic below Nyquist (the tests' measure)
    var harm = [], fund = 0, sumH = 0;
    for (var h = 1; h * hz < sr * 0.5 - 50; h++) {
      // sin / cos of the harmonic's phase, turned one sample at a time (a rotating phasor)
      var w = 2 * PI * hz * h / o, cr = Math.cos(w), sr1 = Math.sin(w), ph0 = w * settle;
      var cs = Math.cos(ph0), sn = Math.sin(ph0), sc = 0, cc = 0;
      for (n = 0; n < N; n++) {
        sc += out[n] * sn; cc += out[n] * cs;
        var t2 = cs * cr - sn * sr1; sn = sn * cr + cs * sr1; cs = t2;
      }
      var am = Math.sqrt(sc * sc + cc * cc) * 2 / N;
      if (h === 1) fund = am; else { harm.push(am); sumH += am * am; }
    }
    var rel = harm.map(function (v) { return 20 * Math.log10(Math.max(v, 1e-12) / Math.max(fund, 1e-12)); });
    return { thd: 100 * Math.sqrt(sumH) / Math.max(fund, 1e-12), harmonicsDb: rel, fundamental: fund };
  }

  G.ENHDSP = {
    clamp: clamp, gridHz: gridHz, peakingDb: peakingDb, bandDb: bandDb, bellOctaves: bellOctaves,
    precisionBands: precisionBands, earGuard: earGuard, radarLift: radarLift,
    characterSine: characterSine, MODELS: MODELS
  };
})(window);
