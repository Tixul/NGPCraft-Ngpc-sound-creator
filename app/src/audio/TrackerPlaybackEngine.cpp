#include "audio/TrackerPlaybackEngine.h"
#include "models/InstrumentStore.h"
#include "ngpc/instrument.h"

#include <algorithm>
#include <cmath>

namespace {
uint16_t transpose_divider_by_semitones(uint16_t base_div, uint8_t semitones) {
    if (base_div < 1) return 1;
    const double ratio = std::pow(2.0, -static_cast<double>(semitones) / 12.0);
    const int d = static_cast<int>(std::lround(static_cast<double>(base_div) * ratio));
    return static_cast<uint16_t>(std::clamp(d, 1, 1023));
}
}

// ============================================================
// Construction
// ============================================================

TrackerPlaybackEngine::TrackerPlaybackEngine(QObject* parent)
    : QObject(parent)
{
}

void TrackerPlaybackEngine::set_document(TrackerDocument* doc) {
    doc_ = doc;
}

void TrackerPlaybackEngine::set_instrument_store(InstrumentStore* store) {
    store_ = store;
}

// ============================================================
// Transport
// ============================================================

void TrackerPlaybackEngine::start(int from_row) {
    if (!doc_) return;
    playing_ = true;
    current_row_ = std::clamp(from_row, 0, doc_->length() - 1);
    tick_counter_ = 0;

    for (auto& v : voices_) v.note_off();
    for (auto& fs : fx_state_) fs.reset();
    noise_val_ = 0;
    fade_speed_ = 0;
    fade_counter_ = 0;
    fade_attn_ = 0;

    process_row(current_row_);
}

void TrackerPlaybackEngine::stop() {
    playing_ = false;
    for (auto& v : voices_) v.note_off();
    for (auto& fs : fx_state_) fs.reset();
    fade_speed_ = 0;
    fade_counter_ = 0;
    fade_attn_ = 0;
}

void TrackerPlaybackEngine::set_ticks_per_row(int tpr) {
    ticks_per_row_ = std::clamp(tpr, 1, 32);
}

void TrackerPlaybackEngine::set_loop_range(int start, int end) {
    loop_start_ = start;
    loop_end_ = end;
}

void TrackerPlaybackEngine::clear_loop_range() {
    loop_start_ = -1;
    loop_end_ = -1;
}

// ============================================================
// Tick — returns true when a new row starts
// ============================================================

bool TrackerPlaybackEngine::tick() {
    if (!playing_ || !doc_) return false;

    // Advance voices and effects
    for (int ch = 0; ch < 4; ++ch) {
        voices_[ch].tick();
        tick_fx(ch);
    }

    // Global fade processing — matches driver Bgm_Update (sounds.c:2189-2203).
    // When fade reaches max attenuation, stop playback entirely like the driver.
    if (fade_speed_ > 0) {
        if (fade_counter_ == 0) {
            if (fade_attn_ < 15) {
                fade_attn_++;
            }
            if (fade_attn_ >= 15) {
                stop();
                return false;
            }
            fade_counter_ = fade_speed_;
        } else {
            fade_counter_--;
        }
    }

    tick_counter_++;
    if (tick_counter_ >= ticks_per_row_) {
        tick_counter_ = 0;
        current_row_++;
        if (loop_start_ >= 0 && loop_end_ >= 0) {
            // Selection loop: wrap within range
            if (current_row_ > loop_end_) {
                current_row_ = loop_start_;
            }
        } else if (current_row_ >= doc_->length()) {
            current_row_ = 0;
            emit pattern_finished();
        }
        process_row(current_row_);
        emit row_changed(current_row_);
        return true;
    }
    return false;
}

// ============================================================
// Channel output (what to write to PSG)
// ============================================================

