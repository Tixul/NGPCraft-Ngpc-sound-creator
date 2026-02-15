#include "audio/InstrumentPlayer.h"

#include <QTimer>
#include <algorithm>

#include "audio/EngineHub.h"
#include "audio/PsgHelpers.h"

namespace {
const std::vector<ngpc::MacroDef>& MacroDefs() {
    static const std::vector<ngpc::MacroDef> kMacros = ngpc::FactoryMacros();
    return kMacros;
}

int16_t LfoStepWave(uint8_t wave, int16_t cur, int8_t& sign, int16_t depth) {
    if (depth <= 0) return 0;
    switch (wave) {
    case 0: {
        int16_t next = static_cast<int16_t>(cur + sign);
        if (next >= depth) {
            next = depth;
            sign = -1;
        } else if (next <= -depth) {
            next = static_cast<int16_t>(-depth);
            sign = 1;
        }
        return next;
    }
    case 1:
        sign = (sign < 0) ? static_cast<int8_t>(1) : static_cast<int8_t>(-1);
        return static_cast<int16_t>(depth * sign);
    case 2: {
        int16_t next = static_cast<int16_t>(cur + 1);
        if (next > depth) next = static_cast<int16_t>(-depth);
        return next;
    }
    case 3:
        if (cur < depth) return static_cast<int16_t>(cur + 1);
        return depth;
    case 4:
        if (cur > -depth) return static_cast<int16_t>(cur - 1);
        return static_cast<int16_t>(-depth);
    default:
        return cur;
    }
}

bool LfoTick(bool on, uint8_t wave, uint8_t rate, uint8_t depth,
             uint8_t& hold_counter, uint8_t& counter, int8_t& sign, int16_t& delta) {
    if (!on || depth == 0 || rate == 0) {
        if (delta != 0) {
            delta = 0;
            return true;
        }
        return false;
    }
    if (hold_counter > 0) {
        hold_counter--;
        if (delta != 0) {
            delta = 0;
            return true;
        }
        return false;
    }
    if (counter == 0) {
        counter = rate;
        const int16_t next = LfoStepWave(static_cast<uint8_t>(std::min<int>(wave, 4)),
                                         delta, sign, static_cast<int16_t>(depth));
        if (next != delta) {
            delta = next;
            return true;
        }
    } else {
        counter--;
    }
    return false;
}

int8_t LfoToAttnDelta(int16_t mod) {
    int16_t d = static_cast<int16_t>(mod / 16);
    d = std::clamp<int16_t>(d, -15, 15);
    return static_cast<int8_t>(-d);
}

void ResolveLfoAlgo(uint8_t algo,
                    int16_t l1,
                    int16_t l2,
                    int16_t& pitch_delta,
                    int8_t& attn_delta) {
    const int16_t mix = std::clamp<int16_t>(static_cast<int16_t>(l1 + l2), -255, 255);
    switch (algo & 0x07) {
    default:
    case 0:
        pitch_delta = 0;
        attn_delta = 0;
        break;
    case 1:
        pitch_delta = l2;
        attn_delta = LfoToAttnDelta(l1);
        break;
    case 2:
        pitch_delta = mix;
        attn_delta = LfoToAttnDelta(mix);
        break;
    case 3:
        pitch_delta = l2;
        attn_delta = LfoToAttnDelta(mix);
        break;
    case 4:
        pitch_delta = mix;
        attn_delta = LfoToAttnDelta(l1);
        break;
    case 5:
        pitch_delta = 0;
        attn_delta = LfoToAttnDelta(mix);
        break;
    case 6:
        pitch_delta = mix;
        attn_delta = 0;
        break;
    case 7:
        pitch_delta = static_cast<int16_t>(mix / 2);
        attn_delta = 0;
        break;
    }
}
}

InstrumentPlayer::InstrumentPlayer(EngineHub* hub, QObject* parent)
    : QObject(parent),
      hub_(hub)
{
    timer_ = new QTimer(this);
    timer_->setInterval(1000 / 60);  // 60 fps, same as driver
    connect(timer_, &QTimer::timeout, this, &InstrumentPlayer::tick);
}

