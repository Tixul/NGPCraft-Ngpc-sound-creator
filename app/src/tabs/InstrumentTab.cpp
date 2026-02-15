#include "InstrumentTab.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "audio/EngineHub.h"
#include "audio/InstrumentPlayer.h"
#include "audio/TrackerPlaybackEngine.h"
#include "i18n/AppLanguage.h"
#include "models/InstrumentStore.h"
#include "widgets/EnvelopeCurveWidget.h"
#include "ngpc/instrument.h"

namespace {
// MIDI note frequencies for preview (C3 = 60 .. C6 = 96).
double midi_to_freq(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

uint16_t freq_to_divider(double freq) {
    if (freq <= 0.0) return 1;
    const double div = 3072000.0 / (32.0 * freq);
    int d = static_cast<int>(div + 0.5);
    if (d < 1) d = 1;
    if (d > 1023) d = 1023;
    return static_cast<uint16_t>(d);
}

int frames_to_ms(int frames) {
    if (frames <= 0) return 0;
    return (frames * 1000 + 59) / 60;
}

int estimate_preview_gate_ms(const ngpc::BgmInstrumentDef& d) {
    int gate_frames = (d.mode == 1) ? 12 : 22;

    if (d.adsr_on) {
        const int base_attn = std::clamp<int>(d.attn, 0, 15);
        const int sustain_attn = std::max(base_attn, std::clamp<int>(d.adsr_sustain, 0, 15));
        const int attack_steps = std::max(0, 15 - base_attn);
        const int decay_steps = std::max(0, sustain_attn - base_attn);
        const int atk_frames = (d.adsr_attack == 0) ? 0 : attack_steps * (static_cast<int>(d.adsr_attack) + 1);
        const int dec_frames = (d.adsr_decay == 0) ? 0 : decay_steps * (static_cast<int>(d.adsr_decay) + 1);
        gate_frames = std::max(gate_frames, atk_frames + dec_frames + 12);
    }

    if (d.mode == 0 && d.vib_on && d.vib_depth > 0) {
        const int vib_frames = static_cast<int>(d.vib_delay) + (static_cast<int>(d.vib_speed) + 1) * 2 + 8;
        gate_frames = std::max(gate_frames, vib_frames);
    }

    if (d.mode == 0 && d.lfo_on && d.lfo_depth > 0) {
        int lfo_frames = 0;
        if (d.lfo_wave == 0) {
            const int audible_steps = std::max(6, static_cast<int>(d.lfo_depth) / 2);
            lfo_frames = audible_steps * (static_cast<int>(d.lfo_rate) + 1) + 10;
        } else {
            lfo_frames = (static_cast<int>(d.lfo_rate) + 1) * 4 + 10;
        }
        gate_frames = std::max(gate_frames, lfo_frames);
    }

    gate_frames = std::clamp(gate_frames, (d.mode == 1) ? 10 : 18, 150);
    return frames_to_ms(gate_frames);
}

int estimate_preview_hard_stop_ms(const ngpc::BgmInstrumentDef& d, int gate_ms) {
    int tail_ms = 1200;
    if (d.adsr_on && d.adsr_release > 0) {
        const int rel_frames = 10 + 15 * (static_cast<int>(d.adsr_release) + 1);
        tail_ms = std::clamp(frames_to_ms(rel_frames), 400, 5000);
    }
    return gate_ms + tail_ms;
}

bool should_auto_gate_preview(const ngpc::BgmInstrumentDef& d, bool loop_preview) {
    if (loop_preview) return true;
    if (d.mode == 1) return true; // Noise presets are usually one-shot.
    if (d.adsr_on) return true;   // Needed to hear release behavior automatically.
    if (d.env_on) return true;
    if (d.sweep_on && d.sweep_step != 0) return true;
    if (d.vib_on && d.vib_depth > 0) return true;
    if (d.lfo_on && d.lfo_depth > 0) return true;
    if (d.pitch_curve_id != 0) return true;
    if (d.macro_id != 0) return true;
    return false;
}

QString tracker_code_hex(int index) {
    return QString("0x%1").arg(index & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
}

QString default_instrument_name(int index) {
    return QString("Instrument %1").arg(tracker_code_hex(index));
}

QString instrument_list_label(int index, const std::string& name) {
    const QString qname = QString::fromStdString(name).trimmed();
    const QString shown = qname.isEmpty() ? QString("Untitled") : qname;
    return QString("[%1] %2").arg(tracker_code_hex(index), shown);
}

QString ui_text(const char* fr, const char* en) {
    return app_lang_pick(load_app_language(), fr, en);
}
}  // namespace

InstrumentTab::InstrumentTab(EngineHub* hub, InstrumentStore* store, QWidget* parent)
    : QWidget(parent),
      hub_(hub),
      store_(store)
{
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };

    player_ = new InstrumentPlayer(hub_, this);

    auto* root = new QHBoxLayout(this);

    // ---- Left panel: preset list + buttons ----
    auto* left = new QVBoxLayout();
    tracker_code_label_ = new QLabel(ui("Code tracker : --", "Tracker code: --"), this);
    left->addWidget(tracker_code_label_);

    auto* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel(ui("Nom :", "Name:"), this));
    name_edit_ = new QLineEdit(this);
    name_row->addWidget(name_edit_, 1);
    auto* rename_btn = new QPushButton(ui("Appliquer Nom", "Apply Name"), this);
    name_row->addWidget(rename_btn);
    left->addLayout(name_row);
    name_edit_->setPlaceholderText(ui("Nom instrument (ex: Lead 1)", "Instrument name (e.g. Lead 1)"));
    name_edit_->setToolTip(ui(
        "Nom affiche dans la liste. Le code tracker reste l'index 0x00..0x7F.",
        "Display name in list. Tracker code remains the slot index 0x00..0x7F."));
    rename_btn->setToolTip(ui(
        "Applique le nom saisi au slot courant.",
        "Apply typed name to current slot."));

