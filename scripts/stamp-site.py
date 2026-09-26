#!/usr/bin/env python3
"""Cache-busting for the website: every local .css / .js the pages in docs/ load gets ?v=<first 10 hex of
its SHA-256>, so a browser never pairs a new page with an old script it kept (GitHub Pages lets browsers
keep files 10 minutes). Run after changing anything in docs/; it only rewrites the ?v= parts."""
import hashlib, pathlib, re

docs = pathlib.Path (__file__).resolve().parent.parent / "docs"
ref = re.compile (r'((?:src|href)=")([^":?#]+\.(?:css|js))(?:\?v=[0-9a-f]+)?(")')

def stamp (m, base):
    f = (base / m.group (2)).resolve()
    if not f.is_file():
        return m.group (0)
    v = hashlib.sha256 (f.read_bytes()).hexdigest()[:10]
    return f'{m.group (1)}{m.group (2)}?v={v}{m.group (3)}'

for page in docs.glob ("*.html"):
    text = page.read_text()
    new = ref.sub (lambda m: stamp (m, page.parent), text)
    if new != text:
        page.write_text (new)
        print ("stamped", page.name)
