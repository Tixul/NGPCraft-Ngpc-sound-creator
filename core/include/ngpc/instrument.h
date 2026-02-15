#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ngpc {

// Mirrors BgmInstrument from driver_custom_latest/sounds.c
struct BgmInstrumentDef {
    uint8_t  attn           = 2;
    uint8_t  env_on         = 0;
    uint8_t  env_step       = 1;
    uint8_t  env_speed      = 1;
    uint8_t  env_curve_id   = 0;
    uint8_t  pitch_curve_id = 0;
    uint8_t  vib_on         = 0;
    uint8_t  vib_depth      = 0;
    uint8_t  vib_speed      = 1;
    uint8_t  vib_delay      = 0;
    uint8_t  sweep_on       = 0;
    uint16_t sweep_end      = 1;
    int16_t  sweep_step     = 0;
    uint8_t  sweep_speed    = 1;
    uint8_t  mode           = 0;  // 0=tone, 1=noise
    uint8_t  noise_config   = 0;  // noise: 0-7 (rate=bits0-1, type=bit2)
    uint8_t  macro_id       = 0;
    uint8_t  adsr_on        = 0;  // 0=legacy env, 1=ADSR
    uint8_t  adsr_attack    = 0;  // frames per step 15→attn
    uint8_t  adsr_decay     = 0;  // frames per step attn→sustain
    uint8_t  adsr_sustain   = 0;  // sustain attn level 0-15
    uint8_t  adsr_release   = 0;  // frames per step cur->15
    uint8_t  adsr_sustain_rate = 0; // frames per step sustain->silent (0=hold)
    uint8_t  lfo_on         = 0;  // 0=off, 1=on
    uint8_t  lfo_wave       = 0;  // LFO1: 0=tri,1=square,2=saw,3=sweep up,4=sweep down
    uint8_t  lfo_rate       = 1;  // LFO1 frames per step (0=off)
    uint8_t  lfo_depth      = 0;  // divider delta amount
    uint8_t  lfo_hold       = 0;  // LFO1 hold frames (0=immediate)
    uint8_t  lfo2_on        = 0;  // second LFO enable
    uint8_t  lfo2_wave      = 0;  // LFO2 wave (same encoding as LFO1)
    uint8_t  lfo2_hold      = 0;  // LFO2 hold frames
    uint8_t  lfo2_rate      = 1;  // LFO2 frames per step (0=off)
    uint8_t  lfo2_depth     = 0;  // LFO2 divider delta amount
    uint8_t  lfo_algo       = 1;  // modulation algorithm 0..7 (SNK-style)
};

struct EnvCurveDef {
    std::string name;
    std::vector<int8_t> steps;
};

struct PitchCurveDef {
    std::string name;
    std::vector<int16_t> steps;
};

struct MacroStepDef {
    uint8_t frames = 0;
    int8_t attn_delta = 0;
    int16_t pitch_delta = 0;
};

struct MacroDef {
    std::string name;
    std::vector<MacroStepDef> steps;
};

struct InstrumentPreset {
    std::string name;
    BgmInstrumentDef def;
};

std::vector<InstrumentPreset> FactoryInstrumentPresets();
std::vector<EnvCurveDef> FactoryEnvCurves();
std::vector<PitchCurveDef> FactoryPitchCurves();
std::vector<MacroDef> FactoryMacros();

// Generate C source code for driver integration (BgmInstrument initializers).
std::string InstrumentPresetsToCArray(const std::vector<InstrumentPreset>& presets);

}  // namespace ngpc
