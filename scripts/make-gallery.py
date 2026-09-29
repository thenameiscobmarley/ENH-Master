#!/usr/bin/env python3
"""Refresh the site gallery's list of designer presets (docs/gallery.js) from docs/designer.js."""
import json, re, pathlib
docs = pathlib.Path(__file__).resolve().parent.parent / "docs"
src = (docs / "designer.js").read_text()
names = re.findall(r'^    "([^"]+)": \(\) =>', src, re.M) + re.findall(r'^  templates\["([^"]+)"\] =', src, re.M)
g = (docs / "gallery.js").read_text()
g = re.sub(r"  presets: \[.*?\]", "  presets: " + json.dumps(names), g, flags=re.S)
(docs / "gallery.js").write_text(g)
print(len(names), "presets")
