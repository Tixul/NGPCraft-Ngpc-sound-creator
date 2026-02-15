#pragma once

#include <QWidget>
#include <QString>
#include <vector>

#include "ngpc/instrument.h"

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QTimer;
class QLabel;
class QProgressBar;
class EngineHub;
class InstrumentStore;

class PlayerTab : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerTab(EngineHub* hub, InstrumentStore* store, QWidget* parent = nullptr);

private:
    QLineEdit* midi_path_ = nullptr;
    QLineEdit* driver_path_ = nullptr;
    QPlainTextEdit* log_ = nullptr;

    void on_browse_midi();
    void on_browse_driver();
    void on_load_midi();
    void on_load_driver();
    void append_log(const QString& text);

    EngineHub* hub_ = nullptr;
    InstrumentStore* instrument_store_ = nullptr;

    QComboBox* export_format_ = nullptr;
    QComboBox* preview_profile_ = nullptr;
    QPushButton* rebuild_play_btn_ = nullptr;
    QProgressBar* output_meter_ = nullptr;
    QLabel* output_meter_label_ = nullptr;
    QTimer* meter_timer_ = nullptr;

    struct StreamState {
        std::vector<uint8_t> data;
        uint16_t loop_offset = 0;
        size_t pos = 0;
        int remaining = 0;
        uint8_t attn = 2;
        bool active = false;

        // --- Instrument effect state (for hybrid streams) ---
        bool note_active = false;
        uint8_t attn_base = 2;       // base attn from instrument
        uint8_t attn_cur = 2;        // current after envelope
        uint16_t base_div = 0;       // base divider from note
        uint16_t tone_div = 0;       // current divider after sweep

        // Envelope
        bool env_on = false;
        uint8_t env_step = 1;
        uint8_t env_speed = 1;
        uint8_t env_counter = 0;
        uint8_t env_index = 0;
        std::vector<int8_t> env_curve;
        uint8_t env_curve_id = 0;

        // Vibrato
        bool vib_on = false;
        uint8_t vib_depth = 0;
        uint8_t vib_speed = 1;
        uint8_t vib_delay = 0;
        uint8_t vib_delay_counter = 0;
        uint8_t vib_counter = 0;
        int8_t vib_dir = 1;
        bool lfo_on = false;
        uint8_t lfo_wave = 0;
        uint8_t lfo_hold = 0;
        uint8_t lfo_rate = 1;
        uint8_t lfo_depth = 0;
        uint8_t lfo_hold_counter = 0;
        uint8_t lfo_counter = 0;
        int8_t lfo_sign = 1;
        int16_t lfo_delta = 0;
        bool lfo2_on = false;
        uint8_t lfo2_wave = 0;
        uint8_t lfo2_hold = 0;
        uint8_t lfo2_rate = 1;
        uint8_t lfo2_depth = 0;
        uint8_t lfo2_hold_counter = 0;
        uint8_t lfo2_counter = 0;
        int8_t lfo2_sign = 1;
        int16_t lfo2_delta = 0;
        uint8_t lfo_algo = 1;
        int16_t lfo_pitch_delta = 0;
        int8_t lfo_attn_delta = 0;

        // Sweep
        bool sweep_on = false;
        uint16_t sweep_end = 1;
        int16_t sweep_step = 0;
        uint8_t sweep_speed = 1;
        uint8_t sweep_counter = 0;
        uint8_t mode = 0;
        uint8_t noise_config = 0;
        uint8_t macro_id = 0;

        // Pitch curve + macro
        std::vector<int16_t> pitch_curve;
        uint8_t pitch_counter = 0;
        uint8_t pitch_index = 0;
        int16_t pitch_offset = 0;
        uint8_t macro_step = 0;
        uint8_t macro_counter = 0;
        bool macro_active = false;
        int16_t macro_pitch = 0;

        // ADSR
        bool adsr_on = false;
        uint8_t adsr_attack = 0;
        uint8_t adsr_decay = 0;
        uint8_t adsr_sustain = 0;
        uint8_t adsr_sustain_rate = 0;
        uint8_t adsr_release = 0;
        uint8_t adsr_phase = 0;   // 0=off, 1=ATK, 2=DEC, 3=SUS, 4=REL
        uint8_t adsr_counter = 0;

        // Flags: does this stream use instrument effects?
        bool fx_active = false;
        bool pending_write = false; // force a PSG write on next tick_stream_fx pass

        // Expression (per-voice attn offset, 0-15)
        uint8_t expression = 0;

        // Pitch bend (signed divider offset)
        int16_t pitch_bend = 0;

        void reset_fx() {
            note_active = false;
            attn_base = 2; attn_cur = 2;
            base_div = 0; tone_div = 0;
            env_on = false; env_step = 1; env_speed = 1; env_counter = 0;
            env_index = 0;
            env_curve.clear(); env_curve_id = 0;
            vib_on = false; vib_depth = 0; vib_speed = 1; vib_delay = 0;
            vib_delay_counter = 0; vib_counter = 0; vib_dir = 1;
            lfo_on = false; lfo_wave = 0; lfo_hold = 0; lfo_rate = 1; lfo_depth = 0;
            lfo_hold_counter = 0; lfo_counter = 0; lfo_sign = 1; lfo_delta = 0;
            lfo2_on = false; lfo2_wave = 0; lfo2_hold = 0; lfo2_rate = 1; lfo2_depth = 0;
            lfo2_hold_counter = 0; lfo2_counter = 0; lfo2_sign = 1; lfo2_delta = 0;
            lfo_algo = 1; lfo_pitch_delta = 0; lfo_attn_delta = 0;
            sweep_on = false; sweep_end = 1; sweep_step = 0;
            sweep_speed = 1; sweep_counter = 0;
            mode = 0; noise_config = 0; macro_id = 0;
            pitch_curve.clear(); pitch_counter = 0; pitch_index = 0; pitch_offset = 0;
            macro_step = 0; macro_counter = 0; macro_active = false; macro_pitch = 0;
            adsr_on = false; adsr_attack = 0; adsr_decay = 0;
            adsr_sustain = 0; adsr_sustain_rate = 0; adsr_release = 0;
            adsr_phase = 0; adsr_counter = 0;
            expression = 0;
            pitch_bend = 0;
            fx_active = false;
            pending_write = false;
        }

        void set_note(uint16_t div) {
            note_active = true;
            base_div = div;
            tone_div = div;
            if (adsr_on) {
                attn_cur = 15; // start silent, attack ramps down
                adsr_phase = 1; // ATK
                adsr_counter = adsr_attack;
            } else {
                attn_cur = attn_base;
            }
            env_counter = env_speed;
            env_index = 0;
            pitch_index = 0;
            pitch_counter = env_speed;
            pitch_offset = 0;
            vib_delay_counter = vib_delay;
            vib_counter = vib_speed;
            vib_dir = 1;
            lfo_hold_counter = lfo_hold;
            lfo_counter = lfo_rate;
            lfo_sign = 1;
            lfo_delta = 0;
            lfo2_hold_counter = lfo2_hold;
            lfo2_counter = lfo2_rate;
            lfo2_sign = 1;
            lfo2_delta = 0;
            lfo_pitch_delta = 0;
            lfo_attn_delta = 0;
            sweep_counter = sweep_speed;
            macro_step = 0;
            macro_counter = 0;
            macro_active = false;
            macro_pitch = 0;
            pending_write = false;
        }
    };

    void tick_stream_fx(StreamState& s, int ch, bool noise, bool force_write = false);
    ngpc::BgmInstrumentDef resolve_instrument_def(uint8_t inst_id) const;
    void apply_instrument_to_stream(StreamState& s, const ngpc::BgmInstrumentDef& def, bool noise_channel);
    void stream_macro_reset(StreamState& s);
    uint16_t compute_stream_tone_divider(const StreamState& s, uint16_t base_div) const;

    std::vector<uint8_t> note_table_;
    StreamState streams_[4];
    QTimer* bgm_timer_ = nullptr;
    bool bgm_ready_ = false;
    bool bgm_playing_ = false;

    // Global fade state
    uint8_t fade_speed_ = 0;
    uint8_t fade_counter_ = 0;
    uint8_t fade_attn_ = 0;
    bool warned_bad_table_ = false;
    QString last_c_path_;

    void start_bgm();
    void stop_bgm();
    void tick_bgm();
    void update_output_meter();
    void reset_streams();
    bool convert_midi_to_output(const QString& midi_path,
                                const QString& out_path,
                                bool c_array,
                                bool use_hybrid_opcodes,
                                QString* error);
    bool load_streams_from_c(const QString& path, QString* error);

};
