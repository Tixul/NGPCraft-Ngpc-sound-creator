#include "tabs/ProjectTab.h"

#include <algorithm>

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
#include <QPushButton>
#include <QVBoxLayout>

ProjectTab::ProjectTab(AppLanguage language, QWidget* parent)
    : QWidget(parent)
    , language_(language)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* info_box = new QGroupBox(ui("Projet", "Project"), this);
    auto* info_layout = new QVBoxLayout(info_box);
    mode_label_ = new QLabel(ui("Mode: Projet", "Mode: Project"), this);
    project_label_ = new QLabel(ui("Nom: -", "Name: -"), this);
    path_label_ = new QLabel(ui("Chemin: -", "Path: -"), this);
    path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    instrument_label_ = new QLabel(ui("Instruments: 0 total | 0 custom | 0 modifies",
                                      "Instruments: 0 total | 0 custom | 0 modified"), this);
    auto* proj_actions = new QHBoxLayout();
    create_project_btn_ = new QPushButton(ui("Nouveau projet...", "New project..."), this);
    open_project_btn_ = new QPushButton(ui("Ouvrir projet...", "Open project..."), this);
    save_project_btn_ = new QPushButton(ui("Sauver projet", "Save project"), this);
    save_as_project_btn_ = new QPushButton(ui("Sauver projet sous...", "Save project as..."), this);
    proj_actions->addWidget(create_project_btn_);
    proj_actions->addWidget(open_project_btn_);
    proj_actions->addWidget(save_project_btn_);
    proj_actions->addWidget(save_as_project_btn_);
    proj_actions->addStretch(1);
    info_layout->addWidget(mode_label_);
    info_layout->addWidget(project_label_);
    info_layout->addWidget(path_label_);
    info_layout->addWidget(instrument_label_);
    info_layout->addLayout(proj_actions);
    root->addWidget(info_box);

    auto* songs_box = new QGroupBox(ui("Morceaux", "Songs"), this);
    auto* songs_layout = new QVBoxLayout(songs_box);
    song_list_ = new QListWidget(this);
    song_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    songs_layout->addWidget(song_list_, 1);

    auto* actions = new QHBoxLayout();
    open_btn_ = new QPushButton(ui("Ouvrir morceau", "Open song"), this);
    new_btn_ = new QPushButton(ui("Nouveau", "New"), this);
    import_btn_ = new QPushButton(ui("Nouveau depuis MIDI", "New from MIDI"), this);
    rename_btn_ = new QPushButton(ui("Renommer", "Rename"), this);
    delete_btn_ = new QPushButton(ui("Supprimer", "Delete"), this);
    actions->addWidget(open_btn_);
    actions->addWidget(new_btn_);
    actions->addWidget(import_btn_);
    actions->addWidget(rename_btn_);
    actions->addWidget(delete_btn_);
    songs_layout->addLayout(actions);
    root->addWidget(songs_box, 1);

    auto* sfx_box = new QGroupBox(ui("SFX Projet", "Project SFX"), this);
    auto* sfx_layout = new QVBoxLayout(sfx_box);
    sfx_list_ = new QListWidget(this);
    sfx_layout->addWidget(sfx_list_);
    root->addWidget(sfx_box, 1);

    auto* autosave_box = new QGroupBox(ui("Autosave", "Autosave"), this);
    auto* autosave_layout = new QHBoxLayout(autosave_box);
    autosave_layout->addWidget(new QLabel(ui("Intervalle:", "Interval:"), this));
    autosave_combo_ = new QComboBox(this);
    autosave_combo_->addItem(ui("Off", "Off"));
    autosave_combo_->addItem("30s");
    autosave_combo_->addItem("1m");
    autosave_combo_->addItem("2m");
    autosave_combo_->addItem("5m");
    autosave_layout->addWidget(autosave_combo_);
    autosave_tab_change_ = new QCheckBox(ui("Sauver au changement d'onglet", "Save on tab change"), this);
    autosave_on_close_ = new QCheckBox(ui("Sauver a la fermeture", "Save on close"), this);
    autosave_layout->addWidget(autosave_tab_change_);
    autosave_layout->addWidget(autosave_on_close_);
    autosave_layout->addStretch(1);
    root->addWidget(autosave_box);

    auto* export_box = new QGroupBox(ui("Export Projet", "Project Export"), this);
    auto* export_layout = new QVBoxLayout(export_box);
    auto* export_row_1 = new QHBoxLayout();
    auto* export_row_2 = new QHBoxLayout();
    auto* export_row_3 = new QHBoxLayout();
    export_songs_c_btn_ = new QPushButton("Export Songs C", this);
    export_songs_asm_btn_ = new QPushButton("Export Songs ASM", this);
    export_instruments_btn_ = new QPushButton("Export Instruments", this);
    export_sfx_btn_ = new QPushButton("Export SFX", this);
    export_c_btn_ = new QPushButton("Export All C", this);
    export_asm_btn_ = new QPushButton("Export All ASM", this);
    export_driver_btn_ = new QPushButton(ui("Exporter Pack Driver NGPC...", "Export NGPC Driver Pack..."), this);
    analyze_song_btn_ = new QPushButton(ui("Analyser niveau song", "Analyze song level"), this);
    normalize_song_btn_ = new QPushButton(ui("Normaliser song active", "Normalize active song"), this);
    normalize_sfx_btn_ = new QPushButton(ui("Normaliser SFX projet", "Normalize project SFX"), this);
    export_row_1->addWidget(export_songs_c_btn_);
    export_row_1->addWidget(export_songs_asm_btn_);
    export_row_1->addWidget(export_instruments_btn_);
    export_row_1->addWidget(export_sfx_btn_);
    export_row_2->addWidget(export_c_btn_);
    export_row_2->addWidget(export_asm_btn_);
    export_row_2->addWidget(export_driver_btn_);
    export_row_2->addStretch(1);
    export_row_3->addWidget(analyze_song_btn_);
    export_row_3->addWidget(normalize_song_btn_);
    export_row_3->addWidget(normalize_sfx_btn_);
    export_row_3->addStretch(1);
    export_layout->addLayout(export_row_1);
    export_layout->addLayout(export_row_2);
    export_layout->addLayout(export_row_3);
    root->addWidget(export_box);

    connect(song_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        const int idx = song_list_->currentRow();
        if (idx >= 0) emit open_song_requested(idx);
    });
    connect(sfx_list_, &QListWidget::itemDoubleClicked, this, [this]() {
        const int idx = sfx_list_->currentRow();
        if (idx >= 0) emit open_sfx_requested(idx);
    });
    connect(song_list_, &QListWidget::currentRowChanged, this, [this](int) {
        update_button_states();
    });

    connect(open_btn_, &QPushButton::clicked, this, [this]() {
        const int idx = song_list_->currentRow();
        if (idx >= 0) emit open_song_requested(idx);
    });
    connect(open_project_btn_, &QPushButton::clicked, this, [this]() {
        emit open_project_requested();
    });
    connect(create_project_btn_, &QPushButton::clicked, this, [this]() {
        emit create_project_requested();
    });
    connect(save_project_btn_, &QPushButton::clicked, this, [this]() {
        emit save_project_requested();
    });
    connect(save_as_project_btn_, &QPushButton::clicked, this, [this]() {
        emit save_project_as_requested();
    });

    connect(new_btn_, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            ui("Nouveau morceau", "New song"),
            ui("Nom du morceau:", "Song name:"),
            QLineEdit::Normal,
            ui("Nouveau morceau", "New Song"),
            &ok);
        if (!ok) return;
        if (name.trimmed().isEmpty()) return;
        emit create_song_requested(name.trimmed());
    });

    connect(import_btn_, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this,
            ui("Nouveau morceau depuis MIDI", "New song from MIDI"),
            ui("Nom du morceau:", "Song name:"),
            QLineEdit::Normal,
            ui("Morceau importe", "Imported Song"),
            &ok);
        if (!ok) return;
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) return;
        const QString midi_path = QFileDialog::getOpenFileName(
            this,
            ui("Choisir un MIDI", "Choose a MIDI"),
            QString(),
            "MIDI files (*.mid *.midi);;All files (*)");
        if (midi_path.isEmpty()) return;
        emit import_midi_song_requested(trimmed, midi_path);
    });

    connect(rename_btn_, &QPushButton::clicked, this, [this]() {
        const int idx = song_list_->currentRow();
        if (idx < 0) return;
        const QString current_name = song_list_->item(idx)->text();
        bool ok = false;
        const QString renamed = QInputDialog::getText(
            this,
            ui("Renommer morceau", "Rename song"),
            ui("Nouveau nom:", "New name:"),
            QLineEdit::Normal,
            current_name,
            &ok);
        if (!ok) return;
        if (renamed.trimmed().isEmpty()) return;
        emit rename_song_requested(idx, renamed.trimmed());
    });

    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        const int idx = song_list_->currentRow();
        if (idx < 0) return;
        const QString current_name = song_list_->item(idx)->text();
        const auto answer = QMessageBox::question(
            this,
            ui("Supprimer morceau", "Delete song"),
            (language_ == AppLanguage::English)
                ? QString("Delete song '%1'?").arg(current_name)
                : QString("Supprimer le morceau '%1' ?").arg(current_name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        emit delete_song_requested(idx);
    });

    connect(export_songs_c_btn_, &QPushButton::clicked, this, [this]() {
        emit export_songs_c_requested();
    });
    connect(export_songs_asm_btn_, &QPushButton::clicked, this, [this]() {
        emit export_songs_asm_requested();
    });
    connect(export_instruments_btn_, &QPushButton::clicked, this, [this]() {
        emit export_instruments_requested();
    });
    connect(export_sfx_btn_, &QPushButton::clicked, this, [this]() {
        emit export_sfx_requested();
    });
    connect(export_c_btn_, &QPushButton::clicked, this, [this]() {
        emit export_all_c_requested();
    });
    connect(export_asm_btn_, &QPushButton::clicked, this, [this]() {
        emit export_all_asm_requested();
    });
    connect(export_driver_btn_, &QPushButton::clicked, this, [this]() {
        emit export_driver_package_requested();
    });
    connect(analyze_song_btn_, &QPushButton::clicked, this, [this]() {
        emit analyze_song_level_requested();
    });
    connect(normalize_song_btn_, &QPushButton::clicked, this, [this]() {
        emit normalize_song_requested();
    });
    connect(normalize_sfx_btn_, &QPushButton::clicked, this, [this]() {
        emit normalize_sfx_requested();
    });

    connect(autosave_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!updating_ui_) emit_autosave_settings();
    });
    connect(autosave_tab_change_, &QCheckBox::toggled, this, [this](bool) {
        if (!updating_ui_) emit_autosave_settings();
    });
    connect(autosave_on_close_, &QCheckBox::toggled, this, [this](bool) {
        if (!updating_ui_) emit_autosave_settings();
    });

    update_button_states();
}

