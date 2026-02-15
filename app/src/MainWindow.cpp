#include "MainWindow.h"

#include <algorithm>

#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTabWidget>
#include <QTimer>

#include "audio/EngineHub.h"
#include "models/InstrumentStore.h"
#include "models/SongDocument.h"
#include "ngpc/instrument.h"
#include "tabs/PlayerTab.h"
#include "tabs/ProjectTab.h"
#include "tabs/TrackerTab.h"
#include "tabs/InstrumentTab.h"
#include "tabs/SfxLabTab.h"
#include "tabs/DebugTab.h"
#include "tabs/HelpTab.h"

namespace {
bool instrument_def_equals(const ngpc::BgmInstrumentDef& a, const ngpc::BgmInstrumentDef& b) {
    return
        a.attn == b.attn &&
        a.env_on == b.env_on &&
        a.env_step == b.env_step &&
        a.env_speed == b.env_speed &&
        a.env_curve_id == b.env_curve_id &&
        a.pitch_curve_id == b.pitch_curve_id &&
        a.vib_on == b.vib_on &&
        a.vib_depth == b.vib_depth &&
        a.vib_speed == b.vib_speed &&
        a.vib_delay == b.vib_delay &&
        a.sweep_on == b.sweep_on &&
        a.sweep_end == b.sweep_end &&
        a.sweep_step == b.sweep_step &&
        a.sweep_speed == b.sweep_speed &&
        a.mode == b.mode &&
        a.noise_config == b.noise_config &&
        a.macro_id == b.macro_id &&
        a.adsr_on == b.adsr_on &&
        a.adsr_attack == b.adsr_attack &&
        a.adsr_decay == b.adsr_decay &&
        a.adsr_sustain == b.adsr_sustain &&
        a.adsr_sustain_rate == b.adsr_sustain_rate &&
        a.adsr_release == b.adsr_release &&
        a.lfo_on == b.lfo_on &&
        a.lfo_wave == b.lfo_wave &&
        a.lfo_hold == b.lfo_hold &&
        a.lfo_rate == b.lfo_rate &&
        a.lfo_depth == b.lfo_depth &&
        a.lfo2_on == b.lfo2_on &&
        a.lfo2_wave == b.lfo2_wave &&
        a.lfo2_hold == b.lfo2_hold &&
        a.lfo2_rate == b.lfo2_rate &&
        a.lfo2_depth == b.lfo2_depth &&
        a.lfo_algo == b.lfo_algo;
}

bool instrument_preset_equals(const ngpc::InstrumentPreset& a, const ngpc::InstrumentPreset& b) {
    return a.name == b.name && instrument_def_equals(a.def, b.def);
}

QString c_string_escape(const QString& text) {
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar ch : text) {
        if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '\"') {
            out += "\\\"";
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else {
            out += ch;
        }
    }
    return out;
}
} // namespace

MainWindow::MainWindow(const QString& project_root,
                       bool create_new_project,
                       const QString& project_name,
                       AppLanguage language,
                       bool free_edit_mode,
                       QWidget* parent)
    : QMainWindow(parent)
    , project_root_(project_root)
    , free_edit_mode_(free_edit_mode)
    , language_(language)
{
    setWindowTitle("NGPC Sound Creator");

    engine_ = new EngineHub(this);
    instrument_store_ = new InstrumentStore(this);
    autosave_timer_ = new QTimer(this);

    tabs_ = new QTabWidget(this);
    project_tab_ = new ProjectTab(language_, this);
    tracker_tab_ = new TrackerTab(engine_, instrument_store_, this);
    instrument_tab_ = new InstrumentTab(engine_, instrument_store_, this);
    sfx_tab_ = new SfxLabTab(engine_, this);

    tabs_->addTab(project_tab_, ui("Projet", "Project"));
    tabs_->addTab(new PlayerTab(engine_, instrument_store_, this), "Player");
    tabs_->addTab(tracker_tab_, "Tracker");
    tabs_->addTab(instrument_tab_, "Instruments");
    tabs_->addTab(sfx_tab_, "SFX Lab");
    tabs_->addTab(new DebugTab(this), "Debug");
    tabs_->addTab(new HelpTab(this), ui("Aide", "Help"));

    setCentralWidget(tabs_);
    resize(1200, 800);

    // Warm-up audio engine once at startup so SFX preview works immediately.
    QTimer::singleShot(0, this, [this]() {
        if (!engine_) return;
        engine_->set_step_z80(false);
        engine_->ensure_audio_running(44100);
    });

    connect(autosave_timer_, &QTimer::timeout, this, [this]() {
        autosave_now("timer");
    });

    connect(instrument_store_, &InstrumentStore::list_changed, this, [this]() {
        refresh_instrument_stats();
    });
    connect(instrument_store_, &InstrumentStore::preset_changed, this, [this](int) {
        refresh_instrument_stats();
    });

    connect_project_signals();

    if (!free_edit_mode_) {
        QString init_error;
        if (!initialize_project(create_new_project, project_name, &init_error)) {
            QMessageBox::critical(this, ui("Initialisation projet echouee", "Project init failed"), init_error);
            QTimer::singleShot(0, this, &QWidget::close);
            return;
        }
        project_ready_ = true;
        refresh_project_tab();
        refresh_instrument_stats();
        apply_autosave_settings();
        push_recent_project(project_root_);
        if (create_new_project) {
            show_driver_required_project_notice();
        }
    } else {
        setWindowTitle(ui("NGPC Sound Creator - Edition libre", "NGPC Sound Creator - Free edit"));
        project_tab_->set_project_mode(false, ui("Edition libre", "Free edit"));
        project_tab_->set_project_info(ui("Aucun projet", "No project"), "-");
        project_tab_->set_song_list({}, -1);
        project_tab_->set_sfx_list({});
        project_tab_->set_instrument_stats(instrument_store_->count(), 0, 0);
        tabs_->setCurrentWidget(tracker_tab_);
    }

    connect(tabs_, &QTabWidget::currentChanged, this, [this](int) {
        if (!project_ready_) return;
        if (!project_doc_.autosave.on_tab_change) return;
        autosave_now("tab-change");
    });

    connect(sfx_tab_, &SfxLabTab::save_sfx_to_project_requested, this,
            [this](const QString& name,
                   int tone_on, int tone_ch, int tone_div, int tone_attn, int tone_frames,
                   int tone_sw_on, int tone_sw_end, int tone_sw_step, int tone_sw_speed, int tone_sw_ping,
                   int tone_env_on, int tone_env_step, int tone_env_spd,
                   int noise_on, int noise_rate, int noise_type, int noise_attn,
                   int noise_frames, int noise_burst, int noise_burst_dur,
                   int noise_env_on, int noise_env_step, int noise_env_spd,
                   int tone_adsr_on, int tone_adsr_ar, int tone_adsr_dr, int tone_adsr_sl,
                   int tone_adsr_sr, int tone_adsr_rr,
                   int tone_lfo1_on, int tone_lfo1_wave, int tone_lfo1_hold, int tone_lfo1_rate, int tone_lfo1_depth,
                   int tone_lfo2_on, int tone_lfo2_wave, int tone_lfo2_hold, int tone_lfo2_rate, int tone_lfo2_depth,
                   int tone_lfo_algo) {
        if (!project_ready_) {
            QMessageBox::warning(
                this,
                ui("Mode libre", "Free edit"),
                ui("Aucun projet actif. Ouvre ou cree un projet d'abord.",
                   "No active project. Open or create a project first."));
            return;
        }
        const QString id_base = sanitize_song_id(name);
        QString id = QString("sfx_%1").arg(id_base);
        int suffix = 2;
        while (project_doc_.sfx_index_by_id(id) >= 0) {
            id = QString("sfx_%1_%2").arg(id_base).arg(suffix++);
        }
        ProjectSfxEntry e;
        e.id = id;
        e.name = name;
        e.tone_on = tone_on;
        e.tone_ch = tone_ch;
        e.tone_div = tone_div;
        e.tone_attn = tone_attn;
        e.tone_frames = tone_frames;
        e.tone_sw_on = tone_sw_on;
        e.tone_sw_end = tone_sw_end;
        e.tone_sw_step = tone_sw_step;
        e.tone_sw_speed = tone_sw_speed;
        e.tone_sw_ping = tone_sw_ping;
        e.tone_env_on = tone_env_on;
        e.tone_env_step = tone_env_step;
        e.tone_env_spd = tone_env_spd;
        e.noise_on = noise_on;
        e.noise_rate = noise_rate;
        e.noise_type = noise_type;
        e.noise_attn = noise_attn;
        e.noise_frames = noise_frames;
        e.noise_burst = noise_burst;
        e.noise_burst_dur = noise_burst_dur;
        e.noise_env_on = noise_env_on;
        e.noise_env_step = noise_env_step;
        e.noise_env_spd = noise_env_spd;
        e.tone_adsr_on = tone_adsr_on;
        e.tone_adsr_ar = tone_adsr_ar;
        e.tone_adsr_dr = tone_adsr_dr;
        e.tone_adsr_sl = tone_adsr_sl;
        e.tone_adsr_sr = tone_adsr_sr;
        e.tone_adsr_rr = tone_adsr_rr;
        e.tone_lfo1_on = tone_lfo1_on;
        e.tone_lfo1_wave = tone_lfo1_wave;
        e.tone_lfo1_hold = tone_lfo1_hold;
        e.tone_lfo1_rate = tone_lfo1_rate;
        e.tone_lfo1_depth = tone_lfo1_depth;
        e.tone_lfo2_on = tone_lfo2_on;
        e.tone_lfo2_wave = tone_lfo2_wave;
        e.tone_lfo2_hold = tone_lfo2_hold;
        e.tone_lfo2_rate = tone_lfo2_rate;
        e.tone_lfo2_depth = tone_lfo2_depth;
        e.tone_lfo_algo = tone_lfo_algo;
        project_doc_.sfx.push_back(e);
        save_project_metadata();
        refresh_project_tab();
    });

    connect(sfx_tab_, &SfxLabTab::update_sfx_in_project_requested, this,
            [this](const QString& id,
                   const QString& name,
                   int tone_on, int tone_ch, int tone_div, int tone_attn, int tone_frames,
                   int tone_sw_on, int tone_sw_end, int tone_sw_step, int tone_sw_speed, int tone_sw_ping,
                   int tone_env_on, int tone_env_step, int tone_env_spd,
                   int noise_on, int noise_rate, int noise_type, int noise_attn,
                   int noise_frames, int noise_burst, int noise_burst_dur,
                   int noise_env_on, int noise_env_step, int noise_env_spd,
                   int tone_adsr_on, int tone_adsr_ar, int tone_adsr_dr, int tone_adsr_sl,
                   int tone_adsr_sr, int tone_adsr_rr,
                   int tone_lfo1_on, int tone_lfo1_wave, int tone_lfo1_hold, int tone_lfo1_rate, int tone_lfo1_depth,
                   int tone_lfo2_on, int tone_lfo2_wave, int tone_lfo2_hold, int tone_lfo2_rate, int tone_lfo2_depth,
                   int tone_lfo_algo) {
        if (!project_ready_) {
            QMessageBox::warning(
                this,
                ui("Mode libre", "Free edit"),
                ui("Aucun projet actif. Ouvre ou cree un projet d'abord.",
                   "No active project. Open or create a project first."));
            return;
        }
        const int idx = project_doc_.sfx_index_by_id(id);
        if (idx < 0 || idx >= project_doc_.sfx.size()) {
            QMessageBox::warning(this, "SFX introuvable", "Le SFX a mettre a jour n'existe plus.");
            return;
        }

        auto& e = project_doc_.sfx[idx];
        e.name = name;
        e.tone_on = tone_on;
        e.tone_ch = tone_ch;
        e.tone_div = tone_div;
        e.tone_attn = tone_attn;
        e.tone_frames = tone_frames;
        e.tone_sw_on = tone_sw_on;
        e.tone_sw_end = tone_sw_end;
        e.tone_sw_step = tone_sw_step;
        e.tone_sw_speed = tone_sw_speed;
        e.tone_sw_ping = tone_sw_ping;
        e.tone_env_on = tone_env_on;
        e.tone_env_step = tone_env_step;
        e.tone_env_spd = tone_env_spd;
        e.noise_on = noise_on;
        e.noise_rate = noise_rate;
        e.noise_type = noise_type;
        e.noise_attn = noise_attn;
        e.noise_frames = noise_frames;
        e.noise_burst = noise_burst;
        e.noise_burst_dur = noise_burst_dur;
        e.noise_env_on = noise_env_on;
        e.noise_env_step = noise_env_step;
        e.noise_env_spd = noise_env_spd;
        e.tone_adsr_on = tone_adsr_on;
        e.tone_adsr_ar = tone_adsr_ar;
        e.tone_adsr_dr = tone_adsr_dr;
        e.tone_adsr_sl = tone_adsr_sl;
        e.tone_adsr_sr = tone_adsr_sr;
        e.tone_adsr_rr = tone_adsr_rr;
        e.tone_lfo1_on = tone_lfo1_on;
        e.tone_lfo1_wave = tone_lfo1_wave;
        e.tone_lfo1_hold = tone_lfo1_hold;
        e.tone_lfo1_rate = tone_lfo1_rate;
        e.tone_lfo1_depth = tone_lfo1_depth;
        e.tone_lfo2_on = tone_lfo2_on;
        e.tone_lfo2_wave = tone_lfo2_wave;
        e.tone_lfo2_hold = tone_lfo2_hold;
        e.tone_lfo2_rate = tone_lfo2_rate;
        e.tone_lfo2_depth = tone_lfo2_depth;
        e.tone_lfo_algo = tone_lfo_algo;

        save_project_metadata();
        refresh_project_tab();
    });
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (project_ready_ && project_doc_.autosave.on_close) {
        autosave_now("close");
    }
    persist_startup_settings();
    QMainWindow::closeEvent(event);
}

