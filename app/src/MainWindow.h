#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <array>
#include <vector>

#include "i18n/AppLanguage.h"
#include "models/ProjectDocument.h"
#include "ngpc/instrument.h"

class EngineHub;
class InstrumentStore;
class QTabWidget;
class QTimer;
class QCloseEvent;
class TrackerTab;
class InstrumentTab;
class ProjectTab;
class SfxLabTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& project_root,
                        bool create_new_project,
                        const QString& project_name,
                        AppLanguage language,
                        bool free_edit_mode,
                        QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    EngineHub* engine_ = nullptr;
    InstrumentStore* instrument_store_ = nullptr;

    QTabWidget* tabs_ = nullptr;
    ProjectTab* project_tab_ = nullptr;
    TrackerTab* tracker_tab_ = nullptr;
    InstrumentTab* instrument_tab_ = nullptr;
    SfxLabTab* sfx_tab_ = nullptr;
    QTimer* autosave_timer_ = nullptr;

    QString project_root_;
    bool project_ready_ = false;
    bool free_edit_mode_ = false;
    AppLanguage language_ = AppLanguage::French;

    ProjectDocument project_doc_;

    QString project_file_path() const;
    QString instruments_file_path() const;

    bool initialize_project(bool create_new_project, const QString& project_name, QString* error);
    bool create_new_project(const QString& project_name, QString* error);
    bool load_existing_project(QString* error);
    bool create_empty_song_file(const QString& abs_path, QString* error);
    bool switch_to_existing_project(const QString& root, QString* error);
    bool save_project_as(const QString& new_root, QString* error);

    QString song_abs_path(int index) const;
    int active_song_index() const;

    bool save_active_song(QString* error = nullptr);
    bool load_song_by_index(int index, QString* error = nullptr);
    bool save_instruments(QString* error = nullptr);
    bool save_project_metadata(QString* error = nullptr);

    void refresh_project_tab();
    void refresh_instrument_stats();
    void apply_autosave_settings();
    void autosave_now(const QString& reason);
    void persist_startup_settings() const;
    void push_recent_project(const QString& path) const;
    QStringList recent_projects() const;
    bool write_project_sfx_export(QString* error = nullptr) const;
    bool write_project_instruments_export(
        QString* error = nullptr,
        const std::vector<ngpc::InstrumentPreset>* presets = nullptr) const;
    bool build_project_instrument_merge(
        std::array<uint8_t, 128>* out_remap,
        std::vector<ngpc::InstrumentPreset>* out_bank,
        QString* error = nullptr);

    void connect_project_signals();
    bool choose_project_song_export_mode(int* out_mode_index, QString* out_mode_label = nullptr);
    bool export_project_songs_only(bool asm_export,
                                   int export_mode_index,
                                   const std::array<uint8_t, 128>* instrument_remap = nullptr,
                                   bool namespace_symbols = false,
                                   QString* error = nullptr);
    bool export_all_project_songs(bool asm_export, int export_mode_index, QString* error = nullptr);
    bool rewrite_song_export_symbols(const QString& out_path,
                                     const QString& symbol_prefix,
                                     QString* error = nullptr) const;
    bool write_project_audio_api_export(bool asm_export, QString* error = nullptr) const;
    static QString make_song_symbol_prefix(const QString& song_id);
    QString ui(const char* fr, const char* en) const;
    QString resolve_driver_source_dir() const;
    bool export_driver_package_to(const QString& out_dir, QString* error = nullptr) const;
    void show_driver_required_project_notice();
    void show_driver_required_export_notice();

    static QString sanitize_song_id(const QString& name);
    QString make_unique_song_id(const QString& base_name) const;
};