void ProjectTab::set_project_info(const QString& project_name, const QString& project_path) {
    project_label_->setText(
        (language_ == AppLanguage::English)
            ? QString("Name: %1").arg(project_name)
            : QString("Nom: %1").arg(project_name));
    path_label_->setText(
        (language_ == AppLanguage::English)
            ? QString("Path: %1").arg(project_path)
            : QString("Chemin: %1").arg(project_path));
}

void ProjectTab::set_song_list(const QVector<ProjectSongEntry>& songs, int active_index) {
    song_list_->clear();
    for (const auto& s : songs) {
        song_list_->addItem(s.name);
    }
    if (!songs.isEmpty()) {
        const int clamped = std::clamp(active_index, 0, static_cast<int>(songs.size()) - 1);
        song_list_->setCurrentRow(clamped);
    }
    update_button_states();
}

void ProjectTab::set_sfx_list(const QVector<ProjectSfxEntry>& sfx) {
    sfx_list_->clear();
    for (const auto& e : sfx) {
        QString tone = "Tone:off";
        if (e.tone_on) {
            tone = QString("Tone:ch%1 d%2 a%3 f%4")
                       .arg(e.tone_ch)
                       .arg(e.tone_div)
                       .arg(e.tone_attn)
                       .arg(e.tone_frames);
            if (e.tone_sw_on) {
                tone += QString(" sw(%1,%2,%3,%4)")
                            .arg(e.tone_sw_end)
                            .arg(e.tone_sw_step)
                            .arg(e.tone_sw_speed)
                            .arg(e.tone_sw_ping);
            }
            if (e.tone_env_on) {
                tone += QString(" env(%1,%2)")
                            .arg(e.tone_env_step)
                            .arg(e.tone_env_spd);
            }
        }

        QString noise = "Noise:off";
        if (e.noise_on) {
            noise = QString("Noise:r%1 t%2 a%3 f%4")
                        .arg(e.noise_rate)
                        .arg(e.noise_type)
                        .arg(e.noise_attn)
                        .arg(e.noise_frames);
            if (e.noise_burst) {
                noise += QString(" burst(%1)").arg(e.noise_burst_dur);
            }
            if (e.noise_env_on) {
                noise += QString(" env(%1,%2)")
                             .arg(e.noise_env_step)
                             .arg(e.noise_env_spd);
            }
        }

        sfx_list_->addItem(QString("%1  [%2 | %3]").arg(e.name, tone, noise));
    }
}

