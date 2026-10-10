#include <QApplication>
#include <QDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include "MainWindow.h"
#include "i18n/AppLanguage.h"
#include "widgets/ProjectStartDialog.h"

namespace {

// "--project <path>": open a project without the start dialog. <path> is the
// project folder, or a song inside it (used by NgpCraft Studio's "Open in
// Sound Creator"). Returns the folder holding ngpc_project.json, or empty.
QString project_from_arguments(const QStringList& args)
{
    const int i = args.indexOf("--project");
    if (i < 0 || i + 1 >= args.size()) return {};
    const QFileInfo target(args.at(i + 1));
    QDir dir = target.isDir() ? QDir(target.absoluteFilePath()) : target.absoluteDir();
    for (int depth = 0; depth < 2; ++depth) {
        if (QFile::exists(dir.filePath("ngpc_project.json"))) return dir.absolutePath();
        if (!dir.cdUp()) break;
    }
    return {};
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const AppLanguage current_language = load_app_language();

    const QString direct_project = project_from_arguments(QCoreApplication::arguments());
    if (!direct_project.isEmpty()) {
        MainWindow window(direct_project, false, QString(), current_language, false);
        window.show();
        return app.exec();
    }

    ProjectStartDialog start(current_language);
    if (start.exec() != QDialog::Accepted) {
        return 0;
    }

    const AppLanguage chosen_language = start.selected_language();
    if (start.language_changed()) {
        save_app_language(chosen_language);
        QStringList args = QCoreApplication::arguments();
        if (!args.isEmpty()) args.removeFirst();
        QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
        return 0;
    }

    const bool create_new = (start.mode() == ProjectStartDialog::Mode::Create);
    const bool free_edit = (start.mode() == ProjectStartDialog::Mode::FreeEdit);
    MainWindow window(start.project_dir(), create_new, start.project_name(), chosen_language, free_edit);
    window.show();

    return app.exec();
}
