#!/usr/bin/env python3
"""The rack's newer units, from one table: each unit's parameters, panel and place in the rack.

Writes (never edit those by hand - edit UNITS below and run this):
  Source/DSP/units/UnitParams.inc     rows for the parameter table (DesignedUnits.h)
  Source/DSP/units/UnitList.h         counts, where each unit's parameters start, its name
  Source/UI/Scene/UnitPanels.h        each unit's print (the designed-unit Print format), look and colours
  Source/UI/Scene/UnitControls.inc    its knobs, selectors and switches (DeviceLayout controls)
  Source/UI/Scene/UnitIds.inc         the unit numbers (DeviceLayout enum)

Standard layouts: 1U - knobs in one row, an LED ladder, the switches at the right; 2U - a live display strip
on top, knobs in one or two rows under it, the ladder and switches at the right. Positions are panel units
(x across -2.5 .. 2.5, z down; 1U is 0.59 tall).
"""
import pathlib, json

ROOT = pathlib.Path(__file__).resolve().parents[2]
FIRST_PARAM = 62          # the rows before these (PRO X4, VELVETIZER, TAKEBACK, PHOSPHOR)
FIRST_UNIT = 17           # the unit numbers before these

# kind: 0 knob (min, max, default, decimals, unit), 1 toggle, 2 choice ("A|B|C", default index)
def K (id, label, lo, hi, d, dec = 1, unit = ""): return dict (id = id, label = label, kind = 0, lo = lo, hi = hi, d = d, dec = dec, unit = unit)
def T (id, label, d = 0, texts = ""): return dict (id = id, label = label, kind = 1, lo = 0, hi = 1, d = d, dec = 0, unit = "", texts = texts)
def C (id, label, texts, d = 0): return dict (id = id, label = label, kind = 2, lo = 0, hi = len (texts.split ("|")) - 1, d = d, dec = 0, unit = "", texts = texts)

# MULTIPLY and STRENGTH (3.8.0.1): this release's new units that change the sound get both, last on their panel -
# MULTIPLY scales their amount knobs, STRENGTH how much they change the sound (the base unit applies them: RackUnit.h)
MODIFIED = { "clarity", "subdriver", "lathe", "cartest", "phonecheck", "club", "pressure", "balance", "field", "sonar", "seismo",
             "prism", "furnace", "dither", "rider", "compass", "suspension", "skyline", "hourglass", "aurora", "chroma", "detail", "voice", "pitchfix", "vocalstation" }

