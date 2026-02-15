#include "widgets/EnvelopeCurveWidget.h"

#include <QPainter>
#include <QPaintEvent>

EnvelopeCurveWidget::EnvelopeCurveWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 60);
}

void EnvelopeCurveWidget::set_curve(const std::vector<int8_t>& steps) {
    steps_ = steps;
    update();
}

void EnvelopeCurveWidget::set_base_attn(uint8_t attn) {
    base_attn_ = attn;
    update();
}

QSize EnvelopeCurveWidget::minimumSizeHint() const {
    return {120, 60};
}

QSize EnvelopeCurveWidget::sizeHint() const {
    return {240, 80};
}

void EnvelopeCurveWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int margin = 4;
    const int draw_w = w - margin * 2;
    const int draw_h = h - margin * 2;

    // Background
    p.fillRect(rect(), QColor(30, 30, 30));

    // Grid lines (attn levels 0, 5, 10, 15)
    p.setPen(QPen(QColor(60, 60, 60), 1));
    for (int a = 0; a <= 15; a += 5) {
        const int y = margin + draw_h - (draw_h * (15 - a) / 15);
        p.drawLine(margin, y, margin + draw_w, y);
    }

    // Base attenuation line
    {
        const int base_y = margin + draw_h - (draw_h * (15 - base_attn_) / 15);
        p.setPen(QPen(QColor(80, 120, 80), 1, Qt::DashLine));
        p.drawLine(margin, base_y, margin + draw_w, base_y);
    }

    if (steps_.empty()) {
        return;
    }

    // Build absolute attenuation values from cumulative steps
    const int n = static_cast<int>(steps_.size());
    std::vector<int> values(static_cast<size_t>(n));
    int acc = base_attn_;
    for (int i = 0; i < n; ++i) {
        acc += steps_[static_cast<size_t>(i)];
        if (acc < 0) acc = 0;
        if (acc > 15) acc = 15;
        values[static_cast<size_t>(i)] = acc;
    }

    // Draw polyline
    p.setPen(QPen(QColor(100, 200, 100), 2));
    auto to_point = [&](int idx, int val) -> QPointF {
        const double x = margin + (draw_w * idx) / std::max(n - 1, 1);
        const double y = margin + draw_h - (draw_h * (15 - val)) / 15.0;
        return {x, y};
    };

    for (int i = 0; i + 1 < n; ++i) {
        const QPointF a = to_point(i, values[static_cast<size_t>(i)]);
        const QPointF b = to_point(i + 1, values[static_cast<size_t>(i + 1)]);
        p.drawLine(a, b);
    }

    // Draw points
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(150, 255, 150));
    for (int i = 0; i < n; ++i) {
        const QPointF pt = to_point(i, values[static_cast<size_t>(i)]);
        p.drawEllipse(pt, 3.0, 3.0);
    }
}