QString MainWindow::project_file_path() const {
    return QDir(project_root_).filePath("ngpc_project.json");
}

QString MainWindow::instruments_file_path() const {
    return QDir(project_root_).filePath("instruments.json");
}

bool MainWindow::initialize_project(bool create_new_project_flag, const QString& project_name, QString* error) {
    if (project_root_.isEmpty()) {
        if (error) *error = "Project root path is empty";
        return false;
    }

    if (create_new_project_flag) {
        if (!create_new_project(project_name, error)) return false;
    } else {
        if (!load_existing_project(error)) return false;
    }

    int idx = active_song_index();
    if (idx < 0) idx = 0;

    QString load_error;
    if (!load_song_by_index(idx, &load_error)) {
        if (error) *error = load_error;
        return false;
    }

    setWindowTitle(QString("NGPC Sound Creator - %1").arg(project_doc_.name));
    return true;
}

QString MainWindow::ui(const char* fr, const char* en) const {
    return app_lang_pick(language_, fr, en);
}

bool MainWindow::create_new_project(const QString& project_name, QString* error) {
    QDir root(project_root_);
    if (!root.exists()) {
        if (!QDir().mkpath(project_root_)) {
            if (error) *error = QString("Cannot create project folder: %1").arg(project_root_);
            return false;
        }
    }
    if (!root.mkpath("songs") || !root.mkpath("exports")) {
        if (error) *error = QString("Cannot create project subfolders in %1").arg(project_root_);
        return false;
    }

    const QString final_name = project_name.trimmed().isEmpty()
        ? QFileInfo(project_root_).fileName()
        : project_name.trimmed();
    project_doc_.set_defaults(final_name);

    ProjectSongEntry first_song;
    first_song.id = "song_01";
    first_song.name = "Song 1";
    first_song.file = "songs/song_01.ngps";
    project_doc_.songs.push_back(first_song);
    project_doc_.active_song_id = first_song.id;

    QString io_error;
    if (!create_empty_song_file(song_abs_path(0), &io_error)) {
        if (error) *error = io_error;
        return false;
    }
    if (!save_instruments(&io_error)) {
        if (error) *error = io_error;
        return false;
    }
    if (!save_project_metadata(&io_error)) {
        if (error) *error = io_error;
        return false;
    }
    return true;
}

bool MainWindow::load_existing_project(QString* error) {
    QString io_error;
    if (!project_doc_.load_from_file(project_file_path(), &io_error)) {
        if (error) *error = io_error;
        return false;
    }

    const QString instr_path = instruments_file_path();
    if (QFile::exists(instr_path)) {
        if (!instrument_store_->load_json(instr_path)) {
            if (error) *error = QString("Cannot load instruments from %1").arg(instr_path);
            return false;
        }
    } else {
        if (!save_instruments(&io_error)) {
            if (error) *error = io_error;
            return false;
        }
    }

    if (project_doc_.songs.isEmpty()) {
        if (error) *error = "Project contains no songs";
        return false;
    }
    return true;
}

bool MainWindow::switch_to_existing_project(const QString& root, QString* error) {
    if (root.trimmed().isEmpty()) {
        if (error) *error = "Empty project path";
        return false;
    }

    if (project_ready_) {
        autosave_now("switch-project");
    }

    project_root_ = root;
    free_edit_mode_ = false;
    project_ready_ = false;

    QString io_error;
    if (!load_existing_project(&io_error)) {
        if (error) *error = io_error;
        return false;
    }

    int idx = active_song_index();
    if (idx < 0) idx = 0;
    if (!load_song_by_index(idx, &io_error)) {
        if (error) *error = io_error;
        return false;
    }

    project_ready_ = true;
    setWindowTitle(QString("NGPC Sound Creator - %1").arg(project_doc_.name));
    refresh_project_tab();
    refresh_instrument_stats();
    apply_autosave_settings();
    push_recent_project(project_root_);
    return true;
}

bool MainWindow::save_project_as(const QString& new_root, QString* error) {
    if (!project_ready_) {
        if (error) *error = "No active project";
        return false;
    }
    if (new_root.trimmed().isEmpty()) {
        if (error) *error = "Empty destination path";
        return false;
    }
    if (QDir(new_root).exists()) {
        if (error) *error = "Destination folder already exists";
        return false;
    }

    autosave_now("save-as");

    const QString old_root = project_root_;
    project_root_ = new_root;

    QDir root(project_root_);
    if (!QDir().mkpath(project_root_) || !root.mkpath("songs") || !root.mkpath("exports")) {
        project_root_ = old_root;
        if (error) *error = "Cannot create destination project folders";
        return false;
    }

    const QDir old_dir(old_root);
    for (const auto& s : project_doc_.songs) {
        const QString src = old_dir.filePath(s.file);
        const QString dst = root.filePath(s.file);
        const QFileInfo dst_info(dst);
        QDir().mkpath(dst_info.absolutePath());
        if (!QFile::copy(src, dst)) {
            project_root_ = old_root;
            if (error) *error = QString("Copy failed for %1").arg(src);
            return false;
        }
    }

    const QString src_instr = old_dir.filePath("instruments.json");
    const QString dst_instr = root.filePath("instruments.json");
    if (QFile::exists(src_instr) && !QFile::copy(src_instr, dst_instr)) {
        project_root_ = old_root;
        if (error) *error = QString("Copy failed for %1").arg(src_instr);
        return false;
    }

    QString io_error;
    if (!save_project_metadata(&io_error)) {
        project_root_ = old_root;
        if (error) *error = io_error;
        return false;
    }

    push_recent_project(project_root_);
    refresh_project_tab();
    return true;
}

