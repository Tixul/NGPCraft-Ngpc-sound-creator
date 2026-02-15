#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include "i18n/AppLanguage.h"

class QComboBox;
class QListWidget;

class ProjectStartDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        None,
        Create,
        OpenExisting,
        FreeEdit
    };

    explicit ProjectStartDialog(AppLanguage language, QWidget* parent = nullptr);

    Mode mode() const { return mode_; }
    QString project_dir() const { return project_dir_; }
    QString project_name() const { return project_name_; }
    AppLanguage selected_language() const;
    bool language_changed() const;

private:
    Mode mode_ = Mode::None;
    QString project_dir_;
    QString project_name_;
    AppLanguage initial_language_ = AppLanguage::French;
    QComboBox* language_combo_ = nullptr;
    QListWidget* recent_list_ = nullptr;
    QStringList recent_projects_;

    QString ui(const char* fr, const char* en) const;

    void choose_create_project();
    void choose_open_project();
    void choose_free_edit();
    void load_recent_projects();
};
