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
  if (v !== "2d") { if (!started) start(); else { resize(); rebuild(); } }
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
function knob (style, r, deg, pointer) {
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

function screw (style, r) {
  const g = new Group();
  g.add (mesh (turned ([[0, 0], [r, 0], [r, 0.3], [r * 0.85, r * 0.45], [0, r * 0.55]], 32), mats.chrome));
  const slot = new MeshStandardMaterial ({ color: 0x222222, roughness: 0.6 });
  if (style === "phillips") { for (const rot of [0, Math.PI / 2]) { const s = mesh (new BoxGeometry (r * 1.1, r * 0.2, 0.3), slot, false); s.rotation.z = rot; s.position.z = r * 0.5; g.add (s); } }
  else if (style === "hex") { const s = mesh (new CylinderGeometry (r * 0.45, r * 0.45, 0.4, 6).rotateX (Math.PI / 2), slot, false); s.position.z = r * 0.45; g.add (s); }
  else { g.add (mesh (turned ([[0, 0.3], [r * 1.25, 0.3], [r * 1.25, r * 0.9], [0, r * 1.0]], 32, { count: 24, depth: 0.08, y0: 0.4, y1: r * 0.85, sharp: 0.7 }), mats.aluSatin)); }
  return g;
}

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
  const plateMat = new MeshPhysicalMaterial ({ color: u.colour, roughness: u.finish === "brushed" ? 0.35 : u.finish === "anodised" ? 0.42 : 0.5,
    metalness: u.finish === "brushed" ? 0.85 : u.finish === "anodised" ? 0.45 : 0.1, clearcoat: u.finish === "paint" || u.finish === "hammertone" ? 0.6 : 0.2, clearcoatRoughness: 0.25 });
  root.add (mesh (new ExtrudeGeometry (rrect (W, H, rx), { depth: 3, bevelEnabled: u.edge !== "square", bevelThickness: 0.6, bevelSize: u.edge === "bevel" ? 1.0 : 0.5, bevelSegments: u.edge === "bevel" ? 1 : 3, curveSegments: 8 })
    .translate (0, 0, -3.65), plateMat));   // (its front, bevel and all, just behind the printed face)
  const canvasTex = await D.printCanvas (6);
  if (canvasTex) {
    if (texture) texture.dispose();
    texture = new CanvasTexture (canvasTex); texture.colorSpace = SRGBColorSpace; texture.anisotropy = 4;
    const face = mesh (new PlaneGeometry (W - 0.6, H - 0.6), new MeshPhysicalMaterial ({ map: texture, roughness: u.finish === "brushed" ? 0.34 : u.finish === "anodised" ? 0.4 : 0.48,
      metalness: u.finish === "brushed" ? 0.75 : u.finish === "anodised" ? 0.35 : 0.05, clearcoat: u.finish === "paint" || u.finish === "hammertone" ? 0.55 : 0.2, clearcoatRoughness: 0.22 }), false);
    face.position.z = 0.02; root.add (face);
  }
  // The chassis behind it
  const ch = mesh (new BoxGeometry (W - 2 * EAR - 4, H - 2, 180), mats.chassis); ch.position.z = -93; root.add (ch);

  const at = (o, x, y, z = 0) => { o.position.set (x - W / 2, H / 2 - y, z); root.add (o); return o; };
  if (u.ears !== "none") for (const x of [EAR / 2, W - EAR / 2]) for (let k = 0; k < u.height; ++k) at (screw (u.screws, 3.2), x, (k + 0.5) * D.U, 0.1);
  if (u.handles !== "none") for (const x of [EAR + 6, W - EAR - 6]) {
    if (u.handles === "bar") { const b = mesh (new ExtrudeGeometry (rrect (5, H - 8, 2.5), { depth: 6, bevelEnabled: true, bevelThickness: 1.2, bevelSize: 1.2, bevelSegments: 4 }), mats.chrome); at (b, x, H / 2, 1.5); }
    else { const n = 14; for (let i = 0; i < n; ++i) { const t0 = i / n, t1 = (i + 1) / n, pt = (t) => { const a = Math.PI * t; return [Math.sin (a) * 11, (H / 2 - 5) * Math.cos (a)]; };
      const [z0, y0] = pt (t0), [z1, y1] = pt (t1), len = Math.hypot (z1 - z0, y1 - y0) + 0.4;
      const c = mesh (new CylinderGeometry (1.7, 1.7, len, 12), mats.chrome); c.position.set (x - W / 2, (y0 + y1) / 2, 1 + (z0 + z1) / 2); c.rotation.x = Math.atan2 (z1 - z0, y1 - y0) * -1 + 0; c.rotation.x = -Math.atan2 (z1 - z0, y1 - y0); root.add (c); } }
  }

  for (const p of d.parts) {
    const r = Math.min (p.w, p.h) / 2; let o = null;
    switch (p.type) {
      case "knob": o = knob (p.style, r, D.knobAngle ? D.knobAngle (p) : -135 + 2.7 * p.value, p.pointer); break;
      case "toggle": o = new Group();
        if (p.style === "bat") { o.add (mesh (new CylinderGeometry (3.4, 3.4, 1.4, 6).rotateX (Math.PI / 2).translate (0, 0, 0.7), mats.chrome));
          const lever = mesh (turned ([[1.0, 0], [0.75, 7.5], [1.25, 8.2], [1.3, 8.8], [0, 9.3]], 20), mats.chrome); lever.rotation.x = (p.on ? -1 : 1) * 0.42; lever.position.z = 1.4; o.add (lever); }
        else { o.add (mesh (new ExtrudeGeometry (rrect (8, 14, 1.2), { depth: 1.2, bevelEnabled: false }), mats.dark));
          const pad = mesh (new ExtrudeGeometry (rrect (6.2, 12.2, 1), { depth: 2.6, bevelEnabled: true, bevelThickness: 0.4, bevelSize: 0.4, bevelSegments: 2 }),
            p.style === "rockerred" ? new MeshPhysicalMaterial ({ color: 0xb3231c, roughness: 0.3, clearcoat: 0.8, emissive: p.on ? 0x400000 : 0 }) : mats.blackGloss);
          pad.rotation.x = (p.on ? 1 : -1) * 0.14; pad.position.z = 0.6; o.add (pad); }
        break;
      case "button": o = new Group(); o.add (mesh (new ExtrudeGeometry (rrect (p.w + 1.6, p.h + 1.6, 1.4), { depth: 1, bevelEnabled: false }), mats.dark));
        o.add (mesh (new ExtrudeGeometry (rrect (p.w, p.h, p.style === "round" ? Math.min (p.w, p.h) / 2 : 1), { depth: p.on ? 1.6 : 3, bevelEnabled: true, bevelThickness: 0.5, bevelSize: 0.5, bevelSegments: 3 }),
          new MeshPhysicalMaterial ({ color: p.colour, roughness: 0.35, clearcoat: 0.7, emissive: p.on ? new Color (p.colour).multiplyScalar (0.7) : 0 })));
        break;
      case "led": o = new Group(); o.add (mesh (new CylinderGeometry (r + 0.6, r + 0.6, 0.8, 24).rotateX (Math.PI / 2).translate (0, 0, 0.4), mats.chrome));
        o.add (mesh (new SphereGeometry (r, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2).rotateX (Math.PI / 2).translate (0, 0, 0.8),
          new MeshPhysicalMaterial ({ color: p.colour, roughness: 0.15, transmission: 0, clearcoat: 1, emissive: p.on ? new Color (p.colour).multiplyScalar (1.4) : new Color (p.colour).multiplyScalar (0.05) }), false));
        break;
      case "jack": o = new Group(); o.add (mesh (turned ([[r * 0.78, 0], [r + 0.6, 0], [r + 0.6, 1.2], [r + 0.2, 1.8], [r * 0.78, 1.8]], 40), mats.chrome));
        o.add (mesh (new CircleGeometry (r * 0.78, 32).translate (0, 0, 0.3), mats.dark, false));
        if (p.style === "xlr") for (const [x, y] of [[-r * 0.3, r * 0.15], [r * 0.3, r * 0.15], [0, -r * 0.3]]) { const pin = mesh (new CircleGeometry (r * 0.1, 12), mats.brass, false); pin.position.set (x, y, 0.35); o.add (pin); }
        break;
      case "screw": o = screw (u.screws, r); break;
      case "vu": case "display": { o = new Group(); const gl = mesh (new PlaneGeometry (p.w, p.h), mats.glass, false); gl.position.z = 0.25; o.add (gl); break; }
      case "ladder": { o = new Group(); const n = p.segments, sh = p.h / n, lit = Math.round (n * p.value / 100);
        for (let i = 0; i < lit; ++i) { const c = i >= n - 1 ? 0xff4a3a : i >= n - 3 ? 0xffcc33 : 0x46e070;
          const s = mesh (new BoxGeometry (p.w, sh * 0.72, 0.3), new MeshStandardMaterial ({ color: c, emissive: c, emissiveIntensity: 1.2 }), false); s.position.set (0, -p.h / 2 + (i + 0.5) * sh, 0.2); o.add (s); }
        break; }
      default: break;
    }
    if (o) { o.userData.id = p.id; o.traverse ((c) => (c.userData.id = p.id)); if (p.rot && ["jack"].includes (p.type)) o.rotation.z = -p.rot * Math.PI / 180; at (o, p.x, p.y, 0.02); }
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
