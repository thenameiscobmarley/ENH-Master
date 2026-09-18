# TONE & SPACE unit

The purple unit on top - a "celestial processor", not a compressor. It takes ENH Master's output and
makes it sound finished. Zero latency.

## Front panel

One unified front rather than a channel-split strip: the display in the middle, ten knobs in a row
along the bottom, six bat toggles in a grid on the right, the jewel lamp and the chicken-head POWER
selector on the left, MULTIPLY and STRENGTH masters at far left.

**POWER**: OFF (true bypass) / TONE / TONE + SPACE.

Right of the display (1.2.0): **AUTO** (a round button with its own LED, like every latching button) hands the heaven to
the unit, and the **HEAVEN** knob sets how far it may take it (0 = your knobs, 10 = all AUTO). AUTO
listens over ~4 s: crest factor (sustained or percussive), side/mid ratio (mono or wide), 8 kHz+ vs
presence (dull or bright), sub-120 Hz vs mids (thin or full). From those it picks REVERB, DECAY,
SHIMMER, SPACE TONE, WIDTH, AIR and SUB, and blends toward them over ~0.5 s. It learns nothing from silence.

**SUB** (the sixth TONE knob): an 80 Hz shelf, an envelope-normalised 2nd harmonic of the bass, and a
mono bloom (two damped 71 / 97 ms delay lines) that is let through only when the bass is not busy, so
it swells in the gaps. Everything backs off by up to 80 % (the harmonic by 92 %) as the bass passes -24 dBFS.

## Display

Left: the resonance dips SMOOTH is applying across 150 Hz - 16 kHz, live. Right: a pair of L/R bars
per process - SMOOTH, AIR, WARMTH, BODY, TAPE, LEVEL (auto gain, centre = 0 dB), WIDTH, SPACE,
SHIMMER - so the panel shows **what is happening to each channel**, which is the thing a stereo
processor normally hides.

## Stages

TONE (tone and texture) then SPACE (width and space). Both are described in [[Seraph stages]].

## Headroom and pumping

Up to 1.1 the unit ended in a zero-latency limiter. On loud bass its gain moved inside each bass
cycle, which is distortion. In 1.2.0 that limiter is gone. The rack ends in one lookahead limiter
(`FinalLimiter.h`: 1.5 ms lookahead, 25 ms hold, 150 ms release, -0.5 dBFS), so the gain never moves
inside a cycle.

LOUDNESS no longer pumps with the bass. Its level measure is K-weighted (150 Hz high-pass twice, and a
+4 dB shelf at 1.5 kHz) and averaged over 2 s, its gain moves over 3 s, and SPACE's DUCK and SMOOTH's
detector ignore everything below 120-200 Hz. Measured whole-rack pumping on the bass test is now 0.31 dB
(it was 1.41 dB).

Practical use: [[04 Making it sound heavenly]]. Related: [[ENH Master unit]], [[Parameters]].