void InstrumentPlayer::play(const ngpc::BgmInstrumentDef& def, uint16_t divider, uint8_t tone_ch) {
    stop();

    // Disable Z80 stepping — instrument preview writes directly to PSG.
    if (hub_) hub_->set_step_z80(false);

    if (!hub_ || !hub_->ensure_audio_running(44100)) {
        return;
    }

    // Only clear the lane used by preview; do not globally mute PSG state.
    if (def.mode == 1) {
        psg_helpers::DirectSilenceNoise(hub_->engine());
    } else {
        psg_helpers::DirectSilenceTone(hub_->engine(), std::min<uint8_t>(tone_ch, 2));
    }

    def_ = def;
    base_div_ = divider;
    base_attn_ = def.attn;
    tone_ch_ = std::min<uint8_t>(tone_ch, 2);

    // Resolve curves
    {
        const auto curves = ngpc::FactoryEnvCurves();
        if (def.env_curve_id < static_cast<uint8_t>(curves.size())) {
            env_curve_ = curves[def.env_curve_id].steps;
        } else {
            env_curve_.clear();
        }
    }
    {
        const auto curves = ngpc::FactoryPitchCurves();
        if (def.pitch_curve_id < static_cast<uint8_t>(curves.size())) {
            pitch_curve_ = curves[def.pitch_curve_id].steps;
        } else {
            pitch_curve_.clear();
        }
    }

    // Initialize envelope/ADSR state (mirrors driver note-on behavior)
    if (def_.adsr_on) {
        attn_cur_ = 15;          // start silent, attack ramps down
        adsr_phase_ = 1;         // ATK
        adsr_counter_ = def_.adsr_attack;
    } else {
        attn_cur_ = base_attn_;
        adsr_phase_ = 0;
        adsr_counter_ = 0;
    }
    env_counter_ = std::max<uint8_t>(def.env_speed, 1);
    env_index_ = 0;

    // Initialize pitch curve state
    pitch_counter_ = std::max<uint8_t>(def.env_speed, 1);  // pitch uses env_speed too
    pitch_index_ = 0;
    pitch_offset_ = 0;
    macro_step_ = 0;
    macro_counter_ = 0;
    macro_active_ = false;
    macro_pitch_ = 0;
    {
        const auto& macros = MacroDefs();
        if (def_.macro_id < static_cast<uint8_t>(macros.size()) &&
            !macros[def_.macro_id].steps.empty()) {
            const auto& step0 = macros[def_.macro_id].steps[0];
            if (step0.frames > 0) {
                macro_active_ = true;
                macro_counter_ = step0.frames;
                macro_pitch_ = step0.pitch_delta;
                if (!def_.adsr_on) {
                    int16_t attn = static_cast<int16_t>(base_attn_) +
                                   static_cast<int16_t>(step0.attn_delta);
                    attn = std::clamp<int16_t>(attn, 0, 15);
                    attn_cur_ = static_cast<uint8_t>(attn);
                }
            }
        }
    }

    // Initialize vibrato state
    vib_delay_counter_ = def.vib_delay;
    vib_counter_ = std::max<uint8_t>(def.vib_speed, 1);
    vib_dir_ = 1;

    // Initialize LFO state
    lfo_hold_counter_ = def.lfo_hold;
    lfo_counter_ = def.lfo_rate;
    lfo_sign_ = 1;
    lfo_delta_ = 0;
    lfo2_hold_counter_ = def.lfo2_hold;
    lfo2_counter_ = def.lfo2_rate;
    lfo2_sign_ = 1;
    lfo2_delta_ = 0;
    lfo_pitch_delta_ = 0;
    lfo_attn_delta_ = 0;

    // Initialize sweep state
    tone_div_ = base_div_;
    sweep_counter_ = std::max<uint8_t>(def.sweep_speed, 1);
    sweep_active_ = (def.sweep_on != 0 && def.sweep_step != 0);

    silent_frames_ = 0;
    playing_ = true;

    // Write initial PSG state
    write_psg();
    timer_->start();
}

