#include "models/TrackerDocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

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

// ============================================================
// TrackerVoice
// ============================================================

void TrackerVoice::note_on(const ngpc::BgmInstrumentDef& def,
                           const std::vector<int8_t>& env_curve,
                           const std::vector<int16_t>& pitch_curve,
                           uint16_t divider, uint8_t attn_override) {
    active_ = true;
    def_ = def;
    env_curve_ = env_curve;
    pitch_curve_ = pitch_curve;
    base_div_ = divider;
    base_attn_ = (attn_override != 0xFF) ? attn_override : def.attn;
    if (def.adsr_on) {
        attn_cur_ = 15;
        adsr_phase_ = 1; // ATK
        adsr_counter_ = def.adsr_attack;
    } else {
        attn_cur_ = base_attn_;
        adsr_phase_ = 0;
        adsr_counter_ = 0;
    }
    env_counter_ = std::max<uint8_t>(def.env_speed, 1);
    env_index_ = 0;
    pitch_counter_ = std::max<uint8_t>(def.env_speed, 1);
    pitch_index_ = 0;
    pitch_offset_ = 0;
    macro_step_ = 0;
    macro_counter_ = 0;
    macro_active_ = false;
    macro_pitch_ = 0;
    {
        const auto& macros = MacroDefs();
        if (def.macro_id < static_cast<uint8_t>(macros.size()) &&
            !macros[def.macro_id].steps.empty()) {
            const auto& step0 = macros[def.macro_id].steps[0];
            if (step0.frames > 0) {
                macro_active_ = true;
                macro_counter_ = step0.frames;
                macro_pitch_ = step0.pitch_delta;
                if (!def.adsr_on) {
                    int16_t attn = static_cast<int16_t>(base_attn_) +
                                   static_cast<int16_t>(step0.attn_delta);
                    attn = std::clamp<int16_t>(attn, 0, 15);
                    attn_cur_ = static_cast<uint8_t>(attn);
                }
            }
        }
    }
    vib_delay_counter_ = def.vib_delay;
    vib_counter_ = std::max<uint8_t>(def.vib_speed, 1);
    vib_dir_ = 1;
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
    tone_div_ = base_div_;
    sweep_counter_ = std::max<uint8_t>(def.sweep_speed, 1);
    sweep_active_ = (def.sweep_on != 0 && def.sweep_step != 0);
}

void TrackerVoice::note_off() {
    if (def_.adsr_on && def_.adsr_release > 0 && active_) {
        adsr_phase_ = 4; // REL
        adsr_counter_ = def_.adsr_release;
        // Stay active — tick() will deactivate when release completes
    } else {
        active_ = false;
        adsr_phase_ = 0;
    }
}

