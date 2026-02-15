#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QSlider;
class QSpinBox;

class AttnInputDialog : public QDialog
{
    Q_OBJECT

public:
    // current_attn: 0-15 or 0xFF (auto/instrument-managed)
    explicit AttnInputDialog(uint8_t current_attn, QWidget* parent = nullptr);

    // Result: 0-15 or 0xFF
    uint8_t attn() const { return result_attn_; }

private:
    uint8_t result_attn_ = 0xFF;

    QSlider* slider_ = nullptr;
    QSpinBox* spin_ = nullptr;
    QCheckBox* auto_check_ = nullptr;
    QLabel* bar_label_ = nullptr;
    QLabel* desc_label_ = nullptr;

    void update_preview();
};
