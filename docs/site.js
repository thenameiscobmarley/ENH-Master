/* ENH Master site: navigation, scroll reveals, counters, tilt, graph draw-ins, gauges, lightbox.
   No network access, no storage, no cookies. Everything is readable without this file. */
(function () {
  "use strict";
  var root = document.documentElement;
  root.classList.remove("no-js");
  root.classList.add("js");
  var reduce = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var isStatic = /[?&](static|prerender)\b/.test(location.search);
  var animate = !reduce && !isStatic && "IntersectionObserver" in window;
  root.classList.add(animate ? "motion" : "still");
  window.ENHMOTION = animate;

  /* The 3D rack (rack3d.js, an ES module) needs WebGL 2. While it loads, the hero keeps a quiet
     placeholder; if it can't run, rack3d.js (or the checks here) put the still pictures back. */
  var prerender = /[?&]prerender\b/.test(location.search);
  if (!prerender && window.WebGL2RenderingContext && "noModule" in document.createElement("script")) {
    root.classList.add("gl-try");
    if (animate) root.classList.add("pin");                 // the scroll-driven 3D sequence
    var giveUp = function (late) { if (!root.classList.contains("gl") && !(late && window.ENHRACK_RUNNING)) { root.classList.remove("gl-try", "pin"); root.classList.add("gl-no"); } };
    window.addEventListener("error", function (e) {        // the module or three.js failed to load
      var t = e.target;
      if (t && t.tagName === "SCRIPT" && /rack3d|three/.test(t.src || "")) giveUp();
    }, true);
    setTimeout(function () { giveUp(true); }, 15000);   // the module never ran (rack3d.js handles its own failures once it runs)
  } else root.classList.add("gl-no");

  function ready(fn) { if (document.readyState !== "loading") fn(); else document.addEventListener("DOMContentLoaded", fn); }
  function easeOut(t) { return 1 - Math.pow(1 - t, 3); }
  function each(sel, fn, rootEl) { [].forEach.call((rootEl || document).querySelectorAll(sel), fn); }
  function once(els, fn, opts) {                 // call fn(el) the first time each element is seen
    if (!("IntersectionObserver" in window)) { els.forEach(fn); return; }
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) { if (en.isIntersecting) { io.unobserve(en.target); fn(en.target); } });
    }, opts || { threshold: 0.25 });
    els.forEach(function (e) { io.observe(e); });
  }

  ready(function () {
    if (isStatic) each("img[loading=lazy]", function (i) { i.loading = "eager"; });
    var nav = document.querySelector(".nav");
    var bar = document.querySelector(".progress");
    var para = [].slice.call(document.querySelectorAll("[data-parallax]"));

    /* ---- navigation: mobile menu and the section you're in ---- */
    var menuBtn = document.querySelector(".menu-btn");
    function setMenu(open) {
      nav.classList.toggle("open", open);
      menuBtn.setAttribute("aria-expanded", String(open));
    }
    if (menuBtn) {
      menuBtn.addEventListener("click", function () { setMenu(!nav.classList.contains("open")); });
      each("#nav-links a", function (a) { a.addEventListener("click", function () { setMenu(false); }); });
      document.addEventListener("keydown", function (e) { if (e.key === "Escape" && nav.classList.contains("open")) { setMenu(false); menuBtn.focus(); } });
      document.addEventListener("click", function (e) { if (nav.classList.contains("open") && !nav.contains(e.target)) setMenu(false); });
    }
    var navLinks = [].slice.call(document.querySelectorAll("#nav-links a"));
    var targets = navLinks.map(function (a) { return document.querySelector(a.getAttribute("href")); });
    var current = -1;
    function markSection(vh) {
      var idx = -1;
      for (var i = 0; i < targets.length; i++) {
        var t = targets[i]; if (!t) continue;
        if (t.getBoundingClientRect().top <= vh * 0.4) idx = i;
      }
      if (idx === current) return;
      current = idx;
      navLinks.forEach(function (a, i) { if (i === idx) a.setAttribute("aria-current", "true"); else a.removeAttribute("aria-current"); });
    }

    /* ---- scroll-driven: progress bar, nav, hero tilt, parallax (rAF-throttled, passive listeners) ---- */
    var ticking = false;
    function onScroll() { if (!ticking) { ticking = true; requestAnimationFrame(update); } }
    function update() {
      ticking = false;
      var y = window.scrollY || window.pageYOffset;
      var vh = window.innerHeight;
      var max = document.documentElement.scrollHeight - vh;
      if (bar) bar.style.transform = "scaleX(" + (max > 0 ? Math.min(1, y / max) : 0).toFixed(4) + ")";
      if (nav) nav.classList.toggle("solid", y > 24);
      markSection(vh);
      if (!animate) return;
      for (var i = 0; i < para.length; i++) {
        var el = para[i], r = el.getBoundingClientRect();
        if (r.bottom < -200 || r.top > vh + 200) continue;
        var c = (r.top + r.height / 2 - vh / 2) * parseFloat(el.getAttribute("data-parallax"));
        el.style.transform = "translate3d(0," + c.toFixed(1) + "px,0)";
      }
    }
    window.addEventListener("scroll", onScroll, { passive: true });
    window.addEventListener("resize", onScroll, { passive: true });
    update();

    /* ---- gauges and bars: markup holds the final values; we play them in from zero ---- */
    function play(box) {
      var arcs = [].slice.call(box.querySelectorAll("[data-arc]"));
      var nums = [].slice.call(box.querySelectorAll("[data-count]"));
      var rots = [].slice.call(box.querySelectorAll("[data-rot]"));
      var bars = [].slice.call(box.querySelectorAll("[data-w]"));
      var dur = 1800, t0 = null;
      function frameFn(ts) {
        if (t0 === null) t0 = ts;
        var t = Math.min(1, (ts - t0) / dur), e = easeOut(t);
        arcs.forEach(function (a) {
          var from = parseFloat(a.getAttribute("data-from") || "100"), to = parseFloat(a.getAttribute("data-arc"));
          a.style.strokeDashoffset = from + (to - from) * e;
        });
        nums.forEach(function (n) {
          var from = parseFloat(n.getAttribute("data-from") || "0"), to = parseFloat(n.getAttribute("data-count"));
          var d = parseInt(n.getAttribute("data-dec") || "0", 10), v = from + (to - from) * e;
          var s = v.toFixed(d);
          if (n.hasAttribute("data-sign") && v > 0) s = "+" + s;
          n.textContent = s.replace("-", "−") + (n.getAttribute("data-suffix") || "");
        });
        rots.forEach(function (g) {
          var from = parseFloat(g.getAttribute("data-rot0")), to = parseFloat(g.getAttribute("data-rot"));
          g.setAttribute("transform", "rotate(" + (from + (to - from) * e).toFixed(2) + " " + g.getAttribute("data-c") + ")");
        });
        bars.forEach(function (b) { b.setAttribute("width", (parseFloat(b.getAttribute("data-w")) * e).toFixed(2)); });
        if (t < 1) requestAnimationFrame(frameFn);
      }
      requestAnimationFrame(frameFn);
    }

    if (animate) {
      var gauges = [].slice.call(document.querySelectorAll(".anim"));
      gauges.forEach(function (box) {        // park them at zero until seen
        each("[data-arc]", function (a) { a.style.strokeDashoffset = a.getAttribute("data-from") || "100"; }, box);
        each("[data-w]", function (b) { b.setAttribute("width", "0"); }, box);
        each("[data-rot]", function (g) { g.setAttribute("transform", "rotate(" + g.getAttribute("data-rot0") + " " + g.getAttribute("data-c") + ")"); }, box);
      });
      once(gauges, play, { threshold: 0.35 });

      /* staggered reveals: siblings that come into view together follow one another */
      var rv = [].slice.call(document.querySelectorAll(".rv"));
      rv.forEach(function (el) {
        if (el.hasAttribute("data-delay") || el.closest(".hero")) return;
        var sib = [].filter.call(el.parentNode.children, function (c) { return c.classList.contains("rv"); });
        var i = sib.indexOf(el);
        if (i > 0) el.setAttribute("data-delay", String(Math.min(i, 6) * 80));
      });
      once(rv, function (el) {
        var d = el.getAttribute("data-delay");
        if (d) el.style.transitionDelay = d + "ms";
        el.classList.add("in");
      }, { threshold: 0.12, rootMargin: "0px 0px -6% 0px" });

      /* aurora sweep through gradient headings */
      once([].slice.call(document.querySelectorAll(".grad.sweep")), function (el) {
        setTimeout(function () { el.classList.add("go"); }, 250);
      }, { threshold: 0.6 });

      /* counters count up */
      once([].slice.call(document.querySelectorAll("[data-num]")), function (el) {
        var m = /^(\D*)(\d+(?:\.\d+)?)([\s\S]*)$/.exec(el.textContent);
        if (!m) return;
        var to = parseFloat(m[2]), dec = (m[2].split(".")[1] || "").length, t0 = null;
        function step(ts) {
          if (t0 === null) t0 = ts;
          var t = Math.min(1, (ts - t0) / 1500);
          el.textContent = m[1] + (to * easeOut(t)).toFixed(dec) + m[3];
          if (t < 1) requestAnimationFrame(step);
        }
        el.textContent = m[1] + (0).toFixed(dec) + m[3];
        requestAnimationFrame(step);
      }, { threshold: 0.6 });

      /* the heat map fills in cell by cell */
      once([].slice.call(document.querySelectorAll(".heat")), function (t) {
        [].forEach.call(t.querySelectorAll("tbody td"), function (td) {
          var r = td.parentNode.rowIndex, c = td.cellIndex;
          td.style.animationDelay = ((r + c) * 45) + "ms";
        });
        t.classList.add("go");
      }, { threshold: 0.3 });

      /* demo graphs draw themselves in the first time they're seen */
      once([].slice.call(document.querySelectorAll("[data-demo]")), function (box) {
        var lines = box.querySelectorAll(".g-outl, .r-liftl, .out, .g-inp, .r-send, .g-mom");
        [].forEach.call(lines, function (p) { p.setAttribute("pathLength", "1"); });
        box.classList.add("drawin");
        setTimeout(function () {
          box.classList.remove("drawin");
          [].forEach.call(box.querySelectorAll("[pathLength='1']"), function (p) { if (!p.matches(".g-val, .g-glow, .k-val, .k-glow, .g-ghost")) p.removeAttribute("pathLength"); });
        }, 2400);
      }, { threshold: 0.35 });

      /* cards lean toward the pointer (mouse and pen only) */
      if (window.matchMedia("(hover: hover) and (pointer: fine)").matches) {
        each(".tilt", function (card) {
          var raf = 0, px = 0, py = 0;
          function apply() {
            raf = 0;
            card.style.transform = "perspective(900px) rotateX(" + (-py * 5).toFixed(2) + "deg) rotateY(" + (px * 6).toFixed(2) + "deg) translate3d(0,-3px,0)";
          }
          card.addEventListener("pointermove", function (e) {
            if (e.pointerType !== "mouse" && e.pointerType !== "pen") return;
            var r = card.getBoundingClientRect();
            px = (e.clientX - r.left) / r.width - 0.5; py = (e.clientY - r.top) / r.height - 0.5;
            card.classList.add("tilting");
            if (!raf) raf = requestAnimationFrame(apply);
          });
          card.addEventListener("pointerleave", function () {
            if (raf) cancelAnimationFrame(raf), raf = 0;
            card.classList.remove("tilting");
            card.style.transform = "";
          });
        });
      }
    }

    /* ---- inside the rack without WebGL: the picker swaps the drawn exploded diagram ---- */
    var xpStatic = document.querySelector(".xp-static");
    var U = window.ENHUNITS || [];
    if (xpStatic && U.length) {
      var infoK = document.querySelector(".xp-info-k"), infoT = document.querySelector(".xp-info-t"), infoD = document.querySelector(".xp-info-d");
      each(".xp-u", function (b) {
        b.addEventListener("click", function () {
          if (root.classList.contains("gl")) return;            // the 3D view handles it
          var id = b.getAttribute("data-unit"), u = null;
          U.forEach(function (x) { if (x.id === id) u = x; });
          if (!u) return;
          each(".xp-u", function (o) { o.setAttribute("aria-pressed", String(o === b)); });
          ["b", "d", "p", "c"].forEach(function (k) { var im = xpStatic.querySelector(".xs-" + k); if (im) im.src = "img/units/" + id + "-" + k + ".svg"; });
          var role = u.role.toLowerCase().replace(/(^|[\s-])([a-z])/g, function (m, a, c) { return a + c.toUpperCase(); }).replace(/ - /g, " · ");
          infoK.textContent = (u.pos < 10 ? "0" : "") + u.pos + " · " + role;
          infoT.textContent = u.name;
          infoD.textContent = u.blurb + " Front to back: the controls, the faceplate, the displays and meters, the DSP board and the chassis.";
        });
      });
    }

    /* ---- lightbox ---- */
    var dlg = document.getElementById("lb");
    var links = [].slice.call(document.querySelectorAll("a[data-lb]"));
    if (!dlg || typeof dlg.showModal !== "function" || !links.length) return;
    var img = dlg.querySelector("img"), cap = dlg.querySelector(".lb-bar p");
    var cur = 0, opener = null;
    function show(i) {
      cur = (i + links.length) % links.length;
      var a = links[cur], fig = a.closest("figure"), t = a.querySelector("img");
      img.src = a.getAttribute("data-lb") || a.getAttribute("href");
      img.alt = t ? t.alt : "";
      var fc = fig && fig.querySelector("figcaption");
      cap.innerHTML = fc ? fc.innerHTML : "";  /* our own static captions */
    }
    links.forEach(function (a, i) {
      a.addEventListener("click", function (ev) {
        if (ev.button !== 0 || ev.ctrlKey || ev.metaKey || ev.shiftKey) return;
        ev.preventDefault(); opener = a; show(i); dlg.showModal();
        dlg.querySelector(".lb-close").focus();
      });
    });
    dlg.querySelector(".lb-close").addEventListener("click", function () { dlg.close(); });
    dlg.querySelector(".lb-prev").addEventListener("click", function () { show(cur - 1); });
    dlg.querySelector(".lb-next").addEventListener("click", function () { show(cur + 1); });
    dlg.addEventListener("click", function (ev) { if (ev.target === dlg || ev.target.classList.contains("lb-img")) dlg.close(); });
    dlg.addEventListener("keydown", function (ev) {
      if (ev.key === "ArrowRight") { ev.preventDefault(); show(cur + 1); }
      else if (ev.key === "ArrowLeft") { ev.preventDefault(); show(cur - 1); }
    });
    dlg.addEventListener("close", function () { img.removeAttribute("src"); if (opener) opener.focus(); });
  });
})();
