#pragma once

#include <QByteArray>
#include <QObject>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "ngpc/instrument.h"

// --- TrackerCell ---
struct TrackerCell {
    uint8_t note       = 0;    // 0=empty, 1-127=MIDI note, 0xFF=note-off
    uint8_t instrument = 0;    // instrument index (0-127)
    uint8_t attn       = 0xFF; // 0xFF=use instrument default, 0-15=override
    uint8_t fx         = 0;    // effect command: 0=none, 1=pitch up, 2=pitch down, 3=porta, 0xA=volslide, 0xB=speed, 0xC=cut, 0xD=delay
    uint8_t fx_param   = 0;    // effect parameter (00-FF)

    bool is_empty() const    { return note == 0 && fx == 0; }
    bool is_note_off() const { return note == 0xFF; }
    bool is_note_on() const  { return note >= 1 && note <= 127; }
    bool has_fx() const      { return fx != 0; }
};

// --- TrackerVoice ---
struct TrackerVoice {
    void note_on(const ngpc::BgmInstrumentDef& def,
                 const std::vector<int8_t>& env_curve,
                 const std::vector<int16_t>& pitch_curve,
                 uint16_t divider, uint8_t attn_override);
    void note_off();
    void tick();

    uint16_t final_divider() const;
    uint8_t  final_attn() const;
    bool     active() const { return active_; }

private:
    bool active_ = false;
    ngpc::BgmInstrumentDef def_;
    std::vector<int8_t>  env_curve_;
    std::vector<int16_t> pitch_curve_;
    uint16_t base_div_  = 0;
    uint8_t  base_attn_ = 0;
    uint8_t  env_counter_ = 0;
    uint8_t  env_index_   = 0;
    uint8_t  attn_cur_    = 0;
    uint8_t  pitch_counter_ = 0;
    uint8_t  pitch_index_   = 0;
    int16_t  pitch_offset_  = 0;
    uint8_t  macro_step_    = 0;
    uint8_t  macro_counter_ = 0;
    bool     macro_active_  = false;
    int16_t  macro_pitch_   = 0;
    uint8_t  vib_delay_counter_ = 0;
    uint8_t  vib_counter_       = 0;
    int8_t   vib_dir_           = 1;
    uint8_t  lfo_hold_counter_  = 0;
    uint8_t  lfo_counter_       = 0;
    int8_t   lfo_sign_          = 1;
    int16_t  lfo_delta_         = 0;
    uint8_t  lfo2_hold_counter_ = 0;
    uint8_t  lfo2_counter_      = 0;
    int8_t   lfo2_sign_         = 1;
    int16_t  lfo2_delta_        = 0;
    int16_t  lfo_pitch_delta_   = 0;
    int8_t   lfo_attn_delta_    = 0;
    uint16_t tone_div_      = 0;
    uint8_t  sweep_counter_ = 0;
    bool     sweep_active_  = false;
    // ADSR
    uint8_t  adsr_phase_    = 0;  // 0=off, 1=ATK, 2=DEC, 3=SUS, 4=REL
    uint8_t  adsr_counter_  = 0;
};

// --- Clipboard ---
struct TrackerClipboard {
    int num_channels = 0;     // 1=single channel, 4=all
    int source_ch    = 0;
    std::array<std::vector<TrackerCell>, 4> cells;
    int row_count() const {
        if (num_channels == 1 && source_ch >= 0 && source_ch < 4) {
            return static_cast<int>(cells[static_cast<size_t>(source_ch)].size());
        }
        int max_rows = 0;
        for (int c = 0; c < 4; ++c) {
            max_rows = std::max(max_rows, static_cast<int>(cells[static_cast<size_t>(c)].size()));
        }
        return max_rows;
    }
};

// --- TrackerDocument ---
class TrackerDocument : public QObject
{
    Q_OBJECT

public:
    static constexpr int kChannelCount  = 4;
    static constexpr int kMinLength     = 16;
    static constexpr int kMaxLength     = 128;
    static constexpr int kDefaultLength = 64;
    static constexpr uint8_t kMaxInstrument = 127;

    explicit TrackerDocument(QObject* parent = nullptr);

    int length() const { return length_; }
    void set_length(int len);

    const TrackerCell& cell(int ch, int row) const;
    void set_cell(int ch, int row, const TrackerCell& cell);
    void set_note(int ch, int row, uint8_t note);
    void set_instrument(int ch, int row, uint8_t inst);
    void set_attn(int ch, int row, uint8_t attn);
    void set_fx(int ch, int row, uint8_t fx);
    void set_fx_param(int ch, int row, uint8_t param);
    void clear_cell(int ch, int row);
    void clear_all();

    // --- Undo / Redo ---
    void push_undo();
    void undo();
    void redo();
    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    // --- Clipboard ---
    void copy(int ch, int row_start, int row_end, TrackerClipboard& out) const;
    void cut(int ch, int row_start, int row_end, TrackerClipboard& out);
    void paste(int ch, int row_start, const TrackerClipboard& clip);

    // --- Transpose ---
    void transpose(int ch, int row_start, int row_end, int semitones);

    // --- Row operations ---
    void insert_row_all(int row);   // insert empty row at position, shift rest down
    void delete_row_all(int row);   // delete row at position, shift rest up
    void duplicate_row(int ch, int row);  // copy row to row+1, shift down

    // --- Interpolation ---
    void interpolate_attn(int ch, int row_start, int row_end);

    // --- Serialization ---
    QByteArray to_json() const;
    bool from_json(const QByteArray& data);

signals:
    void cell_changed(int ch, int row);
    void length_changed(int len);
    void document_reset();

private:
    int length_ = kDefaultLength;
    std::array<std::vector<TrackerCell>, kChannelCount> channels_;
    static const TrackerCell kEmptyCell;

    struct Snapshot {
        int length;
        std::array<std::vector<TrackerCell>, kChannelCount> channels;
    };
    std::vector<Snapshot> undo_stack_;
    std::vector<Snapshot> redo_stack_;
    static constexpr int kMaxUndo = 100;

    Snapshot make_snapshot() const;
    void restore_snapshot(const Snapshot& snap);
    void ensure_size();
};
