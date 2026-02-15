#include "SfxLabTab.h"

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "audio/EngineHub.h"
#include "audio/PsgHelpers.h"
#include "i18n/AppLanguage.h"
#include "models/ProjectDocument.h"

namespace {
int clamp_i(int v, int lo, int hi) {
    return std::min(hi, std::max(lo, v));
}

void tight_spin(QSpinBox* sb, int width = 78) {
    sb->setMinimumWidth(width);
    sb->setAlignment(Qt::AlignRight);
}

void set_combo_data(QComboBox* cb, int value) {
    if (!cb) return;
    const int idx = cb->findData(value);
    cb->setCurrentIndex(idx >= 0 ? idx : 0);
}

int combo_data(const QComboBox* cb, int fallback = 0) {
    if (!cb) return fallback;
    const QVariant data = cb->currentData();
    if (!data.isValid()) return fallback;
    return data.toInt();
}

int lfo_step_wave(int wave, int cur, int& sign, int depth) {
    if (depth <= 0) return 0;
    switch (wave) {
    case 0: {
        int next = cur + sign;
        if (next >= depth) {
            next = depth;
            sign = -1;
        } else if (next <= -depth) {
            next = -depth;
            sign = 1;
        }
        return next;
    }
    case 1:
        sign = (sign < 0) ? 1 : -1;
        return depth * sign;
    case 2: {
        int next = cur + 1;
        if (next > depth) next = -depth;
        return next;
    }
    case 3:
        return (cur < depth) ? (cur + 1) : depth;
    case 4:
        return (cur > -depth) ? (cur - 1) : -depth;
    default:
        return cur;
    }
}

bool lfo_tick(bool on,
              int wave,
              int rate,
              int depth,
              int& hold_counter,
              int& counter,
              int& sign,
              int& delta) {
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
        const int next = lfo_step_wave(clamp_i(wave, 0, 4), delta, sign, depth);
        if (next != delta) {
            delta = next;
            return true;
        }
    } else {
        counter--;
    }
    return false;
}

int lfo_to_attn_delta(int mod) {
    int d = mod / 16;
    d = clamp_i(d, -15, 15);
    return -d;
}

void resolve_lfo_algo(int algo, int l1, int l2, int& pitch_delta, int& attn_delta) {
    const int mix = clamp_i(l1 + l2, -255, 255);
    switch (algo & 0x07) {
    default:
    case 0:
        pitch_delta = 0;
        attn_delta = 0;
        break;
    case 1:
        pitch_delta = l2;
        attn_delta = lfo_to_attn_delta(l1);
        break;
    case 2:
        pitch_delta = mix;
        attn_delta = lfo_to_attn_delta(mix);
        break;
    case 3:
        pitch_delta = l2;
        attn_delta = lfo_to_attn_delta(mix);
        break;
    case 4:
        pitch_delta = mix;
        attn_delta = lfo_to_attn_delta(l1);
        break;
    case 5:
        pitch_delta = 0;
        attn_delta = lfo_to_attn_delta(mix);
        break;
    case 6:
        pitch_delta = mix;
        attn_delta = 0;
        break;
    case 7:
        pitch_delta = mix / 2;
        attn_delta = 0;
        break;
    }
}

struct SfxPreset {
    const char* name;
    int tone_on;
    int tone_ch;
    int tone_div;
    int tone_attn;
    int tone_frames;
    int tone_sw_on;
    int tone_sw_end;
    int tone_sw_step;
    int tone_sw_speed;
    int tone_sw_ping;
    int tone_env_on;
    int tone_env_step;
    int tone_env_spd;
    int noise_on;
    int noise_rate;
    int noise_type;
    int noise_attn;
    int noise_frames;
    int noise_burst;
    int noise_burst_dur;
    int noise_env_on;
    int noise_env_step;
    int noise_env_spd;
    int tone_adsr_on;
    int tone_adsr_ar;
    int tone_adsr_dr;
    int tone_adsr_sl;
    int tone_adsr_sr;
    int tone_adsr_rr;
    int tone_lfo1_on;
    int tone_lfo1_wave;
    int tone_lfo1_hold;
    int tone_lfo1_rate;
    int tone_lfo1_depth;
    int tone_lfo2_on;
    int tone_lfo2_wave;
    int tone_lfo2_hold;
    int tone_lfo2_rate;
    int tone_lfo2_depth;
    int tone_lfo_algo;
};

constexpr SfxPreset kPresets[] = {
    // UI / menu
    {"UI Click",          1, 0, 360, 4,  3, 0, 360,  1, 1, 0, 1, 2, 1, 0, 1, 1, 10,  0, 0, 1, 0, 1, 1},
    {"UI Move",           1, 0, 320, 3,  4, 1, 280, -4, 2, 0, 1, 1, 2, 0, 1, 1, 10,  0, 0, 1, 0, 1, 1},
    {"UI Confirm",        1, 0, 320, 2,  7, 1, 220, -6, 2, 0, 1, 1, 2, 0, 1, 1, 10,  0, 0, 1, 0, 1, 1},
    {"UI Error",          1, 0, 190, 4, 12, 1, 340,  6, 1, 0, 1, 1, 2, 1, 1, 1, 10, 10, 1, 1, 1, 2, 2},

    // Pickups / rewards
    {"Coin Classic",      1, 0, 300, 1,  8, 1, 450, -7, 1, 0, 1, 1, 2, 0, 1, 1, 15,  0, 0, 1, 0, 1, 1},
    {"Powerup Shine",     1, 0, 300, 2, 14, 1, 220, -5, 1, 1, 1, 1, 2, 1, 2, 1, 11, 10, 1, 3, 1, 1, 2,
                          1, 0, 2, 7, 0, 3, 1, 0, 0, 2, 16, 1, 0, 0, 1, 12, 1},
    {"Goal Stinger",      1, 0, 380, 1, 18, 1, 240, -8, 1, 1, 1, 1, 2, 0, 1, 1, 15,  0, 0, 1, 0, 1, 1,
                          1, 0, 2, 6, 0, 3, 0, 0, 0, 1, 0, 1, 0, 0, 1, 10, 1},

    // Weapons / actions
    {"Shot Soft",         1, 0, 220, 3,  7, 1, 120, -6, 1, 0, 1, 1, 2, 0, 2, 1, 13,  0, 0, 1, 0, 1, 1},
    {"Shot Heavy",        1, 1, 180, 4, 10, 1, 110, -8, 1, 0, 1, 1, 2, 1, 1, 1,  9, 10, 1, 2, 1, 2, 2,
                          1, 0, 1, 9, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0},
    {"Laser Thin",        1, 0, 240, 3, 10, 1,  70, -7, 1, 0, 0, 1, 1, 0, 2, 1, 12,  0, 0, 1, 0, 1, 1,
                          1, 0, 1, 10, 0, 2, 0, 0, 0, 1, 0, 1, 2, 0, 1, 22, 6},
    {"Laser Thick",       1, 0, 210, 2, 14, 1,  60, -8, 1, 0, 1, 1, 2, 1, 0, 1,  8, 14, 0, 1, 1, 1, 2,
                          1, 0, 2, 8, 0, 3, 1, 1, 0, 2, 8, 1, 2, 0, 1, 28, 2},
    {"Charge Start",      1, 1, 300, 3, 12, 1, 160, -2, 2, 0, 0, 1, 1, 0, 1, 1, 15,  0, 0, 1, 0, 1, 1,
                          1, 0, 3, 10, 1, 3, 1, 0, 4, 2, 10, 1, 3, 0, 1, 18, 2},
    {"Charge Release",    1, 1, 170, 2, 16, 1, 320,  7, 1, 0, 1, 2, 1, 1, 0, 1,  5, 14, 1, 3, 1, 2, 2,
                          1, 0, 1, 7, 0, 2, 0, 0, 0, 1, 0, 1, 3, 0, 1, 22, 6},

    // Hits / impacts / explosions
    {"Hit Light",         1, 2, 260, 4,  4, 1, 340,  9, 1, 0, 1, 2, 1, 1, 1, 1,  7,  6, 1, 1, 1, 2, 1},
    {"Hit Heavy",         1, 1, 210, 4,  8, 1, 340, 10, 1, 0, 1, 2, 1, 1, 0, 1,  4, 10, 1, 2, 1, 2, 2},
    {"Explosion Small",   0, 0, 220, 8,  2, 0, 220,  1, 1, 0, 0, 1, 1, 1, 0, 1,  3, 14, 1, 3, 1, 2, 2},
    {"Explosion Medium",  1, 1, 130, 8, 12, 1, 280,  5, 1, 0, 1, 2, 2, 1, 0, 1,  2, 20, 1, 4, 1, 2, 3},
    {"Explosion Big",     1, 1, 110,10, 18, 1, 320,  6, 1, 0, 1, 2, 2, 1, 0, 1,  1, 28, 1, 4, 1, 2, 3},
    {"Debris Rattle",     0, 0, 220,10,  2, 0, 220,  1, 1, 0, 0, 1, 1, 1, 2, 0,  6, 20, 1, 2, 1, 1, 2},

    // Sports / movement style
    {"Dash Whoosh",       1, 0, 340, 4, 12, 1, 200, -9, 1, 0, 1, 2, 2, 1, 3, 1,  8, 10, 1, 2, 1, 2, 2,
                          0, 0, 0, 8, 0, 0, 0, 0, 0, 1, 0, 1, 4, 0, 1, 18, 6},
    {"Throw Spin",        1, 0, 260, 2, 12, 1, 380,  6, 1, 1, 1, 1, 2, 0, 3, 1, 15,  0, 0, 1, 0, 1, 1,
                          1, 0, 2, 9, 0, 3, 1, 1, 0, 2, 10, 1, 0, 0, 1, 12, 3},
    {"Catch Snap",        1, 2, 300, 2,  4, 1, 230, -8, 1, 0, 1, 2, 1, 1, 1, 0,  7,  4, 1, 1, 1, 2, 1},

    // Pure references
    {"Noise HiHat",       0, 0, 220, 8,  2, 0, 220,  1, 1, 0, 0, 1, 1, 1, 3, 1,  6,  6, 1, 1, 1, 2, 1},
    {"Noise Snare",       0, 0, 220, 8,  2, 0, 220,  1, 1, 0, 0, 1, 1, 1, 1, 1,  4, 10, 1, 1, 1, 2, 2},
    {"Noise Kick",        0, 0, 220, 8,  2, 0, 220,  1, 1, 0, 0, 1, 1, 1, 0, 0,  2, 12, 0, 1, 1, 1, 2},
};
constexpr int kPresetCount = static_cast<int>(std::size(kPresets));
}  // namespace

