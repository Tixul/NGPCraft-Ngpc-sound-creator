#include "ngpc/instrument.h"

#include <sstream>

namespace ngpc {

std::vector<InstrumentPreset> FactoryInstrumentPresets() {
    //                     attn env_on step spd crv pcrv vib_on vdp vsp vdl sw_on sw_end sw_step sw_spd mode ncfg macro adsr_on a d s r sr lfo_on w r d h lfo2_on w h r d algo
    auto presets = std::vector<InstrumentPreset>{
        {"Clean Tone",    {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 0, 0, 0, 0, 0, 0,  0, 0}},
        {"Noise Kick",    {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 0, 0, 1, 0, 1, 13, 2}},  // periodic high
        {"Noise HiHat",   {4, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 4, 0, 1, 0, 0, 15, 1}},  // white high
        {"Noise Snare",   {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 5, 0, 1, 0, 1, 11, 2}},  // white mid
        {"Bright Lead",   {1, 0, 1, 1, 0, 0, 1, 2, 3, 2, 0,   1,  0, 1, 0, 0, 0, 1, 1, 2,  4, 4}},
        {"Soft Pad",      {4, 0, 1, 1, 0, 0, 1, 1, 5, 4, 0,   1,  0, 1, 0, 0, 0, 1, 3, 4,  8, 8, 1, 0, 10, 2}},
        {"Pluck",         {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 0, 0, 1, 1, 0, 1, 10, 2}},
        {"Bass",          {3, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 180, -2, 3, 0, 0, 0, 1, 0, 2,  6, 4}},
        {"Bell",          {2, 0, 1, 1, 0, 3, 1, 1, 4, 2, 0,   1,  0, 1, 0, 0, 0, 1, 0, 2,  9, 4}},
        {"Zap",           {2, 0, 1, 1, 0, 4, 0, 0, 1, 0, 1, 220, -6, 2, 0, 0, 1, 1, 0, 0, 12, 1}},
        {"Square Brass",  {2, 0, 1, 1, 0, 2, 0, 0, 1, 0, 1, 260, -3, 2, 0, 0, 0, 1, 1, 2,  5, 4}},
        {"Wide Lead",     {1, 0, 1, 1, 0, 3, 1, 2, 3, 1, 0,   1,  0, 1, 0, 0, 0, 1, 0, 1,  4, 3}},
        {"Deep Bass",     {2, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 140, -1, 4, 0, 0, 0, 1, 0, 2,  7, 5}},
        {"Soft Keys",     {3, 0, 1, 1, 1, 0, 1, 1, 4, 3, 0,   1,  0, 1, 0, 0, 0, 1, 2, 3,  8, 6}},
        {"Chime Pad",     {3, 0, 1, 1, 1, 2, 1, 1, 5, 4, 0,   1,  0, 1, 0, 0, 0, 1, 2, 3,  9, 8, 1, 0, 8, 2}},
        {"Sweep Rise FX", {2, 0, 1, 1, 0, 2, 0, 0, 1, 0, 1, 380,  6, 1, 0, 0, 1, 1, 0, 0, 11, 1}},
        {"Sweep Fall FX", {2, 0, 1, 1, 0, 4, 0, 0, 1, 0, 1, 120, -8, 1, 0, 0, 1, 1, 0, 0, 12, 1}},
        {"Noise Clap",    {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 6, 0, 1, 0, 0, 10, 3}},  // white low
        {"Noise Crash",   {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 6, 0, 1, 0, 1, 14, 6}},  // white low long tail
        {"Open HiHat",    {4, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 4, 0, 1, 0, 0, 14, 4}},  // white high
        {"Noise Tom",     {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 1, 0, 1, 0, 1, 11, 3}},  // periodic medium
        {"Siren FX",      {2, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1,  90, -2, 2, 0, 0, 0, 1, 0, 0, 10, 2, 1, 0, 2, 9}},
        {"Chip Lead PWM", {1, 0, 1, 1, 0, 0, 1, 1, 3, 1, 0,   1,  0, 1, 0, 0, 0, 1, 0, 1,  4, 3, 1, 1, 4, 4}},
        {"Pulse Organ",   {2, 0, 1, 1, 0, 0, 1, 1, 6, 10, 0,  1,  0, 1, 0, 0, 0, 0, 0, 0,  0, 0, 1, 1, 6, 2}},
        {"Chip Piano",    {2, 1, 2, 1, 2, 0, 0, 0, 1, 0, 0,   1,  0, 1, 0, 0, 1, 0, 0, 0,  0, 0}},
        {"Air Pad",       {5, 0, 1, 1, 0, 0, 1, 1, 6, 6, 0,   1,  0, 1, 0, 0, 0, 1, 6, 6,  9,12, 1, 0,12, 1}},
        {"Pulse Bass",    {2, 0, 1, 1, 0, 5, 0, 0, 1, 0, 1, 220, -3, 2, 0, 0, 3, 1, 0, 2,  7, 4}},
        {"Metal Lead",    {1, 0, 1, 1, 0, 6, 1, 2, 2, 1, 0,   1,  0, 1, 0, 0, 0, 1, 0, 1,  4, 4, 1, 1, 3, 3}},
        {"UI Blip",       {1, 1, 2, 1, 2, 7, 0, 0, 1, 0, 1, 300,-18, 1, 0, 0, 0, 0, 0, 0,  0, 0}},
        {"Noise Ride",    {5, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 6, 0, 1, 0, 1, 13, 8}},
        {"Noise Rim",     {1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 0, 0, 1, 0, 0, 12, 1}},
        {"Noise Shaker",  {6, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0,   1,  0, 1, 1, 4, 0, 1, 0, 0, 14, 2}},
    };

    // New modulation fields were added after legacy presets.
    // Normalize all presets first, then opt-in richer behavior on selected tones.
    for (auto& p : presets) {
        auto& d = p.def;
        d.adsr_sustain_rate = 0;
        d.lfo_hold = 0;
        d.lfo2_on = 0;
        d.lfo2_wave = 0;
        d.lfo2_hold = 0;
        d.lfo2_rate = 1;
        d.lfo2_depth = 0;
        d.lfo_algo = 1;
        if (d.lfo_wave > 4) d.lfo_wave = 0;
        if (d.lfo2_wave > 4) d.lfo2_wave = 0;
        if (d.adsr_sustain > 15) d.adsr_sustain = 15;
        if (d.mode == 1) {
            d.lfo_on = 0;
            d.lfo_depth = 0;
            d.lfo2_on = 0;
            d.lfo2_depth = 0;
            d.lfo_algo = 0;
        } else {
            if (d.lfo_on && d.lfo_depth > 0 && d.lfo_rate == 0) d.lfo_rate = 1;
        }
    }

    // Bright Lead: SNK-style tremolo + vibrato split (algo 1).
    {
        auto& d = presets[4].def;
        d.adsr_sustain_rate = 2;
        d.lfo_on = 1;
        d.lfo_wave = 1;
        d.lfo_hold = 2;
        d.lfo_rate = 4;
        d.lfo_depth = 6;
        d.lfo2_on = 1;
        d.lfo2_wave = 0;
        d.lfo2_hold = 0;
        d.lfo2_rate = 3;
        d.lfo2_depth = 5;
        d.lfo_algo = 1;
    }

    // Soft Pad: slow evolving sustain + dual LFO blend.
    {
        auto& d = presets[5].def;
        d.adsr_sustain_rate = 1;
        d.lfo_on = 1;
        d.lfo_wave = 0;
        d.lfo_hold = 8;
        d.lfo_rate = 6;
        d.lfo_depth = 4;
        d.lfo2_on = 1;
        d.lfo2_wave = 2;
        d.lfo2_hold = 16;
        d.lfo2_rate = 8;
        d.lfo2_depth = 3;
        d.lfo_algo = 2;
    }

    // Bell: light AM shimmer plus slight FM.
    {
        auto& d = presets[8].def;
        d.adsr_sustain_rate = 1;
        d.lfo_on = 1;
        d.lfo_wave = 2;
        d.lfo_hold = 0;
        d.lfo_rate = 3;
        d.lfo_depth = 4;
        d.lfo2_on = 1;
        d.lfo2_wave = 0;
        d.lfo2_hold = 4;
        d.lfo2_rate = 5;
        d.lfo2_depth = 2;
        d.lfo_algo = 4;
    }

    // Air Pad: deeper dual movement for long textures.
    {
        auto& d = presets[25].def;
        d.adsr_sustain_rate = 1;
        d.lfo_on = 1;
        d.lfo_wave = 0;
        d.lfo_hold = 10;
        d.lfo_rate = 7;
        d.lfo_depth = 6;
        d.lfo2_on = 1;
        d.lfo2_wave = 2;
        d.lfo2_hold = 20;
        d.lfo2_rate = 10;
        d.lfo2_depth = 4;
        d.lfo_algo = 2;
    }

    // Metal Lead: fast dual modulation to add bite.
    {
        auto& d = presets[27].def;
        d.adsr_sustain_rate = 2;
        d.lfo_on = 1;
        d.lfo_wave = 1;
        d.lfo_hold = 0;
        d.lfo_rate = 3;
        d.lfo_depth = 5;
        d.lfo2_on = 1;
        d.lfo2_wave = 2;
        d.lfo2_hold = 2;
        d.lfo2_rate = 4;
        d.lfo2_depth = 3;
        d.lfo_algo = 2;
    }

    return presets;
}

std::vector<EnvCurveDef> FactoryEnvCurves() {
    return {
        {"None",     {}},
        {"Fade Out", {0, 1, 2, 3, 4, 6, 8, 10}},
        {"Staccato", {0, 2, 5, 9, 13, 15}},
        {"Swell",    {12, 8, 5, 2, 0}},
        {"Gate Pulse",{0, 4, 0, 6, 1, 8, 2, 10}},
        {"Long Tail",{0, 1, 1, 2, 2, 3, 4, 5, 7, 9, 11, 13}},
    };
}

std::vector<PitchCurveDef> FactoryPitchCurves() {
    return {
        {"None",        {}},
        {"Gentle Down", {0, -2, -4, -6, -8}},
        {"Gentle Up",   {0, 2, 4, 6, 8}},
        {"Wobble",      {0, 2, 0, -2, 0}},
        {"Fast Fall",   {0, -4, -8, -12, -8, -4, 0}},
        {"Kick Drop",   {8, 4, 2, 0, -2, -4}},
        {"Trill",       {0, 3, 0, -3, 0, 3, 0, -3}},
        {"Pitch Up Fast",   {0, -6, -12, -18, -12, -6, 0}},
        {"Pitch Down Fast", {0, 6, 12, 18, 12, 6, 0}},
    };
}

std::vector<MacroDef> FactoryMacros() {
    return {
        {"None", {}},
        {"Pluck Punch", {
            {2, -4, 0},
            {4, 0, 0},
            {6, 4, 0},
        }},
        {"Hard Attack", {
            {1, -6, 0},
            {2, 0, -3},
            {2, 2, 0},
        }},
        {"Kick Punch", {
            {1, -6, 6},
            {2, 0, 2},
            {3, 3, 0},
        }},
        {"Gate Chop", {
            {2, 6, 0},
            {2, 0, 0},
            {2, 8, 0},
        }},
    };
}

std::string InstrumentPresetsToCArray(const std::vector<InstrumentPreset>& presets) {
    std::ostringstream out;
    out << "/* Generated by NGPC Sound Creator */\n\n";
    out << "static const BgmInstrument s_bgm_instruments[] = {\n";

    for (size_t i = 0; i < presets.size(); ++i) {
        const auto& p = presets[i];
        const auto& d = p.def;
        out << "    /* " << i << ": " << p.name << " */\n";
        out << "    { "
            << static_cast<int>(d.attn) << ", "
            << static_cast<int>(d.env_on) << ", "
            << static_cast<int>(d.env_step) << ", "
            << static_cast<int>(d.env_speed) << ", "
            << static_cast<int>(d.env_curve_id) << ", "
            << static_cast<int>(d.pitch_curve_id) << ", "
            << static_cast<int>(d.vib_on) << ", "
            << static_cast<int>(d.vib_depth) << ", "
            << static_cast<int>(d.vib_speed) << ", "
            << static_cast<int>(d.vib_delay) << ", "
            << static_cast<int>(d.sweep_on) << ", "
            << static_cast<int>(d.sweep_end) << ", "
            << static_cast<int>(d.sweep_step) << ", "
            << static_cast<int>(d.sweep_speed) << ", "
            << static_cast<int>(d.mode) << ", "
            << static_cast<int>(d.noise_config) << ", "
            << static_cast<int>(d.macro_id) << ", "
            << static_cast<int>(d.adsr_on) << ", "
            << static_cast<int>(d.adsr_attack) << ", "
            << static_cast<int>(d.adsr_decay) << ", "
            << static_cast<int>(d.adsr_sustain) << ", "
            << static_cast<int>(d.adsr_release) << ", "
            << static_cast<int>(d.adsr_sustain_rate) << ", "
            << static_cast<int>(d.lfo_on) << ", "
            << static_cast<int>(d.lfo_wave) << ", "
            << static_cast<int>(d.lfo_rate) << ", "
            << static_cast<int>(d.lfo_depth) << ", "
            << static_cast<int>(d.lfo_hold) << ", "
            << static_cast<int>(d.lfo2_on) << ", "
            << static_cast<int>(d.lfo2_wave) << ", "
            << static_cast<int>(d.lfo2_hold) << ", "
            << static_cast<int>(d.lfo2_rate) << ", "
            << static_cast<int>(d.lfo2_depth) << ", "
            << static_cast<int>(d.lfo_algo) << " }";
        if (i + 1 < presets.size()) {
            out << ",";
        }
        out << "\n";
    }

    out << "};\n";
    return out.str();
}

}  // namespace ngpc