void InstrumentPlayer::stop() {
    if (!playing_) {
        return;
    }
    timer_->stop();
    playing_ = false;
    silence();
    emit stopped();
}

void InstrumentPlayer::note_off() {
    if (!playing_) return;

    // Mirror TrackerVoice behavior: release phase only exists for ADSR with non-zero release.
    if (def_.adsr_on && def_.adsr_release > 0) {
        if (adsr_phase_ != 4) {
            adsr_phase_ = 4; // REL
            adsr_counter_ = def_.adsr_release;
        }
        return;
    }

    stop();
}

bool InstrumentPlayer::is_playing() const {
    return playing_;
}

void InstrumentPlayer::tick() {
    if (!playing_ || !hub_ || !hub_->engine_ready()) {
        stop();
        return;
    }

    bool dirty = false;

    // === MACRO TICK ===
    if (macro_active_) {
        if (macro_counter_ == 0) {
            const auto& macros = MacroDefs();
            macro_step_++;
            if (def_.macro_id >= static_cast<uint8_t>(macros.size()) ||
                macro_step_ >= static_cast<uint8_t>(macros[def_.macro_id].steps.size())) {
                macro_active_ = false;
            } else {
                const auto& step = macros[def_.macro_id].steps[macro_step_];
                if (step.frames == 0) {
                    macro_active_ = false;
                } else {
                    macro_counter_ = step.frames;
                    macro_pitch_ = step.pitch_delta;
                    if (!def_.adsr_on) {
                        int16_t attn = static_cast<int16_t>(base_attn_) +
                                       static_cast<int16_t>(step.attn_delta);
                        attn = std::clamp<int16_t>(attn, 0, 15);
                        if (attn_cur_ != static_cast<uint8_t>(attn)) {
                            attn_cur_ = static_cast<uint8_t>(attn);
                            dirty = true;
                        }
                    }
                }
            }
        }
        if (macro_active_ && macro_counter_ > 0) {
            macro_counter_--;
        }
    }

    // === PITCH CURVE TICK ===
    if (!pitch_curve_.empty()) {
        if (pitch_counter_ == 0) {
            uint8_t idx = pitch_index_;
            if (idx >= static_cast<uint8_t>(pitch_curve_.size())) {
                idx = static_cast<uint8_t>(pitch_curve_.size() - 1);  // clamp to last
            } else {
                pitch_index_++;
            }
            pitch_offset_ = pitch_curve_[idx];
            pitch_counter_ = std::max<uint8_t>(def_.env_speed, 1);
            dirty = true;
        } else {
            pitch_counter_--;
        }
    }

    // === ADSR TICK (replaces legacy envelope when active) ===
    if (def_.adsr_on && adsr_phase_ > 0) {
        switch (adsr_phase_) {
        case 1: // ATK: ramp 15 -> base_attn_ (louder)
            if (def_.adsr_attack == 0) {
                attn_cur_ = base_attn_;
                adsr_phase_ = 2;
                adsr_counter_ = def_.adsr_decay;
                dirty = true;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ > base_attn_) {
                    attn_cur_--;
                    dirty = true;
                }
                if (attn_cur_ <= base_attn_) {
                    attn_cur_ = base_attn_;
                    adsr_phase_ = 2;
                    adsr_counter_ = def_.adsr_decay;
                } else {
                    adsr_counter_ = def_.adsr_attack;
                }
            } else {
                adsr_counter_--;
            }
            break;
        case 2: { // DEC: ramp base_attn_ -> sustain (quieter)
            uint8_t sus = def_.adsr_sustain < base_attn_ ? base_attn_ : def_.adsr_sustain;
            if (def_.adsr_decay == 0 || sus <= base_attn_) {
                attn_cur_ = sus;
                adsr_phase_ = 3;
                adsr_counter_ = def_.adsr_sustain_rate;
                dirty = true;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ < sus) {
                    attn_cur_++;
                    dirty = true;
                }
                if (attn_cur_ >= sus) {
                    attn_cur_ = sus;
                    adsr_phase_ = 3;
                    adsr_counter_ = def_.adsr_sustain_rate;
                } else {
                    adsr_counter_ = def_.adsr_decay;
                }
            } else {
                adsr_counter_--;
            }
            break;
        }
        case 3: // SUS: optional sustain-rate fade
            if (def_.adsr_sustain_rate > 0) {
                if (adsr_counter_ == 0) {
                    if (attn_cur_ < 15) {
                        attn_cur_++;
                        dirty = true;
                    }
                    if (attn_cur_ >= 15) {
                        adsr_phase_ = 0;
                    } else {
                        adsr_counter_ = def_.adsr_sustain_rate;
                    }
                } else {
                    adsr_counter_--;
                }
            }
            break;
        case 4: // REL: ramp current -> 15 (silent)
            if (def_.adsr_release == 0) {
                attn_cur_ = 15;
                adsr_phase_ = 0;
                dirty = true;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ < 15) {
                    attn_cur_++;
                    dirty = true;
                }
                if (attn_cur_ >= 15) {
                    adsr_phase_ = 0;
                } else {
                    adsr_counter_ = def_.adsr_release;
                }
            } else {
                adsr_counter_--;
            }
            break;
        }
    }
    // === Legacy envelope tick ===
    else if (def_.env_on) {
        if (env_counter_ == 0) {
            if (!env_curve_.empty()) {
                // Curve-based envelope
                uint8_t idx = env_index_;
                if (idx >= static_cast<uint8_t>(env_curve_.size())) {
                    idx = static_cast<uint8_t>(env_curve_.size() - 1);
                } else {
                    env_index_++;
                }
                int16_t attn = static_cast<int16_t>(base_attn_) + static_cast<int16_t>(env_curve_[idx]);
                attn = std::clamp<int16_t>(attn, 0, 15);
                if (attn_cur_ != static_cast<uint8_t>(attn)) {
                    attn_cur_ = static_cast<uint8_t>(attn);
                    dirty = true;
                }
            } else {
                // Simple linear fade-out
                if (attn_cur_ < 15) {
                    uint8_t next = static_cast<uint8_t>(attn_cur_ + std::max<uint8_t>(def_.env_step, 1));
                    if (next > 15) next = 15;
                    attn_cur_ = next;
                    dirty = true;
                }
            }
            env_counter_ = std::max<uint8_t>(def_.env_speed, 1);
        } else {
            env_counter_--;
        }
    }

    // === SWEEP TICK ===
    if (def_.mode == 0 && sweep_active_) {
        if (sweep_counter_ == 0) {
            int32_t nd = static_cast<int32_t>(tone_div_) + static_cast<int32_t>(def_.sweep_step);
            nd = std::clamp<int32_t>(nd, 1, 1023);
            tone_div_ = static_cast<uint16_t>(nd);
            sweep_counter_ = std::max<uint8_t>(def_.sweep_speed, 1);
            dirty = true;
            // Check if sweep reached end
            if (def_.sweep_step > 0) {
                if (tone_div_ >= def_.sweep_end) sweep_active_ = false;
            } else {
                if (tone_div_ <= def_.sweep_end) sweep_active_ = false;
            }
        } else {
            sweep_counter_--;
        }
    }

    // === VIBRATO TICK ===
    if (def_.mode == 0 && def_.vib_on && def_.vib_depth > 0) {
        if (vib_delay_counter_ > 0) {
            vib_delay_counter_--;
            if (vib_delay_counter_ == 0) {
                vib_counter_ = std::max<uint8_t>(def_.vib_speed, 1);
                vib_dir_ = 1;
                dirty = true;
            }
        } else {
            if (vib_counter_ == 0) {
                vib_dir_ = (vib_dir_ < 0) ? 1 : -1;
                vib_counter_ = std::max<uint8_t>(def_.vib_speed, 1);
                dirty = true;
            } else {
                vib_counter_--;
            }
        }
    }

    if (def_.mode == 0) {
        const int16_t prev_pitch = lfo_pitch_delta_;
        const int8_t prev_attn = lfo_attn_delta_;
        bool lfo_dirty = false;
        if (LfoTick(def_.lfo_on != 0, def_.lfo_wave, def_.lfo_rate, def_.lfo_depth,
                    lfo_hold_counter_, lfo_counter_, lfo_sign_, lfo_delta_)) {
            lfo_dirty = true;
        }
        if (LfoTick(def_.lfo2_on != 0, def_.lfo2_wave, def_.lfo2_rate, def_.lfo2_depth,
                    lfo2_hold_counter_, lfo2_counter_, lfo2_sign_, lfo2_delta_)) {
            lfo_dirty = true;
        }
        ResolveLfoAlgo(def_.lfo_algo, lfo_delta_, lfo2_delta_, lfo_pitch_delta_, lfo_attn_delta_);
        if (lfo_pitch_delta_ != prev_pitch || lfo_attn_delta_ != prev_attn) {
            lfo_dirty = true;
        }
        if (lfo_dirty) {
            dirty = true;
        }
    } else if (lfo_pitch_delta_ != 0 || lfo_attn_delta_ != 0) {
        lfo_pitch_delta_ = 0;
        lfo_attn_delta_ = 0;
        dirty = true;
    }

    if (dirty) {
        write_psg();
    }

    // Auto-stop: if attenuation is max (15 = silent) for a while, stop
    if (attn_cur_ >= 15) {
        silent_frames_++;
        if (silent_frames_ > 30) {  // ~0.5s of silence
            stop();
        }
    } else {
        silent_frames_ = 0;
    }
}