bool MainWindow::create_empty_song_file(const QString& abs_path, QString* error) {
    if (abs_path.isEmpty()) {
        if (error) *error = "Empty song file path";
        return false;
    }

    const QFileInfo fi(abs_path);
    QDir dir(fi.absolutePath());
    if (!dir.exists() && !QDir().mkpath(fi.absolutePath())) {
        if (error) *error = QString("Cannot create song directory: %1").arg(fi.absolutePath());
        return false;
    }

    SongDocument song;
    QSaveFile file(abs_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QString("Cannot write song file: %1").arg(abs_path);
        return false;
    }
    file.write(song.to_json());
    if (!file.commit()) {
        if (error) *error = QString("Cannot commit song file: %1").arg(abs_path);
        return false;
    }
    return true;
}

QString MainWindow::song_abs_path(int index) const {
    if (index < 0 || index >= project_doc_.songs.size()) return QString();
    return QDir(project_root_).filePath(project_doc_.songs[index].file);
}

int MainWindow::active_song_index() const {
    int idx = project_doc_.song_index_by_id(project_doc_.active_song_id);
    if (idx >= 0) return idx;
    if (!project_doc_.songs.isEmpty()) return 0;
    return -1;
}

bool MainWindow::save_active_song(QString* error) {
    const int idx = active_song_index();
    if (idx < 0) {
        if (error) *error = "No active song to save";
        return false;
    }
    return tracker_tab_->save_song_to_path(song_abs_path(idx), error);
}

bool MainWindow::load_song_by_index(int index, QString* error) {
    if (index < 0 || index >= project_doc_.songs.size()) {
        if (error) *error = "Song index out of range";
        return false;
    }

    const QString path = song_abs_path(index);
    if (!QFile::exists(path)) {
        QString create_error;
        if (!create_empty_song_file(path, &create_error)) {
            if (error) *error = create_error;
            return false;
        }
    }

    if (!tracker_tab_->load_song_from_path(path, error)) {
        return false;
    }
    project_doc_.active_song_id = project_doc_.songs[index].id;
    return true;
}

bool MainWindow::save_instruments(QString* error) {
    if (instrument_store_->save_json(instruments_file_path())) {
        return true;
    }
    if (error) *error = QString("Cannot save instruments to %1").arg(instruments_file_path());
    return false;
}

bool MainWindow::save_project_metadata(QString* error) {
    return project_doc_.save_to_file(project_file_path(), error);
}

void MainWindow::refresh_project_tab() {
    if (!project_tab_) return;
    if (!project_ready_) {
        project_tab_->set_project_mode(false, ui("Edition libre", "Free edit"));
        project_tab_->set_project_info(ui("Aucun projet", "No project"), "-");
        project_tab_->set_song_list({}, -1);
        project_tab_->set_sfx_list({});
        return;
    }
    project_tab_->set_project_mode(true, ui("Projet", "Project"));
    project_tab_->set_project_info(project_doc_.name, project_root_);
    project_tab_->set_song_list(project_doc_.songs, active_song_index());
    project_tab_->set_sfx_list(project_doc_.sfx);
    project_tab_->set_autosave_settings(project_doc_.autosave);
}

void MainWindow::refresh_instrument_stats() {
    if (!project_tab_ || !instrument_store_) return;

    const auto factory = ngpc::FactoryInstrumentPresets();
    const int total = instrument_store_->count();
    const int factory_count = static_cast<int>(factory.size());
    const int custom = std::max(0, total - factory_count);

    const int compare_count = std::min(total, factory_count);
    int modified = 0;
    for (int i = 0; i < compare_count; ++i) {
        if (!instrument_preset_equals(instrument_store_->at(i), factory[static_cast<size_t>(i)])) {
            modified++;
        }
    }

    project_tab_->set_instrument_stats(total, custom, modified);
}

void MainWindow::apply_autosave_settings() {
    if (!autosave_timer_) return;
    if (!project_ready_) {
        autosave_timer_->stop();
        return;
    }
    if (project_doc_.autosave.interval_sec <= 0) {
        autosave_timer_->stop();
        return;
    }
    autosave_timer_->setInterval(project_doc_.autosave.interval_sec * 1000);
    autosave_timer_->start();
}

void MainWindow::autosave_now(const QString& reason) {
    Q_UNUSED(reason);
    if (!project_ready_) return;
    save_active_song();
    save_instruments();
    save_project_metadata();
}

void MainWindow::persist_startup_settings() const {
    QSettings settings("NGPC", "SoundCreator");
    settings.setValue("startup/last_mode", project_ready_ ? "project" : "free");
    settings.setValue("startup/last_project_dir", project_ready_ ? project_root_ : QString());
}

QStringList MainWindow::recent_projects() const {
    QSettings settings("NGPC", "SoundCreator");
    return settings.value("startup/recent_projects").toStringList();
}

void MainWindow::push_recent_project(const QString& path) const {
    if (path.trimmed().isEmpty()) return;
    QStringList recents = recent_projects();
    recents.removeAll(path);
    recents.prepend(path);
    while (recents.size() > 10) recents.removeLast();
    QSettings settings("NGPC", "SoundCreator");
    settings.setValue("startup/recent_projects", recents);
    settings.setValue("startup/last_project_dir", path);
}

QString MainWindow::resolve_driver_source_dir() const {
    QStringList candidates;
    const QString app_dir = QCoreApplication::applicationDirPath();
    candidates.push_back(QDir::cleanPath(QDir(app_dir).filePath("../../driver_custom_latest")));
    candidates.push_back(QDir::cleanPath(QDir(app_dir).filePath("../driver_custom_latest")));
    candidates.push_back(QDir::cleanPath(QDir::current().filePath("driver_custom_latest")));
    candidates.push_back(QDir::cleanPath(QDir::current().filePath("NGPC_SOUND_CREATOR/driver_custom_latest")));

    for (const QString& c : candidates) {
        const QString sounds_c = QDir(c).filePath("sounds.c");
        const QString sounds_h = QDir(c).filePath("sounds.h");
        if (QFile::exists(sounds_c) && QFile::exists(sounds_h)) {
            return c;
        }
    }
    return QString();
}

bool MainWindow::export_driver_package_to(const QString& out_dir, QString* error) const {
    const QString src_dir = resolve_driver_source_dir();
    if (src_dir.isEmpty()) {
        if (error) {
            *error = "Dossier technique driver_custom_latest introuvable depuis l'application.";
        }
        return false;
    }

    if (out_dir.trimmed().isEmpty()) {
        if (error) *error = "Dossier de destination vide.";
        return false;
    }

    QDir dst_root(out_dir);
    const QString package_dir = dst_root.filePath("ngpc_audio_driver_pack");
    if (!QDir().mkpath(package_dir)) {
        if (error) *error = QString("Impossible de creer: %1").arg(package_dir);
        return false;
    }

    const QStringList files = {
        "sounds.c",
        "sounds.h",
        "README.md",
        "INTEGRATION_QUICKSTART.md",
        "sounds_game_sfx_template.c",
    };

    for (const QString& name : files) {
        const QString src = QDir(src_dir).filePath(name);
        if (!QFile::exists(src)) {
            if (error) *error = QString("Fichier source manquant: %1").arg(src);
            return false;
        }

        QFile in(src);
        if (!in.open(QIODevice::ReadOnly)) {
            if (error) *error = QString("Impossible de lire: %1").arg(src);
            return false;
        }
        const QByteArray bytes = in.readAll();
        in.close();

        QSaveFile out(QDir(package_dir).filePath(name));
        if (!out.open(QIODevice::WriteOnly)) {
            if (error) *error = QString("Impossible d'ecrire: %1").arg(out.fileName());
            return false;
        }
        if (out.write(bytes) != bytes.size() || !out.commit()) {
            if (error) *error = QString("Impossible de finaliser: %1").arg(out.fileName());
            return false;
        }
    }

    return true;
}

void MainWindow::show_driver_required_project_notice() {
    QMessageBox::information(
        this,
        "Rappel Pack Driver NGPC",
        "Ce projet utilise des fonctions audio avancees qui dependent du Pack Driver NGPC.\n\n"
        "Pour une parite tool -> console, integrez le pack (source technique: driver_custom_latest) "
        "(au minimum sounds.c + sounds.h) dans votre jeu.\n\n"
        "Depuis l'onglet Projet, vous pouvez utiliser le bouton \"Exporter Pack Driver NGPC...\".");
}

