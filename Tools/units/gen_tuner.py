#!/usr/bin/env python3
"""RACK TUNER's bank: what every newer unit and LUNCHBOX module does, and what each of their knobs does, in
twelve plain tags - WARM BRIGHT DEEP PUNCH LOUD SPACE WIDE DIRTY SMOOTH VINTAGE MOTION CLARITY. No AI: a
unit's tags are written here by hand; a knob's come from its name (and its unit's) by the rules below.
Writes Source/UI/TunerBank.inc (UI/RackTuner.cpp builds its 10,000 settings from it at start-up).

Run after Tools/units/gen_units.py whenever a unit or a knob is added."""

import os, re, sys
sys.path.insert (0, os.path.dirname (os.path.abspath (__file__)))
import gen_units as G

TAGS = ["WARM", "BRIGHT", "DEEP", "PUNCH", "LOUD", "SPACE", "WIDE", "DIRTY", "SMOOTH", "VINTAGE", "MOTION", "CLARITY"]
def V (**kw):
    return [float (kw.get (t.lower(), 0.0)) for t in TAGS]

# What each unit brings as it is turned up (its MIX / AMOUNT scales this). Missing: not tuned (a check, a
# visualiser, the tuner itself).
UNIT = {
    "shimmer": V (space = 1.0, bright = 0.4, motion = 0.3, smooth = 0.3), "plate": V (space = 0.8, bright = 0.3, smooth = 0.2),
    "spring": V (space = 0.6, vintage = 0.6, motion = 0.2), "grain": V (motion = 1.0, space = 0.5),
    "tape": V (warm = 0.8, vintage = 0.8, smooth = 0.4, dirty = 0.2), "opto": V (smooth = 0.8, loud = 0.4, warm = 0.3, vintage = 0.3),
    "varimu": V (smooth = 0.7, warm = 0.6, loud = 0.5, vintage = 0.4), "dyneq": V (clarity = 0.7, smooth = 0.4),
    "shuffler": V (wide = 1.0, clarity = 0.2), "rotator": V (loud = 0.4, clarity = 0.3, smooth = 0.2),
    "bode": V (motion = 1.0, wide = 0.3), "harm": V (motion = 0.7, wide = 0.6, space = 0.2), "bbd": V (wide = 0.7, motion = 0.6, vintage = 0.4, warm = 0.2),
    "vocoder": V (motion = 1.0, dirty = 0.3), "submaxx": V (deep = 1.0, loud = 0.2), "clip": V (loud = 0.9, punch = 0.4, dirty = 0.5),
    "deharsh": V (smooth = 1.0, bright = -0.3, clarity = 0.2), "maximizer": V (clarity = 0.9, bright = 0.5, punch = 0.4, deep = 0.3),
    "rayroom": V (space = 0.8, vintage = 0.3), "vinyl": V (vintage = 1.0, warm = 0.6, bright = -0.3, motion = 0.2), "rotary": V (motion = 1.0, wide = 0.5, vintage = 0.5, warm = 0.3),
    "cassette": V (vintage = 1.0, warm = 0.7, bright = -0.4, dirty = 0.3, motion = 0.2), "tapeecho": V (space = 0.7, vintage = 0.8, warm = 0.4, motion = 0.3),
    "valveamp": V (warm = 0.9, dirty = 0.7, vintage = 0.5), "speakercab": V (warm = 0.5, vintage = 0.5, bright = -0.5, dirty = 0.2),
    "radio": V (vintage = 1.0, bright = -0.4, deep = -0.6, dirty = 0.3), "pendulum": V (wide = 0.8, motion = 0.9),
    "bounce": V (space = 0.7, motion = 0.6, wide = 0.4), "sympathy": V (space = 0.5, bright = 0.3, motion = 0.3, smooth = 0.2),
    "flyby": V (motion = 1.0, wide = 0.6, space = 0.3), "tesla": V (dirty = 1.0, bright = 0.4, motion = 0.4), "talkbox": V (motion = 0.9, dirty = 0.2),
    "lavalamp": V (motion = 0.9, warm = 0.3, smooth = 0.3),
    "clarity": V (clarity = 1.0, bright = 0.4), "detail": V (clarity = 1.0, space = 0.3, bright = 0.3, wide = 0.2), "subdriver": V (deep = 1.0, punch = 0.4, loud = 0.2), "lathe": V (vintage = 0.6, deep = -0.2, clarity = 0.2, wide = -0.3),
    "pressure": V (loud = 1.0, punch = 0.3), "balance": V (clarity = 0.5, smooth = 0.4), "field": V (wide = 1.0, clarity = 0.2),
    "sonar": V (clarity = 0.6, smooth = 0.3), "seismo": V (punch = 1.0, clarity = 0.3), "prism": V (warm = 0.3, bright = 0.3, clarity = 0.4),
    "furnace": V (warm = 1.0, dirty = 0.4, deep = 0.2), "dither": V (smooth = 0.1), "rider": V (smooth = 0.8, loud = 0.3),
    "compass": V (clarity = 0.6, deep = 0.3, punch = 0.2), "suspension": V (smooth = 0.8, deep = 0.2), "skyline": V (smooth = 0.6, clarity = 0.4, bright = -0.2),
    "hourglass": V (loud = 1.0, punch = 0.2), "aurora": V (bright = 1.0, clarity = 0.5, smooth = 0.2),
    "chroma": V (space = 1.0, motion = 0.6, wide = 0.5, bright = 0.2),
    # the LUNCHBOX's modules
    "pre": V (warm = 0.6, dirty = 0.3, loud = 0.2), "filter": V (smooth = 0.2, clarity = 0.2), "eq550": V (clarity = 0.4, punch = 0.2),
    "tubeeq": V (warm = 0.6, vintage = 0.5, deep = 0.3, bright = 0.3), "tilt": V (bright = 0.3), "air": V (bright = 1.0, clarity = 0.4),
    "loud": V (deep = 0.6, warm = 0.3), "deess": V (smooth = 0.8, bright = -0.3), "trans": V (punch = 1.0, clarity = 0.3),
    "gate": V (punch = 0.4, clarity = 0.4, space = -0.4), "comp": V (loud = 0.7, punch = 0.4, smooth = 0.3), "sat": V (warm = 0.8, dirty = 0.5, vintage = 0.3),
    "width": V (wide = 0.9), "limit": V (loud = 1.0),
}
# Every parameter's type and range, read from the plugin's own tables (ParameterSpecs.cpp, DesignedUnits.h)
def read_specs ():
    root = os.path.dirname (os.path.dirname (os.path.dirname (os.path.abspath (__file__))))
    specs = {}
    txt = open (os.path.join (root, "Source", "Parameters", "ParameterSpecs.cpp")).read ()
    ids = dict (re.findall (r'inline constexpr const char\* *(\w+) *= *"(\w+)"', open (os.path.join (root, "Source", "Parameters", "ParameterSpecs.h")).read ()))
    for m in re.finditer (r'\{ id::(\w+), *"[^"]*", *"[^"]*", *"[^"]*", *Kind::(\w+), *([-\d.]+)f, *([-\d.]+)f, *([-\d.]+)f', txt):
        specs[ids.get (m.group (1), m.group (1))] = ({"continuous": 0, "toggle": 1, "choice": 2}[m.group (2)], float (m.group (3)), float (m.group (4)), float (m.group (5)))
    des = open (os.path.join (root, "Source", "DSP", "DesignedUnits.h")).read ()
    for m in re.finditer (r'\{ "(\w+)", "[^"]*", "[^"]*", "[^"]*", ([012]), ([-\d.]+)f, ([-\d.]+)f, ([-\d.]+)f', des):
        specs[m.group (1)] = (int (m.group (2)), float (m.group (3)), float (m.group (4)), float (m.group (5)))
    return specs