void InstrumentPlayer::write_psg() {
    if (!hub_ || !hub_->engine_ready()) {
        return;
    }
    uint8_t final_attn = attn_cur_;
    if (lfo_attn_delta_ != 0) {
        int la = static_cast<int>(final_attn) + static_cast<int>(lfo_attn_delta_);
        final_attn = static_cast<uint8_t>(std::clamp(la, 0, 15));
    }

    if (def_.mode == 1) {
        // Noise mode
        const uint8_t cfg = static_cast<uint8_t>(def_.noise_config & 0x07);
        const uint8_t rate = static_cast<uint8_t>(cfg & 0x03);
        const uint8_t type = static_cast<uint8_t>((cfg >> 2) & 0x01);
        psg_helpers::DirectNoise(hub_->engine(), rate, type, final_attn);
    } else {
        // Tone mode: compute final divider with all effects
        uint16_t div = tone_div_;

        // Apply macro pitch
        if (macro_pitch_ != 0) {
            int32_t md = static_cast<int32_t>(div) + static_cast<int32_t>(macro_pitch_);
            md = std::clamp<int32_t>(md, 1, 1023);
            div = static_cast<uint16_t>(md);
        }

        // Apply pitch curve offset
        if (pitch_offset_ != 0) {
            int16_t md = static_cast<int16_t>(div) + pitch_offset_;
            md = std::clamp<int16_t>(md, 1, 1023);
            div = static_cast<uint16_t>(md);
        }

        // Apply LFO
        if (lfo_pitch_delta_ != 0) {
            int16_t ld = static_cast<int16_t>(div) + lfo_pitch_delta_;
            ld = std::clamp<int16_t>(ld, 1, 1023);
            div = static_cast<uint16_t>(ld);
        }

        // Apply vibrato
        if (def_.vib_on && def_.vib_depth > 0 && vib_delay_counter_ == 0) {
            int16_t vib_delta = static_cast<int16_t>(def_.vib_depth) * static_cast<int16_t>(vib_dir_);
            int16_t vd = static_cast<int16_t>(div) + vib_delta;
            vd = std::clamp<int16_t>(vd, 1, 1023);
            div = static_cast<uint16_t>(vd);
        }

        psg_helpers::DirectToneCh(hub_->engine(), tone_ch_, div, final_attn);
    }
}

void InstrumentPlayer::silence() {
    if (!hub_ || !hub_->engine_ready()) {
        return;
    }
    if (def_.mode == 1) {
        psg_helpers::DirectSilenceNoise(hub_->engine());
    } else {
        psg_helpers::DirectSilenceTone(hub_->engine(), tone_ch_);
    }
}