void MainWindow::show_driver_required_export_notice() {
    QSettings settings("NGPC", "SoundCreator");
    if (!settings.value("warnings/show_driver_notice_on_export", true).toBool()) {
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle("Rappel export");
    box.setText(
        "Les exports sont prevus pour fonctionner avec le Pack Driver NGPC.");
    box.setInformativeText(
        "Utilisez le Pack Driver NGPC (source technique: driver_custom_latest) pour garder "
        "la parite tool/jeu.\n\nVous pouvez l'exporter via \"Exporter Pack Driver NGPC...\" dans l'onglet Projet.");
    auto* dont_show = new QCheckBox("Ne plus afficher ce rappel", &box);
    box.setCheckBox(dont_show);
    box.exec();

    if (dont_show->isChecked()) {
        settings.setValue("warnings/show_driver_notice_on_export", false);
    }
}

bool MainWindow::write_project_sfx_export(QString* error) const {
    if (!project_ready_) {
        if (error) *error = "No active project";
        return false;
    }
    QDir root(project_root_);
    if (!root.exists("exports") && !root.mkpath("exports")) {
        if (error) *error = "Cannot create exports directory";
        return false;
    }

    const QString out_path = root.filePath("exports/project_sfx.c");
    QSaveFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot write %1").arg(out_path);
        return false;
    }

    QString code;
    code += "/* Generated by NGPC Sound Creator - Project SFX Bank */\n";
    code += "/* index -> name mapping is in comments below */\n\n";
    code += "const unsigned char PROJECT_SFX_COUNT = " + QString::number(project_doc_.sfx.size()) + ";\n\n";

    const auto append_u8 = [&](const QString& name, const auto& getter) {
        code += "const unsigned char " + name + "[] = {";
        for (int i = 0; i < project_doc_.sfx.size(); ++i) {
            if (i) code += ", ";
            code += QString::number(getter(project_doc_.sfx[i]));
        }
        code += "};\n";
    };

    const auto append_u16 = [&](const QString& name, const auto& getter) {
        code += "const unsigned short " + name + "[] = {";
        for (int i = 0; i < project_doc_.sfx.size(); ++i) {
            if (i) code += ", ";
            code += QString::number(getter(project_doc_.sfx[i]));
        }
        code += "};\n";
    };

    const auto append_s16 = [&](const QString& name, const auto& getter) {
        code += "const signed short " + name + "[] = {";
        for (int i = 0; i < project_doc_.sfx.size(); ++i) {
            if (i) code += ", ";
            code += QString::number(getter(project_doc_.sfx[i]));
        }
        code += "};\n";
    };

    append_u8("PROJECT_SFX_TONE_ON", [](const ProjectSfxEntry& e) { return e.tone_on; });
    append_u8("PROJECT_SFX_TONE_CH", [](const ProjectSfxEntry& e) { return e.tone_ch; });
    append_u16("PROJECT_SFX_TONE_DIV", [](const ProjectSfxEntry& e) { return e.tone_div; });
    append_u8("PROJECT_SFX_TONE_ATTN", [](const ProjectSfxEntry& e) { return e.tone_attn; });
    append_u8("PROJECT_SFX_TONE_FRAMES", [](const ProjectSfxEntry& e) { return e.tone_frames; });
    append_u8("PROJECT_SFX_TONE_SW_ON", [](const ProjectSfxEntry& e) { return e.tone_sw_on; });
    append_u16("PROJECT_SFX_TONE_SW_END", [](const ProjectSfxEntry& e) { return e.tone_sw_end; });
    append_s16("PROJECT_SFX_TONE_SW_STEP", [](const ProjectSfxEntry& e) { return e.tone_sw_step; });
    append_u8("PROJECT_SFX_TONE_SW_SPEED", [](const ProjectSfxEntry& e) { return e.tone_sw_speed; });
    append_u8("PROJECT_SFX_TONE_SW_PING", [](const ProjectSfxEntry& e) { return e.tone_sw_ping; });
    append_u8("PROJECT_SFX_TONE_ENV_ON", [](const ProjectSfxEntry& e) { return e.tone_env_on; });
    append_u8("PROJECT_SFX_TONE_ENV_STEP", [](const ProjectSfxEntry& e) { return e.tone_env_step; });
    append_u8("PROJECT_SFX_TONE_ENV_SPD", [](const ProjectSfxEntry& e) { return e.tone_env_spd; });
    append_u8("PROJECT_SFX_NOISE_ON", [](const ProjectSfxEntry& e) { return e.noise_on; });
    append_u8("PROJECT_SFX_NOISE_RATE", [](const ProjectSfxEntry& e) { return e.noise_rate; });
    append_u8("PROJECT_SFX_NOISE_TYPE", [](const ProjectSfxEntry& e) { return e.noise_type; });
    append_u8("PROJECT_SFX_NOISE_ATTN", [](const ProjectSfxEntry& e) { return e.noise_attn; });
    append_u8("PROJECT_SFX_NOISE_FRAMES", [](const ProjectSfxEntry& e) { return e.noise_frames; });
    append_u8("PROJECT_SFX_NOISE_BURST", [](const ProjectSfxEntry& e) { return e.noise_burst; });
    append_u8("PROJECT_SFX_NOISE_BURST_DUR", [](const ProjectSfxEntry& e) { return e.noise_burst_dur; });
    append_u8("PROJECT_SFX_NOISE_ENV_ON", [](const ProjectSfxEntry& e) { return e.noise_env_on; });
    append_u8("PROJECT_SFX_NOISE_ENV_STEP", [](const ProjectSfxEntry& e) { return e.noise_env_step; });
    append_u8("PROJECT_SFX_NOISE_ENV_SPD", [](const ProjectSfxEntry& e) { return e.noise_env_spd; });
    append_u8("PROJECT_SFX_TONE_ADSR_ON", [](const ProjectSfxEntry& e) { return e.tone_adsr_on; });
    append_u8("PROJECT_SFX_TONE_ADSR_AR", [](const ProjectSfxEntry& e) { return e.tone_adsr_ar; });
    append_u8("PROJECT_SFX_TONE_ADSR_DR", [](const ProjectSfxEntry& e) { return e.tone_adsr_dr; });
    append_u8("PROJECT_SFX_TONE_ADSR_SL", [](const ProjectSfxEntry& e) { return e.tone_adsr_sl; });
    append_u8("PROJECT_SFX_TONE_ADSR_SR", [](const ProjectSfxEntry& e) { return e.tone_adsr_sr; });
    append_u8("PROJECT_SFX_TONE_ADSR_RR", [](const ProjectSfxEntry& e) { return e.tone_adsr_rr; });
    append_u8("PROJECT_SFX_TONE_LFO1_ON", [](const ProjectSfxEntry& e) { return e.tone_lfo1_on; });
    append_u8("PROJECT_SFX_TONE_LFO1_WAVE", [](const ProjectSfxEntry& e) { return e.tone_lfo1_wave; });
    append_u8("PROJECT_SFX_TONE_LFO1_HOLD", [](const ProjectSfxEntry& e) { return e.tone_lfo1_hold; });
    append_u8("PROJECT_SFX_TONE_LFO1_RATE", [](const ProjectSfxEntry& e) { return e.tone_lfo1_rate; });
    append_u8("PROJECT_SFX_TONE_LFO1_DEPTH", [](const ProjectSfxEntry& e) { return e.tone_lfo1_depth; });
    append_u8("PROJECT_SFX_TONE_LFO2_ON", [](const ProjectSfxEntry& e) { return e.tone_lfo2_on; });
    append_u8("PROJECT_SFX_TONE_LFO2_WAVE", [](const ProjectSfxEntry& e) { return e.tone_lfo2_wave; });
    append_u8("PROJECT_SFX_TONE_LFO2_HOLD", [](const ProjectSfxEntry& e) { return e.tone_lfo2_hold; });
    append_u8("PROJECT_SFX_TONE_LFO2_RATE", [](const ProjectSfxEntry& e) { return e.tone_lfo2_rate; });
    append_u8("PROJECT_SFX_TONE_LFO2_DEPTH", [](const ProjectSfxEntry& e) { return e.tone_lfo2_depth; });
    append_u8("PROJECT_SFX_TONE_LFO_ALGO", [](const ProjectSfxEntry& e) { return e.tone_lfo_algo; });
    code += "\n";

    for (int i = 0; i < project_doc_.sfx.size(); ++i) {
        code += QString("/* %1: %2 */\n").arg(i).arg(project_doc_.sfx[i].name);
    }

    out.write(code.toUtf8());
    if (!out.commit()) {
        if (error) *error = QString("Cannot commit %1").arg(out_path);
        return false;
    }
    return true;
}

bool MainWindow::write_project_instruments_export(
    QString* error,
    const std::vector<ngpc::InstrumentPreset>* presets) const {
    if (!project_ready_) {
        if (error) *error = "No active project";
        return false;
    }
    QDir root(project_root_);
    if (!root.exists("exports") && !root.mkpath("exports")) {
        if (error) *error = "Cannot create exports directory";
        return false;
    }

    const QString out_path = root.filePath("exports/project_instruments.c");
    QSaveFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot write %1").arg(out_path);
        return false;
    }

    QString code;
    std::vector<ngpc::InstrumentPreset> fallback_bank;
    const std::vector<ngpc::InstrumentPreset>* bank = presets;
    if (!bank) {
        fallback_bank.reserve(static_cast<size_t>(instrument_store_->count()));
        for (int i = 0; i < instrument_store_->count(); ++i) {
            fallback_bank.push_back(instrument_store_->at(i));
        }
        bank = &fallback_bank;
    }

    code += "/* Generated by NGPC Sound Creator - Project Instrument Bank */\n";
    code += "/* Shared across all songs exported from this project */\n\n";
    code += QString::fromStdString(ngpc::InstrumentPresetsToCArray(*bank));

    out.write(code.toUtf8());
    if (!out.commit()) {
        if (error) *error = QString("Cannot commit %1").arg(out_path);
        return false;
    }
    return true;
}

