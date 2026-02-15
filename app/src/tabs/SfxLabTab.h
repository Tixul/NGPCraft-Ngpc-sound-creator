#pragma once

#include <QWidget>
#include <QString>

class QComboBox;
class QCheckBox;
class QPlainTextEdit;
class QSpinBox;
class QTimer;
class QLabel;
class QProgressBar;
class EngineHub;
struct ProjectSfxEntry;

class SfxLabTab : public QWidget
{
    Q_OBJECT

public:
    explicit SfxLabTab(EngineHub* hub, QWidget* parent = nullptr);
    void load_project_sfx(const ProjectSfxEntry& entry);

signals:
    void save_sfx_to_project_requested(const QString& name,
                                       int tone_on,
                                       int tone_ch,
                                       int tone_div,
                                       int tone_attn,
                                       int tone_frames,
                                       int tone_sw_on,
                                       int tone_sw_end,
                                       int tone_sw_step,
                                       int tone_sw_speed,
                                       int tone_sw_ping,
                                       int tone_env_on,
                                       int tone_env_step,
                                       int tone_env_spd,
                                       int noise_on,
                                       int noise_rate,
                                       int noise_type,
                                       int noise_attn,
                                       int noise_frames,
                                       int noise_burst,
                                       int noise_burst_dur,
                                       int noise_env_on,
                                       int noise_env_step,
                                       int noise_env_spd,
                                       int tone_adsr_on,
                                       int tone_adsr_ar,
                                       int tone_adsr_dr,
                                       int tone_adsr_sl,
                                       int tone_adsr_sr,
                                       int tone_adsr_rr,
                                       int tone_lfo1_on,
                                       int tone_lfo1_wave,
                                       int tone_lfo1_hold,
                                       int tone_lfo1_rate,
                                       int tone_lfo1_depth,
                                       int tone_lfo2_on,
                                       int tone_lfo2_wave,
                                       int tone_lfo2_hold,
                                       int tone_lfo2_rate,
                                       int tone_lfo2_depth,
                                       int tone_lfo_algo);
    void update_sfx_in_project_requested(const QString& id,
                                         const QString& name,
                                         int tone_on,
                                         int tone_ch,
                                         int tone_div,
                                         int tone_attn,
                                         int tone_frames,
                                         int tone_sw_on,
                                         int tone_sw_end,
                                         int tone_sw_step,
                                         int tone_sw_speed,
                                         int tone_sw_ping,
                                         int tone_env_on,
                                         int tone_env_step,
                                         int tone_env_spd,
                                         int noise_on,
                                         int noise_rate,
                                         int noise_type,
                                         int noise_attn,
                                         int noise_frames,
                                         int noise_burst,
                                         int noise_burst_dur,
                                         int noise_env_on,
                                         int noise_env_step,
                                         int noise_env_spd,
                                         int tone_adsr_on,
                                         int tone_adsr_ar,
                                         int tone_adsr_dr,
                                         int tone_adsr_sl,
                                         int tone_adsr_sr,
                                         int tone_adsr_rr,
                                         int tone_lfo1_on,
                                         int tone_lfo1_wave,
                                         int tone_lfo1_hold,
                                         int tone_lfo1_rate,
                                         int tone_lfo1_depth,
                                         int tone_lfo2_on,
                                         int tone_lfo2_wave,
                                         int tone_lfo2_hold,
                                         int tone_lfo2_rate,
                                         int tone_lfo2_depth,
                                         int tone_lfo_algo);

private:
    struct TonePreviewState {
        bool active = false;
        int ch = 0;
        int frames = 0;
        int div_base = 218;
        int div_cur = 218;
        int attn_base = 2;
        int attn_cur = 2;
        bool sw_on = false;
        int sw_end = 218;
        int sw_step_abs = 1;
        int sw_dir = 1;
        int sw_speed = 1;
        int sw_counter = 0;
        bool sw_ping = false;
        bool env_on = false;
        int env_step = 1;
        int env_spd = 1;
        int env_counter = 0;
        bool adsr_on = false;
        int adsr_attack = 0;
        int adsr_decay = 0;
        int adsr_sustain = 8;
        int adsr_sustain_rate = 0;
        int adsr_release = 2;
        int adsr_phase = 0;   // 0=off, 1=ATK, 2=DEC, 3=SUS, 4=REL
        int adsr_counter = 0;
        bool lfo1_on = false;
        int lfo1_wave = 0;
        int lfo1_hold = 0;
        int lfo1_rate = 1;
        int lfo1_depth = 0;
        int lfo1_hold_counter = 0;
        int lfo1_counter = 0;
        int lfo1_sign = 1;
        int lfo1_delta = 0;
        bool lfo2_on = false;
        int lfo2_wave = 0;
        int lfo2_hold = 0;
        int lfo2_rate = 1;
        int lfo2_depth = 0;
        int lfo2_hold_counter = 0;
        int lfo2_counter = 0;
        int lfo2_sign = 1;
        int lfo2_delta = 0;
        int lfo_algo = 1;
        int lfo_pitch_delta = 0;
        int lfo_attn_delta = 0;
        int rendered_div = 218;
        int rendered_attn = 2;
    };

