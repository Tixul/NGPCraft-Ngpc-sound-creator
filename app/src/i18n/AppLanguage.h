#pragma once

#include <QString>

enum class AppLanguage {
    French,
    English
};

AppLanguage app_language_from_code(const QString& code);
QString app_language_to_code(AppLanguage language);

AppLanguage load_app_language();
void save_app_language(AppLanguage language);

QString app_lang_pick(AppLanguage language, const char* fr, const char* en);
