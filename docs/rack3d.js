/* ENH Master site: the rack in real 3D (three.js, WebGL, vendored in vendor/three/).
   Built here from the plugin's own layout: the curved walnut case (units on an arc centred on the
   viewer, Source/UI/Scene/DeviceLayout.h), the eleven units and the POWER strip with 19-inch
   faceplates and their 1U/2U/3U heights, the LUNCHBOX on its stand. Faceplate print and displays are
   the unit art in img/units/; knobs, buttons, switches and screws are real meshes (rack3d-data.js).
   Scrolling pulls a unit out of the case and takes it apart: controls, faceplate, displays and meters,
   DSP board, chassis. Point at a part (or tab to it) to read what it does.
   Renders only while on screen and only when something moves; quality drops if frames run slow.
   No WebGL, ?static or reduced motion: see the notes at the bottom. No network, no storage. */
import {
  WebGLRenderer, Scene, PerspectiveCamera, Group, Mesh, BufferGeometry, Float32BufferAttribute,
  BoxGeometry, PlaneGeometry, CircleGeometry, CylinderGeometry, LatheGeometry, ExtrudeGeometry, SphereGeometry,
  Shape, Vector2, Vector3, Matrix4, Quaternion, Euler, Color, Raycaster, Box3,
  MeshPhysicalMaterial, MeshStandardMaterial, MeshBasicMaterial, LineBasicMaterial, LineSegments, EdgesGeometry,
  SpriteMaterial, Sprite, DirectionalLight, HemisphereLight, PMREMGenerator, CanvasTexture,
  SRGBColorSpace, RepeatWrapping, ACESFilmicToneMapping, PCFShadowMap, BackSide, AdditiveBlending, DoubleSide
} from "./vendor/three/three.module.min.js";
import { PARTS } from "./rack3d-data.js";

const doc = document, root = doc.documentElement;
const PRE = /[?&]prerender\b/.test(location.search);
const U = window.ENHUNITS;
const motion = !!window.ENHMOTION;
const heroSec = doc.getElementById("top");
const heroStage = doc.getElementById("hero-stage");
const section = doc.getElementById("inside");
const xpStage = doc.getElementById("xp-stage");
const layer = doc.querySelector(".gl-layer");

function fail(why) {
  root.classList.remove("gl-try", "gl", "pin");
  root.classList.add("gl-no");
  if (why) console.info("ENH Master: 3D view off (" + why + "); showing the still pictures.");
}

if (PRE || !U || !heroSec || !heroStage || !section || !xpStage || !layer) fail(PRE ? "" : "page parts missing");
else {
  try { start(); } catch (e) { fail(e && e.message || "error"); }
}