void TrackerVoice::tick() {
    if (!active_) return;

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
                        attn_cur_ = static_cast<uint8_t>(attn);
                    }
                }
            }
        }
        if (macro_active_ && macro_counter_ > 0) {
            macro_counter_--;
        }
    }

    // Use !empty() to match driver check (count > 0). A single-step
    // curve is valid and must be applied, not skipped.
    if (!pitch_curve_.empty()) {
        if (pitch_counter_ == 0) {
            uint8_t idx = pitch_index_;
            if (idx >= static_cast<uint8_t>(pitch_curve_.size())) {
                idx = static_cast<uint8_t>(pitch_curve_.size() - 1);
            } else {
                pitch_index_++;
            }
            pitch_offset_ = pitch_curve_[idx];
            pitch_counter_ = std::max<uint8_t>(def_.env_speed, 1);
        } else {
            pitch_counter_--;
        }
    }

    // ADSR state machine (replaces legacy env when active)
    if (def_.adsr_on && adsr_phase_ > 0) {
        switch (adsr_phase_) {
        case 1: // ATK: ramp 15 → base_attn_ (louder)
            if (def_.adsr_attack == 0) {
                attn_cur_ = base_attn_;
                adsr_phase_ = 2;
                adsr_counter_ = def_.adsr_decay;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ > base_attn_) attn_cur_--;
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
        case 2: { // DEC: ramp base_attn_ → sustain (quieter)
            uint8_t sus = std::max(def_.adsr_sustain, base_attn_);
            if (def_.adsr_decay == 0 || sus <= base_attn_) {
                attn_cur_ = sus;
                adsr_phase_ = 3;
                adsr_counter_ = def_.adsr_sustain_rate;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ < sus) attn_cur_++;
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
                    if (attn_cur_ < 15) attn_cur_++;
                    if (attn_cur_ >= 15) {
                        adsr_phase_ = 0;
                        active_ = false;
                    } else {
                        adsr_counter_ = def_.adsr_sustain_rate;
                    }
                } else {
                    adsr_counter_--;
                }
            }
            break;
        case 4: // REL: ramp cur → 15 (silent)
            if (def_.adsr_release == 0) {
                attn_cur_ = 15;
                adsr_phase_ = 0;
                active_ = false;
            } else if (adsr_counter_ == 0) {
                if (attn_cur_ < 15) attn_cur_++;
                if (attn_cur_ >= 15) {
                    adsr_phase_ = 0;
                    active_ = false;
                } else {
                    adsr_counter_ = def_.adsr_release;
                }
            } else {
                adsr_counter_--;
            }
            break;
        }
    } else if (def_.env_on) {
        if (env_counter_ == 0) {
            // Use !empty() to match driver check (count > 0). A single-step
            // curve is valid and must be applied, not skipped.
            if (!env_curve_.empty()) {
                uint8_t idx = env_index_;
                if (idx >= static_cast<uint8_t>(env_curve_.size())) {
                    idx = static_cast<uint8_t>(env_curve_.size() - 1);
                } else {
                    env_index_++;
                }
                int16_t attn = static_cast<int16_t>(base_attn_) + static_cast<int16_t>(env_curve_[idx]);
                attn = std::clamp<int16_t>(attn, 0, 15);
                attn_cur_ = static_cast<uint8_t>(attn);
            } else {
                if (attn_cur_ < 15) {
                    uint8_t next = static_cast<uint8_t>(attn_cur_ + std::max<uint8_t>(def_.env_step, 1));
                    if (next > 15) next = 15;
                    attn_cur_ = next;
                }
            }
            env_counter_ = std::max<uint8_t>(def_.env_speed, 1);
        } else {
            env_counter_--;
        }
    }

    if (def_.mode == 0 && sweep_active_) {
        if (sweep_counter_ == 0) {
            int32_t nd = static_cast<int32_t>(tone_div_) + static_cast<int32_t>(def_.sweep_step);
            nd = std::clamp<int32_t>(nd, 1, 1023);
            tone_div_ = static_cast<uint16_t>(nd);
            sweep_counter_ = std::max<uint8_t>(def_.sweep_speed, 1);
            if (def_.sweep_step > 0) {
                if (tone_div_ >= def_.sweep_end) sweep_active_ = false;
            } else {
                if (tone_div_ <= def_.sweep_end) sweep_active_ = false;
            }
        } else {
            sweep_counter_--;
        }
    }

    if (def_.mode == 0 && def_.vib_on && def_.vib_depth > 0) {
        if (vib_delay_counter_ > 0) {
            vib_delay_counter_--;
            if (vib_delay_counter_ == 0) {
                vib_counter_ = std::max<uint8_t>(def_.vib_speed, 1);
                vib_dir_ = 1;
            }
        } else {
            if (vib_counter_ == 0) {
                vib_dir_ = (vib_dir_ < 0) ? 1 : -1;
                vib_counter_ = std::max<uint8_t>(def_.vib_speed, 1);
            } else {
                vib_counter_--;
            }
        }
    }

    if (def_.mode == 0) {
        LfoTick(def_.lfo_on != 0, def_.lfo_wave, def_.lfo_rate, def_.lfo_depth,
                lfo_hold_counter_, lfo_counter_, lfo_sign_, lfo_delta_);
        LfoTick(def_.lfo2_on != 0, def_.lfo2_wave, def_.lfo2_rate, def_.lfo2_depth,
                lfo2_hold_counter_, lfo2_counter_, lfo2_sign_, lfo2_delta_);
        ResolveLfoAlgo(def_.lfo_algo, lfo_delta_, lfo2_delta_, lfo_pitch_delta_, lfo_attn_delta_);
    } else {
        lfo_pitch_delta_ = 0;
        lfo_attn_delta_ = 0;
    }
}