SPECS = read_specs ()

# The rack's own units (the heart of the sound): the knobs the tuner may move; their ranges come from
# Parameters/ParameterSpecs (flag 4). POWER: the switch that takes the unit in and out ("" = never switched).
CORE = [
    ("enhancer", "ADAPTIVE ENHANCER EQ", "", ["clarityNorm", "clarityAdd", "clarityMode", "sub", "subBoost", "enhStrength"], V (clarity = 0.6, bright = 0.3, deep = 0.3)),
    ("tone", "TONE & SPACE FINISHER", "", ["seraphMode", "silkSmooth", "silkAir", "silkWarmth", "silkBody", "silkSub", "silkTape", "silkAuto", "haloWidth", "haloSpace", "haloDecay", "haloShimmer", "haloTone",
                                  "haloDuck", "haloBassMono", "haloMod", "seraphStrength", "heavenLift", "heavenHold", "heavenAuto", "heavenAutoAmount"], V (warm = 0.3, bright = 0.3, space = 0.3, wide = 0.3)),
    ("compressor", "ADAPTIVE COMPRESSOR", "tideActive", ["tideMix"], V (loud = 0.5, smooth = 0.4)),
    ("leveler", "UPWARD COMPRESSOR", "lumenActive", ["lumenTarget"], V (loud = 0.4, clarity = 0.3)),
    ("limiter", "SPECTRAL LIMITER", "", ["spectralRange"], V (smooth = 0.4)),
    ("balancer", "MULTIBAND BALANCER", "balActive", ["balAmount", "balTilt"], V (smooth = 0.4, clarity = 0.3)),
    ("deepsub", "SUB-HARMONIC SYNTHESIZER", "deepActive", ["deepDepth", "deepHull", "deepSize", "deepPressure"], V (deep = 1.0, punch = 0.3)),
    ("character", "CONSOLE & TAPE EMULATOR", "charActive", ["charModelA", "charModelB", "charBlend", "charDrive", "charColour", "charGrit"], V (warm = 0.6, dirty = 0.5, vintage = 0.4)),
    ("radar", "FOOTSTEP ENHANCER", "footstep", ["radarSens", "radarBoost", "radarSpace", "radarReach"], V (clarity = 0.7, punch = 0.3)),
    # the designed units (they start in the locker)
    ("x4", "SMART TUBE ENHANCER EQ", "x4Pwr", ["x4Mono", "x4X2", "x4Pid", "x4P", "x4I", "x4D", "x4Populate", "x4Saturate", "x4Widen", "x4Crisp"]
        + [f"x4{s}{b}{k}" for s in "LR" for b in "1234" for k in ("Drive", "Tone", "Mix")], V (warm = 0.4, dirty = 0.4, bright = 0.3, clarity = 0.3)),
    ("velvet", "SMOOTHING SATURATOR", "velPower", ["velMode", "velLow", "velMid", "velHigh", "velGrain", "velCrisp", "velColorA", "velBalance", "velColorB"], V (smooth = 0.8, warm = 0.3)),
    ("takeback", "DYNAMICS RESTORER", "tbPower", ["tbAuto", "tbBlur", "tbSharpen", "tbColor", "tbRaw", "tbShine", "tbMix"], V (punch = 0.6, clarity = 0.5)),
]
# the LUNCHBOX's own first modules (its locker numbers 0 - 2)
LB_OWN = [
    ("classaeq", "CLASS-A EQ", 0, "lbEqIn", ["lbHpf", "lbLowFreq", "lbLowGain", "lbMidFreq", "lbMidGain", "lbMidHiQ", "lbHighGain", "lbIron"], V (warm = 0.4, clarity = 0.3)),
    ("lbharsh", "DE-HARSH (500)", 1, "lbHarshIn", ["lbHarshAmount", "lbHarshFreq", "lbHarshSpeed"], V (smooth = 0.8, bright = -0.2)),
    ("crossfeed", "HEADPHONE CROSSFEED", 2, "lbFeedIn", ["lbFeedAmount"], V (wide = -0.6, smooth = 0.3)),
]
CORE_GUESS = {   # (what each core knob does, turned up - only until the AudioLab has measured it)
    "clarityNorm": V (clarity = 0.8, bright = 0.3), "sub": V (deep = 0.9), "enhStrength": V (clarity = 0.5, bright = 0.3, deep = 0.3),
    "silkSmooth": V (smooth = 0.8, bright = -0.3), "silkAir": V (bright = 0.9), "silkWarmth": V (warm = 0.9), "silkBody": V (warm = 0.5, deep = 0.4),
    "silkSub": V (deep = 0.8), "haloWidth": V (wide = 0.9), "haloSpace": V (space = 0.9), "haloDecay": V (space = 0.7), "haloShimmer": V (bright = 0.4, space = 0.4, motion = 0.3),
    "haloTone": V (bright = 0.5), "seraphStrength": V (warm = 0.3, bright = 0.3, space = 0.3), "heavenLift": V (clarity = 0.5, loud = 0.3),
    "tideMix": V (loud = 0.6, smooth = 0.4, punch = -0.2), "lumenTarget": V (loud = 0.5, clarity = 0.3), "spectralRange": V (smooth = 0.5),
    "balAmount": V (smooth = 0.5, clarity = 0.3), "balTilt": V (bright = 0.6, deep = -0.4),
    "deepDepth": V (deep = 1.0), "deepHull": V (deep = 0.6, space = 0.3), "deepSize": V (deep = 0.5, space = 0.3), "deepPressure": V (deep = 0.7, punch = 0.4),
    "charBlend": V (warm = 0.4, vintage = 0.3), "charDrive": V (dirty = 0.8, warm = 0.4), "charColour": V (warm = 0.6, vintage = 0.5),
}   # (the rest: no guess - the AudioLab measures them)

