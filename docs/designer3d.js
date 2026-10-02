/* The Rack Unit Designer's 3D view: the design built as real geometry. Knobs are turned on a lathe from
   the same recipes as ENH Master's own HardwareKit models (skirt, taper, flutes, collar, cap, painted line);
   the faceplate carries the design's print, paint and meter faces as a texture; everything is lit by a
   studio made here (a softbox, a key light, warm walls) - nothing is fetched from anywhere. */
import {
  WebGLRenderer, Scene, PerspectiveCamera, Group, Mesh, BoxGeometry, PlaneGeometry, CircleGeometry, CylinderGeometry,
  LatheGeometry, ExtrudeGeometry, SphereGeometry, Shape, Vector2, Vector3, Color, Raycaster, MeshPhysicalMaterial,
  MeshStandardMaterial, MeshBasicMaterial, DirectionalLight, HemisphereLight, PMREMGenerator, CanvasTexture,
  SRGBColorSpace, ACESFilmicToneMapping, PCFShadowMap, BackSide, LineBasicMaterial, LineSegments, EdgesGeometry,
} from "./vendor/three/three.module.min.js";

const D = window.ENHDesigner;
const canvas = document.getElementById ("c3d"), wrap = document.getElementById ("stage3d"), stage2d = document.getElementById ("stage");
const note = document.getElementById ("note3d");

// ----------------------------------------------------------------------------------------------------
// Views: 2D, 3D, both
let view = "2d", started = false;
for (const b of document.querySelectorAll ("[data-view]")) b.addEventListener ("click", () => setView (b.dataset.view));
function setView (v) {
  view = v;
  for (const b of document.querySelectorAll ("[data-view]")) b.setAttribute ("aria-pressed", String (b.dataset.view === v));
  wrap.hidden = v === "2d"; stage2d.hidden = v === "3d";
  wrap.classList.toggle ("both", v === "both");   // (side by side in the column: the 3D view shorter, the faceplate room)
  if (v !== "2d") { if (!started) start(); else { resize(); rebuild(); } }
  requestAnimationFrame (() => window.dispatchEvent (new Event ("resize")));   // (the 2D stage fits its new size)
}

let renderer, scene, camera, root, pmrem, dirty = true;
const mats = {};
function start () {
  started = true;
  try {
    renderer = new WebGLRenderer ({ canvas, antialias: true, alpha: false, powerPreference: "low-power" });
  } catch (_) { note.textContent = "Your browser could not start 3D here. The 2D view still has everything."; return; }
  renderer.setPixelRatio (Math.min (1.5, window.devicePixelRatio || 1));
  renderer.outputColorSpace = SRGBColorSpace;
  renderer.toneMapping = ACESFilmicToneMapping; renderer.toneMappingExposure = 1.25;
  renderer.shadowMap.enabled = true; renderer.shadowMap.type = PCFShadowMap;
  scene = new Scene(); scene.background = new Color ("#0b0908");
  camera = new PerspectiveCamera (28, 2, 10, 6000);

  // A studio for the reflections: warm dark walls, a big softbox up front, a strip to the left
  const studio = new Scene();
  studio.add (new Mesh (new BoxGeometry (4000, 3000, 4000), new MeshBasicMaterial ({ color: new Color (0.09, 0.06, 0.045), side: BackSide })));
  const box = (w, h, x, y, z, ry, k) => { const m = new Mesh (new PlaneGeometry (w, h), new MeshBasicMaterial ({ color: new Color (k, k, k * 0.96) })); m.position.set (x, y, z); m.lookAt (0, 0, 0); studio.add (m); };
  box (1400, 900, 0, 500, 1800, 0, 5.0);
  box (300, 1600, -1500, 200, 800, 0, 3.0);
  box (2000, 200, 0, -1400, 600, 0, 0.6);
  pmrem = new PMREMGenerator (renderer);
  scene.environment = pmrem.fromScene (studio, 0.02).texture;

  const hemi = new HemisphereLight (0xfff2e0, 0x201810, 0.6); scene.add (hemi);
  const key = new DirectionalLight (0xfff4e6, 2.2); key.position.set (-300, 420, 520); key.castShadow = true;
  key.shadow.mapSize.set (1024, 1024); key.shadow.bias = -0.0004; key.shadow.normalBias = 0.6;
  Object.assign (key.shadow.camera, { left: -300, right: 300, top: 150, bottom: -150, near: 100, far: 1500 });
  scene.add (key);
  const rim = new DirectionalLight (0xffe0c0, 0.6); rim.position.set (400, 120, 300); scene.add (rim);

  const m = (o) => new MeshPhysicalMaterial (o);
  Object.assign (mats, {
    blackGloss: m ({ color: 0x0b0b0c, roughness: 0.22, clearcoat: 1, clearcoatRoughness: 0.08 }),
    blackMatte: m ({ color: 0x121214, roughness: 0.72, clearcoat: 0.15, clearcoatRoughness: 0.5 }),
    rubber: m ({ color: 0x151517, roughness: 0.62 }),
    alu: m ({ color: 0xc9cacf, metalness: 1, roughness: 0.28 }),
    aluSatin: m ({ color: 0xb5b6bb, metalness: 1, roughness: 0.42 }),
    red: m ({ color: 0x8e1d1b, metalness: 0.8, roughness: 0.32, clearcoat: 0.6 }),
    grey: m ({ color: 0x3a3b3f, roughness: 0.55 }),
    capBlue: m ({ color: 0x24428e, roughness: 0.3, clearcoat: 0.8 }),
    capRed: m ({ color: 0x9a221c, roughness: 0.3, clearcoat: 0.8 }),
    capWhite: m ({ color: 0xe9e7e0, roughness: 0.32, clearcoat: 0.8 }),
    chrome: m ({ color: 0xe6e7ea, metalness: 1, roughness: 0.12 }),
    dark: m ({ color: 0x050506, roughness: 0.9 }),
    brass: m ({ color: 0xc9a64a, metalness: 1, roughness: 0.3 }),
    glass: m ({ color: 0xffffff, roughness: 0.04, metalness: 0, transparent: true, opacity: 0.12, clearcoat: 1, clearcoatRoughness: 0.02 }),
    chassis: m ({ color: 0x1a1a1c, metalness: 0.6, roughness: 0.55 }),
    blackMetal: m ({ color: 0x151518, metalness: 0.9, roughness: 0.35 }),
  });
  root = new Group(); scene.add (root);
  bindPointer();
  resize();
  window.addEventListener ("resize", () => { resize(); dirty = true; });
  D.subscribe (() => { if (view !== "2d") schedule(); });
  requestAnimationFrame (loop);
}

