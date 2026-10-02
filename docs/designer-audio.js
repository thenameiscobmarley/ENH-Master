/* The Rack Unit Designer's sound: the design's chain of blocks (designer.js DSP_BLOCKS) built from the
   browser's own audio nodes, played on songs made right here (synthesized - nothing is fetched) or on an
   audio file of the visitor's own (read locally, never uploaded). Knobs, sliders and switches wired to a
   block's parameter (their `ctl`) move it live. A safety limiter and a volume control sit at the very end:
   nothing leaves it too loud. The Sound tab's controls are built here, with DOM calls only. */
(function () {
  "use strict";
  const D = window.ENHDesigner;
  if (!D || !D.DSP_BLOCKS) return;
  const B = D.DSP_BLOCKS, $ = (id) => document.getElementById (id);
  const dbToGain = (db) => Math.pow (10, db / 20);

  // ------------------------------------------------------------------------------------------------
  // Sound presets: chains to start from
  const P = (name, chain) => ({ name, chain: chain.map (([b, p, on]) => ({ b, on: on !== false, p: p || {} })) });
  const PRESETS = [
    P ("Clean (nothing)", []),
    P ("Tube warmth", [["drive", { drive: 12, shape: 0, tone: 12000, mix: 55 }], ["eq", { low: 1.5, lowf: 100, high: 1, highf: 10000 }]]),
    P ("Tape glue", [["drive", { drive: 8, shape: 1, tone: 11000, mix: 70 }], ["comp", { threshold: -20, ratio: 2, attack: 30, release: 300, makeup: 3 }]]),
    P ("Punchy drums", [["comp", { threshold: -24, ratio: 4, attack: 25, release: 120, makeup: 6 }], ["eq", { low: 3, lowf: 70, mid: -2, midf: 400, high: 2, highf: 7000 }]]),
    P ("Vocal presence", [["eq", { low: -3, lowf: 150, mid: 3, midf: 3000, q: 1.2, high: 2, highf: 11000 }], ["comp", { threshold: -22, ratio: 3, attack: 8, release: 150, makeup: 4 }], ["room", { size: 1.2, mix: 12 }]]),
    P ("Air and sparkle", [["exciter", { freq: 6000, amount: 45 }], ["eq", { high: 2.5, highf: 12000 }]]),
    P ("Lo-fi radio", [["filter", { mode: 2, freq: 1500, q: 1.1 }], ["drive", { drive: 22, shape: 2, tone: 5000, mix: 80 }], ["gain", { gain: 4 }]]),
    P ("Plate reverb", [["room", { size: 2.4, damp: 30, predelay: 20, mix: 30 }]]),
    P ("Cathedral", [["room", { size: 6.5, damp: 55, predelay: 60, mix: 42 }], ["width", { width: 150 }]]),
    P ("Tape echo", [["delay", { time: 380, feedback: 45, tone: 3500, mix: 30 }], ["drive", { drive: 6, shape: 1, mix: 40 }]]),
    P ("Slapback", [["delay", { time: 110, feedback: 10, tone: 7000, mix: 28 }]]),
    P ("Wide stereo", [["width", { width: 170 }], ["eq", { low: -1, lowf: 150 }]]),
    P ("Mastering chain", [["eq", { low: 1, lowf: 60, mid: -1, midf: 350, q: 0.8, high: 1.5, highf: 12000 }], ["comp", { threshold: -14, ratio: 2, attack: 30, release: 250, makeup: 2 }],
                            ["exciter", { freq: 8000, amount: 15 }], ["width", { width: 115 }]]),
    P ("Footstep finder", [["eq", { low: -6, lowf: 200, mid: 6, midf: 2500, q: 1.5, high: 2, highf: 6000 }], ["comp", { threshold: -35, ratio: 6, attack: 1, release: 80, makeup: 12 }]]),
    P ("Heavy crush", [["comp", { threshold: -40, ratio: 20, attack: 0.5, release: 60, makeup: 18, mix: 50 }], ["drive", { drive: 18, shape: 2, mix: 40 }]]),
    P ("Dark and warm", [["filter", { mode: 0, freq: 5000, q: 0.7 }], ["drive", { drive: 10, shape: 0, mix: 50 }], ["eq", { low: 3, lowf: 120 }]]),
    P ("Telephone", [["filter", { mode: 2, freq: 1800, q: 2.5 }], ["drive", { drive: 15, shape: 2, mix: 60 }]]),
    P ("Chorus shine", [["chorus", { rate: 0.7, depth: 50, mix: 45 }], ["eq", { high: 1.5, highf: 10000 }]]),
    P ("Pendulum", [["pan", { rate: 0.35, depth: 70, mode: 0 }], ["room", { size: 1.5, mix: 15 }]]),
    P ("Lava lamp", [["wander", { freq: 900, range: 70, speed: 0.12, q: 3 }], ["delay", { time: 420, feedback: 30, tone: 3000, mix: 18 }]]),
    P ("Chopped up", [["stutter", { rate: 8, depth: 85, smooth: 35 }]]),
    P ("Old sampler", [["crush", { bits: 8, mix: 60 }], ["filter", { mode: 0, freq: 9000, q: 0.7 }]]),
    P ("Worn tape", [["wow", { wow: 45, flutter: 35 }], ["drive", { drive: 7, shape: 1, tone: 8000, mix: 45 }]]),
    P ("Shimmer heaven", [["shimmer", { size: 5, octave: 65, mix: 35 }], ["width", { width: 140 }]]),
    P ("Dub delay", [["filter", { mode: 1, freq: 300 }], ["delay", { time: 600, feedback: 70, tone: 2000, mix: 40 }], ["room", { size: 3, mix: 20 }]]),
  ];

  // ------------------------------------------------------------------------------------------------
  // Songs, synthesized once each (an offline render of a few instruments, looped)
  const SONGS = ["Drum groove", "Bass and keys", "Ambient pad", "Game footsteps", "Vocal-like lead", "Full mix"];
  const songCache = {};
  function noiseBuffer (ctx, seconds) {
    const b = ctx.createBuffer (1, Math.floor (ctx.sampleRate * seconds), ctx.sampleRate), d = b.getChannelData (0);
    let s = 12345; for (let i = 0; i < d.length; ++i) { s = (s * 1103515245 + 12345) >>> 0; d[i] = s / 2147483648 - 1; }
    return b;
  }
  async function renderSong (name) {
    if (songCache[name]) return songCache[name];
    const sr = 44100, bpm = name === "Ambient pad" ? 70 : name === "Game footsteps" ? 90 : 100, beat = 60 / bpm, bars = 4, len = bars * 4 * beat;
    const ctx = new OfflineAudioContext (2, Math.ceil (len * sr), sr);
    const master = ctx.createGain (); master.gain.value = 0.5; master.connect (ctx.destination);
    const noise = noiseBuffer (ctx, 2);
    const pan = (v) => { const p = ctx.createStereoPanner (); p.pan.value = v; p.connect (master); return p; };
    const env = (g, t, a, d, peak) => { g.gain.setValueAtTime (0, t); g.gain.linearRampToValueAtTime (peak, t + a); g.gain.exponentialRampToValueAtTime (0.0005, t + a + d); };
    const kick = (t) => { const o = ctx.createOscillator (), g = ctx.createGain (); o.frequency.setValueAtTime (130, t); o.frequency.exponentialRampToValueAtTime (45, t + 0.12);
      env (g, t, 0.002, 0.35, 1.0); o.connect (g).connect (pan (0)); o.start (t); o.stop (t + 0.5); };
    const hit = (t, freq, q, dur, peak, p, type = "bandpass") => { const n = ctx.createBufferSource (), f = ctx.createBiquadFilter (), g = ctx.createGain ();
      n.buffer = noise; f.type = type; f.frequency.value = freq; f.Q.value = q; env (g, t, 0.001, dur, peak); n.connect (f).connect (g).connect (pan (p)); n.start (t, Math.random () * 1.5); n.stop (t + dur + 0.05); };
    const snare = (t) => { hit (t, 1800, 0.8, 0.18, 0.6, 0.05); const o = ctx.createOscillator (), g = ctx.createGain (); o.frequency.value = 190; env (g, t, 0.001, 0.08, 0.35); o.connect (g).connect (pan (0)); o.start (t); o.stop (t + 0.2); };
    const hat = (t, open) => hit (t, 9000, 0.7, open ? 0.25 : 0.05, 0.22, 0.25, "highpass");
    const note = (t, hz, dur, type, peak, cutoff, p = 0, detune = 0) => { for (const dt of detune ? [-detune, detune] : [0]) {
      const o = ctx.createOscillator (), f = ctx.createBiquadFilter (), g = ctx.createGain (); o.type = type; o.frequency.value = hz; o.detune.value = dt;
      f.type = "lowpass"; f.frequency.value = cutoff; g.gain.setValueAtTime (0, t); g.gain.linearRampToValueAtTime (peak, t + Math.min (0.02, dur * 0.2));
      g.gain.setValueAtTime (peak, t + dur * 0.8); g.gain.linearRampToValueAtTime (0, t + dur); o.connect (f).connect (g).connect (pan (p)); o.start (t); o.stop (t + dur + 0.02); } };
    const hz = (n) => 440 * Math.pow (2, (n - 69) / 12);
    const drums = () => { for (let b = 0; b < bars * 4; ++b) { const t = b * beat; if (b % 4 === 0 || b % 4 === 2) kick (t); if (b % 4 === 3) kick (t + beat * 0.5);
      if (b % 2 === 1) snare (t); hat (t, false); hat (t + beat / 2, b % 4 === 3); } };
    const chords = [[57, 60, 64], [53, 57, 60], [55, 59, 62], [52, 55, 59]];   // Am F G Em
    const bass = () => { for (let bar = 0; bar < bars; ++bar) { const root = chords[bar][0] - 24;
      for (let k = 0; k < 8; ++k) note (bar * 4 * beat + k * beat / 2, hz (root + (k === 6 ? 7 : 0)), beat * 0.45, "sawtooth", 0.28, 600); } };
    const keys = () => { for (let bar = 0; bar < bars; ++bar) for (const off of [0.5, 1.5, 2.5, 3.5]) for (const n of chords[bar])
      note ((bar * 4 + off) * beat, hz (n), beat * 0.35, "square", 0.05, 2500, 0.3); };
    const pad = () => { for (let bar = 0; bar < bars; ++bar) for (const n of chords[bar]) note (bar * 4 * beat, hz (n), 4 * beat, "sawtooth", 0.06, 1400, (n % 3 - 1) * 0.5, 12); };
    const lead = () => { const mel = [69, 72, 74, 72, 69, 67, 64, 67, 69, 72, 76, 74, 72, 69, 67, 69];
      mel.forEach ((n, i) => { const t = i * beat, o = ctx.createOscillator (), g = ctx.createGain (); o.type = "sawtooth"; o.frequency.value = hz (n);
        const vib = ctx.createOscillator (), vg = ctx.createGain (); vib.frequency.value = 5.5; vg.gain.value = 6; vib.connect (vg).connect (o.frequency);
        g.gain.setValueAtTime (0, t); g.gain.linearRampToValueAtTime (0.12, t + 0.05); g.gain.setValueAtTime (0.12, t + beat * 0.8); g.gain.linearRampToValueAtTime (0, t + beat * 0.98);
        const out = pan (0.1);
        for (const [f, q, a] of [[700, 6, 1], [1200, 8, 0.6], [2600, 10, 0.35]]) { const bp = ctx.createBiquadFilter (), ag = ctx.createGain (); bp.type = "bandpass"; bp.frequency.value = f * (n % 2 ? 1.08 : 1); bp.Q.value = q; ag.gain.value = a * 3; g.connect (bp).connect (ag).connect (out); }
        o.connect (g); o.start (t); vib.start (t); o.stop (t + beat); vib.stop (t + beat); }); };
    const footsteps = () => { const wind = ctx.createBufferSource (), wf = ctx.createBiquadFilter (), wg = ctx.createGain (); wind.buffer = noise; wind.loop = true;
      wf.type = "lowpass"; wf.frequency.value = 400; wg.gain.value = 0.08; wind.connect (wf).connect (wg).connect (pan (0)); wind.start (0); wind.stop (len);
      for (let k = 0; k < bars * 4 * 2; ++k) { const t = k * beat / 2 + 0.02 * Math.sin (k), side = k % 2 ? -0.6 : -0.3; hit (t, 700 + 300 * (k % 3), 1.5, 0.07, 0.25 + 0.1 * (k % 2), side); hit (t + 0.03, 3500, 1, 0.03, 0.08, side); }
      for (const t of [beat * 3.5, beat * 11]) { hit (t, 300, 0.5, 0.9, 0.9, 0.7, "lowpass"); hit (t, 2000, 0.5, 0.3, 0.4, 0.7); } };
    if (name === "Drum groove") drums ();
    else if (name === "Bass and keys") { bass (); keys (); }
    else if (name === "Ambient pad") pad ();
    else if (name === "Game footsteps") footsteps ();
    else if (name === "Vocal-like lead") { lead (); pad (); }
    else { drums (); bass (); keys (); lead (); }
    const buf = await ctx.startRendering ();
    // Every song to the same peak (-6 dBFS), so switching between them never jumps in level
    let pk = 0; for (let c = 0; c < buf.numberOfChannels; ++c) for (const v of buf.getChannelData (c)) pk = Math.max (pk, Math.abs (v));
    if (pk > 1e-6) for (let c = 0; c < buf.numberOfChannels; ++c) { const d = buf.getChannelData (c), g = 0.5 / pk; for (let i = 0; i < d.length; ++i) d[i] *= g; }
    songCache[name] = buf;
    return buf;
  }

  // ------------------------------------------------------------------------------------------------
  // The engine
  let ctx = null, source = null, input = null, chainOut = null, limiter = null, volume = null, analyser = null;
  let lastLevel = 0, lastWave = null;
  let blocks = [], builtSig = "", playing = false, bypass = false, current = null;
  const curves = {};
  function curve (shape) {   // the saturator's curves: tube (asymmetric, soft), tape (soft, even), hard (clipping)
    if (curves[shape]) return curves[shape];
    const n = 2048, c = new Float32Array (n);
    for (let i = 0; i < n; ++i) { const x = i / (n - 1) * 2 - 1;
      c[i] = shape === 0 ? Math.tanh (x + 0.2) - Math.tanh (0.2) : shape === 1 ? Math.tanh (1.5 * x) / Math.tanh (1.5) : Math.max (-0.7, Math.min (0.7, x)) / 0.7; }
    return (curves[shape] = c);
  }
  function impulse (seconds, damp) {
    const len = Math.max (1, Math.floor (ctx.sampleRate * Math.min (8, seconds))), b = ctx.createBuffer (2, len, ctx.sampleRate);
    for (let c = 0; c < 2; ++c) { const d = b.getChannelData (c); let lp = 0, s = 777 + c * 999; const k = 0.05 + 0.9 * (1 - damp / 100);
      for (let i = 0; i < len; ++i) { s = (s * 1664525 + 1013904223) >>> 0; const n = s / 2147483648 - 1; lp += k * (n - lp); d[i] = lp * Math.pow (1 - i / len, 2.2); } }
    return b;
  }
  /** One block: its nodes, and how to set its parameters (p: the values, 0..range). */
  function makeBlock (type) {
    const i = ctx.createGain (), o = ctx.createGain (), dry = ctx.createGain (), wet = ctx.createGain ();
    const mixIn = (node, last) => { i.connect (dry).connect (o); i.connect (node); last.connect (wet).connect (o); };
    const setMix = (m) => { dry.gain.value = 1 - m / 100; wet.gain.value = m / 100; };
    let set; const oscs = [];
    const lfo = (f, kind) => { const x = ctx.createOscillator (); x.type = kind || "sine"; x.frequency.value = f; x.start (); oscs.push (x); return x; };
    const depthOf = (src, param, amount) => { const g = ctx.createGain (); g.gain.value = amount; src.connect (g).connect (param); return g; };
    switch (type) {
      case "eq": { const lo = ctx.createBiquadFilter (), mid = ctx.createBiquadFilter (), hi = ctx.createBiquadFilter (); lo.type = "lowshelf"; mid.type = "peaking"; hi.type = "highshelf";
        i.connect (lo).connect (mid).connect (hi).connect (o);
        set = (p) => { lo.gain.value = p.low; lo.frequency.value = p.lowf; mid.gain.value = p.mid; mid.frequency.value = p.midf; mid.Q.value = p.q; hi.gain.value = p.high; hi.frequency.value = p.highf; }; break; }
      case "filter": { const f = ctx.createBiquadFilter (); i.connect (f).connect (o);
        set = (p) => { f.type = ["lowpass", "highpass", "bandpass"][Math.round (p.mode)] || "lowpass"; f.frequency.value = p.freq; f.Q.value = p.q; }; break; }
      case "drive": { const pre = ctx.createGain (), ws = ctx.createWaveShaper (), post = ctx.createGain (), tone = ctx.createBiquadFilter (); ws.oversample = "4x"; tone.type = "lowpass";
        pre.connect (ws).connect (post).connect (tone); mixIn (pre, tone);
        set = (p) => { const g = dbToGain (p.drive); pre.gain.value = g; post.gain.value = 1 / Math.sqrt (g); ws.curve = curve (Math.round (p.shape)); tone.frequency.value = p.tone; setMix (p.mix); }; break; }
      case "comp": { const c = ctx.createDynamicsCompressor (), mk = ctx.createGain (); c.knee.value = 6; c.connect (mk); mixIn (c, mk);
        set = (p) => { c.threshold.value = p.threshold; c.ratio.value = p.ratio; c.attack.value = p.attack / 1000; c.release.value = p.release / 1000; mk.gain.value = dbToGain (p.makeup); setMix (p.mix); }; break; }
      case "exciter": { const hp = ctx.createBiquadFilter (), ws = ctx.createWaveShaper (), hp2 = ctx.createBiquadFilter (), g = ctx.createGain (); hp.type = hp2.type = "highpass"; ws.curve = curve (0); ws.oversample = "4x";
        const pre = ctx.createGain (); pre.gain.value = 4; i.connect (o); i.connect (hp).connect (pre).connect (ws).connect (hp2).connect (g).connect (o);
        set = (p) => { hp.frequency.value = p.freq; hp2.frequency.value = p.freq * 1.5; g.gain.value = p.amount / 100 * 0.25; }; break; }
      case "delay": { const d = ctx.createDelay (2), fb = ctx.createGain (), tone = ctx.createBiquadFilter (); tone.type = "lowpass";
        d.connect (tone).connect (fb).connect (d); mixIn (d, tone);
        set = (p) => { d.delayTime.value = p.time / 1000; fb.gain.value = p.feedback / 100; tone.frequency.value = p.tone; setMix (p.mix); }; break; }
      case "room": { const pd = ctx.createDelay (1), cv = ctx.createConvolver (); pd.connect (cv); mixIn (pd, cv); let key = "";
        set = (p) => { pd.delayTime.value = p.predelay / 1000; setMix (p.mix); const k = p.size.toFixed (1) + "/" + Math.round (p.damp / 5);
          if (k !== key) { key = k; cv.buffer = impulse (p.size, p.damp); } }; break; }
      case "width": { const sp = ctx.createChannelSplitter (2), mg = ctx.createChannelMerger (2), g = [0, 1, 2, 3].map (() => ctx.createGain ());
        i.connect (sp); sp.connect (g[0], 0); sp.connect (g[1], 1); sp.connect (g[2], 0); sp.connect (g[3], 1);
        g[0].connect (mg, 0, 0); g[1].connect (mg, 0, 0); g[2].connect (mg, 0, 1); g[3].connect (mg, 0, 1); mg.connect (o);
        set = (p) => { const w = p.width / 100; g[0].gain.value = g[3].gain.value = 0.5 + 0.5 * w; g[1].gain.value = g[2].gain.value = 0.5 - 0.5 * w; }; break; }
      case "chorus": {   // two voices a few ms late, their delays swaying opposite ways
        const sp = ctx.createChannelSplitter (2), mg = ctx.createChannelMerger (2), dl = ctx.createDelay (0.05), dr = ctx.createDelay (0.05), osc = lfo (0.8);
        dl.delayTime.value = dr.delayTime.value = 0.014; const gl = depthOf (osc, dl.delayTime, 0), gr = depthOf (osc, dr.delayTime, 0);
        sp.connect (dl, 0); sp.connect (dr, 1); dl.connect (mg, 0, 0); dr.connect (mg, 0, 1); mixIn (sp, mg);
        set = (p) => { osc.frequency.value = p.rate; gl.gain.value = p.depth / 100 * 0.006; gr.gain.value = -p.depth / 100 * 0.006; setMix (p.mix); }; break; }
      case "pan": {   // a pendulum: side to side, or in and out
        const pn = ctx.createStereoPanner (), vg = ctx.createGain (), osc = lfo (0.5); i.connect (pn).connect (vg).connect (o);
        const gp = depthOf (osc, pn.pan, 0), gv = depthOf (osc, vg.gain, 0);
        set = (p) => { osc.frequency.value = p.rate; const d = p.depth / 100, vol = Math.round (p.mode) === 1;
          gp.gain.value = vol ? 0 : d; gv.gain.value = vol ? d / 2 : 0; vg.gain.value = vol ? 1 - d / 2 : 1; }; break; }
      case "wander": {   // a filter drifting like wax in a lava lamp: two slow swings that never line up
        const f = ctx.createBiquadFilter (), a = lfo (0.15), b = lfo (0.15 * 0.618, "triangle"); f.type = "bandpass"; mixIn (f, f);
        const ga = depthOf (a, f.detune, 0), gb = depthOf (b, f.detune, 0);
        set = (p) => { f.frequency.value = p.freq; f.Q.value = p.q; a.frequency.value = p.speed; b.frequency.value = p.speed * 0.618;
          ga.gain.value = p.range * 14; gb.gain.value = p.range * 10; setMix (70); }; break; }
      case "stutter": {   // chops in time: the sound gated on and off, the edges rounded so they never click
        const vg = ctx.createGain (), osc = lfo (4, "square"), sm = ctx.createBiquadFilter (); sm.type = "lowpass"; i.connect (vg).connect (o);
        osc.connect (sm); const gd = depthOf (sm, vg.gain, 0);
        set = (p) => { osc.frequency.value = p.rate; const d = p.depth / 100; gd.gain.value = d / 2; vg.gain.value = 1 - d / 2;
          sm.frequency.value = 400 - 3.7 * p.smooth; }; break; }
      case "crush": {   // fewer bits: the steps of an old sampler
        const ws = ctx.createWaveShaper (), post = ctx.createGain (); post.gain.value = 0.8; ws.connect (post); mixIn (ws, post); let bits = -1;
        set = (p) => { const nb = Math.round (p.bits); if (nb !== bits) { bits = nb; const n = 4096, c = new Float32Array (n), steps = Math.pow (2, nb - 1);
          for (let k = 0; k < n; ++k) c[k] = Math.round ((k / (n - 1) * 2 - 1) * steps) / steps; ws.curve = c; } setMix (p.mix); }; break; }
      case "wow": {   // a tape machine's slow wow and quick flutter, as pitch wobble
        const d = ctx.createDelay (0.1), w = lfo (0.55), fl = lfo (8.5); d.delayTime.value = 0.012; i.connect (d).connect (o);
        const gw = depthOf (w, d.delayTime, 0), gf = depthOf (fl, d.delayTime, 0);
        set = (p) => { gw.gain.value = p.wow / 100 * 0.004; gf.gain.value = p.flutter / 100 * 0.0004; }; break; }
      case "shimmer": {   // a long, bright space fed an octave up (a rectifier doubles every note)
        const ws = ctx.createWaveShaper (), hp = ctx.createBiquadFilter (), up = ctx.createGain (), plain = ctx.createGain (), cv = ctx.createConvolver (), trim = ctx.createGain ();
        const c = new Float32Array (2048); for (let k = 0; k < 2048; ++k) c[k] = Math.abs (k / 2047 * 2 - 1); ws.curve = c; hp.type = "highpass"; hp.frequency.value = 180; trim.gain.value = 0.7;
        const feed = ctx.createGain (); ws.connect (hp).connect (up).connect (feed); plain.connect (feed); feed.connect (cv).connect (trim);
        i.connect (dry).connect (o); i.connect (ws); i.connect (plain); trim.connect (wet).connect (o); let key = "";
        set = (p) => { up.gain.value = p.octave / 100 * 1.4; plain.gain.value = 1 - p.octave / 200; setMix (p.mix);
          const k = p.size.toFixed (1); if (k !== key) { key = k; cv.buffer = impulse (p.size, 10); } }; break; }
      default: { i.connect (o); set = (p) => { o.gain.value = dbToGain (p.gain); }; }
    }
    return { input: i, output: o, set, stop: () => oscs.forEach ((x) => { try { x.stop (); } catch (_) { /* stopped */ } }) };
  }

  /** The block parameters as the knobs set them: each wired part moves its parameter across its range. */
  function liveChain (d) {
    const chain = d.dsp.chain.map ((b) => ({ b: b.b, on: b.on, p: Object.assign ({}, b.p) }));
    for (const q of d.parts) {
      if (!q.ctl) continue;
      const [bi, key] = q.ctl.split ("."), b = chain[Number (bi)]; if (!b) continue;
      if (key === "on") { b.on = !!q.on; continue; }
      const def = B[b.b][1][key]; if (!def) continue;
      let t = q.type === "selector" ? q.value / Math.max (1, String (q.stops || "").split ("|").length - 1) : q.type === "toggle" || q.type === "button" ? (q.on ? 1 : 0) : q.value / 100;
      t = Math.min (1, Math.max (0, t));
      b.p[key] = def[5] && def[1] > 0 ? def[1] * Math.pow (def[2] / def[1], t) : def[1] + (def[2] - def[1]) * t;
    }
    return chain;
  }

  function ensureContext () {
    if (ctx) return;
    ctx = new (window.AudioContext || window.webkitAudioContext) ();
    input = ctx.createGain (); chainOut = ctx.createGain ();
    limiter = ctx.createDynamicsCompressor (); limiter.threshold.value = -3; limiter.knee.value = 0; limiter.ratio.value = 20; limiter.attack.value = 0.002; limiter.release.value = 0.1;
    volume = ctx.createGain (); analyser = ctx.createAnalyser (); analyser.fftSize = 1024;
    chainOut.connect (limiter).connect (volume).connect (analyser).connect (ctx.destination);
    setVolume ();
  }
  /** Rebuilds the graph when the chain's shape (its blocks, which are in) changes; otherwise only sets values. */
  function apply () {
    if (!ctx) return;
    const chain = liveChain (D.get ()), sig = bypass + "|" + chain.map ((b) => b.b + (b.on ? "1" : "0")).join (",");
    if (sig !== builtSig) {
      builtSig = sig;
      input.disconnect (); blocks.forEach ((b) => { if (b) { b.output.disconnect (); b.stop (); } });
      blocks = chain.map ((b) => (b.on && !bypass ? makeBlock (b.b) : null));
      let at = input;
      blocks.forEach ((blk) => { if (blk) { at.connect (blk.input); at = blk.output; } });
      at.connect (chainOut);
    }
    chain.forEach ((b, k) => { if (blocks[k]) blocks[k].set (b.p); });
  }
  function setVolume () { if (volume) volume.gain.value = dbToGain (Number (($("snd-vol") || {}).value || -12)); }

  async function play (on) {
    ensureContext ();
    if (source) { try { source.stop (); } catch (_) { /* already stopped */ } source.disconnect (); source = null; }
    playing = on;
    $("snd-play").textContent = on ? "Stop" : "Play";
    $("snd-play").setAttribute ("aria-pressed", String (on));
    if (!on) return;
    await ctx.resume ();
    const buf = current || await renderSong ($("snd-song").value);
    if (!playing) return;
    source = ctx.createBufferSource (); source.buffer = buf; source.loop = true;
    source.connect (input); apply (); source.start ();
    meter ();
  }
  function meter () {
    if (!playing || !analyser) { $("snd-meter").style.transform = "scaleX(0)"; lastLevel = 0; lastWave = null; return; }
    const a = new Float32Array (analyser.fftSize); analyser.getFloatTimeDomainData (a);
    lastWave = a;
    let pk = 0; for (const v of a) pk = Math.max (pk, Math.abs (v));
    const db = 20 * Math.log10 (pk + 1e-6); lastLevel = Math.max (0, Math.min (1, (db + 48) / 48)); $("snd-meter").style.transform = "scaleX(" + Math.max (0, Math.min (1, (db + 48) / 48)).toFixed (3) + ")";
    requestAnimationFrame (meter);
  }

  // ------------------------------------------------------------------------------------------------
  // The Sound tab
  function el (tag, props, parent) { const e = document.createElement (tag); Object.assign (e, props || {}); if (parent) parent.appendChild (e); return e; }
  function fmt (v, def) { const u = def[4]; return (Math.abs (v) >= 100 ? Math.round (v) : Math.round (v * 10) / 10) + (u ? " " + u : ""); }
  function chainUi () {
    const box = $("snd-chain"); if (!box) return;
    const d = D.get (), chain = d.dsp.chain;
    box.replaceChildren ();
    if (!chain.length) el ("li", { className: "muted small", textContent: "No blocks yet: add one below, or pick a sound preset." }, box);
    chain.forEach ((b, k) => {
      const li = el ("li", { className: "snd-block" }, box), head = el ("div", { className: "snd-head" }, li);
      const on = el ("input", { type: "checkbox", checked: b.on, title: "In / out" }, head);
      on.addEventListener ("change", () => { const c = JSON.parse (JSON.stringify (D.get ().dsp)); c.chain[k].on = on.checked; D.setDsp (c); });
      el ("b", { textContent: (k + 1) + ". " + B[b.b][0] }, head);
      const move = (dir) => { const c = JSON.parse (JSON.stringify (D.get ().dsp)); const j = k + dir; if (j < 0 || j >= c.chain.length) return; [c.chain[k], c.chain[j]] = [c.chain[j], c.chain[k]]; D.setDsp (c); };
      for (const [t, f, lbl] of [["↑", () => move (-1), "Earlier"], ["↓", () => move (1), "Later"], ["✕", () => { const c = JSON.parse (JSON.stringify (D.get ().dsp)); c.chain.splice (k, 1); D.setDsp (c); }, "Remove"]]) {
        const btn = el ("button", { type: "button", textContent: t, title: lbl }, head); btn.setAttribute ("aria-label", lbl); btn.addEventListener ("click", f); }
      for (const key in B[b.b][1]) {
        const def = B[b.b][1][key], row = el ("label", { className: "snd-param" }, li);
        el ("span", { textContent: def[0] }, row);
        const wired = d.parts.some ((q) => q.ctl === k + "." + key);
        const r = el ("input", { type: "range", min: 0, max: 1000, value: Math.round (1000 * (def[5] && def[1] > 0 ? Math.log (b.p[key] / def[1]) / Math.log (def[2] / def[1]) : (b.p[key] - def[1]) / (def[2] - def[1]))) }, row);
        const out = el ("output", { textContent: wired ? "on a knob" : fmt (b.p[key], def) }, row);
        r.disabled = wired;
        r.addEventListener ("input", () => { const t = r.value / 1000, v = def[5] && def[1] > 0 ? def[1] * Math.pow (def[2] / def[1], t) : def[1] + (def[2] - def[1]) * t;
          out.textContent = fmt (v, def); const live = D.get (); live.dsp.chain[k].p[key] = v; apply (); });
        r.addEventListener ("change", () => { const c = JSON.parse (JSON.stringify (D.get ().dsp)); D.setDsp (c); });
      }
    });
    $("snd-add").disabled = chain.length >= D.MAX_BLOCKS;
  }

  function bind () {
    const song = $("snd-song"); if (!song) return;
    for (const s of SONGS) el ("option", { value: s, textContent: s }, song);
    song.addEventListener ("change", () => { current = null; if (playing) play (true); });
    $("snd-play").addEventListener ("click", () => play (!playing));
    $("snd-file").addEventListener ("change", async (e) => {
      const f = e.target.files && e.target.files[0]; if (!f) return;
      if (f.size > 60 * 1024 * 1024) { $("snd-msg").textContent = "That file is too big (60 MB at most)."; return; }
      ensureContext ();
      try { current = await ctx.decodeAudioData (await f.arrayBuffer ()); $("snd-msg").textContent = "Playing your file (it stays on your computer)."; play (true); }
      catch (_) { $("snd-msg").textContent = "That file could not be read as audio."; }
    });
    $("snd-vol").addEventListener ("input", setVolume);
    $("snd-bypass").addEventListener ("change", (e) => { bypass = e.target.checked; apply (); });
    const pre = $("snd-preset");
    PRESETS.forEach ((p, i) => el ("option", { value: String (i), textContent: p.name }, pre));
    $("snd-preset-go").addEventListener ("click", () => { const p = PRESETS[Number (pre.value)]; if (p) D.setDsp ({ chain: JSON.parse (JSON.stringify (p.chain)) }); });
    $("snd-good").addEventListener ("click", async (e) => {
      if (!D.get ().dsp.chain.length) { $("snd-msg").textContent = "Add a block or load a sound preset first."; return; }
      e.target.disabled = true; $("snd-msg").textContent = "Listening…";
      try { $("snd-msg").textContent = await makeItGood (); } catch (_) { $("snd-msg").textContent = "That didn't work in this browser."; }
      e.target.disabled = false;
    });
    const add = $("snd-add-type");
    for (const t in B) el ("option", { value: t, textContent: B[t][0] }, add);
    $("snd-add").addEventListener ("click", () => { const c = JSON.parse (JSON.stringify (D.get ().dsp)); c.chain.push ({ b: add.value, on: true, p: {} }); D.setDsp (c); });
    let uiSig = "";
    D.subscribe ((d) => {
      apply ();
      const sig = JSON.stringify (d.dsp) + d.parts.map ((q) => q.ctl || "").join ("|");   // (rebuilt only when the chain or wiring changes)
      if (sig !== uiSig) { uiSig = sig; chainUi (); }
    });
  }
  // what is playing now, for the designer's screens (a level 0..1 and the last waveform; null when nothing plays)
  // "Make it sound good": a sensible order (clean-up, dynamics, colour, movement, space, width, level), extremes
  // tamed, and the output set so the unit is as loud as the dry sound (measured on the song, offline)
  const ORDER = ["filter", "eq", "comp", "drive", "exciter", "crush", "wow", "chorus", "wander", "stutter", "pan", "delay", "shimmer", "room", "width", "gain"];
  const TAME = [["delay", "feedback", (v) => Math.min (v, 70)], ["room", "mix", (v) => Math.min (v, 40)], ["shimmer", "mix", (v) => Math.min (v, 40)],
                ["stutter", "depth", (v) => Math.min (v, 85)], ["width", "width", (v) => Math.min (v, 160)], ["comp", "ratio", (v) => Math.min (v, 10)],
                ["drive", "drive", (v) => Math.min (v, 24)], ["exciter", "amount", (v) => Math.min (v, 60)], ["crush", "bits", (v) => Math.max (v, 4)]];
  async function renderThrough (buf, chain) {
    const live = ctx, off = new OfflineAudioContext (2, buf.length, buf.sampleRate); ctx = off;
    try {
      const src = off.createBufferSource (); src.buffer = buf; let at = src; const made = [];
      for (const b of chain) { if (!b.on) continue; const blk = makeBlock (b.b); blk.set (Object.assign (blockDefaultsOf (b.b), b.p)); at.connect (blk.input); at = blk.output; made.push (blk); }
      at.connect (off.destination); src.start ();
      const r = await off.startRendering (); made.forEach ((m) => m.stop ()); return r;
    } finally { ctx = live; }
  }
  const blockDefaultsOf = (t) => Object.fromEntries (Object.entries (B[t][1]).map (([k, v]) => [k, v[3]]));
  const rmsOf = (b) => { let s = 0; for (let c = 0; c < b.numberOfChannels; ++c) { const d = b.getChannelData (c); for (let i = 0; i < d.length; ++i) s += d[i] * d[i]; } return Math.sqrt (s / (b.length * b.numberOfChannels)); };
  async function makeItGood () {
    const d = D.get (), old = d.dsp.chain.map ((b, i) => ({ b: b.b, on: b.on, p: Object.assign ({}, b.p), was: i }));
    const notes = [];
    const sorted = old.slice ().sort ((a, b) => ORDER.indexOf (a.b) - ORDER.indexOf (b.b));
    if (sorted.some ((b, i) => b.was !== i)) notes.push ("put the blocks in a better order");
    let tamed = 0;
    for (const b of sorted) for (const [t, k, f] of TAME) if (b.b === t) { const v = b.p[k] ?? blockDefaultsOf (t)[k], nv = f (v); if (nv !== v) { b.p[k] = nv; ++tamed; } }
    if (tamed) notes.push ("tamed " + tamed + " extreme setting" + (tamed > 1 ? "s" : ""));
    // the level: the chain against the dry song, matched with an Output block at the end (within 12 dB)
    let gi = sorted.findIndex ((b) => b.b === "gain");
    if (gi >= 0) sorted.push (...sorted.splice (gi, 1));
    const song = await renderSong (($("snd-song") || {}).value || SONGS[0]);
    const withGain = (g) => sorted.filter ((b) => b.b !== "gain").concat (g == null ? [] : [{ b: "gain", on: true, p: { gain: g } }]);
    const dryRms = rmsOf (song), wetRms = rmsOf (await renderThrough (song, withGain (null)));
    const need = Math.max (-12, Math.min (12, 20 * Math.log10 ((dryRms + 1e-9) / (wetRms + 1e-9))));
    gi = sorted.findIndex ((b) => b.b === "gain");
    if (Math.abs (need) >= 0.5 || gi >= 0) {
      if (gi >= 0) sorted[gi].p.gain = Math.round (need * 10) / 10;
      else if (sorted.length < D.MAX_BLOCKS) sorted.push ({ b: "gain", on: true, p: { gain: Math.round (need * 10) / 10 }, was: -1 });
      notes.push ("matched the level (" + (need >= 0 ? "+" : "") + need.toFixed (1) + " dB)");
    }
    const remap = {}; sorted.forEach ((b, i) => { if (b.was >= 0) remap[b.was] = i; });
    D.setDsp ({ chain: sorted.map ((b) => ({ b: b.b, on: b.on, p: b.p })) }, remap);
    return notes.length ? "Done: " + notes.join ("; ") + "." : "It already sounds balanced - nothing to change.";
  }

  /** ?selftest: every block type, at its defaults, through an offline context (nothing is heard): does it build,
      change the sound, stay finite and stay in bounds? Returns [type, ok, note] each. */
  async function testBlocks () {
    const out = [], live = ctx;
    for (const type of Object.keys (B)) {
      const off = new OfflineAudioContext (2, 24000, 48000); ctx = off;
      try {
        const src = off.createBufferSource (), buf = off.createBuffer (2, 24000, 48000);
        for (let c = 0; c < 2; ++c) { const d = buf.getChannelData (c); for (let i = 0; i < d.length; ++i) d[i] = 0.3 * Math.sin (i * 0.031 * (c + 1)) + 0.1 * Math.sin (i * 0.37); }
        src.buffer = buf; const blk = makeBlock (type); blk.set (Object.fromEntries (Object.entries (B[type][1]).map (([k, v]) => [k, v[3]])));
        src.connect (blk.input); blk.output.connect (off.destination); src.start ();
        const r = await off.startRendering (); blk.stop ();
        let diff = 0, peak = 0, fin = true;
        for (let c = 0; c < 2; ++c) { const y = r.getChannelData (c), x = buf.getChannelData (c); for (let i = 0; i < y.length; ++i) { fin = fin && Number.isFinite (y[i]); diff += (y[i] - x[i]) ** 2; peak = Math.max (peak, Math.abs (y[i])); } }
        const rms = Math.sqrt (diff / 48000), neutral = type === "gain" || type === "eq";   // (these two are flat at their defaults)
        out.push ([type, fin && peak < 1.2 && (neutral || rms > 0.002), "change " + rms.toFixed (4) + ", peak " + peak.toFixed (2)]);
      } catch (e) { out.push ([type, false, String (e)]); }
    }
    ctx = live;
    return out;
  }
  window.ENHSound = Object.freeze ({ PRESETS, SONGS, renderSong, levelNow: () => lastLevel, waveNow: () => lastWave, testBlocks, makeItGood });
  if (document.readyState === "loading") document.addEventListener ("DOMContentLoaded", bind); else bind ();
}());
