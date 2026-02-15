#pragma once

#include <QWidget>

class QPlainTextEdit;

class DebugTab : public QWidget
{
    Q_OBJECT

public:
    explicit DebugTab(QWidget* parent = nullptr);

private:
    QPlainTextEdit* info_ = nullptr;
};