TrackerPlaybackEngine::ChannelOutput TrackerPlaybackEngine::channel_output(int ch) const {
    ChannelOutput out;
    if (ch < 0 || ch >= 4) return out;

    const auto& v = voices_[static_cast<size_t>(ch)];
    const auto& fs = fx_state_[static_cast<size_t>(ch)];

    out.active = v.active();

    if (!out.active) return out;

    // Base values from voice
    uint16_t divider = v.final_divider();
    uint8_t attn = v.final_attn();

    // Apply effect overrides
    if (fs.fx == 0x0 && fs.param != 0) {
        // Arpeggio: use phase divider
        if (fs.arp_dividers[fs.arp_phase] > 0)
            divider = fs.arp_dividers[fs.arp_phase];
    }
    if (fs.fx == 0x1 || fs.fx == 0x2) {
        // Pitch slide: apply offset
        int32_t d = static_cast<int32_t>(divider) + static_cast<int32_t>(fs.pitch_offset);
        divider = static_cast<uint16_t>(std::clamp(d, 1, 1023));
    }
    if (fs.fx == 0x3 && fs.porta_active) {
        // Portamento: use interpolated divider
        divider = fs.porta_current;
    }
    if (fs.fx == 0xA && fs.vol_override) {
        // Volume slide: use overridden volume
        attn = fs.vol_current;
    }

    // Apply pitch bend (4xx)
    if (fs.pitch_bend != 0) {
        int32_t d = static_cast<int32_t>(divider) + static_cast<int32_t>(fs.pitch_bend);
        divider = static_cast<uint16_t>(std::clamp(d, 1, 1023));
    }

    // Apply expression
    if (fs.expression > 0) {
        int ea = static_cast<int>(attn) + static_cast<int>(fs.expression);
        attn = static_cast<uint8_t>(std::min(ea, 15));
    }

    // Apply global fade
    if (fade_attn_ > 0) {
        int fa = static_cast<int>(attn) + static_cast<int>(fade_attn_);
        attn = static_cast<uint8_t>(std::min(fa, 15));
    }

    out.divider = divider;
    out.attn = attn;
    if (ch == 3) {
        out.noise_val = static_cast<uint8_t>(noise_val_ & 0x07);
    }
    return out;
}

// ============================================================
// Mute
// ============================================================

void TrackerPlaybackEngine::set_channel_muted(int ch, bool muted) {
    if (ch >= 0 && ch < 4) channel_muted_[ch] = muted;
}

bool TrackerPlaybackEngine::is_channel_muted(int ch) const {
    if (ch >= 0 && ch < 4) return channel_muted_[ch];
    return false;
}

// ============================================================
// Process row
// ============================================================

void TrackerPlaybackEngine::process_row(int row) {
    if (!doc_) return;

    const auto env_curves = ngpc::FactoryEnvCurves();
    const auto pitch_curves = ngpc::FactoryPitchCurves();
    auto trigger_note_on = [&](int ch, const TrackerCell& cell) {
        int inst_idx = cell.instrument;
        ngpc::BgmInstrumentDef def;
        if (store_ && inst_idx < store_->count()) {
            def = store_->at(inst_idx).def;
        }

        if (ch == 3) def.mode = 1;

        std::vector<int8_t> ec;
        if (def.env_curve_id < static_cast<uint8_t>(env_curves.size())) {
            ec = env_curves[def.env_curve_id].steps;
        }
        std::vector<int16_t> pc;
        if (def.pitch_curve_id < static_cast<uint8_t>(pitch_curves.size())) {
            pc = pitch_curves[def.pitch_curve_id].steps;
        }

        const uint16_t divider = midi_to_divider(cell.note);
        voices_[ch].note_on(def, ec, pc, divider, cell.attn);
        if (ch == 3) {
            noise_val_ = midi_note_to_noise_val(cell.note);
        }
    };

    for (int ch = 0; ch < 4; ++ch) {
        const auto& c = doc_->cell(ch, row);

        // Handle Bxx (set speed) immediately
        if (c.fx == 0xB && c.fx_param > 0) {
            ticks_per_row_ = std::clamp(static_cast<int>(c.fx_param), 1, 32);
            emit speed_changed(ticks_per_row_);
        }

        // Handle Exx (host commands) immediately
        if (c.fx == 0xE) {
            uint8_t sub = (c.fx_param >> 4) & 0x0F;
            uint8_t val = c.fx_param & 0x0F;
            if (sub == 0) {
                // E0x: fade out (speed x; 0 = cancel fade)
                if (val == 0) {
                    fade_speed_ = 0;
                    fade_counter_ = 0;
                    fade_attn_ = 0;
                } else {
                    fade_speed_ = val;
                    fade_counter_ = val;
                }
            } else if (sub == 1) {
                // E1x: set speed (like Bxx)
                int spd = (val == 0) ? 16 : val;
                ticks_per_row_ = std::clamp(spd, 1, 32);
                emit speed_changed(ticks_per_row_);
            }
        }

        // Handle Fxx (expression) immediately — sticky per-channel
        if (c.fx == 0xF) {
            uint8_t expr = c.fx_param;
            if (expr > 15) expr = 15;
            fx_state_[ch].expression = expr;
        }

        // Handle 4xx (pitch bend) immediately — sticky per-channel
        if (c.fx == 0x4) {
            // Param treated as signed byte: 00=no bend, 01-7F=positive, 80-FF=negative
            int8_t sb = static_cast<int8_t>(c.fx_param);
            fx_state_[ch].pitch_bend = static_cast<int16_t>(sb);
        }

        // Handle Dxx (note delay): defer note trigger
        if (c.fx == 0xD && c.fx_param > 0) {
            fx_state_[ch].reset();
            fx_state_[ch].fx = 0xD;
            fx_state_[ch].param = c.fx_param;
            fx_state_[ch].delay_countdown = c.fx_param;
            fx_state_[ch].delayed_cell = c;
            fx_state_[ch].note_delayed = true;
            continue;
        }

        // Handle 3xx (portamento): don't do normal note_on
        if (c.fx == 0x3) {
            if (c.is_note_on()) {
                // XM-style fallback: if no previous note is active, start the note first
                // so 3xx does not produce silence.
                if (!voices_[ch].active()) {
                    trigger_note_on(ch, c);
                }
                init_fx(ch, c);
            } else if (c.is_note_off()) {
                voices_[ch].note_off();
                fx_state_[ch].reset();
            } else {
                init_fx(ch, c);
            }
            continue;
        }

        // Normal note processing
        if (c.is_note_off()) {
            voices_[ch].note_off();
            fx_state_[ch].reset();
        } else if (c.is_note_on()) {
            trigger_note_on(ch, c);
            init_fx(ch, c);
        } else if (c.has_fx()) {
            init_fx(ch, c);
        }
    }
}

