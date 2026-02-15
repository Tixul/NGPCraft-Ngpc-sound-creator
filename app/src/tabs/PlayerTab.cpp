#include "PlayerTab.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "ngpc/midi.h"
#include "ngpc/instrument.h"
#include "audio/EngineHub.h"
#include "audio/PsgHelpers.h"
#include "i18n/AppLanguage.h"
#include "models/InstrumentStore.h"

PlayerTab::PlayerTab(EngineHub* hub, InstrumentStore* store, QWidget* parent)
    : QWidget(parent),
      hub_(hub),
      instrument_store_(store)
{
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };

    bgm_timer_ = new QTimer(this);
    bgm_timer_->setInterval(1000 / 60);
    connect(bgm_timer_, &QTimer::timeout, this, &PlayerTab::tick_bgm);

    auto* root = new QVBoxLayout(this);

    auto* driver_box = new QGroupBox(ui("Driver Z80 (optionnel)", "Z80 Driver (optional)"), this);
    auto* driver_layout = new QHBoxLayout(driver_box);
    driver_path_ = new QLineEdit(this);
    driver_path_->setPlaceholderText(ui(
        "Chemin vers un driver Z80 externe (.drv/.bin) - optionnel",
        "Path to an external Z80 driver (.drv/.bin) - optional"));
    driver_path_->setToolTip(ui(
        "Option avancee: utile pour tester un driver externe specifique. "
        "Le preview normal n'en a pas besoin.",
        "Advanced option: useful to test a specific external driver. "
        "Normal preview does not require it."));
    auto* driver_browse = new QPushButton(ui("Parcourir", "Browse"), this);
    auto* driver_load = new QPushButton(ui("Charger driver", "Load driver"), this);
    driver_load->setToolTip(ui(
        "Charge un driver Z80 externe pour tests avances. "
        "Si vide, laissez cette section telle quelle.",
        "Loads an external Z80 driver for advanced tests. "
        "If empty, leave this section as-is."));
    driver_layout->addWidget(driver_path_);
    driver_layout->addWidget(driver_browse);
    driver_layout->addWidget(driver_load);

    auto* file_box = new QGroupBox(ui("Entree MIDI", "MIDI input"), this);
    auto* file_layout = new QHBoxLayout(file_box);
    midi_path_ = new QLineEdit(this);
    midi_path_->setPlaceholderText(ui(
        "Choisir un fichier MIDI (.mid/.midi)",
        "Choose a MIDI file (.mid/.midi)"));
    auto* browse_btn = new QPushButton(ui("Parcourir", "Browse"), this);
    auto* load_btn = new QPushButton(ui("Charger MIDI", "Load MIDI"), this);
    load_btn->setToolTip(ui(
        "Convertit le MIDI en streams preview puis le charge dans le Player.",
        "Converts MIDI to preview streams, then loads it into Player."));
    file_layout->addWidget(midi_path_);
    file_layout->addWidget(browse_btn);
    file_layout->addWidget(load_btn);

    auto* action_box = new QGroupBox(ui("Actions", "Actions"), this);
    auto* action_layout = new QHBoxLayout(action_box);
    auto* play_btn = new QPushButton(ui("Play", "Play"), this);
    auto* stop_btn = new QPushButton(ui("Stop", "Stop"), this);
    auto* export_btn = new QPushButton(ui("Exporter", "Export"), this);
    rebuild_play_btn_ = new QPushButton(ui("Convertir + Play", "Convert + Play"), this);
    play_btn->setToolTip(ui(
        "Lance la lecture du flux deja charge dans le Player.",
        "Starts playback of the stream already loaded in Player."));
    stop_btn->setToolTip(ui(
        "Arrete la lecture et coupe le son.",
        "Stops playback and silences audio."));
    export_btn->setToolTip(ui(
        "Convertit le MIDI vers un fichier .c ou .asm (profil export ci-dessous).",
        "Converts MIDI to a .c or .asm file (using export profile below)."));
    rebuild_play_btn_->setToolTip(ui(
        "Reconstruit un export temporaire depuis le MIDI courant puis le joue immediatement.\n"
        "N'ecrase pas vos fichiers projet.",
        "Rebuilds a temporary export from current MIDI, then plays it immediately.\n"
        "Does not overwrite project files."));
    action_layout->addWidget(play_btn);
    action_layout->addWidget(stop_btn);
    action_layout->addWidget(export_btn);
    action_layout->addWidget(rebuild_play_btn_);

    auto* options_box = new QGroupBox(ui("Options", "Options"), this);
    auto* options_layout = new QFormLayout(options_box);
    options_layout->addRow(new QLabel(ui("Mode: K1Sound MVP", "Mode: K1Sound MVP"), this));
    options_layout->addRow(new QLabel(
        ui("Preview: lecteur de stream PSG (effets driver-like)",
           "Preview: PSG stream player (driver-like FX)"), this));
    preview_profile_ = new QComboBox(this);
    preview_profile_->addItem("Pre-baked (stable)");
    preview_profile_->addItem(ui("Hybride (opcodes driver-like)", "Hybrid (driver-like opcodes)"));
    preview_profile_->setCurrentIndex(1);
    preview_profile_->setToolTip(ui(
        "Profil utilise par le bouton Export.\n"
        "Le preview audio Player est force en mode hybride driver-like.",
        "Profile used by Export button.\n"
        "Player audio preview is forced to driver-like hybrid mode."));
    options_layout->addRow(new QLabel(ui("Profil export", "Export profile"), this), preview_profile_);
    export_format_ = new QComboBox(this);
    export_format_->addItem(ui("Tableaux C (.c)", "C arrays (.c)"));
    export_format_->addItem("ASM (.asm)");
    options_layout->addRow(new QLabel(ui("Format export", "Export format"), this), export_format_);
    {
        auto* meter_row = new QWidget(this);
        auto* meter_layout = new QHBoxLayout(meter_row);
        meter_layout->setContentsMargins(0, 0, 0, 0);
        output_meter_ = new QProgressBar(this);
        output_meter_->setRange(0, 100);
        output_meter_->setValue(0);
        output_meter_->setFormat("%p%");
        output_meter_->setMinimumWidth(180);
        output_meter_label_ = new QLabel(ui("Audio off", "Audio off"), this);
        meter_layout->addWidget(output_meter_, 1);
        meter_layout->addWidget(output_meter_label_);
        options_layout->addRow(new QLabel(ui("Niveau sortie", "Output meter"), this), meter_row);
    }

    auto* quick_help = new QLabel(
        ui("Workflow rapide: 1) Charger MIDI  2) Convertir + Play (ecoute rapide)  "
           "3) Exporter (.c/.asm) pour integration jeu.  "
           "Load Driver = option avancee, non requise pour l'usage normal.",
           "Quick workflow: 1) Load MIDI  2) Convert + Play (quick listen)  "
           "3) Export (.c/.asm) for game integration.  "
           "Load driver = advanced option, not required for normal usage."),
        this);
    quick_help->setWordWrap(true);
    quick_help->setStyleSheet("QLabel { color: #8a8a95; font-size: 11px; }");

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setPlaceholderText(ui("Journal Player...", "Player log..."));

    root->addWidget(driver_box);
    root->addWidget(file_box);
    root->addWidget(action_box);
    root->addWidget(options_box);
    root->addWidget(quick_help);
    root->addWidget(log_, 1);

    connect(driver_browse, &QPushButton::clicked, this, &PlayerTab::on_browse_driver);
    connect(driver_load, &QPushButton::clicked, this, &PlayerTab::on_load_driver);

    connect(browse_btn, &QPushButton::clicked, this, &PlayerTab::on_browse_midi);
    connect(load_btn, &QPushButton::clicked, this, &PlayerTab::on_load_midi);
    connect(play_btn, &QPushButton::clicked, this, [this]() {
        if (!hub_) {
            append_log("Engine hub missing");
            return;
        }
        if (bgm_ready_) {
            start_bgm();
            return;
        }
        append_log("No BGM loaded. Load a MIDI file first.");
    });
    connect(stop_btn, &QPushButton::clicked, this, [this]() {
        stop_bgm();
        if (hub_ && hub_->engine_ready()) {
            psg_helpers::DirectSilenceTone(hub_->engine(), 0);
            psg_helpers::DirectSilenceTone(hub_->engine(), 1);
            psg_helpers::DirectSilenceTone(hub_->engine(), 2);
            psg_helpers::DirectSilenceNoise(hub_->engine());
        }
        if (hub_ && hub_->audio_running()) {
            hub_->stop_audio();
            append_log("Audio stopped");
        }
    });
    connect(export_btn, &QPushButton::clicked, this, [this]() {
        if (midi_path_->text().isEmpty()) {
            append_log("MIDI path empty");
            return;
        }
        const QString filter = "C source (*.c);;ASM (*.asm)";
        QString chosen = QFileDialog::getSaveFileName(
            this, "Export MIDI", QString(), filter);
        if (chosen.isEmpty()) {
            return;
        }
        const bool export_c = export_format_ ? (export_format_->currentIndex() == 0) : true;
        if (export_c && !chosen.endsWith(".c", Qt::CaseInsensitive)) {
            chosen += ".c";
        } else if (!export_c && !chosen.endsWith(".asm", Qt::CaseInsensitive)) {
            chosen += ".asm";
        }
        const bool use_hybrid = preview_profile_ ? (preview_profile_->currentIndex() == 1) : true;
        QString error;
        if (!convert_midi_to_output(midi_path_->text(), chosen, export_c, use_hybrid, &error)) {
            append_log(error.isEmpty() ? "MIDI convert failed" : error);
            return;
        }
        append_log(QString("Exported: %1").arg(chosen));
    });
    connect(rebuild_play_btn_, &QPushButton::clicked, this, [this]() {
        if (midi_path_->text().isEmpty()) {
            append_log("MIDI path empty");
            return;
        }
        const QString out_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (out_dir.isEmpty()) {
            append_log("Temp folder not available");
            return;
        }
        const QString out_path = out_dir + "/ngpc_sc_last.c";
        const bool use_hybrid = true; // Force driver-like preview path.
        QString error;
        if (!convert_midi_to_output(midi_path_->text(), out_path, true, use_hybrid, &error)) {
            append_log(error.isEmpty() ? "MIDI convert failed" : error);
            return;
        }
        QString load_error;
        if (!load_streams_from_c(out_path, &load_error)) {
            append_log(load_error.isEmpty() ? "Load streams failed" : load_error);
            return;
        }
        append_log(QString("Converted and loaded: %1").arg(out_path));
        start_bgm();
    });

    meter_timer_ = new QTimer(this);
    meter_timer_->setInterval(100);
    connect(meter_timer_, &QTimer::timeout, this, &PlayerTab::update_output_meter);
    meter_timer_->start();
}