    list_ = new QListWidget(this);
    left->addWidget(list_, 1);

    auto* list_btns = new QHBoxLayout();
    auto* add_btn = new QPushButton(ui("Ajouter", "Add"), this);
    auto* remove_btn = new QPushButton(ui("Supprimer", "Delete"), this);
    auto* dup_btn = new QPushButton(ui("Dupliquer", "Duplicate"), this);
    auto* up_btn = new QPushButton(ui("Monter", "Up"), this);
    auto* down_btn = new QPushButton(ui("Descendre", "Down"), this);
    list_btns->addWidget(add_btn);
    list_btns->addWidget(remove_btn);
    list_btns->addWidget(dup_btn);
    list_btns->addWidget(up_btn);
    list_btns->addWidget(down_btn);
    left->addLayout(list_btns);

    auto* io_btns = new QHBoxLayout();
    auto* save_btn = new QPushButton(ui("Sauver JSON", "Save JSON"), this);
    auto* load_btn = new QPushButton(ui("Charger JSON", "Load JSON"), this);
    auto* export_btn = new QPushButton(ui("Exporter C", "Export C"), this);
    io_btns->addWidget(save_btn);
    io_btns->addWidget(load_btn);
    io_btns->addWidget(export_btn);
    left->addLayout(io_btns);

    auto* preset_ops = new QHBoxLayout();
    factory_combo_ = new QComboBox(this);
    const auto factory_presets = ngpc::FactoryInstrumentPresets();
    for (int i = 0; i < static_cast<int>(factory_presets.size()); ++i) {
        factory_combo_->addItem(
            instrument_list_label(i, factory_presets[static_cast<size_t>(i)].name), i);
    }
    auto* overwrite_btn = new QPushButton(ui("Appliquer Factory -> Slot", "Apply Factory -> Slot"), this);
    auto* reset_slot_btn = new QPushButton(ui("Reinit Slot", "Reset Slot"), this);
    auto* reset_all_btn = new QPushButton(ui("Reset Banque Factory", "Reset Factory Bank"), this);
    preset_ops->addWidget(factory_combo_, 1);
    preset_ops->addWidget(overwrite_btn);
    preset_ops->addWidget(reset_slot_btn);
    preset_ops->addWidget(reset_all_btn);
    left->addLayout(preset_ops);
    factory_combo_->setToolTip(ui(
        "Preset source factory. Utilise avec 'Appliquer Factory -> Slot'.",
        "Factory source preset. Use with 'Apply Factory -> Slot'."));
    overwrite_btn->setToolTip(ui(
        "Ecrase le slot courant avec le preset factory selectionne (meme code tracker).",
        "Overwrite current slot with selected factory preset (same tracker code)."));
    reset_slot_btn->setToolTip(ui(
        "Remet le slot courant a sa valeur par defaut (factory meme index si disponible).",
        "Reset current slot to default value (factory same index when available)."));
    reset_all_btn->setToolTip(ui(
        "Remet toute la banque d'instruments aux presets factory.",
        "Reset full instrument bank to factory presets."));
    tracker_code_label_->setToolTip(ui(
        "Code a utiliser dans la colonne In du tracker.",
        "Code to use in tracker In column."));

    // ---- Right panel: parameter groups ----
    auto* right = new QVBoxLayout();

    // General
    auto* gen_box = new QGroupBox("General", this);
    auto* gen_layout = new QHBoxLayout(gen_box);
    gen_layout->addWidget(new QLabel("Attn (0-15):", this));
    attn_spin_ = new QSpinBox(this);
    attn_spin_->setRange(0, 15);
    gen_layout->addWidget(attn_spin_);
    gen_layout->addWidget(new QLabel("Mode:", this));
    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Tone");
    mode_combo_->addItem("Noise");
    gen_layout->addWidget(mode_combo_);
    noise_label_ = new QLabel("Noise:", this);
    gen_layout->addWidget(noise_label_);
    noise_combo_ = new QComboBox(this);
    noise_combo_->addItem("P.H – Periodique, rapide");
    noise_combo_->addItem("P.M – Periodique, moyen");
    noise_combo_->addItem("P.L – Periodique, lent");
    noise_combo_->addItem("P.T – Periodique, freq. Tone 2");
    noise_combo_->addItem("W.H – Blanc, rapide");
    noise_combo_->addItem("W.M – Blanc, moyen");
    noise_combo_->addItem("W.L – Blanc, lent");
    noise_combo_->addItem("W.T – Blanc, freq. Tone 2");
    gen_layout->addWidget(noise_combo_);
    gen_layout->addStretch();

