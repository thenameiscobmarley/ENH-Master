# Parameters

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Every knob and switch your DAW can automate. The **id** is what sessions and presets store; some
still use the units' old names (lumen = leveler, tide = compressor, seraph / silk / halo = tone & space).
Glass-panel settings are listed in [Methods](Methods.md).

*Made from [`Source/Parameters/ParameterSpecs.cpp`](../../Source/Parameters/ParameterSpecs.cpp) by `scripts/make-searchbar.py` — don't edit by hand.*

| id | On the panel | Name in your DAW | Range | Default |
|---|---|---|---|---|
| [`clarityNorm`](../../Source/Parameters/ParameterSpecs.cpp#L9) | CLARITY | Clarity (Norm) | 0 – 30 | 15 |
| [`clarityAdd`](../../Source/Parameters/ParameterSpecs.cpp#L10) | CLARITY | Clarity (Add) | 0 – 10 | 3 |
| [`clarityMode`](../../Source/Parameters/ParameterSpecs.cpp#L11) | MODE | Clarity Mode | Norm / Add + Norm | Norm |
| [`adaptSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L12) | ADAPT | Adapt Speed | 0 – 100 % | 40 |
| [`sub`](../../Source/Parameters/ParameterSpecs.cpp#L13) | SUB | Sub Enhance | 0 – 100 % | 0 |
| [`subBoost`](../../Source/Parameters/ParameterSpecs.cpp#L14) | +BOOST | Sub Boost | off / on | off |
| [`footstep`](../../Source/Parameters/ParameterSpecs.cpp#L15) | FOOTSTEP | Footstep Priority | off / on | off |
| [`enhMultiply`](../../Source/Parameters/ParameterSpecs.cpp#L16) | MULTIPLY | Enhancer Multiply | 0 – 3 x | 1 |
| [`enhStrength`](../../Source/Parameters/ParameterSpecs.cpp#L17) | STRENGTH | Enhancer Strength | 0 – 5 | 1 |
| [`tideMix`](../../Source/Parameters/ParameterSpecs.cpp#L19) | MIX | Compressor Mix | 0 – 100 % | 60 |
| [`tideResponse`](../../Source/Parameters/ParameterSpecs.cpp#L20) | RESPONSE | Compressor Response | 0 – 10 | 5 |
| [`tideActive`](../../Source/Parameters/ParameterSpecs.cpp#L21) | IN | Compressor In | Out / In | In |
| [`lumenTarget`](../../Source/Parameters/ParameterSpecs.cpp#L23) | TARGET | Leveler Target | -36 – -6 dB | -18 |
| [`lumenResponse`](../../Source/Parameters/ParameterSpecs.cpp#L24) | RESPONSE | Leveler Response | 0 – 10 | 5 |
| [`spectralRange`](../../Source/Parameters/ParameterSpecs.cpp#L25) | RANGE | Spectral Limiter Range | 0 – 18 dB | 9 |
| [`spectralRelease`](../../Source/Parameters/ParameterSpecs.cpp#L26) | RELEASE | Spectral Limiter Release | 30 – 600 ms | 150 |
| [`spectralCeiling`](../../Source/Parameters/ParameterSpecs.cpp#L27) | CEILING | Spectral Limiter Ceiling | -12 – 0 dB | 0 |
| [`spectralActive`](../../Source/Parameters/ParameterSpecs.cpp#L28) | IN | Spectral Limiter In | Out / In | In |
| [`lumenActive`](../../Source/Parameters/ParameterSpecs.cpp#L30) | IN | Leveler In | Out / In | In |
| [`seraphMode`](../../Source/Parameters/ParameterSpecs.cpp#L32) | POWER | Tone & Space Mode | Off / Tone / Tone + Space | Tone + Space |
| [`seraphMultiply`](../../Source/Parameters/ParameterSpecs.cpp#L33) | MULTIPLY | Tone & Space Multiply | 0 – 3 x | 1 |
| [`seraphStrength`](../../Source/Parameters/ParameterSpecs.cpp#L34) | STRENGTH | Tone & Space Strength | 0 – 5 | 1 |
| [`silkSmooth`](../../Source/Parameters/ParameterSpecs.cpp#L36) | SMOOTH | Tone Smooth | 0 – 10 | 4 |
| [`silkAir`](../../Source/Parameters/ParameterSpecs.cpp#L37) | AIR | Tone Air | 0 – 10 | 4 |
| [`silkWarmth`](../../Source/Parameters/ParameterSpecs.cpp#L38) | WARMTH | Tone Warmth | 0 – 10 | 3 |
| [`silkBody`](../../Source/Parameters/ParameterSpecs.cpp#L39) | BODY | Tone Body | 0 – 10 | 2 |
| [`silkOutput`](../../Source/Parameters/ParameterSpecs.cpp#L40) | OUTPUT | Tone Output | -12 – 12 dB | 0 |
| [`silkProtect`](../../Source/Parameters/ParameterSpecs.cpp#L41) | PROTECT | Tone Protect | off / on | on |
| [`silkTape`](../../Source/Parameters/ParameterSpecs.cpp#L42) | TAPE | Tone Tape | off / on | off |
| [`silkAuto`](../../Source/Parameters/ParameterSpecs.cpp#L43) | MATCH | Tone Level Match | off / on | on |
| [`silkSub`](../../Source/Parameters/ParameterSpecs.cpp#L44) | SUB | Tone Sub | 0 – 10 | 0 |
| [`heavenHold`](../../Source/Parameters/ParameterSpecs.cpp#L46) | LOUDNESS | Loudness Hold | 0 – 30 | 12 |
| [`heavenLift`](../../Source/Parameters/ParameterSpecs.cpp#L47) | LOUDNESS | Loudness Lift | 0 – 10 | 4 |
| [`heavenMode`](../../Source/Parameters/ParameterSpecs.cpp#L48) | LIFT | Loudness Mode | Hold / Lift + Hold | Hold |
| [`heavenAuto`](../../Source/Parameters/ParameterSpecs.cpp#L49) | AUTO | Auto Heaven | off / on | off |
| [`heavenAutoAmount`](../../Source/Parameters/ParameterSpecs.cpp#L50) | HEAVEN | Auto Heaven Amount | 0 – 10 | 5 |
| [`haloWidth`](../../Source/Parameters/ParameterSpecs.cpp#L52) | WIDTH | Space Width | 0 – 200 % | 120 |
| [`haloSpace`](../../Source/Parameters/ParameterSpecs.cpp#L53) | REVERB | Space Reverb | 0 – 10 | 2.5 |
| [`haloDecay`](../../Source/Parameters/ParameterSpecs.cpp#L54) | DECAY | Space Decay | 0.3 – 8 s | 2.2 |
| [`haloShimmer`](../../Source/Parameters/ParameterSpecs.cpp#L55) | SHIMMER | Space Shimmer | 0 – 10 | 1.5 |
| [`haloTone`](../../Source/Parameters/ParameterSpecs.cpp#L56) | TONE | Space Tone | 0 – 10 | 6 |
| [`haloDuck`](../../Source/Parameters/ParameterSpecs.cpp#L57) | DUCK | Space Duck | off / on | on |
| [`haloBassMono`](../../Source/Parameters/ParameterSpecs.cpp#L58) | BASS MONO | Space Bass Mono | off / on | on |
| [`haloMod`](../../Source/Parameters/ParameterSpecs.cpp#L59) | MOD | Space Mod | off / on | on |
| [`levelGain`](../../Source/Parameters/ParameterSpecs.cpp#L61) | LEVEL | Level | -24 – 12 dB | 0 |
| [`loudnessReset`](../../Source/Parameters/ParameterSpecs.cpp#L62) | RESET | Loudness Reset | off / on | off |
| [`balAmount`](../../Source/Parameters/ParameterSpecs.cpp#L64) | BALANCE | Balancer Balance | 0 – 10 | 5 |
| [`balSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L65) | SPEED | Balancer Speed | 0 – 10 | 5 |
| [`balTilt`](../../Source/Parameters/ParameterSpecs.cpp#L66) | TILT | Balancer Tilt | -5 – 5 | 0 |
| [`balRange`](../../Source/Parameters/ParameterSpecs.cpp#L67) | RANGE | Balancer Range | 0 – 12 dB | 6 |
| [`balActive`](../../Source/Parameters/ParameterSpecs.cpp#L68) | IN | Balancer In | Out / In | In |
| [`balResolution`](../../Source/Parameters/ParameterSpecs.cpp#L69) | RESOLUTION | Balancer Resolution | 0 – 10 | 0 |
| [`deepDepth`](../../Source/Parameters/ParameterSpecs.cpp#L70) | DEPTH | Deep Sub Depth | 0 – 10 | 0 |
| [`deepHull`](../../Source/Parameters/ParameterSpecs.cpp#L71) | HULL | Deep Sub Hull | 0 – 10 | 0 |
| [`deepSize`](../../Source/Parameters/ParameterSpecs.cpp#L72) | SIZE | Deep Sub Size | 0 – 10 | 5 |
| [`deepPressure`](../../Source/Parameters/ParameterSpecs.cpp#L73) | PRESSURE | Deep Sub Pressure | 0 – 10 | 0 |
| [`deepActive`](../../Source/Parameters/ParameterSpecs.cpp#L74) | IN | Deep Sub In | Out / In | In |
| [`charModelA`](../../Source/Parameters/ParameterSpecs.cpp#L75) | A | Character A | 0 – 8 | 3 |
| [`charModelB`](../../Source/Parameters/ParameterSpecs.cpp#L77) | B | Character B | 0 – 8 | 4 |
| [`charBlend`](../../Source/Parameters/ParameterSpecs.cpp#L79) | BLEND | Character Blend | 0 – 100 % | 0 |
| [`charDrive`](../../Source/Parameters/ParameterSpecs.cpp#L80) | DRIVE | Character Drive | 0 – 10 | 5 |
| [`charActive`](../../Source/Parameters/ParameterSpecs.cpp#L81) | IN | Character In | Out / In | Out |
| [`charGrit`](../../Source/Parameters/ParameterSpecs.cpp#L82) | GRIT | Character Grit | Clean drive / Drive + distortion | Drive + distortion |
| [`abCompare`](../../Source/Parameters/ParameterSpecs.cpp#L83) | COMPARE | Compare | Rack / Input (level-matched) | Rack |
| [`monitorSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L84) | SPEED | Monitor Speed | 1 – 10 | 5 |
| [`presetPrev`](../../Source/Parameters/ParameterSpecs.cpp#L86) | PREV | Preset Previous | off / on | off |
| [`presetNext`](../../Source/Parameters/ParameterSpecs.cpp#L87) | NEXT | Preset Next | off / on | off |
