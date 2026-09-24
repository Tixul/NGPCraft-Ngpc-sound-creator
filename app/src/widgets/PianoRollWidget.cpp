#include "widgets/PianoRollWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cstdlib>

#include "audio/TrackerPlaybackEngine.h"
#include "models/TrackerDocument.h"
#include "widgets/TrackerGridWidget.h"

namespace {

const char* kChannelNames[4] = {"Tone 0", "Tone 1", "Tone 2", "Noise"};

bool is_black_key(uint8_t note) {
    switch ((note - 1) % 12) {
    case 1: case 3: case 6: case 8: case 10: return true;
    default: return false;
    }
}

bool is_c(uint8_t note) { return ((note - 1) % 12) == 0; }

// Bar opacity follows the attenuation: 0 = loudest, 15 = quietest.
int attn_alpha(uint8_t attn) {
    if (attn == 0xFF) return 235;
    return 235 - std::min<int>(attn, 15) * 11;
}

} // namespace

// ============================================================
// Construction
// ============================================================

PianoRollWidget::PianoRollWidget(TrackerDocument* doc, QWidget* parent)
    : QWidget(parent), doc_(doc)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumHeight(kRulerHeight + kToneKeyHeight * 12 + kVolumeGap + kVolumeHeight + 16);

    hscroll_ = new QScrollBar(Qt::Horizontal, this);
    vscroll_ = new QScrollBar(Qt::Vertical, this);
    connect(hscroll_, &QScrollBar::valueChanged, this, [this]() { update(); });
    connect(vscroll_, &QScrollBar::valueChanged, this, [this]() { update(); });

    connect_document();
}

void PianoRollWidget::connect_document() {
    connect(doc_, &TrackerDocument::cell_changed, this, [this](int, int) { update(); });
    connect(doc_, &TrackerDocument::length_changed, this, [this](int) {
        fit_width();
        update();
    });
    connect(doc_, &TrackerDocument::document_reset, this, [this]() {
        update_scrollbars();
        update();
    });
}

void PianoRollWidget::set_document(TrackerDocument* doc) {
    if (doc == doc_ || !doc) return;
    if (doc_) doc_->disconnect(this);
    doc_ = doc;
    connect_document();
    cancel_drag();
    selection_.clear();
    cursor_row_ = std::clamp(cursor_row_, 0, doc_->length() - 1);
    fit_width();
    update();
}

void PianoRollWidget::set_channel(int ch) {
    ch = std::clamp(ch, 0, TrackerDocument::kChannelCount - 1);
    if (ch == channel_) return;
    const bool axis_changed = (ch == 3) != is_noise();
    cancel_drag();
    selection_.clear();
    channel_ = ch;
    hover_lane_ = -1;
    update_scrollbars();
    if (axis_changed) center_on_notes();
    update();
}

void PianoRollWidget::set_cursor_row(int row) {
    if (!doc_) return;
    row = std::clamp(row, 0, doc_->length() - 1);
    if (row == cursor_row_) return;
    cursor_row_ = row;
    ensure_row_visible(row);
    update();
}

void PianoRollWidget::set_playback_row(int row) {
    playback_row_ = row;
    update();
}

void PianoRollWidget::set_channel_muted(int ch, bool muted) {
    if (ch < 0 || ch >= 4) return;
    muted_[ch] = muted;
    update();
}

// ============================================================
// Geometry
// ============================================================

int PianoRollWidget::lane_count() const {
    return is_noise() ? kNoiseLanes : (kHighestNote - kLowestNote + 1);
}

int PianoRollWidget::lane_height() const {
    return is_noise() ? kNoiseLaneHeight : kToneKeyHeight;
}

int PianoRollWidget::lane_of_note(uint8_t note) const {
    if (note < 1 || note > 127) return -1;
    if (is_noise()) return TrackerPlaybackEngine::midi_note_to_noise_val(note);
    if (note < kLowestNote || note > kHighestNote) return -1;
    return kHighestNote - note;
}

uint8_t PianoRollWidget::note_of_lane(int lane) const {
    if (is_noise()) return static_cast<uint8_t>(lane + 1);
    return static_cast<uint8_t>(kHighestNote - lane);
}

QRect PianoRollWidget::grid_rect() const {
    const int sb = vscroll_->sizeHint().width();
    const int sbh = hscroll_->sizeHint().height();
    return QRect(kKeyboardWidth, kRulerHeight,
                 std::max(0, width() - kKeyboardWidth - sb),
                 std::max(0, height() - kRulerHeight - sbh - kVolumeGap - kVolumeHeight));
}

QRect PianoRollWidget::volume_rect() const {
    const QRect g = grid_rect();
    return QRect(g.left(), g.bottom() + 1 + kVolumeGap, g.width(), kVolumeHeight);
}

int PianoRollWidget::row_x(int row) const {
    return grid_rect().left() + row * col_width_ - hscroll_->value();
}

int PianoRollWidget::lane_y(int lane) const {
    return grid_rect().top() + lane * lane_height() - vscroll_->value();
}

int PianoRollWidget::hit_row(int x) const {
    if (!doc_) return -1;
    const int rel = x - grid_rect().left() + hscroll_->value();
    if (rel < 0) return -1;
    const int row = rel / col_width_;
    return (row < doc_->length()) ? row : -1;
}