    // Envelope
    auto* env_box = new QGroupBox("Envelope", this);
    auto* env_layout = new QVBoxLayout(env_box);
    auto* env_top = new QHBoxLayout();
    env_on_ = new QCheckBox("Enable", this);
    env_top->addWidget(env_on_);
    env_top->addWidget(new QLabel("Step:", this));
    env_step_ = new QSpinBox(this);
    env_step_->setRange(0, 15);
    env_top->addWidget(env_step_);
    env_top->addWidget(new QLabel("Speed:", this));
    env_speed_ = new QSpinBox(this);
    env_speed_->setRange(1, 255);
    env_top->addWidget(env_speed_);
    env_top->addWidget(new QLabel("Curve:", this));
    env_curve_combo_ = new QComboBox(this);
    env_top->addWidget(env_curve_combo_);
    env_top->addStretch();
    env_layout->addLayout(env_top);
    env_widget_ = new EnvelopeCurveWidget(this);
    env_layout->addWidget(env_widget_);

    // Pitch Curve
    pitch_box_ = new QGroupBox("Pitch Curve", this);
    auto* pitch_layout = new QHBoxLayout(pitch_box_);
    pitch_layout->addWidget(new QLabel("Curve:", this));
    pitch_curve_combo_ = new QComboBox(this);
    pitch_layout->addWidget(pitch_curve_combo_);
    pitch_layout->addStretch();

    // Vibrato
    vib_box_ = new QGroupBox("Vibrato", this);
    auto* vib_layout = new QHBoxLayout(vib_box_);
    vib_on_ = new QCheckBox("Enable", this);
    vib_layout->addWidget(vib_on_);
    vib_layout->addWidget(new QLabel("Depth:", this));
    vib_depth_ = new QSpinBox(this);
    vib_depth_->setRange(0, 15);
    vib_layout->addWidget(vib_depth_);
    vib_layout->addWidget(new QLabel("Speed:", this));
    vib_speed_ = new QSpinBox(this);
    vib_speed_->setRange(1, 255);
    vib_layout->addWidget(vib_speed_);
    vib_layout->addWidget(new QLabel("Delay:", this));
    vib_delay_ = new QSpinBox(this);
    vib_delay_->setRange(0, 255);
    vib_layout->addWidget(vib_delay_);
    vib_layout->addStretch();

    // Sweep
    sweep_box_ = new QGroupBox("Sweep", this);
    auto* sweep_layout = new QHBoxLayout(sweep_box_);
    sweep_on_ = new QCheckBox("Enable", this);
    sweep_layout->addWidget(sweep_on_);
    sweep_layout->addWidget(new QLabel("End:", this));
    sweep_end_ = new QSpinBox(this);
    sweep_end_->setRange(1, 1023);
    sweep_layout->addWidget(sweep_end_);
    sweep_layout->addWidget(new QLabel("Step:", this));
    sweep_step_ = new QSpinBox(this);
    sweep_step_->setRange(-32768, 32767);
    sweep_layout->addWidget(sweep_step_);
    sweep_layout->addWidget(new QLabel("Speed:", this));
    sweep_speed_ = new QSpinBox(this);
    sweep_speed_->setRange(1, 255);
    sweep_layout->addWidget(sweep_speed_);
    sweep_layout->addStretch();

    // ADSR
    adsr_box_ = new QGroupBox("ADSR Envelope", this);
    auto* adsr_layout = new QHBoxLayout(adsr_box_);
    adsr_on_ = new QCheckBox("Enable", this);
    adsr_layout->addWidget(adsr_on_);
    adsr_layout->addWidget(new QLabel("A:", this));
    adsr_attack_ = new QSpinBox(this);
    adsr_attack_->setRange(0, 255);
    adsr_attack_->setToolTip(ui(
        "Vitesse d'attaque (frames par pas, 0=instantane)",
        "Attack speed (frames per step, 0=instant)"));
    adsr_layout->addWidget(adsr_attack_);
    adsr_layout->addWidget(new QLabel("D:", this));
    adsr_decay_ = new QSpinBox(this);
    adsr_decay_->setRange(0, 255);
    adsr_decay_->setToolTip(ui(
        "Vitesse de decay (frames par pas, 0=instantane)",
        "Decay speed (frames per step, 0=instant)"));
    adsr_layout->addWidget(adsr_decay_);
    adsr_layout->addWidget(new QLabel("S:", this));
    adsr_sustain_ = new QSpinBox(this);
    adsr_sustain_->setRange(0, 15);
    adsr_sustain_->setToolTip(ui(
        "Niveau de sustain (0=fort, 15=silence)",
        "Sustain level (0=loud, 15=silent)"));
    adsr_layout->addWidget(adsr_sustain_);
    adsr_layout->addWidget(new QLabel("SR:", this));
    adsr_sustain_rate_ = new QSpinBox(this);
    adsr_sustain_rate_->setRange(0, 255);
    adsr_sustain_rate_->setToolTip(ui(
        "Vitesse de sustain (frames par pas vers silence, 0=palier)",
        "Sustain rate (frames per step toward silence, 0=hold)"));
    adsr_layout->addWidget(adsr_sustain_rate_);
    adsr_layout->addWidget(new QLabel("R:", this));
    adsr_release_ = new QSpinBox(this);
    adsr_release_->setRange(0, 255);
    adsr_release_->setToolTip(ui(
        "Vitesse de release (frames par pas, 0=instantane)",
        "Release speed (frames per step, 0=instant)"));
    adsr_layout->addWidget(adsr_release_);
    adsr_layout->addStretch();

