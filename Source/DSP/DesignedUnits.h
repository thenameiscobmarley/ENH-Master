#pragma once

#include <array>
#include <string_view>
#include "units/UnitList.h"
#include "units/LbList.h"

/*  The units made in the Rack Unit Designer and built into the rack: LATINSPHIEL PRO X4 (ProX4.h),
    VELVETIZER (Velvetizer.h) and TAKEBACK (Takeback.h). Their parameters, in one table that everything reads - the plugin's
    parameter list (ParameterSpecs), the engine's values (KnobValues::designed), the tests and EnhAudioLab -
    so a knob is added in one place. IDs never change once shipped (sessions and automation keep them). */
namespace enh::dsp::designed
{
    struct Param
    {
        std::string_view id, name, label, unit;
        int kind;                 // 0 continuous, 1 toggle, 2 choice
        float minValue, maxValue, defaultValue;
        int decimals;
        std::string_view texts;   // toggle: "off|on" (optional); choice: the choices, '|' between
    };

    inline constexpr int numParams = 62 + units::numParams + 21 + lbmods::numParams;   // (then the newer units': units/UnitList.h; the CUSTOM slot's 21; the LUNCHBOX modules': units/LbList.h)
    inline constexpr int firstCustomParam = 62 + units::numParams, numCustomKnobs = 16, numCustomSwitches = 4;
    inline constexpr int firstLbParam = firstCustomParam + 21;   // (the LUNCHBOX's 500-series modules)
    inline constexpr int numX4 = 35;        // the first 35 are PRO X4's, then VELVETIZER's (11), then TAKEBACK's (8)
    inline constexpr int numVelvet = 11, numTakeback = 8;   // (then PHOSPHOR, the scope: its 7 settings)

