# Parameters

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Every knob and switch your DAW can automate. The **id** is what sessions and presets store; some
still use the units' old names (lumen = leveler, tide = compressor, seraph / silk / halo = tone & space).
Glass-panel settings are listed in [Methods](Methods.md).

*Made from [`Source/Parameters/ParameterSpecs.cpp`](../../Source/Parameters/ParameterSpecs.cpp) by `scripts/make-searchbar.py` — don't edit by hand.*

| id | On the panel | Name in your DAW | Range | Default |
|---|---|---|---|---|
| [`clarityNorm`](../../Source/Parameters/ParameterSpecs.cpp#L10) | CLARITY | Clarity (Norm) | 0 – 30 | 15 |
| [`clarityAdd`](../../Source/Parameters/ParameterSpecs.cpp#L11) | CLARITY | Clarity (Add) | 0 – 10 | 3 |
| [`clarityMode`](../../Source/Parameters/ParameterSpecs.cpp#L12) | MODE | Clarity Mode | Norm / Add + Norm | Norm |
| [`adaptSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L13) | ADAPT | Adapt Speed | 0 – 100 % | 40 |
| [`sub`](../../Source/Parameters/ParameterSpecs.cpp#L14) | SUB | Sub Enhance | 0 – 100 % | 0 |
| [`subBoost`](../../Source/Parameters/ParameterSpecs.cpp#L15) | +BOOST | Sub Boost | off / on | off |
| [`footstep`](../../Source/Parameters/ParameterSpecs.cpp#L16) | IN | Footstep Radar In | Out / In | Out |
| [`radarSens`](../../Source/Parameters/ParameterSpecs.cpp#L17) | SENSITIVITY | Footstep Sensitivity | 0 – 10 | 6 |
| [`radarBoost`](../../Source/Parameters/ParameterSpecs.cpp#L18) | BOOST | Footstep Boost | 0 – 34 dB | 6 |
| [`radarSpace`](../../Source/Parameters/ParameterSpecs.cpp#L19) | SPACE | Footstep Space | 0 – 10 | 4 |
| [`radarListen`](../../Source/Parameters/ParameterSpecs.cpp#L20) | LISTEN | Footstep Listen | off / on | off |
| [`enhMultiply`](../../Source/Parameters/ParameterSpecs.cpp#L21) | MULTIPLY | Enhancer Multiply | 0 – 3 x | 1 |
| [`enhStrength`](../../Source/Parameters/ParameterSpecs.cpp#L22) | STRENGTH | Enhancer Strength | 0 – 5 | 1 |
| [`tideMix`](../../Source/Parameters/ParameterSpecs.cpp#L24) | MIX | Compressor Mix | 0 – 100 % | 60 |
| [`tideResponse`](../../Source/Parameters/ParameterSpecs.cpp#L25) | RESPONSE | Compressor Response | 0 – 10 | 5 |
| [`tideActive`](../../Source/Parameters/ParameterSpecs.cpp#L26) | IN | Compressor In | Out / In | In |
| [`lumenTarget`](../../Source/Parameters/ParameterSpecs.cpp#L28) | TARGET | Leveler Target | -36 – -6 dB | -18 |
| [`lumenResponse`](../../Source/Parameters/ParameterSpecs.cpp#L29) | RESPONSE | Leveler Response | 0 – 10 | 5 |
| [`spectralRange`](../../Source/Parameters/ParameterSpecs.cpp#L30) | RANGE | Spectral Limiter Range | 0 – 18 dB | 9 |
| [`spectralRelease`](../../Source/Parameters/ParameterSpecs.cpp#L31) | RELEASE | Spectral Limiter Release | 30 – 600 ms | 150 |
| [`spectralCeiling`](../../Source/Parameters/ParameterSpecs.cpp#L32) | CEILING | Spectral Limiter Ceiling | -12 – 0 dB | 0 |
| [`spectralActive`](../../Source/Parameters/ParameterSpecs.cpp#L33) | IN | Spectral Limiter In | Out / In | In |
| [`lumenActive`](../../Source/Parameters/ParameterSpecs.cpp#L35) | IN | Leveler In | Out / In | In |
| [`seraphMode`](../../Source/Parameters/ParameterSpecs.cpp#L37) | POWER | Tone & Space Mode | Off / Tone / Tone + Space | Tone + Space |
| [`seraphMultiply`](../../Source/Parameters/ParameterSpecs.cpp#L38) | MULTIPLY | Tone & Space Multiply | 0 – 3 x | 1 |
| [`seraphStrength`](../../Source/Parameters/ParameterSpecs.cpp#L39) | STRENGTH | Tone & Space Strength | 0 – 5 | 1 |
| [`silkSmooth`](../../Source/Parameters/ParameterSpecs.cpp#L41) | SMOOTH | Tone Smooth | 0 – 10 | 4 |
| [`silkAir`](../../Source/Parameters/ParameterSpecs.cpp#L42) | AIR | Tone Air | 0 – 10 | 4 |
| [`silkWarmth`](../../Source/Parameters/ParameterSpecs.cpp#L43) | WARMTH | Tone Warmth | 0 – 10 | 3 |
| [`silkBody`](../../Source/Parameters/ParameterSpecs.cpp#L44) | BODY | Tone Body | 0 – 10 | 2 |
| [`silkOutput`](../../Source/Parameters/ParameterSpecs.cpp#L45) | OUTPUT | Tone Output | -12 – 12 dB | 0 |
| [`silkProtect`](../../Source/Parameters/ParameterSpecs.cpp#L46) | PROTECT | Tone Protect | off / on | on |
| [`silkTape`](../../Source/Parameters/ParameterSpecs.cpp#L47) | TAPE | Tone Tape | off / on | off |
| [`silkAuto`](../../Source/Parameters/ParameterSpecs.cpp#L48) | MATCH | Tone Level Match | off / on | on |
| [`silkSub`](../../Source/Parameters/ParameterSpecs.cpp#L49) | SUB | Tone Sub | 0 – 10 | 0 |
| [`heavenHold`](../../Source/Parameters/ParameterSpecs.cpp#L51) | LOUDNESS | Loudness Hold | 0 – 30 | 12 |
| [`heavenLift`](../../Source/Parameters/ParameterSpecs.cpp#L52) | LOUDNESS | Loudness Lift | 0 – 10 | 4 |
| [`heavenMode`](../../Source/Parameters/ParameterSpecs.cpp#L53) | LIFT | Loudness Mode | Hold / Lift + Hold | Hold |
| [`heavenAuto`](../../Source/Parameters/ParameterSpecs.cpp#L54) | AUTO | Auto Heaven | off / on | off |
| [`heavenAutoAmount`](../../Source/Parameters/ParameterSpecs.cpp#L55) | HEAVEN | Auto Heaven Amount | 0 – 10 | 5 |
| [`haloWidth`](../../Source/Parameters/ParameterSpecs.cpp#L57) | WIDTH | Space Width | 0 – 200 % | 120 |
| [`haloSpace`](../../Source/Parameters/ParameterSpecs.cpp#L58) | REVERB | Space Reverb | 0 – 10 | 2.5 |
| [`haloDecay`](../../Source/Parameters/ParameterSpecs.cpp#L59) | DECAY | Space Decay | 0.3 – 8 s | 2.2 |
| [`haloShimmer`](../../Source/Parameters/ParameterSpecs.cpp#L60) | SHIMMER | Space Shimmer | 0 – 10 | 1.5 |
| [`haloTone`](../../Source/Parameters/ParameterSpecs.cpp#L61) | TONE | Space Tone | 0 – 10 | 6 |
| [`haloDuck`](../../Source/Parameters/ParameterSpecs.cpp#L62) | DUCK | Space Duck | off / on | on |
| [`haloBassMono`](../../Source/Parameters/ParameterSpecs.cpp#L63) | BASS MONO | Space Bass Mono | off / on | on |
| [`haloMod`](../../Source/Parameters/ParameterSpecs.cpp#L64) | MOD | Space Mod | off / on | on |
| [`levelGain`](../../Source/Parameters/ParameterSpecs.cpp#L66) | LEVEL | Level | -24 – 12 dB | 0 |
| [`loudnessReset`](../../Source/Parameters/ParameterSpecs.cpp#L67) | RESET | Loudness Reset | off / on | off |
| [`balAmount`](../../Source/Parameters/ParameterSpecs.cpp#L69) | BALANCE | Balancer Balance | 0 – 10 | 5 |
| [`balSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L70) | SPEED | Balancer Speed | 0 – 10 | 5 |
| [`balTilt`](../../Source/Parameters/ParameterSpecs.cpp#L71) | TILT | Balancer Tilt | -5 – 5 | 0 |
| [`balRange`](../../Source/Parameters/ParameterSpecs.cpp#L72) | RANGE | Balancer Range | 0 – 12 dB | 6 |
| [`balActive`](../../Source/Parameters/ParameterSpecs.cpp#L73) | IN | Balancer In | Out / In | In |
| [`balResolution`](../../Source/Parameters/ParameterSpecs.cpp#L74) | RESOLUTION | Balancer Resolution | 0 – 10 | 0 |
| [`deepDepth`](../../Source/Parameters/ParameterSpecs.cpp#L75) | DEPTH | Deep Sub Depth | 0 – 10 | 0 |
| [`deepHull`](../../Source/Parameters/ParameterSpecs.cpp#L76) | HULL | Deep Sub Hull | 0 – 10 | 0 |
| [`deepSize`](../../Source/Parameters/ParameterSpecs.cpp#L77) | SIZE | Deep Sub Size | 0 – 10 | 5 |
| [`deepPressure`](../../Source/Parameters/ParameterSpecs.cpp#L78) | PRESSURE | Deep Sub Pressure | 0 – 10 | 0 |
| [`deepActive`](../../Source/Parameters/ParameterSpecs.cpp#L79) | IN | Deep Sub In | Out / In | In |
| [`charModelA`](../../Source/Parameters/ParameterSpecs.cpp#L80) | A | Character A | 0 – 8 | 3 |
| [`charModelB`](../../Source/Parameters/ParameterSpecs.cpp#L82) | B | Character B | 0 – 8 | 4 |
| [`charBlend`](../../Source/Parameters/ParameterSpecs.cpp#L84) | BLEND | Character Blend | 0 – 100 % | 0 |
| [`charDrive`](../../Source/Parameters/ParameterSpecs.cpp#L85) | DRIVE | Character Drive | 0 – 10 | 5 |
| [`charColour`](../../Source/Parameters/ParameterSpecs.cpp#L86) | COLOUR | Character Colour | 0 – 10 | 5 |
| [`charActive`](../../Source/Parameters/ParameterSpecs.cpp#L87) | IN | Character In | Out / In | Out |
| [`charGrit`](../../Source/Parameters/ParameterSpecs.cpp#L88) | GRIT | Character Grit | Clean drive / Drive + distortion | Drive + distortion |
| [`abCompare`](../../Source/Parameters/ParameterSpecs.cpp#L89) | COMPARE | Compare | Rack / Input (level-matched) | Rack |
| [`monitorSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L90) | SPEED | Monitor Speed | 1 – 10 | 5 |
| [`lbEqIn`](../../Source/Parameters/ParameterSpecs.cpp#L91) | IN | EQ In | Out / In | Out |
| [`lbHpf`](../../Source/Parameters/ParameterSpecs.cpp#L92) | HPF | EQ High-Pass | Off / 50 Hz / 80 Hz / 160 Hz / 300 Hz | Off |
| [`lbLowFreq`](../../Source/Parameters/ParameterSpecs.cpp#L93) | LOW | EQ Low Freq | 35 Hz / 60 Hz / 110 Hz / 220 Hz | 60 Hz |
| [`lbLowGain`](../../Source/Parameters/ParameterSpecs.cpp#L94) | LOW | EQ Low Gain | -16 – 16 dB | 0 |
| [`lbMidFreq`](../../Source/Parameters/ParameterSpecs.cpp#L95) | MID | EQ Mid Freq | 360 Hz / 700 Hz / 1.6 kHz / 3.2 kHz / 4.8 kHz / 7.2 kHz | 1.6 kHz |
| [`lbMidGain`](../../Source/Parameters/ParameterSpecs.cpp#L96) | MID | EQ Mid Gain | -18 – 18 dB | 0 |
| [`lbMidHiQ`](../../Source/Parameters/ParameterSpecs.cpp#L97) | HI Q | EQ Mid Hi Q | off / on | off |
| [`lbHighGain`](../../Source/Parameters/ParameterSpecs.cpp#L98) | HIGH | EQ High Gain | -16 – 16 dB | 0 |
| [`lbIron`](../../Source/Parameters/ParameterSpecs.cpp#L99) | IRON | EQ Iron | off / on | off |
| [`lbHarshIn`](../../Source/Parameters/ParameterSpecs.cpp#L100) | IN | De-Harsh In | Out / In | Out |
| [`lbHarshAmount`](../../Source/Parameters/ParameterSpecs.cpp#L101) | AMOUNT | De-Harsh Amount | 0 – 10 | 5 |
| [`lbHarshFreq`](../../Source/Parameters/ParameterSpecs.cpp#L102) | FREQ | De-Harsh Freq | 2.5 kHz / 4 kHz / 6.5 kHz | 4 kHz |
| [`lbHarshSpeed`](../../Source/Parameters/ParameterSpecs.cpp#L103) | SPEED | De-Harsh Speed | 10 – 200 ms | 30 |
| [`lbFeedIn`](../../Source/Parameters/ParameterSpecs.cpp#L104) | IN | Crossfeed In | Out / In | Out |
| [`lbFeedAmount`](../../Source/Parameters/ParameterSpecs.cpp#L105) | AMOUNT | Crossfeed Amount | 0 – 10 | 5 |
| [`presetPrev`](../../Source/Parameters/ParameterSpecs.cpp#L107) | PREV | Preset Previous | off / on | off |
| [`presetNext`](../../Source/Parameters/ParameterSpecs.cpp#L108) | NEXT | Preset Next | off / on | off |
