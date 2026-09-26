/* ENH Master site: the interactive demos. Knobs you can turn, graphs that follow, computed by
   dsp.js (the plugin's own maths). No network, no storage. The page's HTML holds a snapshot of
   every demo at its default settings, so without JavaScript it still shows the graphs. */
(function () {
  "use strict";
  var D = window.ENHDSP;
  if (!D) return;
  var PRE = /[?&]prerender\b/.test(location.search);   // build the default snapshot only (for index.html)
  var clamp = D.clamp;

  /* ---------------- formatting ---------------- */
  function minus(s) { return String(s).replace(/-/g, "−"); }
  function fx(v, d) { var s = v.toFixed(d); return /^-0(\.0+)?$/.test(s) ? s.slice(1) : s; }
  function sdb(v, d) { var s = fx(v, d == null ? 1 : d); return minus((parseFloat(s) > 0 ? "+" : "") + s) + " dB"; }
  function hzText(v) {
    if (v >= 1000) { var k = v / 1000; return String(+k.toFixed(k >= 10 ? 1 : 2)) + " kHz"; }
    return Math.round(v) + " Hz";
  }
  function msText(s) { return s < 1 ? Math.round(s * 1000) + " ms" : String(+s.toFixed(2)) + " s"; }
  function n1(v) { return (Math.round(v * 10) / 10).toString(); }
  function esc(s) { return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/"/g, "&quot;"); }

  /* ---------------- knobs ---------------- */
  // A 270-degree dial: 135 deg (min) round the top to 405 deg (max)
  var ARC = "M20.3 79.7A42 42 0 1 1 79.7 79.7";
  function toNorm(s, v) { return s.log ? Math.log(v / s.min) / Math.log(s.max / s.min) : (v - s.min) / (s.max - s.min); }
  function fromNorm(s, n) {
    n = clamp(n, 0, 1);
    var v = s.log ? s.min * Math.pow(s.max / s.min, n) : s.min + n * (s.max - s.min);
    if (s.step) v = Math.round(v / s.step) * s.step;
    return clamp(v, s.min, s.max);
  }
  function knobGeom(s) {
    var n = toNorm(s, s.value), o = s.origin == null ? 0 : toNorm(s, s.origin);
    var a = Math.min(n, o) * 100, b = Math.max(n, o) * 100;
    var ang = (135 + 270 * n) * Math.PI / 180;
    return { dash: n1(b - a) + " 200", off: n1(-a), x: n1(50 + 23 * Math.cos(ang)), y: n1(50 + 23 * Math.sin(ang)) };
  }
  function knobHTML(s) {
    var g = knobGeom(s), ticks = "";
    for (var i = 0; i <= 4; i++) {
      var an = (135 + 270 * i / 4) * Math.PI / 180, c = Math.cos(an), sn = Math.sin(an);
      ticks += '<line class="k-tick" x1="' + n1(50 + 47.5 * c) + '" y1="' + n1(50 + 47.5 * sn) + '" x2="' + n1(50 + 50.5 * c) + '" y2="' + n1(50 + 50.5 * sn) + '"/>';
    }
    return '<div class="knob" data-k="' + s.id + '"><svg viewBox="-2 -2 104 104" aria-hidden="true" focusable="false">' +
      '<path class="k-track" d="' + ARC + '"/>' + ticks +
      '<path class="k-glow" d="' + ARC + '" pathLength="100" stroke-dasharray="' + g.dash + '" stroke-dashoffset="' + g.off + '"/>' +
      '<path class="k-val" d="' + ARC + '" pathLength="100" stroke-dasharray="' + g.dash + '" stroke-dashoffset="' + g.off + '"/>' +
      '<circle class="k-disc" cx="50" cy="50" r="31"/><circle class="k-sheen" cx="50" cy="50" r="31"/>' +
      '<circle class="k-dot" cx="' + g.x + '" cy="' + g.y + '" r="3.4"/></svg>' +
      '<output class="k-v">' + esc(s.fmt(s.value)) + '</output><span class="k-l">' + esc(s.label) + '</span></div>';
  }

  function enhanceKnob(el, s, changed) {
    var val = el.querySelector(".k-val"), glow = el.querySelector(".k-glow"), dot = el.querySelector(".k-dot"), out = el.querySelector(".k-v");
    el.setAttribute("role", "slider");
    el.tabIndex = 0;
    el.setAttribute("aria-label", s.aria || s.label);
    el.setAttribute("aria-valuemin", String(s.min));
    el.setAttribute("aria-valuemax", String(s.max));
    el.title = s.label + ": drag, scroll or use the arrow keys. Double-click resets.";
    function paint() {
      var g = knobGeom(s);
      [val, glow].forEach(function (p) { p.setAttribute("stroke-dasharray", g.dash); p.setAttribute("stroke-dashoffset", g.off); });
      dot.setAttribute("cx", g.x); dot.setAttribute("cy", g.y);
      out.textContent = s.fmt(s.value);
      el.setAttribute("aria-valuenow", String(Math.round(s.value * 1000) / 1000));
      el.setAttribute("aria-valuetext", s.text ? s.text(s.value) : s.fmt(s.value));
    }
    function set(v) {
      v = clamp(v, s.min, s.max);
      if (s.step) v = Math.round(v / s.step) * s.step;
      if (v === s.value) return;
      s.value = v; paint(); changed();
    }
    s.set = set;
    paint();
    function nudge(dir, big) {
      if (s.step && s.stepped) set(s.value + dir * s.step * (big ? 3 : 1));
      else set(fromNorm(s, toNorm(s, s.value) + dir * (big ? 0.1 : (s.kstep || 0.01))));
    }
    var drag = null;
    el.addEventListener("pointerdown", function (e) {
      if (e.button !== 0) return;
      e.preventDefault();
      try { el.setPointerCapture(e.pointerId); } catch (err) { /* old browsers */ }
      drag = { x: e.clientX, y: e.clientY, n: toNorm(s, s.value) };
      el.classList.add("turning");
      el.focus({ preventScroll: true });
    });
    el.addEventListener("pointermove", function (e) {
      if (!drag) return;
      var d = (drag.y - e.clientY) + (e.clientX - drag.x);
      var span = s.stepped ? 160 : e.shiftKey ? 900 : 220;   // Shift: fine
      set(fromNorm(s, drag.n + d / span));
    });
    function end() { drag = null; el.classList.remove("turning"); }
    el.addEventListener("pointerup", end);
    el.addEventListener("pointercancel", end);
    el.addEventListener("lostpointercapture", end);
    el.addEventListener("wheel", function (e) {
      e.preventDefault();
      var dy = e.deltaY || -e.deltaX;
      if (dy) nudge(dy < 0 ? 1 : -1, false);
    }, { passive: false });
    el.addEventListener("dblclick", function () { set(s.def); });
    el.addEventListener("keydown", function (e) {
      var k = e.key, done = true;
      if (k === "ArrowUp" || k === "ArrowRight") nudge(1, e.shiftKey);
      else if (k === "ArrowDown" || k === "ArrowLeft") nudge(-1, e.shiftKey);
      else if (k === "PageUp") nudge(1, true);
      else if (k === "PageDown") nudge(-1, true);
      else if (k === "Home") set(s.min);
      else if (k === "End") set(s.max);
      else done = false;
      if (done) e.preventDefault();
    });
  }

  function spec(o) { o.def = o.value; return o; }

  /* ---------------- drawing helpers ---------------- */
  function Plot(W, H, m, x0, x1, y0, y1, logX) {
    var p = { W: W, H: H, m: m };
    p.x = function (v) { var t = logX ? Math.log(v / x0) / Math.log(x1 / x0) : (v - x0) / (x1 - x0); return m.l + t * (W - m.l - m.r); };
    p.y = function (v) { return m.t + (y1 - clamp(v, y0, y1)) / (y1 - y0) * (H - m.t - m.b); };
    p.bottom = H - m.b; p.top = m.t; p.left = m.l; p.right = W - m.r;
    return p;
  }
  function line(xs, ys) {
    var d = "";
    for (var i = 0; i < xs.length; i++) d += (i ? "L" : "M") + n1(xs[i]) + " " + n1(ys[i]);
    return d;
  }
  function area(xs, ys, base) { return line(xs, ys) + "L" + n1(xs[xs.length - 1]) + " " + n1(base) + "L" + n1(xs[0]) + " " + n1(base) + "Z"; }
  function gridY(p, vals, fmt) {
    var s = "";
    vals.forEach(function (v) {
      s += '<line class="' + (v === 0 ? "zero" : "axis") + '" x1="' + p.left + '" y1="' + n1(p.y(v)) + '" x2="' + p.right + '" y2="' + n1(p.y(v)) + '"/>' +
           '<text class="t" x="' + (p.left - 6) + '" y="' + n1(p.y(v) + 4) + '" text-anchor="end">' + fmt(v) + "</text>";
    });
    return s;
  }
  function gridX(p, vals, fmt) {
    var s = "";
    vals.forEach(function (v, i) {
      var x = n1(p.x(v));
      s += '<line class="axis" x1="' + x + '" y1="' + p.top + '" x2="' + x + '" y2="' + p.bottom + '"/>' +
           '<text class="t" x="' + x + '" y="' + (p.bottom + 17) + '" text-anchor="' + (i === vals.length - 1 ? "end" : "middle") + '">' + fmt(v, i) + "</text>";
    });
    return s;
  }
  function anchorFor(p, x) { var t = (x - p.left) / (p.right - p.left); return t < 0.22 ? "start" : t > 0.78 ? "end" : "middle"; }
  function q(root, sel) { return root.querySelector(sel); }
  function setD(root, sel, d) { var e = q(root, sel); if (e) e.setAttribute("d", d); }
  function setText(root, sel, t, x, y, anchor) {
    var e = q(root, sel); if (!e) return;
    e.textContent = t;
    if (x != null) { e.setAttribute("x", n1(x)); e.setAttribute("y", n1(y)); }
    if (anchor) e.setAttribute("text-anchor", anchor);
  }
  function knobsHTML(list) { return list.map(knobHTML).join(""); }

  /* ================= 1. CLARITY precision bands ================= */
  function EqDemo(root) {
    var K = {
      hz: spec({ id: "hz", label: "Ring frequency", min: 300, max: 8000, value: 2300, log: true, fmt: hzText }),
      q: spec({ id: "q", label: "Ring width (Q)", min: 2, max: 16, value: 8, log: true, fmt: function (v) { return "Q " + fx(v, 1); } }),
      db: spec({ id: "db", label: "Ring height", min: -12, max: 15, value: 10, step: 0.5, origin: 0, kstep: 0.02, fmt: function (v) { return sdb(v); },
                 text: function (v) { return v < 0 ? sdb(v) + ", a hole" : sdb(v) + ", a ring"; } }),
      clar: spec({ id: "clar", label: "CLARITY", min: 0, max: 30, value: 15, step: 0.5, fmt: function (v) { return fx(v, 1).replace(/\.0$/, ""); } })
    };
    var order = [K.hz, K.q, K.db, K.clar];
    var p, W;
    function build(w) {
      W = w;
      var H = w < 500 ? 250 : 260;
      p = Plot(w, H, { l: 38, r: 10, t: 24, b: 28 }, 200, 10000, -12, 16, true);
      var ticks = w < 500 ? [200, 500, 1000, 2000, 5000] : [200, 300, 500, 1000, 2000, 3000, 5000, 10000];
      return '<svg class="gr' + (w < 500 ? " narrow" : "") + '" viewBox="0 0 ' + w + " " + H + '" role="img" aria-label="">' +
        '<rect class="zone" x="' + n1(p.x(250)) + '" y="' + p.top + '" width="' + n1(p.x(5000) - p.x(250)) + '" height="' + (p.bottom - p.top) + '"/>' +
        '<text class="zt" x="' + n1(p.x(250) + 6) + '" y="' + (p.top - 8) + '">footsteps &amp; voices: 250 Hz to 5 kHz</text>' +
        gridY(p, [-10, -5, 0, 5, 10, 15], function (v) { return v === 0 ? "0 dB" : minus((v > 0 ? "+" : "") + v); }) +
        gridX(p, ticks, function (v, i) { var t = v >= 1000 ? v / 1000 + "k" : String(v); return i === ticks.length - 1 ? t + " Hz" : t; }) +
        '<path class="res"/><path class="cut"/><path class="out"/><line class="bw"/>' +
        '<text class="ann a-ring"></text><text class="ann a-band"></text><text class="ann2 a-bw"></text></svg>';
    }
    function draw() {
      var ring = { hz: K.hz.value, q: K.q.value, db: K.db.value };
      var r = D.precisionBands(ring, K.clar.value, 1);
      var xs = [], yr = [], yc = [], yo = [], N = W < 500 ? 180 : 280, mn = 0, mx = 0;
      for (var i = 0; i < N; i++) {
        var f = 200 * Math.pow(50, i / (N - 1)), rd = D.peakingDb(ring.hz, ring.q, ring.db, f), cd = 0;
        for (var b = 0; b < r.bands.length; b++) cd += D.bandDb(r.bands[b], f);
        xs.push(p.x(f)); yr.push(p.y(rd)); yc.push(p.y(cd)); yo.push(p.y(rd + cd));
        mn = Math.min(mn, cd); mx = Math.max(mx, cd);
      }
      var z = p.y(0);
      setD(root, ".res", area(xs, yr, z));
      setD(root, ".cut", area(xs, yc, z));
      setD(root, ".out", line(xs, yo));
      var rx = p.x(ring.hz), up = ring.db >= 0;
      var ringLabel = (up ? sdb(ring.db, 1) + " ring at " : sdb(ring.db, 1) + " hole at ") + hzText(ring.hz);
      setText(root, ".a-ring", ringLabel, rx, up ? p.y(ring.db) - 8 : p.y(ring.db) + 16, anchorFor(p, rx));
      var bw = q(root, ".bw"), band = r.bands[0], readout;
      if (band) {
        var oct = D.bellOctaves(band.q), yb = p.y(band.gainDb / 2);
        bw.setAttribute("x1", n1(p.x(band.hz * Math.pow(2, -oct / 2)))); bw.setAttribute("x2", n1(p.x(band.hz * Math.pow(2, oct / 2))));
        bw.setAttribute("y1", n1(yb)); bw.setAttribute("y2", n1(yb)); bw.setAttribute("visibility", "visible");
        var bx = p.x(band.hz), cutDown = band.gainDb < 0;
        var bl = "band: " + Math.round(band.hz) + " Hz, Q " + fx(band.q, 1) + ", " + (cutDown ? "cut " : "lift ") + fx(Math.abs(band.gainDb), 1) + " dB";
        setText(root, ".a-band", bl, bx, cutDown ? p.y(band.gainDb) + 17 : p.y(band.gainDb) - 9, anchorFor(p, bx));
        var bwx = p.x(band.hz * Math.pow(2, oct / 2)) + 6, bwa = "start";
        if (bwx > p.right - 70) { bwx = p.x(band.hz * Math.pow(2, -oct / 2)) - 6; bwa = "end"; }
        setText(root, ".a-bw", "width (Q)", bwx, yb + 4, bwa);
        readout = "CLARITY " + K.clar.fmt(K.clar.value) + " placed one band at <b>" + Math.round(band.hz) + " Hz, Q " + fx(band.q, 1) + "</b>, " +
          (cutDown ? "cutting <b>" : "lifting <b>") + fx(Math.abs(band.gainDb), 1) + " dB</b>" +
          (band.limited ? ": held to " + fx(Math.abs(band.gainDb), 1) + " dB, as a cut this wide between 250 Hz and 5 kHz would take what footsteps and voices need." :
           !cutDown && band.gainDb >= 2.99 ? ": lifts stop at 3 dB." : Math.abs(band.gainDb) >= 7.99 ? ": cuts stop at 8 dB." : ".");
        if (r.bands.length > 1) readout += " (" + (r.bands.length - 1) + " more small band" + (r.bands.length > 2 ? "s" : "") + " beside it.)";
      } else {
        bw.setAttribute("visibility", "hidden");
        setText(root, ".a-band", "", 0, 0);
        setText(root, ".a-bw", "", 0, 0);
        readout = r.why === "off" ? "CLARITY at 0: no correction, no band." :
          r.why === "tone" ? "No band: a peak this narrow looks like a note, a hum or a test tone, which is content, so the plugin leaves it alone." :
          r.why === "shallow" ? "No band: nothing sticks out of the spectrum around it enough (a broad tilt is the 24 adaptive bands' job, not shown here)." :
          "No band: the correction would be under 0.5 dB.";
      }
      q(root, ".gr").setAttribute("aria-label", ringLabel + " in pink noise. " + readout.replace(/<[^>]+>/g, ""));
      q(root, ".readout").innerHTML = readout;   // our own text, built above
    }
    return { knobs: order, build: build, draw: draw,
      html: function (w) { return '<div class="demo-grid"><div class="plot">' + build(w) + '</div><div class="knobs k4">' + knobsHTML(order) + '</div></div><p class="readout"></p>'; } };
  }

  /* ================= 2. EAR GUARD ================= */
  function GuardDemo(root, gaugeRoot) {
    var K = {
      blast: spec({ id: "blast", label: "Blast", aria: "Blast loudness over the usual level", min: 0, max: 40, value: 35, step: 0.5, kstep: 0.0125,
                    fmt: function (v) { return sdb(v); } }),
      len: spec({ id: "len", label: "Length", aria: "Blast length", min: 0.01, max: 2, value: 2, log: true, fmt: msText })
    };
    var order = [K.blast, K.len], jump = 12, p, W, last = null;
    function build(w) {
      W = w;
      var H = w < 500 ? 230 : 240;
      p = Plot(w, H, { l: 38, r: 10, t: 14, b: 28 }, -0.5, 2.5, -5, 40, false);
      var ticks = w < 500 ? [-0.5, 0, 1, 2, 2.5] : [-0.5, 0, 0.5, 1, 1.5, 2, 2.5];
      return '<svg class="gr' + (w < 500 ? " narrow" : "") + '" viewBox="0 0 ' + w + " " + H + '" role="img" aria-label="">' +
        gridY(p, [0, 10, 20, 30, 40], function (v) { return v === 0 ? "0" : "+" + v; }) +
        gridX(p, ticks, function (v, i) { return minus(String(v)) + (i === ticks.length - 1 ? " s" : ""); }) +
        '<path class="g-inp"/><path class="g-out"/><path class="g-outl"/><path class="g-mom"/><path class="g-lim"/></svg>';
    }
    function sim() {
      return D.earGuard({ jumpDb: jump, blastDb: K.blast.value, blastS: K.len.value, quietLufs: -40, startS: 2.5, afterS: 2.5, viewBeforeS: 0.5, buckets: W < 500 ? 200 : 300 });
    }
    function draw() {
      var s = sim(), B = s.tIn.length, xs = [], yi = [], yo = [], ym = [], yl = [];
      for (var i = 0; i < B; i++) {
        var t = -0.5 + (i + 0.5) / B * 3;
        xs.push(p.x(t)); yi.push(p.y(s.tIn[i])); yo.push(p.y(s.tOut[i])); ym.push(p.y(s.tMom[i])); yl.push(p.y(s.tLim[i]));
      }
      setD(root, ".g-inp", line(xs, yi));
      setD(root, ".g-out", area(xs, yo, p.bottom));
      setD(root, ".g-outl", line(xs, yo));
      setD(root, ".g-mom", line(xs, ym));
      setD(root, ".g-lim", line(xs, yl));
      var touched = s.mostReduction > 0.5;
      var txt = "Loudest 400 ms after it starts: <b>" + sdb(s.inOver) + "</b> going in, <b>" + sdb(s.outOver) + "</b> reaching your ears" +
        (touched ? " (held down by up to " + fx(s.mostReduction, 1) + " dB)." : ". It fits the budget: not touched at all.");
      q(root, ".readout").innerHTML = txt;
      q(root, ".gr").setAttribute("aria-label", "Level over time, against the usual level. " + txt.replace(/<[^>]+>/g, ""));
      last = s;
      if (gaugeRoot) gauge(gaugeRoot, s.inOver, s.outOver, jump);
    }
    function setJump(j) {
      jump = j;
      [].forEach.call(root.querySelectorAll("[data-jump]"), function (b) { b.setAttribute("aria-checked", String(+b.getAttribute("data-jump") === j)); b.tabIndex = +b.getAttribute("data-jump") === j ? 0 : -1; });
    }
    function preset(blast, len) { K.blast.set ? (K.blast.set(blast), K.len.set(len)) : 0; }
    return { knobs: order, build: build, draw: draw, setJump: setJump, preset: preset,
      html: function (w) {
        var radios = [12, 15, 18].map(function (j) { return '<button type="button" class="chip" role="radio" data-jump="' + j + '" aria-checked="' + (j === 12) + '">' + j + " dB</button>"; }).join("");
        return '<div class="demo-grid"><div class="plot">' + build(w) + '<ul class="legend"><li><i class="l-in"></i>the blast going in</li><li><i class="l-out"></i>what reaches your ears</li><li><i class="l-mom"></i>its last 400 ms</li><li><i class="l-lim"></i>the budget: usual + setting</li></ul></div>' +
          '<div class="knobs k2">' + knobsHTML(order) +
          '<div class="setting"><span class="k-l" id="guard-set-l">EAR GUARD setting</span><div class="chips" role="radiogroup" aria-labelledby="guard-set-l">' + radios + '</div>' +
          '<span class="k-l">Try</span><div class="chips"><button type="button" class="chip ghost" data-preset="35 2">a lasting blast</button><button type="button" class="chip ghost" data-preset="20 0.05">a 50 ms shot</button></div></div>' +
          '</div></div><p class="readout"></p>';
      } };
  }

  // The big dial: in (ghost arc), out (value arc, needle), the three settings marked
  function gaugeHTML() {
    var s = '<svg viewBox="0 0 320 250" role="img" aria-label=""><path class="g-track" d="M56.08 220A120 120 0 1 1 263.92 220" stroke-width="16"/>' +
      '<path class="g-ghost" d="M56.08 220A120 120 0 1 1 263.92 220" pathLength="100" stroke-width="6" stroke-dasharray="100 200"/>' +
      '<path class="g-glow" d="M56.08 220A120 120 0 1 1 263.92 220" pathLength="100" stroke-width="16" stroke-dasharray="100 200"/>' +
      '<path class="g-val" d="M56.08 220A120 120 0 1 1 263.92 220" pathLength="100" stroke-width="16" stroke-dasharray="100 200"/>';
    for (var i = 0; i <= 20; i++) {
      var a = (150 + 240 * i / 20) * Math.PI / 180, c = Math.cos(a), sn = Math.sin(a), major = i % 5 === 0;
      s += '<line class="g-tick' + (major ? " major" : "") + '" x1="' + n1(160 + 133 * c) + '" y1="' + n1(160 + 133 * sn) + '" x2="' + n1(160 + (major ? 142 : 137) * c) + '" y2="' + n1(160 + (major ? 142 : 137) * sn) + '"/>';
      if (major) s += '<text class="g-small" x="' + n1(160 + 154 * c) + '" y="' + n1(160 + 154 * sn + 4) + '" text-anchor="middle" font-size="10">' + (i ? "+" + i * 2 : "0") + "</text>";
    }
    [12, 15, 18].forEach(function (j) {
      var a = (150 + 240 * j / 40) * Math.PI / 180, c = Math.cos(a), sn = Math.sin(a);
      s += '<line class="g-set" data-set="' + j + '" x1="' + n1(160 + 98 * c) + '" y1="' + n1(160 + 98 * sn) + '" x2="' + n1(160 + 110 * c) + '" y2="' + n1(160 + 110 * sn) + '"/>';
    });
    s += '<g class="g-needle-g"><line class="g-needle" x1="160" y1="160" x2="250" y2="160" stroke-width="3"/></g><circle class="g-hub" cx="160" cy="160" r="9"/>' +
      '<text class="g-in" x="112" y="206" text-anchor="middle" font-size="19"></text><text class="g-small" x="112" y="222" text-anchor="middle" font-size="10">blast in</text>' +
      '<text class="g-num" x="208" y="206" text-anchor="middle" font-size="19"></text><text class="g-small" x="208" y="222" text-anchor="middle" font-size="10">what you hear</text></svg>';
    return s;
  }
  function gauge(root, inDb, outDb, jump) {
    var svg = q(root, "svg");
    function arc(sel, v) { var e = q(svg, sel); e.setAttribute("stroke-dashoffset", n1(100 - clamp(v, 0, 40) / 40 * 100)); }
    arc(".g-ghost", inDb); arc(".g-glow", outDb); arc(".g-val", outDb);
    q(svg, ".g-needle-g").setAttribute("transform", "rotate(" + n1(150 + 240 * clamp(outDb, 0, 40) / 40) + " 160 160)");
    q(svg, ".g-in").textContent = sdb(inDb);
    q(svg, ".g-num").textContent = sdb(outDb);
    [].forEach.call(svg.querySelectorAll(".g-set"), function (l) { l.classList.toggle("on", +l.getAttribute("data-set") === jump); });
    svg.setAttribute("aria-label", "Ear Guard gauge: loudest 400 ms of the blast " + sdb(inDb) + " over the usual loudness going in, " + sdb(outDb) + " coming out, with the guard at " + jump + " dB.");
  }

  /* ================= 3. FOOTSTEP RADAR ================= */
  function RadarDemo(root) {
    var K = {
      boost: spec({ id: "boost", label: "BOOST", min: 0, max: 12, value: 6, step: 0.1, fmt: function (v) { return fx(v, 1) + " dB"; } }),
      space: spec({ id: "space", label: "SPACE", min: 0, max: 10, value: 4, step: 0.1, fmt: function (v) { return fx(v, 1); } }),
      peak: spec({ id: "peak", label: "Step's peak", aria: "The step's peak level", min: -60, max: -5, value: -40, step: 0.5, kstep: 0.02,
                   fmt: function (v) { return minus(fx(v, 0)) + " dBFS"; } })
    };
    var order = [K.boost, K.space, K.peak], p, W;
    function build(w) {
      W = w;
      var H = w < 500 ? 220 : 230;
      p = Plot(w, H, { l: 38, r: 40, t: 14, b: 28 }, 0, 1, 0, 12, false);
      var s = '<svg class="gr' + (w < 500 ? " narrow" : "") + '" viewBox="0 0 ' + w + " " + H + '" role="img" aria-label="">' +
        gridY(p, [0, 3, 6, 9, 12], function (v) { return v ? "+" + v : "0 dB"; }) +
        gridX(p, [0, 0.25, 0.5, 0.75, 1], function (v, i) { return i === 0 ? "near" : i === 4 ? "far" : String(v); });
      [0, 30, 60, 90].forEach(function (v) { s += '<text class="t t-send" x="' + (p.right + 6) + '" y="' + n1(p.y(v / 90 * 12) + 4) + '">' + v + "%</text>"; });
      return s + '<path class="r-lift"/><path class="r-liftl"/><path class="r-send"/></svg>';
    }
    function draw() {
      var xs = [], yl = [], ys = [];
      for (var i = 0; i <= 100; i++) {
        var d = i / 100, r = D.radarLift(K.boost.value, K.space.value, K.peak.value, 1, d);
        xs.push(p.x(d)); yl.push(p.y(r.liftDb)); ys.push(p.y(r.send / 0.9 * 12));
      }
      setD(root, ".r-lift", area(xs, yl, p.bottom));
      setD(root, ".r-liftl", line(xs, yl));
      setD(root, ".r-send", line(xs, ys));
      var nr = D.radarLift(K.boost.value, K.space.value, K.peak.value, 1, 0), fr = D.radarLift(K.boost.value, K.space.value, K.peak.value, 1, 1);
      var txt = "A near step gets <b>" + sdb(nr.liftDb) + "</b>, dry; a far one <b>" + sdb(fr.liftDb) + "</b> with <b>" + Math.round(fr.send * 100) + " %</b> sent to the room" +
        (nr.quiet < 1 ? ". A step this loud needs less, so every lift is scaled to " + Math.round(nr.quiet * 100) + " %." : ".");
      q(root, ".readout").innerHTML = txt;
      q(root, ".gr").setAttribute("aria-label", "Lift and room send against the step's distance. " + txt.replace(/<[^>]+>/g, ""));
    }
    return { knobs: order, build: build, draw: draw,
      html: function (w) { return '<div class="demo-grid"><div class="plot">' + build(w) + '<ul class="legend"><li><i class="l-out"></i>lift in the step\'s strongest band</li><li><i class="l-send"></i>room send (right scale)</li></ul></div><div class="knobs k3">' + knobsHTML(order) + '</div></div><p class="readout"></p>'; } };
  }

  /* ================= 4. CHARACTER ================= */
  var SHORT = ["CLEAN", "BRITISH", "AMERICAN", "VINTAGE", "TAPE 15", "TAPE 30", "VALVE", "ARENA", "CINEMA"];
  function CharDemo(root) {
    var K = {
      model: spec({ id: "model", label: "Model", min: 0, max: 8, value: 3, step: 1, stepped: true, fmt: function (v) { return SHORT[v]; },
                    text: function (v) { return D.MODELS[v]; } }),
      colour: spec({ id: "colour", label: "COLOUR", min: 0, max: 10, value: 5, step: 0.1, fmt: function (v) { return fx(v, 1); } }),
      drive: spec({ id: "drive", label: "DRIVE", min: 0, max: 10, value: 5, step: 0.1, fmt: function (v) { return fx(v, 1); } }),
      level: spec({ id: "level", label: "Sine level", aria: "Level of the test sine", min: -40, max: 0, value: -18, step: 0.5, kstep: 0.0125,
                    fmt: function (v) { return minus(fx(v, 0)) + " dBFS"; } })
    };
    var order = [K.model, K.colour, K.drive, K.level], grit = true, p, W, NB = 9;
    function build(w) {
      W = w;
      var H = w < 500 ? 220 : 230;
      p = Plot(w, H, { l: 44, r: 10, t: 14, b: 28 }, 0, NB, -120, 0, false);
      var s = '<svg class="gr' + (w < 500 ? " narrow" : "") + '" viewBox="0 0 ' + w + " " + H + '" role="img" aria-label="">' +
        gridY(p, [-120, -100, -80, -60, -40, -20, 0], function (v) { return v ? minus(String(v)) : "0 dB"; });
      var bw = (p.right - p.left) / NB;
      for (var i = 0; i < NB; i++) {
        var h = i + 2, cls = h % 2 ? "odd" : "even";
        s += '<rect class="hb ' + cls + '" data-h="' + i + '" x="' + n1(p.left + i * bw + bw * 0.2) + '" width="' + n1(bw * 0.6) + '" y="' + p.bottom + '" height="0" rx="3"/>' +
             '<text class="t" x="' + n1(p.left + (i + 0.5) * bw) + '" y="' + (p.bottom + 17) + '" text-anchor="middle">' + h + (h === 2 ? "nd" : h === 3 ? "rd" : "th") + "</text>";
      }
      return s + "</svg>";
    }
    function draw() {
      var r = D.characterSine({ model: K.model.value, colour: K.colour.value, drive: K.drive.value, levelDb: K.level.value, grit: grit });
      [].forEach.call(root.querySelectorAll(".hb"), function (b) {
        var v = r.harmonicsDb[+b.getAttribute("data-h")], y = p.y(Math.max(v, -120));
        b.setAttribute("y", n1(y)); b.setAttribute("height", n1(Math.max(0, p.bottom - y)));
      });
      var thd = r.thd < 0.1 ? fx(r.thd, 3) : r.thd < 10 ? fx(r.thd, 2) : fx(r.thd, 1);
      var txt = "<b>" + D.MODELS[K.model.value] + "</b>, 1 kHz at " + minus(fx(K.level.value, 1)) + " dBFS: THD <b>" + thd + " %</b>. 2nd harmonic " +
        minus(fx(r.harmonicsDb[0], 0)) + " dB, 3rd " + minus(fx(r.harmonicsDb[1], 0)) + " dB against the tone" + (grit ? "." : " (GRIT off: DRIVE stops short of distortion).");
      q(root, ".readout").innerHTML = txt;
      q(root, ".thd b").textContent = thd + " %";
      q(root, ".gr").setAttribute("aria-label", "Harmonics of a 1 kHz sine, 2nd to 10th, in dB against the tone. " + txt.replace(/<[^>]+>/g, ""));
    }
    function setGrit(g) { grit = g; var b = q(root, "[data-grit]"); if (b) { b.setAttribute("aria-pressed", String(g)); b.textContent = "GRIT " + (g ? "on" : "off"); } }
    return { knobs: order, build: build, draw: draw, setGrit: setGrit, grit: function () { return grit; },
      html: function (w) {
        return '<div class="demo-grid"><div class="plot"><p class="thd">THD <b></b></p>' + build(w) + '<ul class="legend"><li><i class="l-even"></i>even harmonics: warmth, weight</li><li><i class="l-odd"></i>odd harmonics: edge, bite</li></ul></div>' +
          '<div class="knobs k4">' + knobsHTML(order) + '<div class="setting"><button type="button" class="chip" data-grit aria-pressed="true">GRIT on</button></div></div></div><p class="readout"></p>';
      } };
  }

  /* ---------------- wiring ---------------- */
  function ready(fn) { if (document.readyState !== "loading") fn(); else document.addEventListener("DOMContentLoaded", fn); }
  ready(function () {
    var demos = [];
    function widthFor(el) { return PRE ? 600 : (el.clientWidth && el.clientWidth < 520 ? 340 : 600); }
    function mount(sel, make, after) {
      var root = document.querySelector(sel);
      if (!root) return;
      var extra = root.getAttribute("data-gauge") ? document.querySelector(root.getAttribute("data-gauge")) : null;
      if (extra) extra.innerHTML = gaugeHTML();
      var d = make(root, extra);
      d.root = root;
      d.w = widthFor(root);
      root.innerHTML = d.html(d.w);
      if (after) after(d, root);
      d.draw();
      if (PRE) return;
      var queued = false;
      d.schedule = function () {
        if (queued) return;
        queued = true;
        requestAnimationFrame(function () { queued = false; d.draw(); });
      };
      d.knobs.forEach(function (s) { enhanceKnob(root.querySelector('[data-k="' + s.id + '"]'), s, d.schedule); });
      root.classList.add("live");
      demos.push(d);
    }
    mount("[data-demo=eq]", EqDemo);
    mount("[data-demo=guard]", GuardDemo, function (d, root) {
      d.setJump(12);
      if (PRE) return;
      var radios = [].slice.call(root.querySelectorAll("[data-jump]"));
      radios.forEach(function (b, i) {
        b.addEventListener("click", function () { d.setJump(+b.getAttribute("data-jump")); d.schedule(); });
        b.addEventListener("keydown", function (e) {
          var dir = e.key === "ArrowRight" || e.key === "ArrowDown" ? 1 : e.key === "ArrowLeft" || e.key === "ArrowUp" ? -1 : 0;
          if (!dir) return;
          e.preventDefault();
          var nb = radios[(i + dir + radios.length) % radios.length];
          nb.focus(); nb.click();
        });
      });
      [].forEach.call(root.querySelectorAll("[data-preset]"), function (b) {
        b.addEventListener("click", function () { var v = b.getAttribute("data-preset").split(" "); d.preset(+v[0], +v[1]); });
      });
    });
    mount("[data-demo=radar]", RadarDemo);
    mount("[data-demo=char]", CharDemo, function (d, root) {
      if (PRE) return;
      var b = root.querySelector("[data-grit]");
      b.addEventListener("click", function () { d.setGrit(!d.grit()); d.schedule(); });
    });
    if (PRE) { document.documentElement.setAttribute("data-prerendered", "1"); return; }

    // Narrow screens get a narrower drawing (so its text stays readable); rebuilt only when that changes
    var rt = null;
    window.addEventListener("resize", function () {
      clearTimeout(rt);
      rt = setTimeout(function () {
        demos.forEach(function (d) {
          var w = widthFor(d.root);
          if (w === d.w) return;
          d.w = w;
          q(d.root, ".plot svg.gr").outerHTML = d.build(w);
          d.draw();
        });
      }, 150);
    }, { passive: true });
  });
})();