bool MainWindow::build_project_instrument_merge(
    std::array<uint8_t, 128>* out_remap,
    std::vector<ngpc::InstrumentPreset>* out_bank,
    QString* error) {
    if (!out_remap || !out_bank) {
        if (error) *error = "Internal error: null output buffer for instrument merge";
        return false;
    }
    if (!project_ready_) {
        if (error) *error = "No active project";
        return false;
    }
    if (!instrument_store_) {
        if (error) *error = "Instrument store unavailable";
        return false;
    }

    out_remap->fill(0);
    out_bank->clear();

    std::array<bool, 128> used{};
    used.fill(false);

    const QString old_active_id = project_doc_.active_song_id;
    const int old_active_idx = active_song_index();

    auto restore_active = [&]() {
        if (old_active_idx < 0) return;
        project_doc_.active_song_id = old_active_id;
        QString ignored;
        load_song_by_index(old_active_idx, &ignored);
    };

    for (int i = 0; i < project_doc_.songs.size(); ++i) {
        QString io_error;
        if (!load_song_by_index(i, &io_error)) {
            restore_active();
            if (error) {
                *error = QString("Cannot load song '%1' for instrument merge: %2")
                             .arg(project_doc_.songs[i].name)
                             .arg(io_error);
            }
            return false;
        }
        const auto song_used = tracker_tab_->collect_used_instruments();
        for (int id = 0; id < 128; ++id) {
            used[static_cast<size_t>(id)] = used[static_cast<size_t>(id)] || song_used[static_cast<size_t>(id)];
        }
    }
    restore_active();

    bool has_any_used = false;
    for (bool flag : used) {
        if (flag) {
            has_any_used = true;
            break;
        }
    }
    if (!has_any_used && instrument_store_->count() > 0) {
        used[0] = true;
    }

    const int store_count = std::min(instrument_store_->count(), 128);
    for (int old_id = 0; old_id < 128; ++old_id) {
        if (!used[static_cast<size_t>(old_id)]) {
            continue;
        }
        if (old_id >= store_count) {
            (*out_remap)[static_cast<size_t>(old_id)] = 0;
            continue;
        }

        const auto& src = instrument_store_->at(old_id);
        int found = -1;
        for (int merged_id = 0; merged_id < static_cast<int>(out_bank->size()); ++merged_id) {
            if (instrument_def_equals(src.def, (*out_bank)[static_cast<size_t>(merged_id)].def)) {
                found = merged_id;
                break;
            }
        }
        if (found < 0) {
            found = static_cast<int>(out_bank->size());
            out_bank->push_back(src);
        }
        (*out_remap)[static_cast<size_t>(old_id)] = static_cast<uint8_t>(found);
    }

    if (out_bank->empty()) {
        if (instrument_store_->count() > 0) {
            out_bank->push_back(instrument_store_->at(0));
        } else {
            ngpc::InstrumentPreset p;
            p.name = "Default";
            out_bank->push_back(p);
        }
    }

    return true;
}

bool MainWindow::choose_project_song_export_mode(int* out_mode_index, QString* out_mode_label) {
    if (!out_mode_index) {
        return false;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Mode d'export songs");
    box.setText("Choisissez le mode d'export pour les morceaux du projet.");
    box.setInformativeText(
        "Hybride (recommande - novice): streams plus compacts, instruments/effets geres par le driver.\n"
        "Pre-baked (avance/debug): rendu tick-by-tick, plus lourd, utile pour verification fine.");

    QPushButton* hybrid_btn = box.addButton("Hybride (recommande - novice)", QMessageBox::AcceptRole);
    QPushButton* prebaked_btn = box.addButton("Pre-baked (avance/debug)", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(hybrid_btn);

    box.exec();
    if (box.clickedButton() == hybrid_btn) {
        *out_mode_index = 1;
        if (out_mode_label) *out_mode_label = "Hybride";
        return true;
    }
    if (box.clickedButton() == prebaked_btn) {
        *out_mode_index = 0;
        if (out_mode_label) *out_mode_label = "Pre-baked";
        return true;
    }
    return false;
}

QString MainWindow::make_song_symbol_prefix(const QString& song_id) {
    QString stem = song_id.trimmed().toUpper();
    if (stem.isEmpty()) stem = "SONG";
    for (int i = 0; i < stem.size(); ++i) {
        const QChar c = stem.at(i);
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!ok) stem[i] = '_';
    }
    while (stem.contains("__")) stem.replace("__", "_");
    while (stem.startsWith('_')) stem.remove(0, 1);
    while (stem.endsWith('_')) stem.chop(1);
    if (stem.isEmpty()) stem = "SONG";
    if (stem[0].isDigit()) stem.prepend("S_");
    return QString("PROJECT_%1").arg(stem);
}

bool MainWindow::rewrite_song_export_symbols(const QString& out_path,
                                             const QString& symbol_prefix,
                                             QString* error) const {
    QFile in(out_path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot read %1").arg(out_path);
        return false;
    }
    QString text = QString::fromUtf8(in.readAll());
    in.close();

    const QStringList base_symbols = {
        "NOTE_TABLE",
        "BGM_CH0_LOOP", "BGM_CH1_LOOP", "BGM_CH2_LOOP", "BGM_CHN_LOOP", "BGM_MONO_LOOP",
        "BGM_CH0", "BGM_CH1", "BGM_CH2", "BGM_CHN", "BGM_MONO"
    };

    for (const QString& base : base_symbols) {
        const QString renamed = QString("%1_%2").arg(symbol_prefix, base);
        const QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(base)));
        text.replace(re, renamed);
    }

    QSaveFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot rewrite %1").arg(out_path);
        return false;
    }
    out.write(text.toUtf8());
    if (!out.commit()) {
        if (error) *error = QString("Cannot commit rewritten file %1").arg(out_path);
        return false;
    }
    return true;
}

