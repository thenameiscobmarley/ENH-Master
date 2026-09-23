# Processing methods

Generated from `Source/DSP/MethodRegistry.h` by `EnhDspTests --methods-doc`; do not edit by hand
(the test suite fails when this page and the registry disagree). Back to [[00 Start Here]].

Click a unit on the rack to open its glass panel. Its settings are grouped in categories: PROCESSING
(how the unit measures, calculates and moves), KNOBS (each knob's law and modifiers), OUTPUT and
DISPLAY. The first method of every setting is the default, and is how the unit sounded before these
settings existed. No method changes the reported latency; audio-rate methods crossfade over 30 ms when
switched, control-rate ones glide through the unit's own smoothing. Choices are stored in the session
(not automatable) and presets leave them alone. RESET TO DEFAULTS at the bottom of a panel puts all of
a unit's settings back.

## LEVEL CONTROL

### GLIDE - How fast LEVEL moves

PROCESSING. Parameter `levelGlide`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** 20 ms glide (default) | LEVEL moves to where it is set over about 20 ms. | The original: knob moves and automation are smooth, but immediate. | No cost (default). |
| **FST** 5 ms glide | Over about 5 ms. | Snappier level changes for automation that should land exactly on the beat. | No cost. |
| **SLW** 150 ms glide | Over about 150 ms. | Level changes fade in gently: rides and automation never jump. | No cost. |

## ADAPTIVE ENHANCER

### HARMONICS - What the exciters generate

PROCESSING. Parameter `enhancerHarmonics`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **CHB** Chebyshev 2 + 3 (default) | The DEPTH and CLARITY exciters generate 2nd and 3rd harmonics in the original mix (body mostly even, definition even and odd). | The original colour: warm body, clear definition. | Zero latency, 2x oversampled (default). |
| **EVN** Even | Mostly 2nd harmonic, very little 3rd. | Warmer, rounder, tube-like: detail becomes fuller rather than sharper. | Zero latency, same CPU. Glides over 30 ms (an exact crossfade). |
| **ODD** Odd | Mostly 3rd harmonic, less 2nd. | Edgier and more forward, tape-and-transistor-like: definition cuts through a dense mix. | Zero latency, same CPU. Glides over 30 ms (an exact crossfade). |

## UPWARD LEVELER

### LIFT - How far quiet material may come up

PROCESSING. Parameter `levelerLift`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Standard lift (default) | The most each band may lift: 6 dB low, 12 dB mid, 9 dB high. | The original amount of detail recovery. | Control rate, no audio cost (default). |
| **GEN** Gentle | 60 % of those limits. | Quiet detail comes up less: more natural dynamics, less noise brought forward. | Control rate, no audio cost. Glides (slew-limited). |
| **BIG** Big | 1.3 times those limits. | The quietest detail comes right up: maximum audibility, flatter dynamics. | Control rate, no audio cost. Glides (slew-limited). |

### GATE - What is too quiet to lift

PROCESSING. Parameter `levelerGate`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Gate -58 dBFS (default) | Nothing below about -58 dBFS, or without enough movement in it, is lifted: hiss and room tone stay down. | The original balance between detail and noise. | Control rate, no audio cost (default). |
| **SNS** Sensitive -66 | The gate opens 8 dB lower. | Even fainter sounds are lifted; in a noisy source more of the noise comes up too. | Control rate, no audio cost. Glides (slew-limited). |
| **STR** Strict -50 | The gate opens 8 dB higher. | Only clearly audible material is lifted: the cleanest, for noisy sources. | Control rate, no audio cost. Glides (slew-limited). |

### BAND BALANCE - Where the lift goes

PROCESSING. Parameter `levelerBalance`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Voiced (default) | Each band aims a little differently: low 4 dB under the target, high 1.5 dB under. | Lifts favour the midrange, where detail and footsteps live. The original. | Control rate, no audio cost (default). |
| **MID** Mid focus | Low 7 dB under, high 3 dB under. | Even more of the lift goes to the midrange: voices and steps forward, boom and hiss left alone. | Control rate, no audio cost. Glides (slew-limited). |
| **FLT** Flat | Every band aims at the target itself. | Lows and highs come up as much as the mids: fuller and brighter, less focused. | Control rate, no audio cost. Glides (slew-limited). |

## DEEP SUB

### SUB SHAPE - What the generated sub is

PROCESSING. Parameter `deepShape`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **SIN** Sine (default) | The generated sub is a pure sine an octave under the bass note. | The deepest and cleanest: felt more than heard, pure pressure. Best on a subwoofer or big headphones. | Zero latency (default). |
| **WRM** Warm | A softly saturated sine: a little 3rd harmonic on top of the sub. | Still deep, but it carries on small speakers and earbuds: the octave-down can be heard, not only felt. | Zero latency, same CPU. Crossfades over 30 ms. |
| **GRL** Growl | A rounded square wave: strong odd harmonics. | A dark, growling engine-room sub with a lot of presence. The most aggressive. | Zero latency, same CPU. Crossfades over 30 ms. |

### TRACKING - How it follows the bass note

PROCESSING. Parameter `deepTracking`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Standard (default) | The generated note follows the bass note with a 30 ms glide. | Locks on to bass lines and hits quickly without warbling. | Control rate, no audio cost (default). |
| **FST** Fast | An 8 ms glide. | Follows fast bass lines and pitch drops (808s) closely; can sound a little busier. | Control rate, no audio cost. |
| **STB** Stable | A 90 ms glide. | A steadier, smoother sub that ignores small pitch wobbles: best for drones and ambience. | Control rate, no audio cost. |

### HULL MATERIAL - How long the hull rings

PROCESSING. Parameter `deepMaterial`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STL** Steel (default) | The hull's resonances ring for up to about 3 seconds. | A vast steel hull: long, singing, metallic low tails after every hit. The submarine. | Control rate, no audio cost (default). |
| **IRN** Iron | They ring about half as long, and the upper modes are darker. | Heavier and duller: a thick cast hull, the weight without as much tail. | Control rate, no audio cost. Glides. |
| **CAV** Cavern | Short rings with brighter upper modes. | A rock cavern rather than a hull: dense and close, the low end blooms and stops. | Control rate, no audio cost. Glides. |

## SPECTRAL LIMITER

### NORMAL - What counts as normal for a band

PROCESSING. Parameter `limiterNormal`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **P97** 97th percentile (default) | A band's normal is how far above its baseline it went 97 % of the time over the last 10 s. | Kick drums and regular hits are learnt as normal and left alone; only real outliers are cut. The original. | Control rate, no audio cost (default). |
| **P90** 90th percentile | Normal is where the band stayed 90 % of the time: a lower bar. | Stricter: more events count as abnormal and are cut, so the balance is held tighter, with more movement. | Control rate, no audio cost. |
| **P99** 99th percentile | Normal is where the band stayed 99 % of the time: only the very rarest peaks count. | Looser: it steps in only for truly unusual events, and leaves dynamic material almost untouched. | Control rate, no audio cost. |

### CUT WIDTH - How wide each cut is

PROCESSING. Parameter `limiterWidth`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Region width (default) | Each cut is as wide as the run of bands that jumped out. | The original: a cut covers the offending region and not much more. | Control rate, no audio cost (default). |
| **NAR** Narrow | Cuts 60 % as wide. | Surgical: less of the surrounding material moves, but a broad event may be only partly caught. | Control rate, no audio cost. Glides. |
| **WID** Wide | Cuts 1.6 times as wide. | Smoother and more forgiving: a broad event is caught whole, at the price of touching more of its neighbours. | Control rate, no audio cost. Glides. |

### LOUDNESS KEEPER - What it gives back while cutting

PROCESSING. Parameter `limiterKeeper`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **K60** Keep 60 % (default) | Where a cut takes a band below its usual level, 60 % of the ear-weighted loss is given back to the whole mix (at most 3 dB, within the headroom). | Heavy cuts no longer make the rest of the mix sound quieter. The original setting. | Control rate, no audio cost (default). |
| **K90** Keep 90 % | Gives back 90 % of that loss. | Holds the loudness almost exactly through a cut; the lift can be a little more noticeable. | Control rate, no audio cost. Glides. |
| **OFF** Off | No make-up for cuts. | Cuts are heard as they are: the mix dips a little while a region is held down. | No cost. Glides. |

## MIX BALANCER

### REFERENCE - What a band's jump is measured against

PROCESSING. Parameter `balancerReference`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **MED** Median (default) | A band's jump is measured against the median move of all the bands. | One or two bands jumping never shift the reference, so exactly those are corrected. The original. | Control rate, no audio cost (default). |
| **AVG** Average | Against the average move of all the bands. | Every band pulls the reference a little: corrections are smaller and spread wider, a gentler overall balance. | Control rate, no audio cost. Glides. |

### DEAD ZONE - How big a move is ignored

PROCESSING. Parameter `balancerDeadZone`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** 1.5 dB (default) | Moves smaller than 1.5 dB beyond the mix's own are taken as the programme breathing and left alone. | The original: it only rides real imbalances. | Control rate, no audio cost (default). |
| **TGT** Tight 0.75 dB | A 0.75 dB dead zone. | Rides smaller imbalances too: a steadier tonal balance, more constant movement. | Control rate, no audio cost. Glides. |
| **LSE** Loose 3 dB | A 3 dB dead zone. | Steps in only for obvious imbalances: the most natural, the least correction. | Control rate, no audio cost. Glides. |

### LIFTS - What it does with a band that drops out

PROCESSING. Parameter `balancerLifts`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **HLF** Half range (default) | A band that drops out is lifted, to half of RANGE, more gently than cuts. | The original: holes are partly filled, never overdone. | Control rate, no audio cost (default). |
| **NON** Cuts only | Bands are only ever cut. | Nothing is ever boosted: the safest, cleanest balance, but holes stay holes. | Control rate, no audio cost. Glides. |
| **FUL** Full range | Lifts go as far as cuts, to the full RANGE. | Missing regions are filled in fully: a fuller, more even sound, with more movement. | Control rate, no audio cost. Glides. |

### ATTACK GUARD - How it treats a fresh transient

PROCESSING. Parameter `balancerGuard`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **STD** Guard attacks (default) | A band in a fresh transient (its fast level 2.5 times its average) is not cut yet, and while footsteps are being lifted by the enhancer the cuts let go. | Footsteps and gunshots keep their front edge, and the balancer never takes back the footstep lift. The original guard. | Control rate, no audio cost (default). |
| **STR** Strong guard | The guard trips at 1.8 times the average: more attacks are protected. | Punchier transients; a harsh burst gets a moment longer before it is caught. | Control rate, no audio cost. Glides. |
| **OFF** No guard | Attacks are cut like anything else (footsteps being lifted still release the cuts). | The tightest control of sudden bursts, at the price of softened attacks. | Control rate, no audio cost. Glides. |

### LOUDNESS KEEPER - What it gives back while cutting

PROCESSING. Parameter `balancerKeeper`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **K60** Keep 60 % (default) | Where a cut takes a band below its usual level, 60 % of the ear-weighted loss is given back to the whole mix (at most 3 dB, within the headroom). | Heavy cuts no longer make the rest of the mix sound quieter. The original setting. | Control rate, no audio cost (default). |
| **K90** Keep 90 % | Gives back 90 % of that loss. | Holds the loudness almost exactly through a cut; the lift can be a little more noticeable. | Control rate, no audio cost. Glides. |
| **OFF** Off | No make-up for cuts. | Cuts are heard as they are: the mix dips a little while a region is held down. | No cost. Glides. |

## ADAPTIVE COMPRESSOR

### DETECTOR - How it measures the level

PROCESSING. Parameter `tideDetector`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **PKR** Peak or RMS (default) | The side-chain's instantaneous peak or its RMS (+3 dB, so a steady sine reads the same either way), whichever is higher. | Catches transients as well as sustained loudness: drum hits and gunshots are held in firmly. The original behaviour. | Zero latency. No extra CPU (default). |
| **RMS** Root mean square | Only the side-chain's power, averaged over 50 ms. Short peaks barely register. | Reacts to loudness, not to transients: attacks pass through fuller and punchier (about a fifth less reduction on a hit), and the reduction lingers a little longer after it. Softer on drums, less peak control. | Zero latency. One multiply-add per sample more than PKR. |
| **KWT** K-weighted | RMS over 50 ms of the side-chain lifted 4 dB above 1.5 kHz, the way a loudness meter hears it. | Compresses what sounds loud rather than what measures loud: bright, harsh passages are held a little more, dull heavy ones a little less. | Zero latency. One filter per sample more than PKR. |

### SIDE-CHAIN - What the detector hears

PROCESSING. Parameter `tideSideChain`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **H90** High-pass 90 Hz (default) | The detector listens through a 90 Hz, 12 dB/oct high-pass, as engineers set a bus compressor's side-chain filter. | Steady bass does not drive the gain reduction, so it cannot pull the whole mix down and let it swell back. The original setting. | Zero latency (default). |
| **H15** High-pass 150 Hz | The same filter, nearly an octave higher. | Even kick drums and bass-heavy hits barely move it: the mix keeps its low-end punch, the compressor rides the mids and highs. | Zero latency, same CPU. Crossfades over 30 ms. |
| **FUL** Full range | No side-chain filter: the detector hears everything. | Bass counts in full: big low end is controlled, at the price of more audible pumping on bass-heavy material. | Zero latency, slightly less CPU. Crossfades over 30 ms. |

### GAIN - How it calculates the reduction

PROCESSING. Parameter `tideGain`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **ADT** Adaptive threshold (default) | Threshold, ratio and knee from the programme: the loud part of the last seconds, crest factor, transient density and spectral tilt. | Compression sits under the music rather than under a fixed number, so it holds the same character on quiet and loud material. The original computer. | Zero latency. Updated at 1.5 kHz (default). |
| **SFT** Soft | The same adaptive threshold, with 60 % of the ratio and a knee 6 dB wider. | Gentler, more transparent levelling: less reduction, and it comes in gradually. Good for music. | Zero latency, same CPU. Crossfades over 30 ms. |
| **HRD** Hard | The same adaptive threshold, with 1.5 times the ratio and a third of the knee. | Firm, obvious control: loud moments are clamped, the level stays tightly even. Good for voice and competitive play. | Zero latency, same CPU. Crossfades over 30 ms. |

### SMOOTHING - How the reduction moves

PROCESSING. Parameter `tideSmoothing`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **DRL** Dual release (default) | Two followers: a slow one carries the average gain reduction, a fast one takes only what a transient needs beyond it and gives it back in about 50 ms. | A kick dips the mix for a moment instead of pulling the sustained parts down and letting them swell back: about 30 % less pumping than SRL at the same average reduction. The original. | Zero latency. No extra CPU (default). |
| **SRL** Single release | One attack / release follower on the target gain, with the programme-dependent release. | The classic bus-compressor movement: more audible breathing and glue, sustained parts pump with the kick. | Zero latency. Slightly less CPU than DRL. Crossfades over 30 ms. |
| **OPT** Opto | One follower whose release slows as the reduction deepens, like an optical cell: quick from light reduction, slow from deep. | Smooth and musical: small moves recover quickly, big ones fade back gently, so heavy moments never snap back. | Zero latency, same CPU as SRL. Crossfades over 30 ms. |

### MAKE-UP - How much level it gives back

PROCESSING. Parameter `tideMakeup`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **AUT** Auto 65 % (default) | Gives back 65 % of the average gain reduction, slowly, so MIX is roughly level-matched. | The original balance: compressed and louder-sounding without jumping in level. | No cost (default). |
| **FUL** Full 90 % | Gives back 90 % of the average reduction. | Denser and louder: quiet detail comes up further. The output limiter catches anything that would go over. | No cost. Glides over seconds. |
| **OFF** None | No make-up gain: the reduction is heard as it is. | The mix gets quieter where it is compressed. Useful for judging what the compressor really does. | No cost. Glides over seconds. |

### LAW (tideResponse knob) - How the knob's travel maps

KNOBS. Parameter `tideResponseLaw`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **LIN** Linear (default) | The knob's travel maps straight to RESPONSE: 5 is half way. | The original feel. | No cost (default). |
| **EXP** Exponential | More of the knob's travel is spent at the slow, gentle end; the last third goes quickly to fast. | Finer control over slow, smooth compression; the same range overall. | No cost. |
| **LOG** Logarithmic | More of the travel is spent at the fast end; the first third goes quickly from slow. | Finer control over fast, aggressive settings; the same range overall. | No cost. |

## TONE & SPACE

### TAPE CURVE - How TAPE saturates

PROCESSING. Parameter `seraphTape`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **TNH** Tanh (default) | TAPE's saturation curve: a hyperbolic tangent after 5 dB of treble pre-emphasis. | The original: harsh transients rounded, a gentle, even softness. | Zero latency (default). |
| **ATN** Arctangent | A gentler curve that keeps rising instead of flattening. | Softer saturation: rounds less, keeps more of the attack, more open. | Zero latency, same CPU. Crossfades over 30 ms. |
| **CUB** Cubic | A cubic soft clipper: clean up to the knee, then a firm rounding. | Cleaner below the knee, thicker above: louder moments get the character, quiet ones stay clean. | Zero latency, less CPU. Crossfades over 30 ms. |

### PRE-DELAY - When the space begins

PROCESSING. Parameter `seraphPreDelay`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **P18** 18 ms (default) | The reverb's pre-delay: how long after the sound its space begins. | The original: the dry sound stays clear, the space follows closely. | Zero latency (default). |
| **P08** 8 ms | A short pre-delay. | The space hugs the sound: more intimate, more blended, a smaller-feeling room. | Zero latency. Crossfades over 30 ms. |
| **P35** 35 ms | A long pre-delay. | The sound stands clear in front of a bigger space: more depth and separation. | Zero latency. Crossfades over 30 ms. |

### LOUDNESS WINDOW - How long LOUDNESS listens

PROCESSING. Parameter `seraphWindow`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **W2S** 2 s window (default) | LOUDNESS measures K-weighted level over about 2 s before it moves. | The original: steady, no pumping with the bass. | Control rate, no audio cost (default). |
| **W1S** 1 s window | Measures over about 1 s. | Follows level changes faster: holds the level tighter, may breathe a little on sparse material. | Control rate, no audio cost. Glides. |
| **W4S** 4 s window | Measures over about 4 s. | The steadiest: level changes are followed slowly and almost inaudibly. | Control rate, no audio cost. Glides. |

## OUTPUT MONITOR

### CEILING - The output limiter's ceiling

OUTPUT. Parameter `outputCeiling`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **0.0** 0.0 dBFS (default) | The output limiter's ceiling: nothing leaves above full scale. | The original: as loud as possible without clipping. | 3 ms lookahead, unchanged (default). |
| **0.3** -0.3 dBFS | Ceiling 0.3 dB under full scale. | A little safety for converters and players that clip slightly early. | 3 ms lookahead, unchanged. Glides with the limiter's release. |
| **1.0** -1.0 dBFS | Ceiling 1 dB under full scale, the usual target for streaming and encoded files. | Room for lossy encoding and sample-rate conversion to overshoot without clipping. | 3 ms lookahead, unchanged. Glides with the limiter's release. |

### LOUDNESS TARGET - How loud everything leaves the rack

OUTPUT. Parameter `outputTarget`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **OFF** No target (default) | Nothing: what leaves the rack is as loud as the units made it. | The original. A quiet game stays quiet and a loud song stays loud. | No cost (default). |
| **23** -23 LUFS | Its own input's loudness (K-weighted, 3 s), and a gain that brings it to -23 LUFS: slowly, holding through bursts and pauses, 12 dB at most either way. | Everything comes out at one quiet, broadcast level: films, games and voice calls sit together, with lots of room for peaks. For late nights and wide dynamics. | No latency. One K-weighting filter pair per channel. |
| **18** -18 LUFS | The same, to -18 LUFS. | One comfortable level for everything: a game, a song and a call come out equally loud, and explosions still stand out above it. | No latency. One K-weighting filter pair per channel. |
| **14** -14 LUFS | The same, to -14 LUFS (the level streaming services play music at). | Loud and even: quiet games are brought right up. The output limiter works harder on material with big peaks. | No latency. One K-weighting filter pair per channel. |

### TONE RANGE - The tone-change curve's scale

DISPLAY. Parameter `displayToneRange`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **12** +-12 dB (default) | The red tone-change curve spans +-12 dB. | The original scale. | Display only (default). |
| **6** +-6 dB | It spans +-6 dB: small changes are drawn twice as large. | Subtle tone changes become easy to see. | Display only. |
| **24** +-24 dB | It spans +-24 dB. | Big moves stay on the card. | Display only. |

### DUCK HOLD - How long DUCK holds a reading

DISPLAY. Parameter `displayDuckHold`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **1.5** 1.5 s (default) | The DUCK readout holds the deepest duck for 1.5 s. | The original: short ducks can be read. | Display only (default). |
| **0.5** 0.5 s | Holds it for 0.5 s. | Follows the ducks closely, changes often. | Display only. |
| **4.0** 4 s | Holds it for 4 s. | Easy to read even brief ducks; slower to show the next one. | Display only. |

### WAVEFORM - What each waveform column shows

DISPLAY. Parameter `displayWaveform`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **PK** Peak (default) | Each waveform column shows the loudest sample in it. | The original: every hit and its reduction are visible. | Display only (default). |
| **RMS** RMS | Each column shows the column's RMS level (+3 dB, so a steady sine reads the same as PK). | Shows loudness rather than peaks: how the rack changes the body of the sound. | Display only. |

## Knob modifiers

Every continuous knob has these, between the knob and its processing (in the panel, under KNOBS). They
are stored in the session, not as parameters: the host always sees the knob's raw value. With all
three off the knob is exactly as it always was.

| Modifier | What it does | How the sound changes | Cost | Settings |
|---|---|---|---|---|
| **SMO** Smoothing | Glides the knob's value toward where it was set, one-pole, with this time constant. The same at every sample rate and block size. | Automation and fast knob moves become gradual changes instead of steps. | No latency on the audio; the knob's effect lags by about the time constant. | **OFF** (off), 50 ms, 250 ms, 1 s |
| **CRV** Curve | Reshapes the knob's travel before it reaches the processing: LOW spends more travel at the bottom of the range (squared), HIGH more at the top (square root), S at both ends (smoothstep). | Finer control where you need it; the knob still covers the same range end to end. | No cost. | **LINEAR** (off), LOW, HIGH, S |
| **LIM** Range | Limits how far the knob reaches: its full travel covers only the bottom part of its range. | A safety net for live use and presets: the setting can never be pushed past the limit, and the knob gets finer. | No cost. | **FULL** (off), 75 %, 50 %, 25 % |

Knobs with modifiers: `clarityNorm`, `clarityAdd`, `adaptSpeed`, `sub`, `enhMultiply`, `enhStrength`, `heavenHold`, `heavenLift`, `heavenAutoAmount`, `tideMix`, `tideResponse`, `lumenTarget`, `lumenResponse`, `spectralRange`, `spectralRelease`, `spectralCeiling`, `levelGain`, `balAmount`, `balSpeed`, `balTilt`, `balRange`, `balResolution`, `silkSmooth`, `silkAir`, `silkWarmth`, `silkBody`, `silkOutput`, `silkSub`, `haloWidth`, `haloSpace`, `haloDecay`, `haloShimmer`, `haloTone`, `seraphMultiply`, `seraphStrength`, `deepDepth`, `deepHull`, `deepSize`, `deepPressure`.