uint16_t TrackerVoice::final_divider() const {
    uint16_t div = tone_div_;
    // Batch non-vibrato pitch modifiers into a single delta then clamp once.
    // Matches driver BgmVoice_CommandFromState (sounds.c:878-901).
    {
        int16_t delta = 0;
        if (macro_pitch_ != 0)
            delta = static_cast<int16_t>(delta + macro_pitch_);
        if (pitch_offset_ != 0)
            delta = static_cast<int16_t>(delta + pitch_offset_);
        if (lfo_pitch_delta_ != 0)
            delta = static_cast<int16_t>(delta + lfo_pitch_delta_);
        if (delta != 0) {
            int32_t md = static_cast<int32_t>(div) + static_cast<int32_t>(delta);
            div = static_cast<uint16_t>(std::clamp<int32_t>(md, 1, 1023));
        }
    }
    // Vibrato applied separately (matches driver, sounds.c:903-911).
    if (def_.vib_on && def_.vib_depth > 0 && vib_delay_counter_ == 0) {
        int16_t vib_delta = static_cast<int16_t>(def_.vib_depth) * static_cast<int16_t>(vib_dir_);
        int32_t vd = static_cast<int32_t>(div) + static_cast<int32_t>(vib_delta);
        div = static_cast<uint16_t>(std::clamp<int32_t>(vd, 1, 1023));
    }
    return div;
}

uint8_t TrackerVoice::final_attn() const {
    int attn = static_cast<int>(attn_cur_) + static_cast<int>(lfo_attn_delta_);
    return static_cast<uint8_t>(std::clamp(attn, 0, 15));
}

// ============================================================
// TrackerDocument
// ============================================================

const TrackerCell TrackerDocument::kEmptyCell{};

TrackerDocument::TrackerDocument(QObject* parent)
    : QObject(parent)
{
    ensure_size();
}

void TrackerDocument::set_length(int len) {
    len = std::clamp(len, kMinLength, kMaxLength);
    if (len == length_) return;
    length_ = len;
    ensure_size();
    emit length_changed(length_);
}

const TrackerCell& TrackerDocument::cell(int ch, int row) const {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_)
        return kEmptyCell;
    return channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)];
}

void TrackerDocument::set_cell(int ch, int row, const TrackerCell& c) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)] = c;
    emit cell_changed(ch, row);
}

void TrackerDocument::set_note(int ch, int row, uint8_t note) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)].note = note;
    emit cell_changed(ch, row);
}

void TrackerDocument::set_instrument(int ch, int row, uint8_t inst) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    if (inst > kMaxInstrument) inst = kMaxInstrument;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)].instrument = inst;
    emit cell_changed(ch, row);
}

void TrackerDocument::set_attn(int ch, int row, uint8_t attn) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)].attn = attn;
    emit cell_changed(ch, row);
}

void TrackerDocument::set_fx(int ch, int row, uint8_t fx) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)].fx = fx;
    emit cell_changed(ch, row);
}

void TrackerDocument::set_fx_param(int ch, int row, uint8_t param) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    channels_[static_cast<size_t>(ch)][static_cast<size_t>(row)].fx_param = param;
    emit cell_changed(ch, row);
}

void TrackerDocument::clear_cell(int ch, int row) {
    set_cell(ch, row, TrackerCell{});
}

void TrackerDocument::clear_all() {
    for (auto& ch : channels_)
        std::fill(ch.begin(), ch.end(), TrackerCell{});
    emit document_reset();
}

void TrackerDocument::ensure_size() {
    for (auto& ch : channels_)
        ch.resize(static_cast<size_t>(length_));
}

// ============================================================
// Undo / Redo
// ============================================================

TrackerDocument::Snapshot TrackerDocument::make_snapshot() const {
    return {length_, channels_};
}

void TrackerDocument::restore_snapshot(const Snapshot& snap) {
    length_ = snap.length;
    channels_ = snap.channels;
    ensure_size();
    emit document_reset();
}

void TrackerDocument::push_undo() {
    undo_stack_.push_back(make_snapshot());
    if (static_cast<int>(undo_stack_.size()) > kMaxUndo)
        undo_stack_.erase(undo_stack_.begin());
    redo_stack_.clear();
}

void TrackerDocument::undo() {
    if (undo_stack_.empty()) return;
    redo_stack_.push_back(make_snapshot());
    restore_snapshot(undo_stack_.back());
    undo_stack_.pop_back();
}

void TrackerDocument::redo() {
    if (redo_stack_.empty()) return;
    undo_stack_.push_back(make_snapshot());
    restore_snapshot(redo_stack_.back());
    redo_stack_.pop_back();
}