bool MainWindow::write_project_audio_api_export(bool asm_export, QString* error) const {
    if (!project_ready_) {
        if (error) *error = "No active project";
        return false;
    }

    QDir root(project_root_);
    if (!root.exists("exports") && !root.mkpath("exports")) {
        if (error) *error = "Cannot create exports directory";
        return false;
    }

    const QString manifest_path = root.filePath("exports/project_audio_manifest.txt");
    QSaveFile manifest(manifest_path);
    if (!manifest.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot write %1").arg(manifest_path);
        return false;
    }

    QString manifest_text;
    manifest_text += "Generated by NGPC Sound Creator - Project Audio Manifest\n";
    manifest_text += QString("mode=%1\n").arg(asm_export ? "ASM" : "C");
    manifest_text += QString("song_count=%1\n").arg(project_doc_.songs.size());
    manifest_text += "songs:\n";
    const QString song_ext = asm_export ? ".inc" : ".c";
    for (const auto& song : project_doc_.songs) {
        manifest_text += "  - id=" + song.id
                         + " | name=" + song.name
                         + " | file=exports/" + song.id + song_ext
                         + " | symbols=" + make_song_symbol_prefix(song.id) + "_*\n";
    }
    manifest_text += "instruments=exports/project_instruments.c\n";
    manifest_text += "sfx=exports/project_sfx.c\n";
    manifest_text += "notes:\n";
    manifest_text += "  - Song symbols are namespaced to avoid collisions.\n";
    manifest_text += "  - Include project_audio_api.h/.c for one-click C integration.\n";
    manifest_text += "  - Use NgpcProject_BgmStartLoop4ByIndex(i) to auto-switch NOTE_TABLE + streams.\n";
    manifest_text += "  - For ASM export, use this manifest as include/reference list.\n";

    manifest.write(manifest_text.toUtf8());
    if (!manifest.commit()) {
        if (error) *error = QString("Cannot commit %1").arg(manifest_path);
        return false;
    }

    if (asm_export) {
        return true;
    }

    const QString header_path = root.filePath("exports/project_audio_api.h");
    QSaveFile header(header_path);
    if (!header.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot write %1").arg(header_path);
        return false;
    }

    QString h;
    h += "/* Generated by NGPC Sound Creator - Project Audio API */\n";
    h += "#ifndef NGPC_PROJECT_AUDIO_API_H\n";
    h += "#define NGPC_PROJECT_AUDIO_API_H\n\n";
    h += "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    h += "typedef struct NgpcProjectSongRef {\n";
    h += "    const char* id;\n";
    h += "    const char* name;\n";
    h += "    const unsigned char* note_table;\n";
    h += "    const unsigned char* ch0;\n";
    h += "    const unsigned char* ch1;\n";
    h += "    const unsigned char* ch2;\n";
    h += "    const unsigned char* chn;\n";
    h += "    unsigned short loop_ch0;\n";
    h += "    unsigned short loop_ch1;\n";
    h += "    unsigned short loop_ch2;\n";
    h += "    unsigned short loop_chn;\n";
    h += "} NgpcProjectSongRef;\n\n";
    h += "extern const unsigned short NGPC_PROJECT_SONG_COUNT;\n";
    h += "extern const NgpcProjectSongRef NGPC_PROJECT_SONGS[];\n\n";
    h += "const NgpcProjectSongRef* NgpcProject_GetSong(unsigned short index);\n";
    h += "void NgpcProject_BgmStartLoop4ByIndex(unsigned short index);\n\n";
    h += "#ifdef __cplusplus\n}\n#endif\n\n";
    h += "#endif /* NGPC_PROJECT_AUDIO_API_H */\n";

    header.write(h.toUtf8());
    if (!header.commit()) {
        if (error) *error = QString("Cannot commit %1").arg(header_path);
        return false;
    }

    const QString source_path = root.filePath("exports/project_audio_api.c");
    QSaveFile source(source_path);
    if (!source.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Cannot write %1").arg(source_path);
        return false;
    }

    QString c;
    c += "/* Generated by NGPC Sound Creator - Project Audio API */\n";
    c += "#include \"project_audio_api.h\"\n\n";
    c += "/* Driver entry points (sounds.c). */\n";
    c += "extern void Bgm_SetNoteTable(const unsigned char* note_table);\n";
    c += "extern void Bgm_StartLoop4Ex(const unsigned char* stream0, unsigned short loop0,\n";
    c += "                             const unsigned char* stream1, unsigned short loop1,\n";
    c += "                             const unsigned char* stream2, unsigned short loop2,\n";
    c += "                             const unsigned char* streamN, unsigned short loopN);\n\n";
    c += "#if defined(__GNUC__)\n";
    c += "#define NGPC_PROJECT_WEAK __attribute__((weak))\n";
    c += "#else\n";
    c += "#define NGPC_PROJECT_WEAK\n";
    c += "#endif\n";
    c += "/* Link fallback only. Real table is selected per song at runtime. */\n";
    c += "const unsigned char NOTE_TABLE[102] NGPC_PROJECT_WEAK = {\n";
    for (int i = 0; i < 51; ++i) {
        c += "    0x01, 0x00";
        if (i < 50) c += ",";
        c += "\n";
    }
    c += "};\n\n";
    c += "/* Song symbols come from exports/song_*.c (namespaced by export pipeline). */\n";
    for (const auto& song : project_doc_.songs) {
        const QString pfx = make_song_symbol_prefix(song.id);
        c += QString("extern const unsigned char %1_NOTE_TABLE[];\n").arg(pfx);
        c += QString("extern const unsigned char %1_BGM_CH0[];\n").arg(pfx);
        c += QString("extern const unsigned char %1_BGM_CH1[];\n").arg(pfx);
        c += QString("extern const unsigned char %1_BGM_CH2[];\n").arg(pfx);
        c += QString("extern const unsigned char %1_BGM_CHN[];\n").arg(pfx);
        c += QString("extern const unsigned short %1_BGM_CH0_LOOP;\n").arg(pfx);
        c += QString("extern const unsigned short %1_BGM_CH1_LOOP;\n").arg(pfx);
        c += QString("extern const unsigned short %1_BGM_CH2_LOOP;\n").arg(pfx);
        c += QString("extern const unsigned short %1_BGM_CHN_LOOP;\n").arg(pfx);
    }
    c += "\n";
    c += QString("const unsigned short NGPC_PROJECT_SONG_COUNT = %1;\n\n")
             .arg(project_doc_.songs.size());
    c += "const NgpcProjectSongRef NGPC_PROJECT_SONGS[] = {\n";
    for (const auto& song : project_doc_.songs) {
        const QString pfx = make_song_symbol_prefix(song.id);
        c += "    {\n";
        c += QString("        \"%1\",\n").arg(c_string_escape(song.id));
        c += QString("        \"%1\",\n").arg(c_string_escape(song.name));
        c += QString("        %1_NOTE_TABLE,\n").arg(pfx);
        c += QString("        %1_BGM_CH0,\n").arg(pfx);
        c += QString("        %1_BGM_CH1,\n").arg(pfx);
        c += QString("        %1_BGM_CH2,\n").arg(pfx);
        c += QString("        %1_BGM_CHN,\n").arg(pfx);
        c += QString("        %1_BGM_CH0_LOOP,\n").arg(pfx);
        c += QString("        %1_BGM_CH1_LOOP,\n").arg(pfx);
        c += QString("        %1_BGM_CH2_LOOP,\n").arg(pfx);
        c += QString("        %1_BGM_CHN_LOOP\n").arg(pfx);
        c += "    },\n";
    }
    c += "};\n\n";
    c += "const NgpcProjectSongRef* NgpcProject_GetSong(unsigned short index)\n";
    c += "{\n";
    c += "    if (index >= NGPC_PROJECT_SONG_COUNT) return 0;\n";
    c += "    return &NGPC_PROJECT_SONGS[index];\n";
    c += "}\n\n";
    c += "void NgpcProject_BgmStartLoop4ByIndex(unsigned short index)\n";
    c += "{\n";
    c += "    const NgpcProjectSongRef* song = NgpcProject_GetSong(index);\n";
    c += "    if (!song) return;\n";
    c += "    Bgm_SetNoteTable(song->note_table);\n";
    c += "    Bgm_StartLoop4Ex(song->ch0, song->loop_ch0,\n";
    c += "                     song->ch1, song->loop_ch1,\n";
    c += "                     song->ch2, song->loop_ch2,\n";
    c += "                     song->chn, song->loop_chn);\n";
    c += "}\n";

    source.write(c.toUtf8());
    if (!source.commit()) {
        if (error) *error = QString("Cannot commit %1").arg(source_path);
        return false;
    }

    return true;
}

QString MainWindow::sanitize_song_id(const QString& name) {
    QString id = name.trimmed().toLower();
    for (int i = 0; i < id.size(); ++i) {
        const QChar c = id.at(i);
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!ok) id[i] = '_';
    }
    while (id.contains("__")) id.replace("__", "_");
    while (id.startsWith('_')) id.remove(0, 1);
    while (id.endsWith('_')) id.chop(1);
    if (id.isEmpty()) id = "song";
    return id;
}

QString MainWindow::make_unique_song_id(const QString& base_name) const {
    const QString base = sanitize_song_id(base_name);
    QString candidate = base;
    int suffix = 2;

    auto in_use = [&](const QString& id) {
        for (const auto& s : project_doc_.songs) {
            if (s.id == id) return true;
        }
        const QString file_path = QDir(project_root_).filePath(QString("songs/%1.ngps").arg(id));
        return QFile::exists(file_path);
    };

    while (in_use(candidate)) {
        candidate = QString("%1_%2").arg(base).arg(suffix++);
    }
    return candidate;
}

