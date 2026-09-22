# Processing methods

Generated from `Source/DSP/MethodRegistry.h` by `EnhDspTests --methods-doc`; do not edit by hand
(the test suite fails when this page and the registry disagree). Back to [[00 Start Here]].

Click a unit on the rack to open its glass panel; each dropdown there is one stage below. The first
method of every stage is the default, and is how the unit sounded before methods existed. Every
method is zero-latency, and a switch crossfades over 30 ms. Method choices are stored in the session
(not automatable) and presets leave them alone.

## ADAPTIVE COMPRESSOR

### DETECTOR - How it measures the level

Parameter `tideDetector`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **PKR** Peak or RMS (default) | The side-chain's instantaneous peak or its RMS (+3 dB, so a steady sine reads the same either way), whichever is higher. | Catches transients as well as sustained loudness: drum hits and gunshots are held in firmly. The compressor's original behaviour. | Zero latency. No extra CPU (default). |
| **RMS** Root mean square | Only the side-chain's power, averaged over 50 ms. Short peaks barely register. | Reacts to loudness, not to transients: attacks pass through fuller and punchier (about a fifth less reduction on a hit), and the reduction lingers a little longer after it. Softer on drums, less peak control. | Zero latency. One multiply-add per sample more than PKR. |

### GAIN - How it calculates the reduction

One method so far.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **ADT** Adaptive threshold (default) | Threshold, ratio and knee from the programme: the loud part of the last seconds, crest factor, transient density and spectral tilt. Peaky material gets a higher threshold and a gentler ratio. | Compression sits under the music rather than under a fixed number, so it holds the same character on quiet and loud material. | Zero latency. Updated at 1.5 kHz. |

### SMOOTHING - How the reduction moves

Parameter `tideSmoothing`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **DRL** Dual release (default) | Two followers: a slow one carries the average gain reduction, a fast one takes only what a transient needs beyond it and gives it back in about 50 ms. | A kick dips the mix for a moment instead of pulling the sustained parts down and letting them swell back: about 30 % less pumping than SRL at the same average reduction. | Zero latency. No extra CPU (default). |
| **SRL** Single release | One attack / release follower on the target gain, with the programme-dependent release. | The classic bus-compressor movement: more audible breathing and glue, sustained parts pump with the kick. | Zero latency. Slightly less CPU than DRL. |

### RESPONSE LAW - How the RESPONSE knob maps

Parameter `tideResponseLaw`.

| Method | What it measures / does | How the sound changes | CPU / latency |
|---|---|---|---|
| **LIN** Linear (default) | The knob's travel maps straight to RESPONSE: 5 is half way. | The original feel. | No cost (default). |
| **EXP** Exponential | More of the knob's travel is spent at the slow, gentle end; the last third goes quickly to fast. | Finer control over slow, smooth compression; the same range overall. | No cost. |

## Knob modifiers

A modifier sits between a knob and its processing. It is stored in the session, not as a parameter:
the host always sees the knob's raw value.

| Knob | Side | Modifier | What it does | How the sound changes | Cost | Settings |
|---|---|---|---|---|---|---|
| `tideResponse` | input | **SMO** Smoothing | Glides the knob's value toward where it was set, one-pole, with this time constant. The same at every sample rate. | Automation and fast knob moves turn into gradual changes instead of steps. | No latency on the audio; the knob's effect lags by about the time constant. | off, 50 ms, 250 ms, 1000 ms |