int PianoRollWidget::hit_lane(int y) const {
    const QRect g = grid_rect();
    if (y < g.top() || y > g.bottom()) return -1;
    const int lane = (y - g.top() + vscroll_->value()) / lane_height();
    return (lane < lane_count()) ? lane : -1;
}

int PianoRollWidget::row_at_clamped(int x) const {
    const int rel = x - grid_rect().left() + hscroll_->value();
    return std::clamp(rel / col_width_, 0, doc_->length() - 1);
}

int PianoRollWidget::lane_at_clamped(int y) const {
    return std::clamp((y - grid_rect().top() + vscroll_->value()) / lane_height(), 0, lane_count() - 1);
}

QString PianoRollWidget::lane_label(int lane) const {
    if (is_noise()) {
        return TrackerPlaybackEngine::noise_display_name(static_cast<uint8_t>(lane));
    }
    return TrackerGridWidget::note_name(note_of_lane(lane));
}

QSize PianoRollWidget::sizeHint() const {
    return QSize(kKeyboardWidth + 64 * kDefaultColWidth,
                 kRulerHeight + kToneKeyHeight * 36 + kVolumeGap + kVolumeHeight);
}

QRect PianoRollWidget::bar_rect(const NoteSpan& s) const {
    const int lane = std::max(0, lane_of_note(s.note));
    return QRect(row_x(s.start) + 1, lane_y(lane) + 1, s.length * col_width_ - 2, lane_height() - 2);
}

// ============================================================
// Scrolling / zoom
// ============================================================

void PianoRollWidget::update_scrollbars() {
    if (!doc_) return;
    const QRect g = grid_rect();
    const int content_w = doc_->length() * col_width_;
    const int content_h = lane_count() * lane_height();

    hscroll_->setRange(0, std::max(0, content_w - g.width()));
    hscroll_->setPageStep(std::max(1, g.width()));
    hscroll_->setSingleStep(col_width_);
    vscroll_->setRange(0, std::max(0, content_h - g.height()));
    vscroll_->setPageStep(std::max(1, g.height()));
    vscroll_->setSingleStep(lane_height());
}

void PianoRollWidget::ensure_row_visible(int row) {
    const QRect g = grid_rect();
    const int left = row * col_width_;
    const int right = left + col_width_;
    const int view_l = hscroll_->value();
    const int view_r = view_l + g.width();
    if (left < view_l) {
        hscroll_->setValue(left);
    } else if (right > view_r) {
        hscroll_->setValue(right - g.width());
    }
}

void PianoRollWidget::set_zoom(int col_width, int anchor_x) {
    col_width = std::clamp(col_width, kMinColWidth, kMaxColWidth);
    if (col_width == col_width_) return;
    // Keep the row under anchor_x in place.
    const int rel = anchor_x - grid_rect().left();
    const double row_at = (rel + hscroll_->value()) / static_cast<double>(col_width_);
    col_width_ = col_width;
    update_scrollbars();
    hscroll_->setValue(static_cast<int>(row_at * col_width_) - rel);
    update();
}

void PianoRollWidget::fit_width() {
    if (!doc_ || doc_->length() <= 0) return;
    const int w = grid_rect().width();
    if (w > 0) {
        col_width_ = std::clamp(w / doc_->length(), kMinFitColWidth, kMaxColWidth);
    }
    update_scrollbars();
    ensure_row_visible(cursor_row_);
}

void PianoRollWidget::center_on_notes() {
    if (!doc_) return;
    int lo = -1, hi = -1;
    for (const NoteSpan& s : note_spans::from_channel(*doc_, channel_)) {
        const int lane = lane_of_note(s.note);
        if (lane < 0) continue;
        lo = (lo < 0) ? lane : std::min(lo, lane);
        hi = std::max(hi, lane);
    }
    if (lo < 0) {
        // Empty channel: middle of the keyboard (C-4) or the first noise lane.
        lo = hi = is_noise() ? 0 : lane_of_note(49);
    }
    const int mid = (lo + hi + 1) * lane_height() / 2;
    vscroll_->setValue(mid - grid_rect().height() / 2);
}

void PianoRollWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const int sb = vscroll_->sizeHint().width();
    const int sbh = hscroll_->sizeHint().height();
    hscroll_->setGeometry(kKeyboardWidth, height() - sbh, width() - kKeyboardWidth - sb, sbh);
    vscroll_->setGeometry(width() - sb, kRulerHeight, sb, grid_rect().height());
    update_scrollbars();
}

void PianoRollWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    fit_width();
    center_on_notes();
    ensure_row_visible(cursor_row_);
}

// ============================================================
// Paint
// ============================================================