    // LFO
    lfo_box_ = new QGroupBox("LFO (Pitch)", this);
    auto* lfo_layout = new QHBoxLayout(lfo_box_);
    lfo_on_ = new QCheckBox("Enable", this);
    lfo_layout->addWidget(lfo_on_);
    lfo_layout->addWidget(new QLabel("Wave:", this));
    lfo_wave_ = new QComboBox(this);
    lfo_wave_->addItem("Triangle");
    lfo_wave_->addItem("Square");
    lfo_wave_->addItem("Saw");
    lfo_wave_->addItem("Sweep Up");
    lfo_wave_->addItem("Sweep Down");
    lfo_layout->addWidget(lfo_wave_);
    lfo_layout->addWidget(new QLabel("Hold:", this));
    lfo_hold_ = new QSpinBox(this);
    lfo_hold_->setRange(0, 255);
    lfo_layout->addWidget(lfo_hold_);
    lfo_layout->addWidget(new QLabel("Rate:", this));
    lfo_rate_ = new QSpinBox(this);
    lfo_rate_->setRange(0, 255);
    lfo_rate_->setToolTip(ui(
        "Frames par demi-cycle (petit = plus rapide, grand = plus lent)",
        "Frames per half-cycle (lower = faster, higher = slower)"));
    lfo_layout->addWidget(lfo_rate_);
    lfo_layout->addWidget(new QLabel("Depth:", this));
    lfo_depth_ = new QSpinBox(this);
    lfo_depth_->setRange(0, 255);
    lfo_depth_->setToolTip(ui(
        "Amplitude du pitch (delta de divider). Pour un effet evident: ~6 a 20.",
        "Pitch amount (divider delta). For an obvious effect: ~6 to 20."));
    lfo_layout->addWidget(lfo_depth_);
    lfo_layout->addStretch();

    lfo2_box_ = new QGroupBox("LFO2 + Algorithm", this);
    auto* lfo2_layout = new QHBoxLayout(lfo2_box_);
    lfo2_layout->addWidget(new QLabel("Algo:", this));
    lfo_algo_ = new QSpinBox(this);
    lfo_algo_->setRange(0, 7);
    lfo2_layout->addWidget(lfo_algo_);
    lfo2_on_ = new QCheckBox("LFO2", this);
    lfo2_layout->addWidget(lfo2_on_);
    lfo2_layout->addWidget(new QLabel("Wave:", this));
    lfo2_wave_ = new QComboBox(this);
    lfo2_wave_->addItem("Triangle");
    lfo2_wave_->addItem("Square");
    lfo2_wave_->addItem("Saw");
    lfo2_wave_->addItem("Sweep Up");
    lfo2_wave_->addItem("Sweep Down");
    lfo2_layout->addWidget(lfo2_wave_);
    lfo2_layout->addWidget(new QLabel("Hold:", this));
    lfo2_hold_ = new QSpinBox(this);
    lfo2_hold_->setRange(0, 255);
    lfo2_layout->addWidget(lfo2_hold_);
    lfo2_layout->addWidget(new QLabel("Rate:", this));
    lfo2_rate_ = new QSpinBox(this);
    lfo2_rate_->setRange(0, 255);
    lfo2_layout->addWidget(lfo2_rate_);
    lfo2_layout->addWidget(new QLabel("Depth:", this));
    lfo2_depth_ = new QSpinBox(this);
    lfo2_depth_->setRange(0, 255);
    lfo2_layout->addWidget(lfo2_depth_);
    lfo2_layout->addStretch();

    // Macro
    auto* macro_box = new QGroupBox("Macro", this);
    auto* macro_layout = new QHBoxLayout(macro_box);
    macro_layout->addWidget(new QLabel("Macro ID (0-15):", this));
    macro_id_ = new QSpinBox(this);
    macro_id_->setRange(0, 15);
    macro_layout->addWidget(macro_id_);
    macro_layout->addStretch();

