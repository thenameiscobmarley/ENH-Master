/* ENH Master site: search this page. The index is built here from the page's own text (headings,
   paragraphs, demo captions, the units and their parts, gallery captions, facts); nothing is sent
   anywhere. "/" or Ctrl+K to search, arrows to move, Enter to go, Esc to close.
   The field draws its own caret: a standing block, like a Linux terminal. Type a character and the
   block turns into that character, glides over its place and prints it left to right, then stands
   again after it; Backspace runs it backwards. Reduced motion: a plain, steady block. */
(function () {
  "use strict";
  var doc = document, root = doc.documentElement;
  var navWrap = doc.querySelector(".nav .wrap"), logo = navWrap && navWrap.querySelector(".logo");
  if (!navWrap || !logo || !("WeakMap" in window)) return;
  var still = (window.matchMedia && matchMedia("(prefers-reduced-motion: reduce)").matches) || /[?&](static|prerender)\b/.test(location.search);
  var mac = /Mac|iPhone|iPad/.test(navigator.platform || "");

  function el(tag, cls, parent, text) {
    var e = doc.createElement(tag);
    if (cls) e.className = cls;
    if (text != null) e.textContent = text;
    if (parent) parent.appendChild(e);
    return e;
  }
  function svg(parent, cls, paths) {
    var NS = "http://www.w3.org/2000/svg", s = doc.createElementNS(NS, "svg");
    s.setAttribute("viewBox", "0 0 24 24"); s.setAttribute("aria-hidden", "true"); s.setAttribute("focusable", "false");
    if (cls) s.setAttribute("class", cls);
    paths.forEach(function (d) { var p = doc.createElementNS(NS, "path"); p.setAttribute("d", d); s.appendChild(p); });
    parent.appendChild(s);
    return s;
  }

  /* ------------------------------------------------------------------ the field */
  var box = el("div", "search");
  box.setAttribute("role", "search");
  var opener = el("button", "search-open", box);
  opener.type = "button";
  opener.setAttribute("aria-label", "Search this page");
  opener.setAttribute("aria-expanded", "false");
  opener.setAttribute("aria-controls", "search-box");
  svg(opener, "", ["M10.5 18a7.5 7.5 0 1 1 0-15 7.5 7.5 0 0 1 0 15z", "M16 16l5 5"]);
  var sbox = el("div", "search-box", box);
  sbox.id = "search-box";
  svg(sbox, "search-ico", ["M10.5 18a7.5 7.5 0 1 1 0-15 7.5 7.5 0 0 1 0 15z", "M16 16l5 5"]);
  var label = el("label", "sr", sbox, "Search this page");
  label.setAttribute("for", "q");
  var field = el("div", "q-field", sbox);
  var input = el("input", "q-input", field);
  input.id = "q"; input.type = "search"; input.autocomplete = "off"; input.spellcheck = false;
  input.setAttribute("autocapitalize", "off"); input.setAttribute("autocorrect", "off");
  input.setAttribute("enterkeyhint", "go");
  input.placeholder = "Search the page";
  input.setAttribute("role", "combobox");
  input.setAttribute("aria-autocomplete", "list");
  input.setAttribute("aria-expanded", "false");
  input.setAttribute("aria-controls", "q-list");
  input.setAttribute("aria-describedby", "q-help");
  var help = el("span", "sr", sbox, "Type to search. Up and down arrows move through the results, Enter goes to one, Escape closes.");
  help.id = "q-help";
  var mir = el("div", "q-mir", field);            // what you see: the text and the caret, drawn over the (see-through) input
  mir.setAttribute("aria-hidden", "true");
  var line = el("div", "q-line", mir);
  var caret = el("div", "q-caret off", line);
  var probe = el("span", "q-probe", mir, "0");
  var kbd = el("kbd", "q-key", sbox, "/");
  kbd.setAttribute("aria-hidden", "true");
  var closer = el("button", "q-close", sbox);
  closer.type = "button";
  closer.setAttribute("aria-label", "Close search");
  svg(closer, "", ["M6 6l12 12M18 6L6 18"]);
  var panel = el("div", "q-panel", box);
  panel.hidden = true;
  var list = el("ul", "q-list", panel);
  list.id = "q-list";
  list.setAttribute("role", "listbox");
  list.setAttribute("aria-label", "Results");
  var foot = el("p", "q-foot", panel);
  foot.setAttribute("aria-hidden", "true");
  foot.textContent = "↑ ↓ to move · Enter to go · Esc to close";
  var live = el("p", "sr", box);
  live.setAttribute("aria-live", "polite");
  logo.parentNode.insertBefore(box, logo.nextSibling);
  root.classList.add("has-search");

  /* ------------------------------------------------------------------ the caret */
  var spans = [], cw = 9, animating = [], lastKey = 0, focused = false, composing = false;
  var prev = { v: "", s: 0, e: 0 };

  function measureCell() { cw = probe.getBoundingClientRect().width || 9; line.style.setProperty("--cw", cw.toFixed(2) + "px"); }
  function render(v, hideFrom, hideTo) {
    spans.forEach(function (s) { line.removeChild(s); });
    spans = [];
    for (var i = 0; i < v.length; i++) {
      var s = doc.createElement("span");
      s.className = "q-ch";
      s.textContent = v[i];
      if (hideFrom != null && i >= hideFrom && i < hideTo) s.classList.add("q-wait");
      line.insertBefore(s, caret);
      spans.push(s);
    }
  }
  function xAt(i) {                                  // left edge of the character at i (or the end)
    if (!spans.length) return 0;
    if (i < spans.length) return spans[i].offsetLeft;
    var l = spans[spans.length - 1];
    return l.offsetLeft + l.offsetWidth;
  }
  function wAt(i) { return i < spans.length ? Math.max(spans[i].offsetWidth, 3) : cw; }
  function scrollSync() { line.style.transform = "translate3d(" + (-input.scrollLeft) + "px,0,0)"; }
  function placeCaret() {
    scrollSync();
    var s = input.selectionStart, e = input.selectionEnd;
    var show = focused && s === e && !animating.length;
    caret.classList.toggle("off", !show);
    if (!show) return;
    caret.style.transform = "translate3d(" + xAt(s).toFixed(2) + "px,0,0)";
    caret.style.width = wAt(s).toFixed(2) + "px";
    caret.classList.toggle("cover", s < spans.length);       // on a character: it shows through, inverted
    restartBlink();
  }
  function restartBlink() {
    if (still) return;
    caret.classList.remove("blink");
    void caret.offsetWidth;
    caret.classList.add("blink");
  }
  function finishAll() {
    var a = animating; animating = [];
    a.forEach(function (x) { try { x.finish(); } catch (err) { /* gone */ } });
    [].forEach.call(line.querySelectorAll(".q-ghost, .q-head"), function (g) { g.remove(); });
    spans.forEach(function (s) { s.classList.remove("q-wait"); });
  }
  function track(anim, then) {
    animating.push(anim);
    anim.onfinish = anim.oncancel = function () {
      var i = animating.indexOf(anim);
      if (i >= 0) animating.splice(i, 1);
      if (then) then();
      if (!animating.length) placeCaret();
    };
    return anim;
  }
  function ghost(ch, x, w, cls) {
    var g = el("div", "q-ghost " + (cls || ""), line);
    g.style.transform = "translate3d(" + x.toFixed(2) + "px,0,0)";
    g.style.width = w.toFixed(2) + "px";
    el("i", "g-block", g);
    el("b", "g-glyph", g, ch);
    return g;
  }
  function head(x, w) {
    var h = el("div", "q-head", line);
    h.style.transform = "translate3d(" + x.toFixed(2) + "px,0,0)";
    h.style.width = w.toFixed(2) + "px";
    return h;
  }
  var EASE = "cubic-bezier(.2,.7,.1,1)", EASE_IO = "cubic-bezier(.65,0,.35,1)";
  function speed() { var now = performance.now(), gap = now - lastKey; lastKey = now; return gap < 90 ? 0.45 : gap < 170 ? 0.7 : 1; }

  /** A character typed at i. 1: the block shows the letter (dark, like a terminal's inverse) and pulls in
      to the letter's own shape, which lights up; 2: the lit letter glides onto its exact place; 3: a thin
      leading edge sweeps across it, printing the real character left to right; 4: the edge stands up
      again as the block, after it. */
  function typeAt(i, ch) {
    var k = speed();
    var x = xAt(i), w = wAt(i), sp = spans[i];
    var G = Math.max(cw, w), off = (G - w) / 2;
    var T1 = 120 * k, TG = 80 * k, TP = 130 * k, T3 = 80 * k, TT = T1 + TG + TP;
    sp.classList.add("q-wait");
    caret.classList.add("off");
    var g = ghost(ch, x, G, "morph"), blk = g.firstChild, gl = g.lastChild;
    track(blk.animate([
      { transform: "scale(1,1)", borderRadius: "1.5px", opacity: 1 },
      { transform: "scale(1,1)", borderRadius: "1.5px", opacity: 1, offset: 0.35 },
      { transform: "scale(" + (w / G * 0.9).toFixed(3) + ",.62)", borderRadius: "5px", opacity: 0 }
    ], { duration: T1, easing: EASE, fill: "forwards" }));
    var o1 = T1 * 0.35 / TT, o2 = T1 / TT, o3 = (T1 + TG) / TT;
    track(gl.animate([
      { color: "#05070a", textShadow: "none", transform: "translate3d(" + off.toFixed(2) + "px,0,0) scale(1.18,1.1)", clipPath: "inset(0 0 0 0)" },
      { color: "#05070a", textShadow: "none", transform: "translate3d(" + off.toFixed(2) + "px,0,0) scale(1.18,1.1)", clipPath: "inset(0 0 0 0)", offset: o1 },
      { color: "#dff9f1", textShadow: "0 0 10px rgba(127,227,224,.9)", transform: "translate3d(" + off.toFixed(2) + "px,-4%,0) scale(1.1,1.06)", clipPath: "inset(0 0 0 0)", offset: o2 },
      { color: "#dff9f1", textShadow: "0 0 8px rgba(127,227,224,.8)", transform: "translate3d(0,0,0) scale(1,1)", clipPath: "inset(0 0 0 0)", offset: o3 },
      { color: "#dff9f1", textShadow: "0 0 8px rgba(127,227,224,.8)", transform: "translate3d(0,0,0) scale(1,1)", clipPath: "inset(0 0 0 100%)" }
    ], { duration: TT, easing: "linear", fill: "forwards" }), function () { g.remove(); });
    track(sp.animate([{ clipPath: "inset(0 100% 0 0)" }, { clipPath: "inset(0 0 0 0)" }],
      { duration: TP, delay: T1 + TG, easing: "linear", fill: "backwards" }), function () { sp.classList.remove("q-wait"); });
    var h = head(x, 2), TH = T1 + TG + TP + T3, q0 = (T1 + TG) / TH, q1 = (T1 + TG + TP) / TH;
    track(h.animate([
      { transform: "translate3d(" + x.toFixed(2) + "px,0,0)", width: "2px", opacity: 0 },
      { transform: "translate3d(" + x.toFixed(2) + "px,0,0)", width: "2px", opacity: 0, offset: q0 * 0.9 },
      { transform: "translate3d(" + x.toFixed(2) + "px,0,0)", width: "2px", opacity: 1, offset: q0 },
      { transform: "translate3d(" + (x + w).toFixed(2) + "px,0,0)", width: "2px", opacity: 1, offset: q1 },
      { transform: "translate3d(" + (x + w).toFixed(2) + "px,0,0)", width: wAt(i + 1).toFixed(2) + "px", opacity: 1 }
    ], { duration: TH, easing: "linear", fill: "forwards" }), function () { h.remove(); });
  }

  /** Backspace, the same steps backwards: the edge sweeps back over the letter, un-printing it into its
      lit shape; the letter goes dark and the block grows round it; the block stands where it was. */
  function backAt(i, ch, oldW) {
    var k = speed();
    var x = xAt(i), w = oldW, G = Math.max(cw, w), off = (G - w) / 2;
    var TP = 110 * k, T1 = 120 * k, TT = TP + T1;
    caret.classList.add("off");
    var g = ghost(ch, x, G, "back"), blk = g.firstChild, gl = g.lastChild;
    var o1 = TP / TT;
    track(gl.animate([
      { color: "var(--q-ink)", textShadow: "none", transform: "translate3d(0,0,0) scale(1,1)" },
      { color: "#dff9f1", textShadow: "0 0 9px rgba(127,227,224,.85)", transform: "translate3d(0,0,0) scale(1,1)", offset: o1 },
      { color: "#05070a", textShadow: "none", transform: "translate3d(" + off.toFixed(2) + "px,0,0) scale(1.18,1.1)", offset: o1 + (1 - o1) * 0.6 },
      { color: "#05070a", textShadow: "none", transform: "translate3d(" + off.toFixed(2) + "px,0,0) scale(1.18,1.1)", opacity: 0 }
    ], { duration: TT, easing: "linear", fill: "forwards" }), function () { g.remove(); });
    track(blk.animate([
      { transform: "scale(" + (w / G * 0.9).toFixed(3) + ",.62)", borderRadius: "5px", opacity: 0 },
      { transform: "scale(" + (w / G * 0.9).toFixed(3) + ",.62)", borderRadius: "5px", opacity: 0, offset: o1 },
      { transform: "scale(1,1)", borderRadius: "1.5px", opacity: 1 }
    ], { duration: TT, easing: EASE_IO, fill: "forwards" }));
    var h = head(x + w, 2);
    track(h.animate([
      { transform: "translate3d(" + (x + w).toFixed(2) + "px,0,0)", width: "2px", opacity: 1 },
      { transform: "translate3d(" + x.toFixed(2) + "px,0,0)", width: "2px", opacity: 1, offset: 0.92 },
      { transform: "translate3d(" + x.toFixed(2) + "px,0,0)", width: "2px", opacity: 0 }
    ], { duration: TP, easing: "linear", fill: "forwards" }), function () { h.remove(); });
  }

  /** Several characters at once (paste, autocomplete, a word from the phone's keyboard): one print sweep. */
  function printRange(a, b) {
    var x0 = xAt(a), x1 = xAt(b), dur = Math.min(90 + 22 * (b - a), 380);
    caret.classList.add("off");
    for (var i = a; i < b; i++) {
      (function (sp, t) {
        sp.classList.add("q-wait");
        track(sp.animate([{ clipPath: "inset(0 100% 0 0)" }, { clipPath: "inset(0 0 0 0)" }],
          { duration: Math.max(40, dur * 0.25), delay: t, easing: "linear", fill: "backwards" }), function () { sp.classList.remove("q-wait"); });
      })(spans[i], (xAt(i) - x0) / Math.max(1, x1 - x0) * dur * 0.8);
    }
    var h = head(x0, 2);
    track(h.animate([
      { transform: "translate3d(" + x0.toFixed(2) + "px,0,0)", width: "2px" },
      { transform: "translate3d(" + x1.toFixed(2) + "px,0,0)", width: "2px", offset: 0.8 },
      { transform: "translate3d(" + x1.toFixed(2) + "px,0,0)", width: wAt(b).toFixed(2) + "px" }
    ], { duration: dur, easing: "linear", fill: "forwards" }), function () { h.remove(); });
  }

  function onInput(e) {
    var v = input.value, p = prev.v;
    finishAll();
    var a = 0;
    while (a < v.length && a < p.length && v[a] === p[a]) a++;
    var b = 0;
    while (b < v.length - a && b < p.length - a && v[v.length - 1 - b] === p[p.length - 1 - b]) b++;
    var added = v.slice(a, v.length - b), removed = p.slice(a, p.length - b);
    var oldW = 0;
    if (removed.length === 1 && spans[a]) oldW = spans[a].offsetWidth;
    render(v);
    scrollSync();
    var inComp = composing || (e && e.isComposing);
    if (!still && focused && typeof line.animate === "function" && spans.length) {
      if (added.length === 1 && !removed.length) typeAt(a, added);
      else if (removed.length === 1 && !added.length) backAt(a, removed, oldW || cw);
      else if (added.length > 1 && !inComp) printRange(a, a + added.length);
    }
    prev = { v: v, s: input.selectionStart, e: input.selectionEnd };
    if (!animating.length) placeCaret();
    query(v);
  }
  input.addEventListener("input", onInput);
  input.addEventListener("compositionstart", function () { composing = true; });
  input.addEventListener("compositionend", function () { composing = false; });
  ["keyup", "click", "select", "scroll"].forEach(function (t) { input.addEventListener(t, function () { if (!animating.length) placeCaret(); else scrollSync(); }); });
  doc.addEventListener("selectionchange", function () { if (doc.activeElement === input && !animating.length) placeCaret(); });
  input.addEventListener("focus", function () {
    focused = true; box.classList.add("focus"); measureCell(); render(input.value); placeCaret();
    if (input.value.trim()) query(input.value);
  });
  input.addEventListener("blur", function () { focused = false; box.classList.remove("focus"); finishAll(); placeCaret(); });
  if (doc.fonts && doc.fonts.ready) doc.fonts.ready.then(function () { measureCell(); if (focused) placeCaret(); });

  /* ------------------------------------------------------------------ the index */
  var index = [];
  function norm(s) { return (s || "").toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "").replace(/[^a-z0-9%.+×·]+/g, " ").trim(); }
  function sectionOf(e) {
    var s = e.closest("section[id]");
    if (!s) return { id: "top", name: "ENH Master" };
    var h = s.querySelector("h1, h2");
    var nav = doc.querySelector('.nav a[href="#' + s.id + '"]');
    return { id: s.id, name: nav ? nav.textContent.trim() : h ? h.textContent.replace(/\s+/g, " ").trim() : s.id };
  }
  function build() {
    index = [];
    var SEL = "h1, h2, h3, p, li, figcaption, dt, dd, summary, caption, pre, .facts > div, .stat, .lbl, .k-l";
    var seen = new WeakMap();
    [].forEach.call(doc.querySelectorAll("main " + SEL.split(", ").join(", main ") + ", footer span"), function (e) {
      if (e.closest("[aria-hidden='true'], .xp-list-w, .xp-pick, .xp-info, .xp-steps, .q-panel, .search, .gl-layer, .xp-hot")) return;
      var up = e.parentElement && e.parentElement.closest(SEL);
      if (up && up.closest("main, footer") && !e.matches("h1, h2, h3")) return;       // nested: its parent covers it
      var text = e.textContent.replace(/\s+/g, " ").trim();
      if (text.length < 3 || seen.has(e)) return;
      seen.set(e, 1);
      var sec = sectionOf(e), isHead = /^H[123]$/.test(e.tagName);
      var title = isHead ? text : e.tagName === "FIGCAPTION" && e.querySelector("b") ? e.querySelector("b").textContent : "";
      index.push({ el: e, sec: sec, title: title, text: text, nt: norm(title), nx: norm(text), w: isHead ? 3 : e.tagName === "FIGCAPTION" ? 1.5 : 1 });
    });
    (window.ENHUNITS || []).forEach(function (u) {
      var sec = { id: "inside", name: "Inside · unit " + (u.pos < 10 ? "0" : "") + u.pos };
      var t = u.name, d = u.role.replace(/ - /g, " · ").toLowerCase() + ". " + u.blurb;
      index.push({ unit: u.id, sec: sec, title: t, text: d, nt: norm(t), nx: norm(d), w: 2.6 });
      u.hot.forEach(function (h) {
        index.push({ unit: u.id, sec: { id: "inside", name: u.name }, title: h.t, text: h.d, nt: norm(h.t), nx: norm(h.d), w: 1.2 });
      });
    });
  }

  function score(it, toks, phrase) {
    var s = 0;
    for (var i = 0; i < toks.length; i++) {
      var t = toks[i], sc = 0;
      var wt = new RegExp("(^| )" + t.replace(/[.+%×·]/g, "\\$&")).test(it.nt), wx = new RegExp("(^| )" + t.replace(/[.+%×·]/g, "\\$&")).test(it.nx);
      if (wt) sc = 7; else if (t.length > 2 && it.nt.indexOf(t) >= 0) sc = 4;
      else if (wx) sc = 2.2; else if (t.length > 2 && it.nx.indexOf(t) >= 0) sc = 1;
      else if (new RegExp("(^| )" + t.replace(/[.+%×·]/g, "\\$&")).test(it.ns || (it.ns = norm(it.sec.name)))) sc = 1.6;
      if (!sc) return 0;
      s += sc;
    }
    if (toks.length > 1 && (it.nt + " " + it.nx).indexOf(phrase) >= 0) s += 5;
    if (it.nt === phrase) s += 6;
    return s * it.w / (1 + Math.min(it.text.length, 600) / 900);
  }
  var results = [], active = -1;
  function query(q) {
    var phrase = norm(q), toks = phrase ? phrase.split(" ").filter(Boolean) : [];
    list.textContent = "";
    results = []; active = -1;
    input.removeAttribute("aria-activedescendant");
    if (!toks.length) { setOpen(false); live.textContent = ""; return; }
    if (!index.length) build();
    var seenEl = new WeakMap(), seenKey = {};
    index.forEach(function (it) {
      if (it.el && !it.el.getClientRects().length && !it.el.closest("details, .xp-list-w")) return;   // not shown at this size
      var s = score(it, toks, phrase);
      if (s > 0) results.push({ it: it, s: s });
    });
    results.sort(function (a, b) { return b.s - a.s; });
    results = results.filter(function (r) {
      var k = r.it.el ? null : r.it.unit + "|" + r.it.title;
      if (r.it.el) { if (seenEl.has(r.it.el)) return false; seenEl.set(r.it.el, 1); return true; }
      if (seenKey[k]) return false; seenKey[k] = 1; return true;
    }).slice(0, 8);
    results.forEach(function (r, i) {
      var li = el("li", "q-opt", list);
      li.id = "q-o-" + i;
      li.setAttribute("role", "option");
      li.setAttribute("aria-selected", "false");
      el("span", "q-sec", li, r.it.sec.name);
      var tt = el("span", "q-title", li);
      marked(tt, r.it.title || snippet(r.it.text, toks, 64), toks);
      if (r.it.title) marked(el("span", "q-snip", li), snippet(r.it.text, toks, 120), toks);
      li.addEventListener("pointerdown", function (e) { e.preventDefault(); });
      li.addEventListener("click", function () { go(i); });
      li.addEventListener("pointermove", function () { if (active !== i) setActive(i, true); });
    });
    if (!results.length) {
      var none = el("li", "q-none", list, "Nothing on this page matches “" + q.trim() + "”.");
      none.setAttribute("role", "presentation");
    }
    live.textContent = results.length ? results.length + (results.length === 1 ? " result" : " results") : "No results";
    setOpen(true);
    if (results.length) setActive(0);
  }
  function snippet(text, toks, n) {
    var lo = text.toLowerCase(), at = -1;
    for (var i = 0; i < toks.length && at < 0; i++) at = lo.indexOf(toks[i]);
    if (text.length <= n || at < 0) return text.length <= n ? text : text.slice(0, n).replace(/\s+\S*$/, "") + "…";
    var a = Math.max(0, at - Math.round(n * 0.3)), b = Math.min(text.length, a + n);
    var s = text.slice(a, b);
    if (a > 0) s = "…" + s.replace(/^\S*\s/, "");
    if (b < text.length) s = s.replace(/\s+\S*$/, "") + "…";
    return s;
  }
  function marked(parent, text, toks) {
    var re = new RegExp("(" + toks.map(function (t) { return t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"); }).join("|") + ")", "ig");
    var last = 0, m;
    while ((m = re.exec(text))) {
      if (!m[0].length) { re.lastIndex++; continue; }
      if (m.index > last) parent.appendChild(doc.createTextNode(text.slice(last, m.index)));
      el("mark", "", parent, m[0]);
      last = m.index + m[0].length;
    }
    if (last < text.length) parent.appendChild(doc.createTextNode(text.slice(last)));
  }
  function setOpen(on) {
    panel.hidden = !on;
    input.setAttribute("aria-expanded", String(on));
    box.classList.toggle("results", on);
  }
  function setActive(i, noScroll) {
    var opts = list.querySelectorAll(".q-opt");
    if (!opts.length) return;
    active = (i + opts.length) % opts.length;
    [].forEach.call(opts, function (o, j) { o.setAttribute("aria-selected", String(j === active)); });
    input.setAttribute("aria-activedescendant", opts[active].id);
    if (!noScroll) opts[active].scrollIntoView({ block: "nearest" });
  }

  /* ------------------------------------------------------------------ going to a result */
  var hitMarks = [];
  function clearHits() {
    hitMarks.forEach(function (m) { var p = m.parentNode; if (!p) return; while (m.firstChild) p.insertBefore(m.firstChild, m); p.removeChild(m); p.normalize(); });
    hitMarks = [];
    [].forEach.call(doc.querySelectorAll(".q-found"), function (e) { e.classList.remove("q-found"); });
  }
  function highlight(e, toks) {
    var w = doc.createTreeWalker(e, NodeFilter.SHOW_TEXT), n;
    while ((n = w.nextNode())) {
      var lo = n.nodeValue.toLowerCase();
      for (var i = 0; i < toks.length; i++) {
        var at = lo.indexOf(toks[i]);
        if (at >= 0) {
          var r = n.splitText(at); r.splitText(toks[i].length);
          var m = doc.createElement("mark"); m.className = "q-hit";
          r.parentNode.insertBefore(m, r); m.appendChild(r);
          hitMarks.push(m);
          return;
        }
      }
    }
  }
  function go(i) {
    var r = results[i]; if (!r) return;
    var it = r.it, toks = norm(input.value).split(" ").filter(Boolean);
    clearHits();
    setOpen(false);
    closeBar();
    input.blur();
    if (it.unit) {
      var btn = doc.querySelector('.xp-u[data-unit="' + it.unit + '"]');
      if (btn) btn.focus({ preventScroll: true });          // focus first: moving focus later would stop the scroll
      if (root.classList.contains("gl") && window.ENHRACK) window.ENHRACK.select(it.unit);
      else { if (btn) btn.click(); doc.getElementById("inside").scrollIntoView({ behavior: still ? "auto" : "smooth", block: "start" }); }
      return;
    }
    var e = it.el;
    var det = e.closest("details"); if (det) det.open = true;
    var rv = e.closest(".rv"); if (rv) rv.classList.add("in");
    highlight(e, toks);
    e.classList.add("q-found");
    e.scrollIntoView({ behavior: still ? "auto" : "smooth", block: "center" });
    if (!e.hasAttribute("tabindex")) e.setAttribute("tabindex", "-1");
    e.focus({ preventScroll: true });
    setTimeout(clearHits, 6000);
  }

  /* ------------------------------------------------------------------ keys, opening and closing */
  function openBar() { box.classList.add("open"); opener.setAttribute("aria-expanded", "true"); }
  function closeBar() { box.classList.remove("open"); opener.setAttribute("aria-expanded", "false"); }
  opener.addEventListener("click", function () { openBar(); input.focus(); });
  closer.addEventListener("click", function () { input.value = ""; onInput(); setOpen(false); closeBar(); opener.focus(); });
  input.addEventListener("keydown", function (e) {
    var k = e.key;
    if (k === "ArrowDown" || k === "ArrowUp") {
      if (panel.hidden && input.value.trim()) query(input.value);
      else if (results.length) setActive(active + (k === "ArrowDown" ? 1 : -1));
      e.preventDefault();
    } else if (k === "Enter") {
      if (results.length) { e.preventDefault(); go(active < 0 ? 0 : active); }
    } else if (k === "Escape") {
      e.preventDefault();
      if (!panel.hidden) setOpen(false);
      else if (input.value) { input.value = ""; onInput(); }
      else { closeBar(); input.blur(); if (getComputedStyle(opener).display !== "none") opener.focus(); }
    } else if (k === "Tab") setOpen(false);
  });
  doc.addEventListener("keydown", function (e) {
    var t = e.target, typing = t && (t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName));
    if ((e.key === "k" || e.key === "K") && (mac ? e.metaKey : e.ctrlKey) && !e.altKey) { e.preventDefault(); openBar(); input.focus(); input.select(); return; }
    if (e.key === "/" && !typing && !e.ctrlKey && !e.metaKey && !e.altKey && !(t && t.closest && t.closest("dialog[open]"))) { e.preventDefault(); openBar(); input.focus(); }
  });
  doc.addEventListener("pointerdown", function (e) { if (!box.contains(e.target)) { setOpen(false); if (!input.value) closeBar(); } });
  addEventListener("resize", function () { if (focused) { measureCell(); placeCaret(); } }, { passive: true });
  kbd.textContent = "/";
  sbox.title = "Search (/ or " + (mac ? "⌘K" : "Ctrl+K") + ")";
})();