void MainWindow::connect_project_signals() {
    connect(project_tab_, &ProjectTab::create_project_requested, this, [this]() {
        bool ok = false;
        const QString raw_name = QInputDialog::getText(
            this,
            "Nouveau projet",
            "Nom du projet:",
            QLineEdit::Normal,
            "MyGameAudio",
            &ok);
        if (!ok) return;

        const QString project_name = raw_name.trimmed();
        if (project_name.isEmpty()) {
            QMessageBox::warning(this, "Nom invalide", "Le nom du projet ne peut pas etre vide.");
            return;
        }

        const QString parent_dir = QFileDialog::getExistingDirectory(
            this,
            "Choisir le dossier parent du projet",
            project_root_.isEmpty() ? QDir::homePath() : QFileInfo(project_root_).absolutePath());
        if (parent_dir.isEmpty()) return;

        QString folder_name = project_name;
        folder_name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
        folder_name = folder_name.simplified().replace(' ', '_');
        if (folder_name.isEmpty()) {
            QMessageBox::warning(this, "Nom invalide", "Le nom du projet n'est pas exploitable en chemin.");
            return;
        }

        const QString new_root = QDir(parent_dir).filePath(folder_name);
        if (QDir(new_root).exists()) {
            QMessageBox::warning(
                this,
                "Dossier deja existant",
                QString("Le dossier existe deja:\n%1").arg(new_root));
            return;
        }

        QDir root_dir;
        if (!root_dir.mkpath(new_root) ||
            !QDir(new_root).mkpath("songs") ||
            !QDir(new_root).mkpath("exports")) {
            QMessageBox::warning(this, "Creation projet echouee", "Impossible de creer les dossiers du projet.");
            return;
        }

        ProjectDocument doc;
        doc.set_defaults(project_name);
        ProjectSongEntry first_song;
        first_song.id = "song_01";
        first_song.name = "Song 1";
        first_song.file = "songs/song_01.ngps";
        doc.songs.push_back(first_song);
        doc.active_song_id = first_song.id;

        QString error;
        const QString song_abs = QDir(new_root).filePath(first_song.file);
        if (!create_empty_song_file(song_abs, &error)) {
            QMessageBox::warning(this, "Creation projet echouee", error);
            return;
        }
        if (!doc.save_to_file(QDir(new_root).filePath("ngpc_project.json"), &error)) {
            QMessageBox::warning(this, "Creation projet echouee", error);
            return;
        }
        if (!instrument_store_->save_json(QDir(new_root).filePath("instruments.json"))) {
            QMessageBox::warning(this, "Creation projet echouee", "Impossible de sauvegarder instruments.json.");
            return;
        }

        if (!switch_to_existing_project(new_root, &error)) {
            QMessageBox::warning(this, "Ouverture projet echouee", error);
            return;
        }
        show_driver_required_project_notice();
    });

    connect(project_tab_, &ProjectTab::open_project_requested, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            "Ouvrir un projet",
            project_root_.isEmpty() ? QDir::homePath() : project_root_);
        if (dir.isEmpty()) return;

        const QString project_file = QDir(dir).filePath("ngpc_project.json");
        if (!QFile::exists(project_file)) {
            QMessageBox::warning(this, "Projet invalide", QString("Fichier introuvable:\n%1").arg(project_file));
            return;
        }

        QString error;
        if (!switch_to_existing_project(dir, &error)) {
            QMessageBox::warning(this, "Ouverture projet echouee", error);
            return;
        }
    });

    connect(project_tab_, &ProjectTab::save_project_requested, this, [this]() {
        if (!project_ready_) return;
        QString error;
        if (!save_active_song(&error) || !save_instruments(&error) || !save_project_metadata(&error)) {
            QMessageBox::warning(this, "Sauvegarde echouee", error);
        }
    });

    connect(project_tab_, &ProjectTab::save_project_as_requested, this, [this]() {
        if (!project_ready_) return;

        const QString parent_dir = QFileDialog::getExistingDirectory(
            this,
            "Choisir le dossier parent",
            QFileInfo(project_root_).absolutePath());
        if (parent_dir.isEmpty()) return;

        bool ok = false;
        const QString default_name = project_doc_.name.trimmed().isEmpty()
            ? "ProjectCopy"
            : QString("%1_copy").arg(project_doc_.name.trimmed());
        QString folder_name = QInputDialog::getText(
            this,
            "Save Project As",
            "Nom du dossier de destination:",
            QLineEdit::Normal,
            default_name,
            &ok);
        if (!ok) return;

        folder_name = folder_name.trimmed();
        folder_name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
        folder_name = folder_name.simplified().replace(' ', '_');
        if (folder_name.isEmpty()) {
            QMessageBox::warning(this, "Nom invalide", "Le nom du dossier est vide.");
            return;
        }

        const QString new_root = QDir(parent_dir).filePath(folder_name);
        QString error;
        if (!save_project_as(new_root, &error)) {
            QMessageBox::warning(this, "Save As echoue", error);
            return;
        }
        QMessageBox::information(this, "Save As", QString("Projet copie dans:\n%1").arg(new_root));
    });

    connect(project_tab_, &ProjectTab::open_song_requested, this, [this](int index) {
        if (!project_ready_) return;
        autosave_now("song-switch");
        QString error;
        if (!load_song_by_index(index, &error)) {
            QMessageBox::warning(this, "Open song failed", error);
            return;
        }
        save_project_metadata();
        refresh_project_tab();
        tabs_->setCurrentWidget(tracker_tab_);
    });

    connect(project_tab_, &ProjectTab::open_sfx_requested, this, [this](int index) {
        if (!project_ready_) return;
        if (index < 0 || index >= project_doc_.sfx.size()) return;
        sfx_tab_->load_project_sfx(project_doc_.sfx[index]);
        tabs_->setCurrentWidget(sfx_tab_);
    });

    connect(project_tab_, &ProjectTab::create_song_requested, this, [this](const QString& name) {
        if (!project_ready_) return;
        autosave_now("create-song");

        ProjectSongEntry entry;
        entry.id = make_unique_song_id(name);
        entry.name = name;
        entry.file = QString("songs/%1.ngps").arg(entry.id);

        const int old_idx = active_song_index();

        QString error;
        const int new_index = project_doc_.songs.size();
        project_doc_.songs.push_back(entry);
        project_doc_.active_song_id = entry.id;
        if (!create_empty_song_file(song_abs_path(new_index), &error) ||
            !load_song_by_index(new_index, &error) ||
            !save_project_metadata(&error)) {
            project_doc_.songs.removeAt(new_index);
            if (old_idx >= 0 && old_idx < project_doc_.songs.size()) {
                project_doc_.active_song_id = project_doc_.songs[old_idx].id;
                load_song_by_index(old_idx);
            }
            QMessageBox::warning(this, "Create song failed", error);
            refresh_project_tab();
            return;
        }
        refresh_project_tab();
    });

    connect(project_tab_, &ProjectTab::import_midi_song_requested, this,
            [this](const QString& name, const QString& midi_path) {
        if (!project_ready_) return;
        autosave_now("import-midi-song");

        ProjectSongEntry entry;
        entry.id = make_unique_song_id(name);
        entry.name = name;
        entry.file = QString("songs/%1.ngps").arg(entry.id);

        const int old_idx = active_song_index();
        const QString old_active_id = project_doc_.active_song_id;

        QString error;
        const int new_index = project_doc_.songs.size();
        project_doc_.songs.push_back(entry);
        project_doc_.active_song_id = entry.id;

        bool ok = create_empty_song_file(song_abs_path(new_index), &error) &&
                  load_song_by_index(new_index, &error) &&
                  tracker_tab_->import_midi_from_path(midi_path, &error) &&
                  save_active_song(&error) &&
                  save_project_metadata(&error);

        if (!ok) {
            QFile::remove(song_abs_path(new_index));
            project_doc_.songs.removeAt(new_index);
            project_doc_.active_song_id = old_active_id;
            if (old_idx >= 0 && old_idx < project_doc_.songs.size()) {
                load_song_by_index(old_idx);
            }
            QMessageBox::warning(this, "Import MIDI failed", error);
            refresh_project_tab();
            return;
        }
        refresh_project_tab();
    });

    connect(project_tab_, &ProjectTab::rename_song_requested, this, [this](int index, const QString& new_name) {
        if (!project_ready_) return;
        if (index < 0 || index >= project_doc_.songs.size()) return;
        project_doc_.songs[index].name = new_name;
        save_project_metadata();
        refresh_project_tab();
    });

    connect(project_tab_, &ProjectTab::delete_song_requested, this, [this](int index) {
        if (!project_ready_) return;
        if (index < 0 || index >= project_doc_.songs.size()) return;
        if (project_doc_.songs.size() <= 1) {
            QMessageBox::warning(this, "Delete blocked", "At least one song must remain in the project.");
            return;
        }

        const int old_active = active_song_index();
        const bool deleting_active = (index == old_active);
        if (deleting_active) autosave_now("delete-active-song");

        const QString file_path = song_abs_path(index);
        project_doc_.songs.removeAt(index);
        QFile::remove(file_path); // best effort

        if (deleting_active) {
            const int next_idx = std::clamp(index - 1, 0, static_cast<int>(project_doc_.songs.size()) - 1);
            project_doc_.active_song_id = project_doc_.songs[next_idx].id;
            QString error;
            if (!load_song_by_index(next_idx, &error)) {
                QMessageBox::warning(this, "Load song failed", error);
            }
        }

        save_project_metadata();
        refresh_project_tab();
    });

    connect(project_tab_, &ProjectTab::autosave_settings_changed, this, [this](ProjectAutosaveSettings settings) {
        if (!project_ready_) return;
        project_doc_.autosave = settings;
        save_project_metadata();
        apply_autosave_settings();
        refresh_project_tab();
    });

    connect(project_tab_, &ProjectTab::analyze_song_level_requested, this, [this]() {
        if (!project_ready_) return;
        const int peak = tracker_tab_->analyze_song_peak_percent();
        QMessageBox::information(
            this,
            "Analyse niveau song",
            QString("Peak estime (rendu offline): %1%\n\n"
                    "Repere pratique:\n"
                    "- 70-85%: marge confortable\n"
                    "- 85-95%: fort mais generalement propre\n"
                    "- >95%: risque de clip/son agressif")
                .arg(peak));
    });

    connect(project_tab_, &ProjectTab::normalize_song_requested, this, [this]() {
        if (!project_ready_) return;

        bool ok = false;
        const int target = QInputDialog::getInt(
            this,
            "Normaliser song active",
            "Peak cible (%) :",
            85,
            50,
            100,
            1,
            &ok);
        if (!ok) return;

        const int before_peak = tracker_tab_->analyze_song_peak_percent();
        if (before_peak <= 0) {
            QMessageBox::warning(this, "Normalisation song", "Impossible d'analyser le rendu (song vide ?).");
            return;
        }

        const int delta = tracker_tab_->suggest_song_attn_offset_for_target_peak(target, -1, before_peak);
        if (delta == 0) {
            QMessageBox::information(
                this,
                "Normalisation song",
                QString("Song deja proche de la cible.\nPeak actuel: %1% | cible: %2%")
                    .arg(before_peak)
                    .arg(target));
            return;
        }

        const QString direction = (delta > 0)
            ? QString("plus faible")
            : QString("plus fort");
        const auto answer = QMessageBox::question(
            this,
            "Normaliser song active",
            QString("Peak actuel: %1% | cible: %2%\n"
                    "Offset attenuation propose: %3 (%4)\n\n"
                    "Appliquer sur les cellules avec attenuation explicite ?")
                .arg(before_peak)
                .arg(target)
                .arg(delta)
                .arg(direction),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer != QMessageBox::Yes) return;

        const int changed = tracker_tab_->apply_song_attn_offset(delta);
        if (changed <= 0) {
            QMessageBox::information(
                this,
                "Normalisation song",
                "Aucune cellule attenuation explicite a ajuster (beaucoup de notes sont peut-etre en AUTO).");
            return;
        }

        autosave_now("normalize-song");
        const int after_peak = tracker_tab_->analyze_song_peak_percent();
        QMessageBox::information(
            this,
            "Normalisation song",
            QString("Ajustement applique.\n"
                    "Cellules modifiees: %1\n"
                    "Peak avant/apres: %2% -> %3%")
                .arg(changed)
                .arg(before_peak)
                .arg(after_peak));
    });

    connect(project_tab_, &ProjectTab::normalize_sfx_requested, this, [this]() {
        if (!project_ready_) return;
        if (project_doc_.sfx.isEmpty()) {
            QMessageBox::information(this, "Normalisation SFX", "Aucun SFX dans le projet.");
            return;
        }

        bool ok = false;
        const int delta = QInputDialog::getInt(
            this,
            "Normaliser SFX projet",
            "Offset attenuation global (-15 a +15)\n"
            "Positif = moins fort | Negatif = plus fort",
            1,
            -15,
            15,
            1,
            &ok);
        if (!ok || delta == 0) return;

        int changed_entries = 0;
        int changed_fields = 0;
        for (int i = 0; i < project_doc_.sfx.size(); ++i) {
            auto& e = project_doc_.sfx[i];
            bool entry_changed = false;
            if (e.tone_on) {
                const int next = std::clamp(e.tone_attn + delta, 0, 15);
                if (next != e.tone_attn) {
                    e.tone_attn = next;
                    ++changed_fields;
                    entry_changed = true;
                }
            }
            if (e.noise_on) {
                const int next = std::clamp(e.noise_attn + delta, 0, 15);
                if (next != e.noise_attn) {
                    e.noise_attn = next;
                    ++changed_fields;
                    entry_changed = true;
                }
            }
            if (entry_changed) {
                ++changed_entries;
            }
        }

        if (changed_fields == 0) {
            QMessageBox::information(
                this,
                "Normalisation SFX",
                "Aucun changement (deja aux bornes 0..15 avec cet offset).");
            return;
        }

        save_project_metadata();
        refresh_project_tab();
        QMessageBox::information(
            this,
            "Normalisation SFX",
            QString("Ajustement applique sur la banque SFX.\n"
                    "SFX touches: %1 / %2\n"
                    "Champs attenuation modifies: %3")
                .arg(changed_entries)
                .arg(project_doc_.sfx.size())
                .arg(changed_fields));
    });

    connect(project_tab_, &ProjectTab::export_driver_package_requested, this, [this]() {
        const QString base_dir = project_ready_
            ? QDir(project_root_).filePath("exports")
            : QDir::homePath();
        const QString out_dir = QFileDialog::getExistingDirectory(
            this,
            "Exporter le Pack Driver NGPC vers...",
            base_dir);
        if (out_dir.isEmpty()) return;

        QString error;
        if (!export_driver_package_to(out_dir, &error)) {
            QMessageBox::warning(this, "Export driver echoue", error);
            return;
        }
        QMessageBox::information(
            this,
            "Export Pack Driver NGPC",
            QString("Pack Driver NGPC exporte dans:\n%1")
                .arg(QDir(out_dir).filePath("ngpc_audio_driver_pack")));
    });

    connect(project_tab_, &ProjectTab::export_songs_c_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        int mode_index = 1;
        if (!choose_project_song_export_mode(&mode_index, nullptr)) return;
        QString error;
        if (!export_project_songs_only(false, mode_index, nullptr, false, &error)) {
            QMessageBox::warning(this, "Export Songs C failed", error);
            return;
        }
        QMessageBox::information(this, "Export Songs C", "Export songs termine (C).");
    });

    connect(project_tab_, &ProjectTab::export_songs_asm_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        int mode_index = 1;
        if (!choose_project_song_export_mode(&mode_index, nullptr)) return;
        QString error;
        if (!export_project_songs_only(true, mode_index, nullptr, false, &error)) {
            QMessageBox::warning(this, "Export Songs ASM failed", error);
            return;
        }
        QMessageBox::information(this, "Export Songs ASM", "Export songs termine (ASM).");
    });

    connect(project_tab_, &ProjectTab::export_instruments_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        autosave_now("export-instruments");
        QString error;
        if (!write_project_instruments_export(&error)) {
            QMessageBox::warning(this, "Export Instruments failed", error);
            return;
        }
        QMessageBox::information(this, "Export Instruments", "Export instruments termine.");
    });

    connect(project_tab_, &ProjectTab::export_sfx_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        autosave_now("export-sfx");
        QString error;
        if (!write_project_sfx_export(&error)) {
            QMessageBox::warning(this, "Export SFX failed", error);
            return;
        }
        QMessageBox::information(this, "Export SFX", "Export SFX termine.");
    });

    connect(project_tab_, &ProjectTab::export_all_c_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        int mode_index = 1;
        if (!choose_project_song_export_mode(&mode_index, nullptr)) return;
        QString error;
        if (!export_all_project_songs(false, mode_index, &error)) {
            QMessageBox::warning(this, "Export All C failed", error);
            return;
        }
        QMessageBox::information(
            this,
            "Export All C",
            "Export projet termine (C).\n"
            "Fichiers API generes: exports/project_audio_api.h + exports/project_audio_api.c\n"
            "Manifest: exports/project_audio_manifest.txt");
    });

    connect(project_tab_, &ProjectTab::export_all_asm_requested, this, [this]() {
        if (!project_ready_) return;
        show_driver_required_export_notice();
        int mode_index = 1;
        if (!choose_project_song_export_mode(&mode_index, nullptr)) return;
        QString error;
        if (!export_all_project_songs(true, mode_index, &error)) {
            QMessageBox::warning(this, "Export All ASM failed", error);
            return;
        }
        QMessageBox::information(
            this,
            "Export All ASM",
            "Export projet termine (ASM).\n"
            "Manifest: exports/project_audio_manifest.txt");
    });
}

