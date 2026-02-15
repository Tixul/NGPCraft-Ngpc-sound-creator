#pragma once

#include <QObject>
#include <cstdint>

#include "ngpc/instrument.h"

class QTimer;
class EngineHub;

// Frame-by-frame instrument preview engine.
// Faithfully reproduces the driver's BgmVoice_UpdateFx / BgmVoice_CommandFromState
// logic: envelope curves, pitch curves, vibrato, and sweep.
class InstrumentPlayer : public QObject
{
    Q_OBJECT

public:
    explicit InstrumentPlayer(EngineHub* hub, QObject* parent = nullptr);

    void play(const ngpc::BgmInstrumentDef& def, uint16_t divider, uint8_t tone_ch = 0);
    void note_off();
    void stop();
    bool is_playing() const;

signals:
    void stopped();

private:
    void tick();
    void write_psg();
    void silence();

    EngineHub* hub_ = nullptr;
    QTimer* timer_ = nullptr;
    bool playing_ = false;

    ngpc::BgmInstrumentDef def_;
    std::vector<int8_t> env_curve_;
    std::vector<int16_t> pitch_curve_;

    // Base values (set at note-on)
    uint16_t base_div_ = 0;
    uint8_t  base_attn_ = 0;
    uint8_t  tone_ch_ = 0;

    // Envelope state
    uint8_t  env_counter_ = 0;
    uint8_t  env_index_ = 0;
    uint8_t  attn_cur_ = 0;

    // ADSR state
    uint8_t  adsr_phase_ = 0;    // 0=off, 1=ATK, 2=DEC, 3=SUS, 4=REL
    uint8_t  adsr_counter_ = 0;

    // Pitch curve state
    uint8_t  pitch_counter_ = 0;
    uint8_t  pitch_index_ = 0;
    int16_t  pitch_offset_ = 0;
    uint8_t  macro_step_ = 0;
    uint8_t  macro_counter_ = 0;
    bool     macro_active_ = false;
    int16_t  macro_pitch_ = 0;

    // Vibrato state
    uint8_t  vib_delay_counter_ = 0;
    uint8_t  vib_counter_ = 0;
    int8_t   vib_dir_ = 1;

    // LFO state (pitch)
    uint8_t  lfo_hold_counter_ = 0;
    uint8_t  lfo_counter_ = 0;
    int8_t   lfo_sign_ = 1;
    int16_t  lfo_delta_ = 0;
    uint8_t  lfo2_hold_counter_ = 0;
    uint8_t  lfo2_counter_ = 0;
    int8_t   lfo2_sign_ = 1;
    int16_t  lfo2_delta_ = 0;
    int16_t  lfo_pitch_delta_ = 0;
    int8_t   lfo_attn_delta_ = 0;

    // Sweep state
    uint16_t tone_div_ = 0;
    uint8_t  sweep_counter_ = 0;
    bool     sweep_active_ = false;

    // Auto-stop: silence after envelope reaches max attenuation
    int      silent_frames_ = 0;
};
