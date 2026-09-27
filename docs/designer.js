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
    knob:    { label: "Knob", w: 22, h: 22, defaults: { style: "ribbed", value: 50, text: "GAIN", scale: true, min: 0, max: 10, size: 22, pointer: "auto",
                                                   steps: 10, nums: "ends", lean: false, sweep: 270, arcText: false,
                                                   marks: "ticks", bipolar: false, ring: false, ringColour: "#ff8a2a", suffix: "", labelPos: "below", labelSize: 2.6, ink: "print", detent: false } },
    toggle:  { label: "Toggle", w: 10, h: 18, defaults: { style: "bat", on: true, text: "IN", three: false, mid: false, upText: "", downText: "", ink: "print" } },
    button:  { label: "Button", w: 12, h: 10, defaults: { style: "square", on: false, colour: "#e0a84a", text: "BYPASS", led: false, momentary: false, capText: "", ink: "print" } },
    led:     { label: "LED", w: 4, h: 4, defaults: { colour: "#46e070", on: true, text: "", shape: "round", blink: false, bezel: "chrome", ink: "print" } },
    vu:      { label: "VU meter", w: 64, h: 36, defaults: { style: "cream", value: 55, text: "VU", dial: "vu", light: false, peak: false } },
    ladder:  { label: "LED ladder", w: 6, h: 40, defaults: { segments: 10, value: 60, text: "", horizontal: false, palette: "classic", peak: false } },
    display: { label: "Display", w: 90, h: 30, defaults: { colour: "#56c8f5", text: "ENH", kind: "wave", content: "", backlit: false } },
    label:   { label: "Text", w: 40, h: 8, defaults: { text: "LABEL", size: 5, bold: true, align: "center", bend: "none", curve: 40, radius: 20, start: 0, flip: false,
                                                   italic: false, spacing: 1, look: "print", ink: "print" } },
    box:     { label: "Section box", w: 90, h: 34, defaults: { text: "SECTION", round: 3, fill: false, lineStyle: "solid", lineW: 0.35, tone: "lighter", fillCol: "#2a2b30", ink: "print" } },
    line:    { label: "Line", w: 60, h: 1, defaults: { dashed: false, ink: "print" } },
    jack:    { label: "Jack", w: 14, h: 14, defaults: { style: "trs", text: "INPUT", plugged: false, nut: "chrome", cable: "#141416", ink: "print" } },
    screw:   { label: "Screw", w: 5, h: 5, defaults: { style: "unit", metal: "unit" } },
    vent:    { label: "Vent", w: 40, h: 16, defaults: { count: 6, shape: "slots" } },
    slider:  { label: "Slider (fader)", w: 12, h: 60, defaults: { style: "black", value: 50, text: "LEVEL", scale: true, horizontal: false, steps: 10, ink: "print" } },
    selector: { label: "Rotary switch", w: 22, h: 22, defaults: { style: "chicken", value: 0, text: "MODE", stops: "LOW|MID|HIGH", sweep: 240, pointer: "auto", ink: "print" } },
    lamp:    { label: "Pilot lamp", w: 10, h: 10, defaults: { style: "jewel", colour: "#ff3b1f", on: true, text: "POWER", ink: "print" } },
    plate:   { label: "Nameplate", w: 60, h: 14, defaults: { style: "brass", text: "SERIAL 0001", screws: true } },
  };
  /* Finishes: name, the colour it comes in (null: keeps the panel's), how it looks in 3D (roughness,
     metalness, clear coat), how strong the 2D sheen is, and the surface effects drawn over the colour */
  const FINISH = {
    anodised:    ["Anodised",             null,      0.42, 0.45, 0.2,  0.7, []],
    brushed:     ["Brushed aluminium",    "#b4b6ba", 0.35, 0.85, 0.2,  1.0, ["brushed"]],
    paint:       ["Paint",                null,      0.50, 0.10, 0.6,  1.0, []],
    hammertone:  ["Hammertone",           null,      0.50, 0.10, 0.6,  1.0, ["hammer"]],
    gloss:       ["Piano gloss",          null,      0.12, 0.05, 1.0,  1.6, ["mirror"]],
    satin:       ["Satin lacquer",        null,      0.38, 0.05, 0.4,  0.6, []],
    wrinkle:     ["Wrinkle paint",        null,      0.70, 0.05, 0.1,  0.5, ["wrinkle"]],
    powder:      ["Powder coat",          null,      0.62, 0.05, 0.15, 0.6, ["grain"]],
    enamel:      ["Vintage enamel",       null,      0.28, 0.05, 0.8,  1.2, ["grain", "mottle"]],
    sandblast:   ["Bead-blasted aluminium", "#a9abaf", 0.55, 0.8, 0.0, 0.5, ["grain"]],
    spun:        ["Spun aluminium",       "#c3c5c9", 0.30, 0.9,  0.2,  1.0, ["spun"]],
    chrome:      ["Mirror chrome",        "#d9dbe0", 0.04, 1.0,  0.3,  1.8, ["mirror"]],
    blackchrome: ["Black chrome",         "#2a2b2f", 0.06, 1.0,  0.3,  1.8, ["mirror"]],
    gold:        ["Brushed gold",         "#c9a24a", 0.30, 1.0,  0.3,  1.2, ["brushed"]],
    copper:      ["Brushed copper",       "#b8683f", 0.32, 1.0,  0.3,  1.2, ["brushed"]],
    titanium:    ["Brushed titanium",     "#7d7f84", 0.36, 0.9,  0.2,  1.0, ["brushed"]],
    carbon:      ["Carbon fibre",         "#1a1b1e", 0.20, 0.2,  1.0,  1.4, ["carbon"]],
    walnut:      ["Walnut veneer",        "#5a3721", 0.35, 0.0,  0.9,  1.1, ["wood"]],
    rosewood:    ["Rosewood veneer",      "#4a1f1a", 0.32, 0.0,  0.9,  1.1, ["wood"]],
    bakelite:    ["Bakelite",             "#3b2417", 0.22, 0.0,  0.8,  1.3, ["mottle"]],
    pearl:       ["Pearl",                "#e9e4ea", 0.25, 0.3,  1.0,  1.3, ["pearl"]],
    candy:       ["Candy metallic",       null,      0.20, 0.5,  1.0,  1.5, ["flake", "mirror"]],
    flake:       ["Metal-flake",          null,      0.28, 0.6,  0.9,  1.2, ["flake"]],
    tolex:       ["Tolex vinyl",          "#1c1c1d", 0.75, 0.0,  0.0,  0.4, ["tolex"]],
    diamond:     ["Diamond plate",        "#a7a9ad", 0.35, 0.9,  0.1,  1.0, ["diamond"]],
    perforated:  ["Perforated steel",     "#6f7176", 0.45, 0.8,  0.1,  0.8, ["perf"]],
    patina:      ["Verdigris patina",     "#8a5a3a", 0.60, 0.6,  0.0,  0.5, ["patina"]],
    rust:        ["Raw steel, rusted",    "#5d5f63", 0.70, 0.7,  0.0,  0.4, ["rust"]],
  };
  const FINISHES = Object.keys (FINISH), FINISH_NAMES = Object.fromEntries (FINISHES.map ((k) => [k, FINISH[k][0]]));
  const EARS = ["slots", "holes", "none"];
  const HANDLES = ["none", "bar", "loop"], SCREWS = ["phillips", "hex", "thumb"];
  const TOGGLES = ["bat", "rocker", "rockerred", "paddle", "mini", "slide"], BUTTONS = ["square", "round", "pill"];
  const VUS = ["cream", "amber", "black", "white", "green", "blue"];
  // The newer settings' choices (each a fixed list: a share code can only pick from them)
  const INKS = ["print", "accent", "white", "black", "red", "gold"], MARKS = ["ticks", "dots", "arc"], LABEL_POS = ["below", "above", "none"];
  const LED_SHAPES = ["round", "square", "rect", "triangle"], BEZELS = ["chrome", "black", "none"], DIALS = ["vu", "ppm", "percent", "gr"];
  const PALETTES = ["classic", "green", "blue", "amber", "white", "red"], DISPLAYS = ["wave", "bars", "spectrum", "digits", "text", "blank"];
  const LOOKS = ["print", "engraved", "embossed", "outline"], LINES = ["solid", "dashed", "double", "none"], TONES = ["lighter", "darker", "colour"];
  const NUTS = ["chrome", "black", "gold"], SCREW_STYLES = ["unit", "phillips", "hex", "thumb", "torx", "flat"], METALS = ["unit", "chrome", "black", "brass"];
  const VENTS = ["slots", "holes", "hex", "louvre", "grille"], FADERS = ["black", "silver", "white", "red"], LAMPS = ["jewel", "dome", "square"];
  const PLATES = ["brass", "silver", "gold", "black"];
  const TRIMS = ["none", "pinstripe", "double", "inset"], TWO_TONES = ["none", "left", "right", "top", "bottom", "band"];
  const TITLE_POS = ["topleft", "topcentre", "bottomleft", "hidden"], SCREW_METALS = ["chrome", "black", "brass"];
  const INK_COLOUR = { white: "#f2f2f2", black: "#111113", red: "#d8322b", gold: "#d4af37" };
  /* Sockets: name, how it is drawn, its accent colour (insulator, nut, ring) */
  const JACK_TYPES = {
    trs: ["1/4\" jack (TRS)", "round"], ts: ["1/4\" jack (TS)", "round"], headphone: ["Headphones (1/4\")", "round"],
    xlr: ["XLR (female)", "xlrf"], xlrm: ["XLR (male)", "xlrm"], combo: ["XLR / 1/4\" combo", "combo"],
    mini: ["3.5 mm mini jack", "mini"], tt: ["Bantam (TT) patch", "tt"],
    rcared: ["RCA, red", "rca", "#c8302a"], rcawhite: ["RCA, white", "rca", "#e8e6e0"], rcablack: ["RCA, black", "rca", "#1a1a1c"],
    rcayellow: ["RCA, yellow (video)", "rca", "#e0c030"], spdif: ["S/PDIF coax (orange)", "rca", "#e07a20"],
    bnc: ["BNC (word clock)", "bnc"], toslink: ["Optical (TOSLINK / ADAT)", "toslink"], midi: ["MIDI (5-pin DIN)", "din"],
    bananared: ["Banana post, red", "banana", "#c8302a"], bananablack: ["Banana post, black", "banana", "#1a1a1c"],
    speakon: ["speakON", "speakon", "#1a1a1c"], powercon: ["powerCON", "speakon", "#2f6fd0"], iec: ["IEC mains inlet", "iec"],
    dcbarrel: ["DC barrel", "dc"], usba: ["USB-A", "usba"], usbb: ["USB-B", "usbb"], usbc: ["USB-C", "usbc"],
    rj45: ["Ethernet (RJ45)", "rj45"], db25: ["D-sub 25 (DB25)", "dsub"],
  };
  const JACKS = Object.keys (JACK_TYPES), JACK_NAMES = Object.fromEntries (JACKS.map ((k) => [k, JACK_TYPES[k][0]]));
  const EDGES = ["square", "rounded", "bevel"], FONTS = ["sans", "serif", "mono", "condensed"], EARCOLS = ["match", "black", "silver"];
  const ALIGNS = ["left", "center", "right"], POINTERS = ["auto", "white", "cream", "black", "red"];
  const NUMS = ["ends", "all", "none"], BENDS = ["none", "curve", "circle"];
  const FONT_FAMILY = { sans: "Inter, Segoe UI, Helvetica, Arial, sans-serif", serif: "Georgia, Times New Roman, serif",
    mono: "ui-monospace, Menlo, Consolas, monospace", condensed: "Arial Narrow, Roboto Condensed, Helvetica Neue, sans-serif" };
  const POINTER_COLOUR = { white: "#f2f2f2", cream: "#e9dfc6", black: "#111111", red: "#d8322b" };

  const blank = () => ({ v: 1, unit: { name: "MY UNIT", model: "EM-X", height: 1, finish: "anodised", colour: "#16171a",
    ink: "#e8e8ea", ears: "slots", handles: "none", screws: "phillips", wear: 15, edge: "rounded", font: "sans", badge: "",
    earColour: "match", sub: "", shine: 50, desc: "",
    accent: "#ff8a2a", trim: "none", twoTone: "none", toneColour: "#2a2b30", toneSize: 30, titlePos: "topleft", titleSize: 4.4,
    glow: false, serial: "", screwMetal: "chrome", chassis: "#1a1a1c", depth: 180 }, knobs: [], parts: [] });

  /* Custom knobs (the Knobs tab): up to MAX_KNOBS per design, each a small set of checked choices and
     clamped numbers - never markup, never a free-form shape. Parts use them as style "c0" .. "c7". */
  const MAX_KNOBS = 8;
  const CK_SHAPES = ["cyl", "taper", "dome", "tophat", "cone", "pointer"], CK_GRIPS = ["none", "ribs", "knurl", "flutes"];
  const CK_CAPS = ["none", "flat", "dome"], CK_POINTERS = ["line", "dot", "notch", "none"], CK_MATS = ["gloss", "satin", "matte", "metal", "rubber"];
  const CK_NAMES = {
    sh: { cyl: "Cylinder", taper: "Tapered", dome: "Domed", tophat: "Top hat", cone: "Cone", pointer: "Pointer bar" },
    gr: { none: "Smooth", ribs: "Ribbed", knurl: "Knurled", flutes: "Fluted" },
    cap: { none: "No cap", flat: "Flat cap", dome: "Domed cap" },
    pt: { line: "Painted line", dot: "Dot", notch: "Notch", none: "No pointer" },
    mt: { gloss: "Gloss plastic", satin: "Satin plastic", matte: "Matte", metal: "Metal", rubber: "Rubber" },
  };
  const blankKnob = () => ({ n: "MY KNOB", sh: "cyl", ht: 0.8, tp: 0.92, sk: 0, gr: "ribs", gc: 24, gd: 0.5, cap: "none", cs: 0.7,
                             pt: "line", bc: "#141416", kc: "#c9cacf", sc: "#141416", pc: "#f2f2f2", mt: "satin" });

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
  // A description: the same printable text, in up to 20 lines
  const textBlock = (v, max) => (typeof v !== "string" ? "" : v.replace (/\r\n?/g, "\n").split ("\n").slice (0, 20).map ((l) => text (l, max, "")).join ("\n").slice (0, max));

  /** Each part's style, from its own list (knobs: the built-in ones and the design's own). */
  const STYLE_LISTS = { knob: (ks) => ks, selector: (ks) => ks, toggle: () => TOGGLES, button: () => BUTTONS, vu: () => VUS,
    screw: () => SCREW_STYLES, slider: () => FADERS, lamp: () => LAMPS, plate: () => PLATES };
  /** The newer settings, each checked by its own rule: a choice from its list, a clamped number, a
      #rrggbb colour, a yes/no or short plain text - anything else falls back to the default. */
  const one = (list) => (v, d) => pick (v, list, d), num = (lo, hi) => (v, d) => clamp (v, lo, hi, d);
  const FIELDS = {
    marks: one (MARKS), bipolar: bool, ring: bool, ringColour: colour, suffix: (v, d) => text (v, 6, d), labelPos: one (LABEL_POS), labelSize: num (1.5, 6),
    ink: one (INKS), detent: bool, three: bool, mid: bool, upText: (v, d) => text (v, 8, d), downText: (v, d) => text (v, 8, d),
    led: bool, momentary: bool, capText: (v, d) => text (v, 6, d), shape: (v, d) => pick (v, LED_SHAPES.includes (d) ? LED_SHAPES : VENTS, d),
    blink: bool, bezel: one (BEZELS), dial: one (DIALS), light: bool, peak: bool, horizontal: bool, palette: one (PALETTES),
    kind: one (DISPLAYS), content: (v, d) => text (v, 24, d), backlit: bool, italic: bool, spacing: num (0, 3), look: one (LOOKS),
    lineStyle: one (LINES), lineW: num (0.1, 2), tone: one (TONES), fillCol: colour, dashed: bool, nut: one (NUTS), cable: colour,
    metal: one (METALS), stops: (v, d) => stopList (typeof v === "string" ? v : d).join ("|"), screws: bool,
  };
  /** A rotary switch's positions: 2 - 12 names, each short plain text. */
  function stopList (s) {
    const a = String (s || "").split ("|").slice (0, 12).map ((x) => text (x, 10, "").trim()).filter ((x) => x.length);
    while (a.length < 2) a.push (String (a.length + 1));
    return a;
  }

  function sanitize (raw) {
    const d = blank();
    if (!raw || typeof raw !== "object") return d;
    const u = raw.unit && typeof raw.unit === "object" ? raw.unit : {};
    d.unit = {
      name: text (u.name, 40, d.unit.name), model: text (u.model, 24, d.unit.model),
      height: Math.round (clamp (u.height, 1, 6, 1)), finish: pick (u.finish, FINISHES, "anodised"),
      colour: colour (u.colour, d.unit.colour), ink: colour (u.ink, d.unit.ink), ears: pick (u.ears, EARS, "slots"),
      handles: pick (u.handles, HANDLES, "none"), screws: pick (u.screws, SCREWS, "phillips"), wear: Math.round (clamp (u.wear, 0, 100, 15)),
      edge: pick (u.edge, EDGES, "rounded"), font: pick (u.font, FONTS, "sans"), badge: text (u.badge, 16, ""),
      earColour: pick (u.earColour, EARCOLS, "match"), sub: text (u.sub, 40, ""),
      shine: Math.round (clamp (u.shine, 0, 100, 50)), desc: textBlock (u.desc, 600),
      accent: colour (u.accent, "#ff8a2a"), trim: pick (u.trim, TRIMS, "none"), twoTone: pick (u.twoTone, TWO_TONES, "none"),
      toneColour: colour (u.toneColour, "#2a2b30"), toneSize: Math.round (clamp (u.toneSize, 10, 90, 30)),
      titlePos: pick (u.titlePos, TITLE_POS, "topleft"), titleSize: clamp (u.titleSize, 3, 9, 4.4), glow: bool (u.glow, false),
      serial: text (u.serial, 16, ""), screwMetal: pick (u.screwMetal, SCREW_METALS, "chrome"), chassis: colour (u.chassis, "#1a1a1c"),
      depth: Math.round (clamp (u.depth, 60, 400, 180)),
    };
    // Custom knobs first (the parts may use them), each rebuilt from checked choices and clamped numbers
    const sk = (k) => { const b = blankKnob(); if (!k || typeof k !== "object") return b;
      return { n: text (k.n, 20, b.n) || b.n, sh: pick (k.sh, CK_SHAPES, b.sh), ht: clamp (k.ht, 0.3, 2.0, b.ht), tp: clamp (k.tp, 0.4, 1.1, b.tp),
               sk: clamp (k.sk, 0, 1.6, b.sk), gr: pick (k.gr, CK_GRIPS, b.gr), gc: Math.round (clamp (k.gc, 6, 120, b.gc)), gd: clamp (k.gd, 0, 1, b.gd),
               cap: pick (k.cap, CK_CAPS, b.cap), cs: clamp (k.cs, 0.3, 1, b.cs), pt: pick (k.pt, CK_POINTERS, b.pt),
               bc: colour (k.bc, b.bc), kc: colour (k.kc, b.kc), sc: colour (k.sc, b.sc), pc: colour (k.pc, b.pc), mt: pick (k.mt, CK_MATS, b.mt) }; };
    d.knobs = Array.isArray (raw.knobs) ? raw.knobs.slice (0, MAX_KNOBS).map (sk) : [];
    const knobStyles = KNOBS.concat (d.knobs.map ((_, i) => "c" + i));
    const H = d.unit.height * U;
    const parts = Array.isArray (raw.parts) ? raw.parts.slice (0, MAX_PARTS * 4) : [];   // (read a bounded amount)
    for (const p of parts) {
      if (d.parts.length >= MAX_PARTS) break;
      if (!p || typeof p !== "object" || !Object.prototype.hasOwnProperty.call (TYPES, p.type)) continue;
      const t = TYPES[p.type], def = t.defaults;
      const q = { id: nextId++, type: p.type,
        x: clamp (p.x, 0, W, W / 2), y: clamp (p.y, 0, H, H / 2),
        w: clamp (p.w, 1, W, t.w), h: clamp (p.h, 0.5, 4 * U, t.h), rot: clamp (p.rot, -180, 180, 0) };
      if ("style" in def) q.style = pick (p.style, STYLE_LISTS[p.type] ? STYLE_LISTS[p.type] (knobStyles) : JACKS, def.style);
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
      if ("steps" in def) q.steps = Math.round (clamp (p.steps, 2, 20, def.steps));
      if ("nums" in def) q.nums = pick (p.nums, NUMS, def.nums);
      if ("lean" in def) q.lean = bool (p.lean, def.lean);
      if ("sweep" in def) q.sweep = clamp (p.sweep, 180, 330, def.sweep);
      if ("arcText" in def) q.arcText = bool (p.arcText, def.arcText);
      if ("bend" in def) q.bend = pick (p.bend, BENDS, def.bend);
      if ("curve" in def) q.curve = clamp (p.curve, -100, 100, def.curve);
      if ("radius" in def) q.radius = clamp (p.radius, 2, 240, def.radius);
      if ("start" in def) q.start = clamp (p.start, -180, 180, def.start);
      if ("flip" in def) q.flip = bool (p.flip, def.flip);
      if ("plugged" in def) q.plugged = bool (p.plugged, def.plugged);
      for (const key in FIELDS) if (key in def) q[key] = FIELDS[key] (p[key], def[key]);
      if (p.type === "selector") q.value = Math.round (clamp (p.value, 0, stopList (q.stops).length - 1, 0));
      q.lock = bool (p.lock, false);
      const grp = Math.round (clamp (p.grp, 0, 9999, 0)); if (grp > 0) q.grp = grp;
      d.parts.push (q);
    }
    return d;
  }

  // ---------------------------------------------------------------------------------------------------
  /* Share codes: short keys, anything left at its default left out -> JSON -> deflate -> base 62 (letters
     and digits), in groups of four: "ENH2-7KQ2-M9XA-...". The code IS the design; nothing is stored anywhere.
     (Older "ENH1." / "ENH0." codes still open.) */
  const SHORT = { type: "t", x: "x", y: "y", w: "w", h: "h", rot: "r", style: "s", value: "v", text: "l", scale: "c", min: "a", max: "b",
    on: "o", colour: "k", segments: "g", size: "z", bold: "d", round: "n", count: "u", align: "e", pointer: "i", fill: "f", lock: "q",
    steps: "st", nums: "nu", lean: "le", sweep: "sw", arcText: "at", bend: "be", curve: "cu", radius: "ra", start: "sa", flip: "fl", grp: "gp", plugged: "pl",
    marks: "mk", bipolar: "bp", ring: "rg", ringColour: "rc", suffix: "sx", labelPos: "lp", labelSize: "lz", ink: "ik", detent: "dt", three: "th", mid: "md",
    upText: "ut", downText: "dx", led: "ld", momentary: "mo", capText: "ct", shape: "sh", blink: "bk", bezel: "bz", dial: "di", light: "li", peak: "pk",
    horizontal: "hz", palette: "pa", kind: "kd", content: "co", backlit: "bl", italic: "it", spacing: "sp", look: "lk", lineStyle: "ls", lineW: "lw",
    tone: "tn", fillCol: "fc", dashed: "da", nut: "nt", cable: "cb", metal: "me", stops: "so", screws: "sc" };
  const LONG = Object.fromEntries (Object.entries (SHORT).map (([a, b]) => [b, a]));
  const round1 = (n) => Math.round (n * 10) / 10;

  function pack (d) {
    const bu = blank().unit, u = {};
    for (const k in d.unit) if (d.unit[k] !== bu[k]) u[k] = d.unit[k];
    return { v: 2, u, k: d.knobs && d.knobs.length ? d.knobs : undefined, p: d.parts.map ((p) => {
      const t = TYPES[p.type], def = t.defaults, o = {};
      for (const k in SHORT) {
        if (!(k in p) || k === "id") continue;
        const v = typeof p[k] === "number" ? round1 (p[k]) : p[k];
        // what sanitize would fill in anyway stays out of the code
        if (k !== "type" && (v === def[k] || (k === "w" && v === t.w) || (k === "h" && v === t.h) || (k === "rot" && v === 0) || (k === "lock" && v === false))) continue;
        o[SHORT[k]] = v;
      }
      return o; }) };
  }
  function unpack (o) {
    if (!o || typeof o !== "object") return null;
    return { unit: o.u, knobs: Array.isArray (o.k) ? o.k : [], parts: Array.isArray (o.p) ? o.p.map ((p) => { const q = {}; if (p && typeof p === "object") for (const k in p) if (LONG[k]) q[LONG[k]] = p[k]; return q; }) : [] };
  }
  const b64u = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode (b); return btoa (s).replace (/\+/g, "-").replace (/\//g, "_").replace (/=+$/, ""); };
  const unb64u = (s) => { const b = atob (s.replace (/-/g, "+").replace (/_/g, "/")); return Uint8Array.from (b, (c) => c.charCodeAt (0)); };
  async function streamBytes (bytes, stream) {
    const out = new Response (new Blob ([bytes]).stream().pipeThrough (stream));
    return new Uint8Array (await out.arrayBuffer());
  }
  const B62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  function toB62 (bytes) {   // a leading 1 byte keeps any leading zero bytes
    let n = BigInt ("0x01" + Array.from (bytes, (b) => b.toString (16).padStart (2, "0")).join ("")), out = "";
    while (n > 0n) { out = B62[Number (n % 62n)] + out; n /= 62n; }
    return out;
  }
  function fromB62 (str) {
    let n = 0n;
    for (const c of str) { const i = B62.indexOf (c); if (i < 0) throw new Error ("That code has a character a design code never has."); n = n * 62n + BigInt (i); }
    let hex = n.toString (16); if (hex.length % 2) hex = "0" + hex;
    if (!hex.startsWith ("01")) throw new Error ("That code is damaged (a character is missing or wrong).");
    return Uint8Array.from (hex.slice (2).match (/../g) || [], (h) => parseInt (h, 16));
  }
  async function encode (d) {
    const json = new TextEncoder().encode (JSON.stringify (pack (d)));
    if (typeof CompressionStream !== "function") return "ENH0." + b64u (json);
    const z = await streamBytes (json, new CompressionStream ("deflate-raw"));
    return "ENH2-" + toB62 (z).match (/.{1,4}/g).join ("-");
  }
  async function decode (code) {
    code = String (code || "").trim().replace (/^.*#d=/, "").replace (/\s+/g, "");
    if (code.length > MAX_CODE) throw new Error ("That code is too long to be a design.");
    const m2 = /^ENH2-?([0-9A-Za-z-]+)$/.exec (code);
    const m = m2 ? null : /^ENH([01])\.([A-Za-z0-9_-]+)$/.exec (code);
    if (!m && !m2) throw new Error ("That is not an ENH Master design code (they start with ENH2-).");
    let bytes = m2 ? fromB62 (m2[1].replace (/-/g, "")) : unb64u (m[2]);
    if (m2 || m[1] === "1") {
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
    if (design.unit.glow) t.setAttribute ("filter", "url(#inkglow)");
    return t;
  };
  // Text along a path: the path is drawn invisibly beside it and referred to by an id unique to this drawing
  let pathSeq = 0;
  const pathText = (parent, d, s, size, fill, opts = {}) => {
    const id = "tp" + (++pathSeq);
    el ("path", { id, d, fill: "none" }, parent);
    const t = el ("text", { "font-size": size, fill, "text-anchor": opts.anchor || "middle", "font-family": FONT_FAMILY[design.unit.font] || FONT_FAMILY.sans,
      "font-weight": opts.bold === false ? 500 : 700, "letter-spacing": opts.spacing ?? size * 0.12, "dominant-baseline": "middle" }, parent);
    const tp = el ("textPath", { href: "#" + id, startOffset: opts.offset || "50%" }, t);
    tp.textContent = s;   // text, never markup
    return t;
  };
  // A whole circle of radius R round (0, 0), its middle at `at` degrees (0 = top, clockwise); `inside`: read
  // from the inside (along the bottom, left to right) - the path runs the other way round
  const circlePath = (R, at, inside) => {
    const P = (a) => { const r = a * Math.PI / 180; return (R * Math.sin (r)).toFixed (3) + " " + (-R * Math.cos (r)).toFixed (3); };
    const sw = inside ? 0 : 1;
    return `M ${P (at + 180)} A ${R} ${R} 0 1 ${sw} ${P (at)} A ${R} ${R} 0 1 ${sw} ${P (at + 180)}`;
  };
  // An arc across width w, bowed by `curve` (-100..100: up to a half circle; + arches up)
  const curvePath = (w, curve) => {
    const sag = curve / 100 * w / 2;
    if (Math.abs (sag) < 0.01) return `M ${-w / 2} 0 L ${w / 2} 0`;
    const R = (w * w / 4 + sag * sag) / (2 * Math.abs (sag)), y = sag / 2;
    return `M ${-w / 2} ${y.toFixed (3)} A ${R.toFixed (3)} ${R.toFixed (3)} 0 0 ${sag > 0 ? 1 : 0} ${w / 2} ${y.toFixed (3)}`;
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
    g ("vuWhite", [[0, "#fbfbf8"], [1, "#e2e2dc"]]);
    g ("vuGreen", [[0, "#12351f"], [1, "#07160c"]]);
    g ("vuBlue", [[0, "#10233f"], [1, "#060d1a"]]);
    g ("vuLamp", [[0, "#ffe2a0", 0.9], [0.6, "#ffb040", 0.25], [1, "#ff9020", 0]]);
    r ("brass", [[0, "#f3dd9a"], [0.5, "#c9a64a"], [1, "#7d6224"]]);
    // Backlit print: the lettering's own light round it
    const glow = el ("filter", { id: "inkglow", x: -0.3, y: -0.6, width: 1.6, height: 2.2 }, d);
    el ("feGaussianBlur", { in: "SourceGraphic", stdDeviation: 0.7, result: "b" }, glow);
    const mg = el ("feMerge", {}, glow); el ("feMergeNode", { in: "b" }, mg); el ("feMergeNode", { in: "b" }, mg); el ("feMergeNode", { in: "SourceGraphic" }, mg);
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
    // The finishes' surfaces: each filter draws only its own marks (alpha), over the panel's colour
    const noiseFilter = (id, type, freq, oct, seed, matrix) => { const f = el ("filter", { id, x: 0, y: 0, width: 1, height: 1 }, d);
      el ("feTurbulence", { type, baseFrequency: freq, numOctaves: oct, seed, result: "n" }, f);
      el ("feColorMatrix", { in: "n", type: "matrix", values: matrix, result: "a" }, f);
      el ("feComposite", { in: "a", in2: "SourceGraphic", operator: "in" }, f); };
    noiseFilter ("grain", "fractalNoise", 1.1, 1, 5, "0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0.9 0.9 0.9 0 -1.05");
    noiseFilter ("flake", "fractalNoise", 1.8, 1, 9, "0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  14 0 0 0 -10.2");   // (only the highest of the noise: sparse glints)
    noiseFilter ("wood", "fractalNoise", "0.005 0.32", 3, 4, "0 0 0 0 0.10  0 0 0 0 0.05  0 0 0 0 0.02  1.6 0 0 0 -0.55");
    noiseFilter ("mottle", "fractalNoise", 0.025, 3, 12, "0 0 0 0 0.05  0 0 0 0 0.02  0 0 0 0 0.01  1.3 0 0 0 -0.55");
    noiseFilter ("patina", "fractalNoise", 0.035, 4, 21, "0 0 0 0 0.30  0 0 0 0 0.62  0 0 0 0 0.52  2.6 0 0 0 -1.25");
    noiseFilter ("rust", "fractalNoise", 0.05, 4, 31, "0 0 0 0 0.52  0 0 0 0 0.22  0 0 0 0 0.08  3.0 0 0 0 -1.55");
    const litFilter = (id, freq, scale, seed) => { const f = el ("filter", { id, x: 0, y: 0, width: 1, height: 1 }, d);
      el ("feTurbulence", { type: "turbulence", baseFrequency: freq, numOctaves: 2, seed, result: "n" }, f);
      el ("feDiffuseLighting", { in: "n", "surface-scale": scale, "lighting-color": "#ffffff", result: "l" }, f).appendChild (el ("feDistantLight", { azimuth: 225, elevation: 50 }));
      el ("feComposite", { in: "l", in2: "SourceGraphic", operator: "arithmetic", k1: 0.9, k2: 0, k3: 0, k4: 0 }, f); };
    litFilter ("wrinkle", 0.4, 2.2, 17);
    litFilter ("tolex", 0.8, 1.0, 23);
    const pat = (id, w, h, build) => { const p = el ("pattern", { id, width: w, height: h, patternUnits: "userSpaceOnUse" }, d); build (p); };
    pat ("carbon", 2.4, 2.4, (p) => { el ("rect", { width: 2.4, height: 2.4, fill: "#0e0f11" }, p);
      el ("rect", { x: 0, y: 0, width: 1.2, height: 1.2, fill: "#2b2d32" }, p); el ("rect", { x: 1.2, y: 1.2, width: 1.2, height: 1.2, fill: "#2b2d32" }, p);
      el ("rect", { x: 0, y: 0.5, width: 1.2, height: 0.2, fill: "#3b3e44" }, p); el ("rect", { x: 1.7, y: 1.2, width: 0.2, height: 1.2, fill: "#3b3e44" }, p); });
    pat ("diamond", 7, 7, (p) => { for (const [x, y, a] of [[1.75, 1.75, 45], [5.25, 5.25, -45]]) {
      el ("rect", { x: x - 1.9, y: y - 0.45, width: 3.8, height: 0.9, rx: 0.45, fill: "#ffffff", opacity: 0.45, transform: `rotate(${a} ${x} ${y})` }, p);
      el ("rect", { x: x - 1.9, y: y + 0.1, width: 3.8, height: 0.4, rx: 0.2, fill: "#000000", opacity: 0.35, transform: `rotate(${a} ${x} ${y})` }, p); } });
    pat ("perf", 3.2, 3.2, (p) => { el ("circle", { cx: 1.6, cy: 1.6, r: 0.75, fill: "#050506" }, p); el ("circle", { cx: 1.6, cy: 1.35, r: 0.75, fill: "#ffffff", opacity: 0.12 }, p); });
    g ("mirror", [[0, "#ffffff", 0.55], [0.18, "#ffffff", 0.05], [0.42, "#000000", 0.35], [0.55, "#ffffff", 0.35], [0.7, "#000000", 0.25], [1, "#ffffff", 0.2]]);
    g ("pearl", [[0, "#ffc9ea"], [0.35, "#bff7ef"], [0.65, "#fff0bd"], [1, "#d5c9ff"]], 1, 0.3);
    const wear = el ("filter", { id: "wear", x: 0, y: 0, width: 1, height: 1 }, d);
    el ("feTurbulence", { type: "fractalNoise", baseFrequency: "0.02 0.3", numOctaves: 3, seed: 11, result: "n" }, wear);
    el ("feColorMatrix", { in: "n", type: "matrix", values: "0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 2.2 -1.35" }, wear);
  }

  /* `ownDefs` false: the pictures on the page (dropdowns, preset cards) use the stage's gradients and
     filters instead of copies with the same ids - a copy inside a hidden tab would be the one the browser
     finds first, and paints nothing: they all find the set at the top of the page (#d-defs). Exports and
     the 3D print carry their own. */
  function render (root = svg, rmode = "edit", ownDefs = true) {
    mode = rmode;
    while (root.lastChild && root.lastChild.nodeName !== "title") root.removeChild (root.lastChild);
    const u = design.unit, H = u.height * U, print = mode === "print";
    root.setAttribute ("viewBox", print ? `0 0 ${W} ${H}` : `-6 -6 ${W + 12} ${H + 12}`);
    root.setAttribute ("width", print ? W : (W + 12) * 2 * zoom);
    root.setAttribute ("height", print ? H : (H + 12) * 2 * zoom);
    if (ownDefs) defs (root);
    const face = el ("g", {}, root);
    const rx = u.edge === "square" ? 0.2 : u.edge === "bevel" ? 0.6 : 1.4;
    // The plate: shadow, body, finish, edge highlight, wear
    if (!print) el ("rect", { x: 0.8, y: 1.6, width: W, height: H, rx, fill: "#000", opacity: 0.55, filter: "url(#drop)" }, face);
    el ("rect", { x: 0, y: 0, width: W, height: H, rx, fill: u.colour }, face);
    // Two-tone: a second paint over part of the plate (a side, the top or bottom, or a band across the middle)
    if (u.twoTone && u.twoTone !== "none") {
      const f = (u.toneSize || 30) / 100, t = u.twoTone;
      const box = t === "left" ? [0, 0, W * f, H] : t === "right" ? [W * (1 - f), 0, W * f, H] : t === "top" ? [0, 0, W, H * f]
                : t === "bottom" ? [0, H * (1 - f), W, H * f] : [0, H * (0.5 - f / 2), W, H * f];
      el ("rect", { x: box[0], y: box[1], width: box[2], height: box[3], rx: t === "band" ? 0 : rx, fill: u.toneColour }, face);
    }
    drawFinish (face, 0, 0, W, H, rx, u.finish, u.colour, print);
    // Trim: a pinstripe (or two) in the accent colour, or a pressed-in inset line, inside the ears
    if (u.trim && u.trim !== "none") {
      const inset = (u.ears === "none" ? 3 : EAR + 2), line = (d, attrs) => el ("rect", Object.assign ({ x: inset + d, y: 2 + d, width: W - 2 * (inset + d), height: H - 4 - 2 * d, rx: 1.2, fill: "none" }, attrs), face);
      if (u.trim === "inset") { line (0.25, { stroke: "#000", "stroke-width": 0.5, opacity: 0.45 }); line (0, { stroke: "#fff", "stroke-width": 0.35, opacity: 0.18 }); }
      else { line (0, { stroke: u.accent, "stroke-width": 0.45 }); if (u.trim === "double") line (1.3, { stroke: u.accent, "stroke-width": 0.25 }); }
    }
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
    if (u.titlePos !== "hidden") {   // the maker's block: top left (as it always was), top centre, or bottom left
      const ts = u.titleSize || 4.4, centre = u.titlePos === "topcentre", x = centre ? W / 2 : left, anchor = centre ? "middle" : "start";
      const y0 = u.titlePos === "bottomleft" ? H - 5 - (u.sub ? 4.5 : 0) - 5.5 - ts * 0.5 : 6.8 + ts * 0.5;
      txt (face, x, y0, u.name, ts, u.ink, { anchor, spacing: ts * 0.2 });
      txt (face, x, y0 + ts * 0.5 + 3.4, "MODEL " + u.model, 2.3, u.ink, { anchor, spacing: 0.4, bold: false });
      if (u.sub) txt (face, x, y0 + ts * 0.5 + 7.9, u.sub, 2.1, u.ink, { anchor, spacing: 0.35, bold: false });
    }
    if (u.serial) txt (face, W - (u.ears === "none" ? 8 : EAR + 8), H - 3.5, "SN " + u.serial, 1.9, u.ink, { anchor: "end", spacing: 0.3, bold: false });
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
      el ("rect", { x: b.x - 1.5, y: b.y - 1.5, width: b.w + 3, height: b.h + 3, rx: 1.5, fill: "none", stroke: p.lock ? "#f59bd6" : p.grp ? "#c7a6ff" : "#7fe3e0", "stroke-width": 0.6, "stroke-dasharray": "2 1.2", class: "d-sel" }, face);
    }
    // The box being dragged out to select with
    if (root === svg && drag && drag.mode === "box" && drag.cur) {
      const r = boxRect (drag.start, drag.cur);
      el ("rect", { x: r.x, y: r.y, width: r.w, height: r.h, fill: "#7fe3e0", "fill-opacity": 0.08, stroke: "#7fe3e0", "stroke-width": 0.4, "stroke-dasharray": "1.5 1" }, face);
    }
    mode = "edit";
    if (root === svg) notify();
  }

  /** A finish's surface over a panel already filled with its colour: its marks, then (not in print: the 3D
      view lights it itself) the sheen. Spun rings are centred on the area. */
  function drawFinish (parent, x, y, w, h, rx, fin, col, print, shine = design.unit.shine ?? 50) {
    const f = FINISH[fin] || FINISH.anodised, box = { x, y, width: w, height: h, rx };
    const gloss = f[5] * shine / 50;   // SHINE: 50 is the finish as it comes
    for (const fx of f[6]) {
      if (fx === "brushed") el ("rect", Object.assign ({ fill: "#fff", filter: "url(#brushed)", opacity: 0.35 }, box), parent);
      else if (fx === "hammer") el ("rect", Object.assign ({ fill: col, filter: "url(#hammer)", opacity: 0.45 }, box), parent);
      else if (fx === "wrinkle") el ("rect", Object.assign ({ fill: col, filter: "url(#wrinkle)", opacity: 0.55 }, box), parent);
      else if (fx === "tolex") el ("rect", Object.assign ({ fill: col, filter: "url(#tolex)", opacity: 0.6 }, box), parent);
      else if (["grain", "flake", "wood", "mottle", "patina", "rust"].includes (fx))
        el ("rect", Object.assign ({ fill: "#fff", filter: `url(#${fx})`, opacity: fx === "grain" ? 0.35 : fx === "flake" ? 0.55 : 0.9 }, box), parent);
      else if (["carbon", "diamond", "perf"].includes (fx)) el ("rect", Object.assign ({ fill: `url(#${fx})`, opacity: fx === "carbon" ? 0.92 : 1 }, box), parent);
      else if (fx === "pearl") el ("rect", Object.assign ({ fill: "url(#pearl)", opacity: 0.32 }, box), parent);
      else if (fx === "mirror" && !print) el ("rect", Object.assign ({ fill: "url(#mirror)", opacity: 0.5 }, box), parent);
      else if (fx === "spun") {
        const id = "spun" + (++pathSeq), rg = el ("radialGradient", { id, gradientUnits: "userSpaceOnUse", cx: x + w / 2, cy: y + h / 2, r: Math.max (0.9, w / 400), spreadMethod: "repeat" }, parent);
        for (const [o, a] of [[0, 0], [0.5, 0.28], [1, 0]]) el ("stop", { offset: o, "stop-color": "#ffffff", "stop-opacity": a }, rg);
        el ("rect", Object.assign ({ fill: `url(#${id})` }, box), parent);
      }
    }
    if (!print) el ("rect", Object.assign ({ fill: "url(#sheen)", opacity: Math.min (1, gloss) }, box), parent);
    if (!print && gloss > 1) el ("rect", Object.assign ({ fill: "url(#sheen)", opacity: Math.min (1, gloss - 1) }, box), parent);
    if (!print && gloss > 1.6) el ("rect", Object.assign ({ fill: "url(#mirror)", opacity: Math.min (0.5, (gloss - 1.6) * 0.4) }, box), parent);
  }

  function screw (parent, x, y, style, r, metal = design.unit.screwMetal || "chrome") {
    const head = { chrome: "url(#knobAlu)", black: "url(#knobBlack)", brass: "url(#brass)" }[metal] || "url(#knobAlu)", slot = metal === "black" ? "#5a5a60" : "#333";
    el ("circle", { cx: x, cy: y, r, fill: head, filter: "url(#drop)" }, parent);
    const bar = (a, len) => el ("line", { x1: x - Math.cos (a) * r * len, y1: y - Math.sin (a) * r * len, x2: x + Math.cos (a) * r * len, y2: y + Math.sin (a) * r * len, stroke: slot, "stroke-width": 0.55 }, parent);
    if (style === "phillips") { bar (0, 0.55); bar (Math.PI / 2, 0.55); }
    else if (style === "flat") bar (Math.PI / 5, 0.75);
    else if (style === "hex" || style === "torx") { const pts = [], n = style === "hex" ? 6 : 12;
      for (let i = 0; i < n; ++i) { const a = Math.PI * 2 / n * i, rr = r * (style === "torx" && i % 2 ? 0.28 : 0.45); pts.push ((x + Math.cos (a) * rr).toFixed (2) + "," + (y + Math.sin (a) * rr).toFixed (2)); }
      el ("polygon", { points: pts.join (" "), fill: metal === "black" ? "#050506" : "#2a2a2c" }, parent); }
    else { el ("circle", { cx: x, cy: y, r: r * 1.25, fill: "none", stroke: "#8f9095", "stroke-width": 0.5, "stroke-dasharray": "0.5 0.6" }, parent); }
  }

  // How much room a part takes (text round a circle takes the circle's)
  const extent = (p) => { if (p.type === "label" && p.bend === "circle") { const d = 2 * (p.radius + p.size * 0.7); return { w: d, h: d }; } return { w: p.w, h: p.h }; };
  const bounds = (p) => { const e = extent (p); return { x: p.x - e.w / 2, y: p.y - e.h / 2, w: e.w, h: e.h }; };
  const boxRect = (a, b) => ({ x: Math.min (a.x, b.x), y: Math.min (a.y, b.y), w: Math.abs (a.x - b.x), h: Math.abs (a.y - b.y) });
  // A knob's angle for 0..100 (degrees from the top, clockwise), over its sweep (270 unless set)
  const angle = (v, sweep = 270) => -sweep / 2 + sweep * v / 100;
  const knobAngle = (p) => angle (p.value, p.sweep || 270);
  const fmt = (n) => String (Math.round (n * 10) / 10);

  /** A part's print colour: the panel's print, its accent, or one of a few fixed inks. */
  const inkOf = (p) => (!p.ink || p.ink === "print" ? design.unit.ink : p.ink === "accent" ? design.unit.accent || design.unit.ink : INK_COLOUR[p.ink] || design.unit.ink);
  const blinkOn = () => !play || Math.floor (performance.now() / 500) % 2 === 0;
  const METAL_FILL = { chrome: "url(#knobAlu)", black: "url(#knobBlack)", brass: "url(#brass)", gold: "url(#brass)" };

  function drawPart (parent, p) {
    const g = el ("g", { "data-id": p.id, class: "d-part", transform: `translate(${p.x} ${p.y})${p.rot ? ` rotate(${p.rot})` : ""}` }, parent);
    const ink = inkOf (p), r = Math.min (p.w, p.h) / 2, printing = mode === "print";
    // A knob's (or rotary switch's) name: under it, over it, or not at all; straight or curved
    const knobLabel = (R) => {
      if (!p.text || p.labelPos === "none") return;
      const size = p.labelSize || 2.6, above = p.labelPos === "above";
      if (p.arcText) pathText (g, circlePath (R + size * 0.2, above ? 0 : 180, !above), p.text, size, ink, { spacing: 0.45 });
      else txt (g, 0, above ? -R - size * 0.2 : R, p.text, size, ink, { spacing: 0.45 });
    };
    switch (p.type) {
      case "knob": {
        const sweep = p.sweep || 270, steps = p.steps || 10, ringR = r + 2.4, out = p.ring ? (r + 4.2) / (r * 1.18) : 1;   // (an LED ring pushes the scale out)
        if (p.scale) {   // ticks (or dots, or an arc) round the knob; numbers at the ends and middle, at every step, or none
          const half = steps % 2 === 0 ? steps / 2 : -1, every = p.nums === "all";
          const nsize = r * (every ? Math.max (0.15, Math.min (0.24, 2.4 / steps)) : 0.24);
          if (p.marks === "arc") {
            const a0 = (angle (0, sweep) - 90) * Math.PI / 180, a1 = (angle (100, sweep) - 90) * Math.PI / 180, R = r * 1.28 * out;
            el ("path", { d: `M ${Math.cos (a0) * R} ${Math.sin (a0) * R} A ${R} ${R} 0 ${sweep > 180 ? 1 : 0} 1 ${Math.cos (a1) * R} ${Math.sin (a1) * R}`, fill: "none", stroke: ink, "stroke-width": 0.45 }, g);
          }
          for (let i = 0; i <= steps; ++i) {
            const deg = angle (i / steps * 100, sweep), a = (deg - 90) * Math.PI / 180;
            const major = i === 0 || i === steps || i === half, centre = p.bipolar && i === half;
            const r0 = r * 1.18 * out, r1 = r * (major || every ? 1.38 : 1.3) * out;
            if (p.marks === "dots") el ("circle", { cx: Math.cos (a) * r * 1.3 * out, cy: Math.sin (a) * r * 1.3 * out, r: (major ? 0.55 : 0.35) * (centre ? 1.5 : 1), fill: ink }, g);
            else el ("line", { x1: Math.cos (a) * r0, y1: Math.sin (a) * r0, x2: Math.cos (a) * r1 * (centre ? 1.06 : 1), y2: Math.sin (a) * r1 * (centre ? 1.06 : 1), stroke: ink, "stroke-width": major ? 0.5 : 0.3 }, g);
            if (p.nums === "none" || (!every && !major)) continue;
            const x = Math.cos (a) * r * 1.62 * out, y = Math.sin (a) * r * 1.62 * out;
            let v = p.min + (p.max - p.min) * i / steps; if (p.bipolar && i === half) v = 0;
            const label = fmt (v) + (p.suffix && (i === 0 || i === steps) ? " " + p.suffix : "");
            const t = txt (g, x, y, label, nsize, ink, { bold: false, spacing: 0 });
            if (p.lean) t.setAttribute ("transform", `rotate(${deg.toFixed (2)} ${x.toFixed (3)} ${y.toFixed (3)})`);   // turned with the dial
          }
        }
        if (p.ring) {   // an LED ring round it: lit from the start (or from the middle, centre-zero) to where it points
          const n = 15, R = ringR;
          for (let i = 0; i < n; ++i) {
            const t = i / (n - 1) * 100, a = (angle (t, sweep) - 90) * Math.PI / 180;
            const lit = !printing && (p.bipolar ? (p.value >= 50 ? t >= 50 && t <= p.value : t <= 50 && t >= p.value) : t <= p.value);
            if (lit) el ("circle", { cx: Math.cos (a) * R, cy: Math.sin (a) * R, r: 1.5, fill: p.ringColour, opacity: 0.25 }, g);
            el ("circle", { cx: Math.cos (a) * R, cy: Math.sin (a) * R, r: 0.7, fill: lit ? p.ringColour : shade (p.ringColour, -0.55) }, g);
          }
        }
        knobLabel (r * (p.scale ? 1.62 * out : 1.2) + (p.ring && !p.scale ? 3 : 0) + 3);
        if (!printing) knob (g, p.style, r, angle (p.value, sweep), p.pointer);
        break;
      }
      case "selector": {   // a rotary switch: its positions named round it, the knob on the chosen one
        const stops = stopList (p.stops), n = stops.length, sweep = p.sweep || 240, at = Math.min (n - 1, Math.max (0, Math.round (p.value)));
        for (let i = 0; i < n; ++i) {
          const deg = angle (i / Math.max (1, n - 1) * 100, sweep), a = (deg - 90) * Math.PI / 180;
          el ("line", { x1: Math.cos (a) * r * 1.15, y1: Math.sin (a) * r * 1.15, x2: Math.cos (a) * r * 1.32, y2: Math.sin (a) * r * 1.32, stroke: ink, "stroke-width": 0.45 }, g);
          const x = Math.cos (a) * r * 1.75, y = Math.sin (a) * r * 1.75;
          txt (g, x, y, stops[i], Math.max (1.6, Math.min (2.4, r * 0.2)), ink, { bold: i === at, spacing: 0.2 });
        }
        if (p.text) txt (g, 0, r * 2.15 + 2, p.text, 2.6, ink, { spacing: 0.45 });
        if (!printing) knob (g, p.style, r, angle (at / Math.max (1, n - 1) * 100, sweep), p.pointer);
        break;
      }
      case "slider": {   // a fader: its slot, a scale beside it, and its cap
        const hz = p.horizontal, L = hz ? p.w : p.h, T = hz ? p.h : p.w, steps = p.steps || 10;
        const tr = (x, y) => (hz ? [y, x] : [x, y]);   // (drawn upright, turned for horizontal)
        const rect = (x, y, w, h, attrs) => { const [a, b] = tr (x, y), [c, d] = hz ? [h, w] : [w, h]; return el ("rect", Object.assign ({ x: a, y: b, width: c, height: d }, attrs), g); };
        rect (-0.9, -L / 2, 1.8, L, { rx: 0.9, fill: "#050506" });
        if (p.scale) for (let i = 0; i <= steps; ++i) {
          const y = L / 2 - L * i / steps, major = i === 0 || i === steps || i * 2 === steps;
          for (const side of [-1, 1]) { const [x1, y1] = tr (side * T * 0.42, y), [x2, y2] = tr (side * T * (major ? 0.72 : 0.6), y); el ("line", { x1, y1, x2, y2, stroke: ink, "stroke-width": major ? 0.4 : 0.25 }, g); }
        }
        if (p.text) { const [x, y] = hz ? [0, T / 2 + 3.5] : [0, L / 2 + 4]; txt (g, x, y, p.text, 2.5, ink, { spacing: 0.4 }); }
        if (!printing) {
          const pos = (hz ? -1 : 1) * (L / 2 - L * p.value / 100) * (hz ? -1 : 1), capL = Math.min (10, L * 0.18);
          const fill = { black: "url(#knobBlack)", silver: "url(#knobAlu)", white: "url(#capWhite)", red: "url(#knobRed)" }[p.style] || "url(#knobBlack)";
          const y = hz ? -pos : pos;
          rect (-T * 0.5, y - capL / 2, T, capL, { rx: 1, fill, filter: "url(#drop)" });
          rect (-T * 0.5, y - 0.2, T, 0.4, { fill: p.style === "white" || p.style === "silver" ? "#111" : "#f2f2f2" });
        }
        break;
      }
      case "toggle": {
        const upDown = () => { if (p.upText) txt (g, 0, -12, p.upText, 2.2, ink, { spacing: 0.3, bold: false }); if (p.downText) txt (g, 0, 11.5, p.downText, 2.2, ink, { spacing: 0.3, bold: false }); };
        const nameY = p.downText ? 15.5 : 12;
        if (printing) { if (p.text) txt (g, 0, nameY, p.text, 2.5, ink, { spacing: 0.4 }); upDown(); if (p.style !== "bat" && p.style !== "mini") el ("rect", { x: -4, y: -7, width: 8, height: 14, rx: 1.2, fill: "#050506" }, g); break; }
        const dir = p.three && p.mid ? 0 : p.on ? -1 : 1;
        if (p.style === "bat" || p.style === "mini" || p.style === "paddle") {
          const k = p.style === "mini" ? 0.7 : 1;
          el ("circle", { r: 3.6 * k, fill: "url(#knobAlu)", filter: "url(#drop)" }, g);
          el ("circle", { r: 2.2 * k, fill: "#2b2c30" }, g);
          if (p.style === "paddle") el ("rect", { x: -2.2, y: dir < 0 ? -8.5 : dir > 0 ? 0 : -1.5, width: 4.4, height: dir === 0 ? 3 : 8.5, rx: 1.2, fill: "url(#knobBlack)", filter: "url(#drop)" }, g);
          else if (dir === 0) el ("circle", { r: 1.7 * k, fill: "url(#metalV)", filter: "url(#drop)" }, g);
          else el ("path", { d: `M ${-1.1 * k} 0 L ${-1.6 * k} ${dir * 8 * k} A ${1.6 * k} ${1.6 * k} 0 0 0 ${1.6 * k} ${dir * 8 * k} L ${1.1 * k} 0 Z`, fill: "url(#metalV)", filter: "url(#drop)" }, g);
        } else if (p.style === "slide") {
          el ("rect", { x: -2.4, y: -7, width: 4.8, height: 14, rx: 1, fill: "#050506" }, g);
          el ("rect", { x: -2, y: (dir < 0 ? -6.5 : dir > 0 ? 0.5 : -3), width: 4, height: 6, rx: 0.8, fill: "url(#knobBlack)", filter: "url(#drop)" }, g);
        } else {
          const c = p.style === "rockerred" ? "#b3231c" : "#18191c";
          el ("rect", { x: -4, y: -7, width: 8, height: 14, rx: 1.2, fill: "#050506" }, g);
          el ("rect", { x: -3.2, y: -6.2, width: 6.4, height: 12.4, rx: 1, fill: c, filter: "url(#drop)" }, g);
          if (dir !== 0) el ("rect", { x: -3.2, y: dir < 0 ? -6.2 : 0, width: 6.4, height: 6.2, rx: 1, fill: "#fff", opacity: 0.12 }, g);
          txt (g, 0, -3, "I", 2.4, "#eee", {}); txt (g, 0, 3, "O", 2.4, "#eee", {});
        }
        upDown();
        if (p.text) txt (g, 0, nameY, p.text, 2.5, ink, { spacing: 0.4 });
        break;
      }
      case "button": {
        const ledY = -p.h / 2 - 3.5;
        if (printing) { if (p.text) txt (g, 0, p.h / 2 + 4, p.text, 2.5, ink, { spacing: 0.4 }); if (p.led) el ("circle", { cy: ledY, r: 1.3, fill: "#1a1a1c" }, g); break; }
        const lit = p.on ? p.colour : shade (p.colour, -0.72), rx = p.style === "pill" ? Math.min (p.w, p.h) / 2 : 1;
        if (p.style === "round") { el ("circle", { r: r + 0.8, fill: "#08080a" }, g); el ("circle", { r, fill: lit, filter: "url(#drop)" }, g); el ("circle", { r, fill: "url(#sheen)" }, g); }
        else { el ("rect", { x: -p.w / 2 - 0.8, y: -p.h / 2 - 0.8, width: p.w + 1.6, height: p.h + 1.6, rx: rx + 0.4, fill: "#08080a" }, g);
               el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx, fill: lit, filter: "url(#drop)" }, g);
               el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx, fill: "url(#sheen)" }, g); }
        if (p.capText) txt (g, 0, 0, p.capText, Math.min (p.h * 0.45, 3), "#111113", { spacing: 0.2 });
        if (p.on) el ("circle", { r: Math.max (p.w, p.h) * 0.9, fill: p.colour, opacity: 0.12 }, g);
        if (p.led) { el ("circle", { cy: ledY, r: 1.6, fill: "#1a1a1c" }, g); el ("circle", { cy: ledY, r: 1.1, fill: p.on ? p.colour : shade (p.colour, -0.75) }, g);
                     if (p.on) el ("circle", { cy: ledY, r: 3, fill: p.colour, opacity: 0.2 }, g); }
        if (p.text) txt (g, 0, p.h / 2 + 4, p.text, 2.5, ink, { spacing: 0.4 });
        break;
      }
      case "led": {
        const labelY = (p.shape === "rect" ? r * 0.6 : r) + 4;
        if (printing) { if (p.text) txt (g, 0, labelY, p.text, 2.2, ink, { spacing: 0.3 }); break; }
        const lit = p.on && (!p.blink || blinkOn()), c = lit ? p.colour : shade (p.colour, -0.75);
        const shapeAt = (rr, fill) => {
          if (p.shape === "square") return el ("rect", { x: -rr, y: -rr, width: rr * 2, height: rr * 2, rx: rr * 0.2, fill }, g);
          if (p.shape === "rect") return el ("rect", { x: -rr * 1.5, y: -rr * 0.6, width: rr * 3, height: rr * 1.2, rx: rr * 0.15, fill }, g);
          if (p.shape === "triangle") return el ("path", { d: `M 0 ${-rr * 1.1} L ${rr} ${rr * 0.7} L ${-rr} ${rr * 0.7} Z`, fill }, g);
          return el ("circle", { r: rr, fill }, g);
        };
        if (lit) el ("circle", { r: r * 3.2, fill: p.colour, opacity: 0.18 }, g);
        if (p.bezel !== "none") shapeAt (r + 0.6, p.bezel === "black" ? "#0d0d0f" : "url(#knobAlu)");
        shapeAt (r, c);
        el ("circle", { cx: -r * 0.3, cy: -r * 0.35, r: r * 0.3, fill: "#fff", opacity: lit ? 0.7 : 0.25 }, g);
        if (p.text) txt (g, 0, labelY, p.text, 2.2, ink, { spacing: 0.3 });
        break;
      }
      case "lamp": {   // a pilot lamp: a jewel (faceted), a dome, or a square lens, in a chrome bezel
        if (p.text) txt (g, 0, r + 4.5, p.text, 2.4, ink, { spacing: 0.4 });
        if (printing) break;
        const lit = p.on, c = lit ? p.colour : shade (p.colour, -0.7);
        if (lit) el ("circle", { r: r * 2.6, fill: p.colour, opacity: 0.2 }, g);
        if (p.style === "square") { el ("rect", { x: -r - 0.8, y: -r - 0.8, width: 2 * r + 1.6, height: 2 * r + 1.6, rx: 1, fill: "url(#knobAlu)", filter: "url(#drop)" }, g);
                                    el ("rect", { x: -r, y: -r, width: 2 * r, height: 2 * r, rx: 0.6, fill: c }, g); }
        else { el ("circle", { r: r + 0.9, fill: "url(#knobAlu)", filter: "url(#drop)" }, g); el ("circle", { r, fill: c }, g);
          if (p.style === "jewel") for (let i = 0; i < 8; ++i) { const a = i / 8 * Math.PI * 2; el ("line", { x1: 0, y1: 0, x2: Math.cos (a) * r, y2: Math.sin (a) * r, stroke: "#fff", "stroke-width": 0.25, opacity: 0.35 }, g); } }
        el ("circle", { cx: -r * 0.3, cy: -r * 0.35, r: r * 0.35, fill: "#fff", opacity: lit ? 0.55 : 0.2 }, g);
        break;
      }
      case "vu": {
        const w = p.w, h = p.h, dark = ["black", "green", "blue"].includes (p.style);
        el ("rect", { x: -w / 2 - 1, y: -h / 2 - 1, width: w + 2, height: h + 2, rx: 1.5, fill: "#050506" }, g);
        el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h, rx: 1, fill: `url(#vu${p.style[0].toUpperCase()}${p.style.slice (1)})` }, g);
        if (p.light) el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h, rx: 1, fill: "url(#vuLamp)", opacity: 0.55 }, g);
        const inkVu = dark ? "#e8e8ea" : "#1d1c1a", cx = 0, cy = h * 0.55, R = h * 0.78;
        // The dial: VU (-20 .. +3, red from 0), PPM (1 .. 7), percent, or gain reduction (read right to left)
        const labels = { vu: ["-20", "-10", "-5", "0", "+3"], ppm: ["1", "2", "3", "4", "5", "6", "7"], percent: ["0", "20", "40", "60", "80", "100"], gr: ["20", "10", "5", "3", "1", "0"] }[p.dial || "vu"];
        const redFrom = p.dial === "vu" || !p.dial ? 8 : p.dial === "ppm" ? 9 : 11;
        for (let i = 0; i <= 10; ++i) { const a = (-50 + i * 10) * Math.PI / 180, r0 = R * 0.86, r1 = R * (i % 5 === 0 ? 0.98 : 0.93);
          el ("line", { x1: cx + Math.sin (a) * r0, y1: cy - Math.cos (a) * r0, x2: cx + Math.sin (a) * r1, y2: cy - Math.cos (a) * r1, stroke: i >= redFrom ? "#b3231c" : inkVu, "stroke-width": 0.45 }, g); }
        labels.forEach ((lb, i) => { const a = (-50 + 100 * i / (labels.length - 1)) * Math.PI / 180;
          txt (g, cx + Math.sin (a) * R * 1.08, cy - Math.cos (a) * R * 1.08, lb, h * 0.075, i / (labels.length - 1) * 10 >= redFrom ? "#b3231c" : inkVu, { spacing: 0, bold: false }); });
        if (redFrom <= 10) el ("path", { d: `M ${cx + Math.sin ((-50 + redFrom * 10) * Math.PI / 180) * R * 0.98} ${cy - Math.cos ((-50 + redFrom * 10) * Math.PI / 180) * R * 0.98} A ${R * 0.98} ${R * 0.98} 0 0 1 ${cx + Math.sin (0.87) * R * 0.98} ${cy - Math.cos (0.87) * R * 0.98}`, stroke: "#b3231c", "stroke-width": 1.2, fill: "none" }, g);
        txt (g, 0, h * 0.30, p.text, h * 0.10, inkVu, { spacing: 0.5 });
        const v = p.dial === "gr" ? 100 - p.value : p.value, na = (-50 + v) * Math.PI / 180;
        el ("line", { x1: cx, y1: cy, x2: cx + Math.sin (na) * R * 0.97, y2: cy - Math.cos (na) * R * 0.97, stroke: dark ? "#ff6a3a" : inkVu, "stroke-width": 0.45 }, g);
        el ("circle", { cx, cy, r: 1.6, fill: "#111" }, g);
        if (p.peak) { const lit = !printing && p.value > 85; el ("circle", { cx: w / 2 - 4, cy: -h / 2 + 4, r: 1.3, fill: lit ? "#ff3b30" : "#3a0f0c" }, g); if (lit) el ("circle", { cx: w / 2 - 4, cy: -h / 2 + 4, r: 3, fill: "#ff3b30", opacity: 0.25 }, g); }
        el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h * 0.45, rx: 1, fill: "#fff", opacity: 0.07 }, g);   // the glass
        break;
      }
      case "ladder": {
        const n = p.segments, hz = p.horizontal, L = hz ? p.w : p.h, T = hz ? p.h : p.w, sh = L / n, litN = Math.round (n * p.value / 100);
        const pal = { classic: null, green: "#46e070", blue: "#3aa0ff", amber: "#ffb020", white: "#f4f6ff", red: "#ff3b30" }[p.palette || "classic"];
        for (let i = 0; i < n; ++i) {
          const lit = i < litN || (p.peak && i === Math.min (n - 1, litN + 1));
          const c = pal || (i >= n - 1 ? "#ff4a3a" : i >= n - 3 ? "#ffcc33" : "#46e070");
          const pos = L / 2 - (i + 0.5) * sh, [x, y] = hz ? [-pos, 0] : [0, pos];
          const seg = (a, b, fill, op) => el ("rect", Object.assign (hz ? { x: x - a * 0.36 * 2 / 2, y: -b / 2, width: a * 0.72, height: b } : { x: -b / 2, y: y - a * 0.36, width: b, height: a * 0.72 }, { rx: 0.4, fill }, op ? { opacity: op } : {}), g);
          if (lit && !printing) seg (sh * 1.9, T * 2, c, 0.15);
          seg (sh, T, lit && !printing ? c : shade (c, -0.78));
        }
        break;
      }
      case "display": {
        el ("rect", { x: -p.w / 2 - 1.2, y: -p.h / 2 - 1.2, width: p.w + 2.4, height: p.h + 2.4, rx: 2, fill: "#050506" }, g);
        el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1.2, fill: shade (p.colour, p.backlit ? -0.45 : -0.88) }, g);
        const fg = p.backlit ? "#0a0b0c" : p.colour, x0 = -p.w / 2 + 3, span = p.w - 6;
        const kind = p.kind || "wave", body = p.content || "";
        if (kind === "wave") { let d = ""; for (let i = 0; i <= 60; ++i) { const x = x0 + span * i / 60, y = Math.sin (i * 0.45) * Math.cos (i * 0.11) * p.h * 0.25; d += (i ? " L " : "M ") + x.toFixed (2) + " " + y.toFixed (2); }
          el ("path", { d, fill: "none", stroke: fg, "stroke-width": 0.6, opacity: 0.9 }, g); }
        else if (kind === "bars" || kind === "spectrum") { const n = kind === "bars" ? 8 : 24, bw = span / n;
          for (let i = 0; i < n; ++i) { const hgt = p.h * (kind === "bars" ? 0.25 + 0.4 * Math.abs (Math.sin (i * 1.3 + 0.4)) : 0.55 * Math.exp (-i / 14) * (0.7 + 0.3 * Math.sin (i * 2.1)));
            el ("rect", { x: x0 + i * bw + bw * 0.15, y: p.h / 2 - 2 - hgt, width: bw * 0.7, height: hgt, fill: fg, opacity: 0.9 }, g); } }
        else if (kind === "digits") { const t = (body || "88.8").slice (0, 12); txt (g, 0, 1, t, Math.min (p.h * 0.62, span / Math.max (3, t.length) * 1.5), fg, { spacing: 0.8 })
          .setAttribute ("font-family", FONT_FAMILY.mono); }
        else if (kind === "text") txt (g, 0, 1.5, body || p.text, Math.min (p.h * 0.32, 5), fg, { spacing: 0.4 });
        if (p.text && kind !== "text") txt (g, x0, -p.h / 2 + 3.5, p.text, 2.4, fg, { anchor: "start", spacing: 0.3 });
        el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h * 0.4, rx: 1.2, fill: "#fff", opacity: 0.05 }, g);
        break;
      }
      case "label": {
        const anchor = p.align === "left" ? "start" : p.align === "right" ? "end" : "middle";
        const opts = { bold: p.bold, spacing: p.size * 0.14 * (p.spacing ?? 1), anchor, offset: anchor === "start" ? "0%" : anchor === "end" ? "100%" : "50%" };
        // Printed, engraved (cut in: dark, a light edge under it), embossed (raised: light, a shadow under it), or outlined
        const layers = p.look === "engraved" ? [[0, 0.25, "#ffffff", 0.25], [0, 0, shade (design.unit.colour, -0.6), 1]]
                     : p.look === "embossed" ? [[0, 0.3, "#000000", 0.45], [0, 0, shade (design.unit.colour, 0.35), 1]]
                     : [[0, 0, ink, 1]];
        for (const [dx, dy, fill, op] of layers) {
          const gg = el ("g", { transform: `translate(${dx} ${dy})`, opacity: op }, g);
          let t;
          if (p.bend === "circle") { const o = Object.assign ({}, opts, { anchor: "middle", offset: "50%" }); t = pathText (gg, circlePath (p.radius, p.start, p.flip), p.text, p.size, fill, o); }
          else if (p.bend === "curve") t = pathText (gg, curvePath (p.w, p.curve), p.text, p.size, fill, opts);
          else t = txt (gg, p.align === "left" ? -p.w / 2 : p.align === "right" ? p.w / 2 : 0, 0, p.text, p.size, fill, opts);
          if (p.italic) t.setAttribute ("font-style", "italic");
          if (p.look === "outline") { t.setAttribute ("fill", "none"); t.setAttribute ("stroke", ink); t.setAttribute ("stroke-width", Math.max (0.12, p.size * 0.04)); }
        }
        break;
      }
      case "box": {
        const fill = p.tone === "darker" ? shade (design.unit.colour, -0.25) : p.tone === "colour" ? p.fillCol : shade (design.unit.colour, 0.1);
        if (p.fill) el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: p.round, fill }, g);
        const lw = p.lineW ?? 0.35, stroke = { fill: "none", stroke: ink, "stroke-width": lw, opacity: 0.85 };
        if (p.lineStyle !== "none") {
          el ("rect", Object.assign ({ x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: p.round }, stroke, p.lineStyle === "dashed" ? { "stroke-dasharray": `${lw * 6} ${lw * 4}` } : {}), g);
          if (p.lineStyle === "double") el ("rect", Object.assign ({ x: -p.w / 2 + lw * 3, y: -p.h / 2 + lw * 3, width: Math.max (0, p.w - lw * 6), height: Math.max (0, p.h - lw * 6), rx: Math.max (0, p.round - lw * 3) }, stroke), g);
        }
        if (p.text) { const tw = p.text.length * 2.1 + 3; el ("rect", { x: -tw / 2, y: -p.h / 2 - 1.5, width: tw, height: 3, fill: p.fill ? fill : design.unit.colour }, g); txt (g, 0, -p.h / 2, p.text, 2.5, ink, { spacing: 0.5 }); }
        break;
      }
      case "line": el ("rect", Object.assign ({ x: -p.w / 2, y: -Math.max (0.3, p.h) / 2, width: p.w, height: Math.max (0.3, p.h), fill: ink, opacity: 0.85 },
                                              p.dashed ? { fill: "none", stroke: ink, "stroke-width": Math.max (0.3, p.h), "stroke-dasharray": "2 1.4", height: 0.01, y: 0 } : {}), g); break;
      case "jack": {
        if (printing) { if (p.text) txt (g, 0, r + 4, p.text, 2.4, ink, { spacing: 0.4 }); break; }
        drawJack (g, p.style, r, p.plugged, p.cable, p.nut);
        if (p.text) txt (g, 0, r + 4, p.text, 2.4, ink, { spacing: 0.4 });
        break;
      }
      case "screw": if (!printing) screw (g, 0, 0, p.style && p.style !== "unit" ? p.style : design.unit.screws, r, p.metal && p.metal !== "unit" ? p.metal : design.unit.screwMetal); break;
      case "vent": {
        const n = p.count, sw = p.w / n, dark = "#050506";
        if (p.shape === "holes" || p.shape === "hex") {   // a grid of round (or hexagonal) holes
          const cols = n, rows = Math.max (1, Math.round (p.h / sw)), rr = Math.min (sw, p.h / rows) * 0.34;
          for (let j = 0; j < rows; ++j) for (let i = 0; i < cols; ++i) {
            const x = -p.w / 2 + (i + 0.5 + (p.shape === "hex" && j % 2 ? 0.5 : 0)) * sw, y = -p.h / 2 + (j + 0.5) * p.h / rows;
            if (x > p.w / 2) continue;
            if (p.shape === "hex") { const pts = []; for (let k = 0; k < 6; ++k) { const a = Math.PI / 3 * k + Math.PI / 6; pts.push ((x + Math.cos (a) * rr).toFixed (2) + "," + (y + Math.sin (a) * rr).toFixed (2)); } el ("polygon", { points: pts.join (" "), fill: dark }, g); }
            else el ("circle", { cx: x, cy: y, r: rr, fill: dark }, g);
          }
        } else if (p.shape === "louvre") {   // slanted louvres: a dark gap, a lit lip over it
          const rows = n, rh = p.h / rows;
          for (let j = 0; j < rows; ++j) { const y = -p.h / 2 + j * rh; el ("rect", { x: -p.w / 2, y: y + rh * 0.2, width: p.w, height: rh * 0.45, rx: rh * 0.2, fill: dark }, g);
            el ("rect", { x: -p.w / 2, y: y + rh * 0.62, width: p.w, height: rh * 0.18, rx: rh * 0.09, fill: "#ffffff", opacity: 0.18 }, g); }
        } else if (p.shape === "grille") {   // a mesh behind a frame
          el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill: dark }, g);
          el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill: "url(#perf)", opacity: 0.9 }, g);
          el ("rect", { x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill: "none", stroke: inkOf (p), "stroke-width": 0.35, opacity: 0.6 }, g);
        } else for (let i = 0; i < n; ++i) el ("rect", { x: -p.w / 2 + i * sw + sw * 0.25, y: -p.h / 2, width: sw * 0.5, height: p.h, rx: sw * 0.25, fill: dark }, g);
        break;
      }
      case "plate": {   // a nameplate screwed to the panel: metal, the text engraved into it
        const fill = { brass: "url(#brass)", silver: "url(#knobAlu)", gold: "url(#brass)", black: "url(#knobBlack)" }[p.style] || "url(#brass)";
        el ("rect", Object.assign ({ x: -p.w / 2, y: -p.h / 2, width: p.w, height: p.h, rx: 1, fill }, printing ? {} : { filter: "url(#drop)" }), g);
        el ("rect", { x: -p.w / 2 + 0.8, y: -p.h / 2 + 0.8, width: p.w - 1.6, height: p.h - 1.6, rx: 0.6, fill: "none", stroke: "#000", "stroke-width": 0.25, opacity: 0.4 }, g);
        const inkP = p.style === "black" ? "#d9d9dc" : "#1d1a14";
        if (p.text) txt (g, 0, 0.2, p.text, Math.min (p.h * 0.4, 4.5), inkP, { spacing: 0.4 });
        if (p.screws && !printing) for (const x of [-p.w / 2 + 2.6, p.w / 2 - 2.6]) screw (g, x, 0, "phillips", 1.3, p.style === "black" ? "black" : p.style === "silver" ? "chrome" : "brass");
        break;
      }
    }
    // an invisible hit area the size of the part, so small parts are easy to grab
    const e = extent (p);
    el ("rect", { x: -e.w / 2 - 2, y: -e.h / 2 - 2, width: e.w + 4, height: e.h + 4, fill: "#000", opacity: 0 }, g);
  }

  /** A socket, drawn as its type looks from the front; plugged: its plug in it and the cable hanging out. */
  function drawJack (g, style, r, plugged, cable = "#0d0d0f", nut = "chrome") {
    const [, shape, accent = "#1a1a1c"] = JACK_TYPES[style] || JACK_TYPES.trs;
    const nutFill = { chrome: "url(#knobAlu)", black: "url(#knobBlack)", gold: "url(#brass)" }[nut] || "url(#knobAlu)";
    const ring = (rr, fill) => el ("circle", { r: rr, fill }, g);
    const hole = (rr) => el ("circle", { r: rr, fill: "#0b0b0d" }, g);
    const pin = (x, y, rr, fill = "#c9a64a") => el ("circle", { cx: x, cy: y, r: rr, fill }, g);
    const rect = (x, y, w, h, fill, rx = 0.3) => el ("rect", { x, y, width: w, height: h, rx, fill }, g);
    switch (shape) {
      case "round": case "mini": case "tt": {
        const k = shape === "mini" ? 0.6 : shape === "tt" ? 0.5 : 1;
        el ("circle", { r: r * k + 0.6, fill: nutFill, filter: "url(#drop)" }, g);
        hole (r * k * 0.78);
        el ("circle", { r: r * k * 0.3, fill: "#2a2a2c", stroke: "#c9a64a", "stroke-width": 0.4 }, g);
        if (style === "headphone") el ("path", { d: `M ${-r * 0.45} ${r * 0.1} A ${r * 0.45} ${r * 0.45} 0 0 1 ${r * 0.45} ${r * 0.1}`, fill: "none", stroke: "#c9cacf", "stroke-width": 0.5 }, g);
        break;
      }
      case "xlrf": case "combo":
        el ("circle", { r: r + 0.6, fill: nutFill, filter: "url(#drop)" }, g); hole (r * 0.78);
        for (const [x, y] of [[-r * 0.3, -r * 0.15], [r * 0.3, -r * 0.15], [0, r * 0.3]]) pin (x, y, r * 0.1);
        rect (-r * 0.12, -r * 0.85, r * 0.24, r * 0.3, "#c9cacf");
        if (shape === "combo") el ("circle", { r: r * 0.24, fill: "#1c1c1e", stroke: "#8f9095", "stroke-width": 0.35 }, g);
        break;
      case "xlrm":
        el ("circle", { r: r + 0.6, fill: nutFill, filter: "url(#drop)" }, g); ring (r * 0.8, "#1c1c1e");
        for (const [x, y] of [[-r * 0.3, -r * 0.15], [r * 0.3, -r * 0.15], [0, r * 0.3]]) pin (x, y, r * 0.14, "#d8b860");
        break;
      case "rca":
        ring (r * 0.9, accent); ring (r * 0.62, "url(#knobAlu)"); hole (r * 0.36); pin (0, 0, r * 0.1, "#d8b860");
        break;
      case "bnc":
        el ("circle", { r: r * 0.8, fill: nutFill, filter: "url(#drop)" }, g);
        pin (-r * 0.82, 0, r * 0.12, "#c9cacf"); pin (r * 0.82, 0, r * 0.12, "#c9cacf");
        ring (r * 0.55, "#e8e2d0"); pin (0, 0, r * 0.12, "#d8b860");
        break;
      case "toslink":
        rect (-r * 0.8, -r * 0.8, r * 1.6, r * 1.6, "#1a1a1c", r * 0.2); rect (-r * 0.5, -r * 0.5, r * 1.0, r * 1.0, "#4a4a4e", r * 0.15);
        break;
      case "din":
        el ("circle", { r: r + 0.6, fill: nutFill, filter: "url(#drop)" }, g); hole (r * 0.8);
        for (let i = 0; i < 5; ++i) { const a = Math.PI + i * Math.PI / 4; pin (Math.cos (a) * r * 0.5, Math.sin (a) * r * 0.5, r * 0.09); }   // five pins round the top half
        rect (-r * 0.1, r * 0.55, r * 0.2, r * 0.25, "#c9cacf");
        break;
      case "banana": {
        const pts = []; for (let i = 0; i < 6; ++i) { const a = Math.PI / 3 * i; pts.push ((Math.cos (a) * r * 0.85).toFixed (2) + "," + (Math.sin (a) * r * 0.85).toFixed (2)); }
        el ("polygon", { points: pts.join (" "), fill: accent, filter: "url(#drop)" }, g); ring (r * 0.45, "url(#knobAlu)"); hole (r * 0.2);
        break;
      }
      case "speakon":
        ring (r + 0.6, accent); ring (r * 0.82, "#0b0b0d"); ring (r * 0.42, "#26262a");
        rect (-r * 0.9, -r * 0.12, r * 0.25, r * 0.24, "#0b0b0d", 0.1); rect (r * 0.65, -r * 0.12, r * 0.25, r * 0.24, "#0b0b0d", 0.1);
        break;
      case "iec":
        el ("path", { d: `M ${-r} ${-r * 0.7} L ${r} ${-r * 0.7} L ${r} ${r * 0.3} L ${r * 0.55} ${r * 0.75} L ${-r * 0.55} ${r * 0.75} L ${-r} ${r * 0.3} Z`, fill: "#161618", filter: "url(#drop)" }, g);
        for (const [x, y] of [[-r * 0.45, -r * 0.1], [r * 0.45, -r * 0.1], [0, r * 0.35]]) rect (x - r * 0.07, y - r * 0.18, r * 0.14, r * 0.36, "#c9cacf", 0.05);
        break;
      case "dc": ring (r * 0.7, "url(#knobAlu)"); hole (r * 0.4); pin (0, 0, r * 0.1, "#c9cacf"); break;
      case "usba": rect (-r * 0.9, -r * 0.35, r * 1.8, r * 0.7, "#c9cacf", 0.2); rect (-r * 0.8, -r * 0.27, r * 1.6, r * 0.54, "#0b0b0d", 0.1); rect (-r * 0.7, -r * 0.27, r * 1.4, r * 0.22, "#f0f0f0", 0.05); break;
      case "usbb": el ("path", { d: `M ${-r * 0.6} ${r * 0.6} L ${-r * 0.6} ${-r * 0.3} L ${-r * 0.35} ${-r * 0.6} L ${r * 0.35} ${-r * 0.6} L ${r * 0.6} ${-r * 0.3} L ${r * 0.6} ${r * 0.6} Z`, fill: "#c9cacf" }, g);
        rect (-r * 0.45, -r * 0.4, r * 0.9, r * 0.9, "#0b0b0d", 0.1); rect (-r * 0.25, -r * 0.2, r * 0.5, r * 0.5, "#f0f0f0", 0.05); break;
      case "usbc": rect (-r * 0.8, -r * 0.3, r * 1.6, r * 0.6, "#c9cacf", r * 0.3); rect (-r * 0.68, -r * 0.2, r * 1.36, r * 0.4, "#0b0b0d", r * 0.2); rect (-r * 0.45, -r * 0.05, r * 0.9, r * 0.1, "#3a3a3e", 0.05); break;
      case "rj45": rect (-r * 0.8, -r * 0.7, r * 1.6, r * 1.3, "#c9cacf", 0.2); rect (-r * 0.65, -r * 0.55, r * 1.3, r * 1.0, "#0b0b0d", 0.1); rect (-r * 0.25, r * 0.45, r * 0.5, r * 0.2, "#0b0b0d", 0.05);
        for (let i = 0; i < 8; ++i) rect (-r * 0.5 + i * r * 0.13, -r * 0.5, r * 0.06, r * 0.25, "#d8b860", 0.02); break;
      case "dsub": el ("path", { d: `M ${-r * 1.6} ${-r * 0.5} L ${r * 1.6} ${-r * 0.5} L ${r * 1.4} ${r * 0.5} L ${-r * 1.4} ${r * 0.5} Z`, fill: "#c9cacf", filter: "url(#drop)" }, g);
        el ("path", { d: `M ${-r * 1.45} ${-r * 0.38} L ${r * 1.45} ${-r * 0.38} L ${r * 1.28} ${r * 0.38} L ${-r * 1.28} ${r * 0.38} Z`, fill: "#161618" }, g);
        for (let i = 0; i < 13; ++i) pin (-r * 1.2 + i * r * 0.2, -r * 0.14, r * 0.05);
        for (let i = 0; i < 12; ++i) pin (-r * 1.1 + i * r * 0.2, r * 0.16, r * 0.05);
        for (const x of [-r * 1.95, r * 1.95]) screw (g, x, 0, "hex", r * 0.22); break;
    }
    if (plugged) {
      // Its plug, and the cable out of the back of it, hanging down and away
      const round = ["round", "mini", "tt", "xlrf", "xlrm", "combo", "rca", "bnc", "din", "banana", "speakon", "dc"].includes (shape);
      const pr = r * (shape === "mini" || shape === "dc" ? 0.6 : shape === "tt" ? 0.5 : 0.95);
      const cw = Math.max (0.8, pr * 0.55);
      el ("path", { d: `M 0 0 C 0 ${r * 3} ${r * 1.2} ${r * 4} ${r * 2.2} ${r * 7}`, fill: "none", stroke: cable || "#0d0d0f", "stroke-width": cw, "stroke-linecap": "round", filter: "url(#drop)" }, g);
      if (round) {
        el ("circle", { r: pr, fill: shape === "xlrf" || shape === "combo" || shape === "speakon" ? "url(#knobAlu)" : "#1b1b1e", filter: "url(#drop)" }, g);
        el ("circle", { r: pr * 0.72, fill: "#111113" }, g);
        for (let i = 0; i < 16; ++i) { const a = i / 16 * Math.PI * 2; el ("line", { x1: Math.cos (a) * pr * 0.74, y1: Math.sin (a) * pr * 0.74, x2: Math.cos (a) * pr * 0.95, y2: Math.sin (a) * pr * 0.95, stroke: "#000", "stroke-width": pr * 0.06, opacity: 0.6 }, g); }
      } else {
        const w = shape === "dsub" ? r * 3.4 : r * 1.8, h = shape === "dsub" ? r * 1.2 : r * 1.4;
        el ("rect", { x: -w / 2, y: -h / 2, width: w, height: h, rx: r * 0.2, fill: "#1b1b1e", filter: "url(#drop)" }, g);
      }
      el ("circle", { r: pr * 0.25, fill: "url(#sheen)", opacity: 0.6 }, g);
    }
  }

  /** A custom knob, seen from the front: skirt, body, grip, cap, pointer, in its colours and material. */
  function drawCustomKnob (k, ck, r, pointer) {
    const shine = { gloss: 0.6, satin: 0.4, matte: 0.12, metal: 0.75, rubber: 0.08 }[ck.mt] ?? 0.4;
    const top = r * (ck.sh === "cone" ? ck.tp * 0.55 : ck.tp);
    if (ck.sk > 0) { el ("circle", { r: r * (1 + 0.45 * ck.sk), fill: ck.sc }, k); el ("circle", { r: r * (1 + 0.45 * ck.sk), fill: "url(#sheen)", opacity: shine * 0.6 }, k); }
    if (ck.sh === "pointer") {
      el ("circle", { r: r * 0.7, fill: ck.bc }, k);
      el ("rect", { x: -r * 0.2, y: -r * 1.2, width: r * 0.4, height: r * 1.9, rx: r * 0.2, fill: shade (ck.bc, 0.08) }, k);
    } else {
      el ("circle", { r, fill: ck.bc }, k);
      if (Math.abs (top - r) > 0.05) el ("circle", { r: top, fill: shade (ck.bc, ck.mt === "metal" ? 0.18 : 0.06), stroke: shade (ck.bc, -0.4), "stroke-width": 0.25 }, k);
      if (ck.sh === "tophat") el ("circle", { r: r * 1.25, fill: "none", stroke: shade (ck.bc, 0.1), "stroke-width": 0.5 }, k);
    }
    if (ck.gr !== "none" && ck.sh !== "pointer") {
      const n = Math.min (ck.gc, ck.gr === "knurl" ? 120 : 60), from = ck.gr === "flutes" ? top * 0.75 : r * (1 - 0.14 - 0.1 * ck.gd);
      for (let i = 0; i < n; ++i) { const a = i / n * Math.PI * 2;
        el ("line", { x1: Math.cos (a) * from, y1: Math.sin (a) * from, x2: Math.cos (a) * r, y2: Math.sin (a) * r,
                      stroke: shade (ck.bc, -0.6), "stroke-width": r * (ck.gr === "flutes" ? 0.12 : ck.gr === "knurl" ? 0.03 : 0.07) * (0.5 + ck.gd), opacity: 0.8 }, k); }
    }
    if (ck.cap !== "none") { const cr = top * ck.cs; el ("circle", { r: cr, fill: ck.kc }, k); el ("circle", { r: cr, fill: "url(#sheen)", opacity: ck.cap === "dome" ? 0.8 : 0.4 }, k); }
    const pc = POINTER_COLOUR[pointer] || ck.pc, tipR = ck.sh === "pointer" ? r * 1.12 : top * 0.96;
    if (ck.pt === "line") el ("line", { x1: 0, y1: -r * 0.2, x2: 0, y2: -tipR, stroke: pc, "stroke-width": Math.max (0.5, r * 0.08), "stroke-linecap": "round" }, k);
    else if (ck.pt === "dot") el ("circle", { cx: 0, cy: -tipR * 0.72, r: Math.max (0.4, r * 0.1), fill: pc }, k);
    else if (ck.pt === "notch") el ("rect", { x: -r * 0.06, y: -tipR, width: r * 0.12, height: r * 0.32, fill: "#050506" }, k);
    el ("circle", { r: r * 0.95, fill: "url(#sheen)", opacity: shine }, k);
  }

  function knob (g, style, r, deg, pointer = "auto") {
    const k = el ("g", { transform: `rotate(${deg})` }, el ("g", { filter: "url(#drop)" }, g));
    if (typeof style === "string" && /^c[0-7]$/.test (style)) {
      const ck = (design.knobs || [])[Number (style.slice (1))];
      if (ck) { drawCustomKnob (k, ck, r, pointer); return; }
    }
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
    W, U, KNOBS, JACK_TYPES, knobAngle, extent, finishPbr: (k, shine = 50) => { const f = FINISH[k] || FINISH.anodised, t = shine / 100;
      return { roughness: Math.min (1, Math.max (0.02, f[2] * (1.6 - 1.2 * t))), metalness: f[3], clearcoat: Math.min (1, f[4] * 2 * t + Math.max (0, t - 0.6)) }; }, get: () => design, selected: () => selected.slice(),
    subscribe: (f) => { listeners.push (f); f (design, selected); },
    select: (id) => { if (byId (id)) { selected = withGroups ([id]); render(); props(); } },
    printCanvas,
  });

  // ---------------------------------------------------------------------------------------------------
  // Pickers: dropdowns that show each choice as a small picture next to its name
  function thumb (kind, value) {
    const t = document.createElementNS (NS, "svg"); t.setAttribute ("viewBox", "-14 -14 28 28"); t.setAttribute ("width", 34); t.setAttribute ("height", 34);
    t.setAttribute ("aria-hidden", "true");   // (drawn with the stage's gradients: see render)
    const g = el ("g", {}, t), was = design;
    if (kind === "knob") knob (g, value, 9, 30);
    else if (kind.startsWith ("ck:")) {   // the knob maker's choices: the knob being edited, with this one choice
      const base = (design.knobs || [])[ckEditing] || blankKnob(), field = kind.slice (3);
      const kk = el ("g", { transform: "rotate(30)" }, g);
      drawCustomKnob (kk, Object.assign ({}, base, { [field]: value }), 9, "auto");
    }
    else if (kind === "pointer") { knob (g, "matte", 9, 30, value === "auto" ? "white" : value); }
    else if (kind === "toggle") drawPart (g, sanitize ({ parts: [{ type: "toggle", x: 0, y: 0, style: value, on: true, text: "" }] }).parts[0]);
    else if (kind === "button") drawPart (g, sanitize ({ parts: [{ type: "button", x: 0, y: 0, style: value, on: true, text: "", w: 12, h: 9, colour: "#e0a84a" }] }).parts[0]);
    else if (kind === "vu") { const q = sanitize ({ parts: [{ type: "vu", x: 0, y: 0, w: 26, h: 16, style: value, value: 55, text: "" }] }).parts[0]; drawPart (g, q); }
    else if (kind === "jack") drawJack (g, value, 7, false);
    else if (kind === "screws") screw (g, 0, 0, value === "unit" ? design.unit.screws : value, 8);
    else if (kind.startsWith ("part:")) {   // a small sample of a part with this one setting (the newer choices)
      const [, type, field] = kind.split (":"), size = THUMB_PARTS[type] || {};
      const q = sanitize ({ knobs: design.knobs, parts: [Object.assign ({ type, x: 0, y: 0 }, size, { [field]: value })] }).parts[0];
      if (q) { q.x = 0; q.y = 0; drawPart (g, q); }
    }
    else if (kind === "nums") drawPart (g, sanitize ({ parts: [{ type: "knob", x: 0, y: 0, w: 10, h: 10, style: "ribbed", value: 50, text: "", scale: true, nums: value, steps: 10, min: 0, max: 10 }] }).parts[0]);
    else if (kind === "bend") drawPart (g, sanitize ({ parts: [{ type: "label", x: 0, y: value === "circle" ? 0 : 3, w: 24, text: value === "circle" ? "ROUND AND ROUND ·" : "TEXT",
      size: value === "circle" ? 3 : 5, bend: value, curve: 70, radius: 9, start: 0 }] }).parts[0]);
    else if (kind === "trim" || kind === "twoTone") {
      const u = design.unit;
      el ("rect", { x: -13, y: -10, width: 26, height: 20, rx: 2, fill: u.colour }, g);
      if (kind === "twoTone" && value !== "none") { const b = { left: [-13, -10, 9, 20], right: [4, -10, 9, 20], top: [-13, -10, 26, 7], bottom: [-13, 3, 26, 7], band: [-13, -3.5, 26, 7] }[value];
        el ("rect", { x: b[0], y: b[1], width: b[2], height: b[3], fill: u.toneColour }, g); }
      if (kind === "trim" && value !== "none") { el ("rect", { x: -10.5, y: -7.5, width: 21, height: 15, rx: 1, fill: "none", stroke: value === "inset" ? "#000" : u.accent, "stroke-width": 0.8, opacity: value === "inset" ? 0.5 : 1 }, g);
        if (value === "double") el ("rect", { x: -8.5, y: -5.5, width: 17, height: 11, rx: 0.8, fill: "none", stroke: u.accent, "stroke-width": 0.5 }, g); }
    }
    else if (kind === "finish" || kind === "edge" || kind === "ears" || kind === "handles" || kind === "earColour" || kind === "font") {
      const u = design.unit, fin = kind === "finish" ? value : u.finish;
      const rx = kind === "edge" ? (value === "square" ? 0.3 : value === "bevel" ? 2 : 5) : 3;
      const col = kind === "finish" && FINISH[value] && FINISH[value][1] ? FINISH[value][1] : u.colour;
      el ("rect", { x: -13, y: -10, width: 26, height: 20, rx, fill: col }, g);
      drawFinish (g, -13, -10, 26, 20, rx, fin, col, false);
      if (kind === "edge" && value === "bevel") el ("rect", { x: -11, y: -8, width: 22, height: 16, rx: 1, fill: "none", stroke: shade (u.colour, 0.5), "stroke-width": 0.8 }, g);
      if (kind === "ears") { if (value !== "none") for (const x of [-10, 10]) value === "slots" ? el ("rect", { x: x - 2.2, y: -1.2, width: 4.4, height: 2.4, rx: 1.2, fill: "#050506" }, g) : el ("circle", { cx: x, cy: 0, r: 1.6, fill: "#050506" }, g); }
      if (kind === "earColour" && value !== "match") for (const x of [-13, 7]) el ("rect", { x, y: -10, width: 6, height: 20, fill: value === "black" ? "#0d0d0f" : "#c3c4c8" }, g);
      if (kind === "handles" && value !== "none") for (const x of [-7, 7]) value === "bar" ? el ("rect", { x: x - 1.5, y: -8, width: 3, height: 16, rx: 1.5, fill: "url(#metalV)" }, g)
                                                                                           : el ("path", { d: `M ${x} -8 C ${x + 5} -8 ${x + 5} 8 ${x} 8`, fill: "none", stroke: "#c9cacf", "stroke-width": 2 }, g);
      if (kind === "font") { design = Object.assign ({}, design, { unit: Object.assign ({}, u, { font: value }) }); txt (g, 0, 0, "Ab", 10, u.ink, {}); design = was; }
    }
    return t;
  }

  /** The samples the newer choices are shown on (sizes that fit a dropdown's little picture). */
  const THUMB_PARTS = {
    knob: { w: 11, h: 11, text: "", scale: true, nums: "none", value: 70 }, led: { w: 9, h: 9, on: true }, ladder: { w: 5, h: 22, value: 70, segments: 8 },
    display: { w: 26, h: 18, text: "", colour: "#56c8f5", content: "12.4" }, label: { w: 24, h: 8, text: "Ab", size: 9 },
    vent: { w: 24, h: 18, count: 5 }, slider: { w: 8, h: 24, value: 60, text: "", scale: false }, lamp: { w: 12, h: 12, text: "" },
    plate: { w: 26, h: 12, text: "ENH" }, toggle: { text: "" }, box: { w: 24, h: 18, text: "", fill: true }, vu: { w: 26, h: 16, text: "", value: 55 },
  };

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
    if (design.parts.length + selected.length > MAX_PARTS) return;
    const regroup = groupMap();
    const copies = selected.map (byId).filter (Boolean).map ((p) => Object.assign ({}, p, { id: nextId++, x: Math.min (W, p.x + 8), y: Math.min (design.unit.height * U, p.y + 4) }, p.grp ? { grp: regroup (p.grp) } : {}));
    design.parts.push (...copies); selected = copies.map ((p) => p.id); commit();
  }
  function align (how) {
    const ps = selected.map (byId).filter (Boolean); if (ps.length < 2) return;
    const xs = ps.map ((p) => p.x), ys = ps.map ((p) => p.y);
    const lo = (a) => Math.min (...a), hi = (a) => Math.max (...a);
    const ew = (q) => extent (q).w, eh = (q) => extent (q).h;
    if (how === "left") ps.forEach ((p) => (p.x = lo (ps.map ((q) => q.x - ew (q) / 2)) + ew (p) / 2));
    if (how === "right") ps.forEach ((p) => (p.x = hi (ps.map ((q) => q.x + ew (q) / 2)) - ew (p) / 2));
    if (how === "hcenter") { const c = (lo (xs) + hi (xs)) / 2; ps.forEach ((p) => (p.x = c)); }
    if (how === "top") ps.forEach ((p) => (p.y = lo (ps.map ((q) => q.y - eh (q) / 2)) + eh (p) / 2));
    if (how === "bottom") ps.forEach ((p) => (p.y = hi (ps.map ((q) => q.y + eh (q) / 2)) - eh (p) / 2));
    if (how === "vcenter") { const c = (lo (ys) + hi (ys)) / 2; ps.forEach ((p) => (p.y = c)); }
    if (how === "hdist" && ps.length > 2) { ps.sort ((a, b) => a.x - b.x); const a = ps[0].x, b = ps[ps.length - 1].x; ps.forEach ((p, i) => (p.x = a + (b - a) * i / (ps.length - 1))); }
    if (how === "vdist" && ps.length > 2) { ps.sort ((a, b) => a.y - b.y); const a = ps[0].y, b = ps[ps.length - 1].y; ps.forEach ((p, i) => (p.y = a + (b - a) * i / (ps.length - 1))); }
    commit();
  }
  function order (dir) {
    const ids = new Set (selected); const moving = design.parts.filter ((p) => ids.has (p.id)), rest = design.parts.filter ((p) => !ids.has (p.id));
    design.parts = dir > 0 ? rest.concat (moving) : moving.concat (rest); commit();
  }

  // Groups: parts that select and move together (a knob and the text wrapped round it, a row of switches)
  const newGroup = () => 1 + Math.max (0, ...design.parts.map ((p) => p.grp || 0));
  const groupMap = () => { const m = new Map(); let n = newGroup(); return (g) => { if (!m.has (g)) m.set (g, n++); return m.get (g); }; };
  const withGroups = (ids) => { const gs = new Set (ids.map (byId).filter ((p) => p && p.grp).map ((p) => p.grp));
    return [...new Set (ids.concat (design.parts.filter ((p) => p.grp && gs.has (p.grp)).map ((p) => p.id)))]; };
  function group () { if (selected.length < 2) return; const g = newGroup(); selected.map (byId).forEach ((p) => p && (p.grp = g)); commit(); }
  function ungroup () { selected.map (byId).forEach ((p) => p && delete p.grp); commit(); }
  function mirror () {   // left for right across the panel's middle
    for (const p of selected.map (byId)) if (p && !p.lock) { p.x = W - p.x; if (p.rot) p.rot = -p.rot; if (p.type === "label" && p.bend === "circle") p.start = -p.start; }
    commit();
  }
  function matchSize () {   // everything the size of the first picked
    const ps = selected.map (byId).filter (Boolean); if (ps.length < 2) return;
    for (const p of ps.slice (1)) { if (p.type === ps[0].type || (p.type !== "led" && p.type !== "screw")) { p.w = ps[0].w; p.h = p.type === "knob" ? ps[0].w : ps[0].h; } }
    commit();
  }
  function setLock (on) { selected.map (byId).forEach ((p) => p && (p.lock = on)); commit(); }
  /** Text round a part: centred on it, just outside it (its printed scale too), and grouped with it. */
  function wrapAround (l, q) {
    const r = Math.min (q.w, q.h) / 2;
    const R = q.type === "knob" ? r * (q.scale ? 1.62 + 0.3 : 1.2) + 1.5 : Math.max (q.w, q.h) / 2 + 2;
    Object.assign (l, { bend: "circle", x: q.x, y: q.y, radius: Math.round ((R + l.size * 0.55) * 10) / 10, start: l.flip ? 180 : 0 });
    const g = q.grp || l.grp || newGroup(); q.grp = g; l.grp = g;
    commit();
  }

  // Copy, cut and paste: the parts go to the clipboard as a small tagged text, so they paste into another
  // design (another tab) too. What comes back in is sanitized like any share code.
  const CLIP = "ENHPARTS1.";
  let clip = "", pasteN = 0;
  function copyParts () {
    const ps = selected.map (byId).filter (Boolean); if (!ps.length) return "";
    clip = CLIP + JSON.stringify (pack ({ unit: design.unit, parts: ps }).p); pasteN = 1;
    return clip;
  }
  function pasteParts (s) {
    if (!s || !s.startsWith (CLIP) || s.length > MAX_JSON) return false;
    let arr; try { arr = JSON.parse (s.slice (CLIP.length)); } catch (_) { return false; }
    if (!Array.isArray (arr)) return false;
    const room = MAX_PARTS - design.parts.length; if (room <= 0) return true;
    const ps = sanitize (Object.assign (unpack ({ u: design.unit, p: arr.slice (0, room) }), { knobs: design.knobs })).parts, H = design.unit.height * U, off = 6 * pasteN++;
    const regroup = groupMap();
    for (const p of ps) { p.x = clamp (p.x + off, 0, W, p.x); p.y = clamp (p.y + off, 0, H, p.y); if (p.grp) p.grp = regroup (p.grp); }
    design.parts.push (...ps); selected = ps.map ((p) => p.id); commit();
    return true;
  }
  const typingElsewhere = (e) => (e.target && e.target.closest && e.target.closest ("input, textarea, select, dialog")) || String (window.getSelection() || "").length > 0;
  document.addEventListener ("copy", (e) => { if (typingElsewhere (e) || !selected.length) return; e.preventDefault(); e.clipboardData.setData ("text/plain", copyParts()); });
  document.addEventListener ("cut", (e) => { if (typingElsewhere (e) || !selected.length) return; e.preventDefault(); e.clipboardData.setData ("text/plain", copyParts()); pasteN = 0; removeSelected(); });
  document.addEventListener ("paste", (e) => {
    if (typingElsewhere (e)) return;
    const s = (e.clipboardData && e.clipboardData.getData ("text/plain")) || "";
    if (pasteParts (s.startsWith (CLIP) ? s : clip)) e.preventDefault();
  });

  // Pointer: select, drag, and in Play mode turn knobs / flip switches
  let drag = null;
  function svgPoint (e) { const pt = svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY; return pt.matrixTransform (svg.getScreenCTM().inverse()); }
  svg.addEventListener ("pointerdown", (e) => {
    const g = e.target.closest (".d-part"); const pt = svgPoint (e);
    if (!g) {   // empty space: drag a box to select what it touches (Shift adds to what is selected)
      if (play) return;
      try { svg.setPointerCapture (e.pointerId); } catch (_) { /* a synthetic pointer: no capture needed */ }
      drag = { mode: "box", start: pt, cur: null, base: e.shiftKey ? selected.slice() : [] };
      if (!e.shiftKey) { selected = []; render(); props(); }
      return;
    }
    const p = byId (Number (g.getAttribute ("data-id"))); if (!p) return;
    e.preventDefault(); try { svg.setPointerCapture (e.pointerId); } catch (_) { /* a synthetic pointer: no capture needed */ }
    if (play) {
      if (p.type === "toggle" && p.three) {   // on -> middle -> off -> on
        if (p.mid) { p.mid = false; p.on = false; } else if (p.on) p.mid = true; else p.on = true;
        render(); save(); return;
      }
      if (p.type === "button" && p.momentary) { p.on = true; drag = { mode: "hold", p }; render(); return; }
      if (p.type === "toggle" || p.type === "button" || p.type === "led" || p.type === "lamp") { p.on = !p.on; render(); save(); return; }
      if (p.type === "selector") { p.value = (Math.round (p.value) + 1) % stopList (p.stops).length; render(); save(); return; }
      if ("value" in p) drag = { mode: "turn", p, y0: e.clientY, x0: e.clientX, v0: p.value };
      return;
    }
    // a part in a group picks the whole group (Alt: just that part)
    const ids = e.altKey ? [p.id] : withGroups ([p.id]);
    if (e.shiftKey) selected = selected.includes (p.id) ? selected.filter ((i) => !ids.includes (i)) : [...new Set (selected.concat (ids))];
    else if (!selected.includes (p.id)) selected = ids;
    drag = { mode: "move", start: pt, moved: false, orig: selected.map (byId).filter ((q) => q && !q.lock).map ((q) => ({ q, x: q.x, y: q.y })) };
    render(); props();
  });
  svg.addEventListener ("pointermove", (e) => {
    if (!drag) return;
    if (drag.mode === "hold") return;
    if (drag.mode === "turn") {
      const q = drag.p, along = q.type === "slider" && q.horizontal ? e.clientX - drag.x0 : drag.y0 - e.clientY;
      let v = clamp (drag.v0 + along * (q.type === "slider" ? 100 / Math.max (10, (q.horizontal ? q.w : q.h) * 2 * zoom) : 0.6), 0, 100, 0);
      if (q.detent && q.steps) v = Math.round (v / 100 * q.steps) * 100 / q.steps;   // clicks in steps
      q.value = v; render(); return;
    }
    if (drag.mode === "box") {
      drag.cur = svgPoint (e); const r = boxRect (drag.start, drag.cur);
      const hit = design.parts.filter ((p) => { const b = bounds (p); return b.x < r.x + r.w && b.x + b.w > r.x && b.y < r.y + r.h && b.y + b.h > r.y; }).map ((p) => p.id);
      selected = [...new Set (drag.base.concat (withGroups (hit)))];
      render(); return;
    }
    const pt = svgPoint (e), dx = pt.x - drag.start.x, dy = pt.y - drag.start.y, H = design.unit.height * U;
    if (Math.abs (dx) + Math.abs (dy) > 0.2) drag.moved = true;
    for (const o of drag.orig) { o.q.x = clamp (snapV (o.x + dx), 0, W, o.x); o.q.y = clamp (snapV (o.y + dy), 0, H, o.y); }
    render();
  });
  const endDrag = () => { if (!drag) return; const d = drag; drag = null;
    if (d.mode === "hold") { d.p.on = false; render(); return; }
    if (d.mode === "box") { render(); props(); } else if (d.mode === "turn" || d.moved) commit(); };
  svg.addEventListener ("pointerup", endDrag); svg.addEventListener ("pointercancel", endDrag);
  svg.addEventListener ("wheel", (e) => {
    if (!play) return; const g = e.target.closest (".d-part"); const p = g && byId (Number (g.getAttribute ("data-id")));
    if (p && "value" in p) { e.preventDefault(); p.value = clamp (p.value - Math.sign (e.deltaY) * 3, 0, 100, 0); render(); save(); }
  }, { passive: false });

  document.addEventListener ("keydown", (e) => {
    if (e.target.closest && e.target.closest ("input, textarea, select, dialog, [role=tab], .d-pick")) return;
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key.toLowerCase() === "z") { e.preventDefault(); e.shiftKey ? redo() : undo(); return; }
    if (mod && e.key.toLowerCase() === "y") { e.preventDefault(); redo(); return; }
    if (mod && e.key.toLowerCase() === "d") { e.preventDefault(); duplicate(); return; }
    if (mod && e.key.toLowerCase() === "a") { e.preventDefault(); selected = design.parts.map ((p) => p.id); render(); props(); return; }
    if (mod && e.key.toLowerCase() === "g") { e.preventDefault(); e.shiftKey ? ungroup() : group(); return; }
    if (e.key === "Escape" && selected.length) { selected = []; render(); props(); return; }
    if (e.key === "Delete" || e.key === "Backspace") { if (selected.length) { e.preventDefault(); removeSelected(); } return; }
    if (e.key.toLowerCase() === "p" && !mod) { setPlay (!play); return; }
    const step = e.shiftKey ? 5 : 0.5, H = design.unit.height * U;
    const mv = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] }[e.key];
    if (mv && selected.length) { e.preventDefault(); for (const id of selected) { const p = byId (id); if (p.lock) continue; p.x = clamp (p.x + mv[0], 0, W, p.x); p.y = clamp (p.y + mv[1], 0, H, p.y); } commit(); }
  });

  function undo () { if (!history.length) return; future.push (JSON.stringify (design)); design = JSON.parse (history.pop()); last = JSON.stringify (design); selected = []; save(); render(); props(); syncUnit(); }
  function redo () { if (!future.length) return; history.push (JSON.stringify (design)); design = JSON.parse (future.pop()); last = JSON.stringify (design); selected = []; save(); render(); props(); syncUnit(); }
  let blinkTimer = 0;
  function setPlay (on) { play = on;
    clearInterval (blinkTimer); if (on) blinkTimer = setInterval (() => { if (design.parts.some ((q) => q.blink && q.on)) render(); }, 250); const b = document.getElementById ("play"); b.setAttribute ("aria-pressed", String (on)); b.textContent = on ? "Edit" : "Play"; document.body.classList.toggle ("playing", on); render(); }

  // ---------------------------------------------------------------------------------------------------
  // Panels
  const $ = (id) => document.getElementById (id);
  const unitPickers = {};
  function syncUnit () {
    const u = design.unit;
    $("u-name").value = u.name; $("u-model").value = u.model; $("u-height").value = String (u.height);
    $("u-colour").value = u.colour; $("u-ink").value = u.ink; $("u-wear").value = String (u.wear); $("u-grid").checked = snap;
    $("u-sub").value = u.sub; $("u-badge").value = u.badge; $("u-shine").value = String (u.shine); $("u-desc").value = u.desc;
    $("u-accent").value = u.accent; $("u-toneColour").value = u.toneColour; $("u-toneSize").value = String (u.toneSize);
    $("u-titlePos").value = u.titlePos; $("u-titleSize").value = String (u.titleSize); $("u-glow").checked = u.glow; $("u-serial").value = u.serial;
    $("u-screwMetal").value = u.screwMetal; $("u-chassis").value = u.chassis; $("u-depth").value = String (u.depth);
    $("tone-row").hidden = u.twoTone === "none";
    showAbout();
    showKnobMaker();
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
    on ("u-shine", (e) => (design.unit.shine = Math.round (clamp (e.value, 0, 100, 50)))); done ("u-shine");
    $("u-desc").addEventListener ("input", () => { design.unit.desc = textBlock ($("u-desc").value, 600); save(); showAbout(); });
    done ("u-desc");
    const unitPick = (key, list, names) => {
      const w = picker (key, list, names, design.unit[key], (v) => {
        design.unit[key] = pick (v, list, design.unit[key]);
        // a metal, a wood, a plastic comes in its own colour (change it after if you like)
        if (key === "finish" && FINISH[v][1]) { design.unit.colour = FINISH[v][1]; $("u-colour").value = design.unit.colour; }
        commit(); refreshPickers(); if (key === "finish") rebuildUnitPickers(); if (key === "twoTone") $("tone-row").hidden = design.unit.twoTone === "none"; }, key);
      $("u-" + key).replaceChildren (w); unitPickers[key] = w;
    };
    unitPick ("finish", FINISHES, FINISH_NAMES);
    unitPick ("edge", EDGES, { square: "Square", rounded: "Rounded", bevel: "Bevelled" });
    unitPick ("font", FONTS, { sans: "Sans", serif: "Serif", mono: "Mono", condensed: "Condensed" });
    unitPick ("ears", EARS, { slots: "Slotted", holes: "Round holes", none: "None (500 module)" });
    unitPick ("earColour", EARCOLS, { match: "As the panel", black: "Black", silver: "Silver" });
    unitPick ("handles", HANDLES, { none: "None", bar: "Bar handles", loop: "Loop handles" });
    unitPick ("screws", SCREWS, { phillips: "Phillips", hex: "Hex", thumb: "Thumb screws" });
    unitPick ("trim", TRIMS, { none: "None", pinstripe: "Pinstripe (accent)", double: "Double pinstripe", inset: "Pressed-in line" });
    unitPick ("twoTone", TWO_TONES, { none: "One colour", left: "Left side", right: "Right side", top: "Top", bottom: "Bottom", band: "A band across" });
    rebuildUnitPickers = () => { unitPick ("finish", FINISHES, FINISH_NAMES);
      unitPick ("edge", EDGES, { square: "Square", rounded: "Rounded", bevel: "Bevelled" }); unitPick ("earColour", EARCOLS, { match: "As the panel", black: "Black", silver: "Silver" }); };
    on ("u-colour", (e) => (design.unit.colour = colour (e.value, design.unit.colour))); done ("u-colour");
    $("u-colour").addEventListener ("change", rebuildUnitPickers);
    for (const id of ["u-accent", "u-toneColour"]) $(id).addEventListener ("change", () => { unitPick ("trim", TRIMS, { none: "None", pinstripe: "Pinstripe (accent)", double: "Double pinstripe", inset: "Pressed-in line" });
      unitPick ("twoTone", TWO_TONES, { none: "One colour", left: "Left side", right: "Right side", top: "Top", bottom: "Bottom", band: "A band across" }); });
    on ("u-ink", (e) => (design.unit.ink = colour (e.value, design.unit.ink))); done ("u-ink");
    on ("u-wear", (e) => (design.unit.wear = Number (e.value))); done ("u-wear");
    // Paint and trim, lettering, hardware (every value checked as the sanitizer would)
    on ("u-accent", (e) => (design.unit.accent = colour (e.value, design.unit.accent))); done ("u-accent");
    on ("u-toneColour", (e) => (design.unit.toneColour = colour (e.value, design.unit.toneColour))); done ("u-toneColour");
    on ("u-toneSize", (e) => (design.unit.toneSize = Math.round (clamp (e.value, 10, 90, 30)))); done ("u-toneSize");
    on ("u-titlePos", (e) => (design.unit.titlePos = pick (e.value, TITLE_POS, "topleft")), "change"); done ("u-titlePos");
    on ("u-titleSize", (e) => (design.unit.titleSize = clamp (e.value, 3, 9, 4.4))); done ("u-titleSize");
    on ("u-glow", (e) => (design.unit.glow = e.checked), "change"); done ("u-glow");
    on ("u-serial", (e) => (design.unit.serial = text (e.value, 16, ""))); done ("u-serial");
    on ("u-screwMetal", (e) => (design.unit.screwMetal = pick (e.value, SCREW_METALS, "chrome")), "change"); done ("u-screwMetal");
    on ("u-chassis", (e) => (design.unit.chassis = colour (e.value, design.unit.chassis))); done ("u-chassis");
    on ("u-depth", (e) => (design.unit.depth = Math.round (clamp (e.value, 60, 400, 180)))); done ("u-depth");
    $("u-grid").addEventListener ("change", (e) => (snap = e.target.checked));
  }

  // The description under the stage: what the unit does and how it works (text only, never markup)
  function showAbout () {
    const box = $("about"), d = design.unit.desc;
    box.hidden = !d;
    $("about-text").textContent = d;
    $("about-title").textContent = "About " + (design.unit.name || "this unit");
  }

  // The pictures follow the panel's colour and finish: redraw them when those change
  let rebuildUnitPickers = () => {};
  function refreshPickers () { for (const k in unitPickers) unitPickers[k].setValue (design.unit[k]); }

  function field (parent, label, input) { const l = document.createElement ("label"); l.textContent = label + " "; l.appendChild (input); parent.appendChild (l); return input; }
  const partName = (q) => TYPES[q.type].label + (q.text ? " “" + q.text + "”" : "");
  /** The selected part's settings - or, with several selected, the settings they share: a change goes to
      all of them (a field they differ on shows "mixed" until it is set). */
  function props () {
    const body = $("props-body"); body.replaceChildren();
    const ps = selected.map (byId).filter (Boolean);
    $("props-empty").hidden = ps.length > 0;
    $("align").hidden = ps.length < 2;
    if (!ps.length) return;
    const p = ps[0], many = ps.length > 1, sameType = ps.every ((q) => q.type === p.type);
    const has = (key) => ps.every ((q) => key in q);
    const same = (key) => ps.every ((q) => q[key] === p[key]);
    const setAll = (key, v) => { for (const q of ps) if (key in q || key === "lock") q[key] = v; };
    const h = document.createElement ("p"); h.className = "d-kind";
    h.textContent = many ? ps.length + " parts" + (sameType ? " (" + TYPES[p.type].label.toLowerCase() + ")" : "") + (ps.every ((q) => q.grp && q.grp === p.grp) ? " · grouped" : "") : TYPES[p.type].label;
    body.appendChild (h);
    const mixed = (i, key) => { if (!same (key)) { i.value = ""; i.placeholder = "mixed"; } };
    const num = (key, label, lo, hi, step = 0.5, after) => { const i = document.createElement ("input"); i.type = "number"; i.min = lo; i.max = hi; i.step = step; i.value = Math.round (p[key] * 10) / 10; mixed (i, key);
      i.addEventListener ("input", () => { if (i.value === "") return; const v = clamp (i.value, lo, hi, p[key]); for (const q of ps) { q[key] = v; if (["knob", "selector", "led", "lamp", "screw"].includes (q.type) && key === "w") q.h = v; } if (after) after(); render(); save(); });
      i.addEventListener ("change", commit); field (body, label, i); };
    const str = (key, label, max) => { const i = document.createElement ("input"); i.maxLength = max; i.value = p[key]; i.autocomplete = "off"; mixed (i, key);
      i.addEventListener ("input", () => { setAll (key, text (i.value, max, "")); render(); save(); }); i.addEventListener ("change", commit); field (body, label, i); };
    const chk = (key, label) => { const i = document.createElement ("input"); i.type = "checkbox"; i.checked = !!p[key]; i.indeterminate = !same (key);
      i.addEventListener ("change", () => { setAll (key, i.checked); commit(); });
      const l = document.createElement ("label"); l.className = "row"; l.appendChild (i); l.appendChild (document.createTextNode (" " + label)); body.appendChild (l); };
    const col = (key, label) => { const i = document.createElement ("input"); i.type = "color"; i.value = p[key]; i.addEventListener ("input", () => { setAll (key, colour (i.value, p[key])); render(); save(); }); i.addEventListener ("change", commit);
      const l = field (body, label, i); l.parentElement.classList.add ("row"); };
    const choice = (key, label, list, names) => { const sel = document.createElement ("select"); for (const v of list) { const o = document.createElement ("option"); o.value = v; o.textContent = names ? names[v] : v[0].toUpperCase() + v.slice (1); sel.appendChild (o); }
      sel.value = p[key]; sel.addEventListener ("change", () => { setAll (key, pick (sel.value, list, p[key])); commit(); }); field (body, label, sel); };
    const pick2 = (key, label, kind, list, names) => { const l = document.createElement ("div"); l.className = "d-field"; const t = document.createElement ("span"); t.textContent = label + (same (key) ? "" : " (mixed)"); l.appendChild (t);
      l.appendChild (picker (kind, list, names, p[key], (v) => { setAll (key, pick (v, list, p[key])); commit(); }, label)); body.appendChild (l); };
    const btns = (list) => { const b = document.createElement ("div"); b.className = "d-btns";
      for (const [t, f, cls] of list) { const x = document.createElement ("button"); x.type = "button"; x.textContent = t; if (cls) x.className = cls; x.addEventListener ("click", f); b.appendChild (x); }
      body.appendChild (b); };

    // Where: one part's centre, or the whole selection moved as one
    if (many) {
      const cx = (Math.min (...ps.map ((q) => bounds (q).x)) + Math.max (...ps.map ((q) => bounds (q).x + bounds (q).w))) / 2;
      const cy = (Math.min (...ps.map ((q) => bounds (q).y)) + Math.max (...ps.map ((q) => bounds (q).y + bounds (q).h))) / 2;
      const moveAll = (label, c, axis, hi) => { const i = document.createElement ("input"); i.type = "number"; i.step = 0.5; i.value = Math.round (c * 10) / 10; let from = c;
        i.addEventListener ("input", () => { if (i.value === "") return; const d = Number (i.value) - from; from = Number (i.value);
          for (const q of ps) if (!q.lock) q[axis] = clamp (q[axis] + d, 0, hi, q[axis]); render(); save(); });
        i.addEventListener ("change", commit); field (body, label, i); };
      moveAll ("Across, middle (mm)", cx, "x", W); moveAll ("Down, middle (mm)", cy, "y", design.unit.height * U);
    } else { num ("x", "Across (mm)", 0, W); num ("y", "Down (mm)", 0, design.unit.height * U); }

    if (sameType) {
      if (p.type === "knob" || p.type === "selector") num ("w", "Size (mm)", 8, 60);
      else if (!["led", "screw", "lamp"].includes (p.type)) { if (!(p.type === "label" && p.bend === "circle")) num ("w", "Width (mm)", 1, W); if (p.type !== "label") num ("h", "Height (mm)", 0.5, 4 * U); }
      else num ("w", "Size (mm)", 2, 14);
    }
    if (ps.every ((q) => ["label", "box", "line", "display", "vent", "jack"].includes (q.type))) num ("rot", "Rotation (°)", -180, 180, 1);
    if (sameType && p.type === "knob") {
      const customIds = (design.knobs || []).map ((_, i) => "c" + i);
      pick2 ("style", "Knob", "knob", KNOBS.concat (customIds), Object.assign ({}, KNOB_NAMES, Object.fromEntries (customIds.map ((c, i) => [c, "Yours: " + design.knobs[i].n])))); pick2 ("pointer", "Pointer", "pointer", POINTERS, { auto: "As the knob comes" });
      num ("sweep", "Turns through (°)", 180, 330, 5);
      chk ("scale", "Printed scale");
      if (ps.some ((q) => q.scale)) {
        pick2 ("nums", "Scale numbers", "nums", NUMS, { ends: "Ends and middle", all: "At every step", none: "Ticks only" });
        num ("steps", "Steps", 2, 20, 1); num ("min", "Scale from", -99, 999, 1); num ("max", "Scale to", -99, 999, 1);
        chk ("lean", "Numbers turn with the dial");
      }
      chk ("arcText", "Label curves under the knob");
    }
    if (sameType && p.type === "toggle") pick2 ("style", "Switch", "toggle", TOGGLES, { bat: "Bat handle", rocker: "Rocker", rockerred: "Red rocker" });
    if (sameType && p.type === "button") pick2 ("style", "Button", "button", BUTTONS);
    if (sameType && p.type === "vu") pick2 ("style", "Dial", "vu", VUS);
    if (sameType && p.type === "jack") { pick2 ("style", "Socket", "jack", JACKS, JACK_NAMES); chk ("plugged", "Plugged in (a cable in it)"); }
    if (has ("bend")) {
      pick2 ("bend", "Text shape", "bend", BENDS, { none: "Straight", curve: "Curved", circle: "Round a circle" });
      if (ps.some ((q) => q.bend === "curve")) num ("curve", "Curve (− dips, + arches)", -100, 100, 1);
      if (ps.some ((q) => q.bend === "circle")) { num ("radius", "Circle radius (mm)", 2, 240, 0.5); num ("start", "Centred at (°, 0 = top)", -180, 180, 1); chk ("flip", "Read from inside (along the bottom)"); }
      if (!many) {   // wrap round another part
        const others = design.parts.filter ((q) => q.id !== p.id && q.type !== "label");
        if (others.length) {
          const sel = document.createElement ("select"); const o0 = document.createElement ("option"); o0.value = ""; o0.textContent = "Choose a part…"; sel.appendChild (o0);
          for (const q of others) { const o = document.createElement ("option"); o.value = String (q.id); o.textContent = partName (q); sel.appendChild (o); }
          sel.addEventListener ("change", () => { const q = byId (Number (sel.value)); if (q) wrapAround (p, q); });
          field (body, "Wrap round a part", sel);
        }
      }
    }
    if (many && ps.length === 2) {   // a text and one other part: wrap the one round the other
      const l = ps.find ((q) => q.type === "label"), q = ps.find ((x) => x.type !== "label");
      if (l && q) btns ([["Wrap the text round the " + TYPES[q.type].label.toLowerCase(), () => wrapAround (l, q), "primary"]]);
    }
    // The newer settings, each for the parts that have it
    const ask = (key, label, list, names) => { if (has (key)) pick2 (key, label, "part:" + p.type + ":" + key, list, names); };
    if (sameType && p.type === "knob") {
      if (ps.some ((q) => q.scale)) ask ("marks", "Scale marks", MARKS, { ticks: "Ticks", dots: "Dots", arc: "An arc" });
      chk ("bipolar", "Centre-zero (bipolar)");
      chk ("ring", "LED ring round it"); if (ps.some ((q) => q.ring)) col ("ringColour", "Ring colour");
      str ("suffix", "Unit after the numbers (dB, Hz…)", 6);
      choice ("labelPos", "Label", LABEL_POS, { below: "Under it", above: "Over it", none: "No label" }); num ("labelSize", "Label size", 1.5, 6, 0.1);
      chk ("detent", "Clicks in steps (Play)");
    }
    if (sameType && p.type === "selector") {
      pick2 ("style", "Knob", "knob", KNOBS.concat ((design.knobs || []).map ((_, i) => "c" + i)), Object.assign ({}, KNOB_NAMES, Object.fromEntries ((design.knobs || []).map ((k, i) => ["c" + i, "Yours: " + k.n]))));
      pick2 ("pointer", "Pointer", "pointer", POINTERS, { auto: "As the knob comes" });
      str ("stops", "Positions (names, | between; 2 - 12)", 130);
      if (!many) num ("value", "Set to position", 0, stopList (p.stops).length - 1, 1);
      num ("sweep", "Turns through (°)", 180, 330, 5);
    }
    if (sameType && p.type === "slider") {
      ask ("style", "Cap", FADERS, { black: "Black", silver: "Silver", white: "White", red: "Red" });
      chk ("horizontal", "Horizontal"); chk ("scale", "Printed scale"); if (ps.some ((q) => q.scale)) num ("steps", "Steps", 2, 20, 1);
    }
    if (sameType && p.type === "toggle") {
      chk ("three", "Three positions (on - off - on)"); if (ps.some ((q) => q.three)) chk ("mid", "Set in the middle");
      str ("upText", "Word over it", 8); str ("downText", "Word under it", 8);
    }
    if (sameType && p.type === "button") { chk ("led", "LED over it"); chk ("momentary", "Momentary (lit only while held, in Play)"); str ("capText", "Text on the cap", 6); }
    if (sameType && p.type === "led") { ask ("shape", "Shape", LED_SHAPES); choice ("bezel", "Bezel", BEZELS, { chrome: "Chrome", black: "Black", none: "None" }); chk ("blink", "Blinks (Play)"); }
    if (sameType && p.type === "lamp") ask ("style", "Lens", LAMPS, { jewel: "Jewel (faceted)", dome: "Dome", square: "Square" });
    if (sameType && p.type === "vu") { choice ("dial", "Dial", DIALS, { vu: "VU (-20 .. +3)", ppm: "PPM (1 .. 7)", percent: "Percent", gr: "Gain reduction" }); chk ("light", "Lamp behind the dial"); chk ("peak", "Peak LED"); }
    if (sameType && p.type === "ladder") { chk ("horizontal", "Horizontal"); ask ("palette", "Colours", PALETTES, { classic: "Green, yellow, red" }); chk ("peak", "Peak hold"); }
    if (sameType && p.type === "display") { ask ("kind", "Shows", DISPLAYS, { wave: "A waveform", bars: "Bars", spectrum: "A spectrum", digits: "Digits (7-segment)", text: "Text", blank: "Nothing" });
      str ("content", "Digits / text on it", 24); chk ("backlit", "Backlit (lit glass, dark print)"); }
    if (sameType && p.type === "label") { chk ("italic", "Italic"); num ("spacing", "Letter spacing (x)", 0, 3, 0.1); ask ("look", "Look", LOOKS, { print: "Printed", engraved: "Engraved", embossed: "Embossed", outline: "Outline" }); }
    if (sameType && p.type === "box") { choice ("lineStyle", "Border", LINES, { solid: "Solid", dashed: "Dashed", double: "Double", none: "None" }); num ("lineW", "Border width", 0.1, 2, 0.05);
      if (ps.some ((q) => q.fill)) { choice ("tone", "Fill", TONES, { lighter: "Lighter than the panel", darker: "Darker than the panel", colour: "A colour" }); if (ps.some ((q) => q.tone === "colour")) col ("fillCol", "Fill colour"); } }
    if (sameType && p.type === "line") chk ("dashed", "Dashed");
    if (sameType && p.type === "jack") { choice ("nut", "Nut", NUTS, { chrome: "Chrome", black: "Black", gold: "Gold" }); if (ps.some ((q) => q.plugged)) col ("cable", "Cable colour"); }
    if (sameType && p.type === "screw") { pick2 ("style", "Head", "screws", SCREW_STYLES, { unit: "As the rack screws", phillips: "Phillips", hex: "Hex", thumb: "Thumb", torx: "Torx", flat: "Flat" });
      choice ("metal", "Metal", METALS, { unit: "As the rack screws", chrome: "Chrome", black: "Black", brass: "Brass" }); }
    if (sameType && p.type === "vent") ask ("shape", "Kind", VENTS, { slots: "Slots", holes: "Round holes", hex: "Hex grid", louvre: "Louvres", grille: "Grille" });
    if (sameType && p.type === "plate") { ask ("style", "Metal", PLATES); chk ("screws", "Screwed on"); }
    if (has ("ink")) choice ("ink", "Print colour", INKS, { print: "The panel's print", accent: "The accent colour", white: "White", black: "Black", red: "Red", gold: "Gold" });

    if (has ("align")) choice ("align", "Align", ALIGNS);
    if (has ("fill")) chk ("fill", "Filled");
    if (has ("value") && !ps.some ((q) => q.type === "selector")) num ("value", sameType && p.type === "vu" ? "Needle" : sameType && p.type === "ladder" ? "Lit (%)" : "Position (%)", 0, 100, 1);
    if (has ("on")) chk ("on", sameType && p.type === "toggle" ? "On" : "Lit");
    if (has ("colour")) col ("colour", "Colour");
    if (has ("segments")) num ("segments", "Segments", 3, 24, 1);
    if (has ("count")) num ("count", ps.every ((q) => !q.shape || q.shape === "slots") ? "Slots" : "How many across", 2, 30, 1);
    if (has ("size") && sameType && p.type === "label") num ("size", "Text size", 2, 20, 0.5, () => { for (const q of ps) if (q.bend === "circle") q.radius = Math.max (2, q.radius); });
    if (has ("bold")) chk ("bold", "Bold");
    if (has ("round")) num ("round", "Corner", 0, 12, 0.5);
    if (has ("text")) str ("text", sameType && p.type === "label" ? "Text" : "Label", 40);
    chk ("lock", "Locked (can't be moved by accident)");
    if (many) {
      const grouped = ps.every ((q) => q.grp && q.grp === p.grp);
      btns ([[grouped ? "Ungroup" : "Group", grouped ? ungroup : group], ["Duplicate", duplicate], ["Mirror left ↔ right", mirror], ["Match size", matchSize],
             ["Lock all", () => setLock (true)], ["Unlock all", () => setLock (false)], ["Bring forward", () => order (1)], ["Send back", () => order (-1)], ["Delete", removeSelected, "danger"]]);
    } else {
      btns ([["Duplicate", duplicate], ["Mirror left ↔ right", mirror], ["Bring forward", () => order (1)], ["Send back", () => order (-1)]].concat (p.grp ? [["Leave group", ungroup]] : []).concat ([["Delete", removeSelected, "danger"]]));
    }
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
    "Opto leveler": () => T ({ name: "LEVELING AMPLIFIER", model: "EM-2A", height: 3, colour: "#8e9296", ink: "#15161a", finish: "wrinkle", badge: "TUBE", wear: 20 }, [
      { type: "knob", x: 95, y: 70, w: 44, h: 44, style: "tophat", value: 45, text: "GAIN", scale: true, min: 0, max: 100, steps: 10, nums: "all" },
      { type: "vu", x: 241, y: 62, w: 120, h: 70, style: "cream", value: 40, text: "GAIN REDUCTION" },
      { type: "knob", x: 387, y: 70, w: 44, h: 44, style: "tophat", value: 60, text: "PEAK REDUCTION", scale: true, min: 0, max: 100, steps: 10, nums: "all" },
      { type: "toggle", x: 180, y: 112, style: "bat", on: true, text: "LIMIT" },
      { type: "toggle", x: 302, y: 112, style: "bat", on: false, text: "METER" } ]),
    "Console channel": () => T ({ name: "CLASS A CHANNEL", model: "EM-73", height: 3, colour: "#5b6f86", ink: "#f3f3f3", finish: "enamel", sub: "DISCRETE  -  TRANSFORMER COUPLED" }, [
      { type: "box", x: 170, y: 72, w: 200, h: 92, text: "EQUALIZER", round: 3 },
      ...["HIGH", "MID", "LOW"].map ((t, i) => ({ type: "knob", x: 110 + i * 60, y: 55, w: 22, h: 22, style: "redtrim", value: 50, text: t, scale: true, min: -16, max: 16, steps: 8, nums: "ends" })),
      ...["12K", "FREQ", "HPF"].map ((t, i) => ({ type: "knob", x: 110 + i * 60, y: 100, w: 16, h: 16, style: "matte", value: 35 + i * 10, text: t, scale: false })),
      { type: "knob", x: 330, y: 66, w: 36, h: 36, style: "redtrim", value: 55, text: "GAIN", scale: true, min: -20, max: 70, steps: 18, nums: "all", lean: true },
      { type: "button", x: 400, y: 50, w: 12, h: 9, style: "square", on: false, colour: "#e8e3d6", text: "PHASE" },
      { type: "button", x: 400, y: 80, w: 12, h: 9, style: "square", on: true, colour: "#ff5a3c", text: "48V" },
      { type: "jack", x: 440, y: 110, w: 16, h: 16, style: "xlr", text: "LINE" } ]),
    "Bus compressor": () => T ({ name: "BUS COMPRESSOR", model: "EM-G", height: 1, colour: "#1b1c1f", ink: "#e8e8ea", finish: "powder" }, [
      ...["THRESHOLD", "RATIO", "ATTACK", "RELEASE", "MAKE-UP"].map ((t, i) => ({ type: "knob", x: 110 + i * 42, y: 20, w: 14, h: 14, style: "capwhite", value: 30 + i * 9, text: t, scale: false })),
      { type: "vu", x: 360, y: 22, w: 56, h: 30, style: "black", value: 30, text: "GR" },
      { type: "toggle", x: 425, y: 21, style: "rocker", on: true, text: "IN" } ]),
    "Mastering limiter": () => T ({ name: "MASTERING LIMITER", model: "EM-L2", height: 1, finish: "brushed", colour: "#b4b6ba", ink: "#1a1b1e" }, [
      { type: "display", x: 150, y: 22, w: 100, h: 26, colour: "#56c8f5", text: "CEILING -0.3" },
      ...["INPUT", "CEILING", "RELEASE"].map ((t, i) => ({ type: "knob", x: 250 + i * 40, y: 20, w: 16, h: 16, style: "knurled", value: 40 + i * 12, text: t, scale: false })),
      { type: "ladder", x: 382, y: 22, w: 5, h: 32, segments: 16, value: 70 },
      { type: "ladder", x: 392, y: 22, w: 5, h: 32, segments: 16, value: 64 },
      { type: "button", x: 430, y: 20, w: 10, h: 10, style: "round", on: true, colour: "#46e070", text: "LINK" } ]),
    "Tape echo": () => T ({ name: "TAPE ECHO", model: "EM-201", height: 2, colour: "#3f5f45", ink: "#efe6cf", finish: "paint", wear: 30 }, [
      ...["REPEAT RATE", "INTENSITY", "ECHO", "REVERB", "BASS", "TREBLE"].map ((t, i) => ({ type: "knob", x: 70 + i * 55, y: 44, w: 22, h: 22, style: "chicken", value: 30 + i * 8, text: t, scale: true, min: 0, max: 10, steps: 10, nums: "ends" })),
      { type: "vu", x: 420, y: 36, w: 56, h: 32, style: "cream", value: 35, text: "INPUT" },
      { type: "led", x: 395, y: 20, w: 4, h: 4, colour: "#ff3b30", on: true, text: "PEAK" },
      { type: "toggle", x: 420, y: 72, style: "bat", on: true, text: "BYPASS" } ]),
    "Plate reverb": () => T ({ name: "PLATE REVERB", model: "EM-140", height: 2, finish: "spun", colour: "#c3c5c9", ink: "#1b1c20" }, [
      { type: "display", x: 170, y: 36, w: 130, h: 36, colour: "#ffb347", text: "PLATE  2.4 s" },
      { type: "vent", x: 170, y: 72, w: 130, h: 8, count: 22 },
      ...["DECAY", "PRE-DELAY", "DAMP", "MIX"].map ((t, i) => ({ type: "knob", x: 290 + i * 42, y: 42, w: 18, h: 18, style: "knurled", value: 35 + i * 10, text: t, scale: true, min: 0, max: 10, steps: 10, nums: "none" })) ]),
    "Twin mic preamp": () => T ({ name: "TWIN MIC PREAMP", model: "EM-P2", height: 1, colour: "#7a1c1c", ink: "#f4f4f4", finish: "anodised" }, [
      ...[0, 1].flatMap ((c) => [
        { type: "jack", x: 70 + c * 190, y: 22, w: 14, h: 14, style: "xlr", text: "IN " + (c + 1) },
        { type: "knob", x: 115 + c * 190, y: 20, w: 18, h: 18, style: "capwhite", value: 55, text: "GAIN", scale: false },
        { type: "button", x: 155 + c * 190, y: 20, w: 9, h: 9, style: "round", on: c === 0, colour: "#ff5a3c", text: "48V" },
        { type: "button", x: 180 + c * 190, y: 20, w: 9, h: 9, style: "round", on: false, colour: "#e0a84a", text: "PAD" },
        { type: "led", x: 205 + c * 190, y: 18, w: 3, h: 3, colour: "#46e070", on: true, text: "" } ]),
      { type: "line", x: 238, y: 22, w: 0.5, h: 34 } ]),
    "Headphone amp": () => T ({ name: "HEADPHONE AMPLIFIER", model: "EM-HA", height: 1, colour: "#1a1b1e", ink: "#e9e9ea", finish: "carbon", sub: "CLASS A" }, [
      { type: "jack", x: 190, y: 22, w: 12, h: 12, style: "trs", text: "PHONES A" },
      { type: "jack", x: 230, y: 22, w: 12, h: 12, style: "trs", text: "PHONES B" },
      { type: "knob", x: 320, y: 20, w: 26, h: 26, style: "knurled", value: 45, text: "", scale: true, min: 0, max: 10, steps: 20, nums: "none" },
      { type: "led", x: 385, y: 20, w: 3, h: 3, colour: "#56c8f5", on: true, text: "ON" },
      { type: "toggle", x: 425, y: 21, style: "rocker", on: true, text: "POWER" } ]),
    "Power conditioner": () => T ({ name: "POWER CONDITIONER", model: "EM-PC8", height: 1, colour: "#0c0c0e", ink: "#e8e8ea", finish: "gloss" }, [
      { type: "display", x: 210, y: 22, w: 60, h: 18, colour: "#ffb347", text: "120 V" },
      { type: "ladder", x: 260, y: 22, w: 5, h: 30, segments: 10, value: 50 },
      { type: "button", x: 330, y: 20, w: 12, h: 9, style: "square", on: false, colour: "#e8e3d6", text: "LIGHTS" },
      { type: "toggle", x: 425, y: 21, style: "rockerred", on: true, text: "POWER" } ]),
    "Boutique EQ": () => T ({ name: "PASSIVE EQUALIZER", model: "EM-GOLD", height: 2, colour: "#c9a24a", ink: "#2a2215", finish: "gold", badge: "HAND WIRED" }, [
      ...["LOW", "LO MID", "HI MID", "HIGH"].map ((t, i) => ({ type: "knob", x: 110 + i * 62, y: 34, w: 22, h: 22, style: "knurled", value: 50, text: t, scale: true, min: -12, max: 12, steps: 8, nums: "all", lean: true })),
      ...["Hz", "Hz", "kHz", "kHz"].map ((t, i) => ({ type: "knob", x: 110 + i * 62, y: 72, w: 13, h: 13, style: "knurled", value: 40, text: "", scale: false })),
      { type: "toggle", x: 400, y: 45, style: "bat", on: true, text: "EQ IN" },
      { type: "led", x: 400, y: 20, w: 4, h: 4, colour: "#ffb347", on: true, text: "" } ]),
    "Broadcast limiter": () => T ({ name: "PROGRAM LIMITER", model: "EM-BL", height: 3, colour: "#3b2417", ink: "#e9dcc0", finish: "bakelite", badge: "BROADCAST", wear: 35 }, [
      { type: "knob", x: 95, y: 66, w: 36, h: 36, style: "chicken", value: 40, text: "INPUT", scale: true, min: 0, max: 10, steps: 10, nums: "all" },
      { type: "vu", x: 241, y: 58, w: 118, h: 66, style: "amber", value: 50, text: "PROGRAM LEVEL" },
      { type: "knob", x: 387, y: 66, w: 36, h: 36, style: "chicken", value: 55, text: "OUTPUT", scale: true, min: 0, max: 10, steps: 10, nums: "all" },
      { type: "toggle", x: 185, y: 112, style: "bat", on: true, text: "AGC" },
      { type: "toggle", x: 297, y: 112, style: "bat", on: true, text: "POWER" } ]),
    "Walnut tube preamp": () => T ({ name: "TUBE PREAMPLIFIER", model: "EM-12AX7", height: 2, colour: "#5a3721", ink: "#f0e2c8", finish: "walnut", earColour: "black" }, [
      ...["DRIVE", "TONE", "OUTPUT"].map ((t, i) => ({ type: "knob", x: 90 + i * 70, y: 42, w: 26, h: 26, style: "tophat", value: 40 + i * 10, text: t, scale: true, min: 0, max: 10, steps: 10, nums: "ends", arcText: true })),
      { type: "toggle", x: 300, y: 40, style: "bat", on: false, text: "HI-Z" },
      { type: "vu", x: 390, y: 42, w: 70, h: 40, style: "cream", value: 45, text: "VU" } ]),
    "DI box": () => T ({ name: "DIRECT BOX", model: "EM-DI", height: 1, colour: "#a7a9ad", ink: "#111214", finish: "diamond", edge: "square" }, [
      { type: "jack", x: 80, y: 22, w: 14, h: 14, style: "trs", text: "INST" },
      { type: "toggle", x: 190, y: 21, style: "bat", on: false, text: "GND LIFT" },
      { type: "toggle", x: 240, y: 21, style: "bat", on: true, text: "PAD" },
      { type: "jack", x: 400, y: 22, w: 14, h: 14, style: "xlr", text: "OUT" } ]),
    "Synth voice": () => T ({ name: "ANALOG VOICE", model: "EM-VCO", height: 3, colour: "#3b1f6b", ink: "#ffffff", finish: "flake" }, [
      ...["PITCH", "FINE", "SHAPE", "PWM", "CUTOFF", "RES", "ENV", "LFO"].map ((t, i) => ({ type: "knob", x: 70 + (i % 4) * 58, y: 40 + Math.floor (i / 4) * 52, w: 20, h: 20,
        style: i < 4 ? "capblue" : "capred", value: 30 + i * 7, text: t, scale: true, min: 0, max: 10, steps: 10, nums: "none" })),
      { type: "display", x: 370, y: 44, w: 110, h: 36, colour: "#b77dff", text: "SAW" },
      { type: "ladder", x: 440, y: 100, w: 5, h: 30, segments: 12, value: 60 },
      { type: "button", x: 370, y: 100, w: 12, h: 12, style: "round", on: true, colour: "#b77dff", text: "GATE" } ]),
  };

  // A preset that shows the newer parts and settings off
  templates["Studio showcase"] = () => T ({ name: "SHOWCASE", model: "EM-S2", height: 3, colour: "#1b2230", ink: "#e9edf4", finish: "satin", accent: "#ffb020",
    trim: "pinstripe", twoTone: "left", toneColour: "#0f141d", toneSize: 22, glow: true, serial: "0427", screwMetal: "brass", titlePos: "topleft", titleSize: 5 }, [
    { type: "lamp", x: 45, y: 105, w: 10, h: 10, style: "jewel", colour: "#ff3b1f", on: true, text: "POWER" },
    { type: "toggle", x: 45, y: 72, style: "paddle", on: true, three: true, mid: false, text: "MODE", upText: "HI", downText: "LO" },
    { type: "knob", x: 150, y: 58, w: 30, h: 30, style: "knurled", value: 62, text: "DRIVE", scale: true, min: -12, max: 12, bipolar: true, ring: true, ringColour: "#ffb020", suffix: "dB", marks: "dots" },
    { type: "selector", x: 230, y: 58, w: 22, h: 22, style: "chicken", value: 2, text: "SHAPE", stops: "SOFT|WARM|FAT|HOT|CRUSH" },
    { type: "slider", x: 300, y: 70, w: 12, h: 62, style: "silver", value: 65, text: "MIX", scale: true, ink: "accent" },
    { type: "vu", x: 390, y: 52, w: 70, h: 40, style: "green", value: 58, text: "OUTPUT", dial: "ppm", light: true, peak: true },
    { type: "ladder", x: 390, y: 104, w: 70, h: 5, segments: 16, value: 70, horizontal: true, palette: "blue", peak: true },
    { type: "button", x: 150, y: 112, w: 14, h: 8, style: "pill", colour: "#ffb020", on: true, text: "BOOST", led: true, capText: "ON" },
    { type: "display", x: 230, y: 112, w: 50, h: 16, colour: "#ffb020", kind: "digits", content: "-3.5", text: "" },
    { type: "plate", x: 300, y: 12, w: 64, h: 12, style: "brass", text: "HAND BUILT", screws: true },
    { type: "vent", x: 440, y: 20, w: 30, h: 12, count: 8, shape: "hex" },
    { type: "box", x: 190, y: 62, w: 150, h: 70, text: "CHANNEL", round: 3, lineStyle: "double", ink: "accent" } ]);

  // The gradients and filters every picture on the page shares: first in the page, never hidden (see render)
  defs (document.getElementById ("d-defs"));

  /** A whole design as a small picture (the preset cards): drawn as the editor draws it. */
  function designThumb (d, width = 150) {
    const t = document.createElementNS (NS, "svg"), was = design, wasSel = selected;
    design = d; selected = [];
    render (t, "edit", false);
    design = was; selected = wasSel;
    t.setAttribute ("width", width); t.setAttribute ("height", Math.round (width * (d.unit.height * U + 12) / (W + 12)));
    t.setAttribute ("aria-hidden", "true");
    return t;
  }

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

  // Sharing: a link (the design rides in the part after "#", which browsers never send to any server), the
  // system's own share sheet where there is one, or a small file - no typing anywhere
  const dialog = $("share-dialog");
  const shareLink = () => location.href.split ("#")[0] + "#d=" + $("share-code").value;
  $("share").addEventListener ("click", async () => {
    const code = await encode (design); $("share-code").value = code; $("import-msg").textContent = "";
    $("send-native").hidden = typeof navigator.share !== "function";
    dialog.showModal();
  });
  $("share-close").addEventListener ("click", () => dialog.close());
  const copy = async (s, btn) => { try { await navigator.clipboard.writeText (s); const t = btn.textContent; btn.textContent = "Copied"; setTimeout (() => (btn.textContent = t), 1400); } catch (_) { $("share-code").select(); } };
  $("copy-code").addEventListener ("click", (e) => copy ($("share-code").value, e.currentTarget));
  $("copy-link").addEventListener ("click", (e) => copy (shareLink(), e.currentTarget));
  $("send-native").addEventListener ("click", async () => { try { await navigator.share ({ title: design.unit.name || "My rack unit", text: "A rack unit I designed", url: shareLink() }); } catch (_) { /* cancelled */ } });
  $("save-file").addEventListener ("click", () => download (new Blob ([$("share-code").value + "\n"], { type: "text/plain" }), fileName ("enh")));
  // Opening: a pasted link or code, a chosen file, a file dropped on the stage - all through the decoder
  async function openCode (text, msgEl) {
    try { replaceDesign (await decode (text)); if (dialog.open) dialog.close(); return true; }
    catch (err) { if (msgEl) msgEl.textContent = err && err.message ? err.message : "That could not be opened."; return false; }
  }
  const readFile = (f) => (f && f.size <= MAX_CODE + 64 ? f.text() : Promise.reject (new Error ("That file is not a design (too big).")));
  $("import-go").addEventListener ("click", () => openCode ($("import-code").value, $("import-msg")));
  $("open-file").addEventListener ("click", () => $("open-file-input").click());
  $("open-file-input").addEventListener ("change", async (e) => {
    try { await openCode (await readFile (e.target.files[0]), $("import-msg")); } catch (err) { $("import-msg").textContent = err.message; }
    e.target.value = "";
  });
  const stageEl = $("stage");
  stageEl.addEventListener ("dragover", (e) => { if (e.dataTransfer && [...e.dataTransfer.types].includes ("Files")) { e.preventDefault(); stageEl.classList.add ("drop"); } });
  stageEl.addEventListener ("dragleave", () => stageEl.classList.remove ("drop"));
  stageEl.addEventListener ("drop", async (e) => {
    e.preventDefault(); stageEl.classList.remove ("drop");
    try { await openCode (await readFile (e.dataTransfer.files[0]), null); } catch (_) { /* not a design */ }
  });
  // A link clicked while the designer is already open
  window.addEventListener ("hashchange", () => { if (location.hash.startsWith ("#d=")) openCode (location.hash, null); });

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

  // The knob maker: the design's own knobs, each made of checked choices and clamped numbers
  let ckEditing = 0;
  function knobPreview (ck, size = 110) {
    const t = document.createElementNS (NS, "svg"); t.setAttribute ("viewBox", "-22 -22 44 44"); t.setAttribute ("width", size); t.setAttribute ("height", size);
    t.setAttribute ("aria-hidden", "true");
    drawCustomKnob (el ("g", { transform: "rotate(30)" }, t), ck, 13, "auto");
    return t;
  }
  function showKnobMaker () {
    const list = $("ck-list"); list.replaceChildren();
    const ks = design.knobs || [];
    ckEditing = Math.min (ckEditing, Math.max (0, ks.length - 1));
    if (!ks.length) { const li = document.createElement ("li"); li.className = "muted small"; li.textContent = "No knobs of your own yet: make one."; list.appendChild (li); }
    ks.forEach ((ck, i) => {
      const li = document.createElement ("li"); if (i === ckEditing) li.className = "on";
      li.appendChild (knobPreview (ck, 40));
      const n = document.createElement ("span"); n.textContent = ck.n; li.appendChild (n);
      const del = document.createElement ("button"); del.type = "button"; del.textContent = "Delete"; del.className = "danger";
      del.addEventListener ("click", (e) => { e.stopPropagation(); deleteKnob (i); });
      li.appendChild (del);
      li.addEventListener ("click", () => { ckEditing = i; showKnobMaker(); });
      list.appendChild (li);
    });
    $("ck-new").disabled = ks.length >= MAX_KNOBS;
    const ed = $("ck-edit"); ed.replaceChildren();
    const ck = ks[ckEditing]; if (!ck) return;
    const prev = document.createElement ("div"); prev.className = "d-ck-preview"; prev.appendChild (knobPreview (ck)); ed.appendChild (prev);
    const setK = (key, v) => { design.knobs[ckEditing] = sanitize ({ knobs: [Object.assign ({}, ck, { [key]: v })] }).knobs[0]; render(); save(); showKnobMaker(); };
    const lab = (label, input) => { const l = document.createElement ("label"); l.textContent = label + " "; l.appendChild (input); ed.appendChild (l); return input; };
    const nameIn = document.createElement ("input"); nameIn.maxLength = 20; nameIn.value = ck.n; nameIn.autocomplete = "off";
    nameIn.addEventListener ("change", () => { setK ("n", nameIn.value); commit(); }); lab ("Name", nameIn);
    const pickK = (key, label, list) => { const f = document.createElement ("div"); f.className = "d-field"; const t = document.createElement ("span"); t.textContent = label; f.appendChild (t);
      f.appendChild (picker ("ck:" + key, list, CK_NAMES[key], ck[key], (v) => { setK (key, v); commit(); }, label)); ed.appendChild (f); };
    const slide = (key, label, lo, hi, step) => { const i = document.createElement ("input"); i.type = "range"; i.min = lo; i.max = hi; i.step = step; i.value = ck[key];
      i.addEventListener ("input", () => { design.knobs[ckEditing] = sanitize ({ knobs: [Object.assign ({}, design.knobs[ckEditing], { [key]: Number (i.value) })] }).knobs[0]; render(); save();
        prev.replaceChildren (knobPreview (design.knobs[ckEditing])); });
      i.addEventListener ("change", () => { commit(); showKnobMaker(); }); lab (label, i); };
    const col = (key, label) => { const i = document.createElement ("input"); i.type = "color"; i.value = ck[key];
      i.addEventListener ("input", () => { design.knobs[ckEditing] = sanitize ({ knobs: [Object.assign ({}, design.knobs[ckEditing], { [key]: i.value })] }).knobs[0]; render(); save();
        prev.replaceChildren (knobPreview (design.knobs[ckEditing])); });
      i.addEventListener ("change", () => { commit(); showKnobMaker(); });
      const l = lab (label, i); l.parentElement.classList.add ("row"); };
    pickK ("sh", "Shape", CK_SHAPES); slide ("ht", "Height", 0.3, 2, 0.05); slide ("tp", "Top width", 0.4, 1.1, 0.02); slide ("sk", "Skirt", 0, 1.6, 0.05);
    pickK ("gr", "Grip", CK_GRIPS); slide ("gc", "Grip count", 6, 120, 1); slide ("gd", "Grip depth", 0, 1, 0.05);
    pickK ("cap", "Cap", CK_CAPS); slide ("cs", "Cap size", 0.3, 1, 0.02); pickK ("pt", "Pointer", CK_POINTERS); pickK ("mt", "Material", CK_MATS);
    col ("bc", "Body"); col ("kc", "Cap"); col ("sc", "Skirt"); col ("pc", "Pointer");
  }
  function deleteKnob (i) {
    // parts using it go back to a standard knob; parts using a later one follow it down the list
    for (const p of design.parts) if (p.type === "knob" && /^c[0-7]$/.test (p.style)) {
      const k = Number (p.style.slice (1));
      if (k === i) p.style = "ribbed"; else if (k > i) p.style = "c" + (k - 1);
    }
    design.knobs.splice (i, 1); commit(); showKnobMaker();
  }
  $("ck-new").addEventListener ("click", () => {
    design.knobs = design.knobs || [];
    if (design.knobs.length >= MAX_KNOBS) return;
    const k = blankKnob(); k.n = "MY KNOB " + (design.knobs.length + 1);
    design.knobs.push (k); ckEditing = design.knobs.length - 1; commit(); showKnobMaker();
  });
  $("ck-use").addEventListener ("click", () => {
    if (!design.knobs || !design.knobs[ckEditing]) return;
    for (const p of selected.map (byId)) if (p && p.type === "knob") p.style = "c" + ckEditing;
    commit();
  });

  // Tool tabs (the rail on the left): one panel at a time, arrow keys between them, the last one used remembered
  const tabs = [...document.querySelectorAll ('.d-rail [role="tab"]')];
  function showTab (t, focus) {
    for (const b of tabs) { const on = b === t; b.setAttribute ("aria-selected", String (on)); b.tabIndex = on ? 0 : -1; $(b.getAttribute ("aria-controls")).hidden = !on; }
    if (focus) t.focus();
    try { localStorage.setItem ("enh-designer-tab", t.id); } catch (_) { /* fine */ }
  }
  tabs.forEach ((b, i) => {
    b.addEventListener ("click", () => showTab (b));
    b.addEventListener ("keydown", (e) => { const k = { ArrowDown: 1, ArrowRight: 1, ArrowUp: -1, ArrowLeft: -1 }[e.key];
      if (k) { e.preventDefault(); showTab (tabs[(i + k + tabs.length) % tabs.length], true); } });
  });
  try { const t = document.getElementById (localStorage.getItem ("enh-designer-tab") || ""); if (t && tabs.includes (t)) showTab (t); } catch (_) { /* fine */ }

  // Palette and templates
  for (const type in TYPES) { const b = document.createElement ("button"); b.type = "button"; b.textContent = TYPES[type].label; b.addEventListener ("click", () => addPart (type)); $("palette").appendChild (b); }
  for (const name in templates) {
    const b = document.createElement ("button"); b.type = "button"; b.className = "d-tpl";
    b.appendChild (designThumb (sanitize (templates[name]())));
    const n = document.createElement ("span"); n.textContent = name; b.appendChild (n);
    b.addEventListener ("click", () => replaceDesign (sanitize (templates[name]()))); $("templates").appendChild (b); }

  // ?selftest: hostile share codes through the decoder, and a round trip (results printed on the page)
  const strip0 = (x) => JSON.stringify (x.parts.map ((p) => Object.assign ({}, p, { id: 0 })).map ((p) => Object.fromEntries (Object.entries (p).map (([a, b]) => [a, typeof b === "number" ? Math.round (b * 10) / 10 : b]))));
  async function selftest () {
    const out = []; const ok = (c, m) => out.push ((c ? "PASS " : "FAIL ") + m);
    const mk = async (obj) => "ENH1." + b64u (await streamBytes (new TextEncoder().encode (JSON.stringify (obj)), new CompressionStream ("deflate-raw")));
    const evil = await mk ({ v: 1, u: { name: "<img src=x onerror=alert(1)>", model: "\u0000\u202e", height: 99, finish: "javascript:", colour: "red;x", ink: "#FFFFFF",
      ears: "x", extra: "secret" }, p: [ { t: "script", x: 1 }, { t: "knob", x: 1e9, y: -5, w: "NaN", s: "evil", v: 1e6, l: "<b>" + "A".repeat (500) },
      { t: "label", l: "javascript:alert(1)", z: 999 }, ...Array.from ({ length: 400 }, () => ({ t: "led" })) ], __proto__: { polluted: true } });
    const d = await decode (evil);
    ok (!/[<>]/.test (d.unit.name) && d.unit.name.length <= 40, "markup is stripped from names: " + JSON.stringify (d.unit.name));
    ok (d.unit.height === 6 && d.unit.finish === "anodised" && d.unit.colour === "#16171a" && d.unit.ink === "#ffffff" && d.unit.ears === "slots", "unknown or out-of-range unit fields fall back or clamp");
    ok (!("extra" in d.unit), "fields that are not design data are dropped");
    ok (!d.parts.some ((p) => p.type === "script"), "unknown part types are dropped");
    ok (d.parts.length === MAX_PARTS, "at most " + MAX_PARTS + " parts (" + d.parts.length + ")");
    const k = d.parts[0]; ok (k.x === W && k.y === 0 && k.value === 100 && k.style === "ribbed" && k.text.length <= 40 && !/[<>]/.test (k.text), "part numbers clamp, styles checked, text limited");
    ok (!({}).polluted, "no prototype pollution");
    const sd = sanitize ({ unit: { desc: "Line one\n<script>x</script>\n" + "z".repeat (900), shine: 900 } });
    ok (!/[<>]/.test (sd.unit.desc) && sd.unit.desc.length <= 600 && sd.unit.desc.includes ("\n") && sd.unit.shine === 100, "descriptions keep lines, lose markup, and are limited");
    // Custom knobs and sockets: every field checked, at most MAX_KNOBS, unknown references dropped
    const ek = await decode (await mk ({ v: 2, u: {}, k: Array.from ({ length: 20 }, () => ({ n: "<svg onload=x>", sh: "evil", ht: 1e9, gc: -5, bc: "url(#x)", mt: "lava", extra: 1 })),
      p: [{ t: "knob", s: "c9" }, { t: "knob", s: "c1" }, { t: "jack", s: "usbc", pl: true }, { t: "jack", s: "<b>" }] }));
    const k0 = ek.knobs[0];
    ok (ek.knobs.length === MAX_KNOBS && !/[<>]/.test (k0.n) && k0.sh === "cyl" && k0.ht === 2 && k0.gc === 6 && k0.bc === "#141416" && k0.mt === "satin" && !("extra" in k0),
        "custom knobs: at most " + MAX_KNOBS + ", every field checked or clamped");
    ok (ek.parts[0].style === "ribbed" && ek.parts[1].style === "c1", "a knob refers only to a custom knob that exists");
    ok (ek.parts[2].style === "usbc" && ek.parts[2].plugged === true && ek.parts[3].style === "trs", "sockets: known types only; plugged kept");
    const withKnob = sanitize ({ knobs: [{ n: "MINE", sh: "tophat", gr: "flutes", bc: "#aa2233" }], parts: [{ type: "knob", style: "c0" }, { type: "jack", style: "xlrm", plugged: true }] });
    const backK = await decode (await encode (withKnob));
    ok (JSON.stringify (backK.knobs) === JSON.stringify (withKnob.knobs) && backK.parts[0].style === "c0" && backK.parts[1].plugged === true, "custom knobs and plugged sockets survive the round trip");
    const c2 = await encode (sanitize (templates["Console channel"]()));
    ok (/^ENH2(-[0-9A-Za-z]{1,4})+$/.test (c2), "codes come in dashed groups of four (" + c2.split ("-").length + " groups)");
    let bad = false; try { await decode (c2.slice (0, -3) + "!!!"); } catch (_) { bad = true; } ok (bad, "a damaged code is refused");
    const odd = await decode (await mk ({ v: 1, u: {}, p: [{ t: "label", be: "evil", ra: 1e9, cu: -1e9, gp: -4, fl: "yes" }, { t: "knob", st: 999, nu: "<x>", sw: 5, le: 1 }] }));
    const [ol, ok2] = odd.parts;
    ok (ol.bend === "none" && ol.radius === 240 && ol.curve === -100 && !("grp" in ol) && ol.flip === false, "text-bending fields are checked and clamped");
    ok (ok2.steps === 20 && ok2.nums === "ends" && ok2.sweep === 180 && ok2.lean === false, "scale fields are checked and clamped");
    // The newer settings and parts: each field checked, clamped or picked from its list; round trip intact
    const en = await decode (await mk ({ v: 2, u: { accent: "red", trim: "<x>", twoTone: "band", toneSize: 900, titlePos: "js:", titleSize: -3, glow: "yes",
      serial: "<b>" + "9".repeat (40), screwMetal: "gold", chassis: "#ABCDEF", depth: 1e9 },
      p: [{ t: "knob", mk: "evil", bp: 1, rg: true, rc: "url(#x)", sx: "<i>dBdBdB", lp: "side", lz: 99, ik: "neon", dt: true },
          { t: "selector", so: "A|<b>|" + Array.from ({ length: 30 }, (_, i) => "P" + i).join ("|"), v: 999, s: "c7" },
          { t: "slider", s: "gold", hz: true, st: 999 }, { t: "lamp", s: "disco", k: "#00ff00" }, { t: "plate", s: "wood", sc: "no" },
          { t: "led", sh: "star", bz: "diamond", bk: 1 }, { t: "vent", sh: "hex" }, { t: "vent", sh: "round" }, { t: "display", kd: "<svg>", co: "<script>12" },
          { t: "box", ls: "wavy", lw: 50, tn: "x", fc: "#123" }, { t: "screw", s: "<x>", me: "gold" }, { t: "label", lk: "fire", sp: 99, it: "y" }] }));
    const eu = en.unit, [nk, ns, nf, nl, np, nd, nv1, nv2, ndis, nb, nsc, nlab] = en.parts;
    ok (eu.accent === "#ff8a2a" && eu.trim === "none" && eu.twoTone === "band" && eu.toneSize === 90 && eu.titlePos === "topleft" && eu.titleSize === 3 && eu.glow === false
        && !/[<>]/.test (eu.serial) && eu.serial.length <= 16 && eu.screwMetal === "chrome" && eu.chassis === "#abcdef" && eu.depth === 400, "the new panel settings are checked and clamped");
    ok (nk.marks === "ticks" && nk.bipolar === false && nk.ring === true && nk.ringColour === "#ff8a2a" && nk.suffix.length <= 6 && !/[<>]/.test (nk.suffix)
        && nk.labelPos === "below" && nk.labelSize === 6 && nk.ink === "print" && nk.detent === true, "the new knob settings are checked and clamped");
    ok (ns.stops.split ("|").length === 12 && !/[<>]/.test (ns.stops) && ns.value === 11 && ns.style === "chicken", "a rotary switch: 2 - 12 plain positions, its setting one of them, its knob one that exists");
    ok (nf.style === "black" && nf.horizontal === true && nf.steps === 20 && nl.style === "jewel" && nl.colour === "#00ff00" && np.style === "brass" && np.screws === true,
        "sliders, pilot lamps and nameplates: known styles only");
    ok (nd.shape === "round" && nd.bezel === "chrome" && nd.blink === false && nv1.shape === "hex" && nv2.shape === "slots", "LED and vent shapes come from their own lists");
    ok (ndis.kind === "wave" && !/[<>]/.test (ndis.content) && nb.lineStyle === "solid" && nb.lineW === 2 && nb.tone === "lighter" && nb.fillCol === "#2a2b30"
        && nsc.style === "unit" && nsc.metal === "unit" && nlab.look === "print" && nlab.spacing === 3 && nlab.italic === false, "displays, boxes, screws and text: every new field checked");
    const showcase = sanitize (templates["Studio showcase"]()), backS = await decode (await encode (showcase));
    ok (JSON.stringify (backS.unit) === JSON.stringify (showcase.unit) && strip0 (backS) === strip0 (showcase), "the new parts and settings survive the round trip");
    const before = design.parts.length; selected = design.parts.slice (0, 2).map ((q) => q.id);
    const c = copyParts(); ok (c.startsWith (CLIP) && !/unit|name/.test (c), "copied parts carry only the parts");
    pasteParts (c); ok (design.parts.length === before + selected.length, "copied parts paste back");
    ok (pasteParts (CLIP + JSON.stringify ([{ t: "script" }, { t: "knob", x: 1e9 }])) && design.parts[design.parts.length - 1].x <= W, "pasted parts are sanitized");
    undo(); undo();
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