// ============================================================
// Effect engine
// ============================================================

void TrackerPlaybackEngine::init_fx(int ch, const TrackerCell& cell) {
    auto& fs = fx_state_[ch];
    uint8_t saved_expr = fs.expression; // expression is sticky across effects
    int16_t saved_bend = fs.pitch_bend; // pitch bend is sticky across effects
    bool saved_porta_active = fs.porta_active;
    uint16_t saved_porta_target = fs.porta_target;
    uint16_t saved_porta_current = fs.porta_current;
    uint8_t saved_porta_speed = fs.porta_speed;
    fs.reset();
    fs.expression = saved_expr;
    fs.pitch_bend = saved_bend;
    fs.fx = cell.fx;
    fs.param = cell.fx_param;

    if (cell.fx == 0 && cell.fx_param == 0) return;

    uint8_t hi = (cell.fx_param >> 4) & 0x0F;
    uint8_t lo = cell.fx_param & 0x0F;

    switch (cell.fx) {
    case 0x0: {
        if (hi == 0 && lo == 0) break;
        uint16_t base_div = voices_[ch].active() ? voices_[ch].final_divider() : 0;
        fs.arp_dividers[0] = base_div;
        if (cell.is_note_on()) {
            uint8_t base_note = cell.note;
            fs.arp_dividers[0] = midi_to_divider(base_note);
            fs.arp_dividers[1] = midi_to_divider(
                static_cast<uint8_t>(std::clamp(static_cast<int>(base_note) + hi, 1, 127)));
            fs.arp_dividers[2] = midi_to_divider(
                static_cast<uint8_t>(std::clamp(static_cast<int>(base_note) + lo, 1, 127)));
        } else if (voices_[ch].active()) {
            fs.arp_dividers[0] = base_div;
            fs.arp_dividers[1] = transpose_divider_by_semitones(base_div, hi);
            fs.arp_dividers[2] = transpose_divider_by_semitones(base_div, lo);
        }
        fs.arp_phase = 0;
        break;
    }
    case 0x1:
        fs.pitch_offset = 0;
        break;
    case 0x2:
        fs.pitch_offset = 0;
        break;
    case 0x3: {
        fs.porta_speed = (cell.fx_param != 0) ? cell.fx_param : saved_porta_speed;
        if (cell.is_note_on()) {
            fs.porta_target = midi_to_divider(cell.note);
            if (voices_[ch].active()) {
                fs.porta_current = voices_[ch].final_divider();
                fs.porta_active = (fs.porta_current != fs.porta_target);
            } else {
                fs.porta_current = fs.porta_target;
                fs.porta_active = false;
            }
        } else {
            // 3xx effect-only rows should keep sliding toward the previous target.
            fs.porta_active = saved_porta_active;
            fs.porta_target = saved_porta_target;
            fs.porta_current = saved_porta_current;
        }
        break;
    }
    case 0xA:
        if (hi > 0)
            fs.vol_delta = -static_cast<int8_t>(hi);
        else
            fs.vol_delta = static_cast<int8_t>(lo);
        fs.vol_current = voices_[ch].active() ? voices_[ch].final_attn() : 0;
        fs.vol_override = true;
        break;
    case 0xB:
        break;
    case 0xC:
        if (cell.fx_param == 0) {
            // C00: immediate cut at row start.
            voices_[ch].note_off();
            fs.fx = 0;
            fs.param = 0;
            fs.cut_countdown = -1;
        } else {
            fs.cut_countdown = cell.fx_param;
        }
        break;
    case 0xD:
        break;
    case 0xE:
        // Host commands: handled immediately in process_row()
        break;
    case 0xF:
        // Expression: handled immediately in process_row()
        break;
    default:
        break;
    }
}

