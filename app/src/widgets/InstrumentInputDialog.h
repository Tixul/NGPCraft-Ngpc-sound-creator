#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

class InstrumentInputDialog : public QDialog
{
    Q_OBJECT

public:
    // names: list of instrument names (typically up to 128)
    explicit InstrumentInputDialog(uint8_t current_inst,
                                   const QStringList& names,
                                   QWidget* parent = nullptr);

    uint8_t instrument() const { return result_inst_; }

private:
    uint8_t result_inst_ = 0;
    QListWidget* list_ = nullptr;
};