void PlayerTab::on_browse_driver() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Z80 Driver", QString(), "Driver files (*.drv *.bin);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    driver_path_->setText(path);
}

void PlayerTab::on_load_driver() {
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };
    if (!hub_) {
        append_log("Engine hub missing");
        return;
    }
    const QString driver_path = driver_path_->text().trimmed();
    if (driver_path.isEmpty()) {
        append_log(ui(
            "Aucun driver externe selectionne. C'est optionnel: ignorez cette etape en usage normal.",
            "No external driver selected. This is optional: skip this step for normal usage."));
        return;
    }
    QString error;
    if (hub_->load_driver(driver_path, &error)) {
        append_log(ui("Driver Z80 externe charge.", "External Z80 driver loaded."));
    } else {
        if (error.isEmpty()) {
            append_log("Z80 driver load failed");
        } else {
            append_log(QString("Z80 driver load failed: %1").arg(error));
        }
    }
}

void PlayerTab::on_browse_midi() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Open MIDI", QString(), "MIDI files (*.mid *.midi)");
    if (path.isEmpty()) {
        return;
    }
    midi_path_->setText(path);
}

void PlayerTab::on_load_midi() {
    stop_bgm();
    warned_bad_table_ = false;
    const std::string path = midi_path_->text().toStdString();
    const ngpc::MidiInfo info = ngpc::InspectMidi(path);
    if (!info.valid) {
        append_log(QString("Load failed: %1").arg(QString::fromStdString(info.error)));
        return;
    }
    append_log(QString("MIDI loaded: tracks=%1 division=%2 tempo_events=%3")
                   .arg(info.tracks)
                   .arg(info.ticks_per_beat)
                   .arg(info.tempo_events));
    if (info.downscale_divisor > 1) {
        append_log(QString("Division downscale: /%1 -> %2")
                       .arg(info.downscale_divisor)
                       .arg(info.normalized_ticks_per_beat));
    }
    if (!info.warning.empty()) {
        append_log(QString("Warning: %1").arg(QString::fromStdString(info.warning)));
    }

    const QString out_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (out_dir.isEmpty()) {
        append_log("Temp folder not available");
        return;
    }
    const QString out_path = out_dir + "/ngpc_sc_last.c";
    const bool use_hybrid = true; // Force driver-like preview path.
    QString error;
    if (!convert_midi_to_output(midi_path_->text(), out_path, true, use_hybrid, &error)) {
        append_log(error.isEmpty() ? "MIDI convert failed" : error);
        return;
    }
    QString load_error;
    if (!load_streams_from_c(out_path, &load_error)) {
        append_log(load_error.isEmpty() ? "Load streams failed" : load_error);
        return;
    }
    append_log(QString("Converted and loaded: %1").arg(out_path));
    append_log("Preview profile: Hybride (driver-like, forced)");
    append_log(QString("Preview grid: 48 ticks, fps=60"));
    append_log(QString("Streams: tone=%1 noise=%2")
                   .arg(streams_[0].data.empty() ? 0 : 3)
                   .arg(streams_[3].data.empty() ? 0 : 1));
}

void PlayerTab::append_log(const QString& text) {
    log_->appendPlainText(text);
}

