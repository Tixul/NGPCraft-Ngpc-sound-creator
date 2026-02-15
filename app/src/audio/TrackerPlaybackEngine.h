#pragma once

#include <QObject>
#include <QString>

#include <array>
#include <cstdint>

#include "models/TrackerDocument.h"

class InstrumentStore;

// ============================================================
// TrackerPlaybackEngine
// Reusable playback logic: voices, effects, row processing.
// Used by TrackerTab (live) and WavExporter (offline).
// ============================================================

class TrackerPlaybackEngine : public QObject
{
    Q_OBJECT

public:
    // Per-channel effect state (moved from TrackerTab)
    struct ChannelFxState {
        uint8_t fx = 0, param = 0;
        // Arpeggio (0xy)
        uint16_t arp_dividers[3] = {};
        int arp_phase = 0;
        // Pitch slide (1xx / 2xx)
        int16_t pitch_offset = 0;
        // Portamento (3xx)
        bool porta_active = false;
        uint16_t porta_target = 0;
        uint16_t porta_current = 0;
        uint8_t porta_speed = 0;
        // Volume slide (Axy)
        int8_t vol_delta = 0;
        uint8_t vol_current = 0;
        bool vol_override = false;
        // Note cut (Cxx)
        int cut_countdown = -1;
        // Note delay (Dxx)
        int delay_countdown = -1;
        TrackerCell delayed_cell;
        bool note_delayed = false;
        // Expression (Fxx)
        uint8_t expression = 0; // additional attn offset (0-15)
        // Pitch Bend (4xx)
        int16_t pitch_bend = 0; // additional divider offset (signed)

        void reset() { *this = ChannelFxState{}; }
    };

    // Per-channel output (what to write to PSG)
    struct ChannelOutput {
        bool active = false;
        uint16_t divider = 0;
        uint8_t attn = 15;
        uint8_t noise_val = 0; // PSG noise register low 3 bits (rate/type)
    };

    explicit TrackerPlaybackEngine(QObject* parent = nullptr);

    // Configuration
    void set_document(TrackerDocument* doc);
    void set_instrument_store(InstrumentStore* store);

    // Transport
    void start(int from_row = 0);
    void stop();
    bool is_playing() const { return playing_; }

    // Loop range (for selection looping)
    void set_loop_range(int start, int end);
    void clear_loop_range();
    bool has_loop_range() const { return loop_start_ >= 0; }

    // Tick — advance one frame. Returns true if a new row started.
    bool tick();

    // State
    int current_row() const { return current_row_; }
    int tick_counter() const { return tick_counter_; }
    int ticks_per_row() const { return ticks_per_row_; }
    void set_ticks_per_row(int tpr);

    // Channel output (computed after each tick)
    ChannelOutput channel_output(int ch) const;

    // Mute
    void set_channel_muted(int ch, bool muted);
    bool is_channel_muted(int ch) const;

    // Direct access for advanced use
    TrackerVoice& voice(int ch) { return voices_[static_cast<size_t>(ch)]; }
    const ChannelFxState& fx_state(int ch) const { return fx_state_[static_cast<size_t>(ch)]; }

    // Noise helpers
    struct NoiseConfig { uint8_t rate; uint8_t type; };
    static NoiseConfig decode_noise_val(uint8_t noise_val);
    static QString noise_display_name(uint8_t noise_val); // "P.H", "W.M"...

    // Utility
    static uint16_t midi_to_divider(uint8_t midi_note);
    static uint8_t midi_note_to_noise_val(uint8_t midi_note);

signals:
    void row_changed(int row);
    void pattern_finished();   // emitted when playback wraps past last row
    void speed_changed(int tpr); // emitted when Bxx effect fires

private:
    TrackerDocument* doc_ = nullptr;
    InstrumentStore* store_ = nullptr;

    bool playing_ = false;
    int current_row_ = 0;
    int tick_counter_ = 0;
    int ticks_per_row_ = 8;

    // Loop range (-1 = disabled)
    int loop_start_ = -1;
    int loop_end_ = -1;

    std::array<TrackerVoice, 4> voices_;
    std::array<ChannelFxState, 4> fx_state_;
    uint8_t noise_val_ = 0; // CH3 noise register bits (0-7)
    bool channel_muted_[4] = {};

    // Global fade state (Exx effect: E0x = fade out)
    uint8_t fade_speed_ = 0;   // 0 = no fade; >0 = ticks between fade steps
    uint8_t fade_counter_ = 0;
    uint8_t fade_attn_ = 0;    // additional global attn (0-15)

    void process_row(int row);
    void init_fx(int ch, const TrackerCell& cell);
    void tick_fx(int ch);
};
