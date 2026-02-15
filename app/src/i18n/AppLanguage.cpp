#include "i18n/AppLanguage.h"

#include <QSettings>

namespace {
constexpr const char* kOrg = "NGPC";
constexpr const char* kApp = "SoundCreator";
constexpr const char* kLangKey = "ui/language";
} // namespace

AppLanguage app_language_from_code(const QString& code) {
    const QString c = code.trimmed().toLower();
    if (c == "en" || c == "en_us" || c == "en-gb" || c == "english") {
        return AppLanguage::English;
    }
    return AppLanguage::French;
}

QString app_language_to_code(AppLanguage language) {
    return (language == AppLanguage::English) ? "en" : "fr";
}

AppLanguage load_app_language() {
    QSettings settings(kOrg, kApp);
    return app_language_from_code(settings.value(kLangKey, "fr").toString());
}

void save_app_language(AppLanguage language) {
    QSettings settings(kOrg, kApp);
    settings.setValue(kLangKey, app_language_to_code(language));
}

QString app_lang_pick(AppLanguage language, const char* fr, const char* en) {
    return (language == AppLanguage::English)
        ? QString::fromUtf8(en)
        : QString::fromUtf8(fr);
}