void PlayerTab::start_bgm() {
    if (!hub_) {
        return;
    }
    if (!bgm_ready_) {
        append_log("No BGM loaded");
        return;
    }
    if (note_table_.size() < 2) {
        append_log("NOTE_TABLE missing or too small");
        return;
    }
    if (bgm_playing_) {
        return;
    }
    if (!hub_->ensure_audio_running(44100)) {
        const QString err = hub_->last_audio_error();
        append_log(err.isEmpty() ? "Audio start failed" : err);
        return;
    }
    hub_->set_step_z80(false);
    reset_streams();
    if (!bgm_timer_->isActive()) {
        bgm_timer_->start();
    }
    bgm_playing_ = true;
    append_log("BGM playback started");
}

void PlayerTab::stop_bgm() {
    bgm_playing_ = false;
    if (bgm_timer_ && bgm_timer_->isActive()) {
        bgm_timer_->stop();
    }
    if (hub_ && hub_->engine_ready()) {
        // Silence all PSG channels.
        hub_->engine().psg().write_tone(0x9F);
        hub_->engine().psg().write_tone(0xBF);
        hub_->engine().psg().write_tone(0xDF);
        hub_->engine().psg().write_noise(0xFF);
    }
}

static void PsgTone(ngpc::SoundEngine& engine, int ch, uint8_t lo, uint8_t hi, uint8_t attn) {
    static const uint8_t kToneBase[3] = {0x80, 0xA0, 0xC0};
    static const uint8_t kAttnBase[3] = {0x90, 0xB0, 0xD0};
    const uint8_t b1 = static_cast<uint8_t>(kToneBase[ch] | (lo & 0x0F));
    const uint8_t b2 = static_cast<uint8_t>(hi & 0x3F);
    const uint8_t b3 = static_cast<uint8_t>(kAttnBase[ch] | (attn & 0x0F));
    engine.psg().write_tone(b1);
    engine.psg().write_tone(b2);
    engine.psg().write_tone(b3);
}

static void PsgNoise(ngpc::SoundEngine& engine, uint8_t val, uint8_t attn) {
    const uint8_t b1 = static_cast<uint8_t>(0xE0 | (val & 0x07));
    const uint8_t b3 = static_cast<uint8_t>(0xF0 | (attn & 0x0F));
    engine.psg().write_noise(b1);
    engine.psg().write_noise(b3);
}

static void PsgSilenceTone(ngpc::SoundEngine& engine, int ch) {
    static const uint8_t kAttnBase[3] = {0x90, 0xB0, 0xD0};
    engine.psg().write_tone(static_cast<uint8_t>(kAttnBase[ch] | 0x0F));
}

static void PsgSilenceNoise(ngpc::SoundEngine& engine) {
    engine.psg().write_noise(0xFF);
}

