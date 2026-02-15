#pragma once

#include <QWidget>
#include "i18n/AppLanguage.h"

class QTextBrowser;
class QListWidget;

class HelpTab : public QWidget
{
    Q_OBJECT

public:
    explicit HelpTab(QWidget* parent = nullptr);

private:
    AppLanguage language_ = AppLanguage::French;
    QListWidget* topic_list_ = nullptr;
    QTextBrowser* content_ = nullptr;

    void load_topic(int index);
    QString topic_english(int index) const;

    static QString topic_intro();
    static QString topic_concepts();
    static QString topic_instruments();
    static QString topic_tracker_basics();
    static QString topic_tracker_keyboard();
    static QString topic_tracker_edit();
    static QString topic_tracker_advanced();
    static QString topic_tracker_effects();
    static QString topic_playback();
    static QString topic_export();
    static QString topic_sfxlab();
    static QString topic_shortcuts();
    static QString topic_faq();
};
