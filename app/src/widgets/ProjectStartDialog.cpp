#include "widgets/ProjectStartDialog.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QString sanitize_name_for_path(const QString& raw) {
    QString s = raw.trimmed();
    s.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    s = s.simplified().replace(' ', '_');
    return s;
}
}

ProjectStartDialog::ProjectStartDialog(AppLanguage language, QWidget* parent)
    : QDialog(parent)
    , initial_language_(language)
{
    setWindowTitle(ui("NGPC Sound Creator - Projet", "NGPC Sound Creator - Project"));
    setModal(true);
    setMinimumWidth(440);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* language_row = new QHBoxLayout();
    auto* language_label = new QLabel(ui("Langue:", "Language:"), this);
    language_combo_ = new QComboBox(this);
    language_combo_->addItem("Francais", QStringLiteral("fr"));
    language_combo_->addItem("English", QStringLiteral("en"));
    language_combo_->setCurrentIndex(initial_language_ == AppLanguage::English ? 1 : 0);
    language_row->addWidget(language_label);
    language_row->addWidget(language_combo_, 1);
    root->addLayout(language_row);

    auto* title = new QLabel(
        ui(
            "Choisissez une action:\n"
            "- Creer un nouveau projet\n"
            "- Ouvrir un projet existant",
            "Choose an action:\n"
            "- Create a new project\n"
            "- Open an existing project"),
        this);
    title->setWordWrap(true);
    root->addWidget(title);

    auto* create_btn = new QPushButton(ui("Nouveau Projet...", "New Project..."), this);
    auto* open_btn = new QPushButton(ui("Ouvrir Projet...", "Open Project..."), this);
    auto* free_btn = new QPushButton(ui("Edition libre (sans projet)", "Free Edit (no project)"), this);
    auto* cancel_btn = new QPushButton(ui("Annuler", "Cancel"), this);
    cancel_btn->setAutoDefault(false);

    recent_list_ = new QListWidget(this);
    recent_list_->setMinimumHeight(110);
    recent_list_->setToolTip(ui(
        "Double-clique pour ouvrir un projet recent",
        "Double-click to open a recent project"));
    load_recent_projects();

    root->addWidget(create_btn);
    root->addWidget(open_btn);
    root->addWidget(free_btn);
    root->addWidget(new QLabel(ui("Projets recents:", "Recent projects:"), this));
    root->addWidget(recent_list_);
    root->addWidget(cancel_btn);

    connect(create_btn, &QPushButton::clicked, this, [this]() { choose_create_project(); });
    connect(open_btn, &QPushButton::clicked, this, [this]() { choose_open_project(); });
    connect(free_btn, &QPushButton::clicked, this, [this]() { choose_free_edit(); });
    connect(cancel_btn, &QPushButton::clicked, this, [this]() { reject(); });
    connect(recent_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;
        if (!QFile::exists(QDir(path).filePath("ngpc_project.json"))) {
            QMessageBox::warning(
                this,
                ui("Projet introuvable", "Project not found"),
                ui("Le projet selectionne n'existe plus.", "The selected project no longer exists."));
            return;
        }
        mode_ = Mode::OpenExisting;
        project_dir_ = path;
        project_name_.clear();
        accept();
    });
}

QString ProjectStartDialog::ui(const char* fr, const char* en) const {
    const AppLanguage lang = language_combo_ ? selected_language() : initial_language_;
    return app_lang_pick(lang, fr, en);
}

AppLanguage ProjectStartDialog::selected_language() const {
    if (!language_combo_) return initial_language_;
    return app_language_from_code(language_combo_->currentData().toString());
}

bool ProjectStartDialog::language_changed() const {
    return selected_language() != initial_language_;
}

void ProjectStartDialog::choose_create_project() {
    bool ok = false;
    const QString raw_name = QInputDialog::getText(
        this,
        ui("Nouveau projet", "New project"),
        ui("Nom du projet:", "Project name:"),
        QLineEdit::Normal,
        "MyGameAudio",
        &ok);
    if (!ok) return;

    const QString project_name = raw_name.trimmed();
    if (project_name.isEmpty()) {
        QMessageBox::warning(
            this,
            ui("Nom invalide", "Invalid name"),
            ui("Le nom du projet ne peut pas etre vide.", "Project name cannot be empty."));
        return;
    }

    const QString parent_dir = QFileDialog::getExistingDirectory(
        this, ui("Choisir le dossier parent du projet", "Choose project parent folder"));
    if (parent_dir.isEmpty()) return;

    const QString folder_name = sanitize_name_for_path(project_name);
    if (folder_name.isEmpty()) {
        QMessageBox::warning(
            this,
            ui("Nom invalide", "Invalid name"),
            ui("Le nom du projet n'est pas exploitable en chemin.",
               "Project name cannot be used as a folder path."));
        return;
    }

    const QString full_dir = QDir(parent_dir).filePath(folder_name);
    if (QDir(full_dir).exists()) {
        QMessageBox::warning(
            this,
            ui("Dossier deja existant", "Folder already exists"),
            (selected_language() == AppLanguage::English)
                ? QString("Folder already exists:\n%1\nChoose another name or parent folder.").arg(full_dir)
                : QString("Le dossier existe deja:\n%1\nChoisis un autre nom ou dossier parent.").arg(full_dir));
        return;
    }

    mode_ = Mode::Create;
    project_name_ = project_name;
    project_dir_ = full_dir;
    accept();
}

void ProjectStartDialog::choose_open_project() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, ui("Ouvrir un projet", "Open project"));
    if (dir.isEmpty()) return;

    const QString project_file = QDir(dir).filePath("ngpc_project.json");
    if (!QFile::exists(project_file)) {
        QMessageBox::warning(
            this,
            ui("Projet invalide", "Invalid project"),
            (selected_language() == AppLanguage::English)
                ? QString("File not found:\n%1").arg(project_file)
                : QString("Fichier introuvable:\n%1").arg(project_file));
        return;
    }

    mode_ = Mode::OpenExisting;
    project_dir_ = dir;
    project_name_.clear();
    accept();
}

void ProjectStartDialog::choose_free_edit() {
    mode_ = Mode::FreeEdit;
    project_dir_.clear();
    project_name_.clear();
    accept();
}

void ProjectStartDialog::load_recent_projects() {
    recent_projects_.clear();
    if (!recent_list_) return;
    recent_list_->clear();

    QSettings settings("NGPC", "SoundCreator");
    const QStringList recent = settings.value("startup/recent_projects").toStringList();
    for (const QString& p : recent) {
        const QString path = p.trimmed();
        if (path.isEmpty()) continue;
        if (!QFile::exists(QDir(path).filePath("ngpc_project.json"))) continue;
        recent_projects_.push_back(path);
        auto* item = new QListWidgetItem(path, recent_list_);
        item->setData(Qt::UserRole, path);
    }
}