SfxLabTab::SfxLabTab(EngineHub* hub, QWidget* parent)
    : QWidget(parent),
      hub_(hub)
{
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };

    auto* root = new QVBoxLayout(this);
    full_preview_timer_ = new QTimer(this);
    full_preview_timer_->setInterval(16);
    connect(full_preview_timer_, &QTimer::timeout, this, [this]() { tick_full_preview(); });

    auto* quick_box = new QGroupBox(ui("SFX Lab (Driver polling)", "SFX Lab (Polling driver)"), this);
    auto* quick_layout = new QVBoxLayout(quick_box);

    sfx_preset_ = new QComboBox(this);
    for (const auto& preset : kPresets) {
        sfx_preset_->addItem(QString::fromUtf8(preset.name));
    }
    auto* load_builtin_btn = new QPushButton(ui("Charger driver integre", "Load built-in driver"), this);
    auto* apply_preset_btn = new QPushButton(ui("Appliquer preset", "Apply preset"), this);
    auto* save_project_sfx_btn = new QPushButton(ui("Sauver au projet", "Save to project"), this);

    tone_on_ = new QCheckBox(ui("Activer tone", "Enable tone"), this);
    tone_on_->setChecked(true);
    tone_ch_ = new QSpinBox(this);
    tone_ch_->setRange(0, 2);
    tone_ch_->setValue(0);
    tight_spin(tone_ch_, 74);
    tone_div_ = new QSpinBox(this);
    tone_div_->setRange(1, 1023);
    tone_div_->setValue(218);
    tight_spin(tone_div_, 86);
    tone_attn_ = new QSpinBox(this);
    tone_attn_->setRange(0, 15);
    tone_attn_->setValue(2);
    tight_spin(tone_attn_, 74);
    tone_frames_ = new QSpinBox(this);
    tone_frames_->setRange(0, 255);
    tone_frames_->setValue(6);
    tight_spin(tone_frames_, 72);

    tone_sw_on_ = new QCheckBox(ui("Activer", "Enable"), this);
    tone_sw_on_->setChecked(false);
    tone_sw_end_ = new QSpinBox(this);
    tone_sw_end_->setRange(1, 1023);
    tone_sw_end_->setValue(218);
    tight_spin(tone_sw_end_, 86);
    tone_sw_step_ = new QSpinBox(this);
    tone_sw_step_->setRange(-32768, 32767);
    tone_sw_step_->setValue(1);
    tight_spin(tone_sw_step_, 96);
    tone_sw_speed_ = new QSpinBox(this);
    tone_sw_speed_->setRange(1, 30);
    tone_sw_speed_->setValue(1);
    tight_spin(tone_sw_speed_, 74);
    tone_sw_ping_ = new QCheckBox(ui("Ping-Pong", "Ping-Pong"), this);
    tone_sw_ping_->setChecked(false);

    tone_env_on_ = new QCheckBox(ui("Activer", "Enable"), this);
    tone_env_on_->setChecked(false);
    tone_env_step_ = new QSpinBox(this);
    tone_env_step_->setRange(1, 4);
    tone_env_step_->setValue(1);
    tight_spin(tone_env_step_, 74);
    tone_env_spd_ = new QSpinBox(this);
    tone_env_spd_->setRange(1, 10);
    tone_env_spd_->setValue(1);
    tight_spin(tone_env_spd_, 74);

    tone_adsr_on_ = new QCheckBox(ui("Activer ADSR", "Enable ADSR"), this);
    tone_adsr_on_->setChecked(false);
    tone_adsr_ar_ = new QSpinBox(this);
    tone_adsr_ar_->setRange(0, 31);
    tone_adsr_ar_->setValue(0);
    tight_spin(tone_adsr_ar_, 74);
    tone_adsr_dr_ = new QSpinBox(this);
    tone_adsr_dr_->setRange(0, 31);
    tone_adsr_dr_->setValue(2);
    tight_spin(tone_adsr_dr_, 74);
    tone_adsr_sl_ = new QSpinBox(this);
    tone_adsr_sl_->setRange(0, 15);
    tone_adsr_sl_->setValue(8);
    tight_spin(tone_adsr_sl_, 74);
    tone_adsr_sr_ = new QSpinBox(this);
    tone_adsr_sr_->setRange(0, 31);
    tone_adsr_sr_->setValue(0);
    tight_spin(tone_adsr_sr_, 74);
    tone_adsr_rr_ = new QSpinBox(this);
    tone_adsr_rr_->setRange(0, 31);
    tone_adsr_rr_->setValue(2);
    tight_spin(tone_adsr_rr_, 74);

    auto add_lfo_waves = [&](QComboBox* cb) {
        cb->addItem(ui("Triangle", "Triangle"), 0);
        cb->addItem(ui("Carre", "Square"), 1);
        cb->addItem("Saw", 2);
        cb->addItem(ui("Sweep +", "Sweep +"), 3);
        cb->addItem(ui("Sweep -", "Sweep -"), 4);
    };

    tone_lfo1_on_ = new QCheckBox("LFO1", this);
    tone_lfo1_on_->setChecked(false);
    tone_lfo1_wave_ = new QComboBox(this);
    add_lfo_waves(tone_lfo1_wave_);
    set_combo_data(tone_lfo1_wave_, 0);
    tone_lfo1_hold_ = new QSpinBox(this);
    tone_lfo1_hold_->setRange(0, 255);
    tone_lfo1_hold_->setValue(0);
    tight_spin(tone_lfo1_hold_, 72);
    tone_lfo1_rate_ = new QSpinBox(this);
    tone_lfo1_rate_->setRange(0, 255);
    tone_lfo1_rate_->setValue(1);
    tight_spin(tone_lfo1_rate_, 72);
    tone_lfo1_depth_ = new QSpinBox(this);
    tone_lfo1_depth_->setRange(0, 255);
    tone_lfo1_depth_->setValue(0);
    tight_spin(tone_lfo1_depth_, 72);

    tone_lfo2_on_ = new QCheckBox("LFO2", this);
    tone_lfo2_on_->setChecked(false);
    tone_lfo2_wave_ = new QComboBox(this);
    add_lfo_waves(tone_lfo2_wave_);
    set_combo_data(tone_lfo2_wave_, 0);
    tone_lfo2_hold_ = new QSpinBox(this);
    tone_lfo2_hold_->setRange(0, 255);
    tone_lfo2_hold_->setValue(0);
    tight_spin(tone_lfo2_hold_, 72);
    tone_lfo2_rate_ = new QSpinBox(this);
    tone_lfo2_rate_->setRange(0, 255);
    tone_lfo2_rate_->setValue(1);
    tight_spin(tone_lfo2_rate_, 72);
    tone_lfo2_depth_ = new QSpinBox(this);
    tone_lfo2_depth_->setRange(0, 255);
    tone_lfo2_depth_->setValue(0);
    tight_spin(tone_lfo2_depth_, 72);

    tone_lfo_algo_ = new QSpinBox(this);
    tone_lfo_algo_->setRange(0, 7);
    tone_lfo_algo_->setValue(1);
    tight_spin(tone_lfo_algo_, 74);

    noise_on_ = new QCheckBox(ui("Activer noise", "Enable noise"), this);
    noise_on_->setChecked(true);
    noise_rate_ = new QComboBox(this);
    noise_rate_->addItem(ui("Haut (0)", "High (0)"), 0);
    noise_rate_->addItem(ui("Moyen (1)", "Medium (1)"), 1);
    noise_rate_->addItem(ui("Bas (2)", "Low (2)"), 2);
    noise_rate_->addItem(ui("Lie T2 (3)", "Tie to T2 (3)"), 3);
    set_combo_data(noise_rate_, 1);
    noise_rate_->setMinimumWidth(120);
    noise_type_ = new QComboBox(this);
    noise_type_->addItem(ui("Periodique (0)", "Periodic (0)"), 0);
    noise_type_->addItem(ui("Blanc (1)", "White (1)"), 1);
    set_combo_data(noise_type_, 1);
    noise_type_->setMinimumWidth(120);
    noise_attn_ = new QSpinBox(this);
    noise_attn_->setRange(0, 15);
    noise_attn_->setValue(2);
    tight_spin(noise_attn_, 74);
    noise_frames_ = new QSpinBox(this);
    noise_frames_->setRange(0, 255);
    noise_frames_->setValue(4);
    tight_spin(noise_frames_, 72);

    noise_burst_ = new QCheckBox(ui("Activer", "Enable"), this);
    noise_burst_->setChecked(false);
    noise_burst_dur_ = new QSpinBox(this);
    noise_burst_dur_->setRange(1, 30);
    noise_burst_dur_->setValue(1);
    tight_spin(noise_burst_dur_, 74);

    noise_env_on_ = new QCheckBox(ui("Activer", "Enable"), this);
    noise_env_on_->setChecked(false);
    noise_env_step_ = new QSpinBox(this);
    noise_env_step_->setRange(1, 4);
    noise_env_step_->setValue(1);
    tight_spin(noise_env_step_, 74);
    noise_env_spd_ = new QSpinBox(this);
    noise_env_spd_->setRange(1, 10);
    noise_env_spd_->setValue(1);
    tight_spin(noise_env_spd_, 74);

    // General
    auto* general_box = new QGroupBox(ui("General", "General"), this);
    auto* general_layout = new QHBoxLayout(general_box);
    general_layout->addWidget(new QLabel(ui("Preset:", "Preset:"), this));
    general_layout->addWidget(sfx_preset_, 1);
    general_layout->addWidget(apply_preset_btn);
    general_layout->addWidget(load_builtin_btn);

    // Tone
    auto* tone_box = new QGroupBox("Tone", this);
    auto* tone_layout = new QHBoxLayout(tone_box);
    tone_layout->addWidget(tone_on_);
    tone_layout->addWidget(new QLabel("Ch:", this));
    tone_layout->addWidget(tone_ch_);
    tone_layout->addWidget(new QLabel("Div:", this));
    tone_layout->addWidget(tone_div_);
    tone_layout->addWidget(new QLabel("Attn:", this));
    tone_layout->addWidget(tone_attn_);
    tone_layout->addWidget(new QLabel("Frames:", this));
    tone_layout->addWidget(tone_frames_);
    tone_layout->addStretch();

    // Tone Sweep
    auto* tone_sweep_box = new QGroupBox(ui("Sweep tone", "Tone sweep"), this);
    auto* tone_sweep_layout = new QHBoxLayout(tone_sweep_box);
    tone_sweep_layout->addWidget(tone_sw_on_);
    tone_sweep_layout->addWidget(new QLabel("End:", this));
    tone_sweep_layout->addWidget(tone_sw_end_);
    tone_sweep_layout->addWidget(new QLabel("Step:", this));
    tone_sweep_layout->addWidget(tone_sw_step_);
    tone_sweep_layout->addWidget(new QLabel("Speed:", this));
    tone_sweep_layout->addWidget(tone_sw_speed_);
    tone_sweep_layout->addWidget(tone_sw_ping_);
    tone_sweep_layout->addStretch();

    // Tone Envelope
    auto* tone_env_box = new QGroupBox(ui("Envelope tone (legacy)", "Tone envelope (legacy)"), this);
    auto* tone_env_layout = new QHBoxLayout(tone_env_box);
    tone_env_layout->addWidget(tone_env_on_);
    tone_env_layout->addWidget(new QLabel("Step:", this));
    tone_env_layout->addWidget(tone_env_step_);
    tone_env_layout->addWidget(new QLabel("Speed:", this));
    tone_env_layout->addWidget(tone_env_spd_);
    tone_env_layout->addStretch();

    // Tone ADSR
    auto* tone_adsr_box = new QGroupBox("Tone ADSR", this);
    auto* tone_adsr_layout = new QHBoxLayout(tone_adsr_box);
    tone_adsr_layout->addWidget(tone_adsr_on_);
    tone_adsr_layout->addWidget(new QLabel("AR:", this));
    tone_adsr_layout->addWidget(tone_adsr_ar_);
    tone_adsr_layout->addWidget(new QLabel("DR:", this));
    tone_adsr_layout->addWidget(tone_adsr_dr_);
    tone_adsr_layout->addWidget(new QLabel("SL:", this));
    tone_adsr_layout->addWidget(tone_adsr_sl_);
    tone_adsr_layout->addWidget(new QLabel("SR:", this));
    tone_adsr_layout->addWidget(tone_adsr_sr_);
    tone_adsr_layout->addWidget(new QLabel("RR:", this));
    tone_adsr_layout->addWidget(tone_adsr_rr_);
    tone_adsr_layout->addStretch();

    // Tone LFO
    auto* tone_lfo_box = new QGroupBox("Tone LFO / MOD2", this);
    auto* tone_lfo_grid = new QGridLayout(tone_lfo_box);
    tone_lfo_grid->addWidget(tone_lfo1_on_, 0, 0);
    tone_lfo_grid->addWidget(new QLabel("Wave:", this), 0, 1);
    tone_lfo_grid->addWidget(tone_lfo1_wave_, 0, 2);
    tone_lfo_grid->addWidget(new QLabel("Hold:", this), 0, 3);
    tone_lfo_grid->addWidget(tone_lfo1_hold_, 0, 4);
    tone_lfo_grid->addWidget(new QLabel("Rate:", this), 0, 5);
    tone_lfo_grid->addWidget(tone_lfo1_rate_, 0, 6);
    tone_lfo_grid->addWidget(new QLabel("Depth:", this), 0, 7);
    tone_lfo_grid->addWidget(tone_lfo1_depth_, 0, 8);

    tone_lfo_grid->addWidget(tone_lfo2_on_, 1, 0);
    tone_lfo_grid->addWidget(new QLabel("Wave:", this), 1, 1);
    tone_lfo_grid->addWidget(tone_lfo2_wave_, 1, 2);
    tone_lfo_grid->addWidget(new QLabel("Hold:", this), 1, 3);
    tone_lfo_grid->addWidget(tone_lfo2_hold_, 1, 4);
    tone_lfo_grid->addWidget(new QLabel("Rate:", this), 1, 5);
    tone_lfo_grid->addWidget(tone_lfo2_rate_, 1, 6);
    tone_lfo_grid->addWidget(new QLabel("Depth:", this), 1, 7);
    tone_lfo_grid->addWidget(tone_lfo2_depth_, 1, 8);
    tone_lfo_grid->addWidget(new QLabel("Algo:", this), 1, 9);
    tone_lfo_grid->addWidget(tone_lfo_algo_, 1, 10);
    tone_lfo_grid->setColumnStretch(11, 1);

    // Noise
    auto* noise_box = new QGroupBox("Noise", this);
    auto* noise_layout = new QHBoxLayout(noise_box);
    noise_layout->addWidget(noise_on_);
    noise_layout->addWidget(new QLabel(ui("Rate:", "Rate:"), this));
    noise_layout->addWidget(noise_rate_);
    noise_layout->addWidget(new QLabel(ui("Type:", "Type:"), this));
    noise_layout->addWidget(noise_type_);
    noise_layout->addWidget(new QLabel("Attn:", this));
    noise_layout->addWidget(noise_attn_);
    noise_layout->addWidget(new QLabel("Frames:", this));
    noise_layout->addWidget(noise_frames_);
    noise_layout->addStretch();

    // Noise Burst
    auto* noise_burst_box = new QGroupBox(ui("Burst noise", "Noise burst"), this);
    auto* noise_burst_layout = new QHBoxLayout(noise_burst_box);
    noise_burst_layout->addWidget(noise_burst_);
    noise_burst_layout->addWidget(new QLabel(ui("Duree:", "Duration:"), this));
    noise_burst_layout->addWidget(noise_burst_dur_);
    noise_burst_layout->addStretch();

    // Noise Envelope
    auto* noise_env_box = new QGroupBox(ui("Envelope noise", "Noise envelope"), this);
    auto* noise_env_layout = new QHBoxLayout(noise_env_box);
    noise_env_layout->addWidget(noise_env_on_);
    noise_env_layout->addWidget(new QLabel("Step:", this));
    noise_env_layout->addWidget(noise_env_step_);
    noise_env_layout->addWidget(new QLabel("Speed:", this));
    noise_env_layout->addWidget(noise_env_spd_);
    noise_env_layout->addStretch();

    // Preview
    auto* preview_box = new QGroupBox(ui("Preview", "Preview"), this);
    auto* preview_layout = new QVBoxLayout(preview_box);
    auto* preview_mode_row = new QHBoxLayout();
    preview_mode_row->addWidget(new QLabel(ui("Mode:", "Mode:"), this));
    preview_mode_ = new QComboBox(this);
    preview_mode_->addItem(ui("Driver-faithful (force)", "Driver-faithful (forced)"));
    preview_mode_->setCurrentIndex(0);
    preview_mode_->setEnabled(false);
    preview_mode_->setToolTip(
        ui("Le preview SFX utilise toujours le chemin driver-like (frames/sweep/env/burst/ADSR/LFO).",
           "SFX preview always uses the driver-like path (frames/sweep/env/burst/ADSR/LFO)."));
    preview_mode_row->addWidget(preview_mode_);
    preview_mode_row->addStretch();
    preview_layout->addLayout(preview_mode_row);

    auto* preview_buttons = new QHBoxLayout();
    auto* play_tone_btn = new QPushButton(ui("Play Tone", "Play tone"), this);
    auto* play_noise_btn = new QPushButton(ui("Play Noise", "Play noise"), this);
    auto* play_full_btn = new QPushButton(ui("Play Full SFX", "Play full SFX"), this);
    auto* silence_btn = new QPushButton(ui("Silence", "Silence"), this);
    preview_buttons->addWidget(play_tone_btn);
    preview_buttons->addWidget(play_noise_btn);
    preview_buttons->addWidget(play_full_btn);
    preview_buttons->addWidget(silence_btn);
    preview_buttons->addStretch();
    preview_layout->addLayout(preview_buttons);

    auto* meter_row = new QHBoxLayout();
    meter_row->addWidget(new QLabel(ui("Niveau sortie:", "Output meter:"), this));
    output_meter_ = new QProgressBar(this);
    output_meter_->setRange(0, 100);
    output_meter_->setValue(0);
    output_meter_->setFormat("%p%");
    output_meter_->setMinimumWidth(180);
    output_meter_label_ = new QLabel(ui("Audio off", "Audio off"), this);
    meter_row->addWidget(output_meter_, 1);
    meter_row->addWidget(output_meter_label_);
    preview_layout->addLayout(meter_row);

    // Project
    auto* project_box = new QGroupBox(ui("Projet", "Project"), this);
    auto* project_layout = new QHBoxLayout(project_box);
    project_layout->addWidget(save_project_sfx_btn);
    project_layout->addStretch();

    quick_layout->addWidget(general_box);
    quick_layout->addWidget(tone_box);
    quick_layout->addWidget(tone_sweep_box);
    quick_layout->addWidget(tone_env_box);
    quick_layout->addWidget(tone_adsr_box);
    quick_layout->addWidget(tone_lfo_box);
    quick_layout->addWidget(noise_box);
    quick_layout->addWidget(noise_burst_box);
    quick_layout->addWidget(noise_env_box);
    quick_layout->addWidget(preview_box);
    quick_layout->addWidget(project_box);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setPlaceholderText(ui("Log SFX Lab...", "SFX Lab log..."));

    root->addWidget(quick_box);
    root->addWidget(log_, 1);

    // --- Connections ---

    connect(load_builtin_btn, &QPushButton::clicked, this, [this]() {
        if (!hub_) {
            append_log("Engine hub missing");
            return;
        }
        if (!hub_->load_builtin_polling()) {
            append_log("Built-in driver load failed");
            return;
        }
        append_log("Built-in polling driver loaded");
    });

    connect(apply_preset_btn, &QPushButton::clicked, this, [this]() {
        if (!sfx_preset_) {
            return;
        }
        int idx = sfx_preset_->currentIndex();
        if (idx < 0 || idx >= kPresetCount) {
            return;
        }
        const auto& p = kPresets[idx];
        tone_on_->setChecked(p.tone_on != 0);
        tone_ch_->setValue(p.tone_ch);
        tone_div_->setValue(p.tone_div);
        tone_attn_->setValue(p.tone_attn);
        tone_frames_->setValue(p.tone_frames);
        tone_sw_on_->setChecked(p.tone_sw_on != 0);
        tone_sw_end_->setValue(p.tone_sw_end);
        tone_sw_step_->setValue(p.tone_sw_step);
        tone_sw_speed_->setValue(p.tone_sw_speed);
        tone_sw_ping_->setChecked(p.tone_sw_ping != 0);
        tone_env_on_->setChecked(p.tone_env_on != 0);
        tone_env_step_->setValue(p.tone_env_step);
        tone_env_spd_->setValue(p.tone_env_spd);
        noise_on_->setChecked(p.noise_on != 0);
        set_combo_data(noise_rate_, p.noise_rate);
        set_combo_data(noise_type_, p.noise_type);
        noise_attn_->setValue(p.noise_attn);
        noise_frames_->setValue(p.noise_frames);
        noise_burst_->setChecked(p.noise_burst != 0);
        noise_burst_dur_->setValue(p.noise_burst_dur);
        noise_env_on_->setChecked(p.noise_env_on != 0);
        noise_env_step_->setValue(p.noise_env_step);
        noise_env_spd_->setValue(p.noise_env_spd);
        tone_adsr_on_->setChecked(p.tone_adsr_on != 0);
        tone_adsr_ar_->setValue(p.tone_adsr_ar);
        tone_adsr_dr_->setValue(p.tone_adsr_dr);
        tone_adsr_sl_->setValue(p.tone_adsr_sl);
        tone_adsr_sr_->setValue(p.tone_adsr_sr);
        tone_adsr_rr_->setValue(p.tone_adsr_rr);
        tone_lfo1_on_->setChecked(p.tone_lfo1_on != 0);
        set_combo_data(tone_lfo1_wave_, p.tone_lfo1_wave);
        tone_lfo1_hold_->setValue(p.tone_lfo1_hold);
        tone_lfo1_rate_->setValue(p.tone_lfo1_rate);
        tone_lfo1_depth_->setValue(p.tone_lfo1_depth);
        tone_lfo2_on_->setChecked(p.tone_lfo2_on != 0);
        set_combo_data(tone_lfo2_wave_, p.tone_lfo2_wave);
        tone_lfo2_hold_->setValue(p.tone_lfo2_hold);
        tone_lfo2_rate_->setValue(p.tone_lfo2_rate);
        tone_lfo2_depth_->setValue(p.tone_lfo2_depth);
        tone_lfo_algo_->setValue(p.tone_lfo_algo);
        append_log(QString("Preset applied: %1").arg(sfx_preset_->currentText()));
    });

    connect(play_tone_btn, &QPushButton::clicked, this, [this]() {
        start_preview_with_mask(true, false);
    });

    connect(play_noise_btn, &QPushButton::clicked, this, [this]() {
        start_preview_with_mask(false, true);
    });

    connect(play_full_btn, &QPushButton::clicked, this, [this]() {
        start_full_preview();
    });

    connect(silence_btn, &QPushButton::clicked, this, [this]() {
        stop_full_preview(false);
        if (!hub_ || !hub_->engine_ready()) {
            append_log("Engine not ready");
            return;
        }
        if (!hub_->driver_loaded()) {
            append_log("Driver not loaded, loading built-in polling driver...");
            if (!hub_->load_builtin_polling()) {
                append_log("Built-in driver load failed");
                return;
            }
        } else if (!hub_->driver_is_polling()) {
            append_log("Polling driver not loaded");
            return;
        }
        if (hub_->driver_is_polling()) {
            hub_->set_step_z80(false);
        }
        if (!hub_->ensure_audio_running(44100)) {
            const QString err = hub_->last_audio_error();
            append_log(err.isEmpty() ? "Audio start failed" : QString("Audio start failed: %1").arg(err));
            return;
        }
        if (!hub_->polling().silence_all()) {
            append_log("Silence dropped (driver busy) -> direct PSG fallback");
        }
        psg_helpers::DirectSilenceTone(hub_->engine(), 0);
        psg_helpers::DirectSilenceTone(hub_->engine(), 1);
        psg_helpers::DirectSilenceTone(hub_->engine(), 2);
        psg_helpers::DirectSilenceNoise(hub_->engine());
    });

    connect(save_project_sfx_btn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString default_name = project_edit_sfx_name_.isEmpty()
            ? QStringLiteral("New SFX")
            : project_edit_sfx_name_;
        const QString name = QInputDialog::getText(
            this, "Save SFX to Project", "SFX name:", QLineEdit::Normal, default_name, &ok);
        if (!ok) return;
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) return;

        if (!project_edit_sfx_id_.isEmpty()) {
            QMessageBox choice(this);
            choice.setWindowTitle("SFX projet");
            choice.setIcon(QMessageBox::Question);
            choice.setText(QString("SFX lie au projet: %1").arg(project_edit_sfx_name_));
            choice.setInformativeText("Mettre a jour ce SFX ou en creer une copie ?");
            QPushButton* update_btn = choice.addButton("Mettre a jour", QMessageBox::AcceptRole);
            QPushButton* copy_btn = choice.addButton("Creer une copie", QMessageBox::ActionRole);
            choice.addButton(QMessageBox::Cancel);
            choice.setDefaultButton(update_btn);
            choice.exec();

            if (choice.clickedButton() == update_btn) {
                emit update_sfx_in_project_requested(
                    project_edit_sfx_id_,
                    trimmed,
                    tone_on_->isChecked() ? 1 : 0,
                    tone_ch_->value(),
                    tone_div_->value(),
                    tone_attn_->value(),
                    tone_frames_->value(),
                    tone_sw_on_->isChecked() ? 1 : 0,
                    tone_sw_end_->value(),
                    tone_sw_step_->value(),
                    tone_sw_speed_->value(),
                    tone_sw_ping_->isChecked() ? 1 : 0,
                    tone_env_on_->isChecked() ? 1 : 0,
                    tone_env_step_->value(),
                    tone_env_spd_->value(),
                    noise_on_->isChecked() ? 1 : 0,
                    combo_data(noise_rate_),
                    combo_data(noise_type_),
                    noise_attn_->value(),
                    noise_frames_->value(),
                    noise_burst_->isChecked() ? 1 : 0,
                    noise_burst_dur_->value(),
                    noise_env_on_->isChecked() ? 1 : 0,
                    noise_env_step_->value(),
                    noise_env_spd_->value(),
                    tone_adsr_on_->isChecked() ? 1 : 0,
                    tone_adsr_ar_->value(),
                    tone_adsr_dr_->value(),
                    tone_adsr_sl_->value(),
                    tone_adsr_sr_->value(),
                    tone_adsr_rr_->value(),
                    tone_lfo1_on_->isChecked() ? 1 : 0,
                    combo_data(tone_lfo1_wave_),
                    tone_lfo1_hold_->value(),
                    tone_lfo1_rate_->value(),
                    tone_lfo1_depth_->value(),
                    tone_lfo2_on_->isChecked() ? 1 : 0,
                    combo_data(tone_lfo2_wave_),
                    tone_lfo2_hold_->value(),
                    tone_lfo2_rate_->value(),
                    tone_lfo2_depth_->value(),
                    tone_lfo_algo_->value());
                project_edit_sfx_name_ = trimmed;
                append_log(QString("SFX mis a jour: %1").arg(trimmed));
                return;
            }
            if (choice.clickedButton() != copy_btn) {
                return;
            }
        }

        emit save_sfx_to_project_requested(
            trimmed,
            tone_on_->isChecked() ? 1 : 0,
            tone_ch_->value(),
            tone_div_->value(),
            tone_attn_->value(),
            tone_frames_->value(),
            tone_sw_on_->isChecked() ? 1 : 0,
            tone_sw_end_->value(),
            tone_sw_step_->value(),
            tone_sw_speed_->value(),
            tone_sw_ping_->isChecked() ? 1 : 0,
            tone_env_on_->isChecked() ? 1 : 0,
            tone_env_step_->value(),
            tone_env_spd_->value(),
            noise_on_->isChecked() ? 1 : 0,
            combo_data(noise_rate_),
            combo_data(noise_type_),
            noise_attn_->value(),
            noise_frames_->value(),
            noise_burst_->isChecked() ? 1 : 0,
            noise_burst_dur_->value(),
            noise_env_on_->isChecked() ? 1 : 0,
            noise_env_step_->value(),
            noise_env_spd_->value(),
            tone_adsr_on_->isChecked() ? 1 : 0,
            tone_adsr_ar_->value(),
            tone_adsr_dr_->value(),
            tone_adsr_sl_->value(),
            tone_adsr_sr_->value(),
            tone_adsr_rr_->value(),
            tone_lfo1_on_->isChecked() ? 1 : 0,
            combo_data(tone_lfo1_wave_),
            tone_lfo1_hold_->value(),
            tone_lfo1_rate_->value(),
            tone_lfo1_depth_->value(),
            tone_lfo2_on_->isChecked() ? 1 : 0,
            combo_data(tone_lfo2_wave_),
            tone_lfo2_hold_->value(),
            tone_lfo2_rate_->value(),
            tone_lfo2_depth_->value(),
            tone_lfo_algo_->value());
        append_log(QString("SFX ajoute au projet: %1").arg(trimmed));
    });

    meter_timer_ = new QTimer(this);
    meter_timer_->setInterval(100);
    connect(meter_timer_, &QTimer::timeout, this, &SfxLabTab::update_output_meter);
    meter_timer_->start();
}

