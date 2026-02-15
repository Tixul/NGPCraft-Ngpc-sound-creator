#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSpinBox;

class FxInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FxInputDialog(uint8_t current_fx, uint8_t current_param,
                           QWidget* parent = nullptr);

    uint8_t fx() const;
    uint8_t fx_param() const;

private:
    QComboBox* fx_combo_ = nullptr;
    QLabel* desc_label_ = nullptr;
    QLabel* param1_label_ = nullptr;
    QLabel* param2_label_ = nullptr;
    QSpinBox* param1_spin_ = nullptr;
    QSpinBox* param2_spin_ = nullptr;
    QLabel* result_label_ = nullptr;

    void on_fx_changed(int index);
    void update_result_preview();

    // Map combo index -> fx code
    static uint8_t combo_to_fx(int index);
    // Map fx code -> combo index
    static int fx_to_combo(uint8_t fx);
};
