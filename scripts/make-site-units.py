#!/usr/bin/env python3
"""The website's units gallery (docs/units.html): its data and media, made from the plugin itself.

  1. build/site-units/ - made by the plugin and the AudioLab (run them first):
       PAD_UI_EXPORT_SITE=build/site-units "build/EnhMaster_artefacts/Release/Standalone/ENH Master"
       build/EnhAudioLab_artefacts/Release/EnhAudioLab demos --out build/site-units/audio
  2. scripts/make-site-units.py - turns them into docs/units/ (faceplates and screens as WebP, sound as MP3)
     and docs/units-data.js (every unit: name, model, what it does, its categories - the gear locker's own,
     from Source/UI/GlassPanel.cpp - and a guide to each knob, from its page in Vault/Devices).

Only what changed is converted again. Nothing on the site is fetched from anywhere else."""

import json, os, re, subprocess, sys

ROOT = os.path.dirname (os.path.dirname (os.path.abspath (__file__)))
SRC = os.path.join (ROOT, "build", "site-units")
OUT = os.path.join (ROOT, "docs", "units")
sys.path.insert (0, os.path.join (ROOT, "Tools", "units"))
import gen_units   # (the newer units' specs; importing it writes nothing)

# The rack's own units (not in gen_units): the name, the page in Vault/Devices, the height in U
ORIGINAL = [
    ("enhancer", "ADAPTIVE ENHANCER EQ", "ADAPTIVE ENHANCER", 2), ("tonespace", "TONE & SPACE FINISHER", "TONE and SPACE", 3),
    ("compressor", "ADAPTIVE COMPRESSOR", "ADAPTIVE COMPRESSOR", 1), ("leveler", "UPWARD COMPRESSOR", "UPWARD LEVELER", 1),
    ("limiter", "SPECTRAL LIMITER", "SPECTRAL LIMITER", 1), ("level", "LEVEL CONTROL", "LEVEL CONTROL and OUTPUT MONITOR", 1),
    ("monitor", "LOUDNESS MONITOR", "LEVEL CONTROL and OUTPUT MONITOR", 3), ("balancer", "MULTIBAND BALANCER", "MIX BALANCER", 1),
    ("deepsub", "SUB-HARMONIC SYNTHESIZER", "DEEP SUB", 1), ("character", "CONSOLE & TAPE EMULATOR", "CHARACTER", 1), ("radar", "FOOTSTEP ENHANCER", "FOOTSTEP RADAR", 1),
    ("x4", "SMART TUBE ENHANCER EQ", "LATINSPHIEL PRO X4", 4), ("velvet", "SMOOTHING SATURATOR", "VELVETIZER", 2), ("takeback", "DYNAMICS RESTORER", "TAKEBACK", 2),
    ("scope", "OSCILLOSCOPE", "PHOSPHOR SCOPE", 2), ("lunchbox", "500-SERIES RACK", "LUNCHBOX", 3),
]

def categories ():
    """The gear locker's categories, read from its table in GlassPanel.cpp (one source for both)."""
    src = open (os.path.join (ROOT, "Source", "UI", "GlassPanel.cpp"), encoding = "utf-8").read ()
    table = src[src.index ("static const std::vector<LockerGroup> table {") + len ("static const std::vector<LockerGroup> table "): src.index ("return table;")]
    # by brace depth: a group's name at depth 2, a section's at 4, its units' keys at 5
    groups, depth, i = [], 0, 0
    while i < len (table):
        c = table[i]
        if c == "{": depth += 1
        elif c == "}": depth -= 1
        elif c == '"':
            j = table.index ('"', i + 1); word = table[i + 1:j]; i = j
            if depth == 2: groups.append ({ "name": word, "subs": [] })
            elif depth == 4: groups[-1]["subs"].append ({ "name": word, "keys": [] })
            elif depth == 5: groups[-1]["subs"][-1]["keys"].append (word)
        i += 1
    groups.append ({ "name": "Always in the rack", "subs": [{ "name": "The frame", "keys": ["monitor", "lunchbox"] }] })
    return groups

def docOf (page):
    """A unit's page: its first sentence, and each knob's line (- **KNOB**: what it does)."""
    path = os.path.join (ROOT, "Vault", "Devices", page + ".md")
    if not os.path.exists (path): return "", []
    text = open (path, encoding = "utf-8").read ()
    # the first paragraph that says what it is: not the heading, the search link, a list, or where it starts
    paras = [" ".join (l.strip () for l in p.split ("\n")) for p in text.split ("\n\n")]
    intro = ""
    for p in paras:
        if not p or p[0] in "#>-|" or p.startswith ("Code:") or "Gear locker" in p or "POWER" in p: continue
        intro = p; break
    intro = re.sub (r"\[([^\]]+)\]\([^)]*\)", r"\1", intro).replace ("**", "").replace ("`", "")
    if len (intro) > 260: intro = intro[:intro.rfind (". ", 0, 260) + 1] or intro[:257] + "..."
    knobs = []
    for m in re.finditer (r"^- \*\*([^*]+)\*\*:?\s*(.+)$", text, re.M):
        line = re.sub (r"\[([^\]]+)\]\([^)]*\)", r"\1", m.group (2)).replace ("**", "").replace ("`", "")
        knobs.append ({ "k": m.group (1).strip (), "d": line.strip () })
    return intro, knobs[:14]

def fresh (src, dst):
    return os.path.exists (dst) and os.path.getmtime (dst) >= os.path.getmtime (src)

def run (cmd):
    subprocess.run (cmd, check = True, stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL)