function resize () {
  if (!renderer) return;
  const w = canvas.clientWidth || 800, h = canvas.clientHeight || 400;
  renderer.setSize (w, h, false); camera.aspect = w / h; camera.updateProjectionMatrix(); dirty = true;
}

// ----------------------------------------------------------------------------------------------------
// Geometry helpers
function rrect (w, h, r) {
  const s = new Shape(), x = -w / 2, y = -h / 2; r = Math.min (r, w / 2, h / 2);
  s.moveTo (x + r, y); s.lineTo (x + w - r, y); s.quadraticCurveTo (x + w, y, x + w, y + r);
  s.lineTo (x + w, y + h - r); s.quadraticCurveTo (x + w, y + h, x + w - r, y + h);
  s.lineTo (x + r, y + h); s.quadraticCurveTo (x, y + h, x, y + h - r);
  s.lineTo (x, y + r); s.quadraticCurveTo (x, y, x + r, y);
  return s;
}
/** A turned part: profile [[radius, height]...] around the knob's axis (out of the panel = +z), with an
    optional relief carved in (count flutes/ribs of `depth` between heights y0..y1, `sharp` 0 round .. 1 crisp). */
function turned (profile, seg, relief) {
  const g = new LatheGeometry (profile.map (([r, y]) => new Vector2 (Math.max (0.0001, r), y)), seg);
  if (relief && relief.count) {
    const p = g.attributes.position;
    for (let i = 0; i < p.count; ++i) {
      const x = p.getX (i), y = p.getY (i), z = p.getZ (i);
      if (y < relief.y0 || y > relief.y1) continue;
      const a = Math.atan2 (z, x), c = Math.cos (a * relief.count);
      const wave = relief.sharp > 0.5 ? Math.abs (c) : 0.5 + 0.5 * c;
      const k = 1 - relief.depth * (1 - Math.pow (wave, 1 + 4 * relief.sharp));
      p.setX (i, x * k); p.setZ (i, z * k);
    }
    g.computeVertexNormals();
  }
  g.rotateX (Math.PI / 2);   // lathe y (height) -> +z, out of the panel
  return g;
}
const mesh = (g, mat, shadow = true) => { const m = new Mesh (g, mat); m.castShadow = shadow; m.receiveShadow = true; return m; };
function line (group, r, h, colour, side) {   // the painted line: across the top, and down the flank to the base
  const mat = new MeshStandardMaterial ({ color: colour, roughness: 0.45 });
  const top = mesh (new BoxGeometry (Math.max (0.5, r * 0.09), r * 0.78, 0.12), mat, false); top.position.set (0, r * 0.52, h + 0.06); group.add (top);
  if (side) { const s = mesh (new BoxGeometry (Math.max (0.5, r * 0.09), 0.12, h * 0.92), mat, false); s.position.set (0, side, h * 0.5); group.add (s); }
}

// The knob recipes (HardwareKit Hardware.cpp, "classic studio gear, matched to photographs")
/** A knob of the owner's own (the Knobs tab): turned on the lathe from its checked choices and numbers. */
function customKnob (ck, r, deg, pointer) {
  const g = new Group(), seg = 64;
  const matFor = (hex) => colourMat (hex, { gloss: { roughness: 0.2, clearcoat: 1, clearcoatRoughness: 0.08 }, satin: { roughness: 0.45, clearcoat: 0.3 },
    matte: { roughness: 0.78, clearcoat: 0 }, metal: { metalness: 1, roughness: 0.3, clearcoat: 0 }, rubber: { roughness: 0.66, clearcoat: 0 } }[ck.mt] || {});
  const body = matFor (ck.bc);
  let b = 0;
  if (ck.sk > 0) { const R = r * (1 + 0.45 * ck.sk); g.add (mesh (turned ([[0, 0], [R, 0], [R, 0.8], [R - 0.3, 1.2], [r * 1.02, 1.4]], seg), matFor (ck.sc))); b = 1.4; }
  const h = ck.ht * r, top = r * (ck.sh === "cone" ? ck.tp * 0.55 : ck.tp);
  const relief = ck.gr === "none" ? null : { count: ck.gr === "knurl" ? Math.max (ck.gc, 60) : ck.gc, depth: (ck.gr === "flutes" ? 0.09 : ck.gr === "knurl" ? 0.02 : 0.04) * ck.gd,
                                            y0: b + 0.1 * h, y1: b + h - 0.5, sharp: ck.gr === "flutes" ? 0.15 : ck.gr === "knurl" ? 1 : 0.55 };
  const profiles = {
    cyl: [[0, b], [r, b], [r, b + h - 0.6], [r - 0.6, b + h], [0, b + h]],
    taper: [[0, b], [r, b], [top, b + h - 0.5], [Math.max (0.1, top - 0.5), b + h], [0, b + h]],
    dome: [[0, b], [r, b], [r, b + h * 0.5], [r * 0.85, b + h * 0.8], [r * 0.5, b + h * 0.97], [0, b + h]],
    tophat: [[0, b], [r * 1.25, b], [r * 1.25, b + h * 0.18], [top, b + h * 0.22], [top, b + h - 0.5], [Math.max (0.1, top - 0.5), b + h], [0, b + h]],
    cone: [[0, b], [r, b], [top, b + h], [0, b + h]],
  };
  if (ck.sh === "pointer") {
    g.add (mesh (turned ([[0, b], [0.7 * r, b], [0.7 * r, b + h], [0, b + h]], seg), body));
    const bar = mesh (new BoxGeometry (0.4 * r, 1.9 * r, h * 0.85), body); bar.position.set (0, 0.25 * r, b + h * 0.42); g.add (bar);
  } else g.add (mesh (turned (profiles[ck.sh] || profiles.cyl, relief ? seg * 2 : seg, relief || undefined), body));
  let capTop = b + h;
  if (ck.cap !== "none" && ck.sh !== "pointer") {
    const cr = top * ck.cs, dome = ck.cap === "dome";
    g.add (mesh (turned (dome ? [[0, b + h - 0.2], [cr, b + h - 0.2], [cr, b + h + 0.2], [cr * 0.7, b + h + 0.7], [cr * 0.3, b + h + 0.95], [0, b + h + 1.0]]
                              : [[0, b + h - 0.2], [cr, b + h - 0.2], [cr, b + h + 0.3], [0, b + h + 0.35]], seg), matFor (ck.kc)));
    capTop = b + h + (dome ? 1.0 : 0.35);
  }
  const pc = new Color ({ white: "#f2f2f2", cream: "#e9dfc6", black: "#111111", red: "#d8322b" }[pointer] || ck.pc).getHex();
  const tipR = ck.sh === "pointer" ? r * 1.12 : top;
  if (ck.pt === "line") line (g, tipR, capTop, pc, 0);
  else if (ck.pt === "dot") { const d = mesh (new SphereGeometry (Math.max (0.3, r * 0.1), 12, 8), new MeshStandardMaterial ({ color: pc })); d.position.set (0, tipR * 0.7, capTop + 0.1); g.add (d); }
  else if (ck.pt === "notch") { const n = mesh (new BoxGeometry (r * 0.12, r * 0.32, 0.3), mats.dark); n.position.set (0, tipR - r * 0.16, capTop); g.add (n); }
  g.rotation.z = -deg * Math.PI / 180;
  return g;
}