    // Preview
    auto* preview_box = new QGroupBox("Preview", this);
    auto* preview_layout = new QHBoxLayout(preview_box);
    preview_layout->addWidget(new QLabel("Note:", this));
    preview_note_ = new QComboBox(this);
    const char* kNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int oct = 3; oct <= 6; ++oct) {
        for (int n = 0; n < 12; ++n) {
            const int midi = 48 + (oct - 3) * 12 + n;
            preview_note_->addItem(QString("%1%2").arg(kNoteNames[n]).arg(oct), midi);
        }
    }
    preview_note_->setCurrentIndex(12); // C4
    preview_layout->addWidget(preview_note_);
    auto* play_btn = new QPushButton("Play", this);
    auto* stop_btn = new QPushButton("Stop", this);
    auto* loop_btn = new QPushButton("Loop", this);
    loop_btn->setCheckable(true);
    loop_btn->setToolTip(ui(
        "Apercu en boucle: rejoue automatiquement le son avec les parametres mis a jour",
        "Loop preview: automatically replays the sound with updated parameters"));
    preview_layout->addWidget(play_btn);
    preview_layout->addWidget(stop_btn);
    preview_layout->addWidget(loop_btn);
    preview_layout->addStretch();

    // Log
    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumHeight(80);
    log_->setPlaceholderText("Instrument log...");

    right->addWidget(gen_box);
    right->addWidget(env_box);
    right->addWidget(adsr_box_);
    right->addWidget(lfo_box_);
    right->addWidget(lfo2_box_);
    right->addWidget(pitch_box_);
    right->addWidget(vib_box_);
    right->addWidget(sweep_box_);
    right->addWidget(macro_box);
    right->addWidget(preview_box);
    right->addWidget(log_);
    right->addStretch(1);

    root->addLayout(left, 1);
    root->addLayout(right, 3);

    // ---- Populate curve combos ----
    const auto env_curves = ngpc::FactoryEnvCurves();
    for (const auto& c : env_curves) {
        env_curve_combo_->addItem(QString::fromStdString(c.name));
    }
    const auto pitch_curves = ngpc::FactoryPitchCurves();
    for (const auto& c : pitch_curves) {
        pitch_curve_combo_->addItem(QString::fromStdString(c.name));
    }

    // ---- Connections ----

    connect(list_, &QListWidget::currentRowChanged, this, [this, ui](int row) {
        if (row >= 0 && row < store_->count()) {
            load_preset_to_ui(row);
            if (factory_combo_ && factory_combo_->count() > 0) {
                factory_combo_->setCurrentIndex(std::clamp(row, 0, factory_combo_->count() - 1));
            }
        } else {
            tracker_code_label_->setText(ui("Code tracker : --", "Tracker code: --"));
            if (name_edit_) name_edit_->clear();
        }
    });

    connect(store_, &InstrumentStore::list_changed, this, &InstrumentTab::refresh_list);
    connect(store_, &InstrumentStore::preset_changed, this, [this, ui](int index) {
        update_list_item(index);
        if (index == list_->currentRow()) {
            tracker_code_label_->setText(QString("%1 %2")
                .arg(ui("Code tracker :", "Tracker code:"), tracker_code_hex(index)));
        }
    });

    connect(add_btn, &QPushButton::clicked, this, [this, ui]() {
        if (store_->count() >= InstrumentStore::kMaxPresets) {
            append_log(ui("Maximum d'instruments atteint (128)", "Max instruments reached (128)"));
            return;
        }
        const int new_index = store_->count();
        const QString suggested_name = default_instrument_name(new_index);
        bool ok = false;
        QString name = QInputDialog::getText(
            this,
            ui("Nouvel instrument", "New Instrument"),
            ui("Nom de l'instrument :", "Instrument name:"),
            QLineEdit::Normal,
            suggested_name,
            &ok).trimmed();
        if (!ok) {
            return;
        }
        if (name.isEmpty()) {
            name = suggested_name;
        }
        ngpc::InstrumentPreset p;
        p.name = name.toStdString();
        store_->add(p);
        list_->setCurrentRow(store_->count() - 1);
    });

    connect(remove_btn, &QPushButton::clicked, this, [this, ui]() {
        const int row = list_->currentRow();
        if (row >= 0) {
            const auto& p = store_->at(row);
            const QString msg = QString("%1 [%2] %3 ?")
                .arg(ui("Supprimer l'instrument", "Delete instrument"))
                .arg(tracker_code_hex(row), QString::fromStdString(p.name));
            if (QMessageBox::question(this, ui("Supprimer instrument", "Delete Instrument"), msg) != QMessageBox::Yes) {
                return;
            }
            store_->remove(row);
        }
    });

    connect(dup_btn, &QPushButton::clicked, this, [this, ui]() {
        const int row = list_->currentRow();
        if (row >= 0) {
            if (store_->count() >= InstrumentStore::kMaxPresets) {
                append_log(ui("Maximum d'instruments atteint (128)", "Max instruments reached (128)"));
                return;
            }
            store_->duplicate(row);
            list_->setCurrentRow(row + 1);
        }
    });

    connect(up_btn, &QPushButton::clicked, this, [this]() {
        const int row = list_->currentRow();
        if (row > 0) {
            store_->move_up(row);
            list_->setCurrentRow(row - 1);
        }
    });

    connect(down_btn, &QPushButton::clicked, this, [this]() {
        const int row = list_->currentRow();
        if (row >= 0 && row < store_->count() - 1) {
            store_->move_down(row);
            list_->setCurrentRow(row + 1);
        }
    });

    connect(save_btn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, "Save Instruments", QString(), "JSON (*.json)");
        if (path.isEmpty()) return;
        if (store_->save_json(path)) {
            append_log(QString("Saved: %1").arg(path));
        } else {
            append_log("Save failed");
        }
    });

    connect(load_btn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Load Instruments", QString(), "JSON (*.json)");
        if (path.isEmpty()) return;
        if (store_->load_json(path)) {
            append_log(QString("Loaded: %1").arg(path));
        } else {
            append_log("Load failed");
        }
    });

    connect(export_btn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, "Export C Array", QString(), "C source (*.c *.h)");
        if (path.isEmpty()) return;
        const QString code = store_->export_c_array();
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(code.toUtf8());
            append_log(QString("Exported: %1").arg(path));
        } else {
            append_log("Export failed");
        }
    });

    connect(rename_btn, &QPushButton::clicked, this, &InstrumentTab::on_name_changed);
    connect(name_edit_, &QLineEdit::editingFinished, this, &InstrumentTab::on_name_changed);

    connect(overwrite_btn, &QPushButton::clicked, this, [this, ui]() {
        const int row = list_->currentRow();
        if (row < 0 || row >= store_->count()) {
            append_log(ui("Aucun instrument selectionne", "No instrument selected"));
            return;
        }
        const int src = factory_combo_ ? factory_combo_->currentIndex() : -1;
        const auto factory = ngpc::FactoryInstrumentPresets();
        if (src < 0 || src >= static_cast<int>(factory.size())) {
            append_log(ui("Aucun preset factory selectionne", "No factory preset selected"));
            return;
        }
        const QString dst_name = QString::fromStdString(store_->at(row).name);
        const QString src_name = QString::fromStdString(factory[static_cast<size_t>(src)].name);
        const QString msg = QString("%1 [%2] %3 %4 [%5] %6 ?")
            .arg(ui("Ecraser", "Overwrite"))
            .arg(tracker_code_hex(row))
            .arg(dst_name)
            .arg(ui("avec", "with"))
            .arg(tracker_code_hex(src))
            .arg(src_name);
        if (QMessageBox::question(this, ui("Ecraser instrument", "Overwrite Instrument"), msg) != QMessageBox::Yes) {
            return;
        }
        store_->set(row, factory[static_cast<size_t>(src)]);
        load_preset_to_ui(row);
        append_log(QString("%1 %2 %3 %4")
            .arg(ui("Ecrase", "Overwrote"))
            .arg(tracker_code_hex(row))
            .arg(ui("depuis factory", "from factory"))
            .arg(tracker_code_hex(src)));
    });

    connect(reset_slot_btn, &QPushButton::clicked, this, [this, ui]() {
        const int row = list_->currentRow();
        if (row < 0 || row >= store_->count()) {
            append_log(ui("Aucun instrument selectionne", "No instrument selected"));
            return;
        }
        const QString msg = QString("%1 [%2] %3")
            .arg(ui("Remettre", "Reset"), tracker_code_hex(row), ui("aux valeurs par defaut ?", "to default value?"));
        if (QMessageBox::question(this, ui("Reset instrument", "Reset Instrument"), msg) != QMessageBox::Yes) {
            return;
        }
        const auto factory = ngpc::FactoryInstrumentPresets();
        ngpc::InstrumentPreset p;
        if (row < static_cast<int>(factory.size())) {
            p = factory[static_cast<size_t>(row)];
        } else {
            p.name = default_instrument_name(row).toStdString();
            p.def = ngpc::BgmInstrumentDef{};
        }
        store_->set(row, p);
        load_preset_to_ui(row);
        append_log(QString("%1 %2 %3")
            .arg(ui("Reset", "Reset"), tracker_code_hex(row), ui("effectue", "to default")));
    });

    connect(reset_all_btn, &QPushButton::clicked, this, [this, ui]() {
        const int prev = list_->currentRow();
        const QString msg = ui(
            "Remettre toute la banque d'instruments aux valeurs factory ?\nCela ecrasera tous les instruments courants.",
            "Reset full instrument bank to factory defaults?\nThis will overwrite all current instruments.");
        if (QMessageBox::question(this, ui("Reset banque factory", "Reset Factory Bank"), msg) != QMessageBox::Yes) {
            return;
        }
        store_->load_factory_presets();
        if (store_->count() > 0) {
            list_->setCurrentRow(std::clamp(prev, 0, store_->count() - 1));
        }
        append_log(ui("Banque factory restauree", "Factory bank restored"));
    });

    // Parameter change connections (all funnel into on_parameter_changed)
    auto changed = [this]() { on_parameter_changed(); };
    connect(attn_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        on_parameter_changed();
        update_noise_visibility();
    });
    connect(noise_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(env_on_, &QCheckBox::toggled, this, changed);
    connect(env_step_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(env_speed_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(env_curve_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        on_parameter_changed();
        // Update the envelope widget with the selected curve
        const int idx = env_curve_combo_->currentIndex();
        const auto curves = ngpc::FactoryEnvCurves();
        if (idx >= 0 && idx < static_cast<int>(curves.size())) {
            env_widget_->set_curve(curves[static_cast<size_t>(idx)].steps);
        }
    });
    connect(pitch_curve_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(vib_on_, &QCheckBox::toggled, this, changed);
    connect(vib_depth_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(vib_speed_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(vib_delay_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(sweep_on_, &QCheckBox::toggled, this, changed);
    connect(sweep_end_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(sweep_step_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(sweep_speed_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(adsr_on_, &QCheckBox::toggled, this, changed);
    connect(adsr_attack_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(adsr_decay_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(adsr_sustain_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(adsr_sustain_rate_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(adsr_release_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo_on_, &QCheckBox::toggled, this, changed);
    connect(lfo_wave_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(lfo_hold_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo_rate_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo_depth_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo_algo_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo2_on_, &QCheckBox::toggled, this, changed);
    connect(lfo2_wave_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(lfo2_hold_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo2_rate_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(lfo2_depth_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    connect(macro_id_, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);

    connect(play_btn, &QPushButton::clicked, this, &InstrumentTab::on_preview_play);
    connect(stop_btn, &QPushButton::clicked, this, [this, loop_btn]() {
        loop_preview_ = false;
        loop_btn->setChecked(false);
        on_preview_stop();
    });
    connect(loop_btn, &QPushButton::toggled, this, [this](bool checked) {
        loop_preview_ = checked;
        if (checked && !player_->is_playing()) {
            on_preview_play();
        }
    });
    connect(player_, &InstrumentPlayer::stopped, this, [this]() {
        if (loop_preview_) {
            QTimer::singleShot(80, this, [this]() {
                if (loop_preview_) on_preview_play();
            });
        }
    });

    // Initial population
    refresh_list();
    if (store_->count() > 0) {
        list_->setCurrentRow(0);
    }
    update_noise_visibility();
}

void InstrumentTab::refresh_list() {
    const int prev = list_->currentRow();
    list_->clear();
    for (int i = 0; i < store_->count(); ++i) {
        list_->addItem(instrument_list_label(i, store_->at(i).name));
    }
    if (prev >= 0 && prev < store_->count()) {
        list_->setCurrentRow(prev);
    } else if (store_->count() > 0) {
        list_->setCurrentRow(0);
    }
}

void InstrumentTab::update_list_item(int index) {
    if (index < 0 || index >= store_->count()) {
        return;
    }
    QListWidgetItem* item = list_->item(index);
    if (!item) {
        return;
    }
    item->setText(instrument_list_label(index, store_->at(index).name));
}

void InstrumentTab::load_preset_to_ui(int index) {
    if (index < 0 || index >= store_->count()) {
        return;
    }
    updating_ui_ = true;
    const auto& d = store_->at(index).def;
    const QString name = QString::fromStdString(store_->at(index).name);

    tracker_code_label_->setText(QString("%1 %2")
        .arg(ui_text("Code tracker :", "Tracker code:"), tracker_code_hex(index)));
    name_edit_->setText(name);

    attn_spin_->setValue(d.attn);
    mode_combo_->setCurrentIndex(d.mode);
    noise_combo_->setCurrentIndex(d.noise_config & 0x07);

    env_on_->setChecked(d.env_on != 0);
    env_step_->setValue(d.env_step);
    env_speed_->setValue(d.env_speed);
    if (d.env_curve_id < env_curve_combo_->count()) {
        env_curve_combo_->setCurrentIndex(d.env_curve_id);
    }

    if (d.pitch_curve_id < pitch_curve_combo_->count()) {
        pitch_curve_combo_->setCurrentIndex(d.pitch_curve_id);
    }

    vib_on_->setChecked(d.vib_on != 0);
    vib_depth_->setValue(d.vib_depth);
    vib_speed_->setValue(d.vib_speed);
    vib_delay_->setValue(d.vib_delay);

    sweep_on_->setChecked(d.sweep_on != 0);
    sweep_end_->setValue(d.sweep_end);
    sweep_step_->setValue(d.sweep_step);
    sweep_speed_->setValue(d.sweep_speed);

    adsr_on_->setChecked(d.adsr_on != 0);
    adsr_attack_->setValue(d.adsr_attack);
    adsr_decay_->setValue(d.adsr_decay);
    adsr_sustain_->setValue(d.adsr_sustain);
    adsr_sustain_rate_->setValue(d.adsr_sustain_rate);
    adsr_release_->setValue(d.adsr_release);
    lfo_on_->setChecked(d.lfo_on != 0);
    lfo_wave_->setCurrentIndex(std::min<int>(d.lfo_wave, lfo_wave_->count() - 1));
    lfo_hold_->setValue(d.lfo_hold);
    lfo_rate_->setValue(d.lfo_rate);
    lfo_depth_->setValue(d.lfo_depth);
    lfo_algo_->setValue(d.lfo_algo > 7 ? 7 : d.lfo_algo);
    lfo2_on_->setChecked(d.lfo2_on != 0);
    lfo2_wave_->setCurrentIndex(std::min<int>(d.lfo2_wave, lfo2_wave_->count() - 1));
    lfo2_hold_->setValue(d.lfo2_hold);
    lfo2_rate_->setValue(d.lfo2_rate);
    lfo2_depth_->setValue(d.lfo2_depth);

    macro_id_->setValue(d.macro_id);

    // Update envelope widget
    const auto curves = ngpc::FactoryEnvCurves();
    if (d.env_curve_id < static_cast<uint8_t>(curves.size())) {
        env_widget_->set_curve(curves[d.env_curve_id].steps);
    }
    env_widget_->set_base_attn(d.attn);

    updating_ui_ = false;
    update_noise_visibility();
}

void InstrumentTab::on_name_changed() {
    if (updating_ui_) {
        return;
    }
    const int row = list_->currentRow();
    if (row < 0 || row >= store_->count()) {
        return;
    }
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        name = default_instrument_name(row);
        updating_ui_ = true;
        name_edit_->setText(name);
        updating_ui_ = false;
    }
    ngpc::InstrumentPreset p = store_->at(row);
    if (QString::fromStdString(p.name) == name) {
        return;
    }
    p.name = name.toStdString();
    store_->set(row, p);
    append_log(QString("%1 %2 \"%3\"")
        .arg(ui_text("Renomme", "Renamed"), tracker_code_hex(row), name));
}

void InstrumentTab::on_parameter_changed() {
    if (updating_ui_) {
        return;
    }
    const int row = list_->currentRow();
    if (row < 0 || row >= store_->count()) {
        return;
    }

    ngpc::InstrumentPreset p = store_->at(row);
    auto& d = p.def;

    d.attn           = static_cast<uint8_t>(attn_spin_->value());
    d.mode           = static_cast<uint8_t>(mode_combo_->currentIndex());
    d.noise_config   = static_cast<uint8_t>(noise_combo_->currentIndex());
    d.env_on         = env_on_->isChecked() ? 1 : 0;
    d.env_step       = static_cast<uint8_t>(env_step_->value());
    d.env_speed      = static_cast<uint8_t>(env_speed_->value());
    d.env_curve_id   = static_cast<uint8_t>(env_curve_combo_->currentIndex());
    d.pitch_curve_id = static_cast<uint8_t>(pitch_curve_combo_->currentIndex());
    d.vib_on         = vib_on_->isChecked() ? 1 : 0;
    d.vib_depth      = static_cast<uint8_t>(vib_depth_->value());
    d.vib_speed      = static_cast<uint8_t>(vib_speed_->value());
    d.vib_delay      = static_cast<uint8_t>(vib_delay_->value());
    d.sweep_on       = sweep_on_->isChecked() ? 1 : 0;
    d.sweep_end      = static_cast<uint16_t>(sweep_end_->value());
    d.sweep_step     = static_cast<int16_t>(sweep_step_->value());
    d.sweep_speed    = static_cast<uint8_t>(sweep_speed_->value());
    d.adsr_on        = adsr_on_->isChecked() ? 1 : 0;
    d.adsr_attack    = static_cast<uint8_t>(adsr_attack_->value());
    d.adsr_decay     = static_cast<uint8_t>(adsr_decay_->value());
    d.adsr_sustain   = static_cast<uint8_t>(adsr_sustain_->value());
    d.adsr_sustain_rate = static_cast<uint8_t>(adsr_sustain_rate_->value());
    d.adsr_release   = static_cast<uint8_t>(adsr_release_->value());
    d.lfo_on         = lfo_on_->isChecked() ? 1 : 0;
    d.lfo_wave       = static_cast<uint8_t>(lfo_wave_->currentIndex());
    d.lfo_hold       = static_cast<uint8_t>(lfo_hold_->value());
    d.lfo_rate       = static_cast<uint8_t>(lfo_rate_->value());
    d.lfo_depth      = static_cast<uint8_t>(lfo_depth_->value());
    d.lfo_algo       = static_cast<uint8_t>(lfo_algo_->value());
    d.lfo2_on        = lfo2_on_->isChecked() ? 1 : 0;
    d.lfo2_wave      = static_cast<uint8_t>(lfo2_wave_->currentIndex());
    d.lfo2_hold      = static_cast<uint8_t>(lfo2_hold_->value());
    d.lfo2_rate      = static_cast<uint8_t>(lfo2_rate_->value());
    d.lfo2_depth     = static_cast<uint8_t>(lfo2_depth_->value());
    d.macro_id       = static_cast<uint8_t>(macro_id_->value());

    env_widget_->set_base_attn(d.attn);

    store_->set(row, p);
}

void InstrumentTab::on_preview_play() {
    if (!hub_) {
        append_log("Engine hub missing");
        return;
    }
    const int row = list_->currentRow();
    if (row < 0 || row >= store_->count()) {
        append_log("No instrument selected");
        return;
    }

    const auto& preset = store_->at(row);
    const auto& d = preset.def;
    const int midi_note = preview_note_->currentData().toInt();
    const double freq = midi_to_freq(midi_note);
    const uint16_t divider = freq_to_divider(freq);

    player_->play(d, divider);
    const int token = ++preview_note_token_;
    if (should_auto_gate_preview(d, loop_preview_)) {
        const int gate_ms = estimate_preview_gate_ms(d);
        const int hard_stop_ms = estimate_preview_hard_stop_ms(d, gate_ms);
        QTimer::singleShot(gate_ms, this, [this, token]() {
            if (!player_) return;
            if (token != preview_note_token_) return;
            if (player_->is_playing()) player_->note_off();
        });
        QTimer::singleShot(hard_stop_ms, this, [this, token]() {
            if (!player_) return;
            if (token != preview_note_token_) return;
            if (player_->is_playing()) player_->stop();
        });
    }

    QString info = QString::fromStdString(preset.name);
    if (d.mode == 1) {
        info += QString(" | Noise %1 attn=%2")
                    .arg(TrackerPlaybackEngine::noise_display_name(d.noise_config))
                    .arg(d.attn);
    } else {
        info += QString(" | Tone div=%1 attn=%2")
                    .arg(divider).arg(d.attn);
    }
    if (d.adsr_on) info += QString(" +ADSR(%1/%2/%3/%4/%5)")
        .arg(d.adsr_attack).arg(d.adsr_decay).arg(d.adsr_sustain).arg(d.adsr_sustain_rate).arg(d.adsr_release);
    else if (d.env_on) info += " +env";
    if (d.lfo_on && d.lfo_depth > 0) info += QString(" +lfo1(w%1,h%2,r%3,d%4)")
        .arg(d.lfo_wave).arg(d.lfo_hold).arg(d.lfo_rate).arg(d.lfo_depth);
    if (d.lfo2_on && d.lfo2_depth > 0) info += QString(" +lfo2(w%1,h%2,r%3,d%4)")
        .arg(d.lfo2_wave).arg(d.lfo2_hold).arg(d.lfo2_rate).arg(d.lfo2_depth);
    if ((d.lfo_on && d.lfo_depth > 0) || (d.lfo2_on && d.lfo2_depth > 0)) {
        info += QString(" algo=%1").arg(d.lfo_algo);
    }
    if (d.vib_on) info += " +vib";
    if (d.sweep_on) info += " +sweep";
    if (d.pitch_curve_id > 0) info += " +pitch";
    append_log(info);
}

void InstrumentTab::on_preview_stop() {
    const int token = ++preview_note_token_;
    if (!player_ || !player_->is_playing()) {
        append_log("Preview stopped");
        return;
    }
    player_->note_off();
    QTimer::singleShot(1500, this, [this, token]() {
        if (!player_) return;
        if (token != preview_note_token_) return;
        if (player_->is_playing()) player_->stop();
    });
    append_log("Preview stopped");
}

void InstrumentTab::update_noise_visibility() {
    const bool is_noise = (mode_combo_->currentIndex() == 1);
    noise_label_->setVisible(is_noise);
    noise_combo_->setVisible(is_noise);
    pitch_box_->setVisible(!is_noise);
    lfo_box_->setVisible(!is_noise);
    lfo2_box_->setVisible(!is_noise);
    vib_box_->setVisible(!is_noise);
    sweep_box_->setVisible(!is_noise);
    preview_note_->setVisible(!is_noise);
}

void InstrumentTab::append_log(const QString& text) {
    log_->appendPlainText(text);
}
