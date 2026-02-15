#include <QApplication>
#include <QDialog>
#include <QCoreApplication>
#include <QProcess>

#include "MainWindow.h"
#include "i18n/AppLanguage.h"
#include "widgets/ProjectStartDialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const AppLanguage current_language = load_app_language();
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