void ProjectTab::set_instrument_stats(int total, int custom, int modified) {
    instrument_label_->setText((language_ == AppLanguage::English)
        ? QString("Instruments: %1 total | %2 custom | %3 modified").arg(total).arg(custom).arg(modified)
        : QString("Instruments: %1 total | %2 custom | %3 modifies").arg(total).arg(custom).arg(modified));
}

void ProjectTab::set_project_mode(bool enabled, const QString& mode_label) {
    project_mode_enabled_ = enabled;
    mode_label_->setText(QString("Mode: %1").arg(mode_label));
    save_project_btn_->setEnabled(enabled);
    save_as_project_btn_->setEnabled(enabled);
    open_btn_->setEnabled(enabled && song_list_->currentRow() >= 0);
    new_btn_->setEnabled(enabled);
    import_btn_->setEnabled(enabled);
    rename_btn_->setEnabled(enabled && song_list_->currentRow() >= 0);
    delete_btn_->setEnabled(enabled && song_list_->currentRow() >= 0);
    export_songs_c_btn_->setEnabled(enabled);
    export_songs_asm_btn_->setEnabled(enabled);
    export_instruments_btn_->setEnabled(enabled);
    export_sfx_btn_->setEnabled(enabled);
    export_c_btn_->setEnabled(enabled);
    export_asm_btn_->setEnabled(enabled);
    // Driver package export is useful in both Project mode and Free mode.
    export_driver_btn_->setEnabled(true);
    analyze_song_btn_->setEnabled(enabled);
    normalize_song_btn_->setEnabled(enabled);
    normalize_sfx_btn_->setEnabled(enabled);
    autosave_combo_->setEnabled(enabled);
    autosave_tab_change_->setEnabled(enabled);
    autosave_on_close_->setEnabled(enabled);
}