void SfxLabTab::load_project_sfx(const ProjectSfxEntry& entry) {
    project_edit_sfx_id_ = entry.id;
    project_edit_sfx_name_ = entry.name;

    tone_on_->setChecked(entry.tone_on != 0);
    tone_ch_->setValue(entry.tone_ch);
    tone_div_->setValue(entry.tone_div);
    tone_attn_->setValue(entry.tone_attn);
    tone_frames_->setValue(entry.tone_frames);
    tone_sw_on_->setChecked(entry.tone_sw_on != 0);
    tone_sw_end_->setValue(entry.tone_sw_end);
    tone_sw_step_->setValue(entry.tone_sw_step);
    tone_sw_speed_->setValue(entry.tone_sw_speed);
    tone_sw_ping_->setChecked(entry.tone_sw_ping != 0);
    tone_env_on_->setChecked(entry.tone_env_on != 0);
    tone_env_step_->setValue(entry.tone_env_step);
    tone_env_spd_->setValue(entry.tone_env_spd);
    tone_adsr_on_->setChecked(entry.tone_adsr_on != 0);
    tone_adsr_ar_->setValue(entry.tone_adsr_ar);
    tone_adsr_dr_->setValue(entry.tone_adsr_dr);
    tone_adsr_sl_->setValue(entry.tone_adsr_sl);
    tone_adsr_sr_->setValue(entry.tone_adsr_sr);
    tone_adsr_rr_->setValue(entry.tone_adsr_rr);
    tone_lfo1_on_->setChecked(entry.tone_lfo1_on != 0);
    set_combo_data(tone_lfo1_wave_, entry.tone_lfo1_wave);
    tone_lfo1_hold_->setValue(entry.tone_lfo1_hold);
    tone_lfo1_rate_->setValue(entry.tone_lfo1_rate);
    tone_lfo1_depth_->setValue(entry.tone_lfo1_depth);
    tone_lfo2_on_->setChecked(entry.tone_lfo2_on != 0);
    set_combo_data(tone_lfo2_wave_, entry.tone_lfo2_wave);
    tone_lfo2_hold_->setValue(entry.tone_lfo2_hold);
    tone_lfo2_rate_->setValue(entry.tone_lfo2_rate);
    tone_lfo2_depth_->setValue(entry.tone_lfo2_depth);
    tone_lfo_algo_->setValue(entry.tone_lfo_algo);

    noise_on_->setChecked(entry.noise_on != 0);
    set_combo_data(noise_rate_, entry.noise_rate);
    set_combo_data(noise_type_, entry.noise_type);
    noise_attn_->setValue(entry.noise_attn);
    noise_frames_->setValue(entry.noise_frames);
    noise_burst_->setChecked(entry.noise_burst != 0);
    noise_burst_dur_->setValue(entry.noise_burst_dur);
    noise_env_on_->setChecked(entry.noise_env_on != 0);
    noise_env_step_->setValue(entry.noise_env_step);
    noise_env_spd_->setValue(entry.noise_env_spd);

    append_log(QString("SFX charge depuis le projet: %1").arg(entry.name));
}