# Not tuned: a playback check (it would stay on the master), a visualiser, the tuner itself
SKIP = { "cartest", "phonecheck", "club", "hypercube", "tuner" }

def knob_tags (unit, p):
    """What turning this knob up does, in tags (and its flags: 1 never changed, 2 its unit's wet amount)."""
    L = p["label"].upper(); key = unit["key"]; base = UNIT.get (key, V())
    if p["kind"] == 1 and any (w in L for w in ("LISTEN", "PHASE", "M/S")): return V(), 1
    if any (w in L for w in ("MIX", "WET")) or (L in ("AMOUNT", "PROCESS", "AIR", "DEPTH", "PEAK REDUCTION", "SPACE", "HARMONICS") and p["kind"] == 0): return base, 2
    if any (w in L for w in ("HISS", "CRACKLE", "POP", "NOISE", "DUST")): return V (vintage = 0.8, dirty = 0.3, clarity = -0.3), 0
    if any (w in L for w in ("DRIVE", "SATURATION", "INPUT", "GRIT", "HEAT", "FIRE", "OVERDRIVE")): return V (dirty = 0.6, warm = 0.4, loud = 0.2, vintage = 0.2), 0
    if any (w in L for w in ("DECAY", "SIZE", "DWELL", "PRE-DELAY", "TIME", "ROOM", "TAIL")): return V (space = 0.8), 0
    if "FEEDBACK" in L: return V (space = 0.5, motion = 0.4), 0
    if any (w in L for w in ("BASS MONO", "LOW WIDTH")): return V (wide = -0.3, deep = 0.2, clarity = 0.2), 0
    if any (w in L for w in ("WIDTH", "SPREAD", "SIDE", "STEREO")): return V (wide = 0.9), 0
    if any (w in L for w in ("DAMP", "SMOOTH", "SOFT")): return V (bright = -0.4, smooth = 0.6), 0
    if any (w in L for w in ("TONE", "SHEEN", "HIGH", "PRESENCE", "BRIGHT", "TREBLE", "HF", "TILT", "LOW-PASS")): return V (bright = 0.7, clarity = 0.3), 0
    if any (w in L for w in ("LOW CUT", "HIGH-PASS")): return V (deep = -0.6, clarity = 0.3), 0
    if any (w in L for w in ("SUB", "BASS", "LOW", "LO CONTOUR", "WEIGHT", "BOOM")): return V (deep = 0.8, warm = 0.2), 0
    if any (w in L for w in ("THRESH", "CEILING")): return V (loud = -0.5, smooth = -0.3), 0   # (lower = more)
    if any (w in L for w in ("RATIO", "RANGE", "MAKE-UP", "GAIN", "OUTPUT", "LEVEL", "PUSH", "LOUD")): return V (loud = 0.5, smooth = 0.2), 0
    if "ATTACK" in L: return V (punch = 0.9), 0
    if "SUSTAIN" in L: return V (smooth = 0.3, space = 0.2, punch = -0.2), 0
    if "RELEASE" in L or "RECOVERY" in L: return V (smooth = 0.4), 0
    if any (w in L for w in ("WOW", "FLUTTER", "WEAR", "AGE")): return V (vintage = 0.8, motion = 0.4), 0
    if any (w in L for w in ("MOD", "RATE", "SPEED", "DETUNE", "SHIFT", "SPIN", "CHOP", "VARIETY", "SWING", "DEPTH")): return V (motion = 0.8), 0
    return V(), 0

