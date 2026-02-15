#pragma once

#include <QWidget>
#include <vector>
#include <cstdint>

class EnvelopeCurveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EnvelopeCurveWidget(QWidget* parent = nullptr);

    void set_curve(const std::vector<int8_t>& steps);
    void set_base_attn(uint8_t attn);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<int8_t> steps_;
    uint8_t base_attn_ = 2;
};
