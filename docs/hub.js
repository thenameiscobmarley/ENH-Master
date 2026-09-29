/* The site's places. Home is the intro (the hero, then the rack taken apart as you scroll) and ends in the
   HUB: four places, each opened by a click - no long scrolling to find things.
     #/hub       the hub by itself (the nav's Hub)
     #/rack      what's inside, who it's for, the demos, the test lab
     #/download  get it, and check your download
     #/gallery   the designs (the units built into the plugin, and the designer's presets)
     designer.html  the Rack Unit Designer (its own page)
   A place is the page's own sections, shown on their own (sections carry data-place). A link to a section
   inside a place (#radar) opens its place and goes there. No storage, no network. */
(function () {
  "use strict";
  var root = document.documentElement;
  var PLACES = { hub: "Hub", rack: "The rack", download: "Download", gallery: "Designs" };

  function ready (fn) { if (document.readyState !== "loading") fn(); else document.addEventListener ("DOMContentLoaded", fn); }

  ready (function () {
    var body = document.body;
    var sections = [].slice.call (document.querySelectorAll ("[data-place]"));
    var reduce = window.matchMedia && window.matchMedia ("(prefers-reduced-motion: reduce)").matches;

    /** Shows a place (or home, ""), then goes to `anchor` inside it (or the top). */
    function show (place, anchor, fromClick) {
      if (place && !PLACES[place]) place = "";
      var changed = (body.getAttribute ("data-place") || "") !== place;
      var go = function () {
        if (place) body.setAttribute ("data-place", place); else body.removeAttribute ("data-place");
        sections.forEach (function (s) { s.hidden = s.getAttribute ("data-place") !== place; });   // (home: none of them)
        // (#/hub: the hub by itself - the intro is for arriving, not for every trip back)
        [].forEach.call (document.querySelectorAll ("[data-home]"), function (s) { s.hidden = !!place && !(place === "hub" && s.id === "hub"); });
        document.title = place ? PLACES[place] + " · ENH Master" : "ENH Master";
        [].forEach.call (document.querySelectorAll ("[data-go]"), function (a) {
          if (a.getAttribute ("data-go") === place) a.setAttribute ("aria-current", "page"); else a.removeAttribute ("aria-current");
        });
        var target = anchor && document.getElementById (anchor);
        if (target) target.scrollIntoView ({ behavior: fromClick && !reduce ? "smooth" : "auto" });
        else if (changed) window.scrollTo (0, 0);
        window.dispatchEvent (new Event ("resize"));   // (the rack and the demos size themselves to what shows)
        if (place === "gallery") buildGallery();
      };
      if (!changed || reduce) { go(); return; }
      body.classList.add ("place-out");                // the old place fades and drops, the new one rises in
      setTimeout (function () { go(); body.classList.remove ("place-out"); body.classList.add ("place-in");
        setTimeout (function () { body.classList.remove ("place-in"); }, 420); }, 220);
    }

    /** Where the address says: #/place, #section (inside its place), or home. */
    function route (fromClick) {
      var h = location.hash.replace (/^#/, "");
      if (h.charAt (0) === "/") { show (h.slice (1).split ("/")[0], null, fromClick); return; }
      var el = h && document.getElementById (h);
      var holder = el && el.closest ("[data-place]");
      if (holder) show (holder.getAttribute ("data-place"), h, fromClick);
      else show ("", h || null, fromClick);
    }
    window.addEventListener ("hashchange", function () { route (true); });
    route (false);

    /** The gallery: the units built into the plugin (their share codes open them in the designer), then the
        designer's presets by name. Text only, built with DOM calls. */
    var built = false;
    function buildGallery () {
      if (built || !window.ENHGALLERY) return;
      built = true;
      var g = window.ENHGALLERY;
      var units = document.getElementById ("gallery-units"), presets = document.getElementById ("gallery-presets");
      g.units.forEach (function (u) {
        if (!/^ENH2(-[0-9A-Za-z]{1,4})+$/.test (u.code)) return;   // (codes only: nothing else goes in a link)
        var a = document.createElement ("a");
        a.className = "g-card"; a.href = "designer.html#d=" + u.code;
        var t = document.createElement ("span"); t.className = "g-tag"; t.textContent = u.tag;
        var n = document.createElement ("b"); n.textContent = u.name;
        var p = document.createElement ("span"); p.className = "g-about"; p.textContent = u.about;
        var o = document.createElement ("span"); o.className = "g-open"; o.textContent = "Open in the designer →";
        a.append (t, n, p, o); units.appendChild (a);
      });
      g.presets.forEach (function (name) {
        var a = document.createElement ("a");
        a.className = "g-chip"; a.href = "designer.html?preset=" + encodeURIComponent (name);
        a.textContent = name; presets.appendChild (a);
      });
    }
  });
}());