namespace {
const std::vector<ngpc::InstrumentPreset>& DefaultInstrumentPresets() {
    static const std::vector<ngpc::InstrumentPreset> kPresets = ngpc::FactoryInstrumentPresets();
    return kPresets;
}

const std::vector<ngpc::EnvCurveDef>& EnvCurves() {
    static const std::vector<ngpc::EnvCurveDef> kCurves = ngpc::FactoryEnvCurves();
    return kCurves;
}

const std::vector<ngpc::PitchCurveDef>& PitchCurves() {
    static const std::vector<ngpc::PitchCurveDef> kCurves = ngpc::FactoryPitchCurves();
    return kCurves;
}

const std::vector<ngpc::MacroDef>& Macros() {
    static const std::vector<ngpc::MacroDef> kMacros = ngpc::FactoryMacros();
    return kMacros;
}

static int16_t LfoStepWave(uint8_t wave, int16_t cur, int8_t& sign, int16_t depth) {
    if (depth <= 0) {
        return 0;
    }
    switch (wave) {
    case 0: { // triangle
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
    case 1: // square
        sign = (sign < 0) ? static_cast<int8_t>(1) : static_cast<int8_t>(-1);
        return static_cast<int16_t>(depth * sign);
    case 2: { // saw
        int16_t next = static_cast<int16_t>(cur + 1);
        if (next > depth) next = static_cast<int16_t>(-depth);
        return next;
    }
    case 3: // sweep up
        if (cur < depth) return static_cast<int16_t>(cur + 1);
        return depth;
    case 4: // sweep down
        if (cur > -depth) return static_cast<int16_t>(cur - 1);
        return static_cast<int16_t>(-depth);
    default:
        return cur;
    }
}

static bool LfoTick(bool on, uint8_t wave, uint8_t rate, uint8_t depth,
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
        const int16_t next = LfoStepWave(static_cast<uint8_t>(std::min<int>(wave, 4)), delta, sign,
                                         static_cast<int16_t>(depth));
        if (next != delta) {
            delta = next;
            return true;
        }
    } else {
        counter--;
    }
    return false;
}

static int8_t LfoToAttnDelta(int16_t mod) {
    int16_t d = static_cast<int16_t>(mod / 16);
    d = std::clamp<int16_t>(d, -15, 15);
    return static_cast<int8_t>(-d);
}

static void ResolveLfoAlgo(uint8_t algo,
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

ngpc::BgmInstrumentDef PlayerTab::resolve_instrument_def(uint8_t inst_id) const {
    if (instrument_store_ && inst_id < static_cast<uint8_t>(instrument_store_->count())) {
        return instrument_store_->at(inst_id).def;
    }
    const auto& presets = DefaultInstrumentPresets();
    if (inst_id < static_cast<uint8_t>(presets.size())) {
        return presets[inst_id].def;
    }
    return ngpc::BgmInstrumentDef{};
}

void PlayerTab::apply_instrument_to_stream(StreamState& s,
                                           const ngpc::BgmInstrumentDef& def,
                                           bool noise_channel) {
    s.attn = static_cast<uint8_t>(std::min<int>(def.attn, 15));
    s.attn_base = s.attn;
    s.attn_cur = s.attn;

    s.env_on = (def.env_on != 0);
    s.env_step = def.env_step ? def.env_step : 1;
    s.env_speed = def.env_speed ? def.env_speed : 1;
    s.env_counter = s.env_speed;
    s.env_index = 0;
    s.env_curve_id = def.env_curve_id;
    s.env_curve.clear();
    {
        const auto& curves = EnvCurves();
        if (def.env_curve_id < static_cast<uint8_t>(curves.size())) {
            s.env_curve = curves[def.env_curve_id].steps;
        }
    }

    s.pitch_curve.clear();
    s.pitch_counter = s.env_speed;
    s.pitch_index = 0;
    s.pitch_offset = 0;
    {
        const auto& curves = PitchCurves();
        if (def.pitch_curve_id < static_cast<uint8_t>(curves.size())) {
            s.pitch_curve = curves[def.pitch_curve_id].steps;
        }
    }

    s.vib_on = (def.vib_on != 0);
    s.vib_depth = def.vib_depth;
    s.vib_speed = def.vib_speed ? def.vib_speed : 1;
    s.vib_delay = def.vib_delay;
    s.vib_delay_counter = s.vib_delay;
    s.vib_counter = s.vib_speed;
    s.vib_dir = 1;
    s.lfo_on = (def.lfo_on != 0);
    s.lfo_wave = static_cast<uint8_t>(std::clamp<int>(def.lfo_wave, 0, 4));
    s.lfo_hold = def.lfo_hold;
    s.lfo_rate = def.lfo_rate;
    s.lfo_depth = def.lfo_depth;
    s.lfo_hold_counter = s.lfo_hold;
    s.lfo_counter = s.lfo_rate;
    s.lfo_sign = 1;
    s.lfo_delta = 0;
    s.lfo2_on = (def.lfo2_on != 0);
    s.lfo2_wave = static_cast<uint8_t>(std::clamp<int>(def.lfo2_wave, 0, 4));
    s.lfo2_hold = def.lfo2_hold;
    s.lfo2_rate = def.lfo2_rate;
    s.lfo2_depth = def.lfo2_depth;
    s.lfo2_hold_counter = s.lfo2_hold;
    s.lfo2_counter = s.lfo2_rate;
    s.lfo2_sign = 1;
    s.lfo2_delta = 0;
    s.lfo_algo = static_cast<uint8_t>(std::clamp<int>(def.lfo_algo, 0, 7));
    s.lfo_pitch_delta = 0;
    s.lfo_attn_delta = 0;
    if (s.lfo_depth == 0 || s.lfo_rate == 0) s.lfo_on = false;
    if (s.lfo2_depth == 0 || s.lfo2_rate == 0) s.lfo2_on = false;

    s.sweep_on = (def.sweep_on != 0 && def.sweep_step != 0);
    s.sweep_end = def.sweep_end < 1 ? static_cast<uint16_t>(1) :
        (def.sweep_end > 1023 ? static_cast<uint16_t>(1023) : def.sweep_end);
    s.sweep_step = def.sweep_step;
    s.sweep_speed = def.sweep_speed ? def.sweep_speed : 1;
    s.sweep_counter = s.sweep_speed;

    /* Mirror driver behavior: only channel N can run in noise mode. */
    s.mode = noise_channel ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0);
    s.noise_config = static_cast<uint8_t>(def.noise_config & 0x07);
    s.macro_id = def.macro_id;
    s.macro_step = 0;
    s.macro_counter = 0;
    s.macro_active = false;
    s.macro_pitch = 0;

    s.adsr_on = (def.adsr_on != 0);
    s.adsr_attack = def.adsr_attack;
    s.adsr_decay = def.adsr_decay;
    s.adsr_sustain = def.adsr_sustain > 15 ? static_cast<uint8_t>(15) : def.adsr_sustain;
    s.adsr_sustain_rate = def.adsr_sustain_rate;
    s.adsr_release = def.adsr_release;
    s.adsr_phase = 0;
    s.adsr_counter = 0;

    s.fx_active = true;
}

void PlayerTab::stream_macro_reset(StreamState& s) {
    s.macro_step = 0;
    s.macro_counter = 0;
    s.macro_pitch = 0;
    s.macro_active = false;
    const auto& macros = Macros();
    if (s.macro_id >= static_cast<uint8_t>(macros.size()) ||
        macros[s.macro_id].steps.empty()) {
        return;
    }
    const auto& st = macros[s.macro_id].steps[0];
    if (st.frames == 0) {
        return;
    }
    s.macro_active = true;
    s.macro_counter = st.frames;
    s.macro_pitch = st.pitch_delta;
    if (!s.adsr_on) {
        int attn = static_cast<int>(s.attn_base) + static_cast<int>(st.attn_delta);
        s.attn_cur = static_cast<uint8_t>(std::clamp(attn, 0, 15));
    }
}

uint16_t PlayerTab::compute_stream_tone_divider(const StreamState& s, uint16_t base_div) const {
    uint16_t div = base_div;

    // Order aligned with TrackerVoice::final_divider + channel pitch bend:
    // macro -> pitch curve -> lfo -> vibrato -> pitch bend.
    if (s.macro_pitch != 0) {
        int32_t md = static_cast<int32_t>(div) + static_cast<int32_t>(s.macro_pitch);
        if (md < 1) md = 1;
        if (md > 1023) md = 1023;
        div = static_cast<uint16_t>(md);
    }
    if (s.pitch_offset != 0) {
        int32_t pd = static_cast<int32_t>(div) + static_cast<int32_t>(s.pitch_offset);
        if (pd < 1) pd = 1;
        if (pd > 1023) pd = 1023;
        div = static_cast<uint16_t>(pd);
    }
    if (s.lfo_pitch_delta != 0) {
        int32_t ld = static_cast<int32_t>(div) + static_cast<int32_t>(s.lfo_pitch_delta);
        if (ld < 1) ld = 1;
        if (ld > 1023) ld = 1023;
        div = static_cast<uint16_t>(ld);
    }
    if (s.vib_on && s.vib_depth > 0 && s.vib_delay_counter == 0) {
        int16_t vib_delta = static_cast<int16_t>(s.vib_depth) * static_cast<int16_t>(s.vib_dir);
        int32_t vd = static_cast<int32_t>(div) + static_cast<int32_t>(vib_delta);
        if (vd < 1) vd = 1;
        if (vd > 1023) vd = 1023;
        div = static_cast<uint16_t>(vd);
    }
    if (s.pitch_bend != 0) {
        int32_t bd = static_cast<int32_t>(div) + static_cast<int32_t>(s.pitch_bend);
        if (bd < 1) bd = 1;
        if (bd > 1023) bd = 1023;
        div = static_cast<uint16_t>(bd);
    }

    return div;
}

void PlayerTab::tick_bgm() {
    if (!hub_ || !bgm_ready_ || !bgm_playing_) {
        return;
    }
    if (note_table_.size() < 2) {
        if (!warned_bad_table_) {
            append_log("NOTE_TABLE invalid; stopping playback");
            warned_bad_table_ = true;
        }
        stop_bgm();
        return;
    }
    auto& engine = hub_->engine();
    bool fade_attn_dirty = false;

    auto step_stream = [&](StreamState& s, int ch, bool noise) {
        if (!s.active) {
            return;
        }
        if (s.remaining > 0) {
            s.remaining--;
            return;
        }

        const auto silence = [&]() {
            if (noise) {
                PsgSilenceNoise(engine);
            } else {
                PsgSilenceTone(engine, ch);
            }
        };

        while (true) {
            if (s.pos >= s.data.size()) {
                if (s.loop_offset > 0 && s.loop_offset < s.data.size()) {
                    s.pos = s.loop_offset;
                } else {
                    s.active = false;
                    silence();
                    return;
                }
            }
            if (s.pos >= s.data.size()) {
                s.active = false;
                silence();
                return;
            }
            const uint8_t note = s.data[s.pos++];
            if (note == 0x00) {
                if (s.loop_offset > 0 && s.loop_offset < s.data.size()) {
                    s.pos = s.loop_offset;
                    continue;
                }
                s.active = false;
                silence();
                return;
            }
            if (note == 0xFF) {
                if (s.pos >= s.data.size()) {
                    s.active = false;
                    s.note_active = false;
                    silence();
                    return;
                }
                const uint8_t dur = s.data[s.pos++];
                s.remaining = dur;
                if (s.adsr_on && s.adsr_release > 0 && s.note_active) {
                    // Start ADSR release instead of immediate silence
                    s.adsr_phase = 4; // REL
                    s.adsr_counter = s.adsr_release;
                    // note_active stays true â€” tick_stream_fx handles release
                } else {
                    s.note_active = false;
                    silence();
                }
                return;
            }
            if (note >= 0xF0) {
                switch (note) {
                case 0xF0: // SET_ATTN
                    if (s.pos < s.data.size()) {
                        s.attn = static_cast<uint8_t>(s.data[s.pos++] & 0x0F);
                        s.attn_base = s.attn;
                        s.attn_cur = s.attn;
                        s.pending_write = true;
                    }
                    break;
                case 0xF1: // SET_ENV
                    if (s.pos + 1 < s.data.size()) {
                        uint8_t step = s.data[s.pos++];
                        uint8_t speed = s.data[s.pos++];
                        if (step > 4) step = 4;
                        if (speed < 1) speed = 1;
                        if (speed > 10) speed = 10;
                        s.env_on = (step > 0);
                        s.env_step = step > 0 ? step : 1;
                        s.env_speed = speed;
                        s.env_counter = speed;
                        s.env_index = 0;
                        s.fx_active = true;
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xF2: // SET_VIB
                    if (s.pos + 2 < s.data.size()) {
                        uint8_t depth = s.data[s.pos++];
                        uint8_t speed = s.data[s.pos++];
                        uint8_t delay = s.data[s.pos++];
                        if (speed < 1) speed = 1;
                        if (speed > 30) speed = 30;
                        s.vib_on = (depth > 0);
                        s.vib_depth = depth;
                        s.vib_speed = speed;
                        s.vib_delay = delay;
                        s.vib_delay_counter = delay;
                        s.vib_counter = speed;
                        s.vib_dir = 1;
                        s.fx_active = true;
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xF3: // SET_SWEEP
                    if (s.pos + 3 < s.data.size()) {
                        uint16_t end_val = static_cast<uint16_t>(s.data[s.pos]) |
                                           (static_cast<uint16_t>(s.data[s.pos + 1]) << 8);
                        int8_t step_val = static_cast<int8_t>(s.data[s.pos + 2]);
                        uint8_t speed = s.data[s.pos + 3];
                        s.pos += 4;
                        if (speed < 1) speed = 1;
                        if (speed > 30) speed = 30;
                        if (end_val < 1) end_val = 1;
                        if (end_val > 1023) end_val = 1023;
                        s.sweep_on = (step_val != 0);
                        s.sweep_end = end_val;
                        s.sweep_step = static_cast<int16_t>(step_val);
                        s.sweep_speed = speed;
                        s.sweep_counter = speed;
                        s.fx_active = true;
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xF4: // SET_INST
                    if (s.pos < s.data.size()) {
                        const uint8_t inst_id = s.data[s.pos++];
                        const ngpc::BgmInstrumentDef def = resolve_instrument_def(inst_id);
                        apply_instrument_to_stream(s, def, noise);
                        s.pending_write = true;
                    }
                    break;
                case 0xF9: // SET_ADSR
                    if (s.pos + 3 < s.data.size()) {
                        uint8_t a = s.data[s.pos++];
                        uint8_t d = s.data[s.pos++];
                        uint8_t sus = s.data[s.pos++];
                        uint8_t r = s.data[s.pos++];
                        s.adsr_on = true;
                        s.adsr_attack = a;
                        s.adsr_decay = d;
                        s.adsr_sustain = sus > 15 ? static_cast<uint8_t>(15) : sus;
                        s.adsr_sustain_rate = 0;
                        s.adsr_release = r;
                        s.adsr_phase = 0;
                        s.adsr_counter = 0;
                        s.fx_active = true;
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xFA: // SET_LFO
                    if (s.pos + 2 < s.data.size()) {
                        uint8_t wave = s.data[s.pos++];
                        uint8_t rate = s.data[s.pos++];
                        uint8_t depth = s.data[s.pos++];
                        s.lfo_on = (depth > 0 && rate > 0);
                        s.lfo_wave = static_cast<uint8_t>(std::clamp<int>(wave, 0, 4));
                        s.lfo_hold = 0;
                        s.lfo_rate = rate;
                        s.lfo_depth = depth;
                        s.lfo_hold_counter = 0;
                        s.lfo_counter = rate;
                        s.lfo_sign = 1;
                        s.lfo_delta = 0;
                        s.lfo2_on = false;
                        s.lfo_algo = 1;
                        s.lfo2_delta = 0;
                        s.lfo_pitch_delta = 0;
                        s.lfo_attn_delta = 0;
                        s.fx_active = true;
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xFE: // EXT (ADSR5 / MOD2)
                    if (s.pos >= s.data.size()) {
                        s.pos = s.data.size();
                        break;
                    }
                    {
                        const uint8_t sub = s.data[s.pos++];
                        if (sub == 0x01) { // ADSR5
                            if (s.pos + 4 < s.data.size()) {
                                uint8_t a = s.data[s.pos++];
                                uint8_t d = s.data[s.pos++];
                                uint8_t sl = s.data[s.pos++];
                                uint8_t sr = s.data[s.pos++];
                                uint8_t rr = s.data[s.pos++];
                                s.adsr_on = true;
                                s.adsr_attack = a;
                                s.adsr_decay = d;
                                s.adsr_sustain = sl > 15 ? static_cast<uint8_t>(15) : sl;
                                s.adsr_sustain_rate = sr;
                                s.adsr_release = rr;
                                s.adsr_phase = 0;
                                s.adsr_counter = 0;
                                s.fx_active = true;
                                s.pending_write = true;
                            } else {
                                s.pos = s.data.size();
                            }
                        } else if (sub == 0x02) { // MOD2
                            if (s.pos + 10 < s.data.size()) {
                                s.lfo_algo = static_cast<uint8_t>(s.data[s.pos++] & 0x07);
                                s.lfo_on = s.data[s.pos++] != 0;
                                s.lfo_wave = static_cast<uint8_t>(std::clamp<int>(s.data[s.pos++], 0, 4));
                                s.lfo_hold = s.data[s.pos++];
                                s.lfo_rate = s.data[s.pos++];
                                s.lfo_depth = s.data[s.pos++];
                                s.lfo2_on = s.data[s.pos++] != 0;
                                s.lfo2_wave = static_cast<uint8_t>(std::clamp<int>(s.data[s.pos++], 0, 4));
                                s.lfo2_hold = s.data[s.pos++];
                                s.lfo2_rate = s.data[s.pos++];
                                s.lfo2_depth = s.data[s.pos++];
                                s.lfo_hold_counter = s.lfo_hold;
                                s.lfo_counter = s.lfo_rate;
                                s.lfo_sign = 1;
                                s.lfo_delta = 0;
                                s.lfo2_hold_counter = s.lfo2_hold;
                                s.lfo2_counter = s.lfo2_rate;
                                s.lfo2_sign = 1;
                                s.lfo2_delta = 0;
                                s.lfo_pitch_delta = 0;
                                s.lfo_attn_delta = 0;
                                if (s.lfo_depth == 0 || s.lfo_rate == 0) s.lfo_on = false;
                                if (s.lfo2_depth == 0 || s.lfo2_rate == 0) s.lfo2_on = false;
                                s.fx_active = true;
                                s.pending_write = true;
                            } else {
                                s.pos = s.data.size();
                            }
                        } else {
                            // Unknown ext subcommand: consume one guard byte.
                            if (s.pos < s.data.size()) s.pos++;
                        }
                    }
                    break;
                case 0xF6: // HOST_CMD
                    if (s.pos + 1 < s.data.size()) {
                        uint8_t type = s.data[s.pos++];
                        uint8_t data = s.data[s.pos++];
                        if (type == 0) {
                            // Fade out
                            if (data == 0) {
                                // Cancel fade and restore baseline attenuation immediately.
                                fade_speed_ = 0;
                                fade_counter_ = 0;
                                if (fade_attn_ != 0) {
                                    fade_attn_ = 0;
                                    fade_attn_dirty = true;
                                }
                            } else {
                                fade_speed_ = data;
                                fade_counter_ = data;
                            }
                        } else if (type == 1) {
                            // Tempo change (not directly applicable in PlayerTab;
                            // note durations are pre-baked, but log it)
                            (void)data;
                        }
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                case 0xF7: // SET_EXPR
                    if (s.pos < s.data.size()) {
                        uint8_t expr = s.data[s.pos++];
                        if (expr > 15) expr = 15;
                        s.expression = expr;
                        s.pending_write = true;
                    }
                    break;
                case 0xF8: // PITCH_BEND
                    if (s.pos + 1 < s.data.size()) {
                        uint8_t lo = s.data[s.pos++];
                        uint8_t hi = s.data[s.pos++];
                        s.pitch_bend = static_cast<int16_t>(
                            static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8));
                        s.pending_write = true;
                    } else {
                        s.pos = s.data.size();
                    }
                    break;
                default:
                    // Future opcodes: skip known param counts
                    if (note == 0xF5) { // SET_PAN: 1 byte
                        s.pos = std::min<size_t>(s.pos + 1, s.data.size());
                    } else {
                        s.pos = std::min<size_t>(s.pos + 1, s.data.size());
                    }
                    break;
                }
                continue;
            }

            if (s.pos >= s.data.size()) {
                s.active = false;
                silence();
                return;
            }
            const uint8_t dur = s.data[s.pos++];
            s.remaining = dur == 0 ? 1 : dur;

            if (noise || s.mode == 1) {
                const uint8_t val = static_cast<uint8_t>((note - 1) & 0x07);
                // Keep note-on behavior aligned with tone channels and driver:
                // reset ADSR/envelope state, then emit using current attenuation.
                s.set_note(1);
                stream_macro_reset(s);
                {
                    uint8_t note_attn = s.attn_cur;
                    if (s.expression > 0) {
                        int ea = static_cast<int>(note_attn) + static_cast<int>(s.expression);
                        note_attn = static_cast<uint8_t>(std::min(ea, 15));
                    }
                    if (fade_attn_ > 0) {
                        int fa = static_cast<int>(note_attn) + static_cast<int>(fade_attn_);
                        note_attn = static_cast<uint8_t>(std::min(fa, 15));
                    }
                    PsgNoise(engine, val, note_attn);
                }
            } else {
                const size_t idx = static_cast<size_t>(note - 1);
                if (idx * 2 + 1 < note_table_.size()) {
                    const uint8_t lo_raw = note_table_[idx * 2 + 0];
                    const uint8_t hi_raw = note_table_[idx * 2 + 1];
                    // Compute divider from lo/hi
                    uint16_t div = static_cast<uint16_t>(lo_raw & 0x0F) |
                                   (static_cast<uint16_t>(hi_raw & 0x3F) << 4);
                    s.set_note(div);
                    stream_macro_reset(s);
                    {
                        uint8_t note_attn = s.attn_cur;
                        if (s.expression > 0) {
                            int ea = static_cast<int>(note_attn) + static_cast<int>(s.expression);
                            note_attn = static_cast<uint8_t>(std::min(ea, 15));
                        }
                        if (fade_attn_ > 0) {
                            int fa = static_cast<int>(note_attn) + static_cast<int>(fade_attn_);
                            note_attn = static_cast<uint8_t>(std::min(fa, 15));
                        }
                        div = compute_stream_tone_divider(s, div);
                        const uint8_t lo = static_cast<uint8_t>(div & 0x0F);
                        const uint8_t hi = static_cast<uint8_t>((div >> 4) & 0x3F);
                        PsgTone(engine, ch, lo, hi, note_attn);
                    }
                } else {
                    PsgSilenceTone(engine, ch);
                    s.note_active = false;
                }
            }
            return;
        }
    };

    step_stream(streams_[0], 0, false);
    step_stream(streams_[1], 1, false);
    step_stream(streams_[2], 2, false);
    step_stream(streams_[3], 3, true);

    // Global fade processing
    if (fade_speed_ > 0) {
        if (fade_counter_ == 0) {
            if (fade_attn_ < 15) {
                fade_attn_++;
                fade_attn_dirty = true;
            }
            fade_counter_ = fade_speed_;
        } else {
            fade_counter_--;
        }
    }

    // Per-tick instrument effect processing (envelope, vibrato, sweep)
    tick_stream_fx(streams_[0], 0, false, fade_attn_dirty);
    tick_stream_fx(streams_[1], 1, false, fade_attn_dirty);
    tick_stream_fx(streams_[2], 2, false, fade_attn_dirty);
    tick_stream_fx(streams_[3], 3, true, fade_attn_dirty);
}

void PlayerTab::update_output_meter() {
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

void PlayerTab::tick_stream_fx(StreamState& s, int ch, bool noise, bool force_write) {
    if (!s.note_active || !hub_ || !hub_->engine_ready()) {
        return;
    }
    if (!s.fx_active && !force_write && !s.pending_write) {
        return;
    }
    auto& engine = hub_->engine();
    bool dirty = s.pending_write;
    s.pending_write = false;

    if (s.macro_active) {
        if (s.macro_counter == 0) {
            const auto& macros = Macros();
            s.macro_step++;
            if (s.macro_id >= static_cast<uint8_t>(macros.size()) ||
                s.macro_step >= static_cast<uint8_t>(macros[s.macro_id].steps.size())) {
                s.macro_active = false;
            } else {
                const auto& st = macros[s.macro_id].steps[s.macro_step];
                if (st.frames == 0) {
                    s.macro_active = false;
                } else {
                    s.macro_counter = st.frames;
                    s.macro_pitch = st.pitch_delta;
                    if (!s.adsr_on) {
                        int attn = static_cast<int>(s.attn_base) + static_cast<int>(st.attn_delta);
                        uint8_t next = static_cast<uint8_t>(std::clamp(attn, 0, 15));
                        if (s.attn_cur != next) {
                            s.attn_cur = next;
                            dirty = true;
                        }
                    }
                }
            }
        }
        if (s.macro_active && s.macro_counter > 0) {
            s.macro_counter--;
        }
    }

    if (!s.pitch_curve.empty()) {
        if (s.pitch_counter == 0) {
            uint8_t idx = s.pitch_index;
            if (idx >= static_cast<uint8_t>(s.pitch_curve.size())) {
                idx = static_cast<uint8_t>(s.pitch_curve.size() - 1);
            } else {
                s.pitch_index++;
            }
            s.pitch_offset = s.pitch_curve[idx];
            s.pitch_counter = s.env_speed;
            dirty = true;
        } else {
            s.pitch_counter--;
        }
    }

    if (s.adsr_on && s.adsr_phase > 0) {
        switch (s.adsr_phase) {
        case 1:
            if (s.adsr_attack == 0) {
                s.attn_cur = s.attn_base;
                s.adsr_phase = 2;
                s.adsr_counter = s.adsr_decay;
                dirty = true;
            } else if (s.adsr_counter == 0) {
                if (s.attn_cur > s.attn_base) {
                    s.attn_cur--;
                    dirty = true;
                }
                if (s.attn_cur <= s.attn_base) {
                    s.attn_cur = s.attn_base;
                    s.adsr_phase = 2;
                    s.adsr_counter = s.adsr_decay;
                } else {
                    s.adsr_counter = s.adsr_attack;
                }
            } else {
                s.adsr_counter--;
            }
            break;
        case 2: {
            uint8_t sus = s.adsr_sustain < s.attn_base ? s.attn_base : s.adsr_sustain;
            if (s.adsr_decay == 0 || sus <= s.attn_base) {
                s.attn_cur = sus;
                s.adsr_phase = 3;
                s.adsr_counter = s.adsr_sustain_rate;
                dirty = true;
            } else if (s.adsr_counter == 0) {
                if (s.attn_cur < sus) {
                    s.attn_cur++;
                    dirty = true;
                }
                if (s.attn_cur >= sus) {
                    s.attn_cur = sus;
                    s.adsr_phase = 3;
                    s.adsr_counter = s.adsr_sustain_rate;
                } else {
                    s.adsr_counter = s.adsr_decay;
                }
            } else {
                s.adsr_counter--;
            }
            break;
        }
        case 3:
            if (s.adsr_sustain_rate > 0) {
                if (s.adsr_counter == 0) {
                    if (s.attn_cur < 15) {
                        s.attn_cur++;
                        dirty = true;
                    }
                    if (s.attn_cur >= 15) {
                        s.adsr_phase = 0;
                        s.note_active = false;
                    } else {
                        s.adsr_counter = s.adsr_sustain_rate;
                    }
                } else {
                    s.adsr_counter--;
                }
            }
            break;
        case 4:
            if (s.adsr_release == 0) {
                s.attn_cur = 15;
                s.adsr_phase = 0;
                s.note_active = false;
                dirty = true;
            } else if (s.adsr_counter == 0) {
                if (s.attn_cur < 15) {
                    s.attn_cur++;
                    dirty = true;
                }
                if (s.attn_cur >= 15) {
                    s.adsr_phase = 0;
                    s.note_active = false;
                } else {
                    s.adsr_counter = s.adsr_release;
                }
            } else {
                s.adsr_counter--;
            }
            break;
        }
    } else if (s.env_on) {
        if (s.env_counter == 0) {
            if (!s.env_curve.empty()) {
                uint8_t idx = s.env_index;
                if (idx >= static_cast<uint8_t>(s.env_curve.size())) {
                    idx = static_cast<uint8_t>(s.env_curve.size() - 1);
                } else {
                    s.env_index++;
                }
                int attn = static_cast<int>(s.attn_base) + static_cast<int>(s.env_curve[idx]);
                uint8_t next = static_cast<uint8_t>(std::clamp(attn, 0, 15));
                if (s.attn_cur != next) {
                    s.attn_cur = next;
                    dirty = true;
                }
            } else if (s.attn_cur < 15) {
                uint8_t next = static_cast<uint8_t>(s.attn_cur + s.env_step);
                if (next > 15) next = 15;
                s.attn_cur = next;
                dirty = true;
            }
            s.env_counter = s.env_speed;
        } else {
            s.env_counter--;
        }
    }

    if (!noise && s.mode == 0 && s.sweep_on && s.sweep_step != 0) {
        if (s.sweep_counter == 0) {
            int32_t nd = static_cast<int32_t>(s.tone_div) + static_cast<int32_t>(s.sweep_step);
            if (nd < 1) nd = 1;
            if (nd > 1023) nd = 1023;
            s.tone_div = static_cast<uint16_t>(nd);
            s.sweep_counter = s.sweep_speed;
            dirty = true;
            if (s.sweep_step > 0) {
                if (s.tone_div >= s.sweep_end) s.sweep_on = false;
            } else {
                if (s.tone_div <= s.sweep_end) s.sweep_on = false;
            }
        } else {
            s.sweep_counter--;
        }
    }

    // Mirror InstrumentPlayer/driver-like behavior:
    // vibrato updates direction at vib_speed cadence; PSG write happens on dirty state.
    if (!noise && s.mode == 0 && s.vib_on && s.vib_depth > 0) {
        if (s.vib_delay_counter > 0) {
            s.vib_delay_counter--;
            if (s.vib_delay_counter == 0) {
                s.vib_counter = s.vib_speed;
                s.vib_dir = 1;
                dirty = true;
            }
        } else {
            if (s.vib_counter == 0) {
                s.vib_dir = (s.vib_dir < 0) ? static_cast<int8_t>(1) : static_cast<int8_t>(-1);
                s.vib_counter = s.vib_speed;
                dirty = true;
            } else {
                s.vib_counter--;
            }
        }
    }

    if (!noise && s.mode == 0) {
        const int16_t prev_pitch = s.lfo_pitch_delta;
        const int8_t prev_attn = s.lfo_attn_delta;
        bool lfo_dirty = false;
        if (LfoTick(s.lfo_on, s.lfo_wave, s.lfo_rate, s.lfo_depth,
                    s.lfo_hold_counter, s.lfo_counter, s.lfo_sign, s.lfo_delta)) {
            lfo_dirty = true;
        }
        if (LfoTick(s.lfo2_on, s.lfo2_wave, s.lfo2_rate, s.lfo2_depth,
                    s.lfo2_hold_counter, s.lfo2_counter, s.lfo2_sign, s.lfo2_delta)) {
            lfo_dirty = true;
        }
        ResolveLfoAlgo(s.lfo_algo, s.lfo_delta, s.lfo2_delta, s.lfo_pitch_delta, s.lfo_attn_delta);
        if (s.lfo_pitch_delta != prev_pitch || s.lfo_attn_delta != prev_attn) {
            lfo_dirty = true;
        }
        if (lfo_dirty) {
            dirty = true;
        }
    } else if (s.lfo_pitch_delta != 0 || s.lfo_attn_delta != 0) {
        s.lfo_pitch_delta = 0;
        s.lfo_attn_delta = 0;
        dirty = true;
    }

    if (!dirty && !force_write) {
        return;
    }

    uint8_t final_attn = s.attn_cur;
    if (s.lfo_attn_delta != 0) {
        int la = static_cast<int>(final_attn) + static_cast<int>(s.lfo_attn_delta);
        final_attn = static_cast<uint8_t>(std::clamp(la, 0, 15));
    }
    if (s.expression > 0) {
        int ea = static_cast<int>(final_attn) + static_cast<int>(s.expression);
        final_attn = static_cast<uint8_t>(std::min(ea, 15));
    }
    if (fade_attn_ > 0) {
        int fa = static_cast<int>(final_attn) + static_cast<int>(fade_attn_);
        final_attn = static_cast<uint8_t>(std::min(fa, 15));
    }

    if (noise || s.mode == 1) {
        engine.psg().write_noise(static_cast<uint8_t>(0xF0 | (final_attn & 0x0F)));
    } else {
        const uint16_t div = compute_stream_tone_divider(s, s.tone_div);
        static const uint8_t kToneBase[3] = {0x80, 0xA0, 0xC0};
        static const uint8_t kAttnBase[3] = {0x90, 0xB0, 0xD0};
        engine.psg().write_tone(static_cast<uint8_t>(kToneBase[ch] | (div & 0x0F)));
        engine.psg().write_tone(static_cast<uint8_t>((div >> 4) & 0x3F));
        engine.psg().write_tone(static_cast<uint8_t>(kAttnBase[ch] | (final_attn & 0x0F)));
    }
}

void PlayerTab::reset_streams() {
    for (auto& s : streams_) {
        s.pos = 0;
        s.remaining = 0;
        s.attn = 2;
        s.active = !s.data.empty();
        s.reset_fx();
    }
    fade_speed_ = 0;
    fade_counter_ = 0;
    fade_attn_ = 0;
}

static std::string StripComments(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < text.size()) {
                i += 2;
            }
            continue;
        }
        out.push_back(text[i++]);
    }
    return out;
}

static bool ParseArray(const std::string& text, const std::string& name, std::vector<uint8_t>* out) {
    const std::string needle = name + "[]";
    size_t pos = text.find(needle);
    if (pos == std::string::npos) {
        return false;
    }
    pos = text.find('{', pos);
    if (pos == std::string::npos) {
        return false;
    }
    size_t end = text.find("};", pos);
    if (end == std::string::npos) {
        return false;
    }
    std::vector<uint8_t> values;
    size_t i = pos + 1;
    while (i < end) {
        while (i < end && (text[i] == ',' || text[i] == ' ' || text[i] == '\n' || text[i] == '\r' || text[i] == '\t')) {
            ++i;
        }
        if (i >= end) {
            break;
        }
        char* next = nullptr;
        long val = std::strtol(&text[i], &next, 0);
        if (&text[i] == next) {
            ++i;
            continue;
        }
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        values.push_back(static_cast<uint8_t>(val));
        i = static_cast<size_t>(next - text.data());
    }
    *out = std::move(values);
    return true;
}

static bool ParseU16(const std::string& text, const std::string& name, uint16_t* out) {
    size_t pos = text.find(name);
    if (pos == std::string::npos) {
        return false;
    }
    pos = text.find('=', pos);
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    char* next = nullptr;
    long val = std::strtol(&text[pos], &next, 0);
    if (&text[pos] == next) {
        return false;
    }
    if (val < 0) val = 0;
    if (val > 65535) val = 65535;
    *out = static_cast<uint16_t>(val);
    return true;
}

bool PlayerTab::load_streams_from_c(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = "Failed to read output file";
        }
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    std::string text = StripComments(data.toStdString());

    std::vector<uint8_t> note_table;
    if (!ParseArray(text, "NOTE_TABLE", &note_table)) {
        if (error) {
            *error = "NOTE_TABLE not found in output";
        }
        return false;
    }
    note_table_ = std::move(note_table);

    streams_[0].data.clear();
    streams_[1].data.clear();
    streams_[2].data.clear();
    streams_[3].data.clear();
    streams_[0].loop_offset = 0;
    streams_[1].loop_offset = 0;
    streams_[2].loop_offset = 0;
    streams_[3].loop_offset = 0;

    bool has_poly = false;
    if (ParseArray(text, "BGM_CH0", &streams_[0].data)) {
        has_poly = true;
        ParseArray(text, "BGM_CH1", &streams_[1].data);
        ParseArray(text, "BGM_CH2", &streams_[2].data);
        ParseArray(text, "BGM_CHN", &streams_[3].data);
        ParseU16(text, "BGM_CH0_LOOP", &streams_[0].loop_offset);
        ParseU16(text, "BGM_CH1_LOOP", &streams_[1].loop_offset);
        ParseU16(text, "BGM_CH2_LOOP", &streams_[2].loop_offset);
        ParseU16(text, "BGM_CHN_LOOP", &streams_[3].loop_offset);
    } else {
        ParseArray(text, "BGM_MONO", &streams_[0].data);
        ParseU16(text, "BGM_MONO_LOOP", &streams_[0].loop_offset);
    }

    bgm_ready_ = !streams_[0].data.empty();
    last_c_path_ = path;
    reset_streams();
    if (!bgm_ready_ && error) {
        *error = "No BGM streams found in output";
    }
    return bgm_ready_;
}

bool PlayerTab::convert_midi_to_output(const QString& midi_path,
                                       const QString& out_path,
                                       bool c_array,
                                       bool use_hybrid_opcodes,
                                       QString* error) {
    const QString script_candidate = QStringLiteral("C:/Users/wilfr/Desktop/NGPC_RAG/midi_to_ngpc/midi_to_ngpc.py");
    QString script = script_candidate;
    if (!QFile::exists(script)) {
        if (error) {
            *error = "midi_to_ngpc.py not found";
        }
        return false;
    }

    QStringList args;
    args << script
         << midi_path
         << out_path
         << "--profile" << "fidelity"
         << "--force-tone-streams"
         << "--force-noise-stream";
    if (!use_hybrid_opcodes) {
        args << "--no-opcodes";
    }
    if (c_array) {
        args << "--c-array";
    }

    QProcess proc;
    proc.start("python", args);
    if (!proc.waitForFinished(60000)) {
        if (error) {
            *error = "Converter timed out";
        }
        return false;
    }
    const QString std_err = QString::fromLocal8Bit(proc.readAllStandardError());
    const QString std_out = QString::fromLocal8Bit(proc.readAllStandardOutput());
    if (proc.exitCode() != 0) {
        if (error) {
            if (std_err.contains("No module named 'mido'")) {
                *error = "Python module 'mido' missing. Run: pip install mido";
            } else if (!std_err.isEmpty()) {
                *error = std_err.trimmed();
            } else {
                *error = "Converter failed";
            }
        }
        return false;
    }
    if (!std_out.trimmed().isEmpty()) {
        append_log(std_out.trimmed());
    }

    return true;
}
