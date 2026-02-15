#include "widgets/InstrumentInputDialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

InstrumentInputDialog::InstrumentInputDialog(uint8_t current_inst,
                                             const QStringList& names,
                                             QWidget* parent)
    : QDialog(parent), result_inst_(current_inst)
{
    setWindowTitle(QString::fromUtf8("Choisir un instrument"));
    setMinimumWidth(340);
    setMinimumHeight(320);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    auto* desc = new QLabel(QString::fromUtf8(
        "Selectionnez l'instrument a utiliser pour cette note.\n"
        "Chaque instrument a son propre son (enveloppe, vibrato, etc.).\n"
        "Editez les instruments dans l'onglet Instruments."), this);
    desc->setWordWrap(true);
    desc->setStyleSheet(
        "QLabel { background: #1e1e2a; color: #aabbcc; padding: 6px 10px;"
        " border: 1px solid #333; border-radius: 4px; font-size: 11px; }");
    root->addWidget(desc);

    list_ = new QListWidget(this);
    list_->setStyleSheet(
        "QListWidget { font-family: 'Consolas', monospace; font-size: 13px; }");

    const int item_count = std::max(1, static_cast<int>(names.size()));
    for (int i = 0; i < item_count; ++i) {
        QString label = QString("%1").arg(i, 2, 16, QLatin1Char('0')).toUpper();
        if (i < names.size() && !names[i].isEmpty()) {
            label += QString::fromUtf8(" \u2014 ") + names[i];
        }
        list_->addItem(label);
    }

    list_->setCurrentRow(std::min<int>(current_inst, item_count - 1));
    root->addWidget(list_, 1);

    // --- OK / Cancel ---
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        int row = list_->currentRow();
        if (row >= 0 && row < list_->count()) {
            result_inst_ = static_cast<uint8_t>(row);
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Double-click to select and confirm
    connect(list_, &QListWidget::itemDoubleClicked, this, [this]() {
        int row = list_->currentRow();
        if (row >= 0 && row < list_->count()) {
            result_inst_ = static_cast<uint8_t>(row);
        }
        accept();
    });
}