void SfxLabTab::stop_full_preview(bool silence) {
    if (full_preview_timer_ && full_preview_timer_->isActive()) {
        full_preview_timer_->stop();
    }
    const bool had_tone = tone_preview_.active;
    const bool had_noise = noise_preview_.active;
    tone_preview_ = TonePreviewState{};
    noise_preview_ = NoisePreviewState{};

    if (silence && hub_ && hub_->engine_ready() && (had_tone || had_noise)) {
        if (had_tone) {
            psg_helpers::DirectSilenceTone(hub_->engine(), 0);
            psg_helpers::DirectSilenceTone(hub_->engine(), 1);
            psg_helpers::DirectSilenceTone(hub_->engine(), 2);
        }
        if (had_noise) {
            psg_helpers::DirectSilenceNoise(hub_->engine());
        }
    }
}

bool SfxLabTab::use_faithful_preview_mode() const {
    return true;
}

void SfxLabTab::start_preview_with_mask(bool use_tone, bool use_noise) {
    stop_full_preview(false);

    if (!hub_) {
        append_log("Engine hub missing");
        return;
    }

    const bool want_tone = use_tone && tone_on_->isChecked();
    const bool want_noise = use_noise && noise_on_->isChecked();
    if (!want_tone && !want_noise) {
        append_log("Requested preview path is disabled in current config");
        return;
    }

    if (want_tone && want_noise) {
        append_log("Play full SFX (driver-like preview)");
    } else if (want_tone) {
        append_log("Play tone (driver-like preview)");
    } else {
        append_log("Play noise (driver-like preview)");
    }

    hub_->set_step_z80(false);
    if (!hub_->ensure_audio_running(44100)) {
        const QString err = hub_->last_audio_error();
        append_log(err.isEmpty() ? "Audio start failed" : QString("Audio start failed: %1").arg(err));
        return;
    }
    if (!hub_->engine_ready() || !hub_->audio_running()) {
        append_log("Audio engine not ready");
        return;
    }

    if (want_tone) {
        tone_preview_.active = true;
        tone_preview_.ch = clamp_i(tone_ch_->value(), 0, 2);
        tone_preview_.div_base = clamp_i(tone_div_->value(), 1, 1023);
        tone_preview_.div_cur = tone_preview_.div_base;
        tone_preview_.attn_base = clamp_i(tone_attn_->value(), 0, 15);
        tone_preview_.attn_cur = tone_preview_.attn_base;
        tone_preview_.rendered_div = tone_preview_.div_cur;
        tone_preview_.rendered_attn = tone_preview_.attn_cur;
        tone_preview_.frames = clamp_i(tone_frames_->value(), 0, 255);
        if (tone_preview_.frames == 0) tone_preview_.frames = 1; // one-shot behavior

        tone_preview_.sw_on = tone_sw_on_->isChecked();
        tone_preview_.sw_end = clamp_i(tone_sw_end_->value(), 1, 1023);
        int sw_step = tone_sw_step_->value();
        if (tone_preview_.sw_on && sw_step == 0) sw_step = 1;
        tone_preview_.sw_dir = (sw_step < 0) ? -1 : 1;
        tone_preview_.sw_step_abs = (sw_step < 0) ? -sw_step : sw_step;
        if (tone_preview_.sw_step_abs <= 0) {
            tone_preview_.sw_step_abs = 1;
        }
        tone_preview_.sw_speed = clamp_i(tone_sw_speed_->value(), 1, 30);
        tone_preview_.sw_counter = 0;
        tone_preview_.sw_ping = tone_sw_ping_->isChecked();

        tone_preview_.env_on = tone_env_on_->isChecked();
        tone_preview_.env_step = clamp_i(tone_env_step_->value(), 1, 4);
        tone_preview_.env_spd = clamp_i(tone_env_spd_->value(), 1, 10);
        tone_preview_.env_counter = 0;
        tone_preview_.adsr_on = tone_adsr_on_->isChecked();
        tone_preview_.adsr_attack = clamp_i(tone_adsr_ar_->value(), 0, 31);
        tone_preview_.adsr_decay = clamp_i(tone_adsr_dr_->value(), 0, 31);
        tone_preview_.adsr_sustain = clamp_i(tone_adsr_sl_->value(), 0, 15);
        tone_preview_.adsr_sustain_rate = clamp_i(tone_adsr_sr_->value(), 0, 31);
        tone_preview_.adsr_release = clamp_i(tone_adsr_rr_->value(), 0, 31);
        tone_preview_.adsr_phase = 0;
        tone_preview_.adsr_counter = 0;

        tone_preview_.lfo1_on = tone_lfo1_on_->isChecked();
        tone_preview_.lfo1_wave = clamp_i(combo_data(tone_lfo1_wave_), 0, 4);
        tone_preview_.lfo1_hold = clamp_i(tone_lfo1_hold_->value(), 0, 255);
        tone_preview_.lfo1_rate = clamp_i(tone_lfo1_rate_->value(), 0, 255);
        tone_preview_.lfo1_depth = clamp_i(tone_lfo1_depth_->value(), 0, 255);
        tone_preview_.lfo1_hold_counter = tone_preview_.lfo1_hold;
        tone_preview_.lfo1_counter = tone_preview_.lfo1_rate;
        tone_preview_.lfo1_sign = 1;
        tone_preview_.lfo1_delta = 0;

        tone_preview_.lfo2_on = tone_lfo2_on_->isChecked();
        tone_preview_.lfo2_wave = clamp_i(combo_data(tone_lfo2_wave_), 0, 4);
        tone_preview_.lfo2_hold = clamp_i(tone_lfo2_hold_->value(), 0, 255);
        tone_preview_.lfo2_rate = clamp_i(tone_lfo2_rate_->value(), 0, 255);
        tone_preview_.lfo2_depth = clamp_i(tone_lfo2_depth_->value(), 0, 255);
        tone_preview_.lfo2_hold_counter = tone_preview_.lfo2_hold;
        tone_preview_.lfo2_counter = tone_preview_.lfo2_rate;
        tone_preview_.lfo2_sign = 1;
        tone_preview_.lfo2_delta = 0;

        tone_preview_.lfo_algo = clamp_i(tone_lfo_algo_->value(), 0, 7);
        tone_preview_.lfo_pitch_delta = 0;
        tone_preview_.lfo_attn_delta = 0;
        if (tone_preview_.lfo1_depth == 0 || tone_preview_.lfo1_rate == 0) {
            tone_preview_.lfo1_on = false;
        }
        if (tone_preview_.lfo2_depth == 0 || tone_preview_.lfo2_rate == 0) {
            tone_preview_.lfo2_on = false;
        }

        if (tone_preview_.adsr_on) {
            tone_preview_.attn_cur = 15;
            tone_preview_.adsr_phase = 1;
            tone_preview_.adsr_counter = tone_preview_.adsr_attack;
        }
        resolve_lfo_algo(
            tone_preview_.lfo_algo,
            tone_preview_.lfo1_delta,
            tone_preview_.lfo2_delta,
            tone_preview_.lfo_pitch_delta,
            tone_preview_.lfo_attn_delta);
        tone_preview_.rendered_div = clamp_i(
            tone_preview_.div_cur + tone_preview_.lfo_pitch_delta, 1, 1023);
        tone_preview_.rendered_attn = clamp_i(
            tone_preview_.attn_cur + tone_preview_.lfo_attn_delta, 0, 15);

        psg_helpers::DirectToneCh(
            hub_->engine(),
            tone_preview_.ch,
            static_cast<uint16_t>(tone_preview_.rendered_div),
            static_cast<uint8_t>(tone_preview_.rendered_attn));
    }

    if (want_noise) {
        noise_preview_.active = true;
        noise_preview_.rate = clamp_i(combo_data(noise_rate_), 0, 3);
        noise_preview_.type = clamp_i(combo_data(noise_type_), 0, 1);
        noise_preview_.attn_cur = clamp_i(noise_attn_->value(), 0, 15);
        noise_preview_.frames = clamp_i(noise_frames_->value(), 0, 255);
        noise_preview_.env_on = noise_env_on_->isChecked();
        noise_preview_.env_step = clamp_i(noise_env_step_->value(), 1, 4);
        noise_preview_.env_spd = clamp_i(noise_env_spd_->value(), 1, 10);
        noise_preview_.env_counter = 0;
        noise_preview_.burst = noise_burst_->isChecked();
        noise_preview_.burst_dur = clamp_i(noise_burst_dur_->value(), 1, 30);
        noise_preview_.burst_counter = noise_preview_.burst_dur;
        noise_preview_.burst_off = false;
        if (noise_preview_.frames == 0 && noise_preview_.burst) {
            noise_preview_.frames = noise_preview_.burst_dur;
        } else if (noise_preview_.frames == 0) {
            noise_preview_.frames = 1; // one-shot behavior
        }

        psg_helpers::DirectNoiseMode(
            hub_->engine(),
            static_cast<uint8_t>(noise_preview_.rate),
            static_cast<uint8_t>(noise_preview_.type));
        psg_helpers::DirectNoiseAttn(hub_->engine(), static_cast<uint8_t>(noise_preview_.attn_cur));
    }

    if (full_preview_timer_) {
        full_preview_timer_->start();
    }
}

