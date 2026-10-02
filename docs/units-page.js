/* ENH Master site: the units gallery (units.html). Its data: units-data.js (scripts/make-site-units.py). */
(function () {
  "use strict";
  const G = window.ENHGALLERY;
  const $ = (id) => document.getElementById (id);
  if (!G) return;
  const byKey = new Map (G.units.map ((u) => [u.key, u]));
  // each unit's categories, as the gear locker files it (a unit can be in several)
  const where = new Map ();
  for (const g of G.categories)
    for (const s of g.subs)
      for (const k of s.keys) {
        if (!byKey.has (k)) continue;
        if (!where.has (k)) where.set (k, []);
        where.get (k).push ([g.name, s.name]);
      }
  const state = { cat: "All", sub: "", q: "" };
  try { const saved = JSON.parse (sessionStorage.getItem ("enh-units") || "{}"); Object.assign (state, saved); } catch (_) { /* fine */ }

  // a unit's colour stripe: warm or cool, from its name (steady, varied)
  const hueOf = (key) => { let h = 0; for (const c of key) h = (h * 31 + c.charCodeAt (0)) >>> 0; return h % 360; };

  const player = $("player");
  let playing = null;   // [button, which]
  function play (btn, src) {
    if (playing && playing[0] === btn && !player.paused) { player.pause (); return; }
    document.querySelectorAll (".ab[aria-pressed=true]").forEach ((b) => b.setAttribute ("aria-pressed", "false"));
    // the same point in the clip when switching dry / with the unit on the same card
    const card = btn.closest (".card"), same = playing && playing[0].closest (".card") === card;
    const at = same ? player.currentTime : 0;
    player.src = src; player.currentTime = 0;
    player.addEventListener ("loadedmetadata", () => { player.currentTime = Math.min (at, player.duration || at); }, { once: true });
    player.play ().catch (() => {});
    btn.setAttribute ("aria-pressed", "true");
    playing = [btn, src];
  }
  player.addEventListener ("ended", () => document.querySelectorAll (".ab[aria-pressed=true]").forEach ((b) => b.setAttribute ("aria-pressed", "false")));

  // the screens: a still until the card is in view, then the moving picture (and back when it leaves)
  const io = "IntersectionObserver" in window ? new IntersectionObserver ((entries) => {
    for (const e of entries) {
      const img = e.target, want = e.isIntersecting ? img.dataset.anim : img.dataset.still;
      if (img.getAttribute ("src") !== want) img.setAttribute ("src", want);
    }
  }, { rootMargin: "120px" }) : null;

  function el (tag, cls, text) { const e = document.createElement (tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; }

  function card (u) {
    const c = el ("article", "card");
    const h = hueOf (u.key);
    c.style.setProperty ("--hue", `linear-gradient(90deg, hsl(${h} 95% 62%), hsl(${(h + 70) % 360} 95% 60%))`);
    const face = el ("div", "face");
    const img = el ("img"); img.src = u.face; img.alt = u.name + " faceplate"; img.loading = "lazy"; img.decoding = "async";
    img.width = 960; img.height = Math.round (960 * (44.45 * u.u) / 482.6);
    face.append (img); c.append (face);
    const head = el ("div");
    head.append (el ("h2", "", u.name));
    if (u.model) head.append (el ("div", "model", u.model + (u.sub ? " · " + u.sub : "")));
    c.append (head);
    if (u.intro || u.role) c.append (el ("p", "intro", u.intro || u.role));
    if (u.screen) {
      const s = el ("div", "screen"), si = el ("img");
      si.alt = u.name + ": its live screen"; si.loading = "lazy"; si.decoding = "async"; si.width = 384; si.height = 221;
      si.dataset.still = u.still; si.dataset.anim = u.screen; si.src = u.still;
      s.append (si); c.append (s);
      if (io) io.observe (si); else si.src = u.screen;
    }
    if (u.audio) {
      const r = el ("div", "row");
      const dry = el ("button", "ab", "▶ Dry"), wet = el ("button", "ab with", "▶ With " + u.name.toLowerCase ().replace (/\b\w/g, (m) => m.toUpperCase ()));
      dry.type = wet.type = "button"; dry.setAttribute ("aria-pressed", "false"); wet.setAttribute ("aria-pressed", "false");
      dry.addEventListener ("click", () => play (dry, G.dry));
      wet.addEventListener ("click", () => play (wet, u.audio));
      r.append (dry, wet); c.append (r);
    }
    const cats = el ("div", "cats");
    for (const [g, s] of where.get (u.key) || []) cats.append (el ("span", "", g + " · " + s));
    c.append (cats);
    if (u.knobs && u.knobs.length) {
      const d = el ("details"), sm = el ("summary", "", "What each knob does"), dl = el ("dl");
      for (const k of u.knobs) { dl.append (el ("dt", "", k.k)); dl.append (el ("dd", "", k.d)); }
      d.append (sm, dl); c.append (d);
    }
    return c;
  }

  const cards = new Map (G.units.map ((u) => [u.key, card (u)]));
  const text = new Map (G.units.map ((u) => [u.key, [u.name, u.model, u.sub, u.role, u.intro, ...(where.get (u.key) || []).flat (), ...(u.knobs || []).map ((k) => k.k + " " + k.d)].join (" ").toLowerCase ()]));

  function chips () {
    const cats = $("cats"); cats.replaceChildren ();
    for (const name of ["All", ...G.categories.map ((g) => g.name)]) {
      const b = el ("button", "", name); b.type = "button"; b.setAttribute ("aria-pressed", String (state.cat === name));
      b.addEventListener ("click", () => { state.cat = name; state.sub = ""; update (); });
      cats.append (b);
    }
    const subs = $("subs"); subs.replaceChildren ();
    const g = G.categories.find ((x) => x.name === state.cat);
    if (g) for (const s of g.subs) {
      const b = el ("button", "", s.name); b.type = "button"; b.setAttribute ("aria-pressed", String (state.sub === s.name));
      b.addEventListener ("click", () => { state.sub = state.sub === s.name ? "" : s.name; update (); });
      subs.append (b);
    }
  }

  function update () {
    try { sessionStorage.setItem ("enh-units", JSON.stringify (state)); } catch (_) { /* fine */ }
    chips ();
    const words = state.q.toLowerCase ().split (/\s+/).filter (Boolean);
    let keys;
    const g = G.categories.find ((x) => x.name === state.cat);
    if (!g) keys = G.units.map ((u) => u.key);
    else keys = [...new Set (g.subs.filter ((s) => !state.sub || s.name === state.sub).flatMap ((s) => s.keys))].filter ((k) => byKey.has (k));
    keys = keys.filter ((k) => words.every ((w) => text.get (k).includes (w)));
    const grid = $("grid");
    grid.replaceChildren (...keys.map ((k) => cards.get (k)));
    if (!keys.length) grid.append (el ("p", "none", "Nothing here matches - try another category, or clear the search."));
    $("shown").textContent = keys.length === G.units.length ? `All ${keys.length} units` : `${keys.length} of ${G.units.length} units`;
  }

  $("count").textContent = String (G.units.length);
  const q = $("q");
  q.value = state.q || "";
  q.addEventListener ("input", () => { state.q = q.value; update (); });
  update ();
}) ();
