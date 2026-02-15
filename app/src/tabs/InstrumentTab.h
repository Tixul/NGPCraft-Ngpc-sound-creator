#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class EngineHub;
class InstrumentPlayer;
class InstrumentStore;
class EnvelopeCurveWidget;

class InstrumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit InstrumentTab(EngineHub* hub, InstrumentStore* store, QWidget* parent = nullptr);

private:
    EngineHub* hub_ = nullptr;
    InstrumentStore* store_ = nullptr;
    InstrumentPlayer* player_ = nullptr;
    bool updating_ui_ = false;
    bool loop_preview_ = false;
    int preview_note_token_ = 0;

    QListWidget* list_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QLabel* tracker_code_label_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QComboBox* factory_combo_ = nullptr;

    // General
    QSpinBox* attn_spin_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QComboBox* noise_combo_ = nullptr;
    QLabel* noise_label_ = nullptr;

    // Envelope
    QCheckBox* env_on_ = nullptr;
    QSpinBox* env_step_ = nullptr;
    QSpinBox* env_speed_ = nullptr;
    QComboBox* env_curve_combo_ = nullptr;
    EnvelopeCurveWidget* env_widget_ = nullptr;

    // Pitch Curve (tone only)
    QGroupBox* pitch_box_ = nullptr;
    QComboBox* pitch_curve_combo_ = nullptr;

    // Vibrato (tone only)
    QGroupBox* vib_box_ = nullptr;
    QCheckBox* vib_on_ = nullptr;
    QSpinBox* vib_depth_ = nullptr;
    QSpinBox* vib_speed_ = nullptr;
    QSpinBox* vib_delay_ = nullptr;

    // Sweep (tone only)
    QGroupBox* sweep_box_ = nullptr;
    QCheckBox* sweep_on_ = nullptr;
    QSpinBox* sweep_end_ = nullptr;
    QSpinBox* sweep_step_ = nullptr;
    QSpinBox* sweep_speed_ = nullptr;

    // ADSR
    QGroupBox* adsr_box_ = nullptr;
    QCheckBox* adsr_on_ = nullptr;
    QSpinBox* adsr_attack_ = nullptr;
    QSpinBox* adsr_decay_ = nullptr;
    QSpinBox* adsr_sustain_ = nullptr;
    QSpinBox* adsr_sustain_rate_ = nullptr;
    QSpinBox* adsr_release_ = nullptr;

    // LFO (pitch)
    QGroupBox* lfo_box_ = nullptr;
    QCheckBox* lfo_on_ = nullptr;
    QComboBox* lfo_wave_ = nullptr;
    QSpinBox* lfo_hold_ = nullptr;
    QSpinBox* lfo_rate_ = nullptr;
    QSpinBox* lfo_depth_ = nullptr;
    QGroupBox* lfo2_box_ = nullptr;
    QSpinBox* lfo_algo_ = nullptr;
    QCheckBox* lfo2_on_ = nullptr;
    QComboBox* lfo2_wave_ = nullptr;
    QSpinBox* lfo2_hold_ = nullptr;
    QSpinBox* lfo2_rate_ = nullptr;
    QSpinBox* lfo2_depth_ = nullptr;

    // Macro
    QSpinBox* macro_id_ = nullptr;

    // Preview
    QComboBox* preview_note_ = nullptr;

    void refresh_list();
    void update_list_item(int index);
    void load_preset_to_ui(int index);
    void on_parameter_changed();
    void on_name_changed();
    void on_preview_play();
    void on_preview_stop();
    void update_noise_visibility();
    void append_log(const QString& text);
};