    inline constexpr std::array<Param, numParams> params {{
        { "x4Pwr", "Pro X4 Power", "PWR", "", 1, 0.0f, 1.0f, 0.0f, 0, "Off|On" },
        { "x4Mono", "Pro X4 Mono", "MONO", "", 1, 0.0f, 1.0f, 0.0f, 0, "" },
        { "x4X2", "Pro X4 X2", "X2", "", 1, 0.0f, 1.0f, 0.0f, 0, "" },
        { "x4Pid", "Pro X4 PID", "PID", "", 1, 0.0f, 1.0f, 0.0f, 0, "" },
        { "x4P", "Pro X4 Proportional", "PROPORTIONAL", "", 0, 0.0f, 10.0f, 1.0f, 1, "" },
        { "x4I", "Pro X4 Integral", "INTEGRAL", "", 0, 0.0f, 1.0f, 0.1f, 2, "" },
        { "x4D", "Pro X4 Derivative", "DERIVATIVE", "", 0, 0.0f, 4.0f, 0.4f, 1, "" },
        { "x4Populate", "Pro X4 Populate", "POPULATE", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "x4Saturate", "Pro X4 Saturate", "SATURATE", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "x4Widen", "Pro X4 Widen", "WIDEN", "", 0, 0.0f, 10.0f, 0.0f, 1, "" },
        { "x4Crisp", "Pro X4 Crisp", "CRISP", "", 0, 0.0f, 10.0f, 0.0f, 1, "" },
        { "x4L1Drive", "Pro X4 Left Low Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4L1Tone", "Pro X4 Left Low Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4L1Mix", "Pro X4 Left Low Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4L2Drive", "Pro X4 Left Low Mid Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4L2Tone", "Pro X4 Left Low Mid Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4L2Mix", "Pro X4 Left Low Mid Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4L3Drive", "Pro X4 Left High Mid Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4L3Tone", "Pro X4 Left High Mid Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4L3Mix", "Pro X4 Left High Mid Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4L4Drive", "Pro X4 Left High Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4L4Tone", "Pro X4 Left High Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4L4Mix", "Pro X4 Left High Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4R1Drive", "Pro X4 Right Low Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4R1Tone", "Pro X4 Right Low Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4R1Mix", "Pro X4 Right Low Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4R2Drive", "Pro X4 Right Low Mid Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4R2Tone", "Pro X4 Right Low Mid Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4R2Mix", "Pro X4 Right Low Mid Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4R3Drive", "Pro X4 Right High Mid Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4R3Tone", "Pro X4 Right High Mid Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4R3Mix", "Pro X4 Right High Mid Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "x4R4Drive", "Pro X4 Right High Drive", "DRIVE", "", 0, 0.0f, 100.0f, 40.0f, 0, "" },
        { "x4R4Tone", "Pro X4 Right High Tone", "TONE", " dB", 0, -10.0f, 10.0f, 0.0f, 1, "" },
        { "x4R4Mix", "Pro X4 Right High Mix", "MIX", "%", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "velPower", "Velvetizer Power", "POWER", "", 1, 0.0f, 1.0f, 0.0f, 0, "Off|On" },
        { "velBypass", "Velvetizer Bypass", "BYPASS", "", 1, 0.0f, 1.0f, 0.0f, 0, "" },
        { "velMode", "Velvetizer Mode", "MODE", "", 1, 0.0f, 1.0f, 1.0f, 0, "Add|Balance" },
        { "velLow", "Velvetizer Velvet Low", "VELVET LOW", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "velMid", "Velvetizer Velvet Mid", "VELVET MID", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "velHigh", "Velvetizer Velvet High", "VELVET HIGH", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "velGrain", "Velvetizer Grain", "GRAIN", "", 0, 0.0f, 10.0f, 2.5f, 1, "" },
        { "velCrisp", "Velvetizer Crisp", "CRISP", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "velColorA", "Velvetizer Color A", "COLOR TYPE A", "", 2, 0.0f, 5.0f, 0.0f, 0, "Tube|Tape|Transformer|Console|Transistor|Crystal" },
        { "velBalance", "Velvetizer Balance", "BALANCE", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "velColorB", "Velvetizer Color B", "COLOR TYPE B", "", 2, 0.0f, 5.0f, 1.0f, 0, "Tube|Tape|Transformer|Console|Transistor|Crystal" },
        { "tbPower", "Takeback Power", "POWER", "", 1, 0.0f, 1.0f, 0.0f, 0, "Off|On" },
        { "tbAuto", "Takeback Auto", "AUTO", "", 1, 0.0f, 1.0f, 1.0f, 0, "" },
        { "tbBlur", "Takeback Blur", "BLUR", "", 0, 0.0f, 10.0f, 0.0f, 1, "" },
        { "tbSharpen", "Takeback Sharpen", "SHARPEN", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "tbColor", "Takeback Color", "COLOR", "", 0, 0.0f, 10.0f, 3.0f, 1, "" },
        { "tbRaw", "Takeback Raw", "RAW", "", 0, 0.0f, 10.0f, 2.0f, 1, "" },
        { "tbShine", "Takeback Shine", "SHINE", "", 0, 0.0f, 10.0f, 4.0f, 1, "" },
        { "tbMix", "Takeback Mix", "MIX", "%", 0, 0.0f, 100.0f, 45.0f, 0, "" },
        // PHOSPHOR (the scope): it draws the sound, it doesn't change it - these only set the picture
        { "scPower", "Scope Power", "POWER", "", 1, 0.0f, 1.0f, 1.0f, 0, "Off|On" },
        { "scMode", "Scope Mode", "MODE", "", 2, 0.0f, 2.0f, 0.0f, 0, "X-Y|M/S|Y-T" },
        { "scIntensity", "Scope Intensity", "INTENSITY", "", 0, 0.0f, 10.0f, 6.0f, 1, "" },
        { "scFocus", "Scope Focus", "FOCUS", "", 0, 0.0f, 10.0f, 6.0f, 1, "" },
        { "scPersist", "Scope Persistence", "PERSIST", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "scGain", "Scope Gain", "V/DIV", "", 0, 0.0f, 10.0f, 5.0f, 1, "" },
        { "scTime", "Scope Timebase", "TIME/DIV", "", 0, 0.0f, 10.0f, 4.0f, 1, "" },
        { "scFit", "Scope Fit", "FIT", "", 1, 0.0f, 1.0f, 0.0f, 0, "Off|Fit" },   // (sizes the trace to fill the screen)
        // The newer units (Tools/units/gen_units.py), each starting with its POWER
#include "units/UnitParams.inc"
        // CUSTOM: a unit made in the Rack Unit Designer, loaded from its share code - its POWER, then 16 knob
        // slots and 4 switch slots (what each does comes from the design: see Custom/DesignCode.h)
        { "cuPower", "Custom Power", "POWER", "", 1, 0.0f, 1.0f, 0.0f, 0, "Off|On" },
        { "cuK1", "Custom Knob 1", "KNOB 1", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK2", "Custom Knob 2", "KNOB 2", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK3", "Custom Knob 3", "KNOB 3", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK4", "Custom Knob 4", "KNOB 4", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK5", "Custom Knob 5", "KNOB 5", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK6", "Custom Knob 6", "KNOB 6", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK7", "Custom Knob 7", "KNOB 7", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK8", "Custom Knob 8", "KNOB 8", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK9", "Custom Knob 9", "KNOB 9", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK10", "Custom Knob 10", "KNOB 10", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK11", "Custom Knob 11", "KNOB 11", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK12", "Custom Knob 12", "KNOB 12", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK13", "Custom Knob 13", "KNOB 13", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK14", "Custom Knob 14", "KNOB 14", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuK15", "Custom Knob 15", "KNOB 15", "", 0, 0.0f, 100.0f, 50.0f, 0, "" }, { "cuK16", "Custom Knob 16", "KNOB 16", "", 0, 0.0f, 100.0f, 50.0f, 0, "" },
        { "cuS1", "Custom Switch 1", "SWITCH 1", "", 1, 0.0f, 1.0f, 1.0f, 0, "" }, { "cuS2", "Custom Switch 2", "SWITCH 2", "", 1, 0.0f, 1.0f, 1.0f, 0, "" },
        { "cuS3", "Custom Switch 3", "SWITCH 3", "", 1, 0.0f, 1.0f, 1.0f, 0, "" }, { "cuS4", "Custom Switch 4", "SWITCH 4", "", 1, 0.0f, 1.0f, 1.0f, 0, "" },
        // The LUNCHBOX's 500-series modules (Tools/units/gen_units.py), each starting with its IN
#include "units/LbParams.inc"
    }};

    /** Where an ID sits in the table (-1: not one of these). */
    inline int indexOf (std::string_view id) noexcept
    {
        for (int i = 0; i < numParams; ++i)
            if (params[(size_t) i].id == id)
                return i;
        return -1;
    }

    inline constexpr std::array<float, numParams> defaults() noexcept
    {
        std::array<float, numParams> d {};
        for (int i = 0; i < numParams; ++i)
            d[(size_t) i] = params[(size_t) i].defaultValue;
        return d;
    }

    // Indices, for the mapping (ParameterMapping.h)
    enum : int
    {
        x4Pwr, x4Mono, x4X2, x4Pid, x4P, x4I, x4D, x4Populate, x4Saturate, x4Widen, x4Crisp,
        x4Bands,                                  // x4Bands + side * 12 + band * 3 + (0 drive, 1 tone, 2 mix)
        velPower = numX4, velBypass, velMode, velLow, velMid, velHigh, velGrain, velCrisp, velColorA, velBalance, velColorB,
        tbPower = numX4 + numVelvet, tbAuto, tbBlur, tbSharpen, tbColor, tbRaw, tbShine, tbMix,
        scPower = numX4 + numVelvet + numTakeback, scMode, scIntensity, scFocus, scPersist, scGain, scTime, scFit
    };
    static_assert (scFit == units::firstParam - 1, "the index list matches the table");
    static_assert (lbmods::count == 0 || lbmods::info[0].firstParam == firstLbParam, "the LUNCHBOX modules follow the CUSTOM slot");
}