function knob (style, r, deg, pointer) {
  if (typeof style === "string" && /^c[0-7]$/.test (style)) {
    const ck = (D.get().knobs || [])[Number (style.slice (1))];
    if (ck) return customKnob (ck, r, deg, pointer);
  }
  const g = new Group(), seg = 64, ptr = { white: 0xf2f2f2, cream: 0xe9dfc6, black: 0x111111, red: 0xd8322b }[pointer];
  const skirt = (R, mat, h0 = 0) => { g.add (mesh (turned ([[0, h0], [R, h0], [R, h0 + 0.8], [R - 0.3, h0 + 1.2], [R * 0.72, h0 + 1.5 + 0.10 * r], [r * 1.02, h0 + 1.6 + 0.24 * r]], seg), mat)); };
  const collar = (cr, ch) => g.add (mesh (turned ([[0, 0], [cr, 0], [cr, ch - 0.2], [cr - 0.2, ch], [0, ch]], seg), mats.aluSatin));
  switch (style) {
    case "tophat": { skirt (r * 1.45, mats.blackGloss); const b = 1.6 + 0.24 * r, h = 1.05 * r;
      g.add (mesh (turned ([[0, b], [r, b], [r, b + 0.1 * h], [0.8 * r, b + h - 0.8], [0.8 * r - 0.5, b + h], [0, b + h]], seg, { count: 11, depth: 0.09, y0: b + 0.1 * h, y1: b + h - 0.5, sharp: 0.12 }), mats.blackGloss));
      line (g, 0.8 * r, b + h, ptr || 0xe9dfc6, 0.8 * r + 0.05); break; }
    case "knurled": { collar (0.62 * r, 0.22 * r); const b = 0.22 * r, h = 0.72 * r;
      g.add (mesh (turned ([[0, b], [r, b], [r, b + h - 0.6], [r - 0.6, b + h], [0, b + h]], seg * 2, { count: 110, depth: 0.018, y0: b + 0.3, y1: b + h - 0.7, sharp: 1 }), mats.alu));
      line (g, r, b + h, ptr || 0x111111, 0); break; }
    case "fluted": { skirt (r * 1.36, mats.blackGloss); const b = 1.6 + 0.24 * r, h = 1.25 * r;
      g.add (mesh (turned ([[0, b], [r, b], [r, b + 0.1 * h], [0.7 * r, b + h - 0.8], [0.7 * r - 0.5, b + h], [0, b + h]], seg, { count: 12, depth: 0.075, y0: b + 0.1 * h, y1: b + h - 0.5, sharp: 0.18 }), mats.blackGloss));
      line (g, 0.7 * r, b + h, ptr || 0xf2f2f2, 0.7 * r + 0.05); break; }
    case "ribbed": { const h = r;
      g.add (mesh (turned ([[0, 0], [r, 0], [r, h - 0.7], [r - 0.7, h], [0, h]], seg, { count: 28, depth: 0.026, y0: 0.12 * h, y1: h - 0.8, sharp: 0.55 }), mats.rubber));
      line (g, r, h, ptr || 0xf2f2f2, r + 0.05); break; }
    case "matte": { collar (0.72 * r, 0.2 * r); const b = 0.2 * r, h = r;
      g.add (mesh (turned ([[0, b], [r, b], [r * 1.035, b + 0.5 * h], [r, b + 0.88 * h], [r - 0.8, b + h], [0, b + h]], seg), mats.blackMatte));
      line (g, r, b + h, ptr || 0xf2f2f2, r * 1.04); break; }
    case "redtrim": { collar (0.72 * r, 0.2 * r); const b = 0.2 * r, h = r;
      g.add (mesh (turned ([[0, b], [r, b], [r, b + h - 0.6], [r - 0.6, b + h], [0, b + h]], seg, { count: 40, depth: 0.012, y0: b + 0.12 * h, y1: b + h - 0.7, sharp: 0.8 }), mats.red));
      line (g, r, b + h, ptr || 0xf2f2f2, r + 0.05); break; }
    case "capblue": case "capred": case "capwhite": { collar (0.7 * r, 0.22 * r); const b = 0.22 * r, h = 1.3 * r;
      g.add (mesh (turned ([[0, b], [r, b], [r, b + h - 0.6], [r - 0.6, b + h], [0, b + h]], seg, { count: 40, depth: 0.014, y0: b + 0.12 * h, y1: b + h - 0.7, sharp: 0.7 }), mats.grey));
      const cr = 0.92 * r, cm = { capblue: mats.capBlue, capred: mats.capRed, capwhite: mats.capWhite }[style];
      g.add (mesh (turned ([[0, b + h - 0.3], [cr, b + h - 0.3], [cr, b + h + 0.2], [cr * 0.7, b + h + 0.8], [cr * 0.3, b + h + 1.05], [0, b + h + 1.1]], seg), cm));
      line (g, cr, b + h + 1.1, ptr || (style === "capwhite" ? 0x222222 : 0xf2f2f2), 0); break; }
    case "chicken": { const h = 0.85 * r;
      g.add (mesh (turned ([[0, 0], [0.65 * r, 0], [0.65 * r, h * 0.5], [0.55 * r, h * 0.6], [0, h * 0.6]], seg), mats.blackGloss));
      const s = new Shape(); s.moveTo (-0.3 * r, -0.5 * r); s.lineTo (-0.16 * r, 1.15 * r); s.quadraticCurveTo (0, 1.25 * r, 0.16 * r, 1.15 * r); s.lineTo (0.3 * r, -0.5 * r); s.quadraticCurveTo (0, -0.62 * r, -0.3 * r, -0.5 * r);
      const beak = mesh (new ExtrudeGeometry (s, { depth: h * 0.55, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.5, bevelSegments: 3, curveSegments: 12 }), mats.blackGloss); beak.position.z = h * 0.5; g.add (beak);
      const t = mesh (new BoxGeometry (0.6, r * 1.1, 0.12), new MeshStandardMaterial ({ color: ptr || 0xefe5cc, roughness: 0.4 }), false); t.position.set (0, r * 0.5, h * 1.05 + 0.6); g.add (t); break; }
    case "pointer": { const h = 0.8 * r;
      g.add (mesh (turned ([[0, 0], [0.7 * r, 0], [0.7 * r, h], [0, h]], seg), mats.blackGloss));
      const bar = mesh (new BoxGeometry (0.4 * r, 1.9 * r, h * 0.85), mats.blackGloss); bar.position.set (0, 0.25 * r, h * 0.42); g.add (bar);
      const t = mesh (new BoxGeometry (0.7, r * 1.4, 0.12), new MeshStandardMaterial ({ color: ptr || 0xf2f2f2 }), false); t.position.set (0, 0.4 * r, h * 0.85 + 0.07); g.add (t); break; }
  }
  g.rotation.z = -deg * Math.PI / 180;
  return g;
}

