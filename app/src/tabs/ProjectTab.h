#pragma once

#include <QWidget>
#include <QVector>

#include "i18n/AppLanguage.h"
#include "models/ProjectDocument.h"

class QLabel;
class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;

class ProjectTab : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectTab(AppLanguage language, QWidget* parent = nullptr);

    void set_project_info(const QString& project_name, const QString& project_path);
    void set_song_list(const QVector<ProjectSongEntry>& songs, int active_index);
    void set_sfx_list(const QVector<ProjectSfxEntry>& sfx);
    void set_instrument_stats(int total, int custom, int modified);
    void set_autosave_settings(const ProjectAutosaveSettings& settings);
    void set_project_mode(bool enabled, const QString& mode_label);

signals:
    void create_project_requested();
    void open_project_requested();
    void save_project_requested();
    void save_project_as_requested();
    void open_song_requested(int index);
    void open_sfx_requested(int index);
    void create_song_requested(const QString& name);
    void import_midi_song_requested(const QString& name, const QString& midi_path);
    void rename_song_requested(int index, const QString& new_name);
    void delete_song_requested(int index);
    void export_songs_c_requested();
    void export_songs_asm_requested();
    void export_instruments_requested();
    void export_sfx_requested();
    void export_all_c_requested();
    void export_all_asm_requested();
    void export_driver_package_requested();
    void analyze_song_level_requested();
    void normalize_song_requested();
    void normalize_sfx_requested();
    void autosave_settings_changed(ProjectAutosaveSettings settings);

private:
    QLabel* project_label_ = nullptr;
    QLabel* path_label_ = nullptr;
    QLabel* instrument_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    QListWidget* song_list_ = nullptr;
    QListWidget* sfx_list_ = nullptr;
    QPushButton* create_project_btn_ = nullptr;
    QPushButton* open_project_btn_ = nullptr;
    QPushButton* save_project_btn_ = nullptr;
    QPushButton* save_as_project_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* export_songs_c_btn_ = nullptr;
    QPushButton* export_songs_asm_btn_ = nullptr;
    QPushButton* export_instruments_btn_ = nullptr;
    QPushButton* export_sfx_btn_ = nullptr;
    QPushButton* export_c_btn_ = nullptr;
    QPushButton* export_asm_btn_ = nullptr;
    QPushButton* export_driver_btn_ = nullptr;
    QPushButton* analyze_song_btn_ = nullptr;
    QPushButton* normalize_song_btn_ = nullptr;
    QPushButton* normalize_sfx_btn_ = nullptr;

    QComboBox* autosave_combo_ = nullptr;
    QCheckBox* autosave_tab_change_ = nullptr;
    QCheckBox* autosave_on_close_ = nullptr;

    bool updating_ui_ = false;
    bool project_mode_enabled_ = true;
    AppLanguage language_ = AppLanguage::French;

    QString ui(const char* fr, const char* en) const;

    static int combo_to_interval_sec(int idx);
    static int interval_sec_to_combo(int seconds);

    void update_button_states();
    void emit_autosave_settings();
};
