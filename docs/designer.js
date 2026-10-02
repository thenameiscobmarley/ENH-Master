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
    display: { label: "Display", w: 90, h: 30, defaults: { colour: "#56c8f5", text: "ENH", kind: "wave", content: "", backlit: false,
                                                   scrShape: "sine", scrStyle: "lines", scrColours: "warmcool", scrSpeed: 5 } },
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
  const PALETTES = ["classic", "green", "blue", "amber", "white", "red"], DISPLAYS = ["wave", "bars", "spectrum", "digits", "text", "blank", "scope", "colour", "cube", "custom"];
  const SCR_SHAPES = ["sine", "square", "saw", "noise", "pulse"], SCR_STYLES = ["lines", "dots", "bars", "rings"], SCR_COLOURS = ["mono", "warmcool", "rainbow"];
  const LOOKS = ["print", "engraved", "embossed", "outline"], LINES = ["solid", "dashed", "double", "none"], TONES = ["lighter", "darker", "colour"];
  const NUTS = ["chrome", "black", "gold"], SCREW_STYLES = ["unit", "phillips", "hex", "thumb", "torx", "flat"], METALS = ["unit", "chrome", "black", "brass"];
  const VENTS = ["slots", "holes", "hex", "louvre", "grille"], FADERS = ["black", "silver", "white", "red"], LAMPS = ["jewel", "dome", "square"];
  const PLATES = ["brass", "silver", "gold", "black"];
  const TRIMS = ["none", "pinstripe", "double", "inset"], TWO_TONES = ["none", "left", "right", "top", "bottom", "band"];
  const TITLE_POS = ["topleft", "topcentre", "bottomleft", "hidden"], SCREW_METALS = ["chrome", "black", "brass"];
  const INK_COLOUR = { white: "#f2f2f2", black: "#111113", red: "#d8322b", gold: "#d4af37" };

  /* The unit's sound (the Sound tab): a chain of up to MAX_BLOCKS blocks, each a type from DSP_BLOCKS with
     its parameters (each clamped to its range). A knob, slider, switch or rotary switch can be wired to one
     parameter (its `ctl`: "<block>.<param>"; a switch's "<block>.on" turns the block in and out). Built
     from the browser's own audio nodes (designer-audio.js); saved in the share code like everything else. */
  const MAX_BLOCKS = 8;
  //   type: [name, { param: [label, min, max, default, unit, log?] }]
  const DSP_BLOCKS = {
    eq:      ["EQ", { low: ["Low", -15, 15, 0, "dB"], lowf: ["Low freq", 30, 500, 120, "Hz", 1], mid: ["Mid", -15, 15, 0, "dB"], midf: ["Mid freq", 200, 8000, 1200, "Hz", 1],
                      q: ["Mid width", 0.3, 6, 0.9, ""], high: ["High", -15, 15, 0, "dB"], highf: ["High freq", 2000, 16000, 8000, "Hz", 1] }],
    filter:  ["Filter", { mode: ["Type (0 low-pass, 1 high-pass, 2 band)", 0, 2, 0, ""], freq: ["Cutoff", 20, 20000, 8000, "Hz", 1], q: ["Resonance", 0.3, 12, 0.7, ""] }],
    drive:   ["Saturator", { drive: ["Drive", 0, 36, 9, "dB"], shape: ["Shape (0 tube, 1 tape, 2 hard)", 0, 2, 0, ""], tone: ["Tone", 1000, 20000, 9000, "Hz", 1], mix: ["Mix", 0, 100, 60, "%"] }],
    comp:    ["Compressor", { threshold: ["Threshold", -60, 0, -18, "dB"], ratio: ["Ratio", 1, 20, 3, ":1", 1], attack: ["Attack", 0.1, 100, 10, "ms", 1],
                              release: ["Release", 10, 1500, 150, "ms", 1], makeup: ["Makeup", 0, 24, 4, "dB"], mix: ["Mix", 0, 100, 100, "%"] }],
    exciter: ["Exciter", { freq: ["From", 1500, 12000, 4000, "Hz", 1], amount: ["Amount", 0, 100, 30, "%"] }],
    delay:   ["Delay", { time: ["Time", 10, 1500, 350, "ms", 1], feedback: ["Feedback", 0, 90, 35, "%"], tone: ["Tone", 500, 16000, 5000, "Hz", 1], mix: ["Mix", 0, 100, 25, "%"] }],
    room:    ["Reverb", { size: ["Size", 0.2, 8, 1.8, "s", 1], damp: ["Damping", 0, 100, 40, "%"], predelay: ["Pre-delay", 0, 200, 15, "ms"], mix: ["Mix", 0, 100, 22, "%"] }],
    width:   ["Stereo width", { width: ["Width", 0, 200, 120, "%"] }],
    chorus:  ["Chorus", { rate: ["Rate", 0.05, 6, 0.8, "Hz", 1], depth: ["Depth", 0, 100, 45, "%"], mix: ["Mix", 0, 100, 45, "%"] }],
    pan:     ["Pendulum pan", { rate: ["Rate", 0.05, 12, 0.5, "Hz", 1], depth: ["Depth", 0, 100, 60, "%"], mode: ["Moves (0 side to side, 1 volume)", 0, 1, 0, ""] }],
    wander:  ["Lava-lamp filter", { freq: ["Centre", 150, 8000, 1200, "Hz", 1], range: ["Range", 0, 100, 50, "%"], speed: ["Speed", 0.02, 2, 0.15, "Hz", 1], q: ["Resonance", 0.3, 10, 2, ""] }],
    stutter: ["Chops (stutter)", { rate: ["Chops a second", 1, 16, 4, "", 1], depth: ["Depth", 0, 100, 70, "%"], smooth: ["Smooth", 0, 100, 40, "%"] }],
    crush:   ["Bit crusher", { bits: ["Bits", 2, 12, 6, ""], mix: ["Mix", 0, 100, 50, "%"] }],
    wow:     ["Tape wow", { wow: ["Wow", 0, 100, 35, "%"], flutter: ["Flutter", 0, 100, 25, "%"] }],
    shimmer: ["Shimmer", { size: ["Size", 1, 8, 4, "s", 1], octave: ["Octave up", 0, 100, 50, "%"], mix: ["Mix", 0, 100, 30, "%"] }],
    gain:    ["Output", { gain: ["Level", -24, 12, 0, "dB"] }],
  };
  const DSP_TYPES = Object.keys (DSP_BLOCKS);
  const blankDsp = () => ({ chain: [] });
  const blockDefaults = (b) => Object.fromEntries (Object.entries (DSP_BLOCKS[b][1]).map (([k, v]) => [k, v[3]]));
  /** A part's wiring, checked against the chain it belongs to ("" = not wired). */
  const ctlOk = (ctl, chain) => { const m = /^([0-7])\.([a-z]+)$/.exec (String (ctl || "")); if (!m) return false;
    const b = chain[Number (m[1])]; return !!b && (m[2] === "on" || Object.prototype.hasOwnProperty.call (DSP_BLOCKS[b.b][1], m[2])); };
  function sanitizeDsp (raw) {
    const out = blankDsp();
    const chain = raw && typeof raw === "object" && Array.isArray (raw.chain) ? raw.chain.slice (0, MAX_BLOCKS * 4) : [];   // (a bounded read)
    for (const b of chain) {
      if (out.chain.length >= MAX_BLOCKS) break;
      if (!b || typeof b !== "object" || !DSP_TYPES.includes (b.b)) continue;
      const def = DSP_BLOCKS[b.b][1], p = {};
      for (const k in def) p[k] = clamp (b.p && typeof b.p === "object" ? b.p[k] : undefined, def[k][1], def[k][2], def[k][3]);
      out.chain.push ({ b: b.b, on: bool (b.on, true), p });
    }
    return out;
  }
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
    glow: false, serial: "", screwMetal: "chrome", chassis: "#1a1a1c", depth: 180 }, knobs: [], parts: [], dsp: { chain: [] } });

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
  let refit = () => {};   // (set once the zoom controls are wired: fits the unit to the stage unless you zoomed)
  let faceGroup = null, upNow = false, lastUp = false;   // (the editor's plate group, and whether it stands upright this render)
  const isUpright = () => design.unit.ears === "none";   // a 500-series module: no rack ears

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
    scrShape: one (SCR_SHAPES), scrStyle: one (SCR_STYLES), scrColours: one (SCR_COLOURS), scrSpeed: num (0, 10),
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
    d.dsp = sanitizeDsp (raw.dsp);
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
      if (["knob", "slider", "toggle", "button", "selector"].includes (p.type) && ctlOk (p.ctl, d.dsp.chain)) q.ctl = p.ctl;   // (wired to the sound)
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
    tone: "tn", fillCol: "fc", dashed: "da", nut: "nt", cable: "cb", metal: "me", stops: "so", screws: "sc", ctl: "cl",
    scrShape: "ssh", scrStyle: "sst", scrColours: "scl", scrSpeed: "ssp" };
  const LONG = Object.fromEntries (Object.entries (SHORT).map (([a, b]) => [b, a]));
  const round1 = (n) => Math.round (n * 10) / 10;

  function pack (d) {
    const bu = blank().unit, u = {};
    for (const k in d.unit) if (d.unit[k] !== bu[k]) u[k] = d.unit[k];
    return { v: 2, u, k: d.knobs && d.knobs.length ? d.knobs : undefined, x: d.dsp && d.dsp.chain.length ? d.dsp : undefined, p: d.parts.map ((p) => {
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
    return { unit: o.u, knobs: Array.isArray (o.k) ? o.k : [], dsp: o.x, parts: Array.isArray (o.p) ? o.p.map ((p) => { const q = {}; if (p && typeof p === "object") for (const k in p) if (LONG[k]) q[LONG[k]] = p[k]; return q; }) : [] };
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
    // A 500-series module (no rack ears) stands upright in the editor, as it sits in a LUNCHBOX: the plate turned a
    // quarter, each part turned back so knobs, switches and print read the right way up (the design itself is unchanged)
    const up = root === svg && !print && isUpright();
    upNow = up;
    if (root === svg && !print && up !== lastUp) { lastUp = up; requestAnimationFrame (() => refit()); }   // (turned: fit it again)
    root.setAttribute ("viewBox", print ? `0 0 ${W} ${H}` : up ? `-6 -6 ${H + 12} ${W + 12}` : `-6 -6 ${W + 12} ${H + 12}`);
    root.setAttribute ("width", print ? W : ((up ? H : W) + 12) * 2 * zoom);
    root.setAttribute ("height", print ? H : ((up ? W : H) + 12) * 2 * zoom);
    if (ownDefs) defs (root);
    const face = el ("g", up ? { transform: `translate(${H} 0) rotate(90)` } : {}, root);
    if (root === svg) faceGroup = face;
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
    // The grid, faint, while a part is dragged with snapping on (every millimetre it snaps to; every 5th brighter)
    if (root === svg && drag && drag.mode === "move" && drag.moved && snap && gridMm >= 1) {
      const gp = [];
      for (let x = 0, i = 0; x <= W; x += gridMm, ++i) gp.push (`M${x.toFixed (2)} 0V${H}`);
      for (let y = 0, i = 0; y <= H; y += gridMm, ++i) gp.push (`M0 ${y.toFixed (2)}H${W}`);
      el ("path", { d: gp.join (""), fill: "none", stroke: "#9ad8ff", "stroke-width": 0.08, opacity: 0.35 }, face);
    }
    // Parts, back to front
    for (const p of design.parts) drawPart (face, p);
    // Selection outlines
    if (!play && !print && root === svg) for (const p of design.parts) if (selected.includes (p.id)) {
      const b = bounds (p);
      el ("rect", { x: b.x - 1.5, y: b.y - 1.5, width: b.w + 3, height: b.h + 3, rx: 1.5, fill: "none", stroke: p.lock ? "#f59bd6" : p.grp ? "#c7a6ff" : "#7fe3e0", "stroke-width": 0.6, "stroke-dasharray": "2 1.2", class: "d-sel" }, face);
    }
    // Smart guides while dragging
    if (root === svg && drag && drag.mode === "move" && drag.guides) for (const g of drag.guides)
      el ("line", g.axis === "x" ? { x1: g.at, y1: -4, x2: g.at, y2: H + 4 } : { x1: -4, y1: g.at, x2: W + 4, y2: g.at }, face).setAttribute ("style", "stroke:#e8a33a;stroke-width:.35;stroke-dasharray:1.4 .9");
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
    const g = el ("g", { "data-id": p.id, class: "d-part", transform: `translate(${p.x} ${p.y})${upNow ? " rotate(-90)" : ""}${p.rot ? ` rotate(${p.rot})` : ""}` }, parent);
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
        if (SCREEN_KINDS.includes (kind)) { const scr = el ("g", { class: "d-scr", "data-scr": p.id }, g); drawScreen (scr, p, 1.3, 0.55, null); }
        else if (kind === "wave") { let d = ""; for (let i = 0; i <= 60; ++i) { const x = x0 + span * i / 60, y = Math.sin (i * 0.45) * Math.cos (i * 0.11) * p.h * 0.25; d += (i ? " L " : "M ") + x.toFixed (2) + " " + y.toFixed (2); }
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
    // The sound (designer-audio.js): the blocks, and a way to change the chain (sanitized, one undo step;
    // any wiring left pointing at a block that is gone is dropped)
    DSP_BLOCKS, MAX_BLOCKS,
    setDsp: (d, remap) => { design.dsp = sanitizeDsp (d);   // (remap: a block's old place -> its new one, so wiring follows a reorder)
      for (const q of design.parts) { if (q.ctl && remap) { const [bi, key] = q.ctl.split ("."); if (remap[Number (bi)] != null) q.ctl = remap[Number (bi)] + "." + key; }
        if (q.ctl && !ctlOk (q.ctl, design.dsp.chain)) delete q.ctl; }
      commit(); },
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
    requestAnimationFrame (refit);
  }
  let gridMm = 0.5;   // the grid's step, mm (Look: Grid size)
  function snapV (v) { return snap ? Math.round (v / gridMm) * gridMm : v; }

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
    // Equal gaps between the parts' edges (not their centres: a big knob and a small one then look even)
    const gaps = (axis) => { if (ps.length < 3) return; const size = axis === "x" ? ew : eh; ps.sort ((a, b) => a[axis] - b[axis]);
      const first = ps[0][axis] - size (ps[0]) / 2, last = ps[ps.length - 1][axis] + size (ps[ps.length - 1]) / 2;
      const gap = (last - first - ps.reduce ((t, q) => t + size (q), 0)) / (ps.length - 1);
      let at = first; for (const q of ps) { q[axis] = snapV (at + size (q) / 2); at += size (q) + gap; } };
    if (how === "hgap") gaps ("x");
    if (how === "vgap") gaps ("y");
    // A grid: rows as they already roughly are (or a square-ish grid), each cell as big as the biggest part
    if (how === "grid") {
      const cw = hi (ps.map (ew)) + 6, ch = hi (ps.map (eh)) + 6, cols = Math.max (1, Math.round (Math.sqrt (ps.length * cw / ch * (hi (xs) - lo (xs) + cw) / (hi (ys) - lo (ys) + ch)) || Math.ceil (Math.sqrt (ps.length))));
      const n = Math.min (cols, ps.length), rows = Math.ceil (ps.length / n), cx = (lo (xs) + hi (xs)) / 2, cy = (lo (ys) + hi (ys)) / 2;
      ps.sort ((a, b) => (Math.abs (a.y - b.y) > ch / 2 ? a.y - b.y : a.x - b.x));
      ps.forEach ((q, i) => { q.x = snapV (clamp (cx + (i % n - (n - 1) / 2) * cw, 0, W, q.x)); q.y = snapV (clamp (cy + (Math.floor (i / n) - (rows - 1) / 2) * ch, 0, design.unit.height * U, q.y)); });
    }
    commit();
  }
  /** AUTO-TIDY (one undo step): rows of controls lined up on one centre line, their gaps evened out when
      they are nearly even already, columns lined up across rows, everything on the half-millimetre grid and
      clear of the rack ears. Section boxes, text, lines and locked parts stay where they are. */
  function tidy () {
    const H = design.unit.height * U, x0 = design.unit.ears === "none" ? 4 : EAR + 3, x1 = W - x0;
    const movable = design.parts.filter ((q) => !q.lock && !["box", "label", "line", "plate", "vent", "screw"].includes (q.type));
    // Rows: parts whose middles are within 4 mm (or a third of the bigger one) of the row's
    const rows = [];
    for (const q of movable.slice().sort ((a, b) => a.y - b.y)) {
      const row = rows.find ((r) => Math.abs (r.y - q.y) < Math.max (4, Math.min (r.h, extent (q).h) / 3));
      if (row) { row.parts.push (q); row.y = row.parts.reduce ((t, p) => t + p.y, 0) / row.parts.length; row.h = Math.max (row.h, extent (q).h); }
      else rows.push ({ y: q.y, h: extent (q).h, parts: [q] });
    }
    for (const r of rows) {
      if (r.parts.length < 2) continue;
      const mid = r.parts.map ((q) => q.y).sort ((a, b) => a - b)[Math.floor (r.parts.length / 2)];
      for (const q of r.parts) q.y = mid;
      // Nearly even gaps (spread under 35 %) become exactly even
      const ps = r.parts.slice().sort ((a, b) => a.x - b.x);
      if (ps.length >= 3) {
        const gaps = ps.slice (1).map ((q, i) => (q.x - extent (q).w / 2) - (ps[i].x + extent (ps[i]).w / 2));
        const mean = gaps.reduce ((t, g) => t + g, 0) / gaps.length, sd = Math.sqrt (gaps.reduce ((t, g) => t + (g - mean) * (g - mean), 0) / gaps.length);
        if (mean > 0 && sd / mean < 0.35) { let at = ps[0].x - extent (ps[0]).w / 2; for (const q of ps) { q.x = at + extent (q).w / 2; at += extent (q).w + mean; } }
      }
    }
    // Columns: middles within 3 mm across different rows line up on their average
    const cols = [];
    for (const q of movable.slice().sort ((a, b) => a.x - b.x)) {
      const c = cols.find ((k) => Math.abs (k.x - q.x) < 3);
      if (c) { c.parts.push (q); c.x = c.parts.reduce ((t, p) => t + p.x, 0) / c.parts.length; } else cols.push ({ x: q.x, parts: [q] });
    }
    for (const c of cols) if (c.parts.length > 1) for (const q of c.parts) q.x = c.x;
    for (const q of movable) {
      const e = extent (q);
      q.x = Math.round (clamp (q.x, x0 + e.w / 2, x1 - e.w / 2, q.x) * 2) / 2;
      q.y = Math.round (clamp (q.y, 2 + e.h / 2, H - 2 - e.h / 2, q.y) * 2) / 2;
    }
    commit();
  }

  // ---------------------------------------------------------------------------------------------------
  // On the panel: every part in a list (front last, as drawn) - pick one that is buried under another, lock
  // it, move it forward or back
  function refreshLayers () {
    const list = $("layers"); if (!list) return;
    list.replaceChildren();
    $("layers-count").textContent = design.parts.length ? design.parts.length + " part" + (design.parts.length === 1 ? "" : "s") + ", the front one last." : "Nothing on the panel yet.";
    design.parts.forEach ((p, i) => {
      const li = document.createElement ("li");
      if (selected.includes (p.id)) li.className = "on";
      const pickB = document.createElement ("button"); pickB.type = "button"; pickB.className = "d-layer-name"; pickB.textContent = partName (p);
      pickB.title = "Select it (Shift or Ctrl: add to the selection)";
      pickB.addEventListener ("click", (e) => {
        if (e.shiftKey || e.ctrlKey || e.metaKey) selected = selected.includes (p.id) ? selected.filter ((id) => id !== p.id) : selected.concat (p.id);
        else selected = [p.id];
        render(); props();
      });
      const mk = (label, title, fn, pressed) => { const b = document.createElement ("button"); b.type = "button"; b.textContent = label; b.title = title; b.setAttribute ("aria-label", title + ": " + partName (p));
        if (pressed !== undefined) b.setAttribute ("aria-pressed", String (pressed)); b.addEventListener ("click", fn); return b; };
      li.append (pickB,
        mk (p.lock ? "Locked" : "Lock", p.lock ? "Unlock it" : "Lock it (it can't be dragged)", () => { p.lock = !p.lock; if (!p.lock) delete p.lock; commit(); }, !!p.lock),
        mk ("↑", "Back one", () => { if (i > 0) { [design.parts[i - 1], design.parts[i]] = [design.parts[i], design.parts[i - 1]]; commit(); } }),
        mk ("↓", "Forward one", () => { if (i < design.parts.length - 1) { [design.parts[i + 1], design.parts[i]] = [design.parts[i], design.parts[i + 1]]; commit(); } }));
      list.appendChild (li);
    });
  }

  // ---------------------------------------------------------------------------------------------------
  // Check the design: what would look wrong or be awkward on a real panel - parts off the panel or under the
  // ears, controls on top of each other, knobs too close for fingers, print too small to read, controls with
  // no name. Each problem selects its parts when clicked; "Fix" mends what can be mended (one undo step).
  const CONTROLS = ["knob", "toggle", "button", "slider", "selector", "jack", "vu", "ladder", "display", "lamp", "led"];
  function designIssues () {
    const H = design.unit.height * U, x0 = design.unit.ears === "none" ? 2 : EAR + 1, x1 = W - x0, out = [];
    const ctl = design.parts.filter ((q) => CONTROLS.includes (q.type));
    for (const q of design.parts) {
      const b = bounds (q);
      if (b.x < x0 - 0.2 || b.x + b.w > x1 + 0.2 || b.y < -0.2 || b.y + b.h > H + 0.2)
        out.push ({ level: "bad", ids: [q.id], text: partName (q) + ": off the panel" + (b.x < x0 || b.x + b.w > x1 ? " (or under the rack ears)" : ""), fix: "inside" });
      if (q.type === "label" && q.size < 2.2) out.push ({ level: "warn", ids: [q.id], text: partName (q) + ": text " + fmt (q.size) + " mm tall - hard to read on a real panel", fix: "text" });
      if (["knob", "slider", "selector"].includes (q.type) && q.labelSize !== undefined && q.labelSize < 2) out.push ({ level: "warn", ids: [q.id], text: partName (q) + ": its label is very small", fix: "text" });
      if (["knob", "slider", "selector"].includes (q.type) && !String (q.text || "").trim()) out.push ({ level: "info", ids: [q.id], text: TYPES[q.type].label + " with no name - what does it do?" });
    }
    for (let i = 0; i < ctl.length; ++i)
      for (let j = i + 1; j < ctl.length; ++j) {
        const a = bounds (ctl[i]), b = bounds (ctl[j]);
        const ox = Math.min (a.x + a.w, b.x + b.w) - Math.max (a.x, b.x), oy = Math.min (a.y + a.h, b.y + b.h) - Math.max (a.y, b.y);
        if (ox > 0.4 && oy > 0.4) { out.push ({ level: "bad", ids: [ctl[i].id, ctl[j].id], text: partName (ctl[i]) + " and " + partName (ctl[j]) + " overlap", fix: "apart" }); continue; }
        const round = (q) => q.type === "knob" || q.type === "selector";
        if (round (ctl[i]) && round (ctl[j])) {
          const gap = Math.hypot (ctl[i].x - ctl[j].x, ctl[i].y - ctl[j].y) - (ctl[i].w + ctl[j].w) / 2;
          if (gap < 4) out.push ({ level: "warn", ids: [ctl[i].id, ctl[j].id], text: partName (ctl[i]) + " and " + partName (ctl[j]) + ": " + fmt (Math.max (0, gap)) + " mm apart - tight for fingers (4 mm or more)", fix: "apart" });
        }
      }
    return out;
  }
  function showIssues () {
    const list = $("issues"), all = designIssues();
    list.replaceChildren();
    $("check-fix").disabled = !all.some ((it) => it.fix);
    $("check-msg").textContent = all.length ? all.length + " thing" + (all.length === 1 ? "" : "s") + " to look at." : "Nothing to fix: it would build.";
    for (const it of all.slice (0, 40)) {
      const li = document.createElement ("li"); li.className = "d-issue " + it.level;
      const b = document.createElement ("button"); b.type = "button"; b.textContent = it.text; b.title = "Select " + (it.ids.length > 1 ? "them" : "it");
      b.addEventListener ("click", () => { selected = it.ids.filter (byId); render(); props(); });
      li.appendChild (b); list.appendChild (li);
    }
  }
  function fixIssues () {
    const H = design.unit.height * U, x0 = design.unit.ears === "none" ? 2 : EAR + 1, x1 = W - x0;
    for (let pass = 0; pass < 30; ++pass) {
      const all = designIssues().filter ((it) => it.fix);
      if (!all.length) break;
      for (const it of all) {
        const ps = it.ids.map (byId).filter (Boolean);
        if (it.fix === "text") for (const q of ps) { if (q.type === "label") q.size = Math.max (q.size, 2.4); if (q.labelSize !== undefined) q.labelSize = Math.max (q.labelSize, 2.2); }
        if (it.fix === "apart" && ps.length === 2) {
          // the unlocked one (the smaller, if both are) steps away along the axis it overlaps least on
          const [a, b] = ps[1].lock || (!ps[0].lock && extent (ps[0]).w * extent (ps[0]).h < extent (ps[1]).w * extent (ps[1]).h) ? [ps[1], ps[0]] : [ps[0], ps[1]];
          if (b.lock) continue;
          const ea = bounds (a), eb = bounds (b), need = (a.type === "knob" || a.type === "selector") && (b.type === "knob" || b.type === "selector") ? 4 : 1;
          const dx = (ea.w + eb.w) / 2 + need - Math.abs (b.x - a.x), dy = (ea.h + eb.h) / 2 + need - Math.abs (b.y - a.y);
          // (sideways, unless the panel is tall enough to step down and that is the shorter way: a 1U or 2U
          // panel has no room to spare up and down, and the edge would only push it back)
          const room = H - extent (b).h - 4 > 2 * extent (b).h;
          if (dx <= dy || !room) b.x += (b.x >= a.x ? 1 : -1) * (dx + 0.25); else b.y += (b.y >= a.y ? 1 : -1) * (dy + 0.25);
        }
        for (const q of ps) {   // (and everything back inside, "inside" or not)
          if (q.lock) continue;
          const e = extent (q);
          q.x = clamp (q.x, x0 + e.w / 2, Math.max (x0 + e.w / 2, x1 - e.w / 2), q.x);
          q.y = clamp (q.y, e.h / 2, Math.max (e.h / 2, H - e.h / 2), q.y);
        }
      }
    }
    commit(); showIssues();
    toast ("Mended what it could - Undo takes it back");
  }

  // ---------------------------------------------------------------------------------------------------
  // Ideas: six takes on the current design - other colours and finishes, other knobs - to click and keep
  const IDEA_LOOKS = [
    ["#16171a", "#e8e8ea", "#ff8a2a", "anodised"], ["#c9c6bd", "#1a1a1a", "#c0392b", "paint"], ["#2d4a6b", "#f2f2f2", "#ffcc33", "anodised"],
    ["#b4b6ba", "#141414", "#3aa0ff", "brushed"], ["#0e2a1f", "#d9e8d0", "#46e070", "hammertone"], ["#5a3721", "#f1e2c6", "#ffb020", "walnut"],
    ["#1a1b1e", "#e6e6e6", "#ff3b30", "carbon"], ["#e9e2cf", "#2b2b2b", "#1f6fb2", "enamel"], ["#7a1f1f", "#f4e9d0", "#ffd27a", "enamel"],
    ["#3b3f45", "#f0f0f0", "#8fd3ff", "wrinkle"], ["#a9abaf", "#101010", "#e0412e", "sandblast"], ["#26282c", "#d8c49a", "#d8c49a", "satin"],
    ["#5b6b4a", "#f3eedf", "#ffb020", "powder"], ["#0f1c2e", "#9fd0ff", "#56c8f5", "gloss"], ["#c9a24a", "#1c140a", "#1c140a", "gold"] ];
  const IDEA_KNOBS = ["tophat", "knurled", "fluted", "ribbed", "matte", "capblue", "capred", "capwhite", "chicken"];
  function ideas () {
    const grid = $("ideas-grid"); grid.replaceChildren();
    const looks = IDEA_LOOKS.slice().sort (() => Math.random() - 0.5).filter ((l, i, all) => all.findIndex ((m) => m[3] === l[3]) === i).slice (0, 6);   // (six different finishes)
    looks.forEach ((l, i) => {
      const d = JSON.parse (JSON.stringify (design));
      [d.unit.colour, d.unit.ink, d.unit.accent, d.unit.finish] = l;
      if (FINISH[l[3]] && FINISH[l[3]][1]) d.unit.colour = FINISH[l[3]][1];
      const k = IDEA_KNOBS[Math.floor (Math.random() * IDEA_KNOBS.length)];
      if (i > 0) for (const q of d.parts) if (q.type === "knob" && KNOBS.includes (q.style)) q.style = k;   // (the first keeps its knobs)
      const b = document.createElement ("button"); b.type = "button"; b.className = "d-tpl";
      b.appendChild (designThumb (d, 150));
      const cap = document.createElement ("span"); cap.textContent = FINISH[l[3]] ? FINISH[l[3]][0] : l[3]; b.appendChild (cap);
      b.setAttribute ("aria-label", "Use this take: " + cap.textContent);
      b.addEventListener ("click", () => { replaceDesign (sanitize (d)); refreshPickers(); toast ("Taken - Undo takes it back"); });
      grid.appendChild (b);
    });
    $("ideas").textContent = "Six more";   // (another click: six others)
  }

  /** COPY FOR AI: the design as readable JSON under a short brief an AI can follow (any chat assistant, free
      tier included - nothing here talks to one); its reply pasted back goes through sanitize() like any share
      code, so it can only ever change the design, within the same limits. */
  function aiBrief () {
    const H = design.unit.height * U, round = (v) => (typeof v === "number" ? Math.round (v * 10) / 10 : v);
    const parts = design.parts.map ((p) => { const o = {}; for (const k in p) if (k !== "id") o[k] = round (p[k]); return o; });
    const lists = { knobStyles: KNOBS, toggleStyles: TOGGLES, buttonStyles: BUTTONS, vuStyles: VUS, jackStyles: JACKS, finishes: FINISHES, marks: MARKS, inks: INKS,
      ledShapes: LED_SHAPES, dials: DIALS, ladderPalettes: PALETTES, displayKinds: DISPLAYS, textLooks: LOOKS, boxLines: LINES, ventKinds: VENTS, faderCaps: FADERS,
      lampLenses: LAMPS, plates: PLATES, trims: TRIMS, twoTones: TWO_TONES, soundBlocks: DSP_TYPES };
    const brief = [
      "You are polishing a rack-unit faceplate designed in the ENH Master Rack Unit Designer. The design is the JSON below.",
      "Units are millimetres. The panel is " + W + " mm wide and " + H + " mm tall (" + design.unit.height + "U); x runs left to right, y top to bottom, and every part's x, y is its CENTRE.",
      design.unit.ears === "none" ? "It has no rack ears." : "Keep parts clear of the rack ears: x between " + (EAR + 3) + " and " + (W - EAR - 3) + ".",
      "Improve the layout and look: align rows and columns, space things evenly, group related controls (a section box around them helps), keep labels readable, nothing overlapping.",
      "Keep every part's type, text and wiring (ctl) unless asked; you may change positions, sizes, styles, colours (#rrggbb) and the unit's look.",
      "Only use values from these lists:",
      ...Object.entries (lists).map (([k, v]) => "  " + k + ": " + v.join (", ")),   // (plain lines: the design below is the only JSON)
      "Reply with ONLY the complete JSON, same shape, nothing before or after it.",
    ].join ("\n");
    return brief + "\n\n" + JSON.stringify ({ unit: design.unit, knobs: design.knobs, dsp: design.dsp, parts }, null, 1);
  }
  function bindAi () {
    const dlg = $("ai-dialog"); if (!dlg) return;
    $("ai-open").addEventListener ("click", () => { $("ai-out").value = aiBrief(); $("ai-in").value = ""; $("ai-msg").textContent = ""; dlg.showModal(); });
    $("ai-copy").addEventListener ("click", async () => { try { await navigator.clipboard.writeText ($("ai-out").value); $("ai-msg").textContent = "Copied. Paste it into your AI chat."; }
      catch (_) { $("ai-out").select(); $("ai-msg").textContent = "Select all and copy (Ctrl+C)."; } });
    $("ai-apply").addEventListener ("click", () => {
      const t = $("ai-in").value, a = t.indexOf ("{"), b = t.lastIndexOf ("}");
      if (t.length > MAX_JSON || a < 0 || b <= a) { $("ai-msg").textContent = "That doesn't contain a design (JSON between { and })."; return; }
      let obj; try { obj = JSON.parse (t.slice (a, b + 1)); } catch (_) { $("ai-msg").textContent = "The reply isn't valid JSON - ask the AI to send only the JSON."; return; }
      const d = sanitize (obj);
      if (!d.parts.length) { $("ai-msg").textContent = "The reply had no parts - nothing changed."; return; }
      replaceDesign (d); $("ai-msg").textContent = "Applied (Undo takes it back)."; setTimeout (() => dlg.close(), 700);
    });
    $("ai-close").addEventListener ("click", () => dlg.close());
  }

  /** The selection moved as one so its middle is the panel's (across, or down). */
  function centreOnPanel (axis) {
    const ps = selected.map (byId).filter ((q) => q && !q.lock); if (!ps.length) return;
    const b = ps.map (bounds), lo = Math.min (...b.map ((r) => axis === "x" ? r.x : r.y)), hi = Math.max (...b.map ((r) => axis === "x" ? r.x + r.w : r.y + r.h));
    const d = (axis === "x" ? W : design.unit.height * U) / 2 - (lo + hi) / 2;
    for (const q of ps) q[axis] += d;
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
  function svgPoint (e) {   // (in the plate's own millimetres, whichever way up it is shown)
    const pt = svg.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY;
    return pt.matrixTransform ((faceGroup && faceGroup.ownerSVGElement === svg ? faceGroup : svg).getScreenCTM().inverse());
  }
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
    const pt = svgPoint (e), H = design.unit.height * U;
    let dx = pt.x - drag.start.x, dy = pt.y - drag.start.y;
    if (e.shiftKey && drag.orig.length) {   // Shift: the move held to 0, 45 or 90 degrees (angle snapping)
      const a = Math.atan2 (dy, dx), step = Math.PI / 4, snapped = Math.round (a / step) * step, len = Math.hypot (dx, dy) * Math.cos (a - snapped);
      dx = len * Math.cos (snapped); dy = len * Math.sin (snapped);
      if (Math.abs (dx) < 1e-6) dx = 0; if (Math.abs (dy) < 1e-6) dy = 0;
    }
    if (Math.abs (dx) + Math.abs (dy) > 0.2) drag.moved = true;
    // Smart guides: the moving parts' edges and middle snap to the other parts' (and the panel's middle)
    // when they come within ~1.2 mm on screen; the lines they snap to are drawn while dragging (Alt: off)
    drag.guides = [];
    if (drag.orig.length && !e.altKey) {
      const moving = new Set (drag.orig.map ((o) => o.q.id)), tol = 1.2 / Math.max (0.4, zoom);
      const ob = drag.orig.map ((o) => { const e2 = extent (o.q); return { l: o.x - e2.w / 2 + dx, r: o.x + e2.w / 2 + dx, t: o.y - e2.h / 2 + dy, b: o.y + e2.h / 2 + dy }; });
      const box = { l: Math.min (...ob.map ((b) => b.l)), r: Math.max (...ob.map ((b) => b.r)), t: Math.min (...ob.map ((b) => b.t)), b: Math.max (...ob.map ((b) => b.b)) };
      const mine = { x: [box.l, (box.l + box.r) / 2, box.r], y: [box.t, (box.t + box.b) / 2, box.b] };
      const lines = { x: [W / 2], y: [H / 2] };
      for (const q of design.parts) { if (moving.has (q.id)) continue; const b = bounds (q); lines.x.push (b.x, b.x + b.w / 2, b.x + b.w); lines.y.push (b.y, b.y + b.h / 2, b.y + b.h); }
      for (const axis of ["x", "y"]) {
        let best = null;
        for (const m of mine[axis]) for (const l of lines[axis]) { const d = l - m; if (Math.abs (d) < tol && (!best || Math.abs (d) < Math.abs (best.d))) best = { d, at: l }; }
        if (best) { if (axis === "x") dx += best.d; else dy += best.d; drag.guides.push ({ axis, at: best.at }); }
      }
    }
    const guided = drag.guides.length > 0;
    for (const o of drag.orig) { o.q.x = clamp (guided ? o.x + dx : snapV (o.x + dx), 0, W, o.x); o.q.y = clamp (guided ? o.y + dy : snapV (o.y + dy), 0, H, o.y); }
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
    if (on && !screenRaf) screenRaf = requestAnimationFrame (animateScreens);
    clearInterval (blinkTimer); if (on) blinkTimer = setInterval (() => { if (design.parts.some ((q) => q.blink && q.on)) render(); }, 250); const b = document.getElementById ("play"); b.setAttribute ("aria-pressed", String (on)); b.textContent = on ? "Edit" : "Play"; document.body.classList.toggle ("playing", on); render(); }


  // ---------------------------------------------------------------------------------------------------
  // Screens: a display's moving picture - drawn still in the editor, alive in Play (following the Sound tab's
  // song when one plays: its level and its waveform). A scope's phase trace, colour lines (warm when it's
  // quiet, cool when it's loud), a warped wireframe cube, or one of your own (shape, style, colours, speed).
  const SCREEN_KINDS = ["wave", "bars", "spectrum", "scope", "colour", "cube", "custom"];
  function screenColour (p, x01, t, i) {
    if (p.kind === "colour" || (p.kind === "custom" && p.scrColours === "warmcool")) { const h = 20 + 200 * x01; return `hsl(${h.toFixed (0)} 95% 62%)`; }
    if (p.kind === "cube" || (p.kind === "custom" && p.scrColours === "rainbow")) return `hsl(${((x01 * 300 + t * 40 + i * 25) % 360).toFixed (0)} 90% 62%)`;
    return p.backlit ? "#0a0b0c" : p.colour;
  }
  function screenShape (shape, ph, i) {
    const f = ph - Math.floor (ph);
    return shape === "square" ? (f < 0.5 ? 1 : -1) : shape === "saw" ? 2 * f - 1 : shape === "pulse" ? (f < 0.15 ? 1 : -0.2)
         : shape === "noise" ? Math.sin (i * 12.9898 + Math.floor (ph * 8) * 78.233) * 0.9 : Math.sin (2 * Math.PI * ph);
  }
  function drawScreen (g, p, t, lv, wave) {
    g = el ("svg", { x: -p.w / 2 + 1, y: -p.h / 2 + 1, width: p.w - 2, height: p.h - 2, viewBox: `${-p.w / 2 + 1} ${-p.h / 2 + 1} ${p.w - 2} ${p.h - 2}`, overflow: "hidden" }, g);   // (kept inside its glass)
    const x0 = -p.w / 2 + 2.5, span = p.w - 5, y0 = -p.h / 2 + 2.5, hgt = p.h - 5, cx = 0, cy = 0, n = 64;
    const line = (pts, col, w = 0.5, op = 0.95) => { if (pts.length < 2) return; el ("path", { d: "M" + pts.map ((q) => q[0].toFixed (2) + " " + q[1].toFixed (2)).join (" L "), fill: "none", stroke: col, "stroke-width": w, opacity: op, "stroke-linejoin": "round" }, g); };
    const glow = (pts, col) => { line (pts, col, 1.6, 0.18); line (pts, col, 0.5, 0.95); };
    const w = (i) => wave && wave.length ? wave[Math.floor (i / n * (wave.length - 16))] * 3 : Math.sin (i * 0.45 + t * 3) * Math.cos (i * 0.11 + t) * (0.4 + lv);
    const kind = p.kind;
    if (kind === "wave") { const pts = []; for (let i = 0; i <= n; ++i) pts.push ([x0 + span * i / n, Math.max (-1, Math.min (1, w (i))) * hgt * 0.4]); glow (pts, screenColour (p, 0, t, 0)); }
    else if (kind === "bars" || kind === "spectrum") {
      const m = kind === "bars" ? 8 : 24, bw = span / m;
      for (let i = 0; i < m; ++i) {
        const base = kind === "bars" ? 0.25 + 0.4 * Math.abs (Math.sin (i * 1.3 + 0.4 + t * 2)) : 0.55 * Math.exp (-i / 14) * (0.7 + 0.3 * Math.sin (i * 2.1 + t * 3));
        const h = hgt * Math.min (0.95, base * (0.6 + 0.8 * lv));
        el ("rect", { x: x0 + i * bw + bw * 0.15, y: y0 + hgt - h, width: bw * 0.7, height: h, fill: screenColour (p, i / m, t, i), opacity: 0.9 }, g);
      }
    }
    else if (kind === "scope") {   // the phase trace: the signal against itself a moment later - a scope's X-Y
      const pts = [], k = 9;
      for (let i = 0; i <= 120; ++i) {
        const a = i / 120 * Math.PI * 2;
        const x = wave && wave.length > 200 ? wave[i * 3] * 3 : Math.sin (3 * a + t * 0.7) * (0.6 + 0.3 * lv);
        const y = wave && wave.length > 200 ? wave[i * 3 + k] * 3 : Math.sin (2 * a + t * 0.45) * (0.6 + 0.3 * lv);
        pts.push ([cx + Math.max (-1, Math.min (1, x)) * span * 0.45, cy + Math.max (-1, Math.min (1, y)) * hgt * 0.45]);
      }
      for (let gx = 1; gx < 8; ++gx) el ("line", { x1: x0 + span * gx / 8, y1: y0, x2: x0 + span * gx / 8, y2: y0 + hgt, stroke: p.colour, "stroke-width": 0.12, opacity: 0.25 }, g);
      glow (pts, screenColour (p, 0, t, 0));
    }
    else if (kind === "colour") {   // strands of light: warm to cool across, swaying with the level
      for (let j = 0; j < 6; ++j) {
        const pts = [], yb = y0 + hgt * (j + 0.5) / 6;
        for (let i = 0; i <= n; ++i) pts.push ([x0 + span * i / n, yb + Math.sin (i * 0.3 + t * 1.7 + j) * hgt * (0.03 + 0.07 * lv)]);
        for (let i = 0; i < n; i += 8) line (pts.slice (i, i + 9), screenColour (p, i / n, t, j), 0.7, 0.9);
      }
    }
    else if (kind === "cube") {   // a wireframe cube, turning, bent by the level
      const ry = t * 0.8, rx = t * 0.5 + 0.4, r = Math.min (span, hgt) * 0.27, gridN = 5;
      const P = (x, y, z) => { const warp = 1 + (0.06 + 0.16 * lv) * Math.sin (x * 2.1 + y * 1.7 + z * 2.3 + t * 2);
        x *= warp; y *= warp; z *= warp;
        const x1 = Math.cos (ry) * x + Math.sin (ry) * z, z1 = -Math.sin (ry) * x + Math.cos (ry) * z;
        const y1 = Math.cos (rx) * y - Math.sin (rx) * z1, z2 = Math.sin (rx) * y + Math.cos (rx) * z1, s = 3 / (3 + z2);
        return [cx + x1 * r * s, cy + y1 * r * s]; };
      const faces = [[1, 0, 0, 0, 1, 0, 0, 0, 1], [-1, 0, 0, 0, 1, 0, 0, 0, 1], [0, 1, 0, 1, 0, 0, 0, 0, 1], [0, -1, 0, 1, 0, 0, 0, 0, 1], [0, 0, 1, 1, 0, 0, 0, 1, 0], [0, 0, -1, 1, 0, 0, 0, 1, 0]];
      faces.forEach ((F, fi) => { for (let a = 0; a <= gridN; ++a) for (const dir of [0, 1]) {
        const pts = []; const u0 = -1 + 2 * a / gridN;
        for (let b = 0; b <= 10; ++b) { const v = -1 + 2 * b / 10, uu = dir ? u0 : v, vv = dir ? v : u0; pts.push (P (F[0] + F[3] * uu + F[6] * vv, F[1] + F[4] * uu + F[7] * vv, F[2] + F[5] * uu + F[8] * vv)); }
        line (pts, screenColour (p, a / gridN, t, fi), 0.28, 0.8); } });
    }
    else if (kind === "custom") {   // yours: a shape, drawn as lines, dots, bars or rings, in one colour, warm to cool, or a rainbow
      const speed = 0.2 + (p.scrSpeed ?? 5) * 0.3, shape = p.scrShape || "sine", style = p.scrStyle || "lines", amp = 0.35 + 0.6 * lv;
      if (style === "rings") {
        for (let k = 1; k <= 6; ++k) { const rr = Math.min (span, hgt) * 0.08 * k * (1 + 0.25 * screenShape (shape, t * speed * 0.3 + k * 0.17, k) * amp);
          el ("circle", { cx, cy, r: Math.max (0.3, rr), fill: "none", stroke: screenColour (p, k / 6, t, k), "stroke-width": 0.45, opacity: 0.9 }, g); }
      } else {
        const pts = [];
        for (let i = 0; i <= n; ++i) pts.push ([x0 + span * i / n, screenShape (shape, i / n * 3 + t * speed * 0.5, i) * hgt * 0.4 * amp]);
        if (style === "lines") for (let i = 0; i < n; i += 8) line (pts.slice (i, i + 9), screenColour (p, i / n, t, 0), 0.6, 0.95);
        else if (style === "dots") pts.forEach ((q, i) => { if (i % 2 === 0) el ("circle", { cx: q[0], cy: q[1], r: 0.55, fill: screenColour (p, i / n, t, 0) }, g); });
        else pts.forEach ((q, i) => { if (i % 3 === 0) el ("rect", { x: q[0] - 0.6, y: Math.min (0, q[1]), width: 1.2, height: Math.max (0.3, Math.abs (q[1])), fill: screenColour (p, i / n, t, 0), opacity: 0.9 }, g); });
      }
    }
  }
  let screenRaf = 0;
  function animateScreens () {   // in Play: every screen redrawn about 30 times a second
    if (!play) { screenRaf = 0; return; }
    const snd = window.ENHSound, lv = snd && snd.levelNow ? snd.levelNow() : 0, wave = snd && snd.waveNow ? snd.waveNow() : null;
    const t = performance.now() / 1000;
    for (const g of svg.querySelectorAll (".d-scr")) {
      const p = byId (Number (g.getAttribute ("data-scr"))); if (!p) continue;
      g.replaceChildren(); drawScreen (g, p, t, lv > 0 ? lv : 0.35 + 0.25 * Math.sin (t * 2.3), wave);
    }
    screenRaf = setTimeout (() => requestAnimationFrame (animateScreens), 30);
  }

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
    refreshLayers();
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
      if (key === "rot" && snap) i.step = 15;   // (angle snapping: its arrows step 15 degrees, and a typed angle snaps when done)
      i.addEventListener ("input", () => { if (i.value === "") return; const v = clamp (i.value, lo, hi, p[key]); for (const q of ps) { q[key] = v; if (["knob", "selector", "led", "lamp", "screw"].includes (q.type) && key === "w") q.h = v; } if (after) after(); render(); save(); });
      i.addEventListener ("change", () => { if (key === "rot" && snap && i.value !== "") { const v = Math.round (clamp (i.value, lo, hi, 0) / 15) * 15; i.value = v; for (const q of ps) q.rot = v; } commit(); }); field (body, label, i); };
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
    if (sameType && p.type === "display") { ask ("kind", "Shows", DISPLAYS, { wave: "A waveform", bars: "Bars", spectrum: "A spectrum", digits: "Digits (7-segment)", text: "Text", blank: "Nothing",
                                                                 scope: "A scope (phosphor trace)", colour: "Colour lines (warm to cool)", cube: "A 3D cube (warped)", custom: "Your own screen" });
      if (p.kind === "custom") {
        ask ("scrShape", "Shape", SCR_SHAPES, { sine: "Sine", square: "Square", saw: "Saw", noise: "Noise", pulse: "Pulse" });
        ask ("scrStyle", "Drawn as", SCR_STYLES, { lines: "Lines", dots: "Dots", bars: "Bars", rings: "Rings" });
        ask ("scrColours", "Colours", SCR_COLOURS, { mono: "Its colour", warmcool: "Warm to cool", rainbow: "Rainbow" });
        num ("scrSpeed", "Speed", 0, 10, 0.5);
      }
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
    // Controls: what this part turns in the unit's sound (the Sound tab's chain)
    if (!many && ["knob", "slider", "toggle", "button", "selector"].includes (p.type)) {
      const sel = document.createElement ("select"), opt = (v, t) => { const o = document.createElement ("option"); o.value = v; o.textContent = t; sel.appendChild (o); };
      opt ("", design.dsp.chain.length ? "Nothing (just for looks)" : "Nothing - add blocks in the Sound tab");
      design.dsp.chain.forEach ((b, k) => {
        const name = (k + 1) + ". " + DSP_BLOCKS[b.b][0];
        if (p.type === "toggle" || p.type === "button") opt (k + ".on", name + ": in / out");
        for (const key in DSP_BLOCKS[b.b][1]) opt (k + "." + key, name + ": " + DSP_BLOCKS[b.b][1][key][0]);
      });
      sel.value = p.ctl || "";
      sel.addEventListener ("change", () => { if (ctlOk (sel.value, design.dsp.chain)) p.ctl = sel.value; else delete p.ctl; commit(); });
      field (body, "Controls", sel);
    }
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
  const T = (unit, parts, dsp) => ({ v: 1, unit: Object.assign (blank().unit, unit), parts, dsp: dsp || { chain: [] } });
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

  /* More presets, each with its sound (a chain, its knobs wired to it) - one line each. U (unit look),
     K: knobs as [label, "block.param", value 0-100], C: the chain [[type, params]], X: extras (meter, switches). */
  const R = (u, K, C, X = {}) => {
    const H = (u.height || 1) * U, n = K.length, size = X.size || (u.height > 1 ? 26 : 16), left = X.left ?? 80;
    const meterW = Math.min (80, H * 1.9), span = X.span ?? (X.meter ? W - 100 - meterW / 2 - 16 - size * 0.9 - left : 340);   // (a meter: the knobs stop short of it)
    const y = X.y ?? H / 2 + (u.height > 1 ? 2 : 1.5);
    const parts = K.map (([t, ctl, v], i) => ({ type: X.selector === i ? "selector" : "knob", x: left + (n > 1 ? span * i / (n - 1) : span / 2), y, w: size, h: size,
      style: X.selector === i ? "chicken" : X.style || "ribbed", value: v ?? 50, text: t, scale: true, min: 0, max: 10, steps: 10, nums: X.nums || "ends",
      marks: X.marks || "ticks", ctl, stops: X.stops, ring: !!X.ring, ringColour: u.accent || "#ff8a2a" }));
    if (X.meter) parts.push ({ type: "vu", x: W - 100, y: H / 2, w: Math.min (80, H * 1.9), h: Math.min (44, H * 0.8), style: X.meter, value: 45, text: X.meterText || "VU" });
    if (X.ladder) parts.push ({ type: "ladder", x: W - 42, y: H / 2, w: 5, h: H * 0.7, segments: 12, value: 60, palette: X.ladder });
    parts.push ({ type: "toggle", x: W - 26, y: H / 2, style: X.toggle || "rockerred", on: true, text: "POWER", ctl: "0.on" });
    if (X.box) parts.push ({ type: "box", x: left + span / 2, y: y + 1, w: span + size + 20, h: H - 10, text: X.box, round: 3 });
    return T (u, parts, { chain: C.map (([b, p]) => ({ b, on: true, p: p || {} })) });
  };
  Object.assign (templates, {
    "Vari-mu compressor": () => R ({ name: "VARIABLE MU", model: "EM-VM", height: 2, colour: "#5b5f63", ink: "#f4f1ea", finish: "hammertone" },
      [["THRESHOLD", "0.threshold", 45], ["ATTACK", "0.attack", 40], ["RELEASE", "0.release", 55], ["GAIN", "0.makeup", 40]], [["comp", { ratio: 2.5 }], ["drive", { drive: 6, shape: 0, mix: 30 }]], { style: "fluted", meter: "cream", meterText: "GAIN REDUCTION" }),
    "Channel strip": () => R ({ name: "CHANNEL STRIP", model: "EM-CS", height: 1, colour: "#1c2230", ink: "#e9edf4" },
      [["TRIM", "2.gain", 50], ["LOW", "0.low", 55], ["MID", "0.mid", 50], ["HIGH", "0.high", 60], ["THRESH", "1.threshold", 45], ["RATIO", "1.ratio", 30]], [["eq"], ["comp"], ["gain"]], { style: "capblue", size: 14 }),
    "Tube saturator": () => R ({ name: "TUBE SATURATOR", model: "EM-T12", height: 1, colour: "#2a1a12", ink: "#f1d9b0", finish: "enamel", accent: "#ff8a2a" },
      [["DRIVE", "0.drive", 45], ["TONE", "0.tone", 60], ["BLEND", "0.mix", 60], ["OUTPUT", "1.gain", 45]], [["drive", { shape: 0 }], ["gain"]], { style: "tophat", ladder: "amber" }),
    "Tape machine": () => R ({ name: "TAPE MACHINE", model: "EM-15IPS", height: 2, colour: "#3a3d40", ink: "#f2efe6", finish: "brushed" },
      [["INPUT", "0.drive", 40], ["BIAS", "0.tone", 55], ["WOW", "1.time", 10], ["OUTPUT", "2.gain", 50]], [["drive", { shape: 1, mix: 80 }], ["delay", { time: 12, feedback: 0, mix: 8 }], ["gain"]], { style: "knurled", meter: "amber", meterText: "RECORD" }),
    "Spring reverb": () => R ({ name: "SPRING REVERB", model: "EM-SPR", height: 1, colour: "#0f2a1f", ink: "#e6f2ea", finish: "wrinkle" },
      [["DWELL", "0.predelay", 30], ["DECAY", "0.size", 35], ["TONE", "0.damp", 45], ["MIX", "0.mix", 35]], [["room", { size: 1.4, damp: 55 }]], { style: "chicken" }),
    "Hall reverb": () => R ({ name: "CONCERT HALL", model: "EM-480", height: 2, colour: "#1b1e24", ink: "#dfe6f1", accent: "#4fb3ff" },
      [["SIZE", "0.size", 60], ["PRE-DELAY", "0.predelay", 25], ["DAMPING", "0.damp", 40], ["MIX", "0.mix", 30], ["WIDTH", "1.width", 60]], [["room", { size: 4 }], ["width"]], { style: "matte", ring: true }),
    "Digital delay": () => R ({ name: "DIGITAL DELAY", model: "EM-DD3", height: 1, colour: "#e8e6df", ink: "#1a1a1a", finish: "powder", accent: "#d8322b" },
      [["TIME", "0.time", 45], ["REPEATS", "0.feedback", 40], ["TONE", "0.tone", 70], ["MIX", "0.mix", 30]], [["delay"]], { style: "capred", ladder: "red" }),
    "Ping-pong echo": () => R ({ name: "STEREO ECHO", model: "EM-PP", height: 1, colour: "#123040", ink: "#eaf6ff" },
      [["TIME", "0.time", 40], ["FEEDBACK", "0.feedback", 50], ["WIDTH", "1.width", 80], ["MIX", "0.mix", 35]], [["delay"], ["width"]], { style: "capwhite" }),
    "Stereo widener": () => R ({ name: "STEREO IMAGER", model: "EM-W2", height: 1, colour: "#0c0c10", ink: "#c9ccd6", finish: "carbon" },
      [["WIDTH", "0.width", 65], ["LOW CUT", "1.lowf", 20], ["LOW", "1.low", 45], ["OUTPUT", "2.gain", 50]], [["width"], ["eq"], ["gain"]], { style: "matte", ladder: "blue" }),
    "Exciter": () => R ({ name: "AURAL EXCITER", model: "EM-AX", height: 1, colour: "#2d2d30", ink: "#ffd27a", finish: "anodised", accent: "#ffd27a" },
      [["TUNE", "0.freq", 45], ["DRIVE", "0.amount", 40], ["AIR", "1.high", 55]], [["exciter"], ["eq", { highf: 12000 }]], { style: "pointer" }),
    "De-esser": () => R ({ name: "DE-ESSER", model: "EM-DS", height: 1, colour: "#1f2a33", ink: "#eef3f6" },
      [["FREQUENCY", "0.highf", 55], ["AMOUNT", "0.high", 35], ["THRESHOLD", "1.threshold", 50]], [["eq", { high: -4, highf: 7000 }], ["comp", { ratio: 3, attack: 1, release: 60 }]], { style: "ribbed", ladder: "amber" }),
    "Transient designer": () => R ({ name: "TRANSIENT DESIGNER", model: "EM-TD4", height: 1, colour: "#142038", ink: "#e8eefc", accent: "#46e070" },
      [["ATTACK", "0.attack", 70], ["SUSTAIN", "0.release", 35], ["PUNCH", "0.makeup", 50], ["MIX", "0.mix", 70]], [["comp", { threshold: -30, ratio: 4 }]], { style: "capblue", ring: true }),
    "Multiband dynamics": () => R ({ name: "MULTIBAND", model: "EM-MB3", height: 2, colour: "#202124", ink: "#f0f0f0" },
      [["LOW", "0.low", 55], ["MID", "0.mid", 50], ["HIGH", "0.high", 55], ["THRESH", "1.threshold", 45], ["RATIO", "1.ratio", 30], ["OUT", "2.gain", 50]], [["eq"], ["comp"], ["gain"]], { style: "matte", box: "BANDS", meter: "black", meterText: "GR" }),
    "Parallel smasher": () => R ({ name: "PARALLEL SMASHER", model: "EM-NY", height: 1, colour: "#5a0f0f", ink: "#ffe9e0", finish: "candy" },
      [["CRUSH", "0.threshold", 20], ["BLEND", "0.mix", 35], ["COLOUR", "1.drive", 35], ["OUTPUT", "2.gain", 45]], [["comp", { ratio: 20, attack: 0.5, release: 60, makeup: 18 }], ["drive", { shape: 2, mix: 40 }], ["gain"]], { style: "redtrim" }),
    "Lo-fi box": () => R ({ name: "LO-FI BOX", model: "EM-8BIT", height: 1, colour: "#c9b27a", ink: "#2a2112", finish: "patina" },
      [["BANDWIDTH", "0.freq", 35], ["CRUNCH", "1.drive", 60], ["NOISE", "1.mix", 50], ["LEVEL", "2.gain", 50]], [["filter", { mode: 2 }], ["drive", { shape: 2 }], ["gain"]], { style: "chicken" }),
    "Filter sweeper": () => R ({ name: "FILTER SWEEP", model: "EM-FS", height: 1, colour: "#26113d", ink: "#f3e8ff", accent: "#b889ff" },
      [["CUTOFF", "0.freq", 60], ["RESONANCE", "0.q", 35], ["DRIVE", "1.drive", 20]], [["filter"], ["drive", { mix: 40 }]], { style: "capwhite", size: 20, ring: true, selector: 0 }),
    "Guitar amp": () => R ({ name: "BRITISH STACK", model: "EM-JCM", height: 2, colour: "#1a1a1a", ink: "#e9c46a", finish: "tolex", accent: "#e9c46a" },
      [["GAIN", "0.drive", 70], ["BASS", "1.low", 55], ["MIDDLE", "1.mid", 60], ["TREBLE", "1.high", 55], ["PRESENCE", "1.highf", 50], ["MASTER", "2.gain", 40]], [["drive", { shape: 2, mix: 100 }], ["eq"], ["gain"]], { style: "tophat", box: "PREAMP" }),
    "Bass amp": () => R ({ name: "BASS AMPLIFIER", model: "EM-SVT", height: 2, colour: "#2b2e33", ink: "#f2f2f2", finish: "brushed" },
      [["GAIN", "0.drive", 35], ["BASS", "1.low", 60], ["MID", "1.mid", 45], ["TREBLE", "1.high", 50], ["VOLUME", "2.gain", 45]], [["drive", { shape: 0 }], ["eq", { lowf: 80 }], ["gain"]], { style: "knurled", meter: "cream" }),
    "Vocal chain": () => R ({ name: "VOCAL CHAIN", model: "EM-VX", height: 1, colour: "#23201c", ink: "#f6ecd9", finish: "walnut" },
      [["WARMTH", "0.drive", 30], ["PRESENCE", "1.mid", 60], ["COMPRESS", "2.threshold", 45], ["SPACE", "3.mix", 20]], [["drive", { mix: 40 }], ["eq", { midf: 3000 }], ["comp"], ["room", { size: 1.2 }]], { style: "fluted", ladder: "green" }),
    "Drum bus": () => R ({ name: "DRUM BUS", model: "EM-DB", height: 1, colour: "#301b10", ink: "#ffe3c4", finish: "rosewood" },
      [["DRIVE", "0.drive", 40], ["CRUSH", "1.threshold", 40], ["BOOM", "2.low", 60], ["TRANSIENTS", "1.attack", 60], ["MIX", "1.mix", 70]], [["drive", { shape: 1 }], ["comp", { ratio: 4 }], ["eq", { lowf: 60 }]], { style: "capred" }),
    "Mix bus glue": () => R ({ name: "MIX BUS", model: "EM-SSL", height: 1, colour: "#8a8d90", ink: "#101010", finish: "sandblast" },
      [["THRESHOLD", "0.threshold", 55], ["RATIO", "0.ratio", 20], ["ATTACK", "0.attack", 50], ["RELEASE", "0.release", 40], ["MAKEUP", "0.makeup", 30]], [["comp", { ratio: 2 }]], { style: "knurled", meter: "black", meterText: "GR" }),
    "Mastering EQ": () => R ({ name: "MASTERING EQUALIZER", model: "EM-MEQ", height: 2, colour: "#a8aaae", ink: "#15171a", finish: "spun" },
      [["LOW", "0.low", 52], ["LOW FREQ", "0.lowf", 30], ["MID", "0.mid", 50], ["MID FREQ", "0.midf", 50], ["HIGH", "0.high", 55], ["AIR", "0.highf", 70]], [["eq"]], { style: "pointer", nums: "all", box: "STEPPED" }),
    "Loudness maximizer": () => R ({ name: "MAXIMIZER", model: "EM-L1", height: 1, colour: "#07294a", ink: "#d8ecff", accent: "#4fb3ff" },
      [["THRESHOLD", "0.threshold", 30], ["RELEASE", "0.release", 30], ["CEILING", "1.gain", 45]], [["comp", { ratio: 20, attack: 0.5, makeup: 10 }], ["gain"]], { style: "matte", ladder: "blue" }),
    "Game audio enhancer": () => R ({ name: "GAME ENHANCER", model: "EM-GX", height: 1, colour: "#0c1410", ink: "#b4ffcf", finish: "carbon", accent: "#46e070" },
      [["FOOTSTEPS", "0.mid", 70], ["QUIET LIFT", "1.threshold", 30], ["BASS TAME", "0.low", 35], ["WIDTH", "2.width", 60]], [["eq", { midf: 2500, lowf: 150 }], ["comp", { ratio: 6, attack: 1, makeup: 10 }], ["width"]], { style: "capwhite", ring: true }),
    "Podcast leveler": () => R ({ name: "VOICE LEVELER", model: "EM-POD", height: 1, colour: "#2e3440", ink: "#eceff4" },
      [["LEVEL", "1.threshold", 45], ["CLARITY", "0.mid", 60], ["ROOM CUT", "0.low", 35], ["OUTPUT", "2.gain", 50]], [["eq", { lowf: 120, midf: 2500 }], ["comp", { ratio: 4 }], ["gain"]], { style: "ribbed", ladder: "green" }),
    "Radio voice": () => R ({ name: "TELEPHONE", model: "EM-TEL", height: 1, colour: "#1e1e1e", ink: "#f0d060", finish: "bakelite" },
      [["BAND", "0.freq", 45], ["GRIT", "1.drive", 55], ["LEVEL", "2.gain", 50]], [["filter", { mode: 2, q: 2 }], ["drive", { shape: 2 }], ["gain"]], { style: "chicken", size: 20 }),
    "Chorus ensemble": () => R ({ name: "ENSEMBLE", model: "EM-CE", height: 1, colour: "#3a4a8a", ink: "#ffffff", finish: "flake" },
      [["RATE", "0.time", 10], ["DEPTH", "0.mix", 40], ["WIDTH", "1.width", 75]], [["delay", { time: 18, feedback: 20, mix: 40 }], ["width"]], { style: "capwhite" }),
    "Ambient machine": () => R ({ name: "AMBIENT MACHINE", model: "EM-AMB", height: 2, colour: "#101a24", ink: "#d7ecff", accent: "#7fd8ff", glow: true },
      [["SIZE", "0.size", 80], ["SHIMMER", "1.amount", 40], ["ECHO", "2.feedback", 55], ["MIX", "0.mix", 45]], [["room", { size: 6 }], ["exciter", { freq: 5000 }], ["delay", { time: 700 }]], { style: "matte", ring: true, meter: "blue", meterText: "SPACE" }),
    "Sub enhancer": () => R ({ name: "SUB HARMONICS", model: "EM-SUB", height: 1, colour: "#150d05", ink: "#ffb35c" },
      [["SUB", "0.low", 65], ["FREQUENCY", "0.lowf", 20], ["DRIVE", "1.drive", 25]], [["eq"], ["drive", { mix: 30 }]], { style: "matte", ladder: "amber" }),
    "Clean boost": () => R ({ name: "CLEAN BOOST", model: "EM-CB", height: 1, colour: "#d0d3d6", ink: "#111", finish: "chrome" },
      [["BOOST", "0.gain", 60]], [["gain"]], { style: "knurled", size: 22 }),
    "Vintage limiter": () => R ({ name: "PEAK LIMITER", model: "EM-LA", height: 2, colour: "#b7b3a8", ink: "#1a1a1a", finish: "hammertone" },
      [["GAIN", "0.makeup", 45], ["PEAK REDUCTION", "0.threshold", 40]], [["comp", { ratio: 8, attack: 10, release: 300 }]], { style: "fluted", size: 34, meter: "cream", meterText: "GAIN REDUCTION" }),
    "Rotary EQ": () => R ({ name: "DJ MIXER EQ", model: "EM-ISO", height: 1, colour: "#0b0b0b", ink: "#ff4d4d", accent: "#ff4d4d" },
      [["LOW", "0.low", 50], ["MID", "0.mid", 50], ["HIGH", "0.high", 50], ["FILTER", "1.freq", 99]], [["eq"], ["filter"]], { style: "redtrim", ring: true }),
    "Stereo meter bridge": () => R ({ name: "METER BRIDGE", model: "EM-MB", height: 2, colour: "#222", ink: "#eee" },
      [["TRIM", "0.gain", 50]], [["gain"]], { style: "ribbed", size: 16, meter: "white", meterText: "LEVEL", ladder: "classic", left: 60, span: 0 }),
    "Warm console": () => R ({ name: "SUMMING MIXER", model: "EM-SUM", height: 1, colour: "#4a2f1d", ink: "#f3dfb5", finish: "walnut", accent: "#e0a84a" },
      [["DRIVE", "0.drive", 35], ["LOW", "1.low", 55], ["HIGH", "1.high", 52], ["WIDTH", "2.width", 55], ["OUTPUT", "3.gain", 50]], [["drive", { shape: 1, mix: 50 }], ["eq"], ["width"], ["gain"]], { style: "tophat" }),
    "Airy vocal": () => R ({ name: "AIR BAND", model: "EM-AIR", height: 1, colour: "#f2efe8", ink: "#20242a", finish: "pearl" },
      [["AIR", "0.high", 65], ["FREQ", "0.highf", 70], ["SHINE", "1.amount", 35]], [["eq"], ["exciter", { freq: 9000 }]], { style: "capwhite" }),
    "Dub siren delay": () => R ({ name: "DUB DELAY", model: "EM-DUB", height: 1, colour: "#1b3b1b", ink: "#ffd400", accent: "#ffd400" },
      [["TIME", "1.time", 55], ["FEEDBACK", "1.feedback", 70], ["TONE", "1.tone", 35], ["SPACE", "2.mix", 30]], [["filter", { mode: 1, freq: 300 }], ["delay"], ["room", { size: 3 }]], { style: "chicken" }),
    "Headphone crossfeed": () => R ({ name: "CROSSFEED", model: "EM-XF", height: 1, colour: "#121820", ink: "#cfe3ff" },
      [["AMOUNT", "0.width", 35], ["WARMTH", "1.high", 45]], [["width", { width: 70 }], ["eq", { highf: 9000 }]], { style: "matte" }),
    "Bit crusher": () => R ({ name: "DESTROYER", model: "EM-X", height: 1, colour: "#000000", ink: "#39ff14", accent: "#39ff14", glow: true },
      [["DESTROY", "0.drive", 75], ["TONE", "0.tone", 40], ["MIX", "0.mix", 70], ["LEVEL", "1.gain", 35]], [["drive", { shape: 2 }], ["gain"]], { style: "pointer", ring: true }),
  });

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
  function designThumb (d, width = 150, ownDefs = false) {
    const t = document.createElementNS (NS, "svg"), was = design, wasSel = selected;
    design = d; selected = [];
    render (t, "edit", ownDefs);
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
    if (!writeLib (a)) alert ("This browser would not keep it (private mode or storage is full)."); else toast ("Saved in this browser"); showLib();
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
  const copy = async (s, btn) => { try { await navigator.clipboard.writeText (s); const t = btn.textContent; btn.textContent = "Copied"; setTimeout (() => (btn.textContent = t), 1400); toast ("Copied"); } catch (_) { $("share-code").select(); } };
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

  /** A short note at the foot of the screen (what just happened), gone after two seconds. */
  let toastTimer = 0;
  function toast (msg) {
    const t = $("toast"); if (!t) return;
    t.textContent = msg; t.classList.add ("on");
    clearTimeout (toastTimer); toastTimer = setTimeout (() => t.classList.remove ("on"), 2000);
  }

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
  for (const b of document.querySelectorAll ("[data-centre]")) b.addEventListener ("click", () => centreOnPanel (b.getAttribute ("data-centre")));
  $("tidy").addEventListener ("click", () => { tidy(); toast ("Tidied up - Undo takes it back"); });
  bindAi();
  const setZoom = (z) => { zoom = Math.min (4, Math.max (0.4, z)); $("zoom-val").textContent = Math.round (zoom * 100) + "%"; render(); };
  // Fit: the whole unit, as big as the stage allows, in its middle - on opening, on a new unit, when the window
  // changes; until you zoom yourself (Fit gives it back)
  let userZoomed = false;
  const fitZoom = () => {
    const st = $("stage"); if (!st || st.clientWidth < 50) return;
    const H = design.unit.height * U, up = isUpright (), across = up ? H : W, down = up ? W : H;
    setZoom (Math.min ((st.clientWidth - 40) / ((across + 12) * 2), (st.clientHeight - 40) / ((down + 12) * 2)));
  };
  refit = () => { if (!userZoomed) fitZoom(); };
  $("zoom-in").addEventListener ("click", () => { userZoomed = true; setZoom (zoom * 1.2); }); $("zoom-out").addEventListener ("click", () => { userZoomed = true; setZoom (zoom / 1.2); });
  $("zoom-fit").addEventListener ("click", () => { userZoomed = false; fitZoom(); });
  addEventListener ("resize", () => requestAnimationFrame (refit));
  requestAnimationFrame (() => requestAnimationFrame (refit));

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
  // ---------------------------------------------------------------------------------------------------
  // Colour snapping: a colour picked close to one of these is pulled onto it (ΔE in CIELAB under 14); the same
  // colours as swatches, a click puts one on the colour field you last changed
  const HARDWARE = [["API blue", "#2c4c86"], ["API red", "#9a2a22"], ["Neve maroon", "#6a1c1a"], ["Neve grey", "#76787c"], ["Neve blue-grey", "#5b6b7a"],
    ["Pultec cream", "#e9e2cf"], ["Pultec blue", "#6f8aa0"], ["SSL grey", "#3a3d42"], ["1176 black", "#1b1b1d"], ["LA-2A silver", "#c9cbce"],
    ["Fairchild grey", "#7f8386"], ["Tube-Tech blue", "#2d4a6b"], ["UREI silver", "#aeb7c0"], ["Manley plum", "#4b2a3a"], ["Lexicon red", "#b3262e"], ["Studer green", "#5d7a5f"]];
  const RAL = [["RAL 1013 oyster white", "#e3d9c6"], ["RAL 1021 colza yellow", "#f3c200"], ["RAL 2004 pure orange", "#e75b12"], ["RAL 3000 flame red", "#af2b1e"],
    ["RAL 3004 purple red", "#6b1c23"], ["RAL 5003 sapphire blue", "#1f3855"], ["RAL 5010 gentian blue", "#0e4c92"], ["RAL 5024 pastel blue", "#6093ac"],
    ["RAL 6005 moss green", "#114232"], ["RAL 6011 reseda green", "#587246"], ["RAL 7016 anthracite", "#293133"], ["RAL 7032 pebble grey", "#b9b9a8"],
    ["RAL 7035 light grey", "#cbd0cc"], ["RAL 8017 chocolate", "#442f29"], ["RAL 9005 jet black", "#0a0a0a"], ["RAL 9006 white aluminium", "#a5a5a5"]];
  const hexRgb = (h) => { const n = parseInt (String (h).replace ("#", ""), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255]; };
  const rgbHex = (r, g, b) => "#" + [r, g, b].map ((v) => Math.round (Math.min (255, Math.max (0, v))).toString (16).padStart (2, "0")).join ("");
  function toLab (hex) {
    const lin = hexRgb (hex).map ((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : Math.pow ((v + 0.055) / 1.055, 2.4); });
    const X = (lin[0] * 0.4124 + lin[1] * 0.3576 + lin[2] * 0.1805) / 0.95047, Y = lin[0] * 0.2126 + lin[1] * 0.7152 + lin[2] * 0.0722, Z = (lin[0] * 0.0193 + lin[1] * 0.1192 + lin[2] * 0.9505) / 1.08883;
    const f = (t) => (t > 0.008856 ? Math.cbrt (t) : 7.787 * t + 16 / 116);
    return [116 * f (Y) - 16, 500 * (f (X) - f (Y)), 200 * (f (Y) - f (Z))];
  }
  function hueShift (hex, deg) {   // the same colour turned round the wheel (HSL)
    let [r, g, b] = hexRgb (hex).map ((v) => v / 255); const mx = Math.max (r, g, b), mn = Math.min (r, g, b), l = (mx + mn) / 2, d = mx - mn;
    if (d < 1e-6) return hex;
    const sat = d / (1 - Math.abs (2 * l - 1)); let h = mx === r ? ((g - b) / d) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; h = (h * 60 + deg + 360) % 360;
    const c = (1 - Math.abs (2 * l - 1)) * sat, x = c * (1 - Math.abs ((h / 60) % 2 - 1)), m = l - c / 2;
    const [r1, g1, b1] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x] : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return rgbHex ((r1 + m) * 255, (g1 + m) * 255, (b1 + m) * 255);
  }
  function snapTargets () {
    const u = design.unit, mine = new Map ();
    for (const [n, c] of [["Panel", u.colour], ["Print", u.ink], ["Accent", u.accent], ["Second colour", u.toneColour], ["Chassis", u.chassis]]) if (c) mine.set (c.toLowerCase(), n);
    for (const p of design.parts) for (const k of ["colour", "ringColour", "fillCol", "cable"]) if (typeof p[k] === "string" && p[k].startsWith ("#") && !mine.has (p[k].toLowerCase())) mine.set (p[k].toLowerCase(), TYPES[p.type].label + " " + k.replace ("Col", " colour"));
    const harmonies = [["Complement", 180], ["Analogous -30°", -30], ["Analogous +30°", 30], ["Triad -120°", -120], ["Triad +120°", 120]].map (([n, d]) => [n + " of the panel", hueShift (u.colour, d)]);
    return [["This unit", [...mine].map (([c, n]) => [n, c])], ["Harmonies", harmonies], ["Hardware", HARDWARE], ["Paint (RAL)", RAL]];
  }
  let lastColourField = null;
  function snapColour (hex) {
    const a = toLab (hex); let best = null, bd = 14;
    for (const [, list] of snapTargets()) for (const [, c] of list) {
      const b = toLab (c), d = Math.hypot (a[0] - b[0], a[1] - b[1], a[2] - b[2]);
      if (d < bd) { bd = d; best = c; }
    }
    return best || hex;
  }
  function drawSwatches () {
    const box = $("u-swatches"); if (!box) return; box.replaceChildren();
    for (const [group, list] of snapTargets()) {
      if (!list.length) continue;
      const row = document.createElement ("div"); row.className = "d-sw-row";
      const h = document.createElement ("span"); h.textContent = group; row.appendChild (h);
      for (const [name, c] of list) {
        const b = document.createElement ("button"); b.type = "button"; b.className = "d-sw"; b.style.background = c; b.title = name + " " + c; b.setAttribute ("aria-label", name);
        b.addEventListener ("click", () => {
          const f = lastColourField && document.contains (lastColourField) ? lastColourField : $("u-colour");
          f.value = c; f.dispatchEvent (new Event ("input", { bubbles: true })); f.dispatchEvent (new Event ("change", { bubbles: true })); lastColourField = f;
          toast (name + " on " + ((f.labels && f.labels[0] && f.labels[0].textContent.trim()) || "the colour"));
        });
        row.appendChild (b);
      }
      box.appendChild (row);
    }
  }
  // (capture: the colour is snapped before the field's own handlers read it)
  for (const type of ["input", "change"])
    document.addEventListener (type, (e) => {
      const f = e.target;
      if (!(f instanceof HTMLInputElement) || f.type !== "color" || !f.closest (".designer") || f.closest ("#u-swatches")) return;
      lastColourField = f;
      if ($("u-colsnap") && $("u-colsnap").checked && type === "change") { const s2 = snapColour (f.value); if (s2.toLowerCase() !== f.value.toLowerCase()) f.value = s2; }
      if (type === "change") requestAnimationFrame (drawSwatches);
    }, true);
  drawSwatches();
  $("u-gridmm").addEventListener ("change", (e) => { gridMm = Number (e.target.value) || 0.5; });

  $("check-run").addEventListener ("click", showIssues);
  $("check-fix").addEventListener ("click", fixIssues);
  $("ideas").addEventListener ("click", ideas);
  // The preset cards. Each picture is drawn once, as a plain image (a live SVG with the finishes' noise
  // filters, times sixty, made the list crawl whenever it scrolled or repainted): the cards on screen first,
  // the rest when the browser is idle - opening the tab never waits for all of them.
  const thumbJobs = [];
  function thumbImage (name, holder) {
    const t = designThumb (sanitize (templates[name]()), 150, true);
    t.setAttribute ("xmlns", NS);
    const url = URL.createObjectURL (new Blob ([new XMLSerializer().serializeToString (t)], { type: "image/svg+xml" }));
    const img = document.createElement ("img");
    img.alt = ""; img.decoding = "async"; img.width = 150; img.height = Number (t.getAttribute ("height")) || 40;
    img.src = url;
    holder.replaceChildren (img);
  }
  const idle = window.requestIdleCallback || ((f) => setTimeout (() => f ({ timeRemaining: () => 8 }), 30));
  function drainThumbs (deadline) {
    while (thumbJobs.length && deadline.timeRemaining() > 4) { const j = thumbJobs.shift(); if (!j.done) { j.done = true; thumbImage (j.name, j.holder); } }
    if (thumbJobs.length) idle (drainThumbs);
  }
  const thumbSeen = "IntersectionObserver" in window ? new IntersectionObserver ((entries) => {
    for (const e of entries) if (e.isIntersecting) { const j = e.target._job; thumbSeen.unobserve (e.target); if (j && !j.done) { j.done = true; thumbImage (j.name, j.holder); } }
  }, { root: null, rootMargin: "200px" }) : null;
  for (const name in templates) {
    const b = document.createElement ("button"); b.type = "button"; b.className = "d-tpl";
    const holder = document.createElement ("span"); holder.className = "d-tpl-pic"; b.appendChild (holder);
    const n = document.createElement ("span"); n.textContent = name; b.appendChild (n);
    b.addEventListener ("click", () => replaceDesign (sanitize (templates[name]()))); $("templates").appendChild (b);
    const job = { name, holder, done: false }; thumbJobs.push (job); b._job = job;
    if (thumbSeen) thumbSeen.observe (b);
  }
  idle (drainThumbs);
  // Search: by name, every word must match (so "tube pre" finds the valve preamps)
  function filterPresets () {
    const words = $("tpl-search").value.toLowerCase().split (/\s+/).filter (Boolean);
    let shown = 0;
    for (const b of $("templates").children) {
      const name = b.textContent.toLowerCase(), on = words.every ((w) => name.includes (w));
      b.hidden = !on; if (on) shown++;
    }
    $("tpl-count").textContent = words.length ? shown + (shown === 1 ? " preset" : " presets") : Object.keys (templates).length + " presets";
  }
  $("tpl-search").addEventListener ("input", filterPresets);
  filterPresets();

  // Quick start (the empty "Selected" panel): each step opens its tab, or the share dialog
  // Guided start: what it does (a preset to build from, its sound included) and how it looks (panel and print)
  const GUIDE_KINDS = [["Compressor", "evens out the level", "FET compressor"], ["EQ", "shapes the tone", "Program EQ"], ["Tube warmth", "thick and warm", "Tube saturator"],
    ["Echo", "repeats that fade", "Tape echo"], ["Reverb", "a space around it", "Hall reverb"], ["Mastering", "loud and polished", "Mastering limiter"],
    ["Lo-fi and weird", "crunchy, wobbly", "Lo-fi box"], ["Game audio", "hear footsteps", "Game audio enhancer"]];
  const GUIDE_LOOKS = [["As it comes", "the preset's own look", null, null], ["Vintage cream", "warm and classic", "#d9cfb6", "#1b1a18"], ["Modern black", "sleek and dark", "#16171a", "#f2f2f2"],
    ["Bold red", "loud and proud", "#b8322a", "#fff4ea"], ["Blue steel", "cool and calm", "#2c4868", "#eef4ff"], ["Army green", "rugged", "#4a5a3a", "#f1efe4"]];
  let guideKind = 0, guideLook = 0;
  function guideChoices (box, list, pick, onPick) {
    box.replaceChildren();
    list.forEach ((c, i) => { const b = document.createElement ("button"); b.type = "button"; b.setAttribute ("role", "radio"); b.setAttribute ("aria-checked", String (i === pick));
      if (c[2] && c[2].startsWith ("#")) { const chip = document.createElement ("span"); chip.className = "d-chip"; chip.style.background = c[2]; b.append (chip); }
      const t = document.createElement ("b"); t.textContent = c[0]; const s2 = document.createElement ("small"); s2.textContent = c[1]; b.append (t, s2);
      b.addEventListener ("click", () => { onPick (i); guideChoices (box, list, i, onPick); }); box.append (b); });
  }
  function openGuide () {
    guideChoices ($("guide-kind"), GUIDE_KINDS, guideKind, (i) => { guideKind = i; });
    guideChoices ($("guide-look"), GUIDE_LOOKS, guideLook, (i) => { guideLook = i; });
    $("guide-dialog").showModal();
  }
  $("guide-open").addEventListener ("click", openGuide);
  $("guide-close").addEventListener ("click", () => $("guide-dialog").close());
  $("guide-go").addEventListener ("click", () => {
    const k = GUIDE_KINDS[guideKind], l = GUIDE_LOOKS[guideLook], d = sanitize (templates[k[2]]());
    if (l[2]) { d.unit.colour = l[2]; d.unit.ink = l[3]; }
    replaceDesign (d); $("guide-dialog").close();
    showTab ($("t-sound"), true);   // (next: hear it)
  });
  document.querySelectorAll (".d-start [data-go]").forEach ((b) => b.addEventListener ("click", () => { const t = $(b.dataset.go); if (t) { showTab (t, true); if (b.dataset.go === "t-presets") $("tpl-search").focus(); } }));
  document.querySelectorAll (".d-start [data-do=share]").forEach ((b) => b.addEventListener ("click", () => $("share").click()));

  // Keyboard shortcuts: the list, from its button or "?"
  const keysDialog = $("keys-dialog");
  $("keys-open").addEventListener ("click", () => keysDialog.showModal());
  $("keys-close").addEventListener ("click", () => keysDialog.close());
  document.addEventListener ("keydown", (e) => {
    if (e.key !== "?" || e.ctrlKey || e.metaKey || e.altKey) return;
    const t = e.target; if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
    e.preventDefault(); if (!keysDialog.open) keysDialog.showModal();
  });

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
    // The sound: blocks from the list only, every parameter clamped, at most MAX_BLOCKS; wiring checked against the chain
    const es = await decode (await mk ({ v: 2, u: {}, x: { chain: [{ b: "comp", p: { threshold: -999, ratio: "x", evil: 1 } }, { b: "<script>" }, ...Array.from ({ length: 20 }, () => ({ b: "gain" }))] },
      p: [{ t: "knob", cl: "0.threshold" }, { t: "knob", cl: "0.evil" }, { t: "knob", cl: "9.gain" }, { t: "toggle", cl: "1.on" }, { t: "knob", cl: "0.threshold;alert(1)" }] }));
    ok (es.dsp.chain.length === MAX_BLOCKS && es.dsp.chain[0].b === "comp" && es.dsp.chain[0].p.threshold === -60 && es.dsp.chain[0].p.ratio === 3 && !("evil" in es.dsp.chain[0].p),
        "sound blocks: known types only, parameters clamped, at most " + MAX_BLOCKS);
    ok (es.parts[0].ctl === "0.threshold" && !("ctl" in es.parts[1]) && !("ctl" in es.parts[2]) && es.parts[3].ctl === "1.on" && !("ctl" in es.parts[4]), "knob wiring: only to a block and parameter that exist");
    const wired = sanitize ({ dsp: { chain: [{ b: "drive", p: { drive: 20 } }] }, parts: [{ type: "knob", ctl: "0.drive" }] }), backW = await decode (await encode (wired));
    ok (JSON.stringify (backW.dsp) === JSON.stringify (wired.dsp) && backW.parts[0].ctl === "0.drive", "the sound and its wiring survive the round trip");
    // Copy for AI: the brief's JSON comes back as the same design; auto-tidy keeps every part, on the panel
    const wasD = design; design = sanitize (templates["Console channel"]());
    const brief = aiBrief(), js = JSON.parse (brief.slice (brief.indexOf ("{"), brief.lastIndexOf ("}") + 1)), backA = sanitize (js);
    ok (backA.parts.length === design.parts.length && strip0 (backA) === strip0 (design), "an AI's reply of the same JSON is the same design");
    const n0 = design.parts.length; tidy();
    ok (design.parts.length === n0 && design.parts.every ((q) => q.x >= 0 && q.x <= W && q.y >= 0 && q.y <= design.unit.height * U), "auto-tidy keeps every part, on the panel");
    design = wasD; undo();
    // Every preset loads, and keeps all its wiring to its sound
    let badPre = [];
    for (const name in templates) { const raw = templates[name](), d = sanitize (raw), want = (raw.parts || []).filter ((q) => q.ctl).length, got = d.parts.filter ((q) => q.ctl).length;
      if (want !== got || d.parts.length !== (raw.parts || []).length) badPre.push (name); }
    ok (!badPre.length && Object.keys (templates).length >= 54, Object.keys (templates).length + " presets load with their wiring" + (badPre.length ? " - not: " + badPre.join (", ") : ""));
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
    ok (GUIDE_KINDS.every ((k) => typeof templates[k[2]] === "function"), "guided start: every choice has its preset");
    // Screens: every kind draws, the custom one's settings survive a code, and Play animates them
    const keep = design;
    design = sanitize ({ unit: { height: 3 }, parts: SCREEN_KINDS.map ((kind, i) => ({ type: "display", kind, x: 60 + (i % 4) * 110, y: 30 + Math.floor (i / 4) * 60, w: 96, h: 44,
      scrShape: "saw", scrStyle: "rings", scrColours: "rainbow", scrSpeed: 8 })) });
    render();
    const scrs = [...svg.querySelectorAll (".d-scr")];
    ok (scrs.length === SCREEN_KINDS.length && scrs.every ((g) => g.childElementCount > 0), "every screen kind draws (" + scrs.map ((g) => g.childElementCount).join (",") + ")");
    const cu = (await decode (await encode (design))).parts.find ((q) => q.kind === "custom");
    ok (cu && cu.scrShape === "saw" && cu.scrStyle === "rings" && cu.scrColours === "rainbow" && cu.scrSpeed === 8, "a custom screen's settings survive the round trip");
    const firstPath = () => { const g = svg.querySelector (".d-scr path"); return g ? g.getAttribute ("d") : ""; };
    setPlay (true); const d0 = firstPath (); await new Promise ((r) => setTimeout (r, 300)); const d1 = firstPath (); setPlay (false);
    ok (d0 !== "" && d0 !== d1, "in Play the screens move");
    if (!location.search.includes ("screens")) { design = keep; render(); }
    if (window.ENHSound && window.ENHSound.testBlocks) {   // every sound block, rendered offline
      const res = await window.ENHSound.testBlocks ();
      const badB = res.filter ((x) => !x[1]);
      const keep2 = design;
      design = sanitize ({ unit: {}, parts: [{ type: "knob", ctl: "1.drive" }], dsp: { chain: [{ b: "room", on: true, p: { mix: 80 } }, { b: "drive", on: true, p: { drive: 36 } }, { b: "eq", on: true, p: {} }] } });
      const said = await window.ENHSound.makeItGood ();
      const ch = design.dsp.chain.map ((b) => b.b).join (",");
      ok (/^eq,drive,room(,gain)?$/.test (ch) && design.parts[0].ctl === "1.drive" && design.dsp.chain[1].p.drive === 24 && design.dsp.chain[2].p.mix === 40,
          "make it sound good: reordered (" + ch + "), wiring followed, extremes tamed - " + said);
      design = keep2; render();
      ok (res.length === DSP_TYPES.length && !badB.length, res.length + " sound blocks build, change the sound and stay in bounds" + (badB.length ? " - not: " + badB.map ((x) => x[0] + " (" + x[2] + ")").join (", ") : ""));
    }
    document.title = out.every ((l) => l.startsWith ("PASS")) ? "SELFTEST PASS" : "SELFTEST FAIL";
    const pre = document.createElement ("pre"); pre.id = "selftest"; pre.textContent = [...out.filter ((l) => !l.startsWith ("PASS")), ...out.filter ((l) => l.startsWith ("PASS")).reverse ()].join ("\n"); document.body.prepend (pre);   // (failures first, then the newest checks)
  }

  // Start: a shared link's design, else the one saved in this browser, else the FET template
  (async () => {
    document.documentElement.classList.remove ("no-js");
    let start = load();
    if (location.hash.startsWith ("#d=")) { try { start = await decode (location.hash); } catch (_) { /* keep the saved one */ } }
    // ?preset=<name>: the site's gallery opens a preset by name (only a name from the list: nothing else is read)
    const presetName = new URLSearchParams (location.search).get ("preset");
    if (presetName && Object.prototype.hasOwnProperty.call (templates, presetName)) start = sanitize (templates[presetName]());
    design = start || sanitize (templates["FET compressor"]());
    nextId = Math.max (nextId, ...design.parts.map ((p) => p.id + 1), 1);
    last = JSON.stringify (design);
    bindUnit(); syncUnit(); render(); props(); showLib();
    setZoom (Math.min (1.4, ($("stage").clientWidth - 24) / ((W + 12) * 2)));
    if (location.search.includes ("selftest")) selftest();
    else if (!start) openGuide();   // (a first visit: nothing saved, no link - offer the two questions)
  })();
}());