// ----------------------------------------------------------------------------------------------------
// Sockets (every type the designer has), and a plugged cable
const colourMat = (hex, o = {}) => new MeshPhysicalMaterial (Object.assign ({ color: new Color (/^#[0-9a-f]{6}$/i.test (hex) ? hex : "#1a1a1c"), roughness: 0.4, clearcoat: 0.4 }, o));

/** A cable: short cylinders along a smooth path (x, y, z points), all black rubber. */
function cable (pts, radius, colour) {
  const g = new Group(), cyl = new CylinderGeometry (radius, radius, 1, 12).rotateX (Math.PI / 2);
  const mat = colour && colour !== "#141416" ? colourMat (colour, { roughness: 0.6, clearcoat: 0.1 }) : mats.rubber;
  for (let i = 0; i + 1 < pts.length; ++i) {
    const a = new Vector3 (...pts[i]), b = new Vector3 (...pts[i + 1]), len = a.distanceTo (b);
    const m = mesh (cyl, mat); m.scale.set (1, 1, len + radius * 0.5);
    m.position.copy (a).lerp (b, 0.5); m.lookAt (b); g.add (m);
  }
  return g;
}
/** Points along a cubic curve (for a cable's hang). */
function curve (p0, p1, p2, p3, n = 16) {
  const out = [];
  for (let i = 0; i <= n; ++i) { const t = i / n, u = 1 - t;
    out.push ([0, 1, 2].map ((k) => u * u * u * p0[k] + 3 * u * u * t * p1[k] + 3 * u * t * t * p2[k] + t * t * t * p3[k])); }
  return out;
}

function jack3d (p, r) {
  const types = D.JACK_TYPES || {}, [, shape, accent = "#1a1a1c"] = types[p.style] || ["", "round"];
  const g = new Group(), nutMat = metalMat (p.nut);
  const disc = (rr, z, mat) => { const d = mesh (new CircleGeometry (rr, 32).translate (0, 0, z), mat, false); g.add (d); return d; };
  const ringT = (r0, r1, h, mat) => g.add (mesh (turned ([[r0, 0], [r1, 0], [r1, h * 0.7], [r1 - 0.2, h], [r0, h]], 40), mat));
  const box = (w, h, d, mat, x = 0, y = 0, z = 0) => { const b = mesh (new BoxGeometry (w, h, d), mat); b.position.set (x, y, z); g.add (b); return b; };
  const pins = (list, rr, mat = mats.brass, z = 0.35) => { for (const [x, y] of list) { const pn = mesh (new CircleGeometry (rr, 12), mat, false); pn.position.set (x, y, z); g.add (pn); } };
  switch (shape) {
    case "round": case "mini": case "tt": { const k = shape === "mini" ? 0.6 : shape === "tt" ? 0.5 : 1; ringT (r * k * 0.78, r * k + 0.6, 1.8, nutMat); disc (r * k * 0.78, 0.3, mats.dark); disc (r * k * 0.3, 0.32, mats.brass); break; }
    case "xlrf": case "combo": ringT (r * 0.78, r + 0.6, 1.8, nutMat); disc (r * 0.78, 0.3, mats.dark);
      pins ([[-r * 0.3, r * 0.15], [r * 0.3, r * 0.15], [0, -r * 0.3]], r * 0.1); if (shape === "combo") disc (r * 0.24, 0.34, mats.grey); break;
    case "xlrm": ringT (r * 0.8, r + 0.6, 1.8, nutMat); disc (r * 0.8, 0.3, mats.blackMatte);
      for (const [x, y] of [[-r * 0.3, r * 0.15], [r * 0.3, r * 0.15], [0, -r * 0.3]]) { const pn = mesh (new CylinderGeometry (r * 0.1, r * 0.1, 1.4, 12).rotateX (Math.PI / 2), mats.brass); pn.position.set (x, y, 1.0); g.add (pn); } break;
    case "rca": ringT (r * 0.62, r * 0.9, 1.4, colourMat (accent)); g.add (mesh (new CylinderGeometry (r * 0.55, r * 0.6, 3.2, 24).rotateX (Math.PI / 2).translate (0, 0, 1.6), mats.alu)); disc (r * 0.36, 3.22, mats.dark); break;
    case "bnc": g.add (mesh (new CylinderGeometry (r * 0.75, r * 0.78, 3.0, 24).rotateX (Math.PI / 2).translate (0, 0, 1.5), mats.chrome));
      for (const x of [-r * 0.8, r * 0.8]) { const n = mesh (new SphereGeometry (r * 0.12, 10, 8), mats.chrome); n.position.set (x, 0, 2.2); g.add (n); }
      disc (r * 0.55, 3.02, colourMat ("#e8e2d0")); disc (r * 0.12, 3.04, mats.brass); break;
    case "toslink": box (r * 1.6, r * 1.6, 1.4, mats.blackMatte, 0, 0, 0.7); box (r * 1.0, r * 1.0, 0.2, mats.grey, 0, 0, 1.45); break;
    case "din": ringT (r * 0.8, r + 0.6, 1.8, nutMat); disc (r * 0.8, 0.3, mats.dark);
      pins (Array.from ({ length: 5 }, (_, i) => { const a = Math.PI + i * Math.PI / 4; return [Math.cos (a) * r * 0.5, -Math.sin (a) * r * 0.5]; }), r * 0.09); break;
    case "banana": g.add (mesh (new CylinderGeometry (r * 0.85, r * 0.85, 2.6, 6).rotateX (Math.PI / 2).translate (0, 0, 1.3), colourMat (accent)));
      g.add (mesh (new CylinderGeometry (r * 0.45, r * 0.45, 3.4, 20).rotateX (Math.PI / 2).translate (0, 0, 1.7), mats.alu)); disc (r * 0.2, 3.42, mats.dark); break;
    case "speakon": ringT (r * 0.82, r + 0.6, 2.2, colourMat (accent)); disc (r * 0.82, 0.3, mats.dark); disc (r * 0.42, 0.34, mats.grey); break;
    case "iec": box (r * 2, r * 1.5, 1.6, mats.blackMatte, 0, 0, 0.8); for (const [x, y] of [[-r * 0.45, r * 0.1], [r * 0.45, r * 0.1], [0, -r * 0.35]]) box (r * 0.14, r * 0.36, 1.0, mats.alu, x, y, 1.2); break;
    case "dc": ringT (r * 0.4, r * 0.7, 1.4, mats.chrome); disc (r * 0.4, 0.3, mats.dark); disc (r * 0.1, 0.32, mats.alu); break;
    case "usba": box (r * 1.8, r * 0.7, 0.8, mats.alu, 0, 0, 0.4); box (r * 1.6, r * 0.54, 0.1, mats.dark, 0, 0, 0.82); box (r * 1.4, r * 0.22, 0.12, mats.capWhite, 0, r * 0.12, 0.86); break;
    case "usbb": box (r * 1.2, r * 1.2, 0.8, mats.alu, 0, 0, 0.4); box (r * 0.9, r * 0.9, 0.1, mats.dark, 0, 0, 0.82); box (r * 0.5, r * 0.5, 0.12, mats.capWhite, 0, 0, 0.86); break;
    case "usbc": box (r * 1.6, r * 0.6, 0.8, mats.alu, 0, 0, 0.4); box (r * 1.36, r * 0.4, 0.1, mats.dark, 0, 0, 0.82); break;
    case "rj45": box (r * 1.6, r * 1.3, 1.0, mats.alu, 0, 0, 0.5); box (r * 1.3, r * 1.0, 0.1, mats.dark, 0, 0, 1.02); break;
    case "dsub": box (r * 3.2, r * 1.0, 1.0, mats.alu, 0, 0, 0.5); box (r * 2.8, r * 0.76, 0.1, mats.blackMatte, 0, 0, 1.02);
      for (const x of [-r * 1.95, r * 1.95]) g.add (mesh (new CylinderGeometry (r * 0.22, r * 0.22, 1.2, 6).rotateX (Math.PI / 2).translate (x, 0, 0.6), mats.chrome)); break;
    default: ringT (r * 0.78, r + 0.6, 1.8, nutMat); disc (r * 0.78, 0.3, mats.dark);
  }
  if (p.plugged) {
    // Its plug, then the cable out of its back, hanging down off the front of the panel
    const round = ["round", "mini", "tt", "xlrf", "xlrm", "combo", "rca", "bnc", "din", "banana", "speakon", "dc"].includes (shape);
    const pr = r * (shape === "mini" || shape === "dc" ? 0.6 : shape === "tt" ? 0.5 : 0.95), len = pr * 2.4 + 4;
    if (round) {
      g.add (mesh (turned ([[0, 1.5], [pr, 1.5], [pr, 1.5 + len * 0.55], [pr * 0.8, 1.5 + len * 0.75], [pr * 0.45, 1.5 + len], [0, 1.5 + len]], 32,
                           { count: 18, depth: 0.05, y0: 1.8, y1: 1.5 + len * 0.5, sharp: 0.6 }),
                    shape === "xlrf" || shape === "combo" || shape === "speakon" ? mats.alu : mats.blackGloss));
    } else {
      const w = shape === "dsub" ? r * 3.4 : r * 1.8, h = shape === "dsub" ? r * 1.2 : r * 1.4;
      box (w, h, len * 0.7, mats.blackGloss, 0, 0, 1.2 + len * 0.35);
    }
    const cr = Math.max (0.8, pr * 0.28), z0 = 1.5 + len;
    g.add (cable (curve ([0, 0, z0], [0, 0, z0 + r * 1.5], [r * 0.5, -r * 3, z0 + r * 2.5], [r * 1.5, -r * 9, z0 + r * 2]), cr, p.cable));
  }
  return g;
}

/** Chrome, black or brass (gold) metal, as a part asks for. */
function metalMat (m) { return m === "black" ? mats.blackMetal : m === "brass" || m === "gold" ? mats.brass : mats.chrome; }

function screw (style, r, metal = "chrome") {
  const g = new Group();
  g.add (mesh (turned ([[0, 0], [r, 0], [r, 0.3], [r * 0.85, r * 0.45], [0, r * 0.55]], 32), metalMat (metal)));
  const slot = new MeshStandardMaterial ({ color: 0x222222, roughness: 0.6 });
  const bar = (rot, len) => { const s = mesh (new BoxGeometry (r * len, r * 0.2, 0.3), slot, false); s.rotation.z = rot; s.position.z = r * 0.5; g.add (s); };
  if (style === "phillips") { bar (0, 1.1); bar (Math.PI / 2, 1.1); }
  else if (style === "flat") bar (Math.PI / 5, 1.5);
  else if (style === "hex" || style === "torx") { const s = mesh (new CylinderGeometry (r * 0.45, r * 0.45, 0.4, style === "hex" ? 6 : 12).rotateX (Math.PI / 2), slot, false); s.position.z = r * 0.45; g.add (s); }
  else { g.add (mesh (turned ([[0, 0.3], [r * 1.25, 0.3], [r * 1.25, r * 0.9], [0, r * 1.0]], 32, { count: 24, depth: 0.08, y0: 0.4, y1: r * 0.85, sharp: 0.7 }), metal === "chrome" ? mats.aluSatin : metalMat (metal))); }
  return g;
}

/** A glowing lens: lit, it glows its colour; off, a dark tint of it. */
const lensMat = (hex, lit) => new MeshPhysicalMaterial ({ color: hex, roughness: 0.15, clearcoat: 1,
  emissive: lit ? new Color (hex).multiplyScalar (1.4) : new Color (hex).multiplyScalar (0.05) });

// ----------------------------------------------------------------------------------------------------
// Building the unit
let texture = null, pending = null;
function schedule () { clearTimeout (pending); pending = setTimeout (rebuild, 60); }

async function rebuild () {
  if (!renderer) return;
  const d = D.get(), u = d.unit, W = D.W, H = u.height * D.U, EAR = 15;
  // Throw the old one away (geometry and textures), keep the shared materials
  root.traverse ((o) => { if (o.geometry) o.geometry.dispose(); if (o.material && !Object.values (mats).includes (o.material)) { if (o.material.map) o.material.map.dispose(); o.material.dispose(); } });
  root.clear();

  // The faceplate: a plate of the design's edge, its print laid on the front
  const rx = u.edge === "square" ? 0.3 : u.edge === "bevel" ? 0.8 : 1.6;
  // Each finish's own surface: how rough, how metallic, how much clear coat (the designer's finish table)
  const pbr = D.finishPbr (u.finish, u.shine);
  const plateMat = new MeshPhysicalMaterial ({ color: u.colour, roughness: pbr.roughness, metalness: pbr.metalness, clearcoat: pbr.clearcoat, clearcoatRoughness: 0.12 });
  root.add (mesh (new ExtrudeGeometry (rrect (W, H, rx), { depth: 3, bevelEnabled: u.edge !== "square", bevelThickness: 0.6, bevelSize: u.edge === "bevel" ? 1.0 : 0.5, bevelSegments: u.edge === "bevel" ? 1 : 3, curveSegments: 8 })
    .translate (0, 0, -3.65), plateMat));   // (its front, bevel and all, just behind the printed face)
  const canvasTex = await D.printCanvas (6);
  if (canvasTex) {
    if (texture) texture.dispose();
    texture = new CanvasTexture (canvasTex); texture.colorSpace = SRGBColorSpace; texture.anisotropy = 4;
    const face = mesh (new PlaneGeometry (W - 0.6, H - 0.6), new MeshPhysicalMaterial ({ map: texture, roughness: pbr.roughness,
      metalness: pbr.metalness * 0.9, clearcoat: pbr.clearcoat, clearcoatRoughness: 0.1 }), false);
    face.position.z = 0.02; root.add (face);
  }
  // The chassis behind it
  const depth = u.depth || 180;
  const ch = mesh (new BoxGeometry (W - 2 * EAR - 4, H - 2, depth), colourMat (u.chassis || "#1a1a1c", { metalness: 0.6, roughness: 0.55, clearcoat: 0 })); ch.position.z = -3 - depth / 2; root.add (ch);

  const at = (o, x, y, z = 0) => { o.position.set (x - W / 2, H / 2 - y, z); root.add (o); return o; };
  if (u.ears !== "none") for (const x of [EAR / 2, W - EAR / 2]) for (let k = 0; k < u.height; ++k) at (screw (u.screws, 3.2, u.screwMetal), x, (k + 0.5) * D.U, 0.1);
  if (u.handles !== "none") for (const x of [EAR + 6, W - EAR - 6]) {
    if (u.handles === "bar") { const b = mesh (new ExtrudeGeometry (rrect (5, H - 8, 2.5), { depth: 6, bevelEnabled: true, bevelThickness: 1.2, bevelSize: 1.2, bevelSegments: 4 }), mats.chrome); at (b, x, H / 2, 1.5); }
    else { const n = 14; for (let i = 0; i < n; ++i) { const t0 = i / n, t1 = (i + 1) / n, pt = (t) => { const a = Math.PI * t; return [Math.sin (a) * 11, (H / 2 - 5) * Math.cos (a)]; };
      const [z0, y0] = pt (t0), [z1, y1] = pt (t1), len = Math.hypot (z1 - z0, y1 - y0) + 0.4;
      const c = mesh (new CylinderGeometry (1.7, 1.7, len, 12), mats.chrome); c.position.set (x - W / 2, (y0 + y1) / 2, 1 + (z0 + z1) / 2); c.rotation.x = Math.atan2 (z1 - z0, y1 - y0) * -1 + 0; c.rotation.x = -Math.atan2 (z1 - z0, y1 - y0); root.add (c); } }
  }

  for (const p of d.parts) {
    const r = Math.min (p.w, p.h) / 2; let o = null;
    switch (p.type) {
      case "knob": o = knob (p.style, r, D.knobAngle ? D.knobAngle (p) : -135 + 2.7 * p.value, p.pointer);
        if (p.ring) {   // the LED ring: small lenses round it, lit up to where it points (from the middle, centre-zero)
          const ring = new Group(), n = 15, R = r + 2.4, sweep = p.sweep || 270;
          for (let i = 0; i < n; ++i) { const t = i / (n - 1) * 100, a = (-sweep / 2 + sweep * t / 100) * Math.PI / 180;
            const lit = p.bipolar ? (p.value >= 50 ? t >= 50 && t <= p.value : t <= 50 && t >= p.value) : t <= p.value;
            const l = mesh (new SphereGeometry (0.6, 10, 6, 0, Math.PI * 2, 0, Math.PI / 2).rotateX (Math.PI / 2), lensMat (p.ringColour, lit), false);
            l.position.set (Math.sin (a) * R, Math.cos (a) * R, 0.1); ring.add (l); }
          const g2 = new Group(); g2.add (o); g2.add (ring); o = g2;
        }
        break;
      case "selector": { const n = Math.max (2, String (p.stops || "").split ("|").filter (Boolean).length), sweep = p.sweep || 240;
        o = knob (p.style, r, -sweep / 2 + sweep * Math.min (n - 1, p.value) / Math.max (1, n - 1), p.pointer); break; }
      case "slider": { o = new Group(); const hz = p.horizontal, L = hz ? p.w : p.h, T = hz ? p.h : p.w, capL = Math.min (10, L * 0.18);
        const slot = mesh (new BoxGeometry (hz ? L : 1.8, hz ? 1.8 : L, 0.4), mats.dark, false); slot.position.z = 0.1; o.add (slot);
        const pos = -L / 2 + L * p.value / 100;
        const capMat = { black: mats.blackGloss, silver: mats.alu, white: mats.capWhite, red: mats.capRed }[p.style] || mats.blackGloss;
        const cap = mesh (new ExtrudeGeometry (rrect (hz ? capL : T, hz ? T : capL, 1), { depth: 5, bevelEnabled: true, bevelThickness: 0.6, bevelSize: 0.5, bevelSegments: 3 }), capMat);
        cap.position.set (hz ? pos : 0, hz ? 0 : pos, 1.2); o.add (cap);
        const stem = mesh (new BoxGeometry (hz ? 1.2 : 1.6, hz ? 1.6 : 1.2, 1.4), mats.aluSatin); stem.position.set (hz ? pos : 0, hz ? 0 : pos, 0.7); o.add (stem);
        const lineM = mesh (new BoxGeometry (hz ? 0.4 : T * 0.9, hz ? T * 0.9 : 0.4, 0.1), new MeshStandardMaterial ({ color: p.style === "white" || p.style === "silver" ? 0x111111 : 0xf2f2f2 }), false);
        lineM.position.set (hz ? pos : 0, hz ? 0 : pos, 7.35); o.add (lineM); break; }
      case "lamp": { o = new Group(); const lit = p.on;
        if (p.style === "square") { o.add (mesh (new ExtrudeGeometry (rrect (2 * r + 1.6, 2 * r + 1.6, 1), { depth: 1.2, bevelEnabled: true, bevelThickness: 0.3, bevelSize: 0.3, bevelSegments: 2 }), mats.chrome));
          const lens = mesh (new ExtrudeGeometry (rrect (2 * r, 2 * r, 0.6), { depth: 2.4, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.4, bevelSegments: 3 }), lensMat (p.colour, lit), false); o.add (lens); }
        else { o.add (mesh (turned ([[0, 0], [r + 0.9, 0], [r + 0.9, 1.6], [r * 0.95, 2.2], [0, 2.2]], 32), mats.chrome));
          const lens = p.style === "jewel" ? new SphereGeometry (r * 0.95, 8, 4, 0, Math.PI * 2, 0, Math.PI / 2) : new SphereGeometry (r * 0.95, 32, 12, 0, Math.PI * 2, 0, Math.PI / 2);
          const l = mesh (lens.rotateX (Math.PI / 2).scale (1, 1, 0.9).translate (0, 0, 2.0), lensMat (p.colour, lit), false); if (p.style === "jewel") l.material.flatShading = true; o.add (l); }
        break; }
      case "plate": { o = new Group();   // (the plate and its engraving are in the print - flat, as a real one is - its screws are real)
        if (p.screws) for (const x of [-p.w / 2 + 2.6, p.w / 2 - 2.6]) { const sc = screw ("phillips", 1.3, p.style === "black" ? "black" : p.style === "silver" ? "chrome" : "brass"); sc.position.set (x, 0, 0.1); o.add (sc); }
        break; }
      case "toggle": o = new Group();
        { const tilt = p.three && p.mid ? 0 : p.on ? -1 : 1;
        if (p.style === "bat" || p.style === "mini" || p.style === "paddle") { const k = p.style === "mini" ? 0.7 : 1;
          o.add (mesh (new CylinderGeometry (3.4 * k, 3.4 * k, 1.4, 6).rotateX (Math.PI / 2).translate (0, 0, 0.7), mats.chrome));
          const lever = p.style === "paddle" ? mesh (new ExtrudeGeometry (rrect (4.4, 1.6, 0.6), { depth: 9, bevelEnabled: true, bevelThickness: 0.4, bevelSize: 0.3, bevelSegments: 2 }), mats.blackGloss)
                                             : mesh (turned ([[1.0 * k, 0], [0.75 * k, 7.5 * k], [1.25 * k, 8.2 * k], [1.3 * k, 8.8 * k], [0, 9.3 * k]], 20), mats.chrome);
          lever.rotation.x = tilt * 0.42; lever.position.z = 1.4; o.add (lever); }
        else if (p.style === "slide") { o.add (mesh (new ExtrudeGeometry (rrect (4.8, 14, 1), { depth: 0.6, bevelEnabled: false }), mats.dark));
          const knobS = mesh (new ExtrudeGeometry (rrect (4, 6, 0.8), { depth: 2.4, bevelEnabled: true, bevelThickness: 0.3, bevelSize: 0.3, bevelSegments: 2 }), mats.blackGloss);
          knobS.position.y = -tilt * 3.5; o.add (knobS); }
        else { o.add (mesh (new ExtrudeGeometry (rrect (8, 14, 1.2), { depth: 1.2, bevelEnabled: false }), mats.dark));
          const pad = mesh (new ExtrudeGeometry (rrect (6.2, 12.2, 1), { depth: 2.6, bevelEnabled: true, bevelThickness: 0.4, bevelSize: 0.4, bevelSegments: 2 }),
            p.style === "rockerred" ? new MeshPhysicalMaterial ({ color: 0xb3231c, roughness: 0.3, clearcoat: 0.8, emissive: p.on ? 0x400000 : 0 }) : mats.blackGloss);
          pad.rotation.x = -tilt * 0.14; pad.position.z = 0.6; o.add (pad); } }
        break;
      case "button": o = new Group(); o.add (mesh (new ExtrudeGeometry (rrect (p.w + 1.6, p.h + 1.6, 1.4), { depth: 1, bevelEnabled: false }), mats.dark));
        if (p.led) { const l = mesh (new SphereGeometry (1.1, 16, 8, 0, Math.PI * 2, 0, Math.PI / 2).rotateX (Math.PI / 2), lensMat (p.colour, p.on), false); l.position.set (0, p.h / 2 + 3.5, 0.3); o.add (l); }
        o.add (mesh (new ExtrudeGeometry (rrect (p.w, p.h, p.style === "round" || p.style === "pill" ? Math.min (p.w, p.h) / 2 : 1), { depth: p.on ? 1.6 : 3, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.5, bevelSegments: 3 }),
          new MeshPhysicalMaterial ({ color: p.colour, roughness: 0.35, clearcoat: 0.7, emissive: p.on ? new Color (p.colour).multiplyScalar (0.7) : 0 })));
        break;
      case "led": { o = new Group(); const round = !p.shape || p.shape === "round";
        const bw = p.shape === "rect" ? r * 3 : r * 2, bh = p.shape === "rect" ? r * 1.2 : r * 2;
        if (p.bezel !== "none") o.add (round ? mesh (new CylinderGeometry (r + 0.6, r + 0.6, 0.8, 24).rotateX (Math.PI / 2).translate (0, 0, 0.4), p.bezel === "black" ? mats.blackMetal : mats.chrome)
                                             : mesh (new ExtrudeGeometry (rrect (bw + 1.2, bh + 1.2, 0.5), { depth: 0.8, bevelEnabled: false }), p.bezel === "black" ? mats.blackMetal : mats.chrome));
        if (round) o.add (mesh (new SphereGeometry (r, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2).rotateX (Math.PI / 2).translate (0, 0, 0.8), lensMat (p.colour, p.on), false));
        else if (p.shape === "triangle") { const t = new Shape(); t.moveTo (0, r * 1.1); t.lineTo (r, -r * 0.7); t.lineTo (-r, -r * 0.7); t.closePath();
          o.add (mesh (new ExtrudeGeometry (t, { depth: 1.2, bevelEnabled: true, bevelThickness: 0.3, bevelSize: 0.2, bevelSegments: 2 }), lensMat (p.colour, p.on), false)); }
        else o.add (mesh (new ExtrudeGeometry (rrect (bw, bh, r * 0.2), { depth: 1.2, bevelEnabled: true, bevelThickness: 0.3, bevelSize: 0.2, bevelSegments: 2 }), lensMat (p.colour, p.on), false));
        break; }
      case "jack": o = jack3d (p, r); break;
      case "screw": o = screw (p.style && p.style !== "unit" ? p.style : u.screws, r, p.metal && p.metal !== "unit" ? p.metal : u.screwMetal); break;
      case "vu": case "display": { o = new Group(); const gl = mesh (new PlaneGeometry (p.w, p.h), mats.glass, false); gl.position.z = 0.25; o.add (gl); break; }
      case "ladder": { o = new Group(); const n = p.segments, hz = p.horizontal, L = hz ? p.w : p.h, T = hz ? p.h : p.w, sh = L / n, litN = Math.round (n * p.value / 100);
        const pal = { green: 0x46e070, blue: 0x3aa0ff, amber: 0xffb020, white: 0xf4f6ff, red: 0xff3b30 }[p.palette];
        for (let i = 0; i < n; ++i) { if (!(i < litN || (p.peak && i === Math.min (n - 1, litN + 1)))) continue;
          const c = pal || (i >= n - 1 ? 0xff4a3a : i >= n - 3 ? 0xffcc33 : 0x46e070), at = -L / 2 + (i + 0.5) * sh;
          const s = mesh (new BoxGeometry (hz ? sh * 0.72 : T, hz ? T : sh * 0.72, 0.3), new MeshStandardMaterial ({ color: c, emissive: c, emissiveIntensity: 1.2 }), false);
          s.position.set (hz ? at : 0, hz ? 0 : at, 0.2); o.add (s); }
        break; }
      default: break;
    }
    if (o) { o.userData.id = p.id; o.traverse ((c) => (c.userData.id = p.id)); if (p.rot && ["jack", "plate"].includes (p.type)) o.rotation.z = -p.rot * Math.PI / 180; at (o, p.x, p.y, 0.02); }
  }

  // The selected part: a thin outline around it
  for (const id of D.selected()) {
    const p = d.parts.find ((q) => q.id === id); if (!p) continue;
    const e = new LineSegments (new EdgesGeometry (new BoxGeometry ((D.extent ? D.extent (p).w : p.w) + 3, (D.extent ? D.extent (p).h : p.h) + 3, 0.5)), new LineBasicMaterial ({ color: 0x7fe3e0 }));
    at (e, p.x, p.y, 0.3);
  }
  if (!camSet) frame (W, H);
  dirty = true;
}

// ----------------------------------------------------------------------------------------------------
// Camera: turn by dragging, come closer with the wheel, click a part to select it
let yaw = -0.35, pitch = 0.18, dist = 0, camSet = false;
function frame (W, H) { dist = Math.max (W / (2 * Math.tan (14 * Math.PI / 180) * camera.aspect), H / (2 * Math.tan (14 * Math.PI / 180))) * 1.15; camSet = true; }
function placeCamera () {
  camera.position.set (Math.sin (yaw) * Math.cos (pitch) * dist, Math.sin (pitch) * dist, Math.cos (yaw) * Math.cos (pitch) * dist);
  camera.lookAt (0, 0, 0);
}
function bindPointer () {
  let down = null;
  canvas.addEventListener ("pointerdown", (e) => { down = { x: e.clientX, y: e.clientY, yaw, pitch, moved: false }; try { canvas.setPointerCapture (e.pointerId); } catch (_) { /* fine */ } });
  canvas.addEventListener ("pointermove", (e) => {
    if (!down) return;
    const dx = e.clientX - down.x, dy = e.clientY - down.y;
    if (Math.abs (dx) + Math.abs (dy) > 3) down.moved = true;
    yaw = Math.max (-1.2, Math.min (1.2, down.yaw - dx * 0.006)); pitch = Math.max (-0.6, Math.min (0.9, down.pitch + dy * 0.005)); dirty = true;
  });
  canvas.addEventListener ("pointerup", (e) => {
    if (down && !down.moved) {
      const rc = new Raycaster(), b = canvas.getBoundingClientRect();
      rc.setFromCamera ({ x: (e.clientX - b.left) / b.width * 2 - 1, y: -((e.clientY - b.top) / b.height * 2 - 1) }, camera);
      const hit = rc.intersectObjects (root.children, true).find ((h) => h.object.userData.id);
      if (hit) D.select (hit.object.userData.id);
    }
    down = null;
  });
  canvas.addEventListener ("wheel", (e) => { e.preventDefault(); dist = Math.max (80, Math.min (2400, dist * (1 + Math.sign (e.deltaY) * 0.1))); dirty = true; }, { passive: false });
  canvas.addEventListener ("dblclick", () => { yaw = -0.35; pitch = 0.18; camSet = false; rebuild(); });
}

function loop () {
  requestAnimationFrame (loop);
  if (!dirty || view === "2d" || !renderer) return;
  dirty = false; placeCamera(); renderer.render (scene, camera);
}

// A shared link (or ?view=3d) may ask for the 3D view first
if (/[?&]view=3d\b/.test (location.search)) setView ("3d");
note.textContent = "Drag to turn · wheel to come closer · click a part to select it · double-click to reset";