def main ():
    if not os.path.isdir (SRC):
        sys.exit ("no build/site-units: run the plugin with PAD_UI_EXPORT_SITE and EnhAudioLab demos first (see the top of this script)")
    for d in ("face", "screen", "audio"): os.makedirs (os.path.join (OUT, d), exist_ok = True)
    units = []
    rows = [(k, n, page, u, None) for k, n, page, u in ORIGINAL]
    about = {   # (the rack's own units: a line each - their pages describe the panel, not what they are for)
        "enhancer": "Brings out what the mix hides: an adaptive EQ that follows the sound, CLARITY for detail, SUB for weight - the heart of the rack.",
        "tonespace": "Tone and space for the whole rack: silk, warmth and air, and a room around the sound that sits it together.",
        "compressor": "An adaptive compressor: loud moments come down a little and the rest stays put, so everything sits steadier.",
        "leveler": "Lifts quiet sounds - dialogue, distant footsteps, soft passages - in three bands, without pumping the loud ones.",
        "limiter": "Takes a huge bass hit down in the bass alone, so the detail over it doesn't duck.",
        "level": "The rack's working level, first in the chain: set it and everything after hears it.",
        "monitor": "The output: loudness, peaks and a spectrum, and the headphone settings.",
        "balancer": "Keeps the mix in balance moment to moment, riding six bands gently where one jumps out.",
        "deepsub": "Low end you feel: a sub an octave under the bass, and a hull that rings on after it.",
        "character": "Six analogue colours - tape, tube, transformer and more - with drive given back so the level holds.",
        "radar": "Finds footsteps in game audio and lifts them - near, far, under music or gunfire.",
        "x4": "A smart tube enhancer: four bands of harmonics, held on target by a PID loop.",
        "velvet": "Smooths the harsh edge off a mix, band by band, as far as it needs to.",
        "takeback": "Takes back transients a mix lost: attack shaping by band, with its own AUTO.",
        "scope": "A green-phosphor CRT scope on the rack's output: X-Y, M/S and waveform.",
        "lunchbox": "A 500-series frame of ten slots beside the rack, with its own locker of modules.",
        "tuner": "Type what you want - \"warm punchy hip-hop\" - and it tunes the whole rack from 60,000 settings, a new pick every time. No AI, nothing online; level-matched, with A/B and UNDO." }
    rows += [(u["key"], u["name"], u["name"], u["u"], u) for u in gen_units.UNITS]
    for key, name, page, height, spec in rows:
        face = os.path.join (SRC, "faces", key + ".png")
        if not os.path.exists (face): continue
        intro, knobs = docOf (page)
        e = { "key": key, "name": name, "u": height, "intro": about.get (key, intro), "knobs": knobs }
        if spec is not None:
            e.update ({ "model": spec["model"], "sub": spec["sub"].title (), "role": spec["role"].capitalize () })
        # the faceplate: 960 px across (a card shows it smaller; pointing at it shows it bigger)
        dst = os.path.join (OUT, "face", key + ".webp")
        if not fresh (face, dst): run (["magick", face, "-resize", "960x", "-quality", "82", dst])
        e["face"] = "units/face/" + key + ".webp"
        # its screen, animated (a still first frame too, shown until the card is in view)
        frames = os.path.join (SRC, "screens", key)
        if os.path.isdir (frames):
            anim, still = os.path.join (OUT, "screen", key + ".webp"), os.path.join (OUT, "screen", key + "-still.webp")
            first = os.path.join (frames, "f00.png")
            if not fresh (first, anim):
                run (["ffmpeg", "-y", "-framerate", "30", "-i", os.path.join (frames, "f%02d.png"), "-vf", "scale=384:-2", "-loop", "0",
                      "-c:v", "libwebp_anim", "-lossless", "0", "-quality", "60", "-compression_level", "5", anim])
                run (["magick", os.path.join (frames, "f24.png"), "-resize", "384x", "-quality", "70", still])
            e["screen"], e["still"] = "units/screen/" + key + ".webp", "units/screen/" + key + "-still.webp"
        # its sound: the same clip before and after (the unit's own DSP, from the AudioLab)
        wav = os.path.join (SRC, "audio", key + ".wav")
        if os.path.exists (wav):
            dst = os.path.join (OUT, "audio", key + ".mp3")
            if not fresh (wav, dst): run (["lame", "--quiet", "-b", "80", wav, dst])
            e["audio"] = "units/audio/" + key + ".mp3"
        units.append (e)
    dry = os.path.join (SRC, "audio", "_dry.wav")
    if os.path.exists (dry) and not fresh (dry, os.path.join (OUT, "audio", "_dry.mp3")):
        run (["lame", "--quiet", "-b", "80", dry, os.path.join (OUT, "audio", "_dry.mp3")])
    data = { "categories": categories (), "units": units, "dry": "units/audio/_dry.mp3" }
    with open (os.path.join (ROOT, "docs", "units-data.js"), "w", encoding = "utf-8") as f:
        f.write ("/* ENH Master site: every unit, for units.html. Generated by scripts/make-site-units.py - do not edit. */\n")
        f.write ("window.ENHGALLERY = " + json.dumps (data, ensure_ascii = False, separators = (",", ":")) + ";\n")
    size = sum (os.path.getsize (os.path.join (dp, fn)) for dp, _, fs in os.walk (OUT) for fn in fs)
    print (f"{len (units)} units, {sum (1 for u in units if 'screen' in u)} screens, {sum (1 for u in units if 'audio' in u)} sound demos; docs/units/ {size / 1e6:.1f} MB")

if __name__ == "__main__":
    main ()
