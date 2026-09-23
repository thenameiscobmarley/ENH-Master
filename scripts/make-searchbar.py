#!/usr/bin/env python3
"""Writes Searchbar.md: one long page listing every doc heading, unit, preset, parameter, glass-panel
setting, test mode, dev setting, class and function in the code, and the GitHub pages, each a link.
Open it on GitHub (or in any Markdown viewer) and press Ctrl+F.

Also writes Vault/Reference/Parameters.md from the parameter list, and keeps the Searchbar link at the
top of every doc. Run it after changing docs or code:  python3 scripts/make-searchbar.py
"""

import os, re, subprocess, sys, urllib.parse
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "https://github.com/thenameiscobmarley/ENH-Master"
HWK = "https://github.com/thenameiscobmarley/HardwareKit"
JUCE = "https://github.com/juce-framework/JUCE"
NAV = "> 🔎 **[Searchbar]({})** — find any doc, setting, function or GitHub page (Ctrl+F)"
SKIP_DIRS = {"build", ".git", "dist", "node_modules", ".obsidian"}
GENERATED = {"Searchbar.md", "Vault/Reference/Methods.md"}   # written by tools, which add the link themselves


def rel(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def url(path, line=None, anchor=None):
    """A link from the repository root (where Searchbar.md lives)."""
    u = urllib.parse.quote(path)
    if line:
        u += f"#L{line}"
    elif anchor:
        u += "#" + anchor
    return u


def slug(heading, seen):
    """GitHub's heading anchors: lower case, punctuation dropped, spaces to hyphens, -1 / -2 for repeats."""
    s = heading.strip().lower()
    s = re.sub(r"[^\w\- ]", "", s, flags=re.UNICODE).replace(" ", "-")
    n = seen[s]
    seen[s] += 1
    return s if n == 0 else f"{s}-{n}"


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read().split("\n")


def files(exts, dirs):
    out = []
    for d in dirs:
        for base, subdirs, names in os.walk(os.path.join(ROOT, d)):
            subdirs[:] = sorted(s for s in subdirs if s not in SKIP_DIRS)
            out += [os.path.join(base, n) for n in sorted(names) if n.endswith(exts)]
    return out


def clean(text, n=110):
    text = re.sub(r"\s+", " ", text.replace("|", "/")).strip()
    return text if len(text) <= n else text[: n - 1].rstrip() + "…"


def entry(name, kind, link, where, what=""):
    what = f" — {clean(what)}" if what else ""
    return f"- **{name}** · {kind} · [{where}]({link}){what}"


# --------------------------------------------------------------------------------------------------
def docs():
    lines = []
    for path in files((".md",), ["."]):
        r = rel(path)
        if r in ("Searchbar.md",):
            continue
        text = read(path)
        title = next((l[2:].strip() for l in text if l.startswith("# ")), os.path.basename(r))
        lines.append(entry(title, "doc", url(r), r))
        seen = defaultdict(int)
        in_code = False
        for l in text:
            if l.startswith("```"):
                in_code = not in_code
            if in_code:
                continue
            m = re.match(r"^(#{1,4})\s+(.*)", l)
            if m and len(m.group(1)) > 1:
                h = m.group(2).strip()
                lines.append(entry(h, "doc section", url(r, anchor=slug(h, seen)), f"{r} › {h}", f"in {title}"))
            elif m:
                slug(m.group(2), seen)
    return lines


def units():
    u = [
        ("LEVEL CONTROL", "", "Devices/LEVEL CONTROL and OUTPUT MONITOR.md", "EnhEngine.cpp", "how loud the rack runs"),
        ("ADAPTIVE ENHANCER", "ENH MASTER unit, enhancer, black unit", "Devices/ADAPTIVE ENHANCER.md", "AdaptiveEQ.cpp", "clarity, sub, footsteps"),
        ("UPWARD LEVELER", "LUMEN, leveler", "Devices/UPWARD LEVELER.md", "SpectralLeveler.cpp", "lifts quiet sounds"),
        ("DEEP SUB", "sub, hull, submarine", "Devices/DEEP SUB.md", "DeepSub.h", "octave-down sub and a ringing hull"),
        ("SPECTRAL LIMITER", "anti-pumping, dynamic EQ", "Devices/SPECTRAL LIMITER.md", "SpectralLimiter.cpp", "takes bangs down where they are"),
        ("MIX BALANCER", "balancer, band faders", "Devices/MIX BALANCER.md", "MixBalancer.h", "keeps the mix in balance"),
        ("ADAPTIVE COMPRESSOR", "TIDE, compressor", "Devices/ADAPTIVE COMPRESSOR.md", "DynamicCompressor.cpp", "evens out the level"),
        ("TONE & SPACE", "SERAPH, SILK, HALO, HEAVEN, reverb, width", "Devices/TONE and SPACE.md", "Seraph.cpp", "polish, air, width, room"),
        ("CHARACTER", "console, tape, valve, saturation, GRIT", "Devices/CHARACTER.md", "Character.h", "the sound of studio hardware"),
        ("OUTPUT MONITOR", "loudness meter, DUCK, COMPARE, A/B", "Devices/LEVEL CONTROL and OUTPUT MONITOR.md", "LoudnessMeter.h", "shows what the rack did"),
        ("Router app", "standalone, gamers, INSERT RACK, VB-CABLE, tray", "Tutorial/07 The router app.md", None, "puts the rack between games and your headset"),
    ]
    out = []
    for name, aka, doc, code, what in u:
        out.append(entry(name, "unit" + (f" (also: {aka})" if aka else ""), url("Vault/" + doc), doc, what))
        if code:
            p = "Source/DSP/" + code
            out.append(entry(name, "unit code", url(p), p, what))
    out.append(entry("Router app", "code", url("Source/Standalone/AudioRouting.cpp"), "Source/Standalone/", "the router: AudioRouting*, RouterBar, StandaloneApp"))
    return out


def presets():
    p = "Source/Parameters/FactoryPresets.h"
    text = read(os.path.join(ROOT, p))
    out = []
    for i, l in enumerate(text):
        m = re.match(r'\s*\{\s*"([A-Z][^"]+)",\s*(?:"([^"]*)")?', l)
        if m and ("{" in l):
            desc = m.group(2) or (re.match(r'\s*"([^"]*)"', text[i + 1]).group(1) if i + 1 < len(text) and re.match(r'\s*"', text[i + 1]) else "")
            out.append(entry(m.group(1), "preset", url(p, i + 1), f"FactoryPresets.h:{i + 1}", desc))
    out.append(entry("Presets", "doc", url("Vault/Reference/Presets.md"), "Presets.md", "what each preset is for, and editing presets.json"))
    return out


def parameter_specs():
    ids = {}
    for l in read(os.path.join(ROOT, "Source/Parameters/ParameterSpecs.h")):
        m = re.search(r'constexpr const char\*\s+(\w+)\s*=\s*"([^"]+)"', l)
        if m:
            ids[m.group(1)] = m.group(2)
    p = "Source/Parameters/ParameterSpecs.cpp"
    specs = []
    for i, l in enumerate(read(os.path.join(ROOT, p))):
        m = re.match(r'\s*\{\s*id::(\w+),\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*Kind::(\w+),\s*([-\d.]+)f,\s*([-\d.]+)f,\s*([-\d.]+)f(.*)', l)
        if m:
            choices = re.findall(r'"([^"]+)"', m.group(9))
            specs.append(dict(id=ids.get(m.group(1), m.group(1)), name=m.group(2), panel=m.group(3), unit=m.group(4).strip(),
                              kind=m.group(5), lo=m.group(6), hi=m.group(7), default=m.group(8), choices=choices, line=i + 1))
    return p, specs


def fmt_num(v):
    f = float(v)
    return str(int(f)) if f == int(f) else f"{f:g}"


def parameters(specs_path, specs):
    out = []
    for s in specs:
        rng = " / ".join(s["choices"]) if s["choices"] else f"{fmt_num(s['lo'])} – {fmt_num(s['hi'])}{(' ' + s['unit']) if s['unit'] else ''}"
        out.append(entry(f"`{s['id']}`", f"parameter ({s['panel']}, \"{s['name']}\")", url(specs_path, s["line"]), f"ParameterSpecs.cpp:{s['line']}", rng))
    return out


def write_parameters_doc(specs_path, specs):
    rows = ["# Parameters", "", NAV.format("../../Searchbar.md"), "",
            "Every knob and switch your DAW can automate. The **id** is what sessions and presets store; some",
            "still use the units' old names (lumen = leveler, tide = compressor, seraph / silk / halo = tone & space).",
            "Glass-panel settings are listed in [Methods](Methods.md).", "",
            f"*Made from [`{specs_path}`](../../{url(specs_path)}) by `scripts/make-searchbar.py` — don't edit by hand.*", "",
            "| id | On the panel | Name in your DAW | Range | Default |", "|---|---|---|---|---|"]
    for s in specs:
        rng = " / ".join(s["choices"]) if s["choices"] else f"{fmt_num(s['lo'])} – {fmt_num(s['hi'])}{(' ' + s['unit'].strip()) if s['unit'] else ''}"
        d = s["choices"][int(float(s["default"]))] if s["choices"] and int(float(s["default"])) < len(s["choices"]) else fmt_num(s["default"])
        if s["kind"] == "toggle" and not s["choices"]:
            rng, d = "off / on", ("on" if float(s["default"]) > 0.5 else "off")
        rows.append(f"| [`{s['id']}`](../../{url(specs_path, s['line'])}) | {s['panel']} | {s['name']} | {rng} | {d} |")
    with open(os.path.join(ROOT, "Vault/Reference/Parameters.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(rows) + "\n")


def methods():
    p = "Source/DSP/MethodRegistry.h"
    text = read(os.path.join(ROOT, p))
    arrays, current = {}, None
    for i, l in enumerate(text):
        m = re.search(r"std::array<Method,\s*\d+>\s+(\w+)\s*\{", l)
        if m:
            current = m.group(1)
            arrays[current] = []
            continue
        m = re.match(r'\s*\{\s*"([A-Z0-9]{2,3})",\s*"([^"]+)"', l)
        if current and m:
            arrays[current].append((m.group(1), m.group(2), i + 1))
        if current and l.strip().startswith("}};"):
            current = None
    out = []
    for i, l in enumerate(text):
        m = re.match(r'\s*\{\s*"([A-Z &]+)",\s*\d+,\s*"(\w+)",\s*"([^"]+)",\s*"([^"]+)",\s*"(\w*)",\s*\w+,\s*"(\w*)",\s*(\w+)\.data\(\)', l)
        if not m:
            continue
        unit, cat, name, question, param, knob, arr = m.groups()
        choices = ", ".join(f"{s} {f}" for s, f, _ in arrays.get(arr, []))
        out.append(entry(f"{unit}: {name}", f"glass-panel setting ({cat}{', ' + knob + ' knob' if knob else ''}; parameter `{param}`)",
                         url(p, i + 1), f"MethodRegistry.h:{i + 1}", f"{question}: {choices}"))
        for s, f, line in arrays.get(arr, []):
            out.append(entry(f"{s} — {f}", f"method of {unit} {name}", url(p, line), f"MethodRegistry.h:{line}"))
    out.append(entry("Every glass-panel setting, explained", "doc", url("Vault/Reference/Methods.md"), "Methods.md"))
    return out


def tools():
    out = []
    t = "Tests/EnhDspTests.cpp"
    for i, l in enumerate(read(os.path.join(ROOT, t))):
        for flag in re.findall(r'== "(--[a-z0-9-]+)"', l):
            out.append(entry(f"EnhDspTests {flag}", "test mode", url(t, i + 1), f"EnhDspTests.cpp:{i + 1}"))
    a = "Tools/AudioLab.cpp"
    for i, l in enumerate(read(os.path.join(ROOT, a))):
        for cmd in re.findall(r'command == "(\w+)"', l):
            out.append(entry(f"EnhAudioLab {cmd}", "tool command", url(a, i + 1), f"AudioLab.cpp:{i + 1}"))
        m = re.match(r'\s*\{\s*"(\w+)",\s*"([^"]+)"\s*\},', l)
        if m:
            out.append(entry(f"scene {m.group(1)}", "AudioLab test scene", url(a, i + 1), f"AudioLab.cpp:{i + 1}", m.group(2)))
    s = "scripts/selftest.sh"
    for i, l in enumerate(read(os.path.join(ROOT, s))):
        m = re.match(r"#\s+(--[a-z-]+)\s+(.*)", l)
        if m:
            out.append(entry(f"selftest.sh {m.group(1)}", "self-test option", url(s, i + 1), f"selftest.sh:{i + 1}", m.group(2)))
    out.insert(0, entry("scripts/selftest.sh", "the whole self-test in one command", url(s), s, "every test, PASS / FAIL per step"))
    b = "build.sh"
    if os.path.exists(os.path.join(ROOT, b)):
        out.append(entry("build.sh", "the builder", url(b), b, "checks tools, fetches JUCE and HardwareKit, builds"))
        for i, l in enumerate(read(os.path.join(ROOT, b))):
            m = re.match(r"\s*(--[a-z-]+)\)", l)
            if m:
                out.append(entry(f"build.sh {m.group(1)}", "builder option", url(b, i + 1), f"build.sh:{i + 1}"))
    out.append(entry("scripts/make-searchbar.py", "writes this page", url("scripts/make-searchbar.py"), "make-searchbar.py"))
    out.append(entry("convert-to-windows.bat", "Windows: download or build any release", url("convert-to-windows.bat"), "convert-to-windows.bat"))
    out.append(entry("Tests/lab-baseline.json", "the audio check's stored results", url("Tests/lab-baseline.json"), "lab-baseline.json", "EnhAudioLab check --update rewrites it"))
    return out


def dev_settings():
    seen = {}
    for path in files((".cpp", ".h", ".sh"), ["Source", "Tests", "Tools", "scripts"]):
        for i, l in enumerate(read(path)):
            for var in re.findall(r'(?:getenv|getEnvironmentVariable)\s*\(\s*"([A-Z0-9_]+)"', l) + re.findall(r'\b(PAD_UI_[A-Z0-9_]+|ENH_[A-Z0-9_]+)=', l):
                seen.setdefault(var, []).append((rel(path), i + 1))
    out = []
    for var in sorted(seen):
        for p, line in seen[var][:3]:
            out.append(entry(var, "dev setting (environment variable)", url(p, line), f"{p}:{line}"))
    return out


KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "catch", "else", "case", "new", "delete", "throw",
            "defined", "static_assert", "decltype", "alignof", "noexcept", "operator", "requires"}
FUNC = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?((?:[\w:<>,\*&~]+\s+)+?)[\*&]*((?:\w+::)*~?\w+)\s*\(")


def comment_before(text, i):
    j = i - 1
    while j >= 0 and text[j].strip() == "":
        j -= 1
    block = []
    while j >= 0 and re.match(r"\s*(//|/\*|\*)", text[j]):
        block.insert(0, text[j])
        j -= 1
    if not block:
        return ""
    s = " ".join(re.sub(r"^\s*(///?|/\*\*?|\*/?)\s?", "", b).replace("*/", "") for b in block)
    s = s.strip()
    m = re.match(r"(.+?[.;])(\s|$)", s)
    return m.group(1) if m else s


def code():
    out = []
    for path in files((".h", ".cpp", ".hpp"), ["Source", "Tests", "Tools"]):
        r = rel(path)
        text = read(path)
        items = []
        owners = []   # (indent, class name): the classes a line is inside, to name methods Class::method
        in_comment = False
        for i, l in enumerate(text):
            s = l.strip()
            if in_comment:
                in_comment = "*/" not in s
                continue
            if "/*" in s and "*/" not in s[s.index("/*"):]:
                in_comment = True
                if s.startswith("/*"):
                    continue
            if s.startswith(("//", "*", "/*", "#", ":", ",")):
                continue
            indent = len(l) - len(l.lstrip())
            if s.startswith("};"):
                owners = [o for o in owners if o[0] < indent]
                continue
            m = re.match(r"^\s*(?:template\s*<[^>]*>\s*)?(class|struct|enum class|enum)\s+(?:JUCE_API\s+)?(\w+)", l)
            if m and not s.endswith(";") and m.group(2) not in KEYWORDS:
                owners = [o for o in owners if o[0] < indent]
                owner = "::".join(o[1] for o in owners)
                items.append(((owner + "::" if owner else "") + m.group(2), m.group(1), i + 1, comment_before(text, i)))
                if not m.group(1).startswith("enum"):
                    owners.append((indent, m.group(2)))
                continue
            m = FUNC.match(l)
            if not m:
                continue
            ret, name = m.group(1).strip(), m.group(2)
            inside = [o[1] for o in owners if o[0] < indent]
            if inside and "::" not in name:
                name = "::".join(inside) + "::" + name
            base = name.split("::")[-1]
            if base in KEYWORDS or ret.split()[0] in KEYWORDS | {"else", "return", "co_return", "goto", "using", "typedef"} or base.isupper() or "=" in l.split("(")[0]:
                continue
            # a definition: the statement reaches '{' before ';'
            stmt = ""
            for k in range(i, min(i + 6, len(text))):
                stmt += text[k]
                if "{" in stmt or ";" in stmt:
                    break
            if "{" not in stmt or (";" in stmt and stmt.index(";") < stmt.index("{")):
                continue
            items.append((name, "function", i + 1, comment_before(text, i)))
        if items:
            out.append(f"\n**[{r}]({url(r)})**\n")
            for name, kind, line, doc in items:
                out.append(entry(f"{name}", kind, url(r, line), f"{os.path.basename(r)}:{line}", doc))
    return out


def all_files():
    out = []
    for path in files((".h", ".cpp", ".py", ".sh", ".bat", ".ps1", ".yml", ".txt", ".json", ".glsl", ".frag", ".vert"),
                      ["Source", "Tests", "Tools", "scripts", ".github"]):
        r = rel(path)
        out.append(entry(os.path.basename(r), "file", url(r), r))
    for top in ("CMakeLists.txt", "build.sh", "convert-to-windows.bat", "LICENSE", "NOTICE"):
        if os.path.exists(os.path.join(ROOT, top)):
            out.append(entry(top, "file", url(top), top))
    return out


def github():
    tags = subprocess.run(["git", "tag", "--sort=-creatordate"], cwd=ROOT, capture_output=True, text=True).stdout.split()
    workflows = sorted(os.listdir(os.path.join(ROOT, ".github/workflows"))) if os.path.isdir(os.path.join(ROOT, ".github/workflows")) else []
    L = [
        entry("ENH Master on GitHub", "GitHub: the repository home page", REPO, "github.com/thenameiscobmarley/ENH-Master"),
        entry("Latest release / download", "GitHub: releases (Windows zip for gamers, Linux zip, VST3)", REPO + "/releases/latest", "releases/latest"),
        entry("All releases", "GitHub: every version with its downloads and notes", REPO + "/releases", "releases"),
        entry("Tags", "GitHub: every version tag", REPO + "/tags", "tags"),
        entry("Issues / report a bug", "GitHub: issues", REPO + "/issues", "issues"),
        entry("New issue / ask for a feature", "GitHub: open an issue", REPO + "/issues/new", "issues/new"),
        entry("Pull requests", "GitHub: pull requests", REPO + "/pulls", "pulls"),
        entry("Actions / CI / builds", "GitHub: automatic builds and tests", REPO + "/actions", "actions"),
        entry("Commits / history", "GitHub: every change", REPO + "/commits", "commits"),
        entry("Branches", "GitHub: branches", REPO + "/branches", "branches"),
        entry("Download convert-to-windows.bat", "GitHub: raw file download", REPO + "/raw/main/convert-to-windows.bat", "raw/main/convert-to-windows.bat"),
        entry("Download the source as a zip", "GitHub: archive", REPO + "/archive/refs/heads/main.zip", "archive/main.zip"),
        entry("CHANGELOG", "what changed in each version", url("CHANGELOG.md"), "CHANGELOG.md"),
        entry("HardwareKit", "GitHub: the 3D hardware UI library (knobs, loupe, materials)", HWK, "github.com/thenameiscobmarley/HardwareKit"),
        entry("HardwareKit source", "GitHub: its module folder", HWK + "/tree/main/modules/hardwarekit", "HardwareKit/modules/hardwarekit"),
        entry("HardwareKit commits", "GitHub: its history", HWK + "/commits", "HardwareKit/commits"),
        entry("HardwareKit releases", "GitHub", HWK + "/releases", "HardwareKit/releases"),
        entry("JUCE", "GitHub: the audio framework ENH Master is built on", JUCE, "github.com/juce-framework/JUCE"),
        entry("JUCE docs", "the framework's API reference", "https://docs.juce.com", "docs.juce.com"),
        entry("VB-CABLE", "free virtual cable for the Windows router", "https://vb-audio.com/Cable/", "vb-audio.com/Cable"),
        entry("Carla", "the plugin host it's tested in (Linux)", "https://kx.studio/Applications:Carla", "kx.studio Carla"),
    ]
    for sub in ("gfx", "geo", "models", "anim", "shaders", "fx", "input"):
        L.append(entry(f"HardwareKit {sub}/", "GitHub: HardwareKit folder", f"{HWK}/tree/main/modules/hardwarekit/{sub}", f"hardwarekit/{sub}"))
    for w in workflows:
        L.append(entry(f"workflow {w}", "GitHub Actions: its runs", f"{REPO}/actions/workflows/{w}", f"actions/workflows/{w}"))
        L.append(entry(f"workflow {w}", "its source", url(f".github/workflows/{w}"), f".github/workflows/{w}"))
    for t in tags:
        if t.startswith("backup/"):
            continue
        L.append(entry(f"release {t}", "GitHub: that version's page and downloads", f"{REPO}/releases/tag/{t}", f"releases/tag/{t}"))
        L.append(entry(f"source at {t}", "GitHub: the code as it was in that version", f"{REPO}/tree/{t}", f"tree/{t}"))
    return L


def add_nav():
    """The Searchbar link under the title of every doc (idempotent)."""
    changed = 0
    for path in files((".md",), ["."]):
        r = rel(path)
        if r in GENERATED or r == "Vault/Reference/Parameters.md":
            continue
        text = read(path)
        link = os.path.relpath(os.path.join(ROOT, "Searchbar.md"), os.path.dirname(path)).replace(os.sep, "/")
        nav = NAV.format(urllib.parse.quote(link))
        body = [l for l in text if not l.startswith("> 🔎 **[Searchbar]")]
        # drop a blank line left behind where the old link was
        out, i = [], 0
        h = next((k for k, l in enumerate(body) if l.startswith("# ")), None)
        if h is None:
            out = [nav, ""] + body
        else:
            rest = body[h + 1:]
            while rest and rest[0].strip() == "":
                rest.pop(0)
            out = body[:h + 1] + ["", nav, ""] + rest
        if out != text:
            with open(path, "w", encoding="utf-8") as f:
                f.write("\n".join(out))
            changed += 1
    return changed


def main():
    specs_path, specs = parameter_specs()
    write_parameters_doc(specs_path, specs)
    changed = add_nav()
    sections = [
        ("GitHub pages", "Releases, downloads, issues, builds, HardwareKit, and every version.", github()),
        ("Units", "The ten units of the rack (and their old names), with their doc and their code.", units()),
        ("Presets", "Every factory preset, with its description.", presets()),
        ("Docs", "Every doc and every section in it.", docs()),
        ("Parameters", "Every automatable knob and switch: its id, panel name and range. Table: [Parameters](Vault/Reference/Parameters.md).", parameters(specs_path, specs)),
        ("Glass-panel settings", "Every setting in the units' glass panels, and every choice.", methods()),
        ("Tests and tools", "Test modes, tool commands, scenes, scripts.", tools()),
        ("Dev settings", "Every environment variable the code reads, and where.", dev_settings()),
        ("Code", "Every class, struct, enum and function, by file, with the first line of its comment.", code()),
        ("Files", "Every source, script and workflow file.", all_files()),
    ]
    head = ["# Searchbar", "",
            "**Press Ctrl+F** (⌘F on a Mac) and type what you're looking for — a knob, a unit, a preset, a setting,",
            "a function, an error word, *release*, *download*, *HardwareKit*… Every line is a link.", "",
            "Each line reads: **name** · what it is · where it is — what it does.", "",
            "Jump to: " + " · ".join(f"[{t}](#{slug(t, defaultdict(int))})" for t, _, _ in sections), "",
            f"*Made by [`scripts/make-searchbar.py`]({url('scripts/make-searchbar.py')}) — run it again after changing docs or code.*", ""]
    body = []
    total = 0
    for title, intro, lines in sections:
        n = sum(1 for l in lines if l.startswith("- "))
        total += n
        body += [f"## {title}", "", intro + f" ({n})", ""] + lines + [""]
    with open(os.path.join(ROOT, "Searchbar.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(head + body))
    print(f"Searchbar.md: {total} entries; Parameters.md: {len(specs)} parameters; search link added or updated in {changed} doc(s)")


if __name__ == "__main__":
    main()