UNITS = [
  dict (key = "shimmer", name = "SHIMMER REVERB", model = "SH-8", sub = "FDN REVERB - OCTAVE SHIMMER", u = 2, colour = (0.010, 0.020, 0.060), glow = (0.55, 0.75, 1.0), knob = "machinedBlack", role = "8-LINE REVERB - PITCHED SHIMMER",
        params = [K ("shSize", "SIZE", 0, 10, 6), K ("shDecay", "DECAY", 0.3, 20, 4, 1, " s"), K ("shShimmer", "SHIMMER", 0, 100, 30, 0, "%"), C ("shPitch", "PITCH", "+12|+7|+19|-12"),
                  K ("shDamp", "DAMP", 0, 10, 5), K ("shMod", "MOD", 0, 10, 4), K ("shMix", "MIX", 0, 100, 30, 0, "%")]),
  dict (key = "plate", name = "PLATE REVERB", model = "EMT-STYLE", sub = "DATTORRO PLATE REVERB", u = 1, colour = (0.30, 0.30, 0.29), glow = (1.0, 0.8, 0.5), knob = "chickenHeadKnob", role = "PLATE REVERB",
        params = [K ("plDecay", "DECAY", 0.2, 8, 2.2, 1, " s"), K ("plPre", "PRE-DELAY", 0, 120, 15, 0, " ms"), K ("plDamp", "DAMPING", 0, 10, 4), K ("plSize", "SIZE", 0, 10, 6), K ("plMix", "MIX", 0, 100, 25, 0, "%")]),
  dict (key = "spring", name = "SPRING REVERB", model = "ST-3", sub = "DISPERSIVE SPRING REVERB", u = 1, colour = (0.12, 0.03, 0.02), glow = (1.0, 0.6, 0.3), knob = "bakelitePointer", role = "SPRING REVERB",
        params = [K ("spDwell", "DWELL", 0, 10, 5), K ("spDecay", "DECAY", 0, 10, 5), K ("spTone", "TONE", 0, 10, 5), C ("spSprings", "SPRINGS", "1|2|3", 2), K ("spMix", "MIX", 0, 100, 25, 0, "%")]),
  dict (key = "grain", name = "GRANULAR DELAY", model = "GC-1", sub = "GRANULAR DELAY", u = 2, colour = (0.05, 0.015, 0.06), glow = (0.9, 0.5, 1.0), knob = "colletBlack", role = "GRANULAR DELAY - PITCH SPRAY",
        params = [K ("grSize", "SIZE", 10, 500, 120, 0, " ms"), K ("grDensity", "DENSITY", 1, 40, 12, 0, "/s"), K ("grPitch", "PITCH", -12, 12, 0, 0, " st"), K ("grSpray", "SPRAY", 0, 100, 30, 0, "%"),
                  K ("grReverse", "REVERSE", 0, 100, 20, 0, "%"), K ("grFeedback", "FEEDBACK", 0, 90, 30, 0, "%"), K ("grMix", "MIX", 0, 100, 35, 0, "%")]),
  dict (key = "tape", name = "TAPE MACHINE", model = "ATR-102", sub = "HYSTERESIS TAPE MACHINE", u = 2, colour = (0.18, 0.19, 0.20), glow = (1.0, 0.75, 0.35), knob = "machinedSilver", role = "TAPE - HYSTERESIS - WOW & FLUTTER",
        params = [K ("atInput", "INPUT", -12, 18, 0, 1, " dB"), K ("atSat", "SATURATION", 0, 10, 5), C ("atSpeed", "SPEED", "7.5|15|30", 1), K ("atWow", "WOW", 0, 10, 2), K ("atFlutter", "FLUTTER", 0, 10, 2),
                  K ("atHiss", "HISS", 0, 10, 0), K ("atOutput", "OUTPUT", -12, 12, 0, 1, " dB")]),
  dict (key = "opto", name = "OPTICAL COMPRESSOR", model = "LA-T4", sub = "OPTICAL LEVELING AMPLIFIER", u = 1, colour = (0.55, 0.55, 0.53), glow = (1.0, 0.85, 0.5), knob = "la2aFluted", role = "OPTICAL COMPRESSOR",
        params = [K ("t4Peak", "PEAK REDUCTION", 0, 100, 40, 0), K ("t4Gain", "GAIN", 0, 30, 6, 1, " dB"), T ("t4Limit", "LIMIT", 0, "Compress|Limit"), K ("t4Emph", "EMPHASIS", 0, 10, 3), K ("t4Mix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "varimu", name = "TUBE COMPRESSOR", model = "VM-2", sub = "VARIABLE-MU TUBE COMPRESSOR", u = 2, colour = (0.08, 0.10, 0.12), glow = (1.0, 0.7, 0.3), knob = "pultecTopHat", role = "TUBE COMPRESSOR - GAIN-RIDING RATIO",
        params = [K ("vmInput", "INPUT", 0, 10, 5), K ("vmThresh", "THRESHOLD", -40, 0, -18, 1, " dB"), C ("vmRecovery", "RECOVERY", "0.1|0.3|0.6|1.2|2.0|AUTO", 5), K ("vmGain", "GAIN", 0, 20, 4, 1, " dB"),
                  T ("vmMs", "M/S"), K ("vmMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "dyneq", name = "4-BAND DYNAMIC EQ", model = "DQ-4", sub = "FOUR DYNAMIC BANDS", u = 2, colour = (0.015, 0.03, 0.05), glow = (0.4, 0.9, 1.0), knob = "consoleBlue", role = "4-BAND DYNAMIC EQ",
        params = [K ("deF1", "FREQ 1", 40, 400, 100, 0, " Hz"), K ("deT1", "THRESH 1", -60, 0, -30, 0, " dB"), K ("deR1", "RANGE 1", -12, 12, -4, 1, " dB"),
                  K ("deF2", "FREQ 2", 200, 2000, 500, 0, " Hz"), K ("deT2", "THRESH 2", -60, 0, -30, 0, " dB"), K ("deR2", "RANGE 2", -12, 12, 0, 1, " dB"),
                  K ("deF3", "FREQ 3", 1000, 8000, 3000, 0, " Hz"), K ("deT3", "THRESH 3", -60, 0, -30, 0, " dB"), K ("deR3", "RANGE 3", -12, 12, -4, 1, " dB"),
                  K ("deF4", "FREQ 4", 4000, 16000, 8000, 0, " Hz"), K ("deT4", "THRESH 4", -60, 0, -30, 0, " dB"), K ("deR4", "RANGE 4", -12, 12, 0, 1, " dB")]),
  dict (key = "shuffler", name = "STEREO SHUFFLER", model = "SF-B", sub = "BLUMLEIN STEREO SHUFFLER", u = 1, colour = (0.02, 0.05, 0.03), glow = (0.5, 1.0, 0.6), knob = "rogan", role = "FREQUENCY-DEPENDENT WIDTH",
        params = [K ("sfWidth", "WIDTH", 0, 200, 120, 0, "%"), K ("sfFreq", "SHUFFLE", 200, 3000, 700, 0, " Hz"), K ("sfLowWidth", "LOW WIDTH", 0, 200, 100, 0, "%"), K ("sfMono", "BASS MONO", 20, 300, 120, 0, " Hz"),
                  K ("sfMid", "MID", -6, 6, 0, 1, " dB"), K ("sfSide", "SIDE", -6, 6, 0, 1, " dB")]),
  dict (key = "rotator", name = "PHASE ROTATOR", model = "PR-16", sub = "BROADCAST CREST REDUCER", u = 1, colour = (0.05, 0.05, 0.055), glow = (1.0, 0.4, 0.3), knob = "pointerBarBlack", role = "PHASE ROTATION - PEAKS DOWN, NO COMPRESSION",
        params = [C ("prStages", "STAGES", "4|8|16", 1), K ("prFreq", "FREQUENCY", 60, 2000, 250, 0, " Hz"), K ("prAmount", "AMOUNT", 0, 100, 100, 0, "%")]),
  dict (key = "bode", name = "FREQUENCY SHIFTER", model = "BS-1630", sub = "HILBERT FREQUENCY SHIFTER", u = 1, colour = (0.30, 0.22, 0.05), glow = (1.0, 0.9, 0.4), knob = "brassKnurl", role = "FREQUENCY SHIFTER - FEEDBACK",
        params = [K ("bsShift", "SHIFT", -500, 500, 5, 1, " Hz"), K ("bsFeedback", "FEEDBACK", 0, 90, 20, 0, "%"), K ("bsSpread", "SPREAD", 0, 50, 0, 1, " Hz"), K ("bsMix", "MIX", 0, 100, 50, 0, "%")]),
  dict (key = "harm", name = "PITCH SHIFTER", model = "H-910", sub = "TWO-VOICE PITCH SHIFTER", u = 2, colour = (0.10, 0.02, 0.02), glow = (1.0, 0.3, 0.25), knob = "consoleRed", role = "PITCH SHIFTER - TWO VOICES",
        params = [K ("hzV1", "VOICE 1", -12, 12, -12, 0, " st"), K ("hzV2", "VOICE 2", -12, 12, 7, 0, " st"), K ("hzDetune", "DETUNE", 0, 50, 10, 0, " ct"), K ("hzDelay", "DELAY", 0, 100, 20, 0, " ms"),
                  K ("hzFeedback", "FEEDBACK", 0, 80, 0, 0, "%"), K ("hzMix", "MIX", 0, 100, 35, 0, "%")]),
  dict (key = "bbd", name = "ANALOG CHORUS", model = "CE-3", sub = "BUCKET-BRIGADE CHORUS", u = 1, colour = (0.03, 0.09, 0.18), glow = (0.5, 0.8, 1.0), knob = "consoleWhite", role = "ANALOG CHORUS - ENSEMBLE",
        params = [K ("bbRate", "RATE", 0.1, 10, 0.6, 2, " Hz"), K ("bbDepth", "DEPTH", 0, 10, 5), C ("bbMode", "MODE", "CHORUS I|CHORUS II|ENSEMBLE", 2), K ("bbNoise", "NOISE", 0, 10, 2), K ("bbMix", "MIX", 0, 100, 50, 0, "%")]),
  dict (key = "vocoder", name = "VOCODER", model = "VX-16", sub = "16-BAND VOCODER", u = 2, colour = (0.02, 0.02, 0.02), glow = (0.4, 1.0, 0.8), knob = "hifiBlackDisc", role = "VOCODER - 16 BANDS",
        params = [K ("rvNote", "NOTE", 24, 72, 45, 0), C ("rvCarrier", "CARRIER", "SAW|PULSE|NOISE|CHORD", 0), K ("rvFormant", "FORMANT", -12, 12, 0, 0, " st"), K ("rvRelease", "RELEASE", 5, 200, 30, 0, " ms"),
                  K ("rvMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "submaxx", name = "PSYCHOACOUSTIC BASS", model = "SMX", sub = "PSYCHOACOUSTIC BASS", u = 1, colour = (0.10, 0.05, 0.0), glow = (1.0, 0.6, 0.2), knob = "machinedGunmetal", role = "BASS YOU HEAR ON SMALL SPEAKERS",
        params = [K ("smFreq", "FREQUENCY", 40, 150, 80, 0, " Hz"), K ("smHarm", "HARMONICS", 0, 10, 5), K ("smOrig", "ORIGINAL", -24, 0, -6, 1, " dB"), K ("smOut", "OUTPUT", -12, 6, 0, 1, " dB")]),
  dict (key = "clip", name = "OVERSAMPLED CLIPPER", model = "OC-4X", sub = "OVERSAMPLED CLIPPER", u = 1, colour = (0.15, 0.0, 0.0), glow = (1.0, 0.25, 0.2), knob = "porticoRed", role = "4X OVERSAMPLED CLIPPER",
        params = [K ("ocDrive", "DRIVE", 0, 24, 6, 1, " dB"), K ("ocCeiling", "CEILING", -12, 0, -1, 1, " dB"), K ("ocKnee", "KNEE", 0, 10, 4), C ("ocMode", "MODE", "SOFT|HARD|TUBE"), K ("ocMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "deharsh", name = "HARSHNESS SUPPRESSOR", model = "DH-4", sub = "DYNAMIC HARSH REMOVER", u = 1, colour = (0.20, 0.21, 0.23), glow = (0.6, 1.0, 0.85), knob = "consoleWhite", role = "EVENS OUT ADAPTIVE FLICKER - TAMES HARSHNESS",
        params = [K ("dhSmooth", "SMOOTH", 0, 10, 6), K ("dhDepth", "DEPTH", 0, 12, 4, 1, " dB"), K ("dhSense", "SENSE", 0, 10, 5), K ("dhFocus", "FOCUS", 2000, 8000, 3500, 0, " Hz"),
                  K ("dhAir", "AIR", 0, 10, 3), T ("dhListen", "LISTEN", 0, "Off|Listen"), K ("dhMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "maximizer", name = "AUDIO EXCITER & MAXIMIZER", model = "SONIC MAXIMIZER", sub = "PROFESSIONAL AUDIO EXCITER & SONIC MAXIMIZER", u = 1, colour = (0.018, 0.018, 0.022), glow = (0.35, 0.62, 1.0), knob = "hifiBlackDisc", role = "PHASE-ALIGNED EXCITER - LO CONTOUR - PROCESS",
        params = [K ("mxLo", "LO CONTOUR", 0, 10, 0), K ("mxProcess", "PROCESS", 0, 10, 0), K ("mxOut", "OUTPUT", -12, 12, 0, 1, " dB")]),
  dict (key = "rayroom", name = "ROOM SIMULATOR", model = "RT-3", sub = "RAY-TRACED ROOM - CRACKLE & POPS", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A ROOM AROUND THE SOUND - TAPE CRACKLE AND POPS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("rrSpace", "SPACE", 0, 10, 4), K ("rrDamp", "DAMP", 0, 10, 5), K ("rrCrackle", "CRACKLE", 0, 10, 0), K ("rrPop", "POPPING", 0, 10, 0),
                  K ("rrMix", "MIX", 0, 100, 30, 0, "%")]),
  dict (key = "vinyl", name = "TURNTABLE SIMULATOR", model = "TT-12", sub = "TURNTABLE SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "PLAYED FROM A RECORD - DUST, WOW, A WORN STYLUS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("vdSpeed", "SPEED", "33|45|78", 0), K ("vdWear", "WEAR", 0, 10, 2), K ("vdDust", "DUST", 0, 10, 3), K ("vdWow", "WOW", 0, 10, 2),
                  K ("vdAge", "AGE", 0, 10, 2), K ("vdMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "rotary", name = "ROTARY SPEAKER", model = "RC-147", sub = "ROTATING SPEAKER SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A HORN AND A DRUM TURNING - DOPPLER, SWIRL",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("rcSpeed", "SPEED", "SLOW|FAST", 0), K ("rcAccel", "ACCEL", 0, 10, 5), K ("rcDrive", "DRIVE", 0, 10, 2), K ("rcDist", "DISTANCE", 0, 10, 4),
                  K ("rcMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "cassette", name = "CASSETTE SIMULATOR", model = "CD-3", sub = "CASSETTE SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "RECORDED TO CASSETTE - WOW, FLUTTER, HISS, TAPE STOP",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("ccTape", "TAPE", "I|II|IV", 1), K ("ccWow", "WOW", 0, 10, 3), K ("ccFlutter", "FLUTTER", 0, 10, 3), K ("ccHiss", "HISS", 0, 10, 3),
                  K ("ccDrop", "DROPOUTS", 0, 10, 1), K ("ccSat", "SATURATE", 0, 10, 4), T ("ccStop", "STOP", 0, "Play|Stop")]),
  dict (key = "tapeecho", name = "TAPE ECHO", model = "TE-201", sub = "TAPE-LOOP ECHO SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A TAPE LOOP PAST THREE HEADS - RUNS AWAY PAST 100%",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("teTime", "TIME", 50, 800, 300, 0, " ms"), K ("teInt", "INTENSITY", 0, 110, 45, 0, "%"), C ("teHeads", "HEADS", "1|2|3|1+2|2+3|ALL", 0),
                  K ("teWow", "WOW", 0, 10, 3), K ("teTone", "TONE", -5, 5, 0), K ("teMix", "MIX", 0, 100, 30, 0, "%")]),
  dict (key = "valveamp", name = "VALVE AMP SIMULATOR", model = "VA-50", sub = "VALVE AMPLIFIER SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A VALVE PREAMP AND POWER STAGE - THE SUPPLY SAGS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("vaDrive", "DRIVE", 0, 10, 4), K ("vaBias", "BIAS", 0, 10, 5), K ("vaSag", "SAG", 0, 10, 4), K ("vaTone", "TONE", -5, 5, 0), K ("vaOut", "OUTPUT", -12, 12, 0, 1, " dB"), K ("vaMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "speakercab", name = "SPEAKER CABINET SIMULATOR", model = "SC-412", sub = "SPEAKER AND MIC SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A SPEAKER IN ITS CABINET AND THE MIC IN FRONT OF IT",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("skSize", "SIZE", "10|12|15", 1), K ("skMic", "MIC POS", 0, 10, 3), K ("skDist", "DISTANCE", 0, 10, 2), K ("skRoom", "ROOM", 0, 10, 2), K ("skMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "radio", name = "RADIO SIMULATOR", model = "AM-5", sub = "RADIO RECEIVER SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "ON OR OFF THE STATION - STATIC, FADING, A SMALL SPEAKER",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("raBand", "BAND", "AM|SW|FM", 0), K ("raTune", "TUNE", -10, 10, 0), K ("raStatic", "STATIC", 0, 10, 3), K ("raFade", "FADE", 0, 10, 2), K ("raSpeaker", "SPEAKER", 0, 10, 5), K ("raMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "pendulum", name = "PENDULUM MODULATOR", model = "PT-2", sub = "PENDULUM MODULATOR", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A REAL SWINGING PENDULUM MOVES THE SOUND - HITS PUSH IT",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("pdLength", "LENGTH", 10, 300, 100, 0, " cm"), K ("pdSwing", "SWING", 0, 10, 5), K ("pdFriction", "FRICTION", 0, 10, 2), C ("pdMode", "MODE", "VOLUME|PAN|FILTER", 1), K ("pdKick", "KICK", 0, 10, 5), K ("pdMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "bounce", name = "BOUNCING DELAY", model = "BD-1", sub = "BOUNCING-BALL ECHO", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "ECHOES AS A BALL BOUNCES - SOONER AND QUIETER",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("bdHeight", "HEIGHT", 50, 1000, 400, 0, " ms"), K ("bdBounce", "BOUNCE", 0.3, 0.95, 0.7, 2), K ("bdTone", "TONE", -5, 5, 0), K ("bdSpread", "SPREAD", 0, 10, 5), K ("bdMix", "MIX", 0, 100, 35, 0, "%")]),
  dict (key = "sympathy", name = "SYMPATHETIC RESONATOR", model = "SR-6", sub = "SYMPATHETIC STRINGS", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "SIX STRINGS RING ALONG WITH THE SOUND",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("syNote", "NOTE", 28, 64, 40, 0), C ("syChord", "CHORD", "OPEN|MAJOR|MINOR|FIFTHS|OCTAVES", 0), K ("syDecay", "DECAY", 0, 10, 6), K ("syBright", "BRIGHT", 0, 10, 5), K ("syMix", "MIX", 0, 100, 30, 0, "%")]),
  dict (key = "flyby", name = "DOPPLER FLYBY", model = "DP-9", sub = "DOPPLER FLYBY", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE SOUND MOVING PAST YOU - DOPPLER, DISTANCE, AIR",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("fySpeed", "SPEED", 5, 100, 30, 0, " m/s"), K ("fyDist", "DISTANCE", 1, 50, 8, 0, " m"), C ("fyPath", "PATH", "LINE|CIRCLE|EIGHT", 1), K ("fyAir", "AIR", 0, 10, 5), K ("fyMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "tesla", name = "TESLA COIL SYNTHESIZER", model = "TC-1M", sub = "SINGING TESLA COIL", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE SOUND PLAYED BY AN ARC - CRACKLE AND MAINS BUZZ",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("tcVolt", "VOLTAGE", 0, 10, 5), K ("tcBuzz", "BUZZ", 0, 10, 3), K ("tcArc", "ARC", 0, 10, 4), K ("tcTone", "TONE", -5, 5, 0), K ("tcMix", "MIX", 0, 100, 50, 0, "%")]),
  dict (key = "talkbox", name = "TALK BOX FORMANT FILTER", model = "TB-3", sub = "FORMANT MOUTH SIMULATION", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE SOUND THROUGH A MOUTH - A E I O U",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("tbVowel", "VOWEL", 0, 4, 0, 1), K ("tbMove", "MOVE", 0, 10, 5), K ("tbRate", "RATE", 0, 10, 0), K ("tbSize", "SIZE", -5, 5, 0), K ("tbMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "lavalamp", name = "RESONANT FILTER BANK", model = "LL-60", sub = "LAVA LAMP RESONATOR", u = 3, colour = (0.012, 0.012, 0.013), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "WAX BLOBS RISE AND SINK - EACH ONE A RESONANCE",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("lvHeat", "HEAT", 0, 10, 5), K ("lvBlobs", "BLOBS", 1, 6, 4, 0), K ("lvDepth", "DEPTH", 0, 10, 5), K ("lvCentre", "CENTRE", 200, 4000, 1000, 0, " Hz"), K ("lvMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "clarity", name = "CLARITY ENHANCER", model = "CL-1", sub = "MASTERING CLARITY SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "BRINGS FORWARD THE PRESENCE THE MIX HIDES",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("clClarity", "CLARITY", 0, 10, 4), K ("clFocus", "FOCUS", 1000, 6000, 3000, 0, " Hz"), K ("clAir", "AIR", 0, 10, 2), K ("clSafe", "SAFE", 0, 10, 5), K ("clMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "subdriver", name = "SUBWOOFER SIMULATOR", model = "SD-18", sub = "SUBWOOFER CONE SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A MODELLED SUB CONE - TIGHT, CONTROLLED LOW END",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("sdSub", "SUB", 0, 10, 4), K ("sdTune", "TUNE", 30, 80, 45, 0, " Hz"), K ("sdXmax", "XMAX", 0, 10, 6), K ("sdTight", "TIGHT", 0, 10, 5), K ("sdMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "lathe", name = "VINYL MASTERING SIMULATOR", model = "VC-70", sub = "CUTTING LATHE SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "WHAT A LATHE ALLOWS - MONO BASS, A TAMED TOP",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("lhMono", "MONO BASS", 50, 300, 150, 0, " Hz"), K ("lhHf", "HF LIMIT", 0, 10, 5), K ("lhDepth", "DEPTH", 0, 10, 3), K ("lhMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "cartest", name = "CAR LISTENING TEST", model = "CT-4", sub = "CAR LISTENING SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE MASTER IN A CAR - CABIN BOOM, DOOR SPEAKERS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("ctCabin", "CABIN", 0, 10, 5), C ("ctSpeakers", "SPEAKERS", "DOOR|DASH|PREMIUM", 0), K ("ctRoad", "ROAD", 0, 10, 2), C ("ctSeat", "SEAT", "DRIVER|MIDDLE|BACK", 0), K ("ctMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "phonecheck", name = "SMALL SPEAKER TEST", model = "PC-2", sub = "SMALL SPEAKER SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE MASTER ON A PHONE, A LAPTOP OR EARBUDS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("phDevice", "DEVICE", "PHONE|LAPTOP|EARBUDS", 0), K ("phVolume", "VOLUME", 0, 10, 6), T ("phMono", "MONO", 1, "Stereo|Mono"), K ("phMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "club", name = "CLUB SOUND SYSTEM TEST", model = "CS-8", sub = "CLUB PA SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE MASTER ON A CLUB PA - SUBS, ROOM, CROWD",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("cbSize", "SIZE", 0, 10, 5), K ("cbSub", "SUB", 0, 10, 5), K ("cbCrowd", "CROWD", 0, 10, 5), K ("cbDist", "DISTANCE", 0, 10, 4), K ("cbMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "pressure", name = "LOUDNESS MAXIMIZER", model = "PV-24", sub = "PRESSURE VESSEL MAXIMIZER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "LOUDNESS AS PRESSURE - THE VALVE VENTS THE PEAKS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("pvPressure", "PRESSURE", 0, 24, 6, 1, " dB"), K ("pvValve", "VALVE", -6, 0, -1, 1, " dB"), K ("pvRelease", "RELEASE", 10, 500, 100, 0, " ms"), K ("pvChar", "CHARACTER", 0, 10, 3), K ("pvMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "balance", name = "TONAL BALANCE TILT", model = "TB-1", sub = "TONAL SEE-SAW", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "LEVELS THE LOW END AGAINST THE TOP",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("blTilt", "TILT", -5, 5, 0), K ("blSpeed", "SPEED", 0, 10, 5), K ("blAmount", "AMOUNT", 0, 10, 5), K ("blMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "field", name = "STEREO WIDTH CONTROL", model = "SF-2", sub = "STEREO FIELD SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "WIDTH AS FIELD LINES - BASS KEPT MONO, MONO-SAFE",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("fdWidth", "WIDTH", 0, 200, 120, 0, "%"), K ("fdMono", "BASS MONO", 20, 300, 120, 0, " Hz"), K ("fdFocus", "FOCUS", 0, 10, 2), K ("fdSafe", "SAFE", 0, 10, 6), K ("fdMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "sonar", name = "TRANSIENT SHAPER", model = "SN-3", sub = "SONAR TRANSIENT SHAPER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "EACH HIT FOUND AS A PING - ATTACK AND SUSTAIN",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("soAttack", "ATTACK", -10, 10, 3), K ("soSustain", "SUSTAIN", -10, 10, 0), K ("soSense", "SENSE", 0, 10, 5), K ("soMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "seismo", name = "LOW-END DAMPER", model = "SG-1", sub = "LOW-END QUAKE DAMPER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "BASS QUAKES CAUGHT AND DAMPED",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("seThresh", "THRESHOLD", -40, 0, -18, 1, " dB"), K ("seDamp", "DAMPING", 0, 10, 5), K ("seFreq", "FREQ", 40, 200, 100, 0, " Hz"), K ("seMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "prism", name = "MULTIBAND SATURATOR", model = "PR-5", sub = "FIVE-COLOUR SATURATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "FIVE BANDS, EACH SATURATED IN ITS OWN COLOUR",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("pmDrive", "DRIVE", 0, 10, 4), K ("pmSpread", "SPREAD", 0, 10, 5), K ("pmWarmth", "WARMTH", 0, 10, 5), K ("pmShine", "SHINE", 0, 10, 5), K ("pmMix", "MIX", 0, 100, 50, 0, "%")]),
  dict (key = "furnace", name = "THERMAL SATURATOR", model = "FN-9", sub = "THERMAL SATURATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "WARMTH THAT BUILDS WITH HEAT",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("fuHeat", "HEAT", 0, 10, 4), K ("fuCool", "COOLING", 0, 10, 5), K ("fuColour", "COLOUR", 0, 10, 5), K ("fuMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "dither", name = "DITHER", model = "DQ-16", sub = "CONVERTER DITHER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "THE LAST BITS - 16, 20 OR 24, DITHERED",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("diBits", "BITS", "16|20|24", 0), C ("diMode", "DITHER", "OFF|TPDF|SHAPED", 1)]),
  dict (key = "rider", name = "AUTOMATIC FADER RIDER", model = "FR-1", sub = "FADER RIDER SIMULATION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A HAND ON THE FADER - RIDES TO A LOUDNESS",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("rdTarget", "TARGET", -24, -6, -14, 1, " LUFS"), K ("rdSpeed", "SPEED", 0, 10, 5), K ("rdRange", "RANGE", 0, 12, 6, 1, " dB"), K ("rdMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "compass", name = "STEREO PHASE ALIGNER", model = "PA-1", sub = "PHASE ALIGNMENT", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "LEFT AND RIGHT BROUGHT BACK INTO PHASE",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("cpAlign", "ALIGN", 0, 10, 8), K ("cpRange", "RANGE", 0.1, 2, 1, 1, " ms"), C ("cpBand", "BAND", "FULL|BASS", 1), K ("cpMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "suspension", name = "DYNAMICS SMOOTHER", model = "SU-2", sub = "DYNAMICS SUSPENSION", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "DYNAMICS SMOOTHED AS A CAR RIDES A ROAD",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("suSpring", "SPRING", 0, 10, 5), K ("suDamper", "DAMPER", 0, 10, 5), K ("suTravel", "TRAVEL", 0, 12, 6, 1, " dB"), K ("suMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "skyline", name = "RESONANCE SUPPRESSOR", model = "SK-16", sub = "RESONANCE SUPPRESSOR", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "RESONANT PEAKS TRIMMED BACK TO THE SKYLINE",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("slDepth", "DEPTH", 0, 10, 4), K ("slSharp", "SHARPNESS", 0, 10, 5), K ("slSpeed", "SPEED", 0, 10, 5), C ("slFocus", "FOCUS", "FULL|MIDS|HIGHS", 0), K ("slMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "hourglass", name = "PROGRAM-DEPENDENT LIMITER", model = "HG-1", sub = "PROGRAM-DEPENDENT LIMITER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "A LIMITER WHOSE RELEASE FOLLOWS THE SAND",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("hgCeiling", "CEILING", -6, 0, -0.3, 1, " dB"), K ("hgDrive", "DRIVE", 0, 12, 3, 1, " dB"), K ("hgRelease", "RELEASE", 10, 1000, 150, 0, " ms")]),
  dict (key = "aurora", name = "AIR BAND EXCITER", model = "AU-1", sub = "AIR BAND EXCITER", u = 3, colour = (0.016, 0.020, 0.030), glow = (1.0, 1.0, 1.0), knob = "hifiBlackDisc", role = "AIR OVER THE TOP - SHIMMER, NEVER HARSH",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("auAir", "AIR", 0, 10, 4), K ("auSheen", "SHEEN", 5000, 16000, 10000, 0, " Hz"), K ("auSmooth", "SMOOTH", 0, 10, 5), K ("auMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "chroma", name = "SPACE & TONE PROCESSOR", model = "CS-X", sub = "SPACE, TONE AND CHOPS", u = 3, colour = (0.010, 0.010, 0.014), glow = (1.0, 0.8, 0.6), knob = "hifiBlackDisc", role = "A MASTER OF SPACE AND TONE - CHOPS THAT FOLLOW THE MELODY",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("chSpace", "SPACE", 0, 10, 4), K ("chTone", "TONE", -10, 10, 0), K ("chWidth", "WIDTH", 0, 10, 6), K ("chChop", "CHOP", 0, 10, 0),
                  K ("chSize", "SIZE", 60, 600, 180, 0, " ms"), K ("chVariety", "VARIETY", 0, 10, 3), K ("chAutoAmt", "AUTO AMT", 0, 10, 5), K ("chMix", "MIX", 0, 100, 40, 0, "%"),
                  T ("chAutoTime", "AUTO TIMING", 1, "Off|On"), T ("chAutoTrick", "AUTO TRICKS", 1, "Off|On"), T ("chAutoLen", "AUTO LENGTH", 1, "Off|On"), T ("chAutoSpace", "AUTO SPACE", 0, "Off|On")]),
  dict (key = "hypercube", name = "MORPHING VISUALIZER", model = "HC-4D", sub = "VECTOR MORPHING VISUALISER", u = 3, colour = (0.008, 0.008, 0.010), glow = (0.75, 0.45, 1.0), knob = "hifiBlackDisc", role = "A NEON VECTOR CUBE THAT MORPHS WITH WHAT PLAYS - THE SOUND UNTOUCHED",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("hcMorph", "MORPH", 0, 10, 5), K ("hcReact", "REACT", 0, 10, 6), K ("hcPalette", "PALETTE", 0, 10, 0), K ("hcBeam", "BEAM", 0, 10, 5),
                  T ("hcAutoMorph", "AUTO MORPH", 1, "Off|On"), T ("hcAutoReact", "AUTO REACT", 1, "Off|On"), T ("hcAutoPalette", "AUTO PALETTE", 1, "Off|On"), T ("hcAutoBeam", "AUTO BEAM", 1, "Off|On")]),
  # RACK TUNER: tunes the whole rack from words (UI/RackTuner.h); its own sound is untouched - the rack's level
  # is matched at the end of the chain (DSP/TunerMatch.h). TUNE, UNDO and A/B sit under the knobs (buttons).
  dict (key = "tuner", name = "AUTOMATIC RACK TUNER", model = "RT-60K", sub = "TUNES THE WHOLE RACK FROM WORDS", u = 3, colour = (0.012, 0.011, 0.016), glow = (1.0, 0.85, 0.55), knob = "hifiBlackDisc", role = "60,000 SETTINGS - PICKS THE BEST FOR EVERY UNIT FROM WHAT YOU ASK",
        screen = (-1.02, 0.13, 2.40, 1.38), buttons = ["tnTune", "tnUndo", "tnAB"],
        params = [K ("tnVariety", "VARIETY", 0, 10, 3), K ("tnAmount", "AMOUNT", 0, 10, 7),
                  T ("tnKnobs", "CHANGE KNOBS", 1, "Off|On"), T ("tnOnOff", "UNITS ON/OFF", 1, "Off|On"), T ("tnSwap", "SWAP UNITS", 0, "Off|On"), T ("tnMatch", "LEVEL MATCH", 1, "Off|On"),
                  T ("tnTune", "TUNE", 0, "Off|Tune"), T ("tnUndo", "UNDO", 0, "Off|Undo"), T ("tnAB", "A/B", 0, "After|Before")]),
  dict (key = "detail", name = "SPECTRAL DETAIL ENHANCER", model = "SD-24", sub = "PSYCHOACOUSTIC MASKING SIMULATION", u = 3, colour = (0.010, 0.014, 0.022), glow = (0.75, 0.92, 1.0), knob = "hifiBlackDisc", role = "BRINGS OUT WHAT THE MIX HIDES - MASKED DETAIL, TAILS, AIR - WITH CLARITY",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [K ("dtDetail", "DETAIL", 0, 10, 5), K ("dtDepth", "DEPTH", 0, 24, 12, 0, " dB"), K ("dtClarity", "CLARITY", 0, 10, 4), K ("dtAir", "AIR", 0, 10, 3),
                  K ("dtSpeed", "SPEED", 0, 10, 5), T ("dtListen", "LISTEN", 0, "Off|Listen"), K ("dtMix", "MIX", 0, 100, 100, 0, "%")]),
  # VOCAL IDENTITY PROCESSOR (3.8.1.1): another person's voice - pitch, vocal tract and glottal source each on their own
  dict (grid4 = True, key = "voice", name = "VOCAL IDENTITY PROCESSOR", model = "VIP-12", sub = "FORMANT-PRESERVING VOICE TRANSFORMATION", u = 4, colour = (0.032, 0.033, 0.035), glow = (1.0, 0.72, 0.28), knob = "hifiBlackDisc", role = "ANOTHER PERSON'S VOICE - PITCH, TRACT AND BREATH, EACH ON ITS OWN",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("viCharacter", "CHARACTER", "CUSTOM|M TO F|F TO M|CHILD|TEEN|ELDERLY MAN|ELDERLY WOMAN|BIG MAN|SMALL PERSON|ANNOUNCER|HUSKY|WHISPER", 1),
                  T ("viMode", "MODE", 0, "LIVE|HQ"), K ("viPitch", "PITCH", -12, 12, 0, 1, " st"), K ("viFormant", "FORMANT", -10, 10, 0),
                  K ("viAge", "AGE", -10, 10, 0), K ("viGender", "GENDER", -10, 10, 0), K ("viBreath", "BREATH", 0, 10, 0), K ("viFry", "FRY", 0, 10, 0),
                  K ("viRough", "ROUGHNESS", 0, 10, 0), K ("viVibrato", "VIBRATO", 0, 10, 0), K ("viGate", "GATE", -80, -20, -60, 0, " dB"),
                  K ("viDeess", "DE-ESS", 0, 10, 3), K ("viMix", "MIX", 0, 100, 100, 0, "%"), K ("viOutput", "OUTPUT", -12, 12, 0, 1, " dB"), K ("viSmooth", "SMOOTH", 0, 10, 5)]),
  # PITCH CORRECTOR (3.8.1.1): autotune - each period to the scale's note, the formants kept
  dict (key = "pitchfix", name = "PITCH CORRECTOR", model = "PC-7", sub = "SCALE-AWARE PITCH CORRECTION", u = 3, colour = (0.032, 0.033, 0.035), glow = (1.0, 0.72, 0.28), knob = "hifiBlackDisc", role = "IN TUNE - FROM AN INVISIBLE NUDGE TO THE HARD, STEPPED SNAP",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [T ("pfAuto", "AUTO", 1, "Off|On"), C ("pfKey", "KEY", "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", 0), C ("pfScale", "SCALE", "CHROMATIC|MAJOR|MINOR|HARM. MINOR|MAJ. PENT.|MIN. PENT.|BLUES", 1),
                  K ("pfSpeed", "SPEED", 0, 400, 40, 0, " ms"), K ("pfHumanize", "HUMANIZE", 0, 10, 3), K ("pfAmount", "AMOUNT", 0, 100, 100, 0, "%"),
                  K ("pfFormant", "FORMANT", -10, 10, 0), T ("pfMode", "MODE", 0, "LIVE|HQ"), K ("pfGate", "GATE", -80, -20, -60, 0, " dB"),
                  K ("pfMix", "MIX", 0, 100, 100, 0, "%"), K ("pfOutput", "OUTPUT", -12, 12, 0, 1, " dB"), K ("pfSmooth", "SMOOTH", 0, 10, 5)]),
  # VOCAL TUNING AND IDENTITY PROCESSOR (3.8.1.1): everything in one - characters, pitch, formants, source, autotune
  dict (grid4 = True, key = "vocalstation", name = "VOCAL TUNING AND IDENTITY PROCESSOR", model = "VT-24", sub = "PITCH, FORMANT, TUNING AND CHARACTER", u = 5, colour = (0.032, 0.033, 0.035), glow = (1.0, 0.72, 0.28), knob = "hifiBlackDisc", role = "THE WHOLE VOICE - WHO IT SOUNDS LIKE AND WHETHER IT SINGS IN TUNE",
        screen = (-1.02, 0.13, 2.40, 1.38),
        params = [C ("vtCharacter", "CHARACTER", "CUSTOM|M TO F|F TO M|CHILD|TEEN|ELDERLY MAN|ELDERLY WOMAN|BIG MAN|SMALL PERSON|ANNOUNCER|HUSKY|WHISPER", 0),
                  T ("vtAuto", "AUTO", 1, "Off|On"), C ("vtKey", "KEY", "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", 0), C ("vtScale", "SCALE", "CHROMATIC|MAJOR|MINOR|HARM. MINOR|MAJ. PENT.|MIN. PENT.|BLUES", 1),
                  K ("vtSpeed", "SPEED", 0, 400, 40, 0, " ms"), K ("vtHumanize", "HUMANIZE", 0, 10, 3), K ("vtTune", "TUNE", 0, 100, 100, 0, "%"),
                  K ("vtPitch", "PITCH", -12, 12, 0, 1, " st"), K ("vtFormant", "FORMANT", -10, 10, 0), K ("vtAge", "AGE", -10, 10, 0), K ("vtGender", "GENDER", -10, 10, 0),
                  K ("vtBreath", "BREATH", 0, 10, 0), K ("vtFry", "FRY", 0, 10, 0), K ("vtRough", "ROUGHNESS", 0, 10, 0), K ("vtVibrato", "VIBRATO", 0, 10, 0),
                  K ("vtGate", "GATE", -80, -20, -60, 0, " dB"), K ("vtDeess", "DE-ESS", 0, 10, 3), T ("vtMode", "MODE", 0, "LIVE|HQ"),
                  K ("vtMix", "MIX", 0, 100, 100, 0, "%"), K ("vtOutput", "OUTPUT", -12, 12, 0, 1, " dB"), K ("vtSmooth", "SMOOTH", 0, 10, 5)]),
]

for _u in UNITS:
    if _u["key"] in MODIFIED and not any (q["id"].endswith ("Multiply") for q in _u["params"]):
        _pf = _u["params"][0]["id"][:2]
        _u["params"] = _u["params"] + [K (_pf + "Multiply", "MULTIPLY", 0.25, 3.0, 1.0, 2, "x"), K (_pf + "Strength", "STRENGTH", 0, 200, 100, 0, "%")]

# The LUNCHBOX's 500-series modules (the four it was built with - CLASS-A EQ, DE-HARSH, CROSSFEED, OUTPUT - are
# hand-made in Lunchbox.h / DeviceLayout.h; these are the rest). Each is a vertical card, `width` slots wide,
# its IN switch at the top and its knobs down the card; `pre`: before the CLASS-A EQ in the signal, else after
# DE-HARSH (CROSSFEED and the OUTPUT meter always come last). Their rows follow the CUSTOM slot's.
LB_MODULES = [
  dict (key = "pre", name = "PREAMP", model = "PR-5", role = "CLEAN PREAMP THAT WARMS AS YOU DRIVE IT", width = 1, pre = True, colour = (0.26, 0.25, 0.23), knob = "neveSmallGrey",
        params = [K ("lpGain", "GAIN", -12, 24, 0, 1, " dB"), K ("lpDrive", "DRIVE", 0, 10, 2), C ("lpCut", "LOW CUT", "OFF|30|60|120"),
                  K ("lpOut", "OUTPUT", -24, 12, 0, 1, " dB"), T ("lpPhase", "PHASE", 0, "Normal|Invert")]),
  dict (key = "filter", name = "HIGH & LOW-PASS FILTER", model = "FL-5", role = "HIGH-PASS AND LOW-PASS, 12 OR 24 DB", width = 1, pre = True, colour = (0.10, 0.11, 0.12), knob = "apiWhite",
        params = [K ("lfHpf", "HIGH-PASS", 20, 1000, 20, 0, " Hz"), K ("lfLpf", "LOW-PASS", 1000, 20000, 20000, 0, " Hz"), C ("lfSlope", "SLOPE", "12 dB|24 dB")]),
  dict (key = "eq550", name = "3-BAND EQ", model = "PROP-Q", role = "3-BAND EQ, PROPORTIONAL Q", width = 2, pre = True, colour = (0.07, 0.08, 0.10), knob = "apiBlue",
        params = [K ("l5Low", "LOW", -12, 12, 0, 1, " dB"), C ("l5LowF", "LOW Hz", "50|100|200|400", 1), K ("l5Mid", "MID", -12, 12, 0, 1, " dB"),
                  C ("l5MidF", "MID Hz", "400|800|1.5k|3k|5k", 2), K ("l5High", "HIGH", -12, 12, 0, 1, " dB"), C ("l5HighF", "HIGH Hz", "5k|7.5k|10k|12.5k|15k", 2),
                  T ("l5Shelf", "SHELF", 1, "Peak|Shelf")]),
  dict (key = "tubeeq", name = "TUBE EQ", model = "EQP-5", role = "PASSIVE TUBE PROGRAM EQ, BOOST AND CUT TOGETHER", width = 3, pre = True, colour = (0.12, 0.20, 0.30), knob = "pultecTopHat",
        params = [C ("ltLowF", "LOW CPS", "20|30|60|100", 2), K ("ltBoost", "LOW BOOST", 0, 10, 0), K ("ltAtten", "LOW ATTEN", 0, 10, 0), C ("ltHighF", "HIGH KCS", "3|4|5|8|10|12|16", 3),
                  K ("ltHBoost", "HIGH BOOST", 0, 10, 0), K ("ltBw", "BANDWIDTH", 0, 10, 5), K ("ltHAtten", "HIGH ATTEN", 0, 10, 0), C ("ltAttenF", "ATTEN SEL", "5|10|20", 1)]),
  dict (key = "tilt", name = "TILT EQ", model = "TL-5", role = "ONE-KNOB TILT ABOUT A PIVOT", width = 1, pre = True, colour = (0.30, 0.10, 0.05), knob = "apiRed",
        params = [K ("lwTilt", "TILT", -6, 6, 0, 1, " dB"), C ("lwPivot", "PIVOT Hz", "300|650|1.2k|2.5k", 1)]),
  dict (key = "air", name = "AIR EQ", model = "AB-5", role = "WIDE AIR-BAND SHELF", width = 1, pre = True, colour = (0.30, 0.32, 0.34), knob = "apiWhite",
        params = [K ("laAir", "AIR", 0, 12, 0, 1, " dB"), C ("laFreq", "FREQ", "2.5k|5k|10k|20k|40k", 2)]),
  dict (key = "loud", name = "LOUDNESS CONTOUR", model = "ISO-226", role = "PUTS BACK THE BASS YOUR EARS LOSE WHEN QUIET", width = 1, pre = True, colour = (0.04, 0.10, 0.08), knob = "consoleWhite",
        params = [K ("llLevel", "LISTEN dB", 40, 90, 70, 0, " dB"), K ("llAmount", "AMOUNT", 0, 100, 100, 0, "%"), T ("llAuto", "AUTO", 0, "Manual|Auto")]),
  dict (key = "deess", name = "DE-ESSER", model = "DS-5", role = "SWEEPABLE DE-ESSER, SPLIT OR WIDE", width = 1, pre = False, colour = (0.14, 0.05, 0.12), knob = "apiWhite",
        params = [K ("ldFreq", "FREQ", 2000, 12000, 6000, 0, " Hz"), K ("ldThresh", "THRESH", -50, 0, -24, 1, " dB"), K ("ldRange", "RANGE", 0, 16, 8, 1, " dB"),
                  T ("ldWide", "WIDE", 0, "Split|Wide"), T ("ldListen", "LISTEN", 0, "Off|Listen")]),
  dict (key = "trans", name = "TRANSIENT DESIGNER", model = "TD-5", role = "ATTACK AND SUSTAIN, LEVEL-INDEPENDENT", width = 1, pre = False, colour = (0.60, 0.45, 0.10), knob = "neveMaroon",
        params = [K ("lxAttack", "ATTACK", -100, 100, 0, 0, "%"), K ("lxSustain", "SUSTAIN", -100, 100, 0, 0, "%"), K ("lxOut", "OUTPUT", -12, 12, 0, 1, " dB")]),
  dict (key = "gate", name = "NOISE GATE", model = "GX-5", role = "CLEAN EXPANDER AND GATE", width = 1, pre = False, colour = (0.06, 0.06, 0.06), knob = "apiWhite",
        params = [K ("lgThresh", "THRESH", -80, -10, -60, 1, " dB"), K ("lgRange", "RANGE", 0, 60, 20, 0, " dB"), C ("lgRatio", "RATIO", "1:2|1:4|GATE"),
                  K ("lgRelease", "RELEASE", 10, 1000, 120, 0, " ms")]),
  dict (key = "comp", name = "BUS COMPRESSOR", model = "VCA-5", role = "CLEAN VCA BUS COMPRESSOR", width = 2, pre = False, colour = (0.16, 0.17, 0.19), knob = "apiRed",
        params = [K ("lcThresh", "THRESH", -30, 0, -10, 1, " dB"), C ("lcRatio", "RATIO", "1.5|2|4|10", 1), C ("lcAttack", "ATTACK", "0.1|0.3|1|3|10|30", 3),
                  C ("lcRelease", "RELEASE", "0.1|0.3|0.6|1.2|AUTO", 4), K ("lcMakeup", "MAKE-UP", 0, 15, 0, 1, " dB"), K ("lcMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "sat", name = "SATURATOR", model = "ST-5", role = "TAPE, TUBE OR CONSOLE WARMTH", width = 1, pre = False, colour = (0.35, 0.12, 0.05), knob = "neveMaroon",
        params = [K ("lsDrive", "DRIVE", 0, 10, 3), C ("lsType", "TYPE", "TAPE|TUBE|CONSOLE"), K ("lsTone", "TONE", -5, 5, 0), K ("lsMix", "MIX", 0, 100, 100, 0, "%")]),
  dict (key = "width", name = "MID/SIDE WIDTH", model = "MS-5", role = "MID/SIDE WIDTH AND BASS MONO", width = 1, pre = False, colour = (0.05, 0.08, 0.16), knob = "apiBlue",
        params = [K ("lmWidth", "WIDTH", 0, 200, 100, 0, "%"), C ("lmMono", "BASS MONO", "OFF|80|120|200"), K ("lmMid", "MID", -6, 6, 0, 1, " dB")]),
  dict (key = "limit", name = "PEAK LIMITER", model = "PL-5", role = "ZERO-LATENCY PEAK LIMITER", width = 1, pre = False, colour = (0.10, 0.02, 0.02), knob = "apiRed",
        params = [K ("lkCeiling", "CEILING", -12, 0, -1, 1, " dB"), K ("lkDrive", "DRIVE", 0, 12, 0, 1, " dB"), K ("lkRelease", "RELEASE", 10, 500, 80, 0, " ms")]),
]

HALF = 0.295   # 1U, half height

def cstr (s): return json.dumps (s)

def layout (u):
    """Where everything goes on a unit's panel: prints and controls."""
    halfH = HALF * u["u"]
    ps = u["params"]
    knobs = [p for p in ps if p["kind"] in (0, 2)]
    toggles = [p for p in ps if p["kind"] == 1]
    prints, ctrls = [], []
    unit = u["key"] + "Unit"
    two = u["u"] >= 2
    rows = [knobs] if (not two or len (knobs) <= 6) else [knobs[: (len (knobs) + 1) // 2], knobs[(len (knobs) + 1) // 2:]]
    if len (knobs) == 12: rows = [knobs[i:i + 6] for i in (0, 6)]
    x0, x1 = -1.30, 1.25
    r = 0.085 if not two else 0.095
    if u.get ("screen"):
        # A big screen on the left (a live picture), the knobs in a grid on the right
        sx, sz, sw, sh = u["screen"]
        prints.append (("D", sx, sz, sw, sh, 0, u["name"], 0, 0, 0, 0, 0, "#%02x%02x%02x" % tuple (int (255 * min (1, g)) for g in u["glow"])))
        cols = 4 if u.get ("grid4") else 3 if len (knobs) > 8 else 2   # (more than eight: three across; the voice units: four)
        four = len (knobs) > 6   # (seven or eight knobs: four rows of smaller ones)
        kr = 0.07 if cols == 4 else 0.075 if cols == 3 else 0.085 if four else 0.105
        for i, p in enumerate (knobs):
            rr, cc = divmod (i, cols)
            x = (0.34 + 0.37 * cc) if cols == 4 else (0.38 + 0.44 * cc) if cols == 3 else (0.52 + 0.56 * cc)
            rows4 = (len (knobs) + 3) // 4
            z = (-halfH + 0.40 + min (0.50, (2 * halfH - 0.85) / max (1, rows4 - 1)) * rr) if cols == 4 else (-0.63 + 0.42 * rr) if four else (-0.52 + 0.52 * rr)
            sel = p["kind"] == 2
            prints.append (("K", x, z, kr, 0, 0, p["label"], 0, int (p["hi"] - p["lo"]) if sel else 10, 1 if sel else 0, 0, 270.0, p["id"]))
            ctrls.append (f'        {{ ControlKind::{"selector" if sel else "knob"}, {x:.4f}f, {z:.4f}f, "{p["id"]}", "{p["label"]}", nullptr, nullptr, {unit}, "{u["name"]}", {kr / 0.105:.3f}f, KnobStyle::{"chickenHeadKnob" if sel else u["knob"]} }},')
        rows = []
    if two and not u.get ("screen"):
        prints.append (("D", 0.0, -halfH + 0.13, 2.60, 0.13, 0, u["name"] + " - " + u["sub"], 0, 0, 0, 0, 0, "#%02x%02x%02x" % tuple (int (255 * min (1, g)) for g in u["glow"])))
        zs = [0.05] if len (rows) == 1 else [-0.15, 0.33]   # (two rows: the lower one's labels clear of the panel's edge)
    else:
        zs = [-0.02]
    if two and len (rows) == 1: zs = [0.12]
    if u.get ("screen"): zs = []
    for row, z in zip (rows, zs):
        n = len (row)
        for i, p in enumerate (row):
            x = (x0 + x1) / 2 if n == 1 else x0 + (x1 - x0) * i / (n - 1)
            sel = p["kind"] == 2
            steps = int (p["hi"] - p["lo"]) if sel else 10
            prints.append (("K", x, z, r, 0, 0, p["label"], 0, steps, 1 if sel else 0, 0, 270.0, p["id"]))
            kind = "selector" if sel else "knob"
            style = "chickenHeadKnob" if sel else u["knob"]
            ctrls.append (f'        {{ ControlKind::{kind}, {x:.4f}f, {z:.4f}f, "{p["id"]}", "{p["label"]}", nullptr, nullptr, {unit}, "{u["name"]}", {r / 0.105:.3f}f, KnobStyle::{style} }},')
    # the right: an LED ladder (how hard it works), the switches, POWER
    nled = 6 if not two else 8
    lz0, lz1 = (-0.16, 0.16) if not two else (-0.30, 0.42)
    for i in range (nled):
        col = "#ff3b30" if i == nled - 1 else "#ffcc33" if i >= nled - 3 else "#46e070"
        prints.append (("E", 1.62, lz1 - (lz1 - lz0) * i / (nled - 1), 0.016, 0, 0, "", 0, 0, 0, 0, 0, col))
    buttons = [t for t in toggles if t["id"] in u.get ("buttons", [])]
    sw = [dict (id = u["power"], label = "POWER")] + [t for t in toggles if t not in buttons]
    column = u.get ("screen") and len (sw) > 2   # (a screen unit with several switches: one above another)
    for i, t in enumerate (buttons):   # (RACK TUNER: its TUNE, UNDO and A/B under the knobs, in the knobs' columns)
        x, z = 0.52 + 0.56 * (i % 2), 0.02 + 0.42 * (i // 2)
        prints.append (("T", x, z, 0, 0, 0, t["label"], 0, 0, 0, 0, 0, t["id"]))
        ctrls.append (f'        {{ ControlKind::toggle, {x:.4f}f, {z:.4f}f, "{t["id"]}", "{t["label"]}", nullptr, nullptr, {unit}, "{u["name"]}", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle }},')
    for i, t in enumerate (sw):
        x = 2.22 - 0.26 * i
        z = 0.0 if not two else 0.05
        if column: x, z = 2.06, -0.66 + 0.33 * i   # (clear of the ears' screws)
        prints.append (("T", x, z - 0.03, 0, 0, 0, t["label"], 0, 0, 0, 0, 0, t["id"]))
        ctrls.append (f'        {{ ControlKind::toggle, {x:.4f}f, {z - 0.03:.4f}f, "{t["id"]}", "{t["label"]}", nullptr, nullptr, {unit}, "{u["name"]}", 1.0f, KnobStyle::proXl, SwitchStyle::{"rockerRed" if i == 0 else "batToggle"} }},')
    # screws by the name, a vent under the ladder on a 2U
    if two: prints.append (("V", 1.62, halfH - 0.08, 0.30, 0.07, 0, "", 0, 7, 0, 0, 0, ""))
    return prints, ctrls

def lb_layout (m):
    """A module's card: IN at the top, then its knobs down the card (two columns when there are more than
    four, or on a 2-wide card), the other switches under them. Positions are local to the card's centre
    (x across, z down, the card 1.92 tall); the LUNCHBOX places the card in its slot at run time."""
    ps = m["params"]
    knobs = [p for p in ps if p["kind"] in (0, 2)]
    toggles = [p for p in ps if p["kind"] == 1]
    ctrls = []
    grp = m["name"]
    def ctl (kind, x, z, p, size, style, sw = None):
        extra = f", SwitchStyle::{sw}" if sw else ""
        ctrls.append (f'        {{ ControlKind::{kind}, {x:.4f}f, {z:.4f}f, "{p["id"]}", "{p["label"]}", nullptr, nullptr, lunchboxUnit, "{grp}", {size:.3f}f, KnobStyle::{style}{extra} }},')
    # IN at the top, between the knobs' columns (a 3-wide card: left of its middle column)
    ctl ("toggle", -0.22 if m["width"] == 3 else 0.0, -0.62, dict (id = m["power"], label = "IN"), 1.0, "proXl", "batToggle")
    cols = 3 if m["width"] == 3 else 2 if (m["width"] == 2 or len (knobs) > 4) else 1
    rows = (len (knobs) + cols - 1) // cols
    # (a single card stops higher: its model's name sits at its foot)
    z0, z1 = -0.28, (0.44 if toggles else 0.56) if m["width"] == 1 else 0.56
    if m["width"] >= 2: z0 = -0.36                              # (wide cards: their IN sits at the side)
    if m["width"] >= 2 and rows > 3: z0, z1 = -0.30, 0.74   # (four rows: the whole card)
    if m["width"] == 3: xs = [-0.44, 0.0, 0.44]
    elif m["width"] == 2: xs = [-0.22, 0.22]
    else: xs = [0.0] if cols == 1 else [-0.115, 0.115]
    size = (0.62 if m["width"] == 3 else (0.66 if rows <= 3 else 0.48) if m["width"] == 2 else (0.60 if cols == 1 else 0.46))
    for i, p in enumerate (knobs):
        r, c = divmod (i, cols)
        z = z0 if rows == 1 else z0 + (z1 - z0) * r / (rows - 1)
        sel = p["kind"] == 2
        ctl ("selector" if sel else "knob", xs[c], z, p, size * (0.85 if sel else 1.0), "neveGrey" if sel else m["knob"])
    for j, t in enumerate (toggles):
        if m["width"] >= 2: x, z = 0.26 * (j + 1), -0.62
        else: x, z = (0.0 if len (toggles) == 1 else (-0.11 + 0.22 * (j % 2))), 0.80
        ctl ("toggle", x, z, t, 1.0, "proXl", "batToggle")
    return ctrls

def lb_main (first):
    rows, info, ctrls = [], [], []
    for m in LB_MODULES:
        m["power"] = m["params"][0]["id"][:2] + "In"
        rs = [dict (id = m["power"], label = "IN", kind = 1, lo = 0, hi = 1, d = 0, dec = 0, unit = "", texts = "Out|In")] + m["params"]
        info.append ((m, first, len (rs)))
        for p in rs:
            name = "Lunchbox " + m["name"].title() + " " + p["label"].title()
            rows.append (f'        {{ {cstr (p["id"])}, {cstr (name)}, {cstr (p["label"])}, {cstr (p.get ("unit", ""))}, {p["kind"]}, {float (p["lo"])}f, {float (p["hi"])}f, {float (p["d"])}f, {p["dec"]}, {cstr (p.get ("texts", ""))} }},')
        first += len (rs)
        ctrls += lb_layout (m)
    (ROOT / "Source/DSP/units/LbParams.inc").write_text ("// GENERATED by Tools/units/gen_units.py - the LUNCHBOX modules' parameter rows\n" + "\n".join (rows) + "\n")
    total = sum (n for _, _, n in info)
    lines = ["#pragma once", "", "// GENERATED by Tools/units/gen_units.py - the LUNCHBOX's 500-series modules (beyond its first four)", "",
             "namespace enh::dsp::lbmods", "{",
             f"    inline constexpr int count = {len (LB_MODULES)};", f"    inline constexpr int numParams = {total};",
             "    struct Info { const char* key; const char* name; const char* model; const char* role; int firstParam, numParams, width; bool pre; float plate[3]; };",
             "    inline constexpr Info info[count] {"]
    for m, f, n in info:
        c = m["colour"]
        lines.append (f'        {{ {cstr (m["key"])}, {cstr (m["name"])}, {cstr (m["model"])}, {cstr (m["role"])}, {f}, {n}, {m["width"]}, {"true" if m["pre"] else "false"}, {{ {c[0]}f, {c[1]}f, {c[2]}f }} }},')
    lines += ["    };", "}", ""]
    (ROOT / "Source/DSP/units/LbList.h").write_text ("\n".join (lines))
    (ROOT / "Source/UI/Scene/LbControls.inc").write_text ("// GENERATED by Tools/units/gen_units.py - the LUNCHBOX modules' controls (card-local; placed at run time)\n" + "\n".join (ctrls) + "\n")
    return len (ctrls), total

def main ():
    power_rows, list_rows, prints_out, ctrl_rows = [], [], [], []
    first = FIRST_PARAM
    firsts = []
    for u in UNITS:
        u["power"] = u["params"][0]["id"][:2] + "Power"   # (each unit's own prefix)
        rows = [dict (id = u["power"], label = "POWER", kind = 1, lo = 0, hi = 1, d = 0, dec = 0, unit = "", texts = "Off|On")] + u["params"]
        firsts.append (first)
        for p in rows:
            name = u["name"].title() + " " + p["label"].title()
            power_rows.append (f'        {{ {cstr (p["id"])}, {cstr (name)}, {cstr (p["label"])}, {cstr (p.get ("unit", ""))}, {p["kind"]}, {float (p["lo"])}f, {float (p["hi"])}f, {float (p["d"])}f, {p["dec"]}, {cstr (p.get ("texts", ""))} }},')
        u["count"] = len (rows)
        first += len (rows)
    total = first - FIRST_PARAM
    (ROOT / "Source/DSP/units/UnitParams.inc").write_text ("// GENERATED by Tools/units/gen_units.py - the newer units' parameter rows\n" + "\n".join (power_rows) + "\n")
    lines = ["#pragma once", "", "// GENERATED by Tools/units/gen_units.py", "", "namespace enh::dsp::units", "{",
             f"    inline constexpr int count = {len (UNITS)};", f"    inline constexpr int firstParam = {FIRST_PARAM}, numParams = {total};",
             f"    inline constexpr int firstUnit = {FIRST_UNIT};",
             "    struct Info { const char* key; const char* name; int firstParam, numParams, heightU; };",
             f"    inline constexpr Info info[count] {{"]
    for u, f in zip (UNITS, firsts):
        lines.append (f'        {{ {cstr (u["key"])}, {cstr (u["name"])}, {f}, {u["count"]}, {u["u"]} }},')
    lines += ["    };", "}", ""]
    (ROOT / "Source/DSP/units/UnitList.h").write_text ("\n".join (lines))
    # the UI
    ids = ["// GENERATED by Tools/units/gen_units.py - the newer units' numbers (DeviceLayout's Unit enum)"]
    for i, u in enumerate (UNITS):
        ids.append (f"                {u['key']}Unit = {FIRST_UNIT + i},")
    (ROOT / "Source/UI/Scene/UnitIds.inc").write_text ("\n".join (ids) + "\n")
    pan = ["#pragma once", "", "// GENERATED by Tools/units/gen_units.py - the newer units' panels (DesignedLayout's Print format)", "",
           "#include <array>", '#include "DesignedLayout.h"', "", "namespace pad::layout::gen", "{",
           "    using designed::Print;",
           "    struct Look { const char* name; const char* model; const char* sub; const char* role; float plate[3], glow[3]; int heightU; };"]
    ctrl_all = []
    for i, u in enumerate (UNITS):
        prints, ctrls = layout (u)
        ctrl_all += ctrls
        pan.append (f"    inline constexpr std::array<Print, {len (prints)}> print{i} {{{{")
        for (k, x, z, w, h, size, text, align, steps, nums, flag, sweep, param) in prints:
            pan.append (f"        {{ '{k}', {x:.4f}f, {z:.4f}f, {w:.4f}f, {h:.4f}f, {size:.4f}f, {cstr (text)}, {align}, {steps}, {nums}, {flag}, {float (sweep):.1f}f, {cstr (param)} }},")
        pan.append ("    }};")
    pan.append (f"    inline constexpr int count = {len (UNITS)};")
    pan.append (f"    inline constexpr int numControls = {sum (len (layout (u)[1]) for u in UNITS)};   // (UnitControls.inc's rows)")
    pan.append ("    inline constexpr Look looks[count] {")
    for u in UNITS:
        c, g = u["colour"], u["glow"]
        pan.append (f'        {{ {cstr (u["name"])}, {cstr (u["model"])}, {cstr (u["sub"])}, {cstr (u["role"])}, {{ {c[0]}f, {c[1]}f, {c[2]}f }}, {{ {g[0]}f, {g[1]}f, {g[2]}f }}, {u["u"]} }},')
    pan.append ("    };")
    pan.append ("    /** Unit k's print (0 .. count-1). */")
    pan.append ("    inline std::pair<const Print*, int> printOf (int k) noexcept")
    pan.append ("    {")
    pan.append ("        switch (k)")
    pan.append ("        {")
    for i in range (len (UNITS)):
        pan.append (f"            case {i}: return {{ print{i}.data(), (int) print{i}.size() }};")
    pan.append ("            default: return { nullptr, 0 };")
    pan.append ("        }")
    pan.append ("    }")
    pan.append ("}")
    (ROOT / "Source/UI/Scene/UnitPanels.h").write_text ("\n".join (pan) + "\n")
    (ROOT / "Source/UI/Scene/UnitControls.inc").write_text ("// GENERATED by Tools/units/gen_units.py - the newer units' controls\n" + "\n".join (ctrl_all) + "\n")
    n, lbp = lb_main (FIRST_PARAM + total + 21)   # (after the CUSTOM slot's 21 rows)
    with open (ROOT / "Source/UI/Scene/UnitPanels.h", "a") as f:
        f.write (f"namespace pad::layout::gen {{ inline constexpr int numLbControls = {n}; }}\n")
    print (len (UNITS), "units,", total, "parameters,", len (ctrl_all), "controls;", len (LB_MODULES), "LUNCHBOX modules,", lbp, "parameters,", n, "controls")

if __name__ == "__main__":
    main()