// ============================================================
// Clipboard
// ============================================================

void TrackerDocument::copy(int ch, int row_start, int row_end, TrackerClipboard& out) const {
    if (row_start > row_end) std::swap(row_start, row_end);
    row_start = std::clamp(row_start, 0, length_ - 1);
    row_end = std::clamp(row_end, 0, length_ - 1);
    int count = row_end - row_start + 1;

    if (ch >= 0 && ch < kChannelCount) {
        out.num_channels = 1;
        out.source_ch = ch;
        for (int c = 0; c < kChannelCount; ++c) out.cells[c].clear();
        out.cells[ch].resize(static_cast<size_t>(count));
        for (int r = 0; r < count; ++r)
            out.cells[ch][static_cast<size_t>(r)] = cell(ch, row_start + r);
    } else {
        out.num_channels = kChannelCount;
        out.source_ch = 0;
        for (int c = 0; c < kChannelCount; ++c) {
            out.cells[c].resize(static_cast<size_t>(count));
            for (int r = 0; r < count; ++r)
                out.cells[c][static_cast<size_t>(r)] = cell(c, row_start + r);
        }
    }
}

void TrackerDocument::paste(int ch, int row_start, const TrackerClipboard& clip) {
    if (clip.row_count() == 0) return;
    push_undo();

    if (clip.num_channels == 1) {
        int src = clip.source_ch;
        for (int r = 0; r < clip.row_count() && row_start + r < length_; ++r)
            set_cell(ch, row_start + r, clip.cells[src][static_cast<size_t>(r)]);
    } else {
        for (int c = 0; c < kChannelCount; ++c) {
            if (static_cast<int>(clip.cells[c].size()) != clip.row_count()) continue;
            for (int r = 0; r < clip.row_count() && row_start + r < length_; ++r)
                set_cell(c, row_start + r, clip.cells[c][static_cast<size_t>(r)]);
        }
    }
}

// ============================================================
// Transpose
// ============================================================

void TrackerDocument::transpose(int ch, int row_start, int row_end, int semitones) {
    if (semitones == 0) return;
    if (row_start > row_end) std::swap(row_start, row_end);
    row_start = std::clamp(row_start, 0, length_ - 1);
    row_end = std::clamp(row_end, 0, length_ - 1);
    push_undo();

    auto do_channel = [&](int c) {
        for (int r = row_start; r <= row_end; ++r) {
            auto& cl = channels_[static_cast<size_t>(c)][static_cast<size_t>(r)];
            if (cl.is_note_on()) {
                int n = static_cast<int>(cl.note) + semitones;
                cl.note = static_cast<uint8_t>(std::clamp(n, 1, 127));
                emit cell_changed(c, r);
            }
        }
    };

    if (ch >= 0 && ch < kChannelCount)
        do_channel(ch);
    else
        for (int c = 0; c < kChannelCount; ++c) do_channel(c);
}

// ============================================================
// Cut
// ============================================================

void TrackerDocument::cut(int ch, int row_start, int row_end, TrackerClipboard& out) {
    copy(ch, row_start, row_end, out);
    push_undo();

    if (row_start > row_end) std::swap(row_start, row_end);
    row_start = std::clamp(row_start, 0, length_ - 1);
    row_end = std::clamp(row_end, 0, length_ - 1);

    auto clear_range = [&](int c) {
        for (int r = row_start; r <= row_end; ++r) {
            channels_[static_cast<size_t>(c)][static_cast<size_t>(r)] = TrackerCell{};
            emit cell_changed(c, r);
        }
    };

    if (ch >= 0 && ch < kChannelCount)
        clear_range(ch);
    else
        for (int c = 0; c < kChannelCount; ++c) clear_range(c);
}

// ============================================================
// Row Operations
// ============================================================

void TrackerDocument::insert_row_all(int row) {
    if (row < 0 || row >= length_) return;
    push_undo();
    for (int c = 0; c < kChannelCount; ++c) {
        auto& ch = channels_[static_cast<size_t>(c)];
        // Insert empty row at position, last row is lost
        ch.insert(ch.begin() + row, TrackerCell{});
        ch.resize(static_cast<size_t>(length_));  // trim back to length
    }
    emit document_reset();
}