function start() {
  window.ENHRACK_RUNNING = true;
  /* ------------------------------------------------------------------ helpers */
  const S = 0.005;                      // 1000 px of unit art = the 19-inch faceplate = 5.0 units
  const DEG = Math.PI / 180;
  const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
  const mix = (a, b, t) => a + (b - a) * t;
  const sm = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
  const easeIO = (t) => t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
  const pad = (n) => (n < 10 ? "0" : "") + n;
  const titleCase = (s) => s.toLowerCase().replace(/(^|[\s-])([a-z])/g, (m, a, b) => a + b.toUpperCase()).replace(/ - /g, " · ");
  const el = (tag, cls, parent) => { const e = doc.createElement(tag); if (cls) e.className = cls; if (parent) parent.appendChild(e); return e; };
  const LNAME = { c: "Controls", p: "Faceplate", d: "Displays & meters", b: "DSP board", k: "Chassis" };

  /* ------------------------------------------------------------------ renderer and quality */
  const renderer = new WebGLRenderer({ antialias: true, alpha: true, powerPreference: "high-performance", failIfMajorPerformanceCaveat: true });
  const gl = renderer.getContext();
  let gpu = "";
  try { const x = gl.getExtension("WEBGL_debug_renderer_info"); gpu = String(x ? gl.getParameter(x.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)); } catch (e) { /* hidden */ }
  const coarse = matchMedia("(pointer: coarse)").matches;
  const weak = coarse || /llvmpipe|swiftshader|software|mali|adreno|powervr|(intel(?!.*(iris|arc)))/i.test(gpu);
  const Q = weak ? { dpr: 1.25, shadow: 1024, aniso: 4, texHi: 1536 } : { dpr: 1.5, shadow: 2048, aniso: 8, texHi: 2048 };
  const TEX = 1024;                      // the rack's faceplates; the unit taken apart gets texHi
  let level = 0;                          // adaptive steps: 0 full, 1 lower resolution, 2 lower still, 3 no shadows
  const LEVEL_DPR = [1, 0.84, 0.7, 0.62];
  const aniso = Math.min(Q.aniso, renderer.capabilities.getMaxAnisotropy());
  renderer.outputColorSpace = SRGBColorSpace;
  renderer.toneMapping = ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.05;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = PCFShadowMap;
  renderer.shadowMap.autoUpdate = false;     // shadows are redrawn only when something in the scene moves
  renderer.setClearColor(0x000000, 0);
  const cv = renderer.domElement;
  cv.setAttribute("aria-hidden", "true");
  cv.className = "gl-canvas";
  cv.addEventListener("webglcontextlost", (e) => { e.preventDefault(); lost = true; fail("the graphics context was lost"); }, false);
  let lost = false;

  const scene = new Scene();
  const camera = new PerspectiveCamera(30, 1, 0.1, 200);

  /* ------------------------------------------------------------------ generated textures */
  function canvas(w, h) { const c = doc.createElement("canvas"); c.width = w; c.height = h; return c; }
  function ctex(c, srgb, rep) {
    const t = new CanvasTexture(c);
    if (srgb) t.colorSpace = SRGBColorSpace;
    if (rep) { t.wrapS = t.wrapT = RepeatWrapping; }
    t.anisotropy = aniso;
    return t;
  }
  let seed = 7;
  const rnd = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646; };
  // walnut: warm brown with dark grain lines running along the boards, and a few pores
  function walnutTex() {
    const c = canvas(512, 1024), g = c.getContext("2d");
    const grd = g.createLinearGradient(0, 0, 512, 0);
    grd.addColorStop(0, "#4a2a17"); grd.addColorStop(0.3, "#5e361d"); grd.addColorStop(0.55, "#6a3e22"); grd.addColorStop(0.8, "#533019"); grd.addColorStop(1, "#472815");
    g.fillStyle = grd; g.fillRect(0, 0, 512, 1024);
    for (let i = 0; i < 150; i++) {
      const x0 = rnd() * 512, amp = 4 + rnd() * 18, f = 0.002 + rnd() * 0.006, ph = rnd() * 6.28;
      g.strokeStyle = rnd() < 0.5 ? "rgba(28,14,6," + (0.12 + rnd() * 0.3) + ")" : "rgba(140,86,48," + (0.06 + rnd() * 0.12) + ")";
      g.lineWidth = 0.6 + rnd() * 2.4;
      g.beginPath();
      for (let y = 0; y <= 1024; y += 16) { const x = x0 + Math.sin(y * f + ph) * amp + Math.sin(y * f * 3.1 + ph) * amp * 0.25; if (y) g.lineTo(x, y); else g.moveTo(x, y); }
      g.stroke();
    }
    for (let i = 0; i < 1400; i++) { g.fillStyle = "rgba(20,10,4," + (0.2 + rnd() * 0.3) + ")"; g.fillRect(rnd() * 512, rnd() * 1024, 1, 2 + rnd() * 5); }
    return ctex(c, true, true);
  }
  // brushed metal: fine horizontal streaks, used as a roughness map
  function brushTex() {
    const c = canvas(256, 256), g = c.getContext("2d");
    g.fillStyle = "rgb(120,120,120)"; g.fillRect(0, 0, 256, 256);
    for (let i = 0; i < 900; i++) {
      const v = 80 + rnd() * 90 | 0; g.fillStyle = "rgba(" + v + "," + v + "," + v + ",0.5)";
      g.fillRect(rnd() * 256 - 60, rnd() * 256, 40 + rnd() * 200, 0.6 + rnd());
    }
    return ctex(c, false, true);
  }
  function radialTex(stops) {
    const c = canvas(256, 256), g = c.getContext("2d");
    const grd = g.createRadialGradient(128, 128, 0, 128, 128, 128);
    stops.forEach((s) => grd.addColorStop(s[0], s[1]));
    g.fillStyle = grd; g.fillRect(0, 0, 256, 256);
    return ctex(c, false, false);
  }
  function dashTex() {
    const c = canvas(64, 4), g = c.getContext("2d");
    const grd = g.createLinearGradient(0, 0, 64, 0);
    grd.addColorStop(0, "rgba(255,255,255,0)"); grd.addColorStop(0.18, "rgba(255,255,255,1)"); grd.addColorStop(0.38, "rgba(255,255,255,1)"); grd.addColorStop(0.55, "rgba(255,255,255,0)"); grd.addColorStop(1, "rgba(255,255,255,0)");
    g.fillStyle = grd; g.fillRect(0, 0, 64, 4);
    const t = ctex(c, false, true); t.wrapT = RepeatWrapping; return t;
  }
  const T = { walnut: walnutTex(), brush: brushTex(), floor: radialTex([[0, "#fff"], [0.45, "#bbb"], [1, "#000"]]), glow: radialTex([[0, "rgba(255,255,255,1)"], [0.25, "rgba(255,255,255,.45)"], [1, "rgba(255,255,255,0)"]]), dash: dashTex() };
  T.walnut.repeat.set(0.3, 0.14);
  T.brush.repeat.set(6, 1.5);

  /* ------------------------------------------------------------------ materials */
  const M = {
    walnut: new MeshPhysicalMaterial({ map: T.walnut, roughness: 0.5, clearcoat: 1, clearcoatRoughness: 0.06, sheen: 0 }),
    caseBack: new MeshStandardMaterial({ color: 0x0c0806, roughness: 0.8 }),
    zinc: new MeshStandardMaterial({ color: 0x9aa1aa, metalness: 0.88, roughness: 0.4, roughnessMap: T.brush }),
    chassis: new MeshStandardMaterial({ color: 0x8d949c, metalness: 0.85, roughness: 0.42, roughnessMap: T.brush }),
    knob: new MeshPhysicalMaterial({ color: 0x0c0c0e, roughness: 0.3, clearcoat: 0.7, clearcoatRoughness: 0.22 }),
    cap: new MeshPhysicalMaterial({ color: 0x3a3c42, metalness: 0.85, roughness: 0.26, clearcoat: 0.4 }),
    pointer: new MeshStandardMaterial({ color: 0xf3eee2, emissive: 0x2a2822, roughness: 0.45 }),
    screw: new MeshStandardMaterial({ color: 0xd8dade, metalness: 1, roughness: 0.2 }),
    slot: new MeshStandardMaterial({ color: 0x2b2b30, metalness: 0.5, roughness: 0.5 }),
    button: new MeshPhysicalMaterial({ color: 0xb4b6ba, roughness: 0.36, clearcoat: 0.5 }),
    bezel: new MeshStandardMaterial({ color: 0x0b0b0c, roughness: 0.5 }),
    paddle: new MeshPhysicalMaterial({ color: 0x1c1c20, roughness: 0.32, clearcoat: 0.6 }),
    red: new MeshPhysicalMaterial({ color: 0xb3262a, emissive: 0x6a0c0c, roughness: 0.3, clearcoat: 0.8 }),
    chrome: new MeshStandardMaterial({ color: 0xe6e8ec, metalness: 1, roughness: 0.12 }),
    pcbEdge: new MeshStandardMaterial({ color: 0x0f271e, roughness: 0.6 }),
    chip: new MeshStandardMaterial({ color: 0x141619, roughness: 0.5, metalness: 0.1 }),
    gold: new MeshStandardMaterial({ color: 0xd6b56a, metalness: 1, roughness: 0.3 }),
    frame: new MeshPhysicalMaterial({ color: 0x0d0d10, roughness: 0.55, metalness: 0.3, clearcoat: 0.3 }),
    floor: new MeshStandardMaterial({ color: 0x07080b, roughness: 0.34, metalness: 0, transparent: true, alphaMap: T.floor, depthWrite: false }),
    rim: new LineBasicMaterial({ color: 0x9ff7e6, transparent: true, opacity: 0, depthWrite: false }),
    rim2: new LineBasicMaterial({ color: 0xc7a6ff, transparent: true, opacity: 0, depthWrite: false })
  };
  const capColour = { blue: 0x3d64b0, red: 0xb33c36, green: 0x3a8a56, white: 0xd9d8d0 };

  /* ------------------------------------------------------------------ geometry helpers */
  const V3 = (x, y, z) => new Vector3(x, y, z);
  function m4(x, y, z, rz, sx, sy, sz, rx) {
    return new Matrix4().compose(V3(x, y, z), new Quaternion().setFromEuler(new Euler(rx || 0, 0, rz || 0)), V3(sx || 1, sy || sx || 1, sz || sx || 1));
  }
  /** Merge [geometry, matrix] pairs into one geometry (one draw call per material per unit). */
  function merge(parts) {
    let n = 0;
    const gs = parts.map(([g, m]) => { const x = g.index ? g.toNonIndexed() : g.clone(); x.applyMatrix4(m); n += x.attributes.position.count; return x; });
    const pos = new Float32Array(n * 3), nor = new Float32Array(n * 3), uv = new Float32Array(n * 2);
    let o = 0;
    gs.forEach((x) => {
      pos.set(x.attributes.position.array, o * 3); nor.set(x.attributes.normal.array, o * 3);
      if (x.attributes.uv) uv.set(x.attributes.uv.array, o * 2);
      o += x.attributes.position.count; x.dispose();
    });
    const g = new BufferGeometry();
    g.setAttribute("position", new Float32BufferAttribute(pos, 3));
    g.setAttribute("normal", new Float32BufferAttribute(nor, 3));
    g.setAttribute("uv", new Float32BufferAttribute(uv, 2));
    g.computeBoundingSphere();
    return g;
  }
  function rrect(w, h, r) {
    const s = new Shape(), x = -w / 2, y = -h / 2;
    s.moveTo(x + r, y); s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r);
    s.lineTo(x + w, y + h - r); s.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    s.lineTo(x + r, y + h); s.quadraticCurveTo(x, y + h, x, y + h - r);
    s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y);
    return s;
  }
  const ext = (shape, depth, bevel, curve) => new ExtrudeGeometry(shape, { depth, bevelEnabled: bevel > 0, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 2, curveSegments: curve || 4 });
  const lathe = (pts, seg) => new LatheGeometry(pts.map((p) => new Vector2(p[0], p[1])), seg).rotateX(Math.PI / 2);
  function mesh(geo, mat, cast, recv) { const m = new Mesh(geo, mat); m.castShadow = !!cast; m.receiveShadow = !!recv; return m; }

  // shared unit-sized parts
  const G = {
    knob: lathe([[0, 0], [1, 0], [1, 0.14], [0.95, 0.2], [0.84, 0.24], [0.8, 0.27], [0.79, 0.84], [0.77, 0.93], [0.72, 0.99], [0.62, 1], [0, 1]], 44),
    cap: new CircleGeometry(0.6, 40).translate(0, 0, 1.004),
    pointer: new BoxGeometry(0.075, 0.46, 0.03).translate(0, 0.37, 1.012),
    screw: lathe([[0, 0], [1, 0], [1, 0.2], [0.9, 0.4], [0.62, 0.55], [0, 0.6]], 20),
    slotA: new BoxGeometry(1.15, 0.2, 0.12).translate(0, 0, 0.55),
    slotB: new BoxGeometry(0.2, 1.15, 0.12).translate(0, 0, 0.55),
    nut: new CylinderGeometry(1, 1, 1, 6).rotateX(Math.PI / 2),
    bat: new CylinderGeometry(0.28, 0.42, 1, 16).rotateX(Math.PI / 2),
    ball: new SphereGeometry(1, 16, 12),
    plane: new PlaneGeometry(1, 1)
  };
  const LX = (px) => (px - 500) * S;
  const LY = (py, hpx) => (hpx / 2 - py) * S;

  /* ------------------------------------------------------------------ the rack's layout (DeviceLayout.h) */
  const R = 9.6, CY = 1.62, CZ = 10.05, RECESS = 0.03, GAP = 0.06, OVER = 0.3;
  const CHEEK_IN = 2.615, CHEEK_W = 0.28, CASE_D = 2.6, CH_W = 4.44, CH_D = 2.0;
  const ZO = 7.0;                         // how far a unit comes out of the case
  const byId = {};
  U.forEach((u) => { byId[u.id] = u; });
  const RACK = [{ id: "power", name: "POWER", h: 118, plate: "#36373c", pos: 0 }].concat(U.slice().sort((a, b) => a.pos - b.pos));
  let ARC = 0;
  RACK.forEach((u, i) => { ARC += u.h * S + (i ? GAP : 0); });
  { let acc = 0; RACK.forEach((u) => { const hh = u.h * S / 2; acc += hh; u.arc = acc; acc += hh + GAP; u.a = (u.arc - ARC / 2) / R; }); }
  const A_LO = -(ARC / 2 + OVER) / R, A_HI = (ARC / 2 + OVER) / R;
  const FLOOR = CY + (R - 0.02) * Math.sin(A_LO) - 0.01;
  const arcPt = (a, r) => [CZ - r * Math.cos(a), Math.max(FLOOR, CY + r * Math.sin(a))];   // [z, y]

  /** A slab of the case: an annular sector of the arc (angles a0..a1, radii r0..r1), across x0..x1. */
  function sector(a0, a1, r0, r1, x0, x1, mat, bevel, n) {
    n = n || 48;
    const s = new Shape();
    const pts = [];
    for (let i = 0; i <= n; i++) pts.push(arcPt(a0 + (a1 - a0) * i / n, r0));
    for (let i = n; i >= 0; i--) pts.push(arcPt(a0 + (a1 - a0) * i / n, r1));
    pts.forEach((p, i) => { if (i) s.lineTo(-p[0], p[1]); else s.moveTo(-p[0], p[1]); });
    const g = new ExtrudeGeometry(s, { depth: x1 - x0 - 2 * bevel, bevelEnabled: bevel > 0, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 3, curveSegments: 1 });
    g.rotateY(Math.PI / 2);
    g.translate(x0 + bevel, 0, 0);
    return mesh(g, mat, true, true);
  }

  const world = new Group();
  scene.add(world);

  /* the case */
  const caseG = new Group();
  world.add(caseG);
  const rF = R - 0.02, rB = R + CASE_D;
  caseG.add(sector(A_LO, A_HI, rF, rB, -CHEEK_IN - CHEEK_W, -CHEEK_IN, M.walnut, 0.03, 72));
  caseG.add(sector(A_LO, A_HI, rF, rB, CHEEK_IN, CHEEK_IN + CHEEK_W, M.walnut, 0.03, 72));
  const edgeTop = (ARC / 2 + 0.012) / R;
  caseG.add(sector(edgeTop, A_HI, rF + 0.015, rB, -CHEEK_IN - 0.01, CHEEK_IN + 0.01, M.walnut, 0.02, 8));
  caseG.add(sector(A_LO, -edgeTop, rF + 0.015, rB, -CHEEK_IN - 0.01, CHEEK_IN + 0.01, M.walnut, 0.02, 8));
  caseG.add(sector(A_LO, A_HI, rB - 0.05, rB - 0.01, -CHEEK_IN, CHEEK_IN, M.caseBack, 0, 48));
  [-1, 1].forEach((sgn) => {                    // the rails the units are screwed to
    const x0 = sgn < 0 ? -CHEEK_IN : 2.3, x1 = sgn < 0 ? -2.3 : CHEEK_IN;
    const rl = sector(-edgeTop, edgeTop, R + 0.06, R + 0.1, x0, x1, M.zinc, 0, 60);
    rl.castShadow = false; caseG.add(rl);
  });

  /* the floor it stands on */
  const floor = new Mesh(new CircleGeometry(15, 64).rotateX(-Math.PI / 2), M.floor);
  floor.position.set(1.8, FLOOR, 1.2);
  floor.receiveShadow = true;
  world.add(floor);

  /* ------------------------------------------------------------------ units */
  const units = {};
  const tmpC = new Color();
  function plateLook(u) {
    const c = tmpC.set(u.plate);
    const l = c.r * 0.3 + c.g * 0.59 + c.b * 0.11;
    if (u.id === "tone") return { metalness: 0.25, roughness: 0.34, clearcoat: 1, clearcoatRoughness: 0.08 };
    if (l > 0.45) return { metalness: 0.45, roughness: 0.42, clearcoat: 0.3, clearcoatRoughness: 0.2 };
    return { metalness: 0.45, roughness: 0.46, clearcoat: 0.45, clearcoatRoughness: 0.16 };
  }
  function plateMaterial(u) {
    const m = new MeshPhysicalMaterial(Object.assign({ color: new Color(u.plate), roughnessMap: T.brush }, plateLook(u)));
    return m;
  }

  /** Knobs, buttons, switches and screws of one unit, as a handful of merged meshes. */
  function controls(id, hpx) {
    const P = PARTS[id], g = new Group();
    if (!P) return g;
    const body = [], caps = [], ptr = [], fins = [], heads = [], slots = [], btn = [], bez = [], pad = [], padR = [], chr = [];
    P.knobs.forEach((k) => {
      const [x, y, rs, rb, ang, fin] = k;
      const r = rs * S, hgt = rb * S * 1.25, X = LX(x), Y = LY(y, hpx);
      if (fin) {
        body.push([G.knob, m4(X, Y, 0, -ang, r * 0.78, r * 0.78, hgt * 0.8)]);
        const sh = new Shape();
        fin.forEach((p, i) => { if (i) sh.lineTo(p[0] * S, -p[1] * S); else sh.moveTo(p[0] * S, -p[1] * S); });
        fins.push([ext(sh, hgt * 0.55, 0.004, 2), m4(X, Y, hgt * 0.5)]);
      } else {
        body.push([G.knob, m4(X, Y, 0, -ang, r, r, hgt)]);
        caps.push([G.cap, m4(X, Y, 0, -ang, r, r, hgt)]);
        ptr.push([G.pointer, m4(X, Y, 0, -ang, r, r, hgt)]);
      }
    });
    P.screws.forEach(([x, y]) => {
      const X = LX(x), Y = LY(y, hpx), r = 10.5 * S, rot = (x * 7 + y * 3) % 3;
      heads.push([G.screw, m4(X, Y, 0, rot, r, r, r)]);
      slots.push([G.slotA, m4(X, Y, 0, rot, r, r, r)]);
      slots.push([G.slotB, m4(X, Y, 0, rot, r, r, r)]);
    });
    P.buttons.forEach(([x, y, w, h]) => {
      btn.push([ext(rrect(w * S - 0.01, h * S - 0.01, 0.012), 0.022, 0.005), m4(LX(x + w / 2), LY(y + h / 2, hpx), 0.005)]);
    });
    P.rockers.forEach(([x, y, w, h, red]) => {
      const X = LX(x + w / 2), Y = LY(y + h / 2, hpx);
      bez.push([ext(rrect(w * S, h * S, 0.016), 0.016, 0.003), m4(X, Y, 0.003)]);
      (red ? padR : pad).push([new BoxGeometry(w * S * 0.76, h * S * 0.86, 0.026), m4(X, Y, 0.03, 0, 1, 1, 1, red ? 0.22 : -0.22)]);
    });
    P.rounds.forEach(([x, y, r]) => {
      chr.push([new CylinderGeometry(r * S, r * S, 0.022, 32).rotateX(Math.PI / 2), m4(LX(x), LY(y, hpx), 0.011)]);
      btn.push([new CylinderGeometry(r * S * 0.62, r * S * 0.62, 0.05, 28).rotateX(Math.PI / 2), m4(LX(x), LY(y, hpx), 0.025)]);
    });
    const add = (list, mat) => { if (list.length) g.add(mesh(merge(list), mat, true, false)); };
    add(body, M.knob); add(fins, M.knob); add(caps, M.cap); add(ptr, M.pointer);
    add(heads, M.screw); add(slots, M.slot); add(btn, M.button); add(bez, M.bezel); add(pad, M.paddle); add(padR, M.red); add(chr, M.chrome);
    return g;
  }

  function plateEdges(hU, mat) {
    const t = 0.024, w = 0.006;
    return mesh(merge([
      [new BoxGeometry(5, w, t), m4(0, hU / 2 - w / 2, -t / 2 - 0.001)],
      [new BoxGeometry(5, w, t), m4(0, -hU / 2 + w / 2, -t / 2 - 0.001)],
      [new BoxGeometry(w, hU, t), m4(-2.5 + w / 2, 0, -t / 2 - 0.001)],
      [new BoxGeometry(w, hU, t), m4(2.5 - w / 2, 0, -t / 2 - 0.001)]
    ]), mat, false, true);
  }

  RACK.forEach((u) => {
    const hU = u.h * S, r = R + RECESS;
    const g = new Group();
    g.position.set(0, CY + r * Math.sin(u.a), CZ - r * Math.cos(u.a));
    g.rotation.x = u.a;
    const slide = new Group();
    g.add(slide);
    const L = {};
    ["k", "b", "d", "p", "c"].forEach((k) => { L[k] = new Group(); slide.add(L[k]); });
    const mat = plateMaterial(u);
    const plate = mesh(new PlaneGeometry(5, hU), mat, false, true);
    plate.userData.unit = u.id;
    L.p.add(plate);
    L.p.add(plateEdges(hU, new MeshStandardMaterial({ color: new Color(u.plate).multiplyScalar(0.7), metalness: 0.6, roughness: 0.4 })));
    if (u.id !== "power") L.c.add(controls(u.id, u.h));
    const box = mesh(new BoxGeometry(CH_W, hU - 0.03, CH_D), M.chassis, false, false);
    box.position.z = -0.03 - CH_D / 2;
    L.k.add(box);
    world.add(g);
    units[u.id] = { u, g, slide, L, plate, mat, box, hU, a: u.a, z: 0, tilt: 0 };
  });

  /* the POWER strip: its print drawn here (it has no art file), mains switch and three lamps */
  (function power() {
    const o = units.power, hpx = 118, c = canvas(TEX, Math.round(TEX * hpx / 1000)), g = c.getContext("2d"), k = TEX / 1000;
    g.scale(k, k);
    const grd = g.createLinearGradient(0, 0, 0, hpx);
    grd.addColorStop(0, "#3e3f44"); grd.addColorStop(0.1, "#35363b"); grd.addColorStop(0.9, "#2f3035"); grd.addColorStop(1, "#26272b");
    g.fillStyle = grd; g.fillRect(0, 0, 1000, hpx);
    g.fillStyle = "#101114";
    [[20, 22], [20, 84], [964, 22], [964, 84]].forEach(([x, y]) => { g.beginPath(); g.roundRect ? g.roundRect(x, y, 16, 12, 6) : g.rect(x, y, 16, 12); g.fill(); });
    g.fillStyle = "#e7e3da"; g.font = "800 17px system-ui, sans-serif"; g.textBaseline = "middle";
    if ("letterSpacing" in g) g.letterSpacing = "3px";
    g.fillText("POWER", 70, 50);
    g.fillStyle = "#a9a59c"; g.font = "600 8.5px system-ui, sans-serif";
    if ("letterSpacing" in g) g.letterSpacing = "1.6px";
    g.fillText("CONDITIONED POWER · RACK LIGHTS", 70, 72);
    g.textAlign = "center";
    [["PROTECTED", 610], ["GROUNDED", 710], ["POWER", 810]].forEach(([t, x]) => g.fillText(t, x, 84));
    g.strokeStyle = "rgba(231,227,218,.35)"; g.lineWidth = 1; g.strokeRect(372, 30, 72, 58);
    g.fillText("MAINS", 408, 100);
    const tex = ctex(c, true, false);
    o.mat.map = tex; o.mat.color.set(0xffffff); o.mat.needsUpdate = true;
    const X = (x) => x * 1, s = o.L.c;
    const bez = mesh(ext(rrect(0.13, 0.2, 0.016), 0.016, 0.003), M.bezel, true); bez.position.set(X(-0.45), 0.01, 0.003); s.add(bez);
    const pd = mesh(new BoxGeometry(0.1, 0.17, 0.026), M.red, true); pd.position.set(-0.45, 0.01, 0.03); pd.rotation.x = 0.22; s.add(pd);
    const lamps = [[0.55, 0x46e08a], [1.05, 0x46e08a], [1.55, 0xffc24a]];
    o.lamps = [];
    lamps.forEach(([x, col]) => {
      const m = mesh(G.ball, new MeshStandardMaterial({ color: col, emissive: col, emissiveIntensity: 0, roughness: 0.25 }));
      m.scale.set(0.034, 0.034, 0.02); m.position.set(x, 0.04, 0.01); s.add(m);
      const ring = mesh(new CylinderGeometry(0.05, 0.05, 0.012, 24).rotateX(Math.PI / 2), M.chrome); ring.position.set(x, 0.04, 0.004); s.add(ring);
      const sp = new Sprite(new SpriteMaterial({ map: T.glow, color: col, transparent: true, opacity: 0, blending: AdditiveBlending, depthWrite: false }));
      sp.scale.set(0.34, 0.34, 1); sp.position.set(x, 0.04, 0.04); s.add(sp);
      o.lamps.push({ m, sp });
    });
    const screws = new Group();
    [[28, 28], [28, 90], [972, 28], [972, 90]].forEach(([x, y]) => {
      const h = mesh(G.screw, M.screw, true); h.scale.setScalar(10.5 * S); h.position.set(LX(x), LY(y, hpx), 0); screws.add(h);
    });
    s.add(screws);
  })();

  /* ------------------------------------------------------------------ the LUNCHBOX on its stand */
  const LB = (function lunchbox() {
    const W = 3.2, H = 2.12, slot = 0.5, g = new Group();
    const x0 = CHEEK_IN + CHEEK_W + 0.55 + W / 2, y0 = FLOOR + 2.3 + H / 2, z0 = CZ - R + 0.85;
    g.position.set(x0, y0, z0);
    world.add(g);
    // the stand: a walnut table
    const top = mesh(new BoxGeometry(W + 0.5, 0.09, 1.35), M.walnut, true, true); top.position.set(0, -H / 2 - 0.045, -0.45); g.add(top);
    [[-1, -1], [1, -1], [-1, 1], [1, 1]].forEach(([sx, sz]) => {
      const leg = mesh(new BoxGeometry(0.1, 2.21, 0.1), M.walnut, true, true);
      leg.position.set(sx * (W / 2 + 0.1), -H / 2 - 0.09 - 1.105, -0.45 + sz * 0.55); g.add(leg);
    });
    const frame = mesh(new BoxGeometry(W, H, 0.95), M.frame, true, true); frame.position.z = -0.475 - 0.005; g.add(frame);
    // the modules' fronts, printed on one texture: CLASS-A EQ (two slots), DE-HARSH, CROSSFEED, OUTPUT, one empty slot
    const TW = 1600, k = TW / W, c = canvas(TW, Math.round(H * k)), x = c.getContext("2d");
    const P = (u) => u * k, cxs = (sx) => P(sx + W / 2), cys = (sy) => P(H / 2 - sy);
    x.fillStyle = "#0c0c0f"; x.fillRect(0, 0, c.width, c.height);
    const mods = [[-1.0, 2, "CLASS-A EQ", "#1c1f24"], [-0.25, 1, "DE-HARSH", "#141518"], [0.25, 1, "CROSSFEED", "#1a1c20"], [0.75, 1, "OUTPUT", "#0f0f12"]];
    x.textAlign = "center"; x.textBaseline = "middle";
    const spacing = (px) => { if ("letterSpacing" in x) x.letterSpacing = px + "px"; };
    mods.forEach(([mx, wslots, name, col]) => {
      const w = wslots * slot - 0.024, h = 1.92 - 0.024;
      x.fillStyle = col; x.fillRect(cxs(mx - w / 2), cys(h / 2), P(w), P(h));
      x.fillStyle = "#c9c9c4"; x.font = "700 " + P(0.052) + "px system-ui, sans-serif"; spacing(P(0.012));
      x.fillText(name, cxs(mx), cys(0.8));
      x.strokeStyle = "rgba(201,201,196,.45)"; x.lineWidth = 2; x.beginPath(); x.moveTo(cxs(mx - w / 2 + 0.05), cys(0.74)); x.lineTo(cxs(mx + w / 2 - 0.05), cys(0.74)); x.stroke();
      x.fillStyle = "#9c9c96";
      [[0.9], [-0.9]].forEach(([yy]) => { x.beginPath(); x.arc(cxs(mx), cys(yy), P(0.018), 0, 6.3); x.fill(); });
    });
    // empty slot: the rails and the connector at the back
    x.fillStyle = "#050506"; x.fillRect(cxs(1.25 - 0.24), cys(0.95), P(0.48), P(1.9));
    // scales round the knobs and labels
    const LBC = [
      ["t", -1.26, 0.62, "IN"], ["t", -1.0, 0.62, "HI Q"], ["t", -0.74, 0.62, "IRON"],
      ["k", -1.22, 0.26, "HIGH", 0.74, "blue"], ["k", -0.78, 0.26, "MID", 0.74, "red"],
      ["k", -1.22, -0.21, "MID kHz", 0.62, "marconi"], ["k", -0.78, -0.21, "LOW", 0.74, "blue"],
      ["k", -1.22, -0.68, "LOW Hz", 0.62, "marconi"], ["k", -0.78, -0.68, "HPF", 0.62, "marconi"],
      ["t", -0.25, 0.62, "IN"], ["k", -0.25, 0.23, "AMOUNT", 0.66, "red"], ["k", -0.25, -0.22, "FREQ", 0.62, "marconi"], ["k", -0.25, -0.62, "SPEED", 0.58, "white"],
      ["t", 0.25, 0.62, "IN"], ["k", 0.25, 0.1, "AMOUNT", 0.95, "green"]
    ];
    x.font = "700 " + P(0.03) + "px system-ui, sans-serif"; spacing(P(0.006));
    LBC.forEach(([kind, cx, cy, label, size]) => {
      x.fillStyle = "#a9a9a3";
      if (kind === "k") {
        const rr = 0.105 * size;
        x.strokeStyle = "#a9a9a3"; x.lineWidth = 2;
        for (let i = 0; i <= 10; i++) {
          const a = (-135 + 27 * i) * DEG, r0 = rr + 0.035, r1 = rr + (i % 5 ? 0.05 : 0.065);
          x.beginPath(); x.moveTo(cxs(cx + Math.sin(a) * r0), cys(cy + Math.cos(a) * r0)); x.lineTo(cxs(cx + Math.sin(a) * r1), cys(cy + Math.cos(a) * r1)); x.stroke();
        }
        x.fillText(label, cxs(cx), cys(cy - rr - 0.1));
      } else x.fillText(label, cxs(cx), cys(cy - 0.15));
    });
    x.fillStyle = "#b7b7b1"; x.font = "800 " + P(0.042) + "px system-ui, sans-serif"; spacing(P(0.012));
    x.fillText("LUNCHBOX", cxs(0.75), cys(-0.1));
    x.font = "600 " + P(0.022) + "px system-ui, sans-serif"; spacing(P(0.008));
    x.fillStyle = "#8d8d88"; x.fillText("6-SLOT FRAME", cxs(0.75), cys(-0.18)); x.fillText("CLASS A · DISCRETE", cxs(0.75), cys(-0.46));
    // the OUTPUT meter's face (behind glass): a cream VU scale
    const vx = 0.75, vy = 0.46, vw = 0.31, vh = 0.25;
    x.fillStyle = "#050505"; x.fillRect(cxs(vx - vw / 2 - 0.03), cys(vy + vh / 2 + 0.03), P(vw + 0.06), P(vh + 0.06));
    const vg = x.createLinearGradient(0, cys(vy + vh / 2), 0, cys(vy - vh / 2));
    vg.addColorStop(0, "#f3e7c6"); vg.addColorStop(1, "#dcc99a");
    x.fillStyle = vg; x.fillRect(cxs(vx - vw / 2), cys(vy + vh / 2), P(vw), P(vh));
    x.strokeStyle = "#2a2622"; x.lineWidth = 2; x.beginPath();
    x.arc(cxs(vx), cys(vy - 0.2), P(0.26), -Math.PI / 2 - 0.62, -Math.PI / 2 + 0.62); x.stroke();
    x.strokeStyle = "#b8352c"; x.lineWidth = 4; x.beginPath(); x.arc(cxs(vx), cys(vy - 0.2), P(0.26), -Math.PI / 2 + 0.3, -Math.PI / 2 + 0.62); x.stroke();
    x.fillStyle = "#2a2622"; x.font = "700 " + P(0.026) + "px system-ui, sans-serif"; spacing(P(0.004));
    x.fillText("OUT  dB", cxs(vx), cys(vy - 0.065));
    const tex = ctex(c, true, false);
    const face = mesh(new PlaneGeometry(W, H), new MeshPhysicalMaterial({ map: tex, metalness: 0.4, roughness: 0.5, clearcoat: 0.3, clearcoatRoughness: 0.3, emissiveMap: tex, emissive: 0x000000 }), false, true);
    face.position.z = 0.002; g.add(face);
    // the VU's glow and needle
    const vu = mesh(new PlaneGeometry(vw, vh), new MeshBasicMaterial({ color: 0xffb35a, transparent: true, opacity: 0.16, blending: AdditiveBlending, depthWrite: false }));
    vu.position.set(vx, vy, 0.006); g.add(vu);
    const needle = mesh(new BoxGeometry(0.005, 0.2, 0.004).translate(0, 0.1, 0), new MeshBasicMaterial({ color: 0x1c1a18 }));
    needle.position.set(vx, vy - 0.11, 0.012); needle.rotation.z = 0.35; g.add(needle);
    const glass = mesh(new PlaneGeometry(vw + 0.04, vh + 0.04), new MeshPhysicalMaterial({ color: 0xffffff, transparent: true, opacity: 0.08, roughness: 0.04, clearcoat: 1, depthWrite: false }));
    glass.position.set(vx, vy, 0.03); g.add(glass);
    const bz = mesh(ext(rrect(vw + 0.08, vh + 0.08, 0.02), 0.03, 0.006), M.bezel, true); bz.position.set(vx, vy, -0.01);
    bz.scale.set(1, 1, 1); g.add(bz);
    // module screws, knobs with coloured caps, bat toggles
    const heads = [], body = [], ptr = [], caps = {}, bats = [], nuts = [];
    mods.forEach(([mx]) => { [0.9, -0.9].forEach((yy) => heads.push([G.screw, m4(mx, yy, 0.004, 0.4, 0.024, 0.024, 0.024)])); });
    LBC.forEach(([kind, cx, cy, , size, style]) => {
      if (kind === "t") {
        nuts.push([G.nut, m4(cx, cy, 0.012, 0, 0.034, 0.034, 0.024)]);
        bats.push([G.bat, m4(cx, cy + 0.03, 0.07, 0, 0.03, 0.03, 0.12, -0.5)]);
        return;
      }
      const r = 0.105 * size, hgt = r * 1.15;
      body.push([G.knob, m4(cx, cy, 0.004, 0.5, r, r, hgt)]);
      ptr.push([G.pointer, m4(cx, cy, 0.004, 0.5, r, r, hgt)]);
      if (style !== "marconi") (caps[style] = caps[style] || []).push([new CircleGeometry(0.62, 32).translate(0, 0, 1.006), m4(cx, cy, 0.004, 0, r, r, hgt)]);
    });
    g.add(mesh(merge(heads), M.screw, true));
    g.add(mesh(merge(body), M.knob, true));
    g.add(mesh(merge(ptr), M.pointer, false));
    g.add(mesh(merge(nuts), M.chrome, true));
    g.add(mesh(merge(bats), M.chrome, true));
    Object.keys(caps).forEach((s) => g.add(mesh(merge(caps[s]), new MeshPhysicalMaterial({ color: capColour[s], roughness: 0.3, clearcoat: 0.8 }), false)));
    // the empty slot's rails
    [-0.22, 0.22].forEach((dx) => { const rl = mesh(new BoxGeometry(0.02, 1.86, 0.05), M.zinc, false, true); rl.position.set(1.25 + dx, 0, -0.08); g.add(rl); });
    const con = mesh(new BoxGeometry(0.16, 0.5, 0.08), M.gold, false, true); con.position.set(1.25, 0, -0.85); g.add(con);
    return { g, x0, y0, z0, W, H };
  })();

  /* ------------------------------------------------------------------ light */
  const pmrem = new PMREMGenerator(renderer);
  (function environment() {
    const s = new Scene();
    s.add(new Mesh(new BoxGeometry(44, 26, 44), new MeshBasicMaterial({ color: 0x08090c, side: BackSide })));
    const panel = (w, h, col, k, pos) => {
      const m = new Mesh(new PlaneGeometry(w, h), new MeshBasicMaterial({ color: new Color(col).multiplyScalar(k), side: DoubleSide }));
      m.position.set(pos[0], pos[1], pos[2]); m.lookAt(0, 1, 0); s.add(m);
    };
    panel(14, 7, 0xffffff, 3.4, [-7, 10, 11]);       // key softbox, high front left
    panel(9, 11, 0xffc49a, 1.2, [14, 1, 7]);         // warm fill, right
    panel(2.4, 18, 0x46d3e6, 1.1, [-15, 2, -8]);     // aurora strips behind: teal left,
    panel(2.4, 18, 0xb889ff, 0.9, [15, 3, -9]);      // violet right,
    panel(22, 2, 0xf59bd6, 0.5, [0, -11, 6]);        // a pink hint from below
    panel(34, 1.4, 0xe3fff3, 1.3, [0, 12, -4]);      // and a pearl strip across the top
    const rt = pmrem.fromScene(s, 0.035);
    scene.environment = rt.texture;
    s.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
    pmrem.dispose();
  })();
  scene.environmentIntensity = 0.95;
  const key = new DirectionalLight(0xffffff, 2.3);
  key.position.set(-7, 13, 15);
  key.target.position.set(0.8, 1.2, 1.2);
  key.castShadow = true;
  key.shadow.mapSize.set(Q.shadow, Q.shadow);
  Object.assign(key.shadow.camera, { left: -10, right: 10, top: 11, bottom: -10, near: 1, far: 50 });
  key.shadow.bias = -0.0004; key.shadow.normalBias = 0.025; key.shadow.radius = 3;
  const fill = new DirectionalLight(0xffc9a0, 0.5); fill.position.set(11, 2, 8);
  const rimL = new DirectionalLight(0x46d3e6, 1.0); rimL.position.set(-10, 5, -8);
  const rimR = new DirectionalLight(0xb889ff, 0.6); rimR.position.set(10, 8, -6);
  const hemi = new HemisphereLight(0x9fb7ff, 0x1a120c, 0.22);
  scene.add(key, key.target, fill, rimL, rimR, hemi);
  const LIGHTS = [[key, 2.3], [fill, 0.5], [rimL, 1.0], [rimR, 0.6], [hemi, 0.22]];

  /* ------------------------------------------------------------------ unit art -> textures */
  function loadImg(src) {
    return new Promise((res, rej) => {
      const im = new Image(); im.decoding = "async";
      im.onload = () => res(im); im.onerror = () => rej(new Error("image " + src));
      im.src = src;
    });
  }
  const imgCache = {};
  const img = (id, k) => imgCache[id + k] || (imgCache[id + k] = loadImg("img/units/" + id + "-" + k + ".svg"));
  async function artTexture(id, hpx, layers, width) {
    const c = canvas(width, Math.round(width * hpx / 1000)), g = c.getContext("2d");
    for (const k of layers) g.drawImage(await img(id, k), 0, 0, c.width, c.height);
    return ctex(c, true, false);
  }
  let rackReady = false;
  async function loadRack() {
    for (const u of RACK) {
      if (u.id === "power" || lost) continue;
      try {
        const t = await artTexture(u.id, u.h, ["d", "p"], TEX);
        const o = units[u.id];
        o.rackTex = t;
        if (!o.detail) { o.mat.map = t; o.mat.color.set(0xffffff); o.mat.needsUpdate = true; }
        dirty = true; kick();
      } catch (e) { /* keep the plain plate colour */ }
      await new Promise((r) => setTimeout(r, 0));   // one unit per task: no long frames
    }
    rackReady = true;
  }

  /* ------------------------------------------------------------------ the unit being taken apart */
  let act = null;
  const flowMats = [];
  function parseFlow(d) {
    const pts = []; let x = 0, y = 0;
    d.replace(/([MHVL])\s*(-?[\d.]+)(?:[\s,]+(-?[\d.]+))?/g, (m, c, a, b) => {
      if (c === "M" || c === "L") { x = +a; y = +b; } else if (c === "H") x = +a; else y = +a;
      pts.push([x, y]); return m;
    });
    return pts;
  }
  function flowGeo(list, hpx, wpx) {
    const pos = [], uv = [], idx = [];
    list.forEach((pts) => {
      let run = 0;
      for (let i = 1; i < pts.length; i++) {
        const [x0, y0] = pts[i - 1], [x1, y1] = pts[i], len = Math.hypot(x1 - x0, y1 - y0);
        if (len < 0.5) continue;
        const nx = -(y1 - y0) / len * wpx / 2, ny = (x1 - x0) / len * wpx / 2, b = pos.length / 3;
        [[x0 + nx, y0 + ny, run, 0], [x0 - nx, y0 - ny, run, 1], [x1 + nx, y1 + ny, run + len, 0], [x1 - nx, y1 - ny, run + len, 1]].forEach(([px, py, u, v]) => {
          pos.push(LX(px), LY(py, hpx), 0); uv.push(u / 42, v);
        });
        idx.push(b, b + 1, b + 2, b + 1, b + 3, b + 2);
        run += len;
      }
    });
    const g = new BufferGeometry();
    g.setAttribute("position", new Float32BufferAttribute(pos, 3));
    g.setAttribute("uv", new Float32BufferAttribute(uv, 2));
    g.setIndex(idx);
    return g;
  }
  function outline(w, h, mat) {
    return new LineSegments(new EdgesGeometry(new PlaneGeometry(w, h)), mat);
  }

  /** Swap a unit's rack-plate for its full set of layers (all loaded before anything is shown). */
  async function detail(id) {
    const o = units[id], u = o.u, hpx = u.h, hU = o.hU;
    const [tp, td, tb] = await Promise.all([artTexture(id, hpx, ["p"], Q.texHi), artTexture(id, hpx, ["d"], Q.texHi), artTexture(id, hpx, ["b"], Q.texHi)]);
    const D = { tex: [tp, td, tb], geo: [], mat: [], obj: [] };
    const own = (x) => { if (x.geometry) D.geo.push(x.geometry); if (x.material && !Array.isArray(x.material) && !Object.values(M).includes(x.material)) D.mat.push(x.material); return x; };
    // faceplate with its windows cut out
    const pm = o.mat.clone(); pm.map = tp; pm.alphaTest = 0.5; pm.color.set(0xffffff); D.mat.push(pm);
    // displays and meters, lit from behind
    const dm = new MeshPhysicalMaterial({ map: td, emissiveMap: td, emissive: 0xffffff, emissiveIntensity: 0.42, roughness: 0.3, clearcoat: 1, clearcoatRoughness: 0.05, alphaTest: 0.35 });
    const disp = own(mesh(new PlaneGeometry(5, hU), dm, false, false)); disp.position.z = -0.03; disp.userData.l = "d";
    // the DSP board: a PCB, its chips standing up from it (their tops carry the board's print), its connectors
    const P = PARTS[id], br = P.board || [58, 5, 884, hpx - 10];
    const bm = new MeshStandardMaterial({ map: tb, roughness: 0.55, metalness: 0.15, alphaTest: 0.4 });
    const board = new Group();
    const face = own(mesh(new PlaneGeometry(5, hU), bm, false, true)); face.userData.l = "b"; board.add(face);
    const slab = own(mesh(new BoxGeometry(br[2] * S, br[3] * S, 0.018), M.pcbEdge, true, false));
    slab.position.set(LX(br[0] + br[2] / 2), LY(br[1] + br[3] / 2, hpx), -0.0095); board.add(slab);
    const chipTop = new MeshStandardMaterial({ map: tb, roughness: 0.42, metalness: 0.1 }); D.mat.push(chipTop);
    P.chips.forEach(([x, y, w, h]) => {
      const g = new BoxGeometry(w * S, h * S, 0.04);
      const uv = g.attributes.uv;           // +z face (vertices 16..19): map to the chip's own patch of the board print
      const u0 = x / 1000, u1 = (x + w) / 1000, v0 = 1 - (y + h) / hpx, v1 = 1 - y / hpx;
      [[u0, v1], [u1, v1], [u0, v0], [u1, v0]].forEach(([a, b], i) => uv.setXY(16 + i, a, b));
      const ch = own(mesh(g, [M.chip, M.chip, M.chip, M.chip, chipTop, M.chip], true, true));
      ch.position.set(LX(x + w / 2), LY(y + h / 2, hpx), 0.02); ch.userData.l = "b"; board.add(ch);
    });
    P.ports.forEach(([x, y, w, h]) => {
      const p = own(mesh(new BoxGeometry(w * S, h * S, 0.05), M.gold, true, false));
      p.position.set(LX(x + w / 2), LY(y + h / 2, hpx), 0.025); board.add(p);
    });
    const fa = [], fc = [];
    (u.flows || []).forEach((f) => (f[1] === "a" ? fa : fc).push(parseFlow(f[0])));
    [[fa, 0x9ff7e6, 5, 1.0], [fc, 0xc7a6ff, 3.4, 0.62]].forEach(([list, col, w, speed]) => {
      if (!list.length) return;
      const map = T.dash.clone(); map.needsUpdate = true; D.tex.push(map);
      const mat = new MeshBasicMaterial({ color: col, map, transparent: true, opacity: 0, blending: AdditiveBlending, depthWrite: false });
      const fl = own(mesh(flowGeo(list, hpx, w), mat)); fl.position.z = 0.006; fl.renderOrder = 2; board.add(fl);
      flowMats.push({ mat, speed });
    });
    board.position.z = -0.72;
    // chassis: an open steel box (the front is the faceplate)
    const k = new Group(), hh = hU - 0.03, t = 0.02, zc = -0.03 - CH_D / 2;
    const wall = (w, h, d, x, y, z) => { const m = own(mesh(new BoxGeometry(w, h, d), M.chassis, true, true)); m.position.set(x, y, z); m.userData.l = "k"; k.add(m); return m; };
    wall(CH_W, t, CH_D, 0, -hh / 2 + t / 2, zc);
    wall(CH_W, t, CH_D, 0, hh / 2 - t / 2, zc);
    wall(t, hh, CH_D, -CH_W / 2 + t / 2, 0, zc);
    wall(t, hh, CH_D, CH_W / 2 - t / 2, 0, zc);
    const back = wall(CH_W, hh, t, 0, 0, -0.03 - CH_D + t / 2);
    const rimK = own(outline(CH_W, hh, M.rim2)); rimK.position.z = 0; k.add(rimK);
    const rimP = own(outline(5, hU, M.rim)); rimP.position.z = 0.004;
    return { D, pm, disp, board, k, back, rimP, rimK };
  }

  let building = 0;
  async function build(id) {
    const token = ++building;
    let d;
    try { d = await detail(id); } catch (e) { return; }
    if (token !== building || lost) { d.D.tex.forEach((t) => t.dispose()); d.D.geo.forEach((g) => g.dispose()); d.D.mat.forEach((m) => m.dispose()); return; }
    if (act) undo(act);
    const o = units[id];
    o.plate.material = d.pm; o.plate.userData.l = "p";
    o.L.d.add(d.disp); o.L.b.add(d.board); o.L.k.add(d.k); o.L.p.add(d.rimP);
    o.box.visible = false;
    o.L.c.children.forEach((m) => { m.userData.l = "c"; m.traverse((x) => { x.userData.l = "c"; }); });
    o.detail = d;
    act = { id, o, u: o.u, d, lit: null, hot: null };
    buildHot();
    summary();
    pickBtns.forEach((b) => b.setAttribute("aria-pressed", String(b.getAttribute("data-unit") === id)));
    dirty = true; shadowDirty = true; kick();
  }
  function undo(a) {
    const o = a.o, d = a.d;
    o.plate.material = o.mat;
    [d.disp, d.board, d.k, d.rimP].forEach((x) => x.parent && x.parent.remove(x));
    d.D.tex.forEach((t) => t.dispose()); d.D.geo.forEach((g) => g.dispose()); d.D.mat.forEach((m) => m.dispose());
    flowMats.length = 0;
    o.box.visible = true; o.detail = null;
    ["c", "p", "d", "b", "k"].forEach((k) => { o.L[k].position.z = 0; });
    o.slide.position.z = 0; o.slide.rotation.x = 0;
    hotWrap.textContent = ""; hotBtns = []; tags = {};
  }

  /* ------------------------------------------------------------------ page parts: picker, info, overlays */
  const track = section.querySelector(".xp-track");
  const steps = [].slice.call(section.querySelectorAll(".xp-steps li"));
  const pickBtns = [].slice.call(section.querySelectorAll(".xp-u"));
  const tools = section.querySelector(".xp-tools");
  const info = { k: section.querySelector(".xp-info-k"), t: section.querySelector(".xp-info-t"), d: section.querySelector(".xp-info-d") };
  const hotWrap = el("div", "xp-hot");
  const call = el("div", "xp-call");
  call.setAttribute("aria-hidden", "true");
  let hotBtns = [], tags = {}, shown = null, shownHs = null;

  function summary() {
    if (!act) return;
    const u = act.u;
    info.k.textContent = pad(u.pos) + " · " + titleCase(u.role);
    info.t.textContent = u.name;
    info.d.textContent = u.blurb + " Front to back: the controls, the faceplate, the displays and meters, the DSP board and the chassis." +
      (act.hot ? " Point at a part, or tab to it, to see what it does." : "");
    if (shown) shown.classList.remove("on");
    shown = null; shownHs = null;
    call.classList.remove("on");
  }
  function show(hs, b) {
    if (shown === b && b) return;
    if (shown) shown.classList.remove("on");
    shown = b; shownHs = hs;
    if (b) b.classList.add("on");
    info.k.textContent = LNAME[hs.l] + " · " + act.u.name;
    info.t.textContent = hs.t;
    info.d.textContent = hs.d;
    call.textContent = hs.t;
    call.classList.add("on");
    dirty = true; kick();
  }
  function buildHot() {
    hotWrap.textContent = ""; hotBtns = []; tags = {};
    const u = act.u, h = u.h;
    const list = u.hot.slice();
    list.push({ l: "k", x: 110, y: h / 2, t: "Chassis", r: 30, d: "The unit's steel body, " + (u.h > 200 ? "a tall box" : "a slim box") + " that slides into the walnut case on its rails; its ears are screwed to the front rails." });
    list.forEach((hs) => {
      const b = el("button", "hs", hotWrap);
      b.type = "button";
      b.setAttribute("aria-label", hs.t + " (" + LNAME[hs.l] + ")");
      const on = () => show(hs, b);
      b.addEventListener("pointerenter", on);
      b.addEventListener("focus", on);
      b.addEventListener("click", on);
      hotBtns.push({ b, hs, r: Math.max(24, hs.r || 30) });
    });
    ["k", "b", "d", "p", "c"].forEach((key) => {
      const t = el("span", "xtag" + (key === "c" || key === "p" ? "" : " dark"), hotWrap);
      t.textContent = LNAME[key] === "Displays & meters" ? "Displays & meters" : LNAME[key];
      tags[key] = t;
    });
    hotWrap.appendChild(call);
    setHot(false);
  }
  function setHot(on) {
    if (!act) return;
    act.hot = on;
    if (on) hotWrap.removeAttribute("inert"); else hotWrap.setAttribute("inert", "");
    xpStage.classList.toggle("hot", on);
    if (!on && shown) summary();
    else if (!shown) summary();
  }

  /* ------------------------------------------------------------------ where things are on the page */
  let W = 0, H = 0, dpr = 1, phone = false;
  const stageEl = motion ? layer : xpStage;
  function measure() {
    const r = stageEl.getBoundingClientRect();
    W = Math.max(1, Math.round(r.width)); H = Math.max(1, Math.round(r.height));
    phone = innerWidth < 900;
    dpr = Math.min(devicePixelRatio || 1, Q.dpr) * LEVEL_DPR[level];
    renderer.setPixelRatio(dpr);
    renderer.setSize(W, H, false);
    dirty = true;
  }
  function relRect(e, base) {
    const a = e.getBoundingClientRect();
    return { x: a.left - base.left, y: a.top - base.top, w: Math.max(1, a.width), h: Math.max(1, a.height) };
  }

  /* ------------------------------------------------------------------ camera: fit a box into a rectangle of the canvas */
  const fitCam = new PerspectiveCamera(30, 1, 0.1, 200);
  const corners = Array.from({ length: 8 }, () => new Vector3());
  function boxCorners(min, max, mat) {
    for (let i = 0; i < 8; i++) {
      corners[i].set(i & 1 ? max.x : min.x, i & 2 ? max.y : min.y, i & 4 ? max.z : min.z);
      if (mat) corners[i].applyMatrix4(mat);
    }
    return corners;
  }
  const tv = new Vector3();
  /** Distance at which the points fit the view (vertical fov, aspect), with a margin. */
  function fitDist(pts, target, yaw, pitch, fov, aspect, margin) {
    let d = 20;
    fitCam.fov = fov; fitCam.aspect = aspect; fitCam.updateProjectionMatrix();
    for (let it = 0; it < 4; it++) {
      place(fitCam, target, d, yaw, pitch);
      fitCam.updateMatrixWorld();
      let m = 0;
      for (const p of pts) { tv.copy(p).project(fitCam); m = Math.max(m, Math.abs(tv.x), Math.abs(tv.y)); }
      d *= Math.max(0.3, m / margin);
    }
    return d;
  }
  function place(cam, t, d, yaw, pitch) {
    const cp = Math.cos(pitch);
    cam.position.set(t.x + d * Math.sin(yaw) * cp, t.y + d * Math.sin(pitch), t.z + d * Math.cos(yaw) * cp);
    cam.lookAt(t);
  }
  /** Put the projection centre of the camera on the middle of rect (canvas pixels); rect.h spans fov. */
  function frameRect(rect, fov) {
    const cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
    const fw = 2 * Math.max(cx, W - cx, 1), fh = 2 * Math.max(cy, H - cy, 1);
    camera.fov = 2 * Math.atan(Math.tan(fov * DEG / 2) * fh / rect.h) / DEG;
    camera.aspect = fw / fh;
    camera.setViewOffset(fw, fh, fw / 2 - cx, fh / 2 - cy, W, H);
    camera.updateProjectionMatrix();
  }

  world.updateMatrixWorld(true);
  const caseBox = new Box3().setFromObject(caseG), lbBox = new Box3().setFromObject(LB.g);
  const rackBox = caseBox.clone().union(lbBox);
  const rackPts = boxCorners(rackBox.min, rackBox.max).map((v) => v.clone());
  const heroPts = boxCorners(caseBox.min, V3(caseBox.max.x + 1.2, caseBox.max.y, caseBox.max.z)).map((v) => v.clone());
  const rackT = rackBox.getCenter(V3(0, 0, 0));
  const heroT = V3(0.5, (caseBox.min.y + caseBox.max.y) / 2, (caseBox.min.z + caseBox.max.z) / 2);
  const introT = V3(0, CY + 2.6, 1.4);

  /* ------------------------------------------------------------------ state */
  const FOV = 28;
  const cur = { h: 0, p: motion ? 0 : 1, k: 1, yaw: 0, pitch: 0, px: 0, py: 0, intro: motion ? 0 : 1 };
  const tgt = { h: 0, p: cur.p, k: 1, yaw: 0, pitch: 0, px: 0, py: 0, intro: 1 };
  let hold = null, holdP = 0, pending = null, visible = !motion, raf = 0, last = 0, dirty = true, shadowDirty = true;
  let t0 = 0, revealed = false, tFlow = 0, lastRender = 0, lastMove = 0;
  const frameTimes = [];

  function scrollP() {
    if (!motion) return 1;
    const r = track.getBoundingClientRect(), span = track.offsetHeight - innerHeight;
    return span > 0 ? clamp(-r.top / span, 0, 1) : 1;
  }
  function heroP() {
    if (!motion) return 1;
    const r = heroSec.getBoundingClientRect();
    return clamp(-r.top / Math.max(1, r.height), 0, 1);
  }
  function targets() {
    const sp = scrollP();
    if (hold !== null && motion && Math.abs(sp - holdP) > 0.12) { hold = null; syncToggle(); }
    tgt.p = hold === 1 ? 1 : hold === 0 ? 0.04 : sp;
    tgt.h = heroP();
    tgt.k = pending ? 0 : 1;
  }

  /* ------------------------------------------------------------------ apply the state to the scene */
  const tq = new Matrix4();
  function apply(dt, now) {
    const p = cur.p, k = cur.k;
    const out = act ? sm(0.3, 0.52, p) * k : 0;
    const e = act ? sm(0.5, 0.78, p) * k : 0;
    // a wave of drawers, bottom to top, before one comes out
    const t = (p - 0.06) / 0.3;
    let moved = false;
    RACK.forEach((u, i) => {
      const o = units[u.id];
      const c = 0.06 + 0.88 * i / (RACK.length - 1);
      const bump = motion && cur.h > 0.98 ? clamp(1 - Math.abs(t - c) / 0.2, 0, 1) : 0;
      let z = 0.32 * bump * bump * (3 - 2 * bump), tilt = 0;
      if (act && o === act.o) { z = z * (1 - out) + out * ZO; tilt = -o.a * out; }
      if (Math.abs(z - o.z) > 1e-4 || Math.abs(tilt - o.tilt) > 1e-5) {
        o.slide.position.z = z; o.slide.rotation.x = tilt; o.z = z; o.tilt = tilt; moved = true;
      }
    });
    if (act) {
      const o = act.o, m = phone ? 1.15 : 1;
      const Z = { c: e * 2.5 * m, p: e * 1.55 * m, d: e * 0.55 * m, b: -e * 0.35 * m, k: -e * 2.3 * m };
      for (const key in Z) if (Math.abs(o.L[key].position.z - Z[key]) > 1e-5) { o.L[key].position.z = Z[key]; moved = true; }
      o.L.b.visible = e > 0.004;
      const lit = e > 0.62;
      M.rim.opacity = M.rim2.opacity = sm(0.55, 0.8, e) * 0.75;
      if (lit !== act.lit) { act.lit = lit; for (const tk in tags) tags[tk].classList.toggle("on", lit); }
      const flowing = e > 0.95;
      flowMats.forEach((f) => { f.mat.opacity = sm(0.9, 1, e) * 0.95; });
      if (flowing && visible && motion) { tFlow += dt; flowMats.forEach((f) => { f.mat.map.offset.x = -tFlow * f.speed; }); }
      const hot = e > 0.85;
      if (hot !== act.hot) setHot(hot);
    }
    if (moved) shadowDirty = true;
    // the rest of the rack sinks into the dark while a unit is out: a veil between the case and the unit
    if (act) {
      veil.visible = out > 0.002;
      if (veil.parent !== act.o.slide) act.o.slide.add(veil);
      veil.position.z = -(0.03 + CH_D + e * 2.3 * (phone ? 1.15 : 1) + 0.3);
      veil.material.opacity = 0.8 * sm(0.05, 0.9, out);
    }
    // the POWER lamps come on during the intro
    const lampOn = motion ? sm(0.28, 0.4, cur.intro) : 1;
    units.power.lamps.forEach((l, i) => { l.m.material.emissiveIntensity = 2.2 * lampOn; l.sp.material.opacity = 0.55 * lampOn; });
    const lightK = motion ? mix(0.12, 1, sm(0, 0.55, cur.intro)) : 1;
    LIGHTS.forEach(([l, v]) => { l.intensity = v * lightK; });
    scene.environmentIntensity = 0.95 * mix(0.25, 1, lightK);
    // steps
    let stp = p < 0.2 ? 0 : p < 0.5 ? 1 : p < 0.8 ? 2 : 3;
    if (hold === 1 || !motion) stp = hold === 0 ? 0 : 3;
    steps.forEach((li, j) => li.classList.toggle("on", j <= stp));
    return { out, e };
  }

  const vA = { t: V3(0, 0, 0), d: 0, yaw: 0, pitch: 0 }, vB = { t: V3(0, 0, 0), d: 0, yaw: 0, pitch: 0 };
  const scratchMin = V3(0, 0, 0), scratchMax = V3(0, 0, 0), ctr = V3(0, 0, 0);
  function lerpView(a, b, t) { a.t.lerp(b.t, t); a.d = mix(a.d, b.d, t); a.yaw = mix(a.yaw, b.yaw, t); a.pitch = mix(a.pitch, b.pitch, t); return a; }
  function setView(v, t, d, yaw, pitch) { v.t.copy(t); v.d = d; v.yaw = yaw; v.pitch = pitch; return v; }
  function unitView(v, e, rect, yaw, pitch, margin) {
    const o = act.o, hh = o.hU / 2, m = phone ? 1.15 : 1;
    scratchMin.set(-2.5, -hh - (e > 0.5 ? 0.16 : 0), -0.03 - CH_D - e * 2.3 * m);
    scratchMax.set(2.5, hh + (e > 0.5 ? 0.16 : 0), 0.16 + e * 2.5 * m);
    o.slide.updateMatrixWorld(true);
    const pts = boxCorners(scratchMin, scratchMax, o.slide.matrixWorld);
    ctr.set(0, 0, 0); pts.forEach((q) => ctr.add(q)); ctr.multiplyScalar(1 / 8);
    const d = fitDist(pts, ctr, yaw, pitch, FOV, rect.w / rect.h, margin);
    return setView(v, ctr, d, yaw, pitch);
  }

  const layerBase = { left: 0, top: 0 };
  let rectHero = null, rectXp = null;
  function camera3d(state) {
    const lb = stageEl.getBoundingClientRect();
    layerBase.left = lb.left; layerBase.top = lb.top;
    let rect;
    if (motion) {
      const hr = heroSec.getBoundingClientRect();
      const a = relRect(heroStage, lb); a.y -= hr.top - lb.top;       // the hero's stage where it sits at the top of the page
      const pin = section.querySelector(".xp-pin").getBoundingClientRect();
      const b = relRect(xpStage, lb); b.y -= pin.top - lb.top;        // the inside section's stage, pinned
      const s = easeIO(cur.h);
      rect = { x: mix(a.x, b.x, s), y: mix(a.y, b.y, s), w: mix(a.w, b.w, s), h: mix(a.h, b.h, s) };
      rectHero = a; rectXp = b;
    } else rect = { x: 0, y: 0, w: W, h: H };
    frameRect(rect, FOV);
    const ar = rect.w / rect.h;
    // the rack, whole
    const hv = setView(vA, heroT, fitDist(heroPts, heroT, -24 * DEG, 5 * DEG, FOV, ar, phone ? 0.94 : 0.97), -24 * DEG, 5 * DEG);
    const rv = setView(vB, rackT, fitDist(rackPts, rackT, -30 * DEG, 7 * DEG, FOV, ar, 0.95), -30 * DEG, 7 * DEG);
    let v = motion ? lerpView(hv, rv, easeIO(cur.h)) : lerpView(hv, rv, 1);
    if (motion && cur.intro < 1) {                       // the opening shot: low, close, from the side
      const it = easeIO(cur.intro);
      const iv = { t: introT.clone(), d: v.d * 0.42, yaw: -64 * DEG, pitch: -9 * DEG };
      v = lerpView(iv, v, it);
    }
    if (act && state.out > 0.001) {
      const w = { t: V3(0, 0, 0), d: 0, yaw: 0, pitch: 0 };
      const yo = -12 * DEG, po = 4 * DEG;
      const yx = phone ? -10 * DEG : -40 * DEG, pxp = phone ? 34 * DEG : 12 * DEG;
      unitView(w, state.e, rect, mix(yo, yx, state.e), mix(po, pxp, state.e), phone ? 0.96 : 0.98);
      v = lerpView({ t: v.t.clone(), d: v.d, yaw: v.yaw, pitch: v.pitch }, w, state.out);
    }
    const yaw = v.yaw + cur.yaw * DEG + cur.px * 3 * DEG, pitch = clamp(v.pitch + cur.pitch * DEG + cur.py * 1.6 * DEG, -40 * DEG, 60 * DEG);
    place(camera, v.t, v.d, yaw, pitch);
    camera.updateMatrixWorld();
    return rect;
  }

  /* ------------------------------------------------------------------ overlays that follow the 3D (hotspots, layer names) */
  const pv = new Vector3(), pv2 = new Vector3();
  function project(obj, x, y, z) {
    pv.set(x, y, z); obj.localToWorld(pv); pv.project(camera);
    return { x: (pv.x + 1) / 2 * W, y: (1 - pv.y) / 2 * H, z: pv.z };
  }
  function overlays(state) {
    if (!act || !hotBtns.length) return;
    const on = act.hot;
    const sr = relRect(xpStage, layerBase);   // the overlay lives in the stage; canvas pixels -> stage pixels
    const o = act.o, hpx = act.u.h;
    const zOf = { c: 0.1, p: 0.004, d: 0.004, b: 0.042, k: 0 };
    if (on || shown) {
      hotBtns.forEach((hb) => {
        const hs = hb.hs, L = o.L[hs.l];
        const X = LX(hs.x), Y = LY(hs.y, hpx);
        const z = hs.l === "k" ? -0.03 - CH_D + 0.03 : zOf[hs.l];
        const a = project(L, X, Y, z), b = project(L, X + hb.r * S, Y, z);
        const rr = clamp(Math.hypot(b.x - a.x, b.y - a.y), 14, 60);
        hb.x = a.x - sr.x; hb.y = a.y - sr.y;
        const st = hb.b.style;
        st.transform = "translate3d(" + hb.x.toFixed(1) + "px," + hb.y.toFixed(1) + "px,0) translate(-50%,-50%)";
        st.width = st.height = (2 * rr).toFixed(0) + "px";
      });
    }
    if (act.lit) {
      const anchors = { k: [o.L.k, -2.22, o.hU / 2 + 0.02, -0.03 - CH_D], b: [o.L.b, -0.4, o.hU / 2 + 0.02, -0.72], d: [o.L.d, 0.9, o.hU / 2 + 0.02, -0.03], p: [o.L.p, -2.5, o.hU / 2 + 0.02, 0], c: [o.L.c, -2.5, -o.hU / 2 - 0.02, 0.05] };
      for (const key in anchors) {
        const [L, x, y, z] = anchors[key], a = project(L, x, y, z);
        const tw = tags[key].offsetWidth || 90, tx = clamp(a.x - sr.x, 4, Math.max(4, sr.w - tw - 4));
        tags[key].style.transform = "translate3d(" + tx.toFixed(1) + "px," + (a.y - sr.y).toFixed(1) + "px,0) translate(0," + (key === "c" ? "20%" : "-120%") + ")";
      }
    }
    if (shown) {
      const hb = hotBtns.find((x) => x.b === shown);
      if (hb) call.style.transform = "translate3d(" + hb.x.toFixed(1) + "px," + hb.y.toFixed(1) + "px,0)";
    }
  }

  const veil = new Mesh(new PlaneGeometry(90, 90), new MeshBasicMaterial({ color: 0x050608, transparent: true, opacity: 0, depthWrite: false, fog: false }));
  veil.visible = false;

  /* ------------------------------------------------------------------ the loop */
  function frame(ts) {
    raf = 0;
    if (lost) return;
    const dt = last ? Math.min(0.1, (ts - last) / 1000) : 0.016;
    const gap = last ? ts - last : 0;
    last = ts;
    targets();
    const a1 = motion ? 1 - Math.exp(-dt / 0.12) : 1, a2 = motion ? 1 - Math.exp(-dt / (pending ? 0.07 : 0.16)) : 1, a3 = 1 - Math.exp(-dt / 0.1), a4 = 1 - Math.exp(-dt / 0.35);
    let d = 0;
    if (motion && revealed && cur.intro < 1) { cur.intro = clamp((ts - t0) / 4200, 0, 1); d = 1; }
    ["p", "h", "k", "yaw", "pitch", "px", "py"].forEach((key) => {
      const al = key === "p" || key === "h" ? a1 : key === "k" ? a2 : key === "px" || key === "py" ? a4 : a3;
      let v = cur[key] + (tgt[key] - cur[key]) * al;
      if (Math.abs(tgt[key] - v) < 1e-4) v = tgt[key];
      d = Math.max(d, Math.abs(v - cur[key]) * (key === "yaw" || key === "pitch" ? 0.02 : 1));
      cur[key] = v;
    });
    if (pending && cur.k < 0.03) { const id = pending; pending = null; build(id); }
    const state = apply(dt, ts);
    const moving = d > 2e-5;
    if (moving || dirty) lastMove = ts;
    const flowing = motion && act && state.e > 0.95 && flowMats.length > 0 && ts - lastMove < 12000;   // then rest: no endless redraws
    if (visible && (moving || dirty || flowing) && revealed) {
      if (!flowing || moving || dirty || ts - lastRender > 30) {       // flow alone: about 30 frames a second
        camera3d(state);
        if (shadowDirty) { renderer.shadowMap.needsUpdate = true; shadowDirty = false; }
        renderer.render(scene, camera);
        overlays(state);
        if (gap && gap < 250 && lastRender && ts - lastRender < 60) adapt(gap);
        lastRender = ts;
        dirty = false;
      }
    }
    if (visible && (moving || flowing || dirty)) raf = requestAnimationFrame(frame); else last = 0;
  }
  function kick() { if (!raf && !lost && (visible || !motion)) raf = requestAnimationFrame(frame); }

  /** Frames running slow (an older laptop's graphics): fewer pixels, then no shadows. */
  function adapt(ms) {
    if (performance.now() - t0 < 2500) return;         // textures still uploading
    frameTimes.push(ms);
    if (frameTimes.length < 40) return;
    const avg = frameTimes.reduce((a, b) => a + b, 0) / frameTimes.length;
    frameTimes.length = 0;
    if (avg > 30 && level < LEVEL_DPR.length - 1) {
      level++;
      if (level === LEVEL_DPR.length - 1) { renderer.shadowMap.enabled = false; scene.traverse((x) => { if (x.material && !Array.isArray(x.material)) x.material.needsUpdate = true; }); }
      measure();
      root.setAttribute("data-gl-level", String(level));
    }
  }

  /* ------------------------------------------------------------------ interaction */
  function select(id, go) {
    if (!byId[id]) return;
    pickBtns.forEach((b) => b.setAttribute("aria-pressed", String(b.getAttribute("data-unit") === id)));
    if (go) reveal(0.9);
    if (act && act.id === id) return;
    if (motion && act && cur.k > 0.05 && sm(0.3, 0.52, cur.p) > 0.05) { pending = id; kick(); }
    else build(id);
  }
  function reveal(p) {
    if (!motion) { section.scrollIntoView({ block: "start" }); return; }
    const top = track.getBoundingClientRect().top + scrollY, span = track.offsetHeight - innerHeight;
    scrollTo({ top: top + span * p, behavior: "smooth" });
  }
  pickBtns.forEach((b) => b.addEventListener("click", () => select(b.getAttribute("data-unit"))));
  const toggleBtn = tools.querySelector('[data-xp="toggle"]');
  function exploded() { return hold !== null ? hold === 1 : motion ? scrollP() > 0.6 : true; }
  function syncToggle() {
    const x = exploded();
    toggleBtn.textContent = x ? "Put it back" : "Take it apart";
    toggleBtn.setAttribute("aria-pressed", String(x));
  }
  tools.hidden = false;
  tools.addEventListener("click", (e) => {
    const b = e.target.closest("[data-xp]"); if (!b) return;
    const a = b.getAttribute("data-xp");
    if (a === "toggle") {
      hold = exploded() ? 0 : 1; holdP = scrollP(); syncToggle();
      if (!motion) { cur.p = tgt.p = hold === 1 ? 1 : 0.04; dirty = true; }
    }
    else if (a === "left") nudge(-15, 0);
    else if (a === "right") nudge(15, 0);
    else if (a === "reset") { tgt.yaw = tgt.pitch = 0; }
    kick();
  });
  function nudge(dy, dp) { tgt.yaw = clamp(tgt.yaw + dy, -55, 55); tgt.pitch = clamp(tgt.pitch + dp, -24, 24); if (!motion) { cur.yaw = tgt.yaw; cur.pitch = tgt.pitch; } dirty = true; kick(); }

  xpStage.tabIndex = 0;
  xpStage.classList.add("grab");
  heroStage.classList.add("grab");
  xpStage.addEventListener("keydown", (e) => {
    if (e.target !== xpStage) return;
    const k = e.key; let done = true;
    if (k === "ArrowLeft") nudge(-6, 0); else if (k === "ArrowRight") nudge(6, 0);
    else if (k === "ArrowUp") nudge(0, 5); else if (k === "ArrowDown") nudge(0, -5);
    else if (k === "Home") { tgt.yaw = tgt.pitch = 0; if (!motion) cur.yaw = cur.pitch = 0; dirty = true; kick(); }
    else done = false;
    if (done) e.preventDefault();
  });

  // pointing at the 3D: which unit (in the rack) or which part (taken apart)
  const ray = new Raycaster(), ndc = new Vector2();
  function pick(clientX, clientY) {
    if (!revealed) return null;
    ndc.set((clientX - layerBase.left) / W * 2 - 1, -((clientY - layerBase.top) / H) * 2 + 1);
    ray.setFromCamera(ndc, camera);
    const out = act ? sm(0.3, 0.52, cur.p) * cur.k : 0;
    if (act && out > 0.5) {
      const o = act.o, list = [];
      o.slide.traverse((x) => { if (x.isMesh && x.userData.l && x.visible) list.push(x); });
      const hit = ray.intersectObjects(list, false).find((h) => !(h.object.material && h.object.material.alphaTest && h.uv && alphaAt(h)));
      if (!hit) return null;
      const l = hit.object.userData.l, Lg = o.L[l];
      pv2.copy(hit.point); Lg.worldToLocal(pv2);
      const x = pv2.x / S + 500, y = act.u.h / 2 - pv2.y / S;
      let best = null, bd = 1e9;
      hotBtns.forEach((hb) => {
        if (hb.hs.l !== l) return;
        const dd = Math.hypot(hb.hs.x - x, hb.hs.y - y);
        if (dd < Math.max(hb.r * 1.6, 46) && dd < bd) { bd = dd; best = hb; }
      });
      return best ? { part: best } : { unitOut: true };
    }
    const list = RACK.map((u) => units[u.id].plate);
    const hit = ray.intersectObjects(list, false)[0];
    return hit ? { unit: hit.object.userData.unit } : null;
  }
  function alphaAt() { return false; }
  const tip = el("div", "gl-tip", layer);
  let tipOn = null;
  function hoverAt(e, stage) {
    const r = pick(e.clientX, e.clientY);
    let cursor = "";
    if (r && r.part && act.hot) { show(r.part.hs, r.part.b); cursor = "pointer"; }
    const uid = r && r.unit && r.unit !== "power" ? r.unit : null;
    if (uid) cursor = "pointer";
    stage.classList.toggle("point", !!cursor);
    if (motion && uid !== tipOn) {
      tipOn = uid;
      if (uid) { tip.textContent = byId[uid].name; tip.classList.add("on"); } else tip.classList.remove("on");
    }
    if (uid) tip.style.transform = "translate3d(" + (e.clientX - layerBase.left + 14).toFixed(0) + "px," + (e.clientY - layerBase.top + 16).toFixed(0) + "px,0)";
  }
  function wire(stage, isHero) {
    let drag = null, hoverRaf = 0, lastEv = null;
    stage.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || e.target.closest(".hs")) return;
      drag = { x: e.clientX, y: e.clientY, yaw: tgt.yaw, pitch: tgt.pitch, id: e.pointerId, on: false, touch: e.pointerType === "touch" };
    });
    stage.addEventListener("pointermove", (e) => {
      if (isHero && e.pointerType === "mouse" && motion) {
        const r = stage.getBoundingClientRect();
        tgt.px = clamp((e.clientX - r.left) / r.width * 2 - 1, -1, 1); tgt.py = clamp((e.clientY - r.top) / r.height * 2 - 1, -1, 1);
        kick();
      }
      if (drag && e.pointerId === drag.id) {
        const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
        if (!drag.on) {
          if (Math.abs(dx) < 5 && Math.abs(dy) < 5) return;
          if (drag.touch && Math.abs(dy) > Math.abs(dx)) { drag = null; return; }   // a vertical swipe scrolls the page
          drag.on = true; stage.classList.add("grabbing"); tip.classList.remove("on"); tipOn = null;
          try { stage.setPointerCapture(e.pointerId); } catch (err) { /* older browsers */ }
        }
        tgt.yaw = clamp(drag.yaw + dx * 0.22, -55, 55);
        if (!drag.touch) tgt.pitch = clamp(drag.pitch - dy * 0.15, -24, 24);
        if (!motion) { cur.yaw = tgt.yaw; cur.pitch = tgt.pitch; dirty = true; }
        kick();
        return;
      }
      if (e.pointerType !== "touch") {
        lastEv = e;
        if (!hoverRaf) hoverRaf = requestAnimationFrame(() => { hoverRaf = 0; if (lastEv) hoverAt(lastEv, stage); });
      }
    });
    function end(e) {
      if (!drag) return;
      const was = drag.on; drag = null; stage.classList.remove("grabbing");
      if (!was && e && e.type === "pointerup") {
        const r = pick(e.clientX, e.clientY);
        if (r && r.part) show(r.part.hs, r.part.b);
        else if (r && r.unit && r.unit !== "power") select(r.unit, isHero);
      }
    }
    stage.addEventListener("pointerup", end);
    stage.addEventListener("pointercancel", end);
    stage.addEventListener("lostpointercapture", () => { if (drag && drag.on) end(); });
    stage.addEventListener("pointerleave", (e) => {
      if (isHero) { tgt.px = tgt.py = 0; kick(); }
      tip.classList.remove("on"); tipOn = null; lastEv = null;
      stage.classList.remove("point");
      if (!isHero && shown && !xpStage.contains(doc.activeElement)) summary();
    });
  }
  wire(xpStage, false);
  wire(heroStage, true);

  /* ------------------------------------------------------------------ put it on the page */
  if (motion) layer.appendChild(cv); else xpStage.insertBefore(cv, xpStage.firstChild);
  xpStage.appendChild(hotWrap);
  heroStage.setAttribute("role", "img");
  heroStage.setAttribute("aria-label", "A 3D model of the ENH Master rack: eleven units and the POWER strip in a curved walnut case, the LUNCHBOX on its stand beside it. Drag to turn it.");
  let poster = null;
  function makePoster() {                 // still mode: the hero gets one rendered frame of the rack
    const r = heroStage.getBoundingClientRect();
    if (r.width < 2 || r.height < 2) return;
    const w = Math.round(r.width), h = Math.round(r.height), pr = Math.min(devicePixelRatio || 1, 2);
    renderer.setPixelRatio(pr); renderer.setSize(w, h, false);
    W = w; H = h;
    const saved = act;
    const so = saved ? { z: saved.o.slide.position.z, t: saved.o.slide.rotation.x, L: ["c", "p", "d", "b", "k"].map((k) => saved.o.L[k].position.z) } : null;
    if (saved) { saved.o.slide.position.z = 0; saved.o.slide.rotation.x = 0; ["c", "p", "d", "b", "k"].forEach((k) => { saved.o.L[k].position.z = 0; }); saved.o.L.b.visible = false; }
    frameRect({ x: 0, y: 0, w, h }, FOV);
    const hv = setView(vA, heroT, fitDist(heroPts, heroT, -24 * DEG, 5 * DEG, FOV, w / h, 0.95), -24 * DEG, 5 * DEG);
    place(camera, hv.t, hv.d, hv.yaw, hv.pitch); camera.updateMatrixWorld();
    veil.visible = false;
    renderer.shadowMap.needsUpdate = true;
    renderer.render(scene, camera);
    if (!poster) { poster = el("canvas", "gl-poster"); poster.setAttribute("aria-hidden", "true"); heroStage.insertBefore(poster, heroStage.firstChild); }
    poster.width = cv.width; poster.height = cv.height;
    poster.getContext("2d").drawImage(cv, 0, 0);
    if (saved) { saved.o.slide.position.z = so.z; saved.o.slide.rotation.x = so.t; ["c", "p", "d", "b", "k"].forEach((k, i) => { saved.o.L[k].position.z = so.L[i]; }); saved.o.L.b.visible = true; }
    shadowDirty = true;
    measure();
  }

  const onScroll = () => { if (visible) { kick(); if (hold === null) syncLazy(); } };
  let tl = 0;
  function syncLazy() { if (!tl) tl = setTimeout(() => { tl = 0; syncToggle(); }, 150); }
  addEventListener("scroll", onScroll, { passive: true });
  let rz = 0;
  function onResize() {
    measure(); kick();
    if (!motion) { clearTimeout(rz); rz = setTimeout(() => { makePoster(); dirty = true; kick(); }, 250); }
  }
  if ("ResizeObserver" in window) new ResizeObserver(onResize).observe(stageEl); else addEventListener("resize", onResize);
  if (!motion) new ResizeObserver(onResize).observe(heroStage);
  doc.addEventListener("visibilitychange", () => { if (!doc.hidden) { last = 0; dirty = true; kick(); } });
  if ("IntersectionObserver" in window) {
    new IntersectionObserver((en) => {
      en.forEach((x) => { visible = x.isIntersecting; if (visible) { last = 0; dirty = true; kick(); } });
    }, { rootMargin: "80px 0px" }).observe(motion ? doc.getElementById("scene") || section : xpStage);
  }

  // the page's search can open a unit here
  window.ENHRACK = { select: (id) => select(id, true), reveal, stats: () => ({ frames: renderer.info.render.frame, level, dpr, unit: act && act.id }) };

  measure();
  syncToggle();
  (async () => {
    const first = build("enhancer");
    const rack = loadRack();
    await Promise.race([rack, new Promise((r) => setTimeout(r, 5000))]);
    await first;
    if (lost) return;
    try { if (renderer.compileAsync) await Promise.race([renderer.compileAsync(scene, camera), new Promise((r) => setTimeout(r, 4000))]); } catch (e) { /* compiles on first render instead */ }
    if (root.classList.contains("gl-no")) return;   // the page already fell back to the still pictures
    if (!motion) makePoster();
    root.classList.remove("gl-try");
    root.classList.add("gl");
    revealed = true;
    t0 = performance.now();
    dirty = true; shadowDirty = true;
    kick();
    rack.then(() => { if (!motion) { makePoster(); dirty = true; kick(); } });
  })().catch((e) => fail(e && e.message || "load error"));
}
/* Fallbacks. No WebGL (or a software renderer), a lost context, or this file failing to load: the
   page keeps its still pictures (the rack screenshot, the drawn exploded diagram), and site.js lets
   the unit picker swap the diagram; nothing is scroll-driven. Reduced motion or ?static: the 3D is
   still here but nothing moves by itself: the hero shows one rendered frame, the inside section one
   exploded unit you can pick, turn and point at. No JavaScript at all: the still content. */