void SfxLabTab::start_full_preview() {
    start_preview_with_mask(true, true);
}

void SfxLabTab::tick_full_preview() {
    if (!hub_ || !hub_->engine_ready() || !hub_->audio_running()) {
        stop_full_preview(false);
        return;
    }

    // Tone update (driver-like: ADSR/env + sweep + LFO then timer--)
    if (tone_preview_.active && tone_preview_.frames > 0) {
        bool dirty = false;
        if (tone_preview_.sw_on) {
            if (tone_preview_.sw_counter == 0) {
                int v = tone_preview_.div_cur + tone_preview_.sw_step_abs * tone_preview_.sw_dir;
                if (tone_preview_.sw_ping) {
                    const int minv = std::min(tone_preview_.div_base, tone_preview_.sw_end);
                    const int maxv = std::max(tone_preview_.div_base, tone_preview_.sw_end);
                    if (v <= minv) {
                        v = minv;
                        tone_preview_.sw_dir = 1;
                    } else if (v >= maxv) {
                        v = maxv;
                        tone_preview_.sw_dir = -1;
                    }
                } else {
                    if (tone_preview_.sw_dir < 0 && v <= tone_preview_.sw_end) {
                        v = tone_preview_.sw_end;
                        tone_preview_.sw_on = false;
                    } else if (tone_preview_.sw_dir > 0 && v >= tone_preview_.sw_end) {
                        v = tone_preview_.sw_end;
                        tone_preview_.sw_on = false;
                    }
                }
                v = std::min(1023, std::max(1, v));
                tone_preview_.div_cur = v;
                tone_preview_.sw_counter = tone_preview_.sw_speed;
                dirty = true;
            } else {
                tone_preview_.sw_counter--;
            }
        }

        if (tone_preview_.adsr_on && tone_preview_.adsr_phase > 0) {
            switch (tone_preview_.adsr_phase) {
            case 1: // ATK: 15 -> base attn
                if (tone_preview_.adsr_attack == 0) {
                    tone_preview_.attn_cur = tone_preview_.attn_base;
                    tone_preview_.adsr_phase = 2;
                    tone_preview_.adsr_counter = tone_preview_.adsr_decay;
                    dirty = true;
                } else if (tone_preview_.adsr_counter == 0) {
                    const int base_attn = tone_preview_.attn_base;
                    if (tone_preview_.attn_cur > base_attn) {
                        tone_preview_.attn_cur--;
                        dirty = true;
                    }
                    if (tone_preview_.attn_cur <= base_attn) {
                        tone_preview_.attn_cur = base_attn;
                        tone_preview_.adsr_phase = 2;
                        tone_preview_.adsr_counter = tone_preview_.adsr_decay;
                    } else {
                        tone_preview_.adsr_counter = tone_preview_.adsr_attack;
                    }
                } else {
                    tone_preview_.adsr_counter--;
                }
                break;
            case 2: { // DEC: base -> sustain
                const int base_attn = tone_preview_.attn_base;
                int sus_target = tone_preview_.adsr_sustain;
                if (sus_target < base_attn) sus_target = base_attn;
                if (tone_preview_.adsr_decay == 0 || sus_target <= base_attn) {
                    tone_preview_.attn_cur = sus_target;
                    tone_preview_.adsr_phase = 3;
                    tone_preview_.adsr_counter = tone_preview_.adsr_sustain_rate;
                    dirty = true;
                } else if (tone_preview_.adsr_counter == 0) {
                    if (tone_preview_.attn_cur < sus_target) {
                        tone_preview_.attn_cur++;
                        dirty = true;
                    }
                    if (tone_preview_.attn_cur >= sus_target) {
                        tone_preview_.attn_cur = sus_target;
                        tone_preview_.adsr_phase = 3;
                        tone_preview_.adsr_counter = tone_preview_.adsr_sustain_rate;
                    } else {
                        tone_preview_.adsr_counter = tone_preview_.adsr_decay;
                    }
                } else {
                    tone_preview_.adsr_counter--;
                }
                break;
            }
            case 3: // SUS: optional sustain fade to silence
                if (tone_preview_.adsr_sustain_rate > 0) {
                    if (tone_preview_.adsr_counter == 0) {
                        if (tone_preview_.attn_cur < 15) {
                            tone_preview_.attn_cur++;
                            dirty = true;
                        }
                        if (tone_preview_.attn_cur >= 15) {
                            tone_preview_.adsr_phase = 0;
                        } else {
                            tone_preview_.adsr_counter = tone_preview_.adsr_sustain_rate;
                        }
                    } else {
                        tone_preview_.adsr_counter--;
                    }
                }
                break;
            case 4: // REL: current -> 15
                if (tone_preview_.adsr_release == 0) {
                    tone_preview_.attn_cur = 15;
                    tone_preview_.adsr_phase = 0;
                    dirty = true;
                } else if (tone_preview_.adsr_counter == 0) {
                    if (tone_preview_.attn_cur < 15) {
                        tone_preview_.attn_cur++;
                        dirty = true;
                    }
                    if (tone_preview_.attn_cur >= 15) {
                        tone_preview_.adsr_phase = 0;
                    } else {
                        tone_preview_.adsr_counter = tone_preview_.adsr_release;
                    }
                } else {
                    tone_preview_.adsr_counter--;
                }
                break;
            default:
                break;
            }
        } else if (tone_preview_.env_on) {
            if (tone_preview_.env_counter == 0) {
                if (tone_preview_.attn_cur < 15) {
                    tone_preview_.attn_cur = std::min(15, tone_preview_.attn_cur + tone_preview_.env_step);
                    dirty = true;
                }
                tone_preview_.env_counter = tone_preview_.env_spd;
            } else {
                tone_preview_.env_counter--;
            }
        }

        if (tone_preview_.ch <= 2) {
            int prev_pitch = tone_preview_.lfo_pitch_delta;
            int prev_attn = tone_preview_.lfo_attn_delta;
            bool lfo_dirty = false;
            if (lfo_tick(
                    tone_preview_.lfo1_on,
                    tone_preview_.lfo1_wave,
                    tone_preview_.lfo1_rate,
                    tone_preview_.lfo1_depth,
                    tone_preview_.lfo1_hold_counter,
                    tone_preview_.lfo1_counter,
                    tone_preview_.lfo1_sign,
                    tone_preview_.lfo1_delta)) {
                lfo_dirty = true;
            }
            if (lfo_tick(
                    tone_preview_.lfo2_on,
                    tone_preview_.lfo2_wave,
                    tone_preview_.lfo2_rate,
                    tone_preview_.lfo2_depth,
                    tone_preview_.lfo2_hold_counter,
                    tone_preview_.lfo2_counter,
                    tone_preview_.lfo2_sign,
                    tone_preview_.lfo2_delta)) {
                lfo_dirty = true;
            }
            resolve_lfo_algo(
                tone_preview_.lfo_algo,
                tone_preview_.lfo1_delta,
                tone_preview_.lfo2_delta,
                tone_preview_.lfo_pitch_delta,
                tone_preview_.lfo_attn_delta);
            if (tone_preview_.lfo_pitch_delta != prev_pitch ||
                tone_preview_.lfo_attn_delta != prev_attn) {
                lfo_dirty = true;
            }
            if (lfo_dirty) {
                dirty = true;
            }
        }

        const int render_div = clamp_i(
            tone_preview_.div_cur + tone_preview_.lfo_pitch_delta, 1, 1023);
        const int render_attn = clamp_i(
            tone_preview_.attn_cur + tone_preview_.lfo_attn_delta, 0, 15);
        if (render_div != tone_preview_.rendered_div ||
            render_attn != tone_preview_.rendered_attn) {
            tone_preview_.rendered_div = render_div;
            tone_preview_.rendered_attn = render_attn;
            dirty = true;
        }

        if (dirty) {
            psg_helpers::DirectToneCh(
                hub_->engine(),
                tone_preview_.ch,
                static_cast<uint16_t>(tone_preview_.rendered_div),
                static_cast<uint8_t>(tone_preview_.rendered_attn));
        }

        tone_preview_.frames--;
        if (tone_preview_.frames <= 0) {
            if (tone_preview_.adsr_on &&
                tone_preview_.adsr_release > 0 &&
                tone_preview_.adsr_phase > 0 &&
                tone_preview_.adsr_phase != 4) {
                tone_preview_.adsr_phase = 4;
                tone_preview_.adsr_counter = tone_preview_.adsr_release;
                tone_preview_.frames = 1;
            } else {
                psg_helpers::DirectSilenceTone(hub_->engine(), tone_preview_.ch);
                tone_preview_.active = false;
            }
        }
    }

    // Noise update (driver-like: env/burst then timer--)
    if (noise_preview_.active && noise_preview_.frames > 0) {
        bool dirty = false;
        if (noise_preview_.env_on) {
            if (noise_preview_.env_counter == 0) {
                if (noise_preview_.attn_cur < 15) {
                    noise_preview_.attn_cur = std::min(15, noise_preview_.attn_cur + noise_preview_.env_step);
                    dirty = true;
                }
                noise_preview_.env_counter = noise_preview_.env_spd;
            } else {
                noise_preview_.env_counter--;
            }
        }

        if (noise_preview_.burst) {
            if (noise_preview_.burst_counter == 0) {
                noise_preview_.burst_off = !noise_preview_.burst_off;
                noise_preview_.burst_counter = noise_preview_.burst_off ? 1 : noise_preview_.burst_dur;
                dirty = true;
            } else {
                noise_preview_.burst_counter--;
            }
        }

        if (dirty) {
            if (noise_preview_.burst && noise_preview_.burst_off) {
                psg_helpers::DirectSilenceNoise(hub_->engine());
            } else {
                psg_helpers::DirectNoiseMode(
                    hub_->engine(),
                    static_cast<uint8_t>(noise_preview_.rate),
                    static_cast<uint8_t>(noise_preview_.type));
                psg_helpers::DirectNoiseAttn(
                    hub_->engine(),
                    static_cast<uint8_t>(noise_preview_.attn_cur));
            }
        }

        noise_preview_.frames--;
        if (noise_preview_.frames <= 0) {
            psg_helpers::DirectSilenceNoise(hub_->engine());
            noise_preview_.active = false;
        }
    }

    if (!tone_preview_.active && !noise_preview_.active) {
        stop_full_preview(false);
    }
}

void SfxLabTab::append_log(const QString& text) {
    log_->appendPlainText(text);
}

void SfxLabTab::update_output_meter() {
    if (!output_meter_ || !output_meter_label_) {
        return;
    }
    if (!hub_ || !hub_->audio_running()) {
        output_meter_->setValue(0);
        output_meter_label_->setText("Audio off");
        output_meter_label_->setStyleSheet(QString());
        return;
    }

    const int peak = hub_->audio_peak_percent();
    const bool clip = hub_->audio_clip_recent();
    output_meter_->setValue(peak);
    if (clip) {
        output_meter_label_->setText(QString("CLIP (%1%)").arg(peak));
        output_meter_label_->setStyleSheet("color:#c62828; font-weight:600;");
    } else {
        output_meter_label_->setText(QString("Peak %1%").arg(peak));
        output_meter_label_->setStyleSheet(QString());
    }
}