def cf (x): return f"{x:.3f}f"

def main ():
    rows, units = [], []
    def add (u, key, name, gen_index, lb_index, power, params):
        first = len (rows)
        for p in params:
            tags, flags = knob_tags (dict (key = key), p)
            if p["kind"] == 1 and p["id"].endswith (("Power", "In")): continue
            if p["id"].endswith (("Multiply", "Strength")): continue   # (MULTIPLY and STRENGTH stay where you set them)
            if key == "chroma" and p["kind"] == 1: flags |= 0   # (its AUTO switches may change)
            rows.append (f'        {{ {G.cstr (p["id"])}, {p["kind"]}, {cf (p["lo"])}, {cf (p["hi"])}, {cf (p["d"])}, {{ {", ".join (cf (t) for t in tags)} }}, {flags} }},')
        units.append (f'        {{ {G.cstr (key)}, {G.cstr (name)}, {gen_index}, {lb_index}, {G.cstr (power)}, {{ {", ".join (cf (t) for t in UNIT[key])} }}, {first}, {len (rows) - first} }},')
    for i, u in enumerate (G.UNITS):
        if u["key"] in SKIP or u["key"] not in UNIT: continue
        add (u, u["key"], u["name"], i, -1, u["params"][0]["id"][:2] + "Power", u["params"])
    def spec_rows (ids):
        for pid in ids:
            if pid not in SPECS: raise SystemExit ("no spec for " + pid)
            kind, lo, hi, d = SPECS[pid]
            fixed = 1 if any (w in pid.lower () for w in ("listen", "bypass", "phase")) else 0
            rows.append (f'        {{ {G.cstr (pid)}, {kind}, {cf (lo)}, {cf (hi)}, {cf (d)}, {{ {", ".join (cf (t) for t in CORE_GUESS.get (pid, V ()))} }}, {4 | fixed} }},')
    for key, name, power, ids, base in CORE:
        first = len (rows)
        spec_rows (ids)
        UNIT[key] = base
        units.append (f'        {{ {G.cstr (key)}, {G.cstr (name)}, -1, -1, {G.cstr (power)}, {{ {", ".join (cf (t) for t in base)} }}, {first}, {len (rows) - first} }},')
    for key, name, lb, power, ids, base in LB_OWN:
        first = len (rows)
        spec_rows (ids)
        UNIT[key] = base
        units.append (f'        {{ {G.cstr (key)}, {G.cstr (name)}, -1, {lb}, {G.cstr (power)}, {{ {", ".join (cf (t) for t in base)} }}, {first}, {len (rows) - first} }},')
    for i, m in enumerate (G.LB_MODULES):
        if m["key"] not in UNIT: continue
        add (m, m["key"], m["name"], -1, 4 + i, m["params"][0]["id"][:2] + "In", m["params"])
    out = ["// GENERATED by Tools/units/gen_tuner.py - RACK TUNER's bank: each unit's tags and each knob's", "",
           "namespace pad::tuner::bank", "{",
           f"    inline constexpr int numTags = {len (TAGS)};",
           "    inline constexpr const char* tagNames[numTags] { " + ", ".join (G.cstr (t) for t in TAGS) + " };",
           "    /** A knob: kind 0 knob, 1 switch, 2 choice; tags: what turning it up does (a first guess: TunerMeasured.inc has what the AudioLab heard);",
           "        flags: 1 never changed, 2 its unit's wet amount, 4 its range from ParameterSpecs (the rack's own units). */",
           "    struct Param { const char* id; int kind; float lo, hi, d; float tags[numTags]; int flags; };",
           "    /** A unit: genIndex (newer units) or lbModule (the LUNCHBOX's, its locker number); base: what it brings, turned up. */",
           "    struct Unit { const char* key; const char* name; int genIndex, lbModule; const char* power; float base[numTags]; int firstParam, numParams; };   // (genIndex and lbModule both -1: one of the rack's own units, by key)",
           f"    inline constexpr int numParams = {len (rows)}, numUnits = {len (units)};",
           "    inline constexpr Param params[numParams] {"] + rows + ["    };", "    inline constexpr Unit units[numUnits] {"] + units + ["    };", "}", ""]
    path = os.path.join (os.path.dirname (os.path.dirname (os.path.dirname (os.path.abspath (__file__)))), "Source", "Tuner", "TunerBank.inc")
    open (path, "w").write ("\n".join (out))
    print (len (units), "units,", len (rows), "knobs ->", path)

if __name__ == "__main__":
    main()
