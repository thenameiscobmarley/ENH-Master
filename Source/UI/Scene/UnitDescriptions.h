#pragma once

#include "DeviceLayout.h"
#include "BackPanels.h"
#include "../../DSP/units/UnitList.h"
#include <juce_core/juce_core.h>
#include <string_view>

/*  What each unit is and does, in a sentence or two: shown when you point at a unit's back (the rack turned round)
    and at a unit in THE GEAR LOCKER. Units by their key (the newer ones, Tools/units/gen_units.py) or number. */
namespace pad::descriptions
{
    struct Entry { const char* key; const char* text; };

    inline constexpr Entry byKey[] {
        { "shimmer",    "A reverb of eight delay lines, its tail pitched up an octave (or a fifth) as it grows, for a shimmering, rising space." },
        { "plate",      "A steel plate reverb, modelled: bright, dense and smooth, with pre-delay and damping." },
        { "spring",     "A spring reverb tank: the drip and boing of real springs, one to three of them." },
        { "grain",      "A granular delay: small grains of the sound replayed, re-pitched, reversed and sprayed into a cloud." },
        { "tape",       "A studio tape machine: tape's saturation and compression, with its wow, flutter and hiss." },
        { "opto",       "An optical compressor: a light and a photocell do the gain riding - slow, smooth and musical." },
        { "varimu",     "A variable-mu tube compressor: the ratio grows as it works harder, for gentle, glueing control." },
        { "dyneq",      "A four-band dynamic EQ: each band cuts or boosts only while its frequency is too loud (or too quiet)." },
        { "shuffler",   "A stereo shuffler: widens the image above a frequency while keeping the bass mono." },
        { "rotator",    "A phase rotator: lowers peaks without compressing, so the master can go louder cleanly." },
        { "bode",       "A frequency shifter: moves every frequency by the same number of hertz, from subtle to metallic." },
        { "harm",       "A two-voice pitch shifter: adds voices above or below, detuned and delayed." },
        { "bbd",        "An analog bucket-brigade chorus: warm, wide ensemble movement." },
        { "vocoder",    "A 16-band vocoder: the sound's shape played by a synth carrier." },
        { "submaxx",    "Psychoacoustic bass: adds the harmonics of the low end so the bass is heard even on small speakers." },
        { "clip",       "An oversampled clipper: shaves peaks off cleanly, soft, hard or tube-like." },
        { "deharsh",    "A harshness suppressor: finds harsh, flickering highs as they happen and smooths them down." },
        { "maximizer",  "An audio exciter and maximizer: aligns the bass and treble in time and adds definition (LO CONTOUR, PROCESS)." },
        { "rayroom",    "A room simulator: a room around the sound traced from its walls, with tape crackle and pops if you want them." },
        { "vinyl",      "A turntable simulator: the sound played from a record - dust, wow and a worn stylus." },
        { "rotary",     "A rotary speaker: a turning horn and drum - Doppler, swirl and speed changes." },
        { "cassette",   "A cassette simulator: recorded to tape and played back - wow, flutter, hiss and tape stops." },
        { "tapeecho",   "A tape echo: a loop of tape past three heads; feedback past 100% runs away like the real thing." },
        { "valveamp",   "A valve amp simulator: a tube preamp and power stage whose supply sags as it works." },
        { "speakercab", "A speaker cabinet simulator: a speaker in its cabinet and the microphone in front of it." },
        { "radio",      "A radio simulator: the sound on or off the station - static, fading and a small speaker." },
        { "pendulum",   "A pendulum modulator: a swinging pendulum moves the sound; hits push it." },
        { "bounce",     "A bouncing delay: echoes that come sooner and quieter, like a dropped ball." },
        { "sympathy",   "A sympathetic resonator: six strings that ring along with the sound." },
        { "flyby",      "A Doppler flyby: the sound moving past you - pitch, distance and air." },
        { "tesla",      "A Tesla coil synthesizer: the sound played by an electric arc, crackle and mains buzz included." },
        { "talkbox",    "A talk box formant filter: the sound shaped by a mouth - A, E, I, O, U." },
        { "lavalamp",   "A resonant filter bank: slowly drifting resonances, rising and sinking." },
        { "clarity",    "A clarity enhancer: lifts the presence band when the rest of the mix is hiding it." },
        { "subdriver",  "A subwoofer simulator: the low end through a modelled sub cone - tight and controlled." },
        { "lathe",      "A vinyl mastering simulator: what a cutting lathe allows - mono bass and a tamed top." },
        { "cartest",    "A car listening test: hear the master in a car - cabin boom, door or dash speakers, road noise." },
        { "phonecheck", "A small speaker test: hear the master on a phone, a laptop or earbuds." },
        { "club",       "A club sound system test: hear the master on a club PA - subs, room and crowd." },
        { "pressure",   "A loudness maximizer: makes it loud and lets the peaks vent off smoothly." },
        { "balance",    "A tonal balance tilt: evens the low end against the top, automatically." },
        { "field",      "A stereo width control: widens the image while keeping the bass mono and mono-safe." },
        { "sonar",      "A transient shaper: finds each hit and shapes its attack and sustain." },
        { "seismo",     "A low-end damper: catches booming bass build-ups and damps them." },
        { "prism",      "A multiband saturator: five bands, each saturated in its own way." },
        { "furnace",    "A thermal saturator: warmth that builds up as it is driven, like a hot circuit." },
        { "dither",     "Dither: reduces the bit depth to 16, 20 or 24 bits, with the right noise to keep it clean." },
        { "rider",      "An automatic fader rider: rides the level to a target loudness, like a hand on the fader." },
        { "compass",    "A stereo phase aligner: brings left and right back into phase." },
        { "suspension", "A dynamics smoother: evens out the dynamics gently, without pumping." },
        { "skyline",    "A resonance suppressor: finds ringing peaks and trims them back." },
        { "hourglass",  "A program-dependent limiter: its release follows the music." },
        { "aurora",     "An air band exciter: adds shimmering air over the top, never harsh." },
        { "chroma",     "A space and tone processor: a space from a room to a vast tail, tone, and chops of the track that follow its melody." },
        { "hypercube",  "A morphing visualizer: a neon vector cube that morphs with the music. It does not change the sound." },
        { "tuner",      "An automatic rack tuner: type what you want and it sets every unit in the rack to match." },
        { "detail",     "A spectral detail enhancer: simulates how the ear masks quiet sounds under loud ones, and brings out what is hidden - "
                        "tails, breaths, ghost notes, air - with clarity, at the same level." },
        // the 500-series modules
        { "pre",    "A preamp module: gain and drive with a colour of its own." },
        { "filter", "A high and low-pass filter module." },
        { "eq550",  "A 3-band EQ module in the classic stepped style." },
        { "tubeeq", "A tube EQ module: passive boosts and cuts through a tube stage." },
        { "tilt",   "A tilt EQ module: one knob tips the sound darker or brighter." },
        { "air",    "An air EQ module: a high shelf for openness." },
        { "loud",   "A loudness contour module: fuller lows and highs at low listening levels." },
        { "deess",  "A de-esser module: tames sharp S sounds." },
        { "trans",  "A transient designer module: more or less attack and sustain." },
        { "gate",   "A noise gate module: silences what falls below a threshold." },
        { "comp",   "A bus compressor module: glues the mix together." },
        { "sat",    "A saturator module: harmonic warmth and grit." },
        { "width",  "A mid/side width module: narrower or wider stereo." },
        { "limit",  "A peak limiter module: catches the peaks." },
    };

