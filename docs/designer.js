/* ENH Master - Rack Unit Designer.
   Everything runs here, in the visitor's browser. A design is a small plain object; a share code is that
   object, packed (short keys), compressed (deflate) and base64url-encoded - the code IS the design, so
   nothing is uploaded or stored anywhere. Codes from others are untrusted input: sanitize() rebuilds the
   design from scratch, keeping only known fields with clamped numbers, known choices, #rrggbb colours
   and short plain text (always shown as text, never as markup). */
(function () {
  "use strict";

  // ---------------------------------------------------------------------------------------------------
  // Model
  const W = 482.6, U = 44.45, EAR = 15.0;            // a 19-inch panel in mm; 1U; the rack ears' width
  const MAX_PARTS = 150, MAX_CODE = 24000, MAX_JSON = 120000;
  const KNOBS = ["tophat", "knurled", "fluted", "ribbed", "matte", "redtrim", "capblue", "capred", "capwhite", "chicken", "pointer"];
  const KNOB_NAMES = { tophat: "Fluted top hat", knurled: "Knurled aluminium", fluted: "Fluted, skirted", ribbed: "Small ribbed",
    matte: "Smooth matte", redtrim: "Red anodised", capblue: "Blue cap", capred: "Red cap", capwhite: "White cap",
    chicken: "Chicken head", pointer: "Pointer bar" };
  const TYPES = {
    knob:    { label: "Knob", w: 22, h: 22, defaults: { style: "ribbed", value: 50, text: "GAIN", scale: true, min: 0, max: 10, size: 22, pointer: "auto" } },
    toggle:  { label: "Toggle", w: 10, h: 18, defaults: { style: "bat", on: true, text: "IN" } },
    button:  { label: "Button", w: 12, h: 10, defaults: { style: "square", on: false, colour: "#e0a84a", text: "BYPASS" } },
    led:     { label: "LED", w: 4, h: 4, defaults: { colour: "#46e070", on: true, text: "" } },
    vu:      { label: "VU meter", w: 64, h: 36, defaults: { style: "cream", value: 55, text: "VU" } },
    ladder:  { label: "LED ladder", w: 6, h: 40, defaults: { segments: 10, value: 60, text: "" } },
    display: { label: "Display", w: 90, h: 30, defaults: { colour: "#56c8f5", text: "ENH" } },
    label:   { label: "Text", w: 40, h: 8, defaults: { text: "LABEL", size: 5, bold: true, align: "center" } },
    box:     { label: "Section box", w: 90, h: 34, defaults: { text: "SECTION", round: 3, fill: false } },
    line:    { label: "Line", w: 60, h: 1, defaults: {} },
    jack:    { label: "Jack", w: 14, h: 14, defaults: { style: "trs", text: "INPUT" } },
    screw:   { label: "Screw", w: 5, h: 5, defaults: {} },
    vent:    { label: "Vent slots", w: 40, h: 16, defaults: { count: 6 } },
  };
  const FINISHES = ["anodised", "brushed", "paint", "hammertone"], EARS = ["slots", "holes", "none"];
  const HANDLES = ["none", "bar", "loop"], SCREWS = ["phillips", "hex", "thumb"];
  const TOGGLES = ["bat", "rocker", "rockerred"], BUTTONS = ["square", "round"], VUS = ["cream", "amber", "black"], JACKS = ["trs", "xlr"];
  const EDGES = ["square", "rounded", "bevel"], FONTS = ["sans", "serif", "mono", "condensed"], EARCOLS = ["match", "black", "silver"];
  const ALIGNS = ["left", "center", "right"], POINTERS = ["auto", "white", "cream", "black", "red"];
  const FONT_FAMILY = { sans: "Inter, Segoe UI, Helvetica, Arial, sans-serif", serif: "Georgia, Times New Roman, serif",
    mono: "ui-monospace, Menlo, Consolas, monospace", condensed: "Arial Narrow, Roboto Condensed, Helvetica Neue, sans-serif" };
  const POINTER_COLOUR = { white: "#f2f2f2", cream: "#e9dfc6", black: "#111111", red: "#d8322b" };

  const blank = () => ({ v: 1, unit: { name: "MY UNIT", model: "EM-X", height: 1, finish: "anodised", colour: "#16171a",
    ink: "#e8e8ea", ears: "slots", handles: "none", screws: "phillips", wear: 15, edge: "rounded", font: "sans", badge: "",
    earColour: "match", sub: "" }, parts: [] });

  let design = blank(), selected = [], history = [], future = [], play = false, zoom = 1, snap = true, nextId = 1;

  // ---------------------------------------------------------------------------------------------------
  // Sanitizing: the only way anything from outside (a share code, the browser's saved copy) gets in
  const clamp = (v, lo, hi, d) => { const n = Number (v); return Number.isFinite (n) ? Math.min (hi, Math.max (lo, n)) : d; };
  const pick = (v, list, d) => (list.includes (v) ? v : d);
  const colour = (v, d) => (typeof v === "string" && /^#[0-9a-fA-F]{6}$/.test (v) ? v.toLowerCase() : d);
  const text = (v, max, d) => {
    if (typeof v !== "string") return d;
    // printable characters only (letters in any script, digits, common punctuation), no controls or markup
    return v.replace (/[^\p{L}\p{N} .,:;!?&%+\-/'()#°|_]/gu, "").slice (0, max);
  };
  const bool = (v, d) => (typeof v === "boolean" ? v : d);

  function sanitize (raw) {
    const d = blank();
    if (!raw || typeof raw !== "object") return d;
    const u = raw.unit && typeof raw.unit === "object" ? raw.unit : {};
    d.unit = {
      name: text (u.name, 40, d.unit.name), model: text (u.model, 24, d.unit.model),
      height: Math.round (clamp (u.height, 1, 4, 1)), finish: pick (u.finish, FINISHES, "anodised"),
      colour: colour (u.colour, d.unit.colour), ink: colour (u.ink, d.unit.ink), ears: pick (u.ears, EARS, "slots"),
      handles: pick (u.handles, HANDLES, "none"), screws: pick (u.screws, SCREWS, "phillips"), wear: Math.round (clamp (u.wear, 0, 100, 15)),
      edge: pick (u.edge, EDGES, "rounded"), font: pick (u.font, FONTS, "sans"), badge: text (u.badge, 16, ""),
      earColour: pick (u.earColour, EARCOLS, "match"), sub: text (u.sub, 40, ""),
    };
    const H = d.unit.height * U;
    const parts = Array.isArray (raw.parts) ? raw.parts.slice (0, MAX_PARTS * 4) : [];   // (read a bounded amount)
    for (const p of parts) {
      if (d.parts.length >= MAX_PARTS) break;
      if (!p || typeof p !== "object" || !Object.prototype.hasOwnProperty.call (TYPES, p.type)) continue;
      const t = TYPES[p.type], def = t.defaults;
      const q = { id: nextId++, type: p.type,
        x: clamp (p.x, 0, W, W / 2), y: clamp (p.y, 0, H, H / 2),
        w: clamp (p.w, 1, W, t.w), h: clamp (p.h, 0.5, 4 * U, t.h), rot: clamp (p.rot, -180, 180, 0) };
      if ("style" in def) q.style = pick (p.style, p.type === "knob" ? KNOBS : p.type === "toggle" ? TOGGLES : p.type === "button" ? BUTTONS
                                                  : p.type === "vu" ? VUS : JACKS, def.style);
      if ("value" in def) q.value = clamp (p.value, 0, 100, def.value);
      if ("text" in def) q.text = text (p.text, 40, def.text);
      if ("scale" in def) q.scale = bool (p.scale, def.scale);
      if ("min" in def) q.min = clamp (p.min, -99, 999, def.min);
      if ("max" in def) q.max = clamp (p.max, -99, 999, def.max);
      if ("on" in def) q.on = bool (p.on, def.on);
      if ("colour" in def) q.colour = colour (p.colour, def.colour);
      if ("segments" in def) q.segments = Math.round (clamp (p.segments, 3, 24, def.segments));
      if ("size" in def && p.type === "label") q.size = clamp (p.size, 2, 20, def.size);
      if ("bold" in def) q.bold = bool (p.bold, def.bold);
      if ("round" in def) q.round = clamp (p.round, 0, 12, def.round);
      if ("count" in def) q.count = Math.round (clamp (p.count, 2, 30, def.count));
      if ("align" in def) q.align = pick (p.align, ALIGNS, def.align);
      if ("pointer" in def) q.pointer = pick (p.pointer, POINTERS, def.pointer);
      if ("fill" in def) q.fill = bool (p.fill, def.fill);
      q.lock = bool (p.lock, false);
      d.parts.push (q);
    }
    return d;
  }

  // ---------------------------------------------------------------------------------------------------
  // Share codes: short keys -> JSON -> deflate -> base64url, prefixed "ENH1."
  const SHORT = { type: "t", x: "x", y: "y", w: "w", h: "h", rot: "r", style: "s", value: "v", text: "l", scale: "c", min: "a", max: "b",
    on: "o", colour: "k", segments: "g", size: "z", bold: "d", round: "n", count: "u", align: "e", pointer: "i", fill: "f", lock: "q" };
  const LONG = Object.fromEntries (Object.entries (SHORT).map (([a, b]) => [b, a]));
  const round1 = (n) => Math.round (n * 10) / 10;

  function pack (d) {
    return { v: 1, u: d.unit, p: d.parts.map ((p) => { const o = {}; for (const k in SHORT) if (k in p) o[SHORT[k]] = typeof p[k] === "number" ? round1 (p[k]) : p[k]; return o; }) };
  }
  function unpack (o) {
    if (!o || typeof o !== "object") return null;
    return { unit: o.u, parts: Array.isArray (o.p) ? o.p.map ((p) => { const q = {}; if (p && typeof p === "object") for (const k in p) if (LONG[k]) q[LONG[k]] = p[k]; return q; }) : [] };
  }
  const b64u = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode (b); return btoa (s).replace (/\+/g, "-").replace (/\//g, "_").replace (/=+$/, ""); };
  const unb64u = (s) => { const b = atob (s.replace (/-/g, "+").replace (/_/g, "/")); return Uint8Array.from (b, (c) => c.charCodeAt (0)); };
  async function streamBytes (bytes, stream) {
    const out = new Response (new Blob ([bytes]).stream().pipeThrough (stream));
    return new Uint8Array (await out.arrayBuffer());
  }
  async function encode (d) {
    const json = new TextEncoder().encode (JSON.stringify (pack (d)));
    const z = typeof CompressionStream === "function" ? await streamBytes (json, new CompressionStream ("deflate-raw")) : null;
    return z ? "ENH1." + b64u (z) : "ENH0." + b64u (json);
  }
  async function decode (code) {
    code = String (code || "").trim().replace (/^.*#d=/, "").replace (/\s+/g, "");
    if (code.length > MAX_CODE) throw new Error ("That code is too long to be a design.");
    const m = /^ENH([01])\.([A-Za-z0-9_-]+)$/.exec (code);
    if (!m) throw new Error ("That is not an ENH Master design code (they start with ENH1.).");
    let bytes = unb64u (m[2]);
    if (m[1] === "1") {
      if (typeof DecompressionStream !== "function") throw new Error ("This browser cannot open compressed codes.");
      bytes = await streamBytes (bytes, new DecompressionStream ("deflate-raw"));
    }
    if (bytes.length > MAX_JSON) throw new Error ("That code unpacks to too much to be a design.");
    return sanitize (unpack (JSON.parse (new TextDecoder().decode (bytes))));
  }

  // ---------------------------------------------------------------------------------------------------
  // Drawing (SVG, built with DOM calls only: never markup from data)
  const NS = "http://www.w3.org/2000/svg";
  const svg = document.getElementById ("svg");
  const el = (name, attrs, parent) => { const e = document.createElementNS (NS, name); for (const k in attrs) e.setAttribute (k, attrs[k]); if (parent) parent.appendChild (e); return e; };
  const txt = (parent, x, y, s, size, fill, opts = {}) => {
    const t = el ("text", { x, y, "font-size": size, fill, "text-anchor": opts.anchor || "middle", "font-family": FONT_FAMILY[design.unit.font] || FONT_FAMILY.sans,
      "font-weight": opts.bold === false ? 500 : 700, "letter-spacing": opts.spacing ?? size * 0.12, "dominant-baseline": "middle" }, parent);
    t.textContent = s;   // text, never markup
    return t;
  };
  const shade = (hex, f) => { const n = parseInt (hex.slice (1), 16); const c = [n >> 16, (n >> 8) & 255, n & 255].map ((v) => Math.max (0, Math.min (255, Math.round (f >= 0 ? v + (255 - v) * f : v * (1 + f)))));
    return "#" + c.map ((v) => v.toString (16).padStart (2, "0")).join (""); };

  // "edit": everything, as in the editor; "print": only what lies flat on the plate (its paint, print, meter
  // faces, screens) - the 3D view lays that on its faceplate and builds the knobs, switches and screws itself
  let mode = "edit";
  function defs (root) {
    const d = el ("defs", {}, root);
    const g = (id, stops, x2 = 0, y2 = 1) => { const lg = el ("linearGradient", { id, x1: 0, y1: 0, x2, y2 }, d); stops.forEach (([o, c, a = 1]) => el ("stop", { offset: o, "stop-color": c, "stop-opacity": a }, lg)); };
    const r = (id, stops, cx = 0.35, cy = 0.3) => { const rg = el ("radialGradient", { id, cx, cy, r: 0.75 }, d); stops.forEach (([o, c]) => el ("stop", { offset: o, "stop-color": c }, rg)); };
    g ("sheen", [[0, "#ffffff", 0.16], [0.45, "#ffffff", 0.02], [1, "#000000", 0.18]]);
    g ("metalV", [[0, "#f4f4f6"], [0.5, "#b9bbc0"], [1, "#e6e7ea"]], 1, 0);
    r ("knobBlack", [[0, "#4a4a4f"], [0.45, "#141416"], [1, "#050506"]]);
    r ("knobMatte", [[0, "#2c2c30"], [1, "#0b0b0d"]]);
    r ("knobAlu", [[0, "#fafafa"], [0.5, "#c7c8cc"], [1, "#8d8f94"]]);
    r ("knobRed", [[0, "#d44c46"], [0.6, "#8e1d1b"], [1, "#4a0d0c"]]);
    r ("capBlue", [[0, "#6d8fd8"], [1, "#1c3572"]]);
    r ("capRed", [[0, "#e36a62"], [1, "#7a1a16"]]);
    r ("capWhite", [[0, "#ffffff"], [1, "#bdbcb6"]]);
    r ("glass", [[0, "#ffffff"], [1, "#ffffff"]]);
    g ("vuCream", [[0, "#f3ebd2"], [1, "#d8cba5"]]);
    g ("vuAmber", [[0, "#ffd585"], [1, "#d88e2c"]]);
    g ("vuBlack", [[0, "#26272b"], [1, "#0d0e10"]]);
    // brushed lines and hammertone dimples, as filters (they only shade: the panel's colour stays its own)
    const br = el ("filter", { id: "brushed", x: 0, y: 0, width: 1, height: 1 }, d);
    el ("feTurbulence", { type: "fractalNoise", baseFrequency: "0.004 0.9", numOctaves: 2, seed: 3, result: "n" }, br);
    el ("feColorMatrix", { in: "n", type: "matrix", values: "0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0.45 0.45 0.45 0 -0.3", result: "a" }, br);
    el ("feComposite", { in: "a", in2: "SourceGraphic", operator: "in" }, br);
    const hm = el ("filter", { id: "hammer", x: 0, y: 0, width: 1, height: 1 }, d);
    el ("feTurbulence", { type: "turbulence", baseFrequency: 0.09, numOctaves: 2, seed: 7, result: "n" }, hm);
    el ("feDiffuseLighting", { in: "n", "surface-scale": 1.4, "lighting-color": "#ffffff", result: "l" }, hm).appendChild (el ("feDistantLight", { azimuth: 225, elevation: 55 }));
    el ("feComposite", { in: "l", in2: "SourceGraphic", operator: "arithmetic", k1: 0.9, k2: 0, k3: 0, k4: 0 }, hm);
    const sh = el ("filter", { id: "drop", x: -0.5, y: -0.5, width: 2, height: 2 }, d);
    el ("feDropShadow", { dx: 0.9, dy: 1.6, stdDeviation: 1.1, "flood-color": "#000", "flood-opacity": 0.55 }, sh);
    const wear = el ("filter", { id: "wear", x: 0, y: 0, width: 1, height: 1 }, d);
    el ("feTurbulence", { type: "fractalNoise", baseFrequency: "0.02 0.3", numOctaves: 3, seed: 11, result: "n" }, wear);
    el ("feColorMatrix", { in: "n", type: "matrix", values: "0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 2.2 -1.35" }, wear);
  }

  function render (root = svg, rmode = "edit") {
    mode = rmode;
    while (root.lastChild && root.lastChild.nodeName !== "title") root.removeChild (root.lastChild);
    const u = design.unit, H = u.height * U, print = mode === "print";
    root.setAttribute ("viewBox", print ? `0 0 ${W} ${H}` : `-6 -6 ${W + 12} ${H + 12}`);
    root.setAttribute ("width", print ? W : (W + 12) * 2 * zoom);
    root.setAttribute ("height", print ? H : (H + 12) * 2 * zoom);
    defs (root);
    const face = el ("g", {}, root);
    const rx = u.edge === "square" ? 0.2 : u.edge === "bevel" ? 0.6 : 1.4;
    // The plate: shadow, body, finish, edge highlight, wear
    if (!print) el ("rect", { x: 0.8, y: 1.6, width: W, height: H, rx, fill: "#000", opacity: 0.55, filter: "url(#drop)" }, face);
    el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: u.colour }, face);
    if (u.finish === "brushed") el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: "#fff", filter: "url(#brushed)", opacity: 0.35 }, face);
    if (u.finish === "hammertone") el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: u.colour, filter: "url(#hammer)", opacity: 0.45 }, face);
    if (!print) el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: "url(#sheen)", opacity: u.finish === "anodised" ? 0.7 : 1 }, face);
    if (u.wear > 0) el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: "#fff", filter: "url(#wear)", opacity: u.wear / 400 }, face);
    // Ears in their own finish (black or silver) where asked
    if (u.ears !== "none" && u.earColour !== "match")
      for (const x of [0, W - EAR]) el ("rect", { x, y: 0, width: EAR, height: H, rx, fill: u.earColour === "black" ? "#0d0d0f" : "#c3c4c8" }, face);
    if (u.edge === "bevel" && !print) el ("rect", { x: 1.2, y: 1.2, width: W - 2.4, height: H - 2.4, rx: 0.4, fill: "none", stroke: shade (u.colour, 0.5), "stroke-width": 0.4, opacity: 0.6 }, face);
    if (!print) el ("rect", { x: 0.25, y: 0.25, width: W - 0.5, height: H - 0.5, rx: Math.max (0.1, rx - 0.2), fill: "none", stroke: shade (u.colour, 0.35), "stroke-width": 0.5, opacity: 0.7 }, face);
    // Ears and rack screws
    if (u.ears !== "none") for (const x of [EAR / 2, W - EAR / 2]) for (let k = 0; k < u.height; ++k) {
      const y = (k + 0.5) * U;
      if (u.ears === "slots") el ("rect", { x: x - 3.8, y: y - 1.9, width: 7.6, height: 3.8, rx: 1.9, fill: "#050506" }, face);
      else el ("circle", { cx: x, cy: y, r: 2.4, fill: "#050506" }, face);
      if (!print) screw (face, x, y, u.screws, 3.2);
    }
    if (u.handles !== "none" && !print) for (const x of [EAR + 6, W - EAR - 6]) {
      if (u.handles === "bar") { el ("rect", { x: x - 2.5, y: 4, width: 5, height: H - 8, rx: 2.5, fill: "url(#metalV)", filter: "url(#drop)" }, face); }
      else { const pth = `M ${x} 5 C ${x + 9} 5 ${x + 9} ${H - 5} ${x} ${H - 5}`; el ("path", { d: pth, fill: "none", stroke: "#c9cacf", "stroke-width": 3.2, "stroke-linecap": "round", filter: "url(#drop)" }, face); }
    }
    // Maker block (name, model, a second line), and a badge on the right if it has text
    const left = u.ears === "none" ? 8 : EAR + (u.handles !== "none" ? 18 : 8);
    txt (face, left, 9, u.name, 4.4, u.ink, { anchor: "start", spacing: 0.9 });
    txt (face, left, 14.5, "MODEL " + u.model, 2.3, u.ink, { anchor: "start", spacing: 0.4, bold: false });
    if (u.sub) txt (face, left, 19, u.sub, 2.1, u.ink, { anchor: "start", spacing: 0.35, bold: false });
    if (u.badge) {
      const bx = W - (u.ears === "none" ? 26 : EAR + (u.handles !== "none" ? 36 : 26)), bw = Math.max (22, u.badge.length * 2.6 + 8);
      el ("rect", { x: bx - bw / 2, y: 4, width: bw, height: 8, rx: 4, fill: shade (u.colour, -0.5), stroke: u.ink, "stroke-width": 0.35 }, face);
      txt (face, bx, 8, u.badge, 3, u.ink, { spacing: 0.5 });
    }
    // Parts, back to front
    for (const p of design.parts) drawPart (face, p);
    // Selection outlines
    if (!play && !print && root === svg) for (const p of design.parts) if (selected.includes (p.id)) {
      const b = bounds (p);
      el ("rect", { x: b.x - 1.5, y: b.y - 1.5, width: b.w + 3, height: b.h + 3, rx: 1.5, fill: "none", stroke: p.lock ? "#f59bd6" : "#7fe3e0", "stroke-width": 0.6, "stroke-dasharray": "2 1.2", class: "d-sel" }, face);
    }
    mode = "edit";
    if (root === svg) notify();
  }

  function screw (parent, x, y, style, r) {
    el ("circle", { cx: x, cy: y, r, fill: "url(#knobAlu)", filter: "url(#drop)" }, parent);
    if (style === "phillips") { el ("line", { x1: x - r * 0.55, y1: y, x2: x + r * 0.55, y2: y, stroke: "#333", "stroke-width": 0.55 }, parent); el ("line", { x1: x, y1: y - r * 0.55, x2: x, y2: y + r * 0.55, stroke: "#333", "stroke-width": 0.55 }, parent); }
    else if (style === "hex") { const pts = []; for (let i = 0; i < 6; ++i) { const a = Math.PI / 3 * i; pts.push ((x + Math.cos (a) * r * 0.45).toFixed (2) + "," + (y + Math.sin (a) * r * 0.45).toFixed (2)); } el ("polygon", { points: pts.join (" "), fill: "#2a2a2c" }, parent); }
    else { el ("circle", { cx: x, cy: y, r: r * 1.25, fill: "none", stroke: "#8f9095", "stroke-width": 0.5, "stroke-dasharray": "0.5 0.6" }, parent); }
  }

  const bounds = (p) => ({ x: p.x - p.w / 2, y: p.y - p.h / 2, w: p.w, h: p.h });
  const angle = (v) => -135 + 2.7 * v;   // 0..100 -> -135..+135 degrees

  function drawPart (parent, p) {
    const g = el ("g", { "data-id": p.id, class: "d-part", transform: `translate(${p.x} ${p.y})${p.rot ? ` rotate(${p.rot})` : ""}` }, parent);
    const ink = design.unit.ink, r = Math.min (p.w, p.h) / 2;
    switch (p.type) {
      case "knob": {
        if (p.scale) {   // eleven ticks and the ends numbered, round the knob
          for (let i = 0; i <= 10; ++i) {
            const a = (angle (i * 10) - 90) * Math.PI / 180, r0 = r * 1.18, r1 = r * (i % 5 === 0 ? 1.38 : 1.3);
            el ("line", { x1: Math.cos (a) * r0, y1: Math.sin (a) * r0, x2: Math.cos (a) * r1, y2: Math.sin (a) * r1, stroke: ink, "stroke-width": i % 5 === 0 ? 0.5 : 0.3 }, g);
          }
          for (const [v, n] of [[0, p.min], [50, (p.min + p.max) / 2], [100, p.max]]) {
            const a = (angle (v) - 90) * Math.PI / 180;
            txt (g, Math.cos (a) * r * 1.62, Math.sin (a) * r * 1.62, String (Math.round (n * 10) / 10), r * 0.24, ink, { bold: false, spacing: 0 });
          }
        }
        if (p.text) txt (g, 0, r * 1.62 + 3, p.text, 2.6, ink, { spacing: 0.45 });
        if (mode !== "print") knob (g, p.style, r, angle (p.value), p.pointer);
        break;
      }
      case "toggle": {
        if (mode === "print") { if (p.text) txt (g, 0, 12, p.text, 2.5, ink, { spacing: 0.4 }); if (p.style !== "bat") el ("rect", { x: -4, y: -7, width: 8, height: 14, rx: 1.2, fill: "#050506" }, g); break; }
        if (p.style === "bat") {
          el ("circle", { r: 3.6, fill: "url(#knobAlu)", filter: "url(#drop)" }, g);
          el ("circle", { r: 2.2, fill: "#2b2c30" }, g);
          const dir = p.on ? -1 : 1;
          el ("path", { d: `M -1.1 0 L -1.6 ${dir * 8} A 1.6 1.6 0 0 0 1.6 ${dir * 8} L 1.1 0 Z`, fill: "url(#metalV)", filter: "url(#drop)" }, g);
        } else {
          const c = p.style === "rockerred" ? "#b3231c" : "#18191c";
          el ("rect", { x: -4, y: -7, width: 8, height: 14, rx: 1.2, fill: "#050506" }, g);
          el ("rect", { x: -3.2, y: -6.2, width: 6.4, height: 12.4, rx: 1, fill: c, filter: "url(#drop)" }, g);
          el ("rect", { x: -3.2, y: p.on ? -6.2 : 0, width: 6.4, height: 6.2, rx: 1, fill: "#fff", opacity: 0.12 }, g);
          txt (g, 0, -3, "I", 2.4, "#eee", {}); txt (g, 0, 3, "O", 2.4, "#eee", {});
        }
        if (p.text) txt (g, 0, 12, p.text, 2.5, ink, { spacing: 0.4 });
        break;
      }
      case "button": {
        if (mode === "print") { if (p.text) txt (g, 0, p.h / 2 + 4, p.text, 2.5, ink, { spacing: 0.4 }); break; }
        const lit = p.on ? p.colour : shade (p.colour, -0.72);
        if (p.style === "round") { el ("circle", { r: r + 0.8, fill: "#08080a" }, g); el ("circle", { r, fill: lit, filter: "url(#drop)" }, g); el ("circle", { r, fill: "url(#sheen)" }, g); }
        else { el ("rect", { x: -p.w / 2 - 0.8, y: -p.h / 2 - 0.8, width: p.w + 1.6, height: p.h + 1.6, rx: 1.4, fill: "#08080a" }, g);
               el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill: lit, filter: "url(#drop)" }, g);
               el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill: "url(#sheen)" }, g); }
        if (p.on) el ("circle", { r: Math.max (p.w, p.h) * 0.9, fill: p.colour, opacity: 0.12 }, g);
        if (p.text) txt (g, 0, p.h / 2 + 4, p.text, 2.5, ink, { spacing: 0.4 });
        break;
      }
      case "led": {
        if (mode === "print") { if (p.text) txt (g, 0, r + 4, p.text, 2.2, ink, { spacing: 0.3 }); break; }
        if (p.on) el ("circle", { r: r * 3.2, fill: p.colour, opacity: 0.18 }, g);
        el ("circle", { r: r + 0.5, fill: "#1a1a1c" }, g);
        el ("circle", { r, fill: p.on ? p.colour : shade (p.colour, -0.75) }, g);
        el ("circle", { cx: -r * 0.3, cy: -r * 0.35, r: r * 0.35, fill: "#fff", opacity: p.on ? 0.7 : 0.25 }, g);
        if (p.text) txt (g, 0, r + 4, p.text, 2.2, ink, { spacing: 0.3 });
        break;
      }
      case "vu": {
        const w = p.w, h = p.h;
        el ("rect", { x: -w / 2 - 1, y: -h / 2 - 1, width: w + 2, height: h + 2, rx: 1.5, fill: "#050506" }, g);
        el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h, rx: 1, fill: `url(#vu${p.style[0].toUpperCase()}${p.style.slice (1)})` }, g);
        const inkVu = p.style === "black" ? "#e8e8ea" : "#1d1c1a", cx = 0, cy = h * 0.55, R = h * 0.78;
        for (let i = 0; i <= 10; ++i) { const a = (-50 + i * 10) * Math.PI / 180, r0 = R * 0.86, r1 = R * (i % 5 === 0 ? 0.98 : 0.93);
          el ("line", { x1: cx + Math.sin (a) * r0, y1: cy - Math.cos (a) * r0, x2: cx + Math.sin (a) * r1, y2: cy - Math.cos (a) * r1, stroke: i >= 8 ? "#b3231c" : inkVu, "stroke-width": 0.45 }, g); }
        el ("path", { d: `M ${cx + Math.sin (0.52) * R * 0.98} ${cy - Math.cos (0.52) * R * 0.98} A ${R * 0.98} ${R * 0.98} 0 0 1 ${cx + Math.sin (0.87) * R * 0.98} ${cy - Math.cos (0.87) * R * 0.98}`, stroke: "#b3231c", "stroke-width": 1.2, fill: "none" }, g);
        txt (g, 0, h * 0.30, p.text, h * 0.10, inkVu, { spacing: 0.5 });
        const na = (-50 + p.value) * Math.PI / 180;
        el ("line", { x1: cx, y1: cy, x2: cx + Math.sin (na) * R * 0.97, y2: cy - Math.cos (na) * R * 0.97, stroke: inkVu, "stroke-width": 0.45 }, g);
        el ("circle", { cx, cy, r: 1.6, fill: "#111" }, g);
        el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h * 0.45, rx: 1, fill: "#fff", opacity: 0.07 }, g);   // the glass
        break;
      }
      case "ladder": {
        const n = p.segments, sh = p.h / n;
        for (let i = 0; i < n; ++i) { const lit = i < Math.round (n * p.value / 100), c = i >= n - 1 ? "#ff4a3a" : i >= n - 3 ? "#ffcc33" : "#46e070";
          const y = p.h / 2 - (i + 0.5) * sh;
          if (lit) el ("rect", { x: -p.w, y: y - sh * 0.7, width: p.w * 2, height: sh * 1.4, fill: c, opacity: 0.15 }, g);
          el ("rect", { x: -p.w / 2, y: y - sh * 0.36, width: p.w, height: sh * 0.72, rx: 0.4, fill: lit ? c : shade (c, -0.78) }, g); }
        break;
      }
      case "display": {
        el ("rect", { x: -p.w / 2 - 1.2, y: -p.h / 2 - 1.2, width: p.w + 2.4, height: p.h + 2.4, rx: 2, fill: "#050506" }, g);
        el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1.2, fill: shade (p.colour, -0.88) }, g);
        let d = ""; for (let i = 0; i <= 60; ++i) { const x = -p.w / 2 + 3 + (p.w - 6) * i / 60, y = Math.sin (i * 0.45) * Math.cos (i * 0.11) * p.h * 0.25; d += (i ? " L " : "M ") + x.toFixed (2) + " " + y.toFixed (2); }
        el ("path", { d, fill: "none", stroke: p.colour, "stroke-width": 0.6, opacity: 0.9 }, g);
        if (p.text) txt (g, -p.w / 2 + 3, -p.h / 2 + 3.5, p.text, 2.4, p.colour, { anchor: "start", spacing: 0.3 });
        el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h * 0.4, rx: 1.2, fill: "#fff", opacity: 0.05 }, g);
        break;
      }
      case "label": txt (g, p.align === "left" ? -p.w / 2 : p.align === "right" ? p.w / 2 : 0, 0, p.text, p.size, ink,
                               { bold: p.bold, spacing: p.size * 0.14, anchor: p.align === "left" ? "start" : p.align === "right" ? "end" : "middle" }); break;
      case "box": {
        if (p.fill) el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: p.round, fill: shade (design.unit.colour, 0.1) }, g);
        el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: p.round, fill: "none", stroke: ink, "stroke-width": 0.35, opacity: 0.85 }, g);
        if (p.text) { const tw = p.text.length * 2.1 + 3; el ("rect", { x: -tw / 2, y: -p.h / 2 - 1.5, width: tw, height: 3, fill: design.unit.colour }, g); txt (g, 0, -p.h / 2, p.text, 2.5, ink, { spacing: 0.5 }); }
        break;
      }
      case "line": el ("rect", { x: -p.w / 2, y: -Math.max (0.3, p.h) / 2, width: p.w, height: Math.max (0.3, p.h), fill: ink, opacity: 0.85 }, g); break;
      case "jack": {
        if (mode === "print") { if (p.text) txt (g, 0, r + 4, p.text, 2.4, ink, { spacing: 0.4 }); break; }
        el ("circle", { r: r + 0.6, fill: "url(#knobAlu)", filter: "url(#drop)" }, g);
        el ("circle", { r: r * 0.78, fill: "#0b0b0d" }, g);
        if (p.style === "xlr") { for (const [x, y] of [[-r * 0.3, -r * 0.15], [r * 0.3, -r * 0.15], [0, r * 0.3]]) el ("circle", { cx: x, cy: y, r: r * 0.1, fill: "#c9a64a" }, g); el ("rect", { x: -r * 0.12, y: -r * 0.85, width: r * 0.24, height: r * 0.3, fill: "#c9cacf" }, g); }
        else el ("circle", { r: r * 0.3, fill: "#2a2a2c", stroke: "#c9a64a", "stroke-width": 0.4 }, g);
        if (p.text) txt (g, 0, r + 4, p.text, 2.4, ink, { spacing: 0.4 });
        break;
      }
      case "screw": if (mode !== "print") screw (g, 0, 0, design.unit.screws, r); break;
      case "vent": {
        const n = p.count, sw = p.w / n;
        for (let i = 0; i < n; ++i) el ("rect", { x: -p.w / 2 + i * sw + sw * 0.25, y: -p.h / 2, width: sw * 0.5, height: p.h, rx: sw * 0.25, fill: "#050506" }, g);
        break;
      }
    }
    // an invisible hit area the size of the part, so small parts are easy to grab
    el ("rect", { x: -p.w / 2 - 2, y: -p.h / 2 - 2, width: p.w + 4, height: p.h + 4, fill: "#000", opacity: 0 }, g);
  }

  function knob (g, style, r, deg, pointer = "auto") {
    const k = el ("g", { transform: `rotate(${deg})` }, el ("g", { filter: "url(#drop)" }, g));
    const line = (colour, from, to, w = 0.9) => lineRaw (POINTER_COLOUR[pointer] || colour, from, to, w);
    const lineRaw = (colour, from, to, w = 0.9) => el ("line", { x1: 0, y1: -r * from, x2: 0, y2: -r * to, stroke: colour, "stroke-width": w, "stroke-linecap": "round" }, k);
    const ribs = (n, rr, colour, op) => { for (let i = 0; i < n; ++i) { const a = i / n * Math.PI * 2; el ("line", { x1: Math.cos (a) * rr * 0.86, y1: Math.sin (a) * rr * 0.86, x2: Math.cos (a) * rr, y2: Math.sin (a) * rr, stroke: colour, "stroke-width": rr * 0.07, opacity: op }, k); } };
    switch (style) {
      case "tophat": el ("circle", { r: r * 1.15, fill: "url(#knobBlack)" }, k); el ("circle", { r: r * 0.72, fill: "url(#knobBlack)", stroke: "#333", "stroke-width": 0.3 }, k); ribs (11, r * 0.72, "#000", 0.6); line ("#e9dfc6", 0.1, 1.12); break;
      case "knurled": el ("circle", { r, fill: "url(#knobAlu)" }, k); ribs (60, r, "#6a6b70", 0.8); el ("circle", { r: r * 0.8, fill: "url(#knobAlu)" }, k); line ("#111", 0.25, 0.95, 0.8); break;
      case "fluted": el ("circle", { r: r * 1.1, fill: "url(#knobBlack)" }, k); el ("circle", { r: r * 0.68, fill: "url(#knobBlack)" }, k); ribs (12, r * 0.68, "#000", 0.7); line ("#f2f2f2", 0.1, 1.05); break;
      case "ribbed": el ("circle", { r, fill: "url(#knobMatte)" }, k); ribs (28, r, "#000", 0.7); line ("#f2f2f2", 0.2, 0.98); break;
      case "matte": el ("circle", { r, fill: "url(#knobMatte)" }, k); el ("circle", { r: r * 0.93, fill: "none", stroke: "#2c2c30", "stroke-width": 0.4 }, k); line ("#f2f2f2", 0.25, 0.98); break;
      case "redtrim": el ("circle", { r, fill: "url(#knobRed)" }, k); ribs (40, r, "#3a0808", 0.5); line ("#f2f2f2", 0.2, 0.98); break;
      case "capblue": case "capred": case "capwhite": {
        el ("circle", { r, fill: "#3a3b3f" }, k); ribs (40, r, "#1c1c1e", 0.8);
        el ("circle", { r: r * 0.8, fill: `url(#cap${style.slice (3, 4).toUpperCase()}${style.slice (4)})` }, k);
        line (style === "capwhite" ? "#222" : "#f2f2f2", 0.2, 0.78); break; }
      case "chicken": el ("circle", { r: r * 0.65, fill: "url(#knobBlack)" }, k); el ("path", { d: `M ${-r * 0.3} ${r * 0.5} L ${-r * 0.16} ${-r * 1.15} L ${r * 0.16} ${-r * 1.15} L ${r * 0.3} ${r * 0.5} Z`, fill: "url(#knobBlack)" }, k); line ("#efe5cc", 0.2, 1.08, 0.8); break;
      case "pointer": el ("circle", { r: r * 0.7, fill: "url(#knobBlack)" }, k); el ("rect", { x: -r * 0.2, y: -r * 1.2, width: r * 0.4, height: r * 1.9, rx: r * 0.2, fill: "url(#knobBlack)" }, k); line ("#f2f2f2", 0.2, 1.12, 0.7); break;
    }
    el ("circle", { r: r * 0.95, fill: "url(#sheen)", opacity: 0.45 }, k);
  }

  // ---------------------------------------------------------------------------------------------------
  // Listeners (the 3D view) and a small API for it
  const listeners = [];
  let notifyQueued = false;
  function notify () {
    if (notifyQueued) return; notifyQueued = true;
    requestAnimationFrame (() => { notifyQueued = false; for (const f of listeners) try { f (design, selected); } catch (_) { /* a listener's own trouble */ } });
  }
  /** The flat layer (paint, print, meter faces) as a canvas, for the 3D faceplate. `scale`: pixels per mm. */
  function printCanvas (scale) {
    return new Promise ((resolve) => {
      const tmp = document.createElementNS (NS, "svg");
      tmp.setAttribute ("xmlns", NS);
      render (tmp, "print");
      const url = URL.createObjectURL (new Blob ([new XMLSerializer().serializeToString (tmp)], { type: "image/svg+xml" }));
      const img = new Image();
      img.onload = () => { const c = document.createElement ("canvas"); c.width = Math.round (W * scale); c.height = Math.round (design.unit.height * U * scale);
        c.getContext ("2d").drawImage (img, 0, 0, c.width, c.height); URL.revokeObjectURL (url); resolve (c); };
      img.onerror = () => { URL.revokeObjectURL (url); resolve (null); };
      img.src = url;
    });
  }
  window.ENHDesigner = Object.freeze ({
    W, U, KNOBS, get: () => design, selected: () => selected.slice(),
    subscribe: (f) => { listeners.push (f); f (design, selected); },
    select: (id) => { if (byId (id)) { selected = [id]; render(); props(); } },
    printCanvas,
  });

  // ---------------------------------------------------------------------------------------------------
  // Pickers: dropdowns that show each choice as a small picture next to its name
  function thumb (kind, value) {
    const t = document.createElementNS (NS, "svg"); t.setAttribute ("viewBox", "-14 -14 28 28"); t.setAttribute ("width", 34); t.setAttribute ("height", 34);
    t.setAttribute ("aria-hidden", "true"); defs (t);
    const g = el ("g", {}, t), was = design;
    if (kind === "knob") knob (g, value, 9, 30);
    else if (kind === "pointer") { knob (g, "matte", 9, 30, value === "auto" ? "white" : value); }
    else if (kind === "toggle") drawPart (g, sanitize ({ parts: [{ type: "toggle", x: 0, y: 0, style: value, on: true, text: "" }] }).parts[0]);
    else if (kind === "button") drawPart (g, sanitize ({ parts: [{ type: "button", x: 0, y: 0, style: value, on: true, text: "", w: 12, h: 9, colour: "#e0a84a" }] }).parts[0]);
    else if (kind === "vu") { const q = sanitize ({ parts: [{ type: "vu", x: 0, y: 0, w: 26, h: 16, style: value, value: 55, text: "" }] }).parts[0]; drawPart (g, q); }
    else if (kind === "jack") drawPart (g, sanitize ({ parts: [{ type: "jack", x: 0, y: 0, w: 16, h: 16, style: value, text: "" }] }).parts[0]);
    else if (kind === "screws") screw (g, 0, 0, value, 8);
    else if (kind === "finish" || kind === "edge" || kind === "ears" || kind === "handles" || kind === "earColour" || kind === "font") {
      const u = design.unit, fin = kind === "finish" ? value : u.finish;
      const rx = kind === "edge" ? (value === "square" ? 0.3 : value === "bevel" ? 2 : 5) : 3;
      el ("rect", { x: -13, y: -10, width: 26, height: 20, rx, fill: u.colour }, g);
      if (fin === "brushed") el ("rect", { x: -13, y: -10, width: 26, height: 20, rx, fill: "#fff", filter: "url(#brushed)", opacity: 0.5 }, g);
      if (fin === "hammertone") el ("rect", { x: -13, y: -10, width: 26, height: 20, rx, fill: u.colour, filter: "url(#hammer)", opacity: 0.6 }, g);
      el ("rect", { x: -13, y: -10, width: 26, height: 20, rx, fill: "url(#sheen)" }, g);
      if (kind === "edge" && value === "bevel") el ("rect", { x: -11, y: -8, width: 22, height: 16, rx: 1, fill: "none", stroke: shade (u.colour, 0.5), "stroke-width": 0.8 }, g);
      if (kind === "ears") { if (value !== "none") for (const x of [-10, 10]) value === "slots" ? el ("rect", { x: x - 2.2, y: -1.2, width: 4.4, height: 2.4, rx: 1.2, fill: "#050506" }, g) : el ("circle", { cx: x, cy: 0, r: 1.6, fill: "#050506" }, g); }
      if (kind === "earColour" && value !== "match") for (const x of [-13, 7]) el ("rect", { x, y: -10, width: 6, height: 20, fill: value === "black" ? "#0d0d0f" : "#c3c4c8" }, g);
      if (kind === "handles" && value !== "none") for (const x of [-7, 7]) value === "bar" ? el ("rect", { x: x - 1.5, y: -8, width: 3, height: 16, rx: 1.5, fill: "url(#metalV)" }, g)
                                                                                           : el ("path", { d: `M ${x} -8 C ${x + 5} -8 ${x + 5} 8 ${x} 8`, fill: "none", stroke: "#c9cacf", "stroke-width": 2 }, g);
      if (kind === "font") { design = Object.assign ({}, design, { unit: Object.assign ({}, u, { font: value }) }); txt (g, 0, 0, "Ab", 10, u.ink, {}); design = was; }
    }
    return t;
  }

  /** A dropdown of pictures: a button showing the choice, a list of every choice with its picture. */
  function picker (kind, choices, names, current, onChange, label) {
    const wrap = document.createElement ("div"); wrap.className = "d-pick";
    const btn = document.createElement ("button"); btn.type = "button"; btn.className = "d-pick-btn"; btn.setAttribute ("aria-haspopup", "listbox"); btn.setAttribute ("aria-expanded", "false");
    if (label) btn.setAttribute ("aria-label", label);
    const list = document.createElement ("ul"); list.className = "d-pick-list"; list.setAttribute ("role", "listbox"); list.hidden = true;
    const nameOf = (v) => (names && names[v]) || v[0].toUpperCase() + v.slice (1);
    const show = (v) => { btn.replaceChildren (thumb (kind, v)); const n = document.createElement ("span"); n.textContent = nameOf (v); btn.appendChild (n); };
    let value = current; show (value);
    const items = choices.map ((v) => {
      const li = document.createElement ("li"); li.setAttribute ("role", "option"); li.tabIndex = -1; li.dataset.v = v;
      li.appendChild (thumb (kind, v)); const n = document.createElement ("span"); n.textContent = nameOf (v); li.appendChild (n);
      li.addEventListener ("click", () => choose (v)); list.appendChild (li); return li;
    });
    const open = (o) => { list.hidden = !o; btn.setAttribute ("aria-expanded", String (o));
      items.forEach ((li) => li.setAttribute ("aria-selected", String (li.dataset.v === value)));
      if (o) (items.find ((li) => li.dataset.v === value) || items[0]).focus(); };
    const choose = (v) => { value = v; show (v); open (false); btn.focus(); onChange (v); };
    btn.addEventListener ("click", () => open (list.hidden));
    list.addEventListener ("keydown", (e) => {
      const i = items.indexOf (document.activeElement);
      if (e.key === "ArrowDown") { e.preventDefault(); items[Math.min (items.length - 1, i + 1)].focus(); }
      else if (e.key === "ArrowUp") { e.preventDefault(); items[Math.max (0, i - 1)].focus(); }
      else if (e.key === "Enter" || e.key === " ") { e.preventDefault(); if (i >= 0) choose (items[i].dataset.v); }
      else if (e.key === "Escape") { e.preventDefault(); open (false); btn.focus(); }
    });
    document.addEventListener ("pointerdown", (e) => { if (!wrap.contains (e.target)) open (false); });
    wrap.append (btn, list);
    wrap.setValue = (v) => { value = v; show (v); };
    return wrap;
  }

  // ---------------------------------------------------------------------------------------------------
  // Editing
  const byId = (id) => design.parts.find ((p) => p.id === id);
  // Undo keeps the state from before each change: `last` is the design as it stood after the previous one
  let last = null;
  function commit () {
    const now = JSON.stringify (design);
    if (last !== null && last !== now) { history.push (last); if (history.length > 200) history.shift(); future = []; }
    last = now; save(); render(); props();
  }
  function replaceDesign (d) {   // a template, an imported code, a new blank unit: one undoable step
    if (last !== null) { history.push (last); future = []; }
    design = d; selected = []; last = JSON.stringify (design); save(); render(); props(); syncUnit();
  }
  function snapV (v) { return snap ? Math.round (v * 2) / 2 : v; }

  function addPart (type) {
    const t = TYPES[type], H = design.unit.height * U;
    const p = Object.assign ({ id: nextId++, type, x: W / 2, y: H / 2, w: t.w, h: t.h, rot: 0 }, JSON.parse (JSON.stringify (t.defaults)));
    if (type === "knob") { p.w = p.h = p.size; delete p.size; }
    // a little offset so repeated adds do not stack exactly
    const n = design.parts.filter ((q) => q.type === type).length;
    p.x = Math.min (W - 30, p.x + (n % 6) * 12); p.y = Math.min (H - 6, p.y + (n % 3) * 4);
    design.parts.push (p); selected = [p.id]; commit();
  }

  function removeSelected () { if (!selected.length) return; design.parts = design.parts.filter ((p) => !selected.includes (p.id)); selected = []; commit(); }
  function duplicate () {
    if (!selected.length) return;
    const copies = selected.map (byId).filter (Boolean).map ((p) => Object.assign ({}, p, { id: nextId++, x: Math.min (W, p.x + 8), y: Math.min (design.unit.height * U, p.y + 4) }));
    design.parts.push (...copies); selected = copies.map ((p) => p.id); commit();
  }
  function align (how) {
    const ps = selected.map (byId).filter (Boolean); if (ps.length < 2) return;
    const xs = ps.map ((p) => p.x), ys = ps.map ((p) => p.y);
    const lo = (a) => Math.min (...a), hi = (a) => Math.max (...a);
    if (how === "left") ps.forEach ((p) => (p.x = lo (ps.map ((q) => q.x - q.w / 2)) + p.w / 2));
    if (how === "right") ps.forEach ((p) => (p.x = hi (ps.map ((q) => q.x + q.w / 2)) - p.w / 2));
    if (how === "hcenter") { const c = (lo (xs) + hi (xs)) / 2; ps.forEach ((p) => (p.x = c)); }
    if (how === "top") ps.forEach ((p) => (p.y = lo (ps.map ((q) => q.y - q.h / 2)) + p.h / 2));
    if (how === "bottom") ps.forEach ((p) => (p.y = hi (ps.map ((q) => q.y + q.h / 2)) - p.h / 2));
    if (how === "vcenter") { const c = (lo (ys) + hi (ys)) / 2; ps.forEach ((p) => (p.y = c)); }
    if (how === "hdist" && ps.length > 2) { ps.sort ((a, b) => a.x - b.x); const a = ps[0].x, b = ps[ps.length - 1].x; ps.forEach ((p, i) => (p.x = a + (b - a) * i / (ps.length - 1))); }
    if (how === "vdist" && ps.length > 2) { ps.sort ((a, b) => a.y - b.y); const a = ps[0].y, b = ps[ps.length - 1].y; ps.forEach ((p, i) => (p.y = a + (b - a) * i / (ps.length - 1))); }
    commit();
  }
  function order (dir) {
    const ids = new Set (selected); const moving = design.parts.filter ((p) => ids.has (p.id)), rest = design.parts.filter ((p) => !ids.has (p.id));
    design.parts = dir > 0 ? rest.concat (moving) : moving.concat (rest); commit();
  }

  // Pointer: select, drag, and in Play mode turn knobs / flip switches
  let drag = null;
  function svgPoint (e) { const pt = svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY; return pt.matrixTransform (svg.getScreenCTM().inverse()); }
  svg.addEventListener ("pointerdown", (e) => {
    const g = e.target.closest (".d-part"); const pt = svgPoint (e);
    if (!g) { if (!play) { selected = []; render(); props(); } return; }
    const p = byId (Number (g.getAttribute ("data-id"))); if (!p) return;
    e.preventDefault(); try { svg.setPointerCapture (e.pointerId); } catch (_) { /* a synthetic pointer: no capture needed */ }
    if (play) {
      if (p.type === "toggle" || p.type === "button" || p.type === "led") { p.on = !p.on; render(); save(); return; }
      if ("value" in p) drag = { mode: "turn", p, y0: e.clientY, v0: p.value };
      return;
    }
    if (e.shiftKey) selected = selected.includes (p.id) ? selected.filter ((i) => i !== p.id) : selected.concat (p.id);
    else if (!selected.includes (p.id)) selected = [p.id];
    drag = { mode: "move", start: pt, moved: false, orig: selected.map (byId).filter ((q) => q && !q.lock).map ((q) => ({ q, x: q.x, y: q.y })) };
    render(); props();
  });
  svg.addEventListener ("pointermove", (e) => {
    if (!drag) return;
    if (drag.mode === "turn") { drag.p.value = clamp (drag.v0 + (drag.y0 - e.clientY) * 0.6, 0, 100, 0); render(); return; }
    const pt = svgPoint (e), dx = pt.x - drag.start.x, dy = pt.y - drag.start.y, H = design.unit.height * U;
    if (Math.abs (dx) + Math.abs (dy) > 0.2) drag.moved = true;
    for (const o of drag.orig) { o.q.x = clamp (snapV (o.x + dx), 0, W, o.x); o.q.y = clamp (snapV (o.y + dy), 0, H, o.y); }
    render();
  });
  const endDrag = () => { if (drag) { if (drag.mode === "turn" || drag.moved) commit(); drag = null; } };
  svg.addEventListener ("pointerup", endDrag); svg.addEventListener ("pointercancel", endDrag);
  svg.addEventListener ("wheel", (e) => {
    if (!play) return; const g = e.target.closest (".d-part"); const p = g && byId (Number (g.getAttribute ("data-id")));
    if (p && "value" in p) { e.preventDefault(); p.value = clamp (p.value - Math.sign (e.deltaY) * 3, 0, 100, 0); render(); save(); }
  }, { passive: false });

  document.addEventListener ("keydown", (e) => {
    if (e.target.closest ("input, textarea, select, dialog")) return;
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key.toLowerCase() === "z") { e.preventDefault(); e.shiftKey ? redo() : undo(); return; }
    if (mod && e.key.toLowerCase() === "y") { e.preventDefault(); redo(); return; }
    if (mod && e.key.toLowerCase() === "d") { e.preventDefault(); duplicate(); return; }
    if (e.key === "Delete" || e.key === "Backspace") { if (selected.length) { e.preventDefault(); removeSelected(); } return; }
    if (e.key.toLowerCase() === "p" && !mod) { setPlay (!play); return; }
    const step = e.shiftKey ? 5 : 0.5, H = design.unit.height * U;
    const mv = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] }[e.key];
    if (mv && selected.length) { e.preventDefault(); for (const id of selected) { const p = byId (id); if (p.lock) continue; p.x = clamp (p.x + mv[0], 0, W, p.x); p.y = clamp (p.y + mv[1], 0, H, p.y); } commit(); }
  });

  function undo () { if (!history.length) return; future.push (JSON.stringify (design)); design = JSON.parse (history.pop()); last = JSON.stringify (design); selected = []; save(); render(); props(); syncUnit(); }
  function redo () { if (!future.length) return; history.push (JSON.stringify (design)); design = JSON.parse (future.pop()); last = JSON.stringify (design); selected = []; save(); render(); props(); syncUnit(); }
  function setPlay (on) { play = on; const b = document.getElementById ("play"); b.setAttribute ("aria-pressed", String (on)); b.textContent = on ? "Edit" : "Play"; document.body.classList.toggle ("playing", on); render(); }

  // ---------------------------------------------------------------------------------------------------
  // Panels
  const $ = (id) => document.getElementById (id);
  const unitPickers = {};
  function syncUnit () {
    const u = design.unit;
    $("u-name").value = u.name; $("u-model").value = u.model; $("u-height").value = String (u.height);
    $("u-colour").value = u.colour; $("u-ink").value = u.ink; $("u-wear").value = String (u.wear); $("u-grid").checked = snap;
    $("u-sub").value = u.sub; $("u-badge").value = u.badge;
    for (const k in unitPickers) unitPickers[k].setValue (u[k]);
    rebuildUnitPickers();   // (their pictures show this panel's colour)
  }
  function bindUnit () {
    const on = (id, f, ev = "input") => $(id).addEventListener (ev, () => { f ($(id)); render(); save(); });
    const done = (id) => $(id).addEventListener ("change", () => commit());
    on ("u-name", (e) => (design.unit.name = text (e.value, 40, ""))); done ("u-name");
    on ("u-model", (e) => (design.unit.model = text (e.value, 24, ""))); done ("u-model");
    on ("u-height", (e) => { design.unit.height = Number (e.value); const H = design.unit.height * U; design.parts.forEach ((p) => (p.y = Math.min (p.y, H))); }, "change"); done ("u-height");
    on ("u-sub", (e) => (design.unit.sub = text (e.value, 40, ""))); done ("u-sub");
    on ("u-badge", (e) => (design.unit.badge = text (e.value, 16, ""))); done ("u-badge");
    const unitPick = (key, list, names) => {
      const w = picker (key, list, names, design.unit[key], (v) => { design.unit[key] = pick (v, list, design.unit[key]); commit(); refreshPickers(); }, key);
      $("u-" + key).replaceChildren (w); unitPickers[key] = w;
    };
    unitPick ("finish", FINISHES, { anodised: "Anodised", brushed: "Brushed aluminium", paint: "Paint", hammertone: "Hammertone" });
    unitPick ("edge", EDGES, { square: "Square", rounded: "Rounded", bevel: "Bevelled" });
    unitPick ("font", FONTS, { sans: "Sans", serif: "Serif", mono: "Mono", condensed: "Condensed" });
    unitPick ("ears", EARS, { slots: "Slotted", holes: "Round holes", none: "None (500 module)" });
    unitPick ("earColour", EARCOLS, { match: "As the panel", black: "Black", silver: "Silver" });
    unitPick ("handles", HANDLES, { none: "None", bar: "Bar handles", loop: "Loop handles" });
    unitPick ("screws", SCREWS, { phillips: "Phillips", hex: "Hex", thumb: "Thumb screws" });
    rebuildUnitPickers = () => { unitPick ("finish", FINISHES, { anodised: "Anodised", brushed: "Brushed aluminium", paint: "Paint", hammertone: "Hammertone" });
      unitPick ("edge", EDGES, { square: "Square", rounded: "Rounded", bevel: "Bevelled" }); unitPick ("earColour", EARCOLS, { match: "As the panel", black: "Black", silver: "Silver" }); };
    on ("u-colour", (e) => (design.unit.colour = colour (e.value, design.unit.colour))); done ("u-colour");
    $("u-colour").addEventListener ("change", rebuildUnitPickers);
    on ("u-ink", (e) => (design.unit.ink = colour (e.value, design.unit.ink))); done ("u-ink");
    on ("u-wear", (e) => (design.unit.wear = Number (e.value))); done ("u-wear");
    $("u-grid").addEventListener ("change", (e) => (snap = e.target.checked));
  }

  // The pictures follow the panel's colour and finish: redraw them when those change
  let rebuildUnitPickers = () => {};
  function refreshPickers () { for (const k in unitPickers) unitPickers[k].setValue (design.unit[k]); }

  function field (parent, label, input) { const l = document.createElement ("label"); l.textContent = label + " "; l.appendChild (input); parent.appendChild (l); return input; }
  function props () {
    const body = $("props-body"); body.replaceChildren();
    $("props-empty").hidden = selected.length > 0;
    $("align").hidden = selected.length < 2;
    if (selected.length !== 1) { if (selected.length > 1) { const p = document.createElement ("p"); p.className = "muted small"; p.textContent = selected.length + " parts selected"; body.appendChild (p); } return; }
    const p = byId (selected[0]); if (!p) return;
    const h = document.createElement ("p"); h.className = "d-kind"; h.textContent = TYPES[p.type].label; body.appendChild (h);
    const num = (key, label, lo, hi, step = 0.5) => { const i = document.createElement ("input"); i.type = "number"; i.min = lo; i.max = hi; i.step = step; i.value = Math.round (p[key] * 10) / 10;
      i.addEventListener ("input", () => { p[key] = clamp (i.value, lo, hi, p[key]); if (p.type === "knob" && key === "w") p.h = p.w; render(); save(); }); i.addEventListener ("change", commit); field (body, label, i); };
    const choice = (key, label, list, names) => { const s = document.createElement ("select"); for (const v of list) { const o = document.createElement ("option"); o.value = v; o.textContent = names ? names[v] : v[0].toUpperCase() + v.slice (1); s.appendChild (o); }
      s.value = p[key]; s.addEventListener ("change", () => { p[key] = pick (s.value, list, p[key]); commit(); }); field (body, label, s); };
    const str = (key, label, max) => { const i = document.createElement ("input"); i.maxLength = max; i.value = p[key]; i.autocomplete = "off";
      i.addEventListener ("input", () => { p[key] = text (i.value, max, ""); render(); save(); }); i.addEventListener ("change", commit); field (body, label, i); };
    const chk = (key, label) => { const i = document.createElement ("input"); i.type = "checkbox"; i.checked = p[key]; i.addEventListener ("change", () => { p[key] = i.checked; commit(); });
      const l = document.createElement ("label"); l.className = "row"; l.appendChild (i); l.appendChild (document.createTextNode (" " + label)); body.appendChild (l); };
    const col = (key, label) => { const i = document.createElement ("input"); i.type = "color"; i.value = p[key]; i.addEventListener ("input", () => { p[key] = colour (i.value, p[key]); render(); save(); }); i.addEventListener ("change", commit);
      const l = field (body, label, i); l.parentElement.classList.add ("row"); };
    num ("x", "Across (mm)", 0, W); num ("y", "Down (mm)", 0, design.unit.height * U);
    if (p.type === "knob") num ("w", "Size (mm)", 8, 60);
    else if (!["led", "screw"].includes (p.type)) { num ("w", "Width (mm)", 1, W); if (!["label"].includes (p.type)) num ("h", "Height (mm)", 0.5, 4 * U); }
    else num ("w", "Size (mm)", 2, 14);
    if (["label", "box", "line", "display", "vent", "jack"].includes (p.type)) num ("rot", "Rotation (°)", -180, 180, 1);
    const pick2 = (key, label, kind, list, names) => { const l = document.createElement ("div"); l.className = "d-field"; const t = document.createElement ("span"); t.textContent = label; l.appendChild (t);
      l.appendChild (picker (kind, list, names, p[key], (v) => { p[key] = pick (v, list, p[key]); commit(); }, label)); body.appendChild (l); };
    if (p.type === "knob") { pick2 ("style", "Knob", "knob", KNOBS, KNOB_NAMES); pick2 ("pointer", "Pointer", "pointer", POINTERS, { auto: "As the knob comes" });
      chk ("scale", "Printed scale"); num ("min", "Scale from", -99, 999, 1); num ("max", "Scale to", -99, 999, 1); }
    if (p.type === "toggle") pick2 ("style", "Switch", "toggle", TOGGLES, { bat: "Bat handle", rocker: "Rocker", rockerred: "Red rocker" });
    if (p.type === "button") pick2 ("style", "Button", "button", BUTTONS);
    if (p.type === "vu") pick2 ("style", "Dial", "vu", VUS);
    if (p.type === "jack") pick2 ("style", "Socket", "jack", JACKS, { trs: "Jack (TRS)", xlr: "XLR" });
    if ("align" in p) choice ("align", "Align", ALIGNS);
    if ("fill" in p) chk ("fill", "Filled");
    if ("value" in p) num ("value", p.type === "vu" ? "Needle" : p.type === "ladder" ? "Lit (%)" : "Position (%)", 0, 100, 1);
    if ("on" in p) chk ("on", p.type === "toggle" ? "On" : "Lit");
    if ("colour" in p) col ("colour", "Colour");
    if ("segments" in p) num ("segments", "Segments", 3, 24, 1);
    if ("count" in p) num ("count", "Slots", 2, 30, 1);
    if ("size" in p) num ("size", "Text size", 2, 20, 0.5);
    if ("bold" in p) chk ("bold", "Bold");
    if ("round" in p) num ("round", "Corner", 0, 12, 0.5);
    if ("text" in p) str ("text", p.type === "label" ? "Text" : "Label", 40);
    chk ("lock", "Locked (can't be moved by accident)");
    const b = document.createElement ("div"); b.className = "d-btns";
    for (const [t, f] of [["Duplicate", duplicate], ["Bring forward", () => order (1)], ["Send back", () => order (-1)], ["Delete", removeSelected]]) {
      const x = document.createElement ("button"); x.type = "button"; x.textContent = t; x.addEventListener ("click", f); b.appendChild (x); }
    body.appendChild (b);
  }

  // ---------------------------------------------------------------------------------------------------
  // Templates
  const T = (unit, parts) => ({ v: 1, unit: Object.assign (blank().unit, unit), parts });
  const templates = {
    "Blank 1U": () => blank(),
    "FET compressor": () => T ({ name: "LIMITING AMPLIFIER", model: "EM-76", height: 2, colour: "#101012", ink: "#e9e9ea", finish: "anodised" }, [
      { type: "knob", x: 70, y: 45, w: 30, h: 30, style: "knurled", value: 40, text: "INPUT", scale: true, min: 0, max: 48 },
      { type: "knob", x: 130, y: 45, w: 30, h: 30, style: "knurled", value: 55, text: "OUTPUT", scale: true, min: 0, max: 24 },
      { type: "knob", x: 185, y: 36, w: 14, h: 14, style: "knurled", value: 30, text: "ATTACK", scale: false, min: 1, max: 7 },
      { type: "knob", x: 185, y: 64, w: 14, h: 14, style: "knurled", value: 70, text: "RELEASE", scale: false, min: 1, max: 7 },
      { type: "vu", x: 300, y: 42, w: 80, h: 44, style: "cream", value: 40, text: "GAIN REDUCTION" },
      { type: "button", x: 225, y: 30, w: 10, h: 8, style: "square", on: true, colour: "#e8e3d6", text: "4" },
      { type: "button", x: 225, y: 46, w: 10, h: 8, style: "square", on: false, colour: "#e8e3d6", text: "8" },
      { type: "button", x: 225, y: 62, w: 10, h: 8, style: "square", on: false, colour: "#e8e3d6", text: "20" },
      { type: "toggle", x: 400, y: 44, style: "rockerred", on: true, text: "POWER" } ]),
    "Program EQ": () => T ({ name: "PROGRAM EQUALIZER", model: "EM-1A", height: 3, colour: "#2b4f66", ink: "#f2f2f2", finish: "hammertone", wear: 25 }, [
      ...["BOOST", "ATTEN", "BOOST", "ATTEN"].map ((t, i) => ({ type: "knob", x: 120 + i * 60, y: 40, w: 28, h: 28, style: "tophat", value: 20 + i * 12, text: t, scale: true, min: 0, max: 10 })),
      { type: "knob", x: 150, y: 100, w: 26, h: 26, style: "chicken", value: 60, text: "LOW FREQUENCY", scale: false, min: 0, max: 10 },
      { type: "knob", x: 270, y: 100, w: 26, h: 26, style: "tophat", value: 50, text: "BANDWIDTH", scale: true, min: 0, max: 10 },
      { type: "led", x: 420, y: 40, w: 7, h: 7, colour: "#ff3b30", on: true, text: "" },
      { type: "toggle", x: 420, y: 100, style: "bat", on: true, text: "EQ IN" } ]),
    "500-series module": () => T ({ name: "DE-HARSH", model: "EM-11B", height: 4, colour: "#2d4a6a", ink: "#eef2f6", finish: "paint", ears: "none", screws: "phillips" }, [
      { type: "line", x: 241, y: 20, w: 460, h: 0.5 },
      { type: "knob", x: 120, y: 70, w: 22, h: 22, style: "capblue", value: 40, text: "AMOUNT", scale: true, min: 0, max: 10 },
      { type: "knob", x: 240, y: 70, w: 22, h: 22, style: "capred", value: 55, text: "FREQ", scale: true, min: 2, max: 8 },
      { type: "knob", x: 360, y: 70, w: 22, h: 22, style: "capwhite", value: 30, text: "SPEED", scale: true, min: 10, max: 200 },
      { type: "ladder", x: 440, y: 110, w: 5, h: 50, segments: 12, value: 50 },
      { type: "button", x: 120, y: 140, w: 14, h: 9, style: "square", on: true, colour: "#46e070", text: "IN" },
      { type: "jack", x: 360, y: 140, w: 16, h: 16, style: "xlr", text: "OUT" } ]),
  };

  // ---------------------------------------------------------------------------------------------------
  // Saving in this browser only (a convenience; it never leaves it), share, export
  function save () { try { localStorage.setItem ("enh-designer", JSON.stringify (design)); } catch (_) { /* private mode: fine */ } }
  function load () { try { const s = localStorage.getItem ("enh-designer"); if (s && s.length < MAX_JSON) return sanitize (JSON.parse (s)); } catch (_) { /* ignore */ } return null; }

  // My designs: named copies in this browser (localStorage), never uploaded. Each is sanitized on the way in.
  const LIB = "enh-designer-library";
  const readLib = () => { try { const a = JSON.parse (localStorage.getItem (LIB) || "[]"); return Array.isArray (a) ? a.slice (0, 200) : []; } catch (_) { return []; } };
  const writeLib = (a) => { try { localStorage.setItem (LIB, JSON.stringify (a)); return true; } catch (_) { return false; } };
  function showLib () {
    const ul = $("lib-list"); ul.replaceChildren();
    const lib = readLib();
    if (!lib.length) { const li = document.createElement ("li"); li.className = "muted small"; li.textContent = "No saved designs yet."; ul.appendChild (li); return; }
    lib.forEach ((e, i) => {
      const li = document.createElement ("li");
      const n = document.createElement ("span"); n.className = "d-lib-name"; n.textContent = text (e.name, 40, "Untitled"); li.appendChild (n);
      const when = document.createElement ("span"); when.className = "muted small"; when.textContent = new Date (Number (e.at) || 0).toLocaleDateString(); li.appendChild (when);
      const mk = (t, f, cls) => { const b = document.createElement ("button"); b.type = "button"; b.textContent = t; if (cls) b.className = cls; b.addEventListener ("click", f); li.appendChild (b); };
      mk ("Open", () => { replaceDesign (sanitize (e.design)); $("lib-name").value = text (e.name, 40, ""); });
      mk ("Rename", () => { const nn = text (prompt ("New name", e.name) || "", 40, ""); if (!nn) return; const a = readLib(); a[i].name = nn; writeLib (a); showLib(); });
      mk ("Delete", () => { if (!confirm ("Delete \"" + text (e.name, 40, "") + "\"?")) return; const a = readLib(); a.splice (i, 1); writeLib (a); showLib(); }, "danger");
      ul.appendChild (li);
    });
  }
  $("lib-save").addEventListener ("click", () => {
    const name = text ($("lib-name").value, 40, "").trim() || design.unit.name || "Untitled";
    const a = readLib(), at = Date.now(), found = a.findIndex ((e) => e.name === name);
    const entry = { name, at, design: sanitize (JSON.parse (JSON.stringify (design))) };
    if (found >= 0) { if (!confirm ("Replace the saved \"" + name + "\"?")) return; a[found] = entry; } else a.unshift (entry);
    if (!writeLib (a)) alert ("This browser would not keep it (private mode or storage is full)."); showLib();
  });

  const dialog = $("share-dialog");
  $("share").addEventListener ("click", async () => {
    const code = await encode (design); $("share-code").value = code; $("import-msg").textContent = "";
    dialog.showModal();
  });
  $("share-close").addEventListener ("click", () => dialog.close());
  const copy = async (s, btn) => { try { await navigator.clipboard.writeText (s); const t = btn.textContent; btn.textContent = "Copied"; setTimeout (() => (btn.textContent = t), 1400); } catch (_) { $("share-code").select(); } };
  $("copy-code").addEventListener ("click", (e) => copy ($("share-code").value, e.currentTarget));
  $("copy-link").addEventListener ("click", (e) => copy (location.href.split ("#")[0] + "#d=" + $("share-code").value, e.currentTarget));
  $("import-go").addEventListener ("click", async () => {
    try { replaceDesign (await decode ($("import-code").value)); dialog.close(); }
    catch (err) { $("import-msg").textContent = err && err.message ? err.message : "That code could not be opened."; }
  });

  function download (blob, name) { const a = document.createElement ("a"); a.href = URL.createObjectURL (blob); a.download = name; a.click(); setTimeout (() => URL.revokeObjectURL (a.href), 2000); }
  const fileName = (ext) => (design.unit.name || "unit").toLowerCase().replace (/[^a-z0-9]+/g, "-").replace (/^-|-$/g, "") + "." + ext;
  function svgString () { const was = selected; selected = []; render(); const s = new XMLSerializer().serializeToString (svg); selected = was; render(); return s; }
  $("export-svg").addEventListener ("click", () => download (new Blob ([svgString()], { type: "image/svg+xml" }), fileName ("svg")));
  $("export-png").addEventListener ("click", () => {
    const url = URL.createObjectURL (new Blob ([svgString()], { type: "image/svg+xml" })), img = new Image();
    img.onload = () => { const c = document.createElement ("canvas"), s = 4; c.width = (W + 12) * s; c.height = (design.unit.height * U + 12) * s;
      c.getContext ("2d").drawImage (img, 0, 0, c.width, c.height); URL.revokeObjectURL (url); c.toBlob ((b) => b && download (b, fileName ("png")), "image/png"); };
    img.src = url;
  });
  $("clear").addEventListener ("click", () => replaceDesign (blank()));
  $("undo").addEventListener ("click", undo); $("redo").addEventListener ("click", redo);
  $("play").addEventListener ("click", () => setPlay (!play));
  for (const b of document.querySelectorAll ("[data-align]")) b.addEventListener ("click", () => align (b.getAttribute ("data-align")));
  const setZoom = (z) => { zoom = Math.min (4, Math.max (0.4, z)); $("zoom-val").textContent = Math.round (zoom * 100) + "%"; render(); };
  $("zoom-in").addEventListener ("click", () => setZoom (zoom * 1.2)); $("zoom-out").addEventListener ("click", () => setZoom (zoom / 1.2));
  $("zoom-fit").addEventListener ("click", () => { const w = $("stage").clientWidth - 24; setZoom (w / ((W + 12) * 2)); });

  // Palette and templates
  for (const type in TYPES) { const b = document.createElement ("button"); b.type = "button"; b.textContent = TYPES[type].label; b.addEventListener ("click", () => addPart (type)); $("palette").appendChild (b); }
  for (const name in templates) { const b = document.createElement ("button"); b.type = "button"; b.textContent = name;
    b.addEventListener ("click", () => replaceDesign (sanitize (templates[name]()))); $("templates").appendChild (b); }

  // ?selftest: hostile share codes through the decoder, and a round trip (results printed on the page)
  async function selftest () {
    const out = []; const ok = (c, m) => out.push ((c ? "PASS " : "FAIL ") + m);
    const mk = async (obj) => "ENH1." + b64u (await streamBytes (new TextEncoder().encode (JSON.stringify (obj)), new CompressionStream ("deflate-raw")));
    const evil = await mk ({ v: 1, u: { name: "<img src=x onerror=alert(1)>", model: "\u0000\u202e", height: 99, finish: "javascript:", colour: "red;x", ink: "#FFFFFF",
      ears: "x", extra: "secret" }, p: [ { t: "script", x: 1 }, { t: "knob", x: 1e9, y: -5, w: "NaN", s: "evil", v: 1e6, l: "<b>" + "A".repeat (500) },
      { t: "label", l: "javascript:alert(1)", z: 999 }, ...Array.from ({ length: 400 }, () => ({ t: "led" })) ], __proto__: { polluted: true } });
    const d = await decode (evil);
    ok (!/[<>]/.test (d.unit.name) && d.unit.name.length <= 40, "markup is stripped from names: " + JSON.stringify (d.unit.name));
    ok (d.unit.height === 4 && d.unit.finish === "anodised" && d.unit.colour === "#16171a" && d.unit.ink === "#ffffff" && d.unit.ears === "slots", "unknown or out-of-range unit fields fall back or clamp");
    ok (!("extra" in d.unit), "fields that are not design data are dropped");
    ok (!d.parts.some ((p) => p.type === "script"), "unknown part types are dropped");
    ok (d.parts.length === MAX_PARTS, "at most " + MAX_PARTS + " parts (" + d.parts.length + ")");
    const k = d.parts[0]; ok (k.x === W && k.y === 0 && k.value === 100 && k.style === "ribbed" && k.text.length <= 40 && !/[<>]/.test (k.text), "part numbers clamp, styles checked, text limited");
    ok (!({}).polluted, "no prototype pollution");
    let threw = false; try { await decode ("ENH1." + "A".repeat (MAX_CODE + 10)); } catch (_) { threw = true; } ok (threw, "oversized codes are refused");
    threw = false; try { await decode ("<script>alert(1)</script>"); } catch (_) { threw = true; } ok (threw, "non-codes are refused");
    const zip = await mk ({ v: 1, u: {}, p: [], pad: "x".repeat (400000) }); threw = false; try { await decode (zip); } catch (_) { threw = true; } ok (threw, "a code that unpacks to too much is refused (" + zip.length + " chars)");
    const t = sanitize (templates["Program EQ"]()), code = await encode (t), back = await decode (code);
    const strip = (x) => JSON.stringify (x.parts.map ((p) => Object.assign ({}, p, { id: 0 })).map ((p) => Object.fromEntries (Object.entries (p).map (([a, b]) => [a, typeof b === "number" ? Math.round (b * 10) / 10 : b]))));
    ok (JSON.stringify (back.unit) === JSON.stringify (t.unit) && strip (back) === strip (t), "a design survives the round trip (" + code.length + " chars)");
    const pre = document.createElement ("pre"); pre.id = "selftest"; pre.textContent = out.join ("\n"); document.body.prepend (pre);
    document.title = out.every ((l) => l.startsWith ("PASS")) ? "SELFTEST PASS" : "SELFTEST FAIL";
  }

  // Start: a shared link's design, else the one saved in this browser, else the FET template
  (async () => {
    document.documentElement.classList.remove ("no-js");
    let start = load();
    if (location.hash.startsWith ("#d=")) { try { start = await decode (location.hash); } catch (_) { /* keep the saved one */ } }
    design = start || sanitize (templates["FET compressor"]());
    nextId = Math.max (nextId, ...design.parts.map ((p) => p.id + 1), 1);
    last = JSON.stringify (design);
    bindUnit(); syncUnit(); render(); props(); showLib();
    setZoom (Math.min (1.4, ($("stage").clientWidth - 24) / ((W + 12) * 2)));
    if (location.search.includes ("selftest")) selftest();
  })();
}());