void TrackerDocument::delete_row_all(int row) {
    if (row < 0 || row >= length_) return;
    push_undo();
    for (int c = 0; c < kChannelCount; ++c) {
        auto& ch = channels_[static_cast<size_t>(c)];
        ch.erase(ch.begin() + row);
        ch.push_back(TrackerCell{});  // append empty at end
    }
    emit document_reset();
}

void TrackerDocument::duplicate_row(int ch, int row) {
    if (ch < 0 || ch >= kChannelCount || row < 0 || row >= length_) return;
    push_undo();
    auto& channel = channels_[static_cast<size_t>(ch)];
    TrackerCell dup = channel[static_cast<size_t>(row)];
    channel.insert(channel.begin() + row + 1, dup);
    channel.resize(static_cast<size_t>(length_));  // trim back
    emit document_reset();
}

// ============================================================
// Interpolation
// ============================================================

void TrackerDocument::interpolate_attn(int ch, int row_start, int row_end) {
    if (ch < 0 || ch >= kChannelCount) return;
    if (row_start > row_end) std::swap(row_start, row_end);
    row_start = std::clamp(row_start, 0, length_ - 1);
    row_end = std::clamp(row_end, 0, length_ - 1);
    int count = row_end - row_start;
    if (count < 2) return;

    auto& channel = channels_[static_cast<size_t>(ch)];
    uint8_t a0 = channel[static_cast<size_t>(row_start)].attn;
    uint8_t a1 = channel[static_cast<size_t>(row_end)].attn;

    // If endpoints don't have explicit attn, use 0 and 15
    if (a0 == 0xFF) a0 = 0;
    if (a1 == 0xFF) a1 = 15;

    push_undo();
    for (int r = row_start; r <= row_end; ++r) {
        float t = static_cast<float>(r - row_start) / static_cast<float>(count);
        uint8_t val = static_cast<uint8_t>(
            std::clamp(static_cast<int>(a0 + t * (static_cast<float>(a1) - static_cast<float>(a0)) + 0.5f), 0, 15));
        channel[static_cast<size_t>(r)].attn = val;
        emit cell_changed(ch, r);
    }
}

// ============================================================
// Serialization (JSON)
// ============================================================

QByteArray TrackerDocument::to_json() const {
    QJsonObject root;
    root["length"] = length_;

    QJsonArray channels;
    for (int c = 0; c < kChannelCount; ++c) {
        QJsonArray rows;
        for (int r = 0; r < length_; ++r) {
            const auto& cl = channels_[static_cast<size_t>(c)][static_cast<size_t>(r)];
            if (cl.is_empty()) {
                rows.append(QJsonValue());  // null = empty
            } else {
                QJsonObject cell;
                cell["n"] = cl.note;
                cell["i"] = cl.instrument;
                cell["a"] = cl.attn;
                if (cl.fx != 0) {
                    cell["f"] = cl.fx;
                    cell["p"] = cl.fx_param;
                }
                rows.append(cell);
            }
        }
        channels.append(rows);
    }
    root["channels"] = channels;

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Compact);
}

bool TrackerDocument::from_json(const QByteArray& data) {
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;

    QJsonObject root = jdoc.object();
    int len = root["length"].toInt(kDefaultLength);
    len = std::clamp(len, kMinLength, kMaxLength);

    QJsonArray channels = root["channels"].toArray();
    if (channels.size() != kChannelCount) return false;

    push_undo();
    length_ = len;
    ensure_size();

    for (int c = 0; c < kChannelCount; ++c) {
        QJsonArray rows = channels[c].toArray();
        for (int r = 0; r < length_ && r < rows.size(); ++r) {
            if (rows[r].isNull() || rows[r].isUndefined()) {
                channels_[static_cast<size_t>(c)][static_cast<size_t>(r)] = TrackerCell{};
            } else {
                QJsonObject cell = rows[r].toObject();
                TrackerCell tc;
                tc.note = static_cast<uint8_t>(cell["n"].toInt(0));
                tc.instrument = static_cast<uint8_t>(
                    std::clamp(cell["i"].toInt(0), 0, static_cast<int>(kMaxInstrument)));
                tc.attn = static_cast<uint8_t>(cell["a"].toInt(0xFF));
                tc.fx = static_cast<uint8_t>(cell["f"].toInt(0));
                tc.fx_param = static_cast<uint8_t>(cell["p"].toInt(0));
                channels_[static_cast<size_t>(c)][static_cast<size_t>(r)] = tc;
            }
        }
    }
    emit document_reset();
    return true;
}