    /** The rack's own units, by number (layout::Unit). */
    inline const char* builtIn (int unit) noexcept
    {
        using namespace layout;
        switch (unit)
        {
            case enhUnit:       return "An adaptive enhancer and EQ: listens to the sound and shapes it as it plays - adaptive EQ, harmonics for clarity, and sub.";
            case tubeUnit:      return "A finishing processor: smooths, adds air, warmth and body, and a space, all loudness-matched.";
            case tideUnit:      return "An adaptive compressor: its threshold and timing follow the music.";
            case lumenUnit:     return "A 3-band upward compressor: lifts the quiet detail without squashing the loud parts.";
            case limiterUnit:   return "A spectral limiter: a dynamic EQ that takes out only the frequencies that jump out, so nothing pumps.";
            case levelUnit:     return "Level control: sets the level the whole rack works at.";
            case balancerUnit:  return "A multiband balancer: six bands kept in balance with each other, dynamically.";
            case monitorUnit:   return "A loudness monitor: the rack's input against its output, and its loudness.";
            case deepUnit:      return "A sub-harmonic synthesizer: makes new sub bass under the low end, with a resonant hull.";
            case characterUnit: return "A console and tape emulator: the sound of consoles, tape and valves, and blends between them.";
            case radarUnit:     return "A footstep enhancer for games: finds footsteps, near and far, and makes them clearer.";
            case powerUnit:     return "A power conditioner: clean power for the rack, and its lights.";
            case lunchboxUnit:  return "A 500-series rack: a frame of modules - a class-A EQ, a harshness suppressor, crossfeed and more.";
            case x4Unit:        return "A smart tube enhancer EQ: four bands a side, each held at its target by a tube stage and a controller.";
            case velvetUnit:    return "A smoothing saturator: rounds off harshness with grain and colour.";
            case takebackUnit:  return "A dynamics restorer: gives back the attack, warmth, room and air that processing took away.";
            case scopeUnit:     return "An oscilloscope: a green CRT showing the sound - waveform, X-Y and mid/side. It does not change the sound.";
            default:            return unit == layout::customUnit ? "A custom unit: your own design from the Rack Unit Designer." : "";
        }
    }

    inline juce::String forUnit (int unit)
    {
        if (unit >= layout::firstGenUnit && unit < layout::firstGenUnit + enh::dsp::units::count)
        {
            const std::string_view key = enh::dsp::units::info[unit - layout::firstGenUnit].key;
            for (const auto& e : byKey)
                if (key == e.key) return e.text;
            return {};
        }
        return builtIn (unit);
    }

    inline juce::String forKey (std::string_view key)
    {
        for (const auto& e : byKey)
            if (key == e.key) return e.text;
        return {};
    }

    /** A unit's tooltip: its name, what it is, and (on its back) who made it. */
    inline juce::String tooltip (int unit, bool back)
    {
        juce::String t = juce::String (layout::unitInfo[(size_t) unit].name) + "\n" + forUnit (unit);
        if (back)
        {
            const auto& mk = backs::makers()[(size_t) backs::makerOf (unit)];
            t << "\n\nMade by " << mk.name << " - " << juce::String (mk.made).toLowerCase();
        }
        return t;
    }
}