void ProjectTab::set_autosave_settings(const ProjectAutosaveSettings& settings) {
    updating_ui_ = true;
    autosave_combo_->setCurrentIndex(interval_sec_to_combo(settings.interval_sec));
    autosave_tab_change_->setChecked(settings.on_tab_change);
    autosave_on_close_->setChecked(settings.on_close);
    updating_ui_ = false;
}

int ProjectTab::combo_to_interval_sec(int idx) {
    switch (idx) {
    case 1: return 30;
    case 2: return 60;
    case 3: return 120;
    case 4: return 300;
    default: return 0;
    }
}

int ProjectTab::interval_sec_to_combo(int seconds) {
    if (seconds <= 0) return 0;
    if (seconds <= 30) return 1;
    if (seconds <= 60) return 2;
    if (seconds <= 120) return 3;
    return 4;
}

void ProjectTab::update_button_states() {
    const bool has_sel = song_list_->currentRow() >= 0;
    open_btn_->setEnabled(project_mode_enabled_ && has_sel);
    rename_btn_->setEnabled(project_mode_enabled_ && has_sel);
    delete_btn_->setEnabled(project_mode_enabled_ && has_sel);
}

void ProjectTab::emit_autosave_settings() {
    ProjectAutosaveSettings s;
    s.interval_sec = combo_to_interval_sec(autosave_combo_->currentIndex());
    s.on_tab_change = autosave_tab_change_->isChecked();
    s.on_close = autosave_on_close_->isChecked();
    emit autosave_settings_changed(s);
}

QString ProjectTab::ui(const char* fr, const char* en) const {
    return app_lang_pick(language_, fr, en);
}
