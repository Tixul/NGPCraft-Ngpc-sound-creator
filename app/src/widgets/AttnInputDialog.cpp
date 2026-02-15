#include "widgets/AttnInputDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

AttnInputDialog::AttnInputDialog(uint8_t current_attn, QWidget* parent)
    : QDialog(parent), result_attn_(current_attn)
{
    setWindowTitle(QString::fromUtf8("Regler le volume"));
    setMinimumWidth(380);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    auto* desc = new QLabel(QString::fromUtf8(
        "L'attenuation controle le volume de la note.\n"
        "0 = volume maximum (le plus fort)\n"
        "15 = silence total\n"
        "Automatique = le volume est gere par l'instrument (enveloppe)."), this);
    desc->setWordWrap(true);
    desc->setStyleSheet(
        "QLabel { background: #1e1e2a; color: #aabbcc; padding: 6px 10px;"
        " border: 1px solid #333; border-radius: 4px; font-size: 11px; }");
    root->addWidget(desc);

    // --- Auto checkbox ---
    auto_check_ = new QCheckBox(
        QString::fromUtf8("Automatique (gere par l'instrument)"), this);
    auto_check_->setChecked(current_attn == 0xFF);
    root->addWidget(auto_check_);

    // --- Slider + spin ---
    auto* vol_row = new QHBoxLayout();
    vol_row->addWidget(new QLabel(QString::fromUtf8("Fort"), this));

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setRange(0, 15);
    slider_->setValue((current_attn <= 15) ? current_attn : 0);
    slider_->setTickPosition(QSlider::TicksBelow);
    slider_->setTickInterval(1);
    vol_row->addWidget(slider_, 1);

    vol_row->addWidget(new QLabel(QString::fromUtf8("Silence"), this));

    spin_ = new QSpinBox(this);
    spin_->setRange(0, 15);
    spin_->setValue((current_attn <= 15) ? current_attn : 0);
    spin_->setFixedWidth(50);
    vol_row->addWidget(spin_);
    root->addLayout(vol_row);

    // --- Visual bar ---
    bar_label_ = new QLabel(this);
    bar_label_->setAlignment(Qt::AlignCenter);
    bar_label_->setFixedHeight(28);
    bar_label_->setStyleSheet(
        "QLabel { font-family: 'Consolas', monospace; font-size: 13px;"
        " font-weight: bold; padding: 2px; }");
    root->addWidget(bar_label_);

    // --- Description ---
    desc_label_ = new QLabel(this);
    desc_label_->setAlignment(Qt::AlignCenter);
    desc_label_->setStyleSheet("QLabel { color: #888; font-size: 11px; }");
    root->addWidget(desc_label_);

    // --- Enable/disable slider based on auto checkbox ---
    auto update_enabled = [this]() {
        bool manual = !auto_check_->isChecked();
        slider_->setEnabled(manual);
        spin_->setEnabled(manual);
        update_preview();
    };

    connect(auto_check_, &QCheckBox::toggled, this, [update_enabled](bool) {
        update_enabled();
    });
    connect(slider_, &QSlider::valueChanged, this, [this](int val) {
        spin_->blockSignals(true);
        spin_->setValue(val);
        spin_->blockSignals(false);
        update_preview();
    });
    connect(spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        slider_->blockSignals(true);
        slider_->setValue(val);
        slider_->blockSignals(false);
        update_preview();
    });

    // --- OK / Cancel ---
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (auto_check_->isChecked()) {
            result_attn_ = 0xFF;
        } else {
            result_attn_ = static_cast<uint8_t>(spin_->value());
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    update_enabled();
    update_preview();
}

void AttnInputDialog::update_preview() {
    if (auto_check_->isChecked()) {
        bar_label_->setText(QString::fromUtf8("Volume : AUTO (gere par l'instrument)"));
        bar_label_->setStyleSheet(
            "QLabel { color: #aaaaaa; font-family: 'Consolas', monospace;"
            " font-size: 13px; font-weight: bold; padding: 2px; }");
        desc_label_->setText("");
        return;
    }

    int attn = spin_->value();
    int vol_pct = static_cast<int>((1.0 - attn / 15.0) * 100.0 + 0.5);

    // Visual bar
    int bars = 15 - attn;
    QString bar_str;
    for (int i = 0; i < bars; ++i) bar_str += QString::fromUtf8("\u2588");
    for (int i = bars; i < 15; ++i) bar_str += QString::fromUtf8("\u2591");

    // Color based on volume
    QString color;
    if (attn <= 3) color = "#55cc55";       // green = loud
    else if (attn <= 7) color = "#cccc55";  // yellow = medium
    else if (attn <= 11) color = "#cc8833"; // orange = quiet
    else color = "#cc5555";                 // red = near silent

    bar_label_->setStyleSheet(
        QString("QLabel { color: %1; font-family: 'Consolas', monospace;"
                " font-size: 13px; font-weight: bold; padding: 2px; }").arg(color));
    bar_label_->setText(QString("%1  %2%").arg(bar_str).arg(vol_pct));

    // Description
    if (attn == 0) desc_label_->setText(QString::fromUtf8("Volume maximum"));
    else if (attn <= 3) desc_label_->setText(QString::fromUtf8("Fort"));
    else if (attn <= 7) desc_label_->setText(QString::fromUtf8("Moyen"));
    else if (attn <= 11) desc_label_->setText(QString::fromUtf8("Faible"));
    else if (attn <= 14) desc_label_->setText(QString::fromUtf8("Tres faible"));
    else desc_label_->setText(QString::fromUtf8("Silence total"));
}