    struct NoisePreviewState {
        bool active = false;
        int frames = 0;
        int rate = 1;
        int type = 1;
        int attn_cur = 2;
        bool env_on = false;
        int env_step = 1;
        int env_spd = 1;
        int env_counter = 0;
        bool burst = false;
        int burst_dur = 1;
        int burst_counter = 1;
        bool burst_off = false;
    };

    EngineHub* hub_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QCheckBox* tone_on_ = nullptr;
    QSpinBox* tone_ch_ = nullptr;
    QSpinBox* tone_div_ = nullptr;
    QSpinBox* tone_attn_ = nullptr;
    QSpinBox* tone_frames_ = nullptr;
    QCheckBox* tone_sw_on_ = nullptr;
    QSpinBox* tone_sw_end_ = nullptr;
    QSpinBox* tone_sw_step_ = nullptr;
    QSpinBox* tone_sw_speed_ = nullptr;
    QCheckBox* tone_sw_ping_ = nullptr;
    QCheckBox* tone_env_on_ = nullptr;
    QSpinBox* tone_env_step_ = nullptr;
    QSpinBox* tone_env_spd_ = nullptr;
    QCheckBox* tone_adsr_on_ = nullptr;
    QSpinBox* tone_adsr_ar_ = nullptr;
    QSpinBox* tone_adsr_dr_ = nullptr;
    QSpinBox* tone_adsr_sl_ = nullptr;
    QSpinBox* tone_adsr_sr_ = nullptr;
    QSpinBox* tone_adsr_rr_ = nullptr;
    QCheckBox* tone_lfo1_on_ = nullptr;
    QComboBox* tone_lfo1_wave_ = nullptr;
    QSpinBox* tone_lfo1_hold_ = nullptr;
    QSpinBox* tone_lfo1_rate_ = nullptr;
    QSpinBox* tone_lfo1_depth_ = nullptr;
    QCheckBox* tone_lfo2_on_ = nullptr;
    QComboBox* tone_lfo2_wave_ = nullptr;
    QSpinBox* tone_lfo2_hold_ = nullptr;
    QSpinBox* tone_lfo2_rate_ = nullptr;
    QSpinBox* tone_lfo2_depth_ = nullptr;
    QSpinBox* tone_lfo_algo_ = nullptr;
    QCheckBox* noise_on_ = nullptr;
    QComboBox* noise_rate_ = nullptr;
    QComboBox* noise_type_ = nullptr;
    QSpinBox* noise_attn_ = nullptr;
    QSpinBox* noise_frames_ = nullptr;
    QCheckBox* noise_burst_ = nullptr;
    QSpinBox* noise_burst_dur_ = nullptr;
    QCheckBox* noise_env_on_ = nullptr;
    QSpinBox* noise_env_step_ = nullptr;
    QSpinBox* noise_env_spd_ = nullptr;
    QComboBox* sfx_preset_ = nullptr;
    QComboBox* preview_mode_ = nullptr;
    QProgressBar* output_meter_ = nullptr;
    QLabel* output_meter_label_ = nullptr;
    QTimer* meter_timer_ = nullptr;
    QTimer* full_preview_timer_ = nullptr;
    TonePreviewState tone_preview_;
    NoisePreviewState noise_preview_;
    QString project_edit_sfx_id_;
    QString project_edit_sfx_name_;

    void append_log(const QString& text);
    void update_output_meter();
    bool use_faithful_preview_mode() const;
    void start_preview_with_mask(bool use_tone, bool use_noise);
    void stop_full_preview(bool silence);
    void start_full_preview();
    void tick_full_preview();
};
