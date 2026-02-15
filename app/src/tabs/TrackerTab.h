#pragma once

#include <QWidget>

#include <array>
#include <vector>

#include "models/TrackerDocument.h"

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QString;
class EngineHub;
class InstrumentStore;
class SongDocument;
class TrackerGridWidget;
class TrackerPlaybackEngine;
class InstrumentPlayer;

class TrackerTab : public QWidget
{
    Q_OBJECT

public:
    explicit TrackerTab(EngineHub* hub, InstrumentStore* store, QWidget* parent = nullptr);

    bool save_song_to_path(const QString& path, QString* error = nullptr);
    bool load_song_from_path(const QString& path, QString* error = nullptr);
    bool import_midi_from_path(const QString& path, QString* error = nullptr);
    std::array<bool, 128> collect_used_instruments() const;
    bool export_song_to_path(const QString& path,
                             bool asm_export,
                             bool include_instrument_export,
                             QString* error = nullptr,
                             QString* instrument_export_path = nullptr,
                             int forced_export_mode = -1,
                             const std::array<uint8_t, 128>* instrument_remap = nullptr);
    int analyze_song_peak_percent(int ticks_per_row = -1);
    int suggest_song_attn_offset_for_target_peak(int target_peak_percent,
                                                 int ticks_per_row = -1,
                                                 int current_peak_percent = -1);
    int apply_song_attn_offset(int delta);

private:
    EngineHub* hub_ = nullptr;
    InstrumentStore* store_ = nullptr;

    SongDocument* song_ = nullptr;          // owns all patterns
    TrackerDocument* doc_ = nullptr;        // convenience: song_->active_pattern()
    TrackerGridWidget* grid_ = nullptr;
    TrackerPlaybackEngine* engine_ = nullptr;
    InstrumentPlayer* preview_player_ = nullptr;
    QPlainTextEdit* log_ = nullptr;

    // Transport
    QPushButton* play_btn_ = nullptr;
    QPushButton* play_song_btn_ = nullptr;  // play whole song (order list)
    QPushButton* loop_sel_btn_ = nullptr;   // loop over selected rows
    QPushButton* stop_btn_ = nullptr;
    QSpinBox* tpr_spin_ = nullptr;
    QSpinBox* octave_spin_ = nullptr;
    QSpinBox* step_spin_ = nullptr;
    QSpinBox* length_spin_ = nullptr;
    QComboBox* kb_layout_combo_ = nullptr;

    // Pattern / Order UI
    QSpinBox* pattern_spin_ = nullptr;
    QLabel* pattern_count_label_ = nullptr;
    QPushButton* pat_add_btn_ = nullptr;
    QPushButton* pat_clone_btn_ = nullptr;
    QPushButton* pat_del_btn_ = nullptr;
    QListWidget* order_list_ = nullptr;
    QPushButton* ord_add_btn_ = nullptr;
    QPushButton* ord_del_btn_ = nullptr;
    QPushButton* ord_up_btn_ = nullptr;
    QPushButton* ord_down_btn_ = nullptr;
    QPushButton* loop_btn_ = nullptr;

    // UI elements
    QLabel* kb_ref_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* bpm_label_ = nullptr;
    QPushButton* follow_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QPushButton* record_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* load_btn_ = nullptr;

    // Clipboard
    TrackerClipboard clipboard_;

    // Follow mode
    bool follow_mode_ = true;

    // Song mode playback
    bool song_mode_ = false;
    int song_order_pos_ = 0;

    // Mute / Solo
    std::array<QPushButton*, 4> mute_btns_{};
    std::array<QPushButton*, 4> solo_btns_{};
    int solo_channel_ = -1;

    // Playback state
    QTimer* play_timer_ = nullptr;
    bool playing_ = false;
    int preview_note_token_ = 0;

    // Export
    QPushButton* export_btn_ = nullptr;
    QPushButton* export_asm_btn_ = nullptr;
    QComboBox* export_mode_combo_ = nullptr;
    QPushButton* runtime_dbg_btn_ = nullptr;
    bool runtime_debug_enabled_ = false;

    // Methods
    void start_playback();
    void start_playback_from_start();
    void start_loop_selection();
    void start_song_playback();
    void stop_playback();
    void on_tick();
    void write_voices_to_psg();
    void silence_all();
    void update_mute_state();
    void append_log(const QString& text);
    void append_runtime_debug_row(int row);
    void update_kb_ref_label();
    void update_status_label();
    void update_bpm_label();

    // Pattern/Order management
    void switch_to_pattern(int index);
    void refresh_pattern_ui();
    void refresh_order_list();
    void on_pattern_finished();

    // Signal handlers
    void on_note_entered(int ch, int row, uint8_t note);
    void on_note_off_entered(int ch, int row);
    void on_cell_cleared(int ch, int row);
    void on_instrument_digit(int ch, int row, int hex_digit);
    void on_attn_digit(int ch, int row, int hex_digit);
    void on_fx_digit(int ch, int row, int col_index, int hex_digit);
    void on_export();
    void on_export_asm();
    void on_save();
    void on_load();
    void on_import_midi();

    // Helpers
    void preview_note(uint8_t midi_note, int ch);

    // Pre-baked export (tick-by-tick simulation with effects + instruments)
    struct ExportStreams {
        std::vector<uint16_t> note_table;            // divider values
        std::array<std::vector<uint8_t>, 4> streams;  // 4 channel streams
        std::array<uint16_t, 4> loop_offsets;         // byte offset of loop point per channel
    };
    ExportStreams build_export_streams() const;         // pre-baked (tick-by-tick)
    ExportStreams build_export_streams_hybrid(
        const std::array<uint8_t, 128>* instrument_remap = nullptr) const;  // hybrid (row-based + instrument opcodes)
};