void PianoRollWidget::paintEvent(QPaintEvent*) {
    if (!doc_) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor bgColor(30, 30, 30);
    const QColor laneWhite(38, 38, 42);
    const QColor laneBlack(31, 31, 35);
    const QColor laneUnplayable(48, 30, 30);
    const QColor outsidePattern(22, 22, 24);
    const QColor gridLine(46, 46, 52);
    const QColor beatLine(62, 62, 72);
    const QColor beat16Line(90, 82, 62);
    const QColor octaveLine(58, 58, 66);
    const QColor cursorHl(200, 180, 60, 45);
    const QColor cursorLine(200, 180, 60);
    const QColor playbackLine(90, 140, 230);
    const QColor rulerBg(40, 40, 45);
    const QColor rulerText(130, 130, 150);
    const QColor ruler16Text(200, 190, 140);
    const QColor ghostColor(150, 150, 170, 70);
    const QColor mutedOverlay(0, 0, 0, 110);
    const QColor selectColor(255, 255, 255);

    p.fillRect(rect(), bgColor);

    const QRect g = grid_rect();
    const int lh = lane_height();
    const int lanes = lane_count();
    const int len = doc_->length();
    const int first_lane = std::max(0, (vscroll_->value()) / lh);
    const int last_lane = std::min(lanes - 1, first_lane + g.height() / lh + 1);
    const int first_row = std::max(0, (hscroll_->value()) / col_width_);
    const int last_row = std::min(len - 1, first_row + g.width() / col_width_ + 1);
    const uint8_t lowest_tone = note_spans::lowest_tone_note();
    const auto spans = note_spans::from_channel(*doc_, channel_);

    // ---- Grid ----
    p.save();
    p.setClipRect(g);

    for (int lane = first_lane; lane <= last_lane; ++lane) {
        const int y = lane_y(lane);
        QColor c = laneWhite;
        if (!is_noise()) {
            const uint8_t note = note_of_lane(lane);
            if (is_black_key(note)) c = laneBlack;
            if (note < lowest_tone) c = laneUnplayable;
        } else if (lane % 2) {
            c = laneBlack;
        }
        p.fillRect(g.left(), y, g.width(), lh, c);
        const bool octave = !is_noise() && is_c(note_of_lane(lane));
        p.setPen(octave ? octaveLine : gridLine);
        p.drawLine(g.left(), y + lh - 1, g.right(), y + lh - 1);
    }

    // Beyond the pattern end
    const int end_x = row_x(len);
    if (end_x < g.right()) p.fillRect(end_x, g.top(), g.right() - end_x + 1, g.height(), outsidePattern);

    // Cursor column
    p.fillRect(row_x(cursor_row_), g.top(), col_width_, g.height(), cursorHl);

    // Vertical row lines: every row when zoomed in, beats every 4, bars every 16
    for (int row = first_row; row <= last_row + 1 && row <= len; ++row) {
        const int x = row_x(row);
        if (row % 16 == 0) p.setPen(beat16Line);
        else if (row % 4 == 0) p.setPen(beatLine);
        else if (col_width_ >= 10) p.setPen(gridLine);
        else continue;
        p.drawLine(x, g.top(), x, g.bottom());
    }

    // Ghost voices: the other tone channels, outlines only
    if (!is_noise()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ghostColor, 1));
        for (int ch = 0; ch < 3; ++ch) {
            if (ch == channel_) continue;
            for (const NoteSpan& s : note_spans::from_channel(*doc_, ch)) {
                const int lane = lane_of_note(s.note);
                if (lane < 0) continue;
                p.drawRect(row_x(s.start) + 1, lane_y(lane) + 1,
                           s.length * col_width_ - 3, lh - 3);
            }
        }
    }

    // Notes of the current channel
    QFont noteFont = font();
    noteFont.setFamily(QStringLiteral("Consolas"));
    noteFont.setPixelSize(std::min(11, lh - 1));
    p.setFont(noteFont);

    auto draw_label = [&](const QRect& bar, uint8_t note) {
        if (bar.width() < 34) return;
        p.setPen(QColor(20, 20, 24));
        const QString label = is_noise() ? TrackerGridWidget::noise_note_name(note)
                                         : TrackerGridWidget::note_name(note);
        p.drawText(bar.adjusted(6, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    };

    // Bars being moved or resized are drawn faded where they were
    std::set<int> lifted;
    if (drag_ == Drag::Move) {
        for (const NoteSpan& s : drag_group_) lifted.insert(s.start);
    } else if (drag_ == Drag::Resize) {
        lifted.insert(drag_orig_.start);
    }

    for (size_t i = 0; i < spans.size(); ++i) {
        const NoteSpan& s = spans[i];
        if (lane_of_note(s.note) < 0) continue;
        const QRect bar = bar_rect(s);

        // 3xx: draw the glide from the previous note
        if (s.slide && i > 0) {
            const int prev_lane = lane_of_note(spans[i - 1].note);
            if (prev_lane >= 0) {
                p.setPen(QPen(QColor(230, 230, 240, 160), 1, Qt::DashLine));
                p.drawLine(bar.left() - 1, lane_y(prev_lane) + lh / 2, bar.left() + 1, bar.center().y());
            }
        }

        QColor fill = TrackerGridWidget::instrument_color(s.instrument);
        fill.setAlpha(lifted.count(s.start) ? 50 : attn_alpha(s.attn));
        p.fillRect(bar, fill);
        p.setPen(fill.darker(220));
        p.drawRect(bar.adjusted(0, 0, -1, -1));
        if (selection_.count(s.start) && !lifted.count(s.start)) {
            p.setPen(QPen(selectColor, 2));
            p.drawRect(bar.adjusted(0, 0, -1, -1));
        }

        // Explicit note-off: darker right edge
        if (s.ends_with_off) {
            p.fillRect(bar.right() - 1, bar.top(), 2, bar.height(), QColor(180, 100, 100));
        }
        // Effect on the start cell: small corner mark
        if (s.has_fx) {
            p.fillRect(bar.left() + 1, bar.top() + 1, 4, 4, QColor(255, 255, 255, 220));
        }
        draw_label(bar, s.note);
    }

    // Bars being drawn, moved or resized: written on mouse release
    std::vector<NoteSpan> floating;
    if (drag_ == Drag::Create || drag_ == Drag::Resize) floating.push_back(drag_cur_);
    if (drag_ == Drag::Move) floating = shifted(drag_group_, drag_d_row_, drag_d_lane_);
    for (const NoteSpan& s : floating) {
        const QRect bar = bar_rect(s);
        QColor fill = TrackerGridWidget::instrument_color(s.instrument);
        fill.setAlpha(200);
        p.fillRect(bar, fill);
        p.setPen(QPen(selectColor, 2));
        p.drawRect(bar.adjusted(0, 0, -1, -1));
        draw_label(bar, s.note);
    }

    // Rubber band selection
    if (drag_ == Drag::Select) {
        p.setPen(QPen(QColor(220, 220, 255), 1, Qt::DashLine));
        p.setBrush(QColor(160, 160, 255, 40));
        p.drawRect(QRect(select_from_, select_to_).normalized());
        p.setBrush(Qt::NoBrush);
    }

    if (muted_[channel_]) p.fillRect(g, mutedOverlay);

    // Cursor and playback lines
    p.setPen(cursorLine);
    p.drawLine(row_x(cursor_row_), g.top(), row_x(cursor_row_), g.bottom());
    if (playback_row_ >= 0 && playback_row_ < len) {
        p.setPen(QPen(playbackLine, 2));
        p.drawLine(row_x(playback_row_), g.top(), row_x(playback_row_), g.bottom());
    }
    p.restore();

    paint_volume_lane(p, spans);

    // ---- Ruler (row numbers) ----
    p.fillRect(g.left(), 0, g.width(), kRulerHeight, rulerBg);
    p.save();
    p.setClipRect(g.left(), 0, g.width(), kRulerHeight);
    QFont rulerFont = font();
    rulerFont.setPixelSize(10);
    p.setFont(rulerFont);
    const int label_every = (col_width_ >= 12) ? 4 : 16;
    for (int row = first_row - (first_row % label_every); row <= last_row; row += label_every) {
        if (row < 0) continue;
        const int x = row_x(row);
        p.setPen(row % 16 == 0 ? ruler16Text : rulerText);
        p.drawLine(x, kRulerHeight - 6, x, kRulerHeight - 1);
        p.drawText(x + 3, 0, 40, kRulerHeight - 4, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(row, 16).toUpper().rightJustified(2, QLatin1Char('0')));
    }
    p.setPen(cursorLine);
    p.drawLine(row_x(cursor_row_), kRulerHeight - 8, row_x(cursor_row_), kRulerHeight - 1);
    p.restore();

    // ---- Keyboard ----
    const QRect kb(0, g.top(), kKeyboardWidth, g.height());
    p.save();
    p.setClipRect(kb);
    QFont keyFont = font();
    keyFont.setPixelSize(is_noise() ? 11 : 9);
    p.setFont(keyFont);
    for (int lane = first_lane; lane <= last_lane; ++lane) {
        const int y = lane_y(lane);
        QColor key(215, 215, 220);
        QColor text(60, 60, 70);
        if (is_noise()) {
            key = (lane % 2) ? QColor(70, 70, 80) : QColor(85, 85, 95);
            text = QColor(210, 210, 225);
        } else {
            const uint8_t note = note_of_lane(lane);
            if (is_black_key(note)) { key = QColor(40, 40, 46); text = QColor(150, 150, 160); }
            if (note < lowest_tone) key = key.darker(160);
        }
        if (lane == hover_lane_) key = QColor(200, 180, 60);
        p.fillRect(0, y, kKeyboardWidth, lh, key);
        p.setPen(QColor(20, 20, 24));
        p.drawLine(0, y + lh - 1, kKeyboardWidth, y + lh - 1);
        if (is_noise() || is_c(note_of_lane(lane)) || lane == hover_lane_) {
            p.setPen(text);
            p.drawText(QRect(4, y, kKeyboardWidth - 8, lh), Qt::AlignVCenter | Qt::AlignRight,
                       lane_label(lane));
        }
    }
    p.restore();

    // ---- Corner: channel name ----
    p.fillRect(0, 0, kKeyboardWidth, kRulerHeight, rulerBg);
    QFont headerFont = font();
    headerFont.setBold(true);
    headerFont.setPixelSize(11);
    p.setFont(headerFont);
    p.setPen(muted_[channel_] ? QColor(100, 100, 110) : QColor(180, 180, 200));
    QString title = QString::fromLatin1(kChannelNames[channel_]);
    if (muted_[channel_]) title += QStringLiteral(" [M]");
    p.drawText(QRect(0, 0, kKeyboardWidth, kRulerHeight), Qt::AlignCenter, title);

    // Focus hint
    if (hasFocus()) {
        p.setPen(QColor(200, 180, 60, 120));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

// Volume lane: one stick per note, height = 15 - attenuation.
// A hollow stick means "instrument default" (no attenuation override).
void PianoRollWidget::paint_volume_lane(QPainter& p, const std::vector<NoteSpan>& spans) {
    const QRect vr = volume_rect();
    const int usable = vr.height() - 8;

    p.fillRect(QRect(0, vr.top(), kKeyboardWidth, vr.height()), QColor(40, 40, 45));
    QFont f = font();
    f.setPixelSize(10);
    p.setFont(f);
    p.setPen(QColor(150, 150, 170));
    p.drawText(QRect(4, vr.top(), kKeyboardWidth - 8, vr.height()), Qt::AlignCenter,
               QStringLiteral("Vol"));

    p.save();
    p.setClipRect(vr);
    p.fillRect(vr, QColor(26, 26, 29));
    p.setPen(QColor(46, 46, 52));
    for (int level : {0, 5, 10, 15}) {
        const int y = vr.bottom() - 2 - level * usable / 15;
        p.drawLine(vr.left(), y, vr.right(), y);
    }
    const int end_x = row_x(doc_->length());
    if (end_x < vr.right()) p.fillRect(end_x, vr.top(), vr.right() - end_x + 1, vr.height(), QColor(22, 22, 24));

    const int stick_w = std::clamp(col_width_ - 6, 3, 10);
    for (const NoteSpan& s : spans) {
        const int x = row_x(s.start) + (col_width_ - stick_w) / 2;
        QColor c = TrackerGridWidget::instrument_color(s.instrument);
        const bool selected = selection_.count(s.start) > 0;
        if (s.attn == 0xFF) {
            p.setPen(QPen(c, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(x, vr.bottom() - 2 - usable, stick_w - 1, usable);
        } else {
            const int level = 15 - std::min<int>(s.attn, 15);
            const int h = std::max(2, level * usable / 15);
            p.fillRect(x, vr.bottom() - 1 - h, stick_w, h, c);
        }
        if (selected) {
            p.setPen(QPen(QColor(255, 255, 255), 1));
            p.drawLine(x - 1, vr.bottom(), x + stick_w, vr.bottom());
        }
    }
    p.restore();
}

// ============================================================
// Editing helpers
// ============================================================

int PianoRollWidget::hit_span(const QPoint& pos, const std::vector<NoteSpan>& spans) const {
    for (size_t i = 0; i < spans.size(); ++i) {
        if (lane_of_note(spans[i].note) >= 0 && bar_rect(spans[i]).adjusted(-1, -1, 1, 1).contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PianoRollWidget::on_resize_edge(const QPoint& pos, const NoteSpan& s) const {
    const QRect bar = bar_rect(s);
    const int grip = std::min(6, bar.width() / 3);
    return pos.x() >= bar.right() - grip;
}

void PianoRollWidget::pick_brush(const NoteSpan& s) {
    brush_inst_[static_cast<size_t>(channel_)] = s.instrument;
    brush_attn_[static_cast<size_t>(channel_)] = s.attn;
}

NoteSpan PianoRollWidget::new_span(int row, int lane) const {
    NoteSpan s;
    s.start = row;
    s.length = std::clamp(new_note_length_, 1, doc_->length() - row);
    s.note = note_of_lane(lane);
    const size_t ch = static_cast<size_t>(channel_);
    if (brush_inst_[ch] >= 0) {
        s.instrument = static_cast<uint8_t>(brush_inst_[ch]);
        s.attn = static_cast<uint8_t>(brush_attn_[ch]);
    } else {
        // Nothing clicked yet on this voice: reuse the note before, else the first one
        const auto spans = note_spans::from_channel(*doc_, channel_);
        const NoteSpan* ref = spans.empty() ? nullptr : &spans.front();
        for (const NoteSpan& o : spans) {
            if (o.start <= row) ref = &o;
        }
        if (ref) {
            s.instrument = ref->instrument;
            s.attn = ref->attn;
        }
    }
    return s;
}

std::vector<NoteSpan> PianoRollWidget::selected_spans() const {
    std::vector<NoteSpan> out;
    if (!doc_) return out;
    for (const NoteSpan& s : note_spans::from_channel(*doc_, channel_)) {
        if (selection_.count(s.start)) out.push_back(s);
    }
    return out;
}

std::vector<NoteSpan> PianoRollWidget::shifted(const std::vector<NoteSpan>& group,
                                               int d_row, int d_lane) const {
    std::vector<NoteSpan> out = group;
    for (NoteSpan& s : out) {
        s.start += d_row;
        s.note = note_of_lane(lane_of_note(s.note) + d_lane);
    }
    return out;
}

// Keep the whole group inside the pattern and the lanes.
void PianoRollWidget::clamp_shift(const std::vector<NoteSpan>& group, int& d_row, int& d_lane) const {
    if (group.empty()) { d_row = d_lane = 0; return; }
    int min_start = doc_->length(), max_end = 0, min_lane = lane_count(), max_lane = 0;
    for (const NoteSpan& s : group) {
        min_start = std::min(min_start, s.start);
        max_end = std::max(max_end, s.start + s.length);
        min_lane = std::min(min_lane, lane_of_note(s.note));
        max_lane = std::max(max_lane, lane_of_note(s.note));
    }
    d_row = std::clamp(d_row, -min_start, doc_->length() - max_end);
    d_lane = std::clamp(d_lane, -min_lane, lane_count() - 1 - max_lane);
}

void PianoRollWidget::apply_group(const std::vector<NoteSpan>& from, const std::vector<NoteSpan>& to) {
    doc_->push_undo();
    note_spans::replace(*doc_, channel_, from, to);
    selection_.clear();
    for (const NoteSpan& s : to) {
        if (s.start >= 0 && s.start < doc_->length()) selection_.insert(s.start);
    }
    update();
}

void PianoRollWidget::transpose_selection(int d_lane) {
    const auto group = selected_spans();
    if (group.empty()) return;
    int d_row = 0;
    clamp_shift(group, d_row, d_lane);
    if (d_lane == 0) return;
    const auto moved = shifted(group, 0, d_lane);
    apply_group(group, moved);
    emit note_preview_requested(channel_, moved.front().note);
}

void PianoRollWidget::copy_selection() {
    auto group = selected_spans();
    if (group.empty()) return;
    const int origin = group.front().start;
    for (NoteSpan& s : group) s.start -= origin;
    clipboard_ = group;
}

void PianoRollWidget::cut_selection() {
    const auto group = selected_spans();
    if (group.empty()) return;
    copy_selection();
    apply_group(group, {});
}

void PianoRollWidget::paste_at_cursor() {
    if (clipboard_.empty()) return;
    std::vector<NoteSpan> to;
    for (NoteSpan s : clipboard_) {
        s.start += cursor_row_;
        if (s.start >= doc_->length()) continue;
        s.length = std::min(s.length, doc_->length() - s.start);
        to.push_back(s);
    }
    if (!to.empty()) apply_group({}, to);
}

void PianoRollWidget::delete_selection() {
    const auto group = selected_spans();
    if (group.empty()) return;
    apply_group(group, {});
}

void PianoRollWidget::commit_drag(const QPoint& pos) {
    const Drag d = drag_;
    drag_ = Drag::None;
    switch (d) {
    case Drag::Create:
        apply_group({}, {drag_cur_});
        new_note_length_ = drag_cur_.length;
        break;
    case Drag::Resize:
        if (drag_cur_.length != drag_orig_.length) {
            apply_group({drag_orig_}, {drag_cur_});
            new_note_length_ = drag_cur_.length;
        }
        break;
    case Drag::Move:
        if (drag_d_row_ != 0 || drag_d_lane_ != 0) {
            apply_group(drag_group_, shifted(drag_group_, drag_d_row_, drag_d_lane_));
        }
        break;
    case Drag::Select: {
        const QRect band = QRect(select_from_, pos).normalized();
        for (const NoteSpan& s : note_spans::from_channel(*doc_, channel_)) {
            if (band.intersects(bar_rect(s))) selection_.insert(s.start);
        }
        break;
    }
    default:
        break;
    }
    update();
}

void PianoRollWidget::cancel_drag() {
    drag_ = Drag::None;
    unsetCursor();
    update();
}

void PianoRollWidget::erase_at(const QPoint& pos) {
    const auto spans = note_spans::from_channel(*doc_, channel_);
    const int idx = hit_span(pos, spans);
    if (idx < 0) return;
    if (!gesture_undo_pushed_) {
        doc_->push_undo();
        gesture_undo_pushed_ = true;
    }
    const int start = spans[static_cast<size_t>(idx)].start;
    note_spans::erase(*doc_, channel_, start);
    selection_.erase(start);
}

void PianoRollWidget::set_volume_at(const QPoint& pos, bool reset) {
    const QRect vr = volume_rect();
    const auto spans = note_spans::from_channel(*doc_, channel_);
    const int idx = note_spans::span_at(spans, row_at_clamped(pos.x()));
    if (idx < 0) return;
    const NoteSpan& s = spans[static_cast<size_t>(idx)];
    uint8_t attn = 0xFF;
    if (!reset) {
        const int usable = vr.height() - 8;
        const int level = std::clamp(((vr.bottom() - 2 - pos.y()) * 15 + usable / 2) / usable, 0, 15);
        attn = static_cast<uint8_t>(15 - level);
    }
    if (attn == s.attn) return;
    if (!gesture_undo_pushed_) {
        doc_->push_undo();
        gesture_undo_pushed_ = true;
    }
    doc_->set_attn(channel_, s.start, attn);
    NoteSpan changed = s;
    changed.attn = attn;
    pick_brush(changed);
}

void PianoRollWidget::update_hover_cursor(const QPoint& pos) {
    if (volume_rect().contains(pos)) {
        setCursor(Qt::SizeVerCursor);
        return;
    }
    if (pos.x() < kKeyboardWidth || !grid_rect().contains(pos)) {
        unsetCursor();
        return;
    }
    const auto spans = note_spans::from_channel(*doc_, channel_);
    const int idx = hit_span(pos, spans);
    if (idx < 0) setCursor(Qt::CrossCursor);
    else if (on_resize_edge(pos, spans[static_cast<size_t>(idx)])) setCursor(Qt::SizeHorCursor);
    else setCursor(Qt::OpenHandCursor);
}

// ============================================================
// Input
// ============================================================

void PianoRollWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (!doc_ || drag_ != Drag::None) return;
    const QPoint pos = event->position().toPoint();
    const auto mod = event->modifiers();
    const bool left = event->button() == Qt::LeftButton;
    const bool right = event->button() == Qt::RightButton;

    // Keyboard: listen to a key
    if (pos.x() < kKeyboardWidth) {
        const int lane = hit_lane(pos.y());
        if (left && lane >= 0) emit note_preview_requested(channel_, note_of_lane(lane));
        return;
    }

    // Ruler: move the cursor only
    if (pos.y() < kRulerHeight) {
        const int row = hit_row(pos.x());
        if (row >= 0) emit cursor_row_clicked(row);
        return;
    }

    // Volume lane: left = set, right = back to the instrument default
    if (volume_rect().contains(pos)) {
        if (!left && !right) return;
        drag_ = left ? Drag::Volume : Drag::VolumeReset;
        gesture_undo_pushed_ = false;
        last_pos_ = pos;
        set_volume_at(pos, right);
        return;
    }

    const int row = hit_row(pos.x());
    const int lane = hit_lane(pos.y());
    if (row < 0 || lane < 0) return;

    // Right button: erase (drag to erase several)
    if (right) {
        drag_ = Drag::Erase;
        gesture_undo_pushed_ = false;
        last_pos_ = pos;
        erase_at(pos);
        return;
    }
    if (!left) return;

    // Shift: rubber band selection
    if (mod & Qt::ShiftModifier) {
        if (!(mod & Qt::ControlModifier)) selection_.clear();
        drag_ = Drag::Select;
        select_from_ = select_to_ = pos;
        update();
        return;
    }

    const auto spans = note_spans::from_channel(*doc_, channel_);
    const int idx = hit_span(pos, spans);
    if (idx >= 0) {
        const NoteSpan& s = spans[static_cast<size_t>(idx)];
        // Ctrl: add / remove from the selection
        if (mod & Qt::ControlModifier) {
            if (!selection_.erase(s.start)) selection_.insert(s.start);
            update();
            return;
        }
        pick_brush(s);
        if (on_resize_edge(pos, s)) {
            selection_ = {s.start};
            drag_ = Drag::Resize;
            drag_orig_ = s;
            drag_cur_ = s;
        } else {
            // Grabbing a selected bar moves the whole selection
            if (!selection_.count(s.start)) selection_ = {s.start};
            drag_ = Drag::Move;
            drag_group_ = selected_spans();
            drag_grabbed_ = s;
            drag_row_ = row;
            drag_lane_ = lane;
            drag_d_row_ = drag_d_lane_ = 0;
            setCursor(Qt::ClosedHandCursor);
        }
        // The preview plays with the instrument of the cursor row
        emit cursor_row_clicked(s.start);
        emit note_preview_requested(channel_, s.note);
    } else {
        selection_.clear();
        drag_ = Drag::Create;
        drag_left_start_row_ = false;
        drag_cur_ = new_span(row, lane);
        emit cursor_row_clicked(row);
        emit note_preview_requested(channel_, drag_cur_.note);
    }
    update();
}

void PianoRollWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->position().toPoint();
    const int lane_kb = (pos.x() < kKeyboardWidth) ? hit_lane(pos.y()) : -1;
    if (lane_kb != hover_lane_ && drag_ == Drag::None) {
        hover_lane_ = lane_kb;
        // Dragging along the keyboard plays each key it crosses
        if (lane_kb >= 0 && (event->buttons() & Qt::LeftButton)) {
            emit note_preview_requested(channel_, note_of_lane(lane_kb));
        }
        update();
    }

    switch (drag_) {
    case Drag::None:
        if (!(event->buttons() & Qt::LeftButton)) update_hover_cursor(pos);
        return;
    case Drag::Erase:
    case Drag::Volume:
    case Drag::VolumeReset: {
        // Follow the whole path: a fast stroke must not skip the notes in between
        const QPoint delta = pos - last_pos_;
        const int steps = std::max(1, std::max(std::abs(delta.x()), std::abs(delta.y())) / 3);
        for (int i = 1; i <= steps; ++i) {
            const QPoint pt = last_pos_ + delta * i / steps;
            if (drag_ == Drag::Erase) erase_at(pt);
            else set_volume_at(pt, drag_ == Drag::VolumeReset);
        }
        last_pos_ = pos;
        return;
    }
    case Drag::Select:
        select_to_ = pos;
        update();
        return;
    default:
        break;
    }

    const int len = doc_->length();
    const int row = row_at_clamped(pos.x());
    if (drag_ == Drag::Move) {
        int d_row = row - drag_row_;
        int d_lane = lane_at_clamped(pos.y()) - drag_lane_;
        clamp_shift(drag_group_, d_row, d_lane);
        if (d_row == drag_d_row_ && d_lane == drag_d_lane_) return;
        if (d_lane != drag_d_lane_) {
            emit note_preview_requested(channel_, shifted({drag_grabbed_}, 0, d_lane).front().note);
        }
        drag_d_row_ = d_row;
        drag_d_lane_ = d_lane;
        update();
        return;
    }

    // Create / Resize: the bar ends under the mouse. A plain click keeps
    // the last length; the drag takes over once the mouse leaves the row.
    NoteSpan next = drag_cur_;
    if (drag_ == Drag::Create && row != next.start) drag_left_start_row_ = true;
    if (drag_ == Drag::Resize || drag_left_start_row_) {
        next.length = std::clamp(row - next.start + 1, 1, len - next.start);
    }
    if (next.length != drag_cur_.length) {
        drag_cur_ = next;
        update();
    }
}

void PianoRollWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (drag_ == Drag::None) return;
    const bool right_gesture = drag_ == Drag::Erase || drag_ == Drag::VolumeReset;
    const Qt::MouseButton expected = right_gesture ? Qt::RightButton : Qt::LeftButton;
    if (event->button() != expected) return;
    commit_drag(event->position().toPoint());
    update_hover_cursor(event->position().toPoint());
}

void PianoRollWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!doc_ || event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier) return;
    const QPoint pos = event->position().toPoint();
    const auto spans = note_spans::from_channel(*doc_, channel_);
    const int idx = hit_span(pos, spans);
    if (idx < 0) return;
    drag_ = Drag::None;
    const int start = spans[static_cast<size_t>(idx)].start;
    selection_ = {start};
    emit instrument_dialog_requested(channel_, start);
    // The dialog may have changed the instrument: new bars follow it
    const auto after = note_spans::from_channel(*doc_, channel_);
    const int j = note_spans::span_at(after, start);
    if (j >= 0) pick_brush(after[static_cast<size_t>(j)]);
    update();
}

void PianoRollWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (hover_lane_ >= 0) {
        hover_lane_ = -1;
        update();
    }
}

void PianoRollWidget::wheelEvent(QWheelEvent* event) {
    const int dy = event->angleDelta().y();
    const auto mod = event->modifiers();
    if (mod & Qt::ControlModifier) {
        const int step = std::max(1, col_width_ / 4);
        set_zoom(col_width_ + (dy > 0 ? step : -step), event->position().toPoint().x());
    } else if (mod & Qt::ShiftModifier) {
        hscroll_->setValue(hscroll_->value() - dy / 120 * col_width_ * 4);
    } else {
        vscroll_->setValue(vscroll_->value() - dy / 120 * lane_height() * 3);
    }
    event->accept();
}

bool PianoRollWidget::focusNextPrevChild(bool) {
    return false; // keep Tab for voice switching
}

void PianoRollWidget::keyPressEvent(QKeyEvent* event) {
    if (!doc_) return;
    const int len = doc_->length();
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    if (ctrl) {
        switch (event->key()) {
        case Qt::Key_A:
            selection_.clear();
            for (const NoteSpan& s : note_spans::from_channel(*doc_, channel_)) selection_.insert(s.start);
            update();
            return;
        case Qt::Key_C: copy_selection(); return;
        case Qt::Key_X: cut_selection(); return;
        case Qt::Key_V: paste_at_cursor(); return;
        case Qt::Key_S: emit save_requested(); return;
        case Qt::Key_O: emit load_requested(); return;
        default: break;
        }
    }

    switch (event->key()) {
    case Qt::Key_Escape:
        if (drag_ != Drag::None) cancel_drag();
        else { selection_.clear(); update(); }
        return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        if (drag_ == Drag::None) delete_selection();
        return;
    case Qt::Key_Space: emit play_stop_toggled(); return;
    case Qt::Key_F5:    emit play_from_start_requested(); return;
    case Qt::Key_F8:    emit stop_requested(); return;
    case Qt::Key_F1: emit mute_toggle_requested(0); return;
    case Qt::Key_F2: emit mute_toggle_requested(1); return;
    case Qt::Key_F3: emit mute_toggle_requested(2); return;
    case Qt::Key_F4: emit mute_toggle_requested(3); return;
    case Qt::Key_Left:
        emit cursor_row_clicked(std::max(0, cursor_row_ - (ctrl ? 4 : 1)));
        return;
    case Qt::Key_Right:
        emit cursor_row_clicked(std::min(len - 1, cursor_row_ + (ctrl ? 4 : 1)));
        return;
    case Qt::Key_Home: emit cursor_row_clicked(0); return;
    case Qt::Key_End:  emit cursor_row_clicked(len - 1); return;
    // With a selection: transpose (Shift = one octave). Without: scroll.
    case Qt::Key_Up:
        if (!selection_.empty()) transpose_selection(-(shift && !is_noise() ? 12 : 1));
        else vscroll_->setValue(vscroll_->value() - lane_height() * 2);
        return;
    case Qt::Key_Down:
        if (!selection_.empty()) transpose_selection(shift && !is_noise() ? 12 : 1);
        else vscroll_->setValue(vscroll_->value() + lane_height() * 2);
        return;
    case Qt::Key_PageUp:   vscroll_->setValue(vscroll_->value() - vscroll_->pageStep()); return;
    case Qt::Key_PageDown: vscroll_->setValue(vscroll_->value() + vscroll_->pageStep()); return;
    case Qt::Key_Tab:     emit channel_requested((channel_ + 1) % 4); return;
    case Qt::Key_Backtab: emit channel_requested((channel_ + 3) % 4); return;
    case Qt::Key_Plus:
        set_zoom(col_width_ + std::max(1, col_width_ / 4), grid_rect().center().x());
        return;
    case Qt::Key_Minus:
        set_zoom(col_width_ - std::max(1, col_width_ / 4), grid_rect().center().x());
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}