void TrackerPlaybackEngine::tick_fx(int ch) {
    auto& fs = fx_state_[ch];
    if (fs.fx == 0 && fs.param == 0) return;

    switch (fs.fx) {
    case 0x0:
        if (fs.param != 0) {
            fs.arp_phase = (fs.arp_phase + 1) % 3;
        }
        break;
    case 0x1:
        fs.pitch_offset -= static_cast<int16_t>(fs.param);
        break;
    case 0x2:
        fs.pitch_offset += static_cast<int16_t>(fs.param);
        break;
    case 0x3:
        if (fs.porta_active && fs.porta_speed > 0) {
            if (fs.porta_current < fs.porta_target) {
                int32_t next = static_cast<int32_t>(fs.porta_current) + fs.porta_speed;
                if (next >= fs.porta_target) {
                    fs.porta_current = fs.porta_target;
                    fs.porta_active = false;
                } else {
                    fs.porta_current = static_cast<uint16_t>(next);
                }
            } else if (fs.porta_current > fs.porta_target) {
                int32_t next = static_cast<int32_t>(fs.porta_current) - fs.porta_speed;
                if (next <= fs.porta_target) {
                    fs.porta_current = fs.porta_target;
                    fs.porta_active = false;
                } else {
                    fs.porta_current = static_cast<uint16_t>(next);
                }
            }
        }
        break;
    case 0xA:
        if (fs.vol_override) {
            int16_t next = static_cast<int16_t>(fs.vol_current) + fs.vol_delta;
            fs.vol_current = static_cast<uint8_t>(std::clamp(static_cast<int>(next), 0, 15));
        }
        break;
    case 0xC:
        if (fs.cut_countdown > 0) {
            fs.cut_countdown--;
            if (fs.cut_countdown == 0) {
                voices_[ch].note_off();
            }
        }
        break;
    case 0xD:
        if (fs.note_delayed && fs.delay_countdown > 0) {
            fs.delay_countdown--;
            if (fs.delay_countdown == 0) {
                fs.note_delayed = false;
                const auto& dc = fs.delayed_cell;
                if (dc.is_note_on()) {
                    int inst_idx = dc.instrument;
                    ngpc::BgmInstrumentDef def;
                    if (store_ && inst_idx < store_->count()) {
                        def = store_->at(inst_idx).def;
                    }
                    if (ch == 3) def.mode = 1;

                    const auto env_curves = ngpc::FactoryEnvCurves();
                    const auto pitch_curves = ngpc::FactoryPitchCurves();
                    std::vector<int8_t> ec;
                    if (def.env_curve_id < static_cast<uint8_t>(env_curves.size()))
                        ec = env_curves[def.env_curve_id].steps;
                    std::vector<int16_t> pc;
                    if (def.pitch_curve_id < static_cast<uint8_t>(pitch_curves.size()))
                        pc = pitch_curves[def.pitch_curve_id].steps;

                    uint16_t divider = midi_to_divider(dc.note);
                    voices_[ch].note_on(def, ec, pc, divider, dc.attn);
                    if (ch == 3) {
                        noise_val_ = midi_note_to_noise_val(dc.note);
                    }
                }
            }
        }
        break;
    default:
        break;
    }
}

// ============================================================
// Utility
// ============================================================

uint16_t TrackerPlaybackEngine::midi_to_divider(uint8_t midi_note) {
    if (midi_note == 0 || midi_note > 127) return 1;
    // Tracker note ids are 1-based with C-0 at 1.
    // Convert to standard MIDI numbering (C-0 = 12) before frequency mapping.
    const double midi_equiv = static_cast<double>(midi_note) + 11.0;
    double freq = 440.0 * std::pow(2.0, (midi_equiv - 69.0) / 12.0);
    double div = 3072000.0 / (32.0 * freq);
    int d = static_cast<int>(std::round(div));
    return static_cast<uint16_t>(std::clamp(d, 1, 1023));
}

uint8_t TrackerPlaybackEngine::midi_note_to_noise_val(uint8_t midi_note) {
    if (midi_note < 1 || midi_note > 127) {
        return 0;
    }
    return static_cast<uint8_t>((midi_note - 1) & 0x07);
}

TrackerPlaybackEngine::NoiseConfig TrackerPlaybackEngine::decode_noise_val(uint8_t noise_val) {
    return { static_cast<uint8_t>(noise_val & 0x03),
             static_cast<uint8_t>((noise_val >> 2) & 0x01) };
}

QString TrackerPlaybackEngine::noise_display_name(uint8_t noise_val) {
    static const char* kNames[8] = {
        "P.H", "P.M", "P.L", "P.T",
        "W.H", "W.M", "W.L", "W.T"
    };
    return QString::fromLatin1(kNames[noise_val & 0x07]);
}