bool MainWindow::export_project_songs_only(bool asm_export,
                                           int export_mode_index,
                                           const std::array<uint8_t, 128>* instrument_remap,
                                           bool namespace_symbols,
                                           QString* error) {
    if (project_doc_.songs.isEmpty()) {
        if (error) *error = "Project has no songs";
        return false;
    }

    autosave_now("export-songs");

    QDir root(project_root_);
    if (!root.exists("exports") && !root.mkpath("exports")) {
        if (error) *error = "Cannot create exports directory";
        return false;
    }

    const QString old_active_id = project_doc_.active_song_id;
    const int old_active_idx = active_song_index();

    const QString ext = asm_export ? ".inc" : ".c";
    for (int i = 0; i < project_doc_.songs.size(); ++i) {
        QString io_error;
        if (!load_song_by_index(i, &io_error)) {
            if (error) *error = QString("Cannot load song '%1': %2")
                                    .arg(project_doc_.songs[i].name)
                                    .arg(io_error);
            if (old_active_idx >= 0) {
                project_doc_.active_song_id = old_active_id;
                load_song_by_index(old_active_idx);
            }
            return false;
        }

        const QString out_file = root.filePath(QString("exports/%1%2").arg(project_doc_.songs[i].id, ext));
        if (!tracker_tab_->export_song_to_path(
                out_file, asm_export, false, &io_error, nullptr, export_mode_index, instrument_remap)) {
            if (error) *error = QString("Cannot export song '%1': %2")
                                    .arg(project_doc_.songs[i].name)
                                    .arg(io_error);
            if (old_active_idx >= 0) {
                project_doc_.active_song_id = old_active_id;
                load_song_by_index(old_active_idx);
            }
            return false;
        }

        if (namespace_symbols) {
            const QString symbol_prefix = make_song_symbol_prefix(project_doc_.songs[i].id);
            if (!rewrite_song_export_symbols(out_file, symbol_prefix, &io_error)) {
                if (error) *error = QString("Cannot rewrite exported symbols for '%1': %2")
                                        .arg(project_doc_.songs[i].name)
                                        .arg(io_error);
                if (old_active_idx >= 0) {
                    project_doc_.active_song_id = old_active_id;
                    load_song_by_index(old_active_idx);
                }
                return false;
            }
        }
    }

    if (old_active_idx >= 0) {
        project_doc_.active_song_id = old_active_id;
        QString restore_error;
        if (!load_song_by_index(old_active_idx, &restore_error)) {
            if (error) *error = QString("Export done but failed to restore active song: %1").arg(restore_error);
            return false;
        }
    }
    save_project_metadata();
    refresh_project_tab();
    return true;
}

bool MainWindow::export_all_project_songs(bool asm_export, int export_mode_index, QString* error) {
    std::array<uint8_t, 128> instrument_remap{};
    std::vector<ngpc::InstrumentPreset> merged_bank;
    const bool use_hybrid_merge = (export_mode_index == 1);

    if (use_hybrid_merge) {
        QString merge_error;
        if (!build_project_instrument_merge(&instrument_remap, &merged_bank, &merge_error)) {
            if (error) *error = QString("Cannot build project instrument merge: %1").arg(merge_error);
            return false;
        }
    }

    const std::array<uint8_t, 128>* remap_ptr = use_hybrid_merge ? &instrument_remap : nullptr;
    if (!export_project_songs_only(asm_export, export_mode_index, remap_ptr, true, error)) {
        return false;
    }

    QString io_error;
    if (!write_project_instruments_export(&io_error, use_hybrid_merge ? &merged_bank : nullptr)) {
        if (error) *error = QString("Cannot export project instruments: %1").arg(io_error);
        return false;
    }

    QString sfx_error;
    if (!write_project_sfx_export(&sfx_error)) {
        if (error) *error = QString("Cannot export project SFX: %1").arg(sfx_error);
        return false;
    }

    QString api_error;
    if (!write_project_audio_api_export(asm_export, &api_error)) {
        if (error) *error = QString("Cannot export project audio API: %1").arg(api_error);
        return false;
    }

    save_project_metadata();
    refresh_project_tab();
    return true;
}
