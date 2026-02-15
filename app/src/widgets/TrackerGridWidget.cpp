#include "widgets/TrackerGridWidget.h"

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>

#include "audio/TrackerPlaybackEngine.h"
#include "i18n/AppLanguage.h"
#include "models/TrackerDocument.h"

const char* TrackerGridWidget::kChannelNames[4] = {"Tone 0", "Tone 1", "Tone 2", "Noise"};

// ============================================================
// Instrument color palette — 16 distinct hues (cycled for instrument ids > 0x0F)
// ============================================================

QColor TrackerGridWidget::instrument_color(uint8_t inst) {
    // HSL palette: 16 distinct hues, high saturation, high lightness
    static const QColor kPalette[16] = {
        QColor(220, 220, 220),  // 00 — white (default)
        QColor(130, 200, 255),  // 01 — light blue
        QColor(255, 180, 100),  // 02 — orange
        QColor(150, 255, 150),  // 03 — green
        QColor(255, 140, 140),  // 04 — red/pink
        QColor(200, 160, 255),  // 05 — purple
        QColor(255, 255, 130),  // 06 — yellow
        QColor(140, 230, 220),  // 07 — cyan
        QColor(255, 160, 210),  // 08 — magenta
        QColor(180, 220, 140),  // 09 — lime
        QColor(200, 190, 170),  // 0A — beige
        QColor(160, 200, 240),  // 0B — sky blue
        QColor(240, 200, 160),  // 0C — peach
        QColor(180, 180, 220),  // 0D — lavender
        QColor(200, 240, 200),  // 0E — mint
        QColor(230, 180, 180),  // 0F — salmon
    };
    return kPalette[inst & 0x0F];
}

// ============================================================
// Construction
// ============================================================

TrackerGridWidget::TrackerGridWidget(TrackerDocument* doc, QWidget* parent)
    : QWidget(parent), doc_(doc)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kHeaderHeight + kRowHeight * 8);
    setMouseTracking(false);

    connect(doc_, &TrackerDocument::document_reset, this, [this]() {
        cursor_row_ = std::clamp(cursor_row_, 0, doc_->length() - 1);
        scroll_offset_ = 0;
        sel_anchor_ = -1;
        update();
    });
    connect(doc_, &TrackerDocument::length_changed, this, [this]() {
        if (cursor_row_ >= doc_->length()) cursor_row_ = doc_->length() - 1;
        update();
    });
    connect(doc_, &TrackerDocument::cell_changed, this, [this](int, int) {
        update();
    });
}

// ============================================================
// set_document — switch pattern
// ============================================================

void TrackerGridWidget::set_document(TrackerDocument* doc) {
    if (doc == doc_) return;

    // Disconnect old
    if (doc_) doc_->disconnect(this);

    doc_ = doc;

    // Reconnect signals
    connect(doc_, &TrackerDocument::document_reset, this, [this]() {
        cursor_row_ = std::clamp(cursor_row_, 0, doc_->length() - 1);
        scroll_offset_ = 0;
        sel_anchor_ = -1;
        update();
    });
    connect(doc_, &TrackerDocument::length_changed, this, [this]() {
        if (cursor_row_ >= doc_->length()) cursor_row_ = doc_->length() - 1;
        update();
    });
    connect(doc_, &TrackerDocument::cell_changed, this, [this](int, int) {
        update();
    });

    // Clamp cursor to new document bounds
    cursor_row_ = std::clamp(cursor_row_, 0, doc_->length() - 1);
    sel_anchor_ = -1;
    sel_ch_start_ = -1;
    sel_ch_end_ = -1;
    sel_anchor_ch_ = -1;
    selected_cells_.clear();
    scroll_offset_ = 0;

    update();
}

// ============================================================
// Cursor
// ============================================================

void TrackerGridWidget::set_cursor(int ch, int row, SubCol sub) {
    cursor_ch_ = std::clamp(ch, 0, TrackerDocument::kChannelCount - 1);
    cursor_row_ = std::clamp(row, 0, doc_->length() - 1);
    cursor_sub_ = sub;
    ensure_row_visible(cursor_row_);
    update();
    emit cursor_moved(cursor_ch_, cursor_row_);
}

void TrackerGridWidget::move_cursor(int d_row, int d_ch, int d_sub) {
    int new_sub = static_cast<int>(cursor_sub_) + d_sub;
    int new_ch = cursor_ch_ + d_ch;
    if (new_sub > 4) { new_sub = 0; new_ch++; }
    if (new_sub < 0) { new_sub = 4; new_ch--; }
    new_ch = std::clamp(new_ch, 0, TrackerDocument::kChannelCount - 1);

    int new_row = cursor_row_ + d_row;
    if (cursor_wrap_) {
        // Wrap around at pattern boundaries
        if (new_row >= doc_->length()) new_row = new_row % doc_->length();
        if (new_row < 0) new_row = doc_->length() + (new_row % doc_->length());
        if (new_row >= doc_->length()) new_row = 0;
    } else {
        new_row = std::clamp(new_row, 0, doc_->length() - 1);
    }
    set_cursor(new_ch, new_row, static_cast<SubCol>(new_sub));
}

// ============================================================
// Selection
// ============================================================

int TrackerGridWidget::sel_start_row() const {
    if (sel_anchor_ < 0) return cursor_row_;
    return std::min(sel_anchor_, cursor_row_);
}

int TrackerGridWidget::sel_end_row() const {
    if (sel_anchor_ < 0) return cursor_row_;
    return std::max(sel_anchor_, cursor_row_);
}

int TrackerGridWidget::sel_start_ch() const {
    if (sel_ch_start_ < 0) return cursor_ch_;
    return std::min(sel_ch_start_, sel_ch_end_);
}

int TrackerGridWidget::sel_end_ch() const {
    if (sel_ch_start_ < 0) return cursor_ch_;
    return std::max(sel_ch_start_, sel_ch_end_);
}

void TrackerGridWidget::clear_selection() {
    if (sel_anchor_ >= 0 || !selected_cells_.empty()) {
        sel_anchor_ = -1;
        sel_anchor_ch_ = -1;
        sel_ch_start_ = -1;
        sel_ch_end_ = -1;
        selected_cells_.clear();
        update();
        emit selection_changed();
    }
}

void TrackerGridWidget::select_all() {
    sel_anchor_ = 0;
    sel_anchor_ch_ = cursor_ch_;
    sel_ch_start_ = -1;
    sel_ch_end_ = -1;
    selected_cells_.clear();
    set_cursor(cursor_ch_, doc_->length() - 1, cursor_sub_);
    emit selection_changed();
}

void TrackerGridWidget::select_all_channels() {
    sel_anchor_ = 0;
    sel_anchor_ch_ = 0;
    sel_ch_start_ = 0;
    sel_ch_end_ = TrackerDocument::kChannelCount - 1;
    selected_cells_.clear();
    set_cursor(cursor_ch_, doc_->length() - 1, cursor_sub_);
    emit selection_changed();
}

bool TrackerGridWidget::is_discrete_selected(int ch, int row) const {
    const int id = row * TrackerDocument::kChannelCount + ch;
    return selected_cells_.find(id) != selected_cells_.end();
}

std::vector<std::pair<int, int>> TrackerGridWidget::selected_cells() const {
    std::vector<std::pair<int, int>> out;
    out.reserve(selected_cells_.size());
    for (int id : selected_cells_) {
        int row = id / TrackerDocument::kChannelCount;
        int ch = id % TrackerDocument::kChannelCount;
        if (row >= 0 && row < doc_->length() && ch >= 0 && ch < TrackerDocument::kChannelCount) {
            out.emplace_back(ch, row);
        }
    }
    return out;
}

// ============================================================
// Playback / Mute
// ============================================================

void TrackerGridWidget::set_playback_row(int row) {
    playback_row_ = row;
    if (row >= 0) ensure_row_visible(row);
    update();
}

void TrackerGridWidget::set_channel_muted(int ch, bool muted) {
    if (ch >= 0 && ch < 4) { channel_muted_[ch] = muted; update(); }
}

bool TrackerGridWidget::is_channel_muted(int ch) const {
    return (ch >= 0 && ch < 4) ? channel_muted_[ch] : false;
}

void TrackerGridWidget::ensure_row_visible(int row) {
    int vis = visible_rows();
    if (vis <= 0) return;

    // Center-scroll: try to keep cursor near the middle
    int margin = vis / 4;
    if (row < scroll_offset_ + margin) {
        scroll_offset_ = std::max(0, row - vis / 2);
    }
    if (row >= scroll_offset_ + vis - margin) {
        scroll_offset_ = std::min(std::max(0, doc_->length() - vis), row - vis / 2);
    }
    scroll_offset_ = std::clamp(scroll_offset_, 0, std::max(0, doc_->length() - vis));
}

// ============================================================
// Geometry / Hit testing
// ============================================================

int TrackerGridWidget::visible_rows() const {
    return (height() - kHeaderHeight) / kRowHeight;
}

int TrackerGridWidget::total_width() const {
    return kRowNumWidth + TrackerDocument::kChannelCount * kCellWidth
           + (TrackerDocument::kChannelCount - 1) * kChannelGap;
}

int TrackerGridWidget::channel_x(int ch) const {
    return kRowNumWidth + ch * (kCellWidth + kChannelGap);
}

int TrackerGridWidget::hit_test_channel(int mx) const {
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        int x = channel_x(ch);
        if (mx >= x && mx < x + kCellWidth) return ch;
    }
    return -1;
}

int TrackerGridWidget::hit_test_row(int my) const {
    if (my < kHeaderHeight) return -1;
    int row = scroll_offset_ + (my - kHeaderHeight) / kRowHeight;
    if (row >= doc_->length()) return -1;
    return row;
}

QSize TrackerGridWidget::sizeHint() const {
    return QSize(total_width(), kHeaderHeight + kRowHeight * 32);
}

// ============================================================
// Paint
// ============================================================

void TrackerGridWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor bgColor(30, 30, 30);
    const QColor bgBeat4(42, 42, 48);
    const QColor bgBeat16(52, 50, 56);
    const QColor headerBg(40, 40, 45);
    const QColor playbackHl(80, 120, 200, 80);
    const QColor cursorHl(200, 180, 60, 100);
    const QColor cursorRecordHl(200, 80, 60, 120);
    const QColor selectionHl(100, 140, 80, 60);
    const QColor noteOffColor(180, 100, 100);
    const QColor emptyColor(55, 55, 60);
    const QColor instColor(100, 200, 100);
    const QColor attnColor(200, 200, 100);
    const QColor rowNumColor(100, 100, 120);
    const QColor rowNumBeatColor(160, 160, 180);
    const QColor rowNum16Color(200, 190, 140);
    const QColor mutedOverlay(0, 0, 0, 120);
    const QColor headerText(180, 180, 200);
    const QColor headerMutedText(100, 100, 110);
    const QColor gridLine(42, 42, 47);
    const QColor beatLine(55, 55, 62);
    const QColor beat16Line(70, 65, 55);
    const QColor scrollTrack(45, 45, 55);
    const QColor scrollThumb(110, 110, 130);
    const QColor volBarBg(35, 35, 40);
    const QColor volBarFg(80, 160, 80, 140);

    const int w = width();
    const int h = height();
    const int trackerRight = std::max(kRowNumWidth, std::min(w, total_width()));
    const int visRows = visible_rows();
    const int selA = sel_start_row();
    const int selB = sel_end_row();

    p.fillRect(rect(), bgColor);

    // Header — two rows: channel name on top, sub-column labels below
    p.fillRect(0, 0, trackerRight, kHeaderHeight, headerBg);
    const int headerRow1H = 22; // channel name row
    const int headerRow2H = kHeaderHeight - headerRow1H; // sub-column labels row

    QFont headerFont = font();
    headerFont.setBold(true);
    headerFont.setPixelSize(12);
    p.setFont(headerFont);
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        int x = channel_x(ch);
        p.setPen(channel_muted_[ch] ? headerMutedText : headerText);
        QString label = QString::fromLatin1(kChannelNames[ch]);
        if (channel_muted_[ch]) label += " [M]";
        p.drawText(QRect(x, 0, kCellWidth, headerRow1H), Qt::AlignCenter, label);
    }
    p.setPen(rowNumColor);
    p.drawText(QRect(0, 0, kRowNumWidth, headerRow1H), Qt::AlignCenter, QStringLiteral("Row"));

    // Sub-column labels
    QFont subFont = font();
    subFont.setPixelSize(10);
    p.setFont(subFont);
    const QColor subLabelColor(120, 120, 140);
    p.setPen(subLabelColor);
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        int x = channel_x(ch);
        int y2 = headerRow1H;
        p.drawText(QRect(x + 2,          y2, kNoteWidth - 2, headerRow2H), Qt::AlignVCenter, QStringLiteral("Note"));
        p.drawText(QRect(x + kNoteWidth,  y2, kInstWidth,     headerRow2H), Qt::AlignVCenter | Qt::AlignHCenter, QStringLiteral("In"));
        p.drawText(QRect(x + kNoteWidth + kInstWidth, y2, kAttnWidth, headerRow2H), Qt::AlignVCenter | Qt::AlignHCenter, QStringLiteral("Vo"));
        p.drawText(QRect(x + kNoteWidth + kInstWidth + kAttnWidth + 2, y2, kFxWidth - 2, headerRow2H), Qt::AlignVCenter, QStringLiteral("FX"));
    }

    // Sub-column separator lines in header row 2
    p.setPen(QColor(38, 38, 43));
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        int x = channel_x(ch);
        p.drawLine(x + kNoteWidth, headerRow1H, x + kNoteWidth, kHeaderHeight);
        p.drawLine(x + kNoteWidth + kInstWidth, headerRow1H, x + kNoteWidth + kInstWidth, kHeaderHeight);
        p.drawLine(x + kNoteWidth + kInstWidth + kAttnWidth, headerRow1H, x + kNoteWidth + kInstWidth + kAttnWidth, kHeaderHeight);
    }

    // Separator line between header rows
    p.setPen(QColor(55, 55, 62));
    p.drawLine(kRowNumWidth, headerRow1H, trackerRight, headerRow1H);

    // Record mode indicator
    if (record_mode_) {
        p.fillRect(0, 0, 3, h, QColor(200, 60, 60));
    }

    // Grid rows
    QFont cellFont = font();
    cellFont.setFamily(QStringLiteral("Consolas"));
    cellFont.setPixelSize(13);
    p.setFont(cellFont);

    for (int vi = 0; vi < visRows && scroll_offset_ + vi < doc_->length(); ++vi) {
        int row = scroll_offset_ + vi;
        int y = kHeaderHeight + vi * kRowHeight;

        // Background — stronger highlight every 16, lighter every 4
        if ((row % 16) == 0) {
            p.fillRect(kRowNumWidth, y, trackerRight - kRowNumWidth, kRowHeight, bgBeat16);
        } else if ((row % 4) == 0) {
            p.fillRect(kRowNumWidth, y, trackerRight - kRowNumWidth, kRowHeight, bgBeat4);
        }

        // Playback row highlight
        if (row == playback_row_) {
            p.fillRect(kRowNumWidth, y, trackerRight - kRowNumWidth, kRowHeight, playbackHl);
        }

        // Selection highlight (supports multi-channel)
        if (has_selection() && row >= selA && row <= selB) {
            if (has_multi_ch_selection()) {
                int sch = sel_start_ch();
                int ech = sel_end_ch();
                for (int sc = sch; sc <= ech; ++sc) {
                    p.fillRect(channel_x(sc), y, kCellWidth, kRowHeight, selectionHl);
                }
            } else {
                int sx = channel_x(cursor_ch_);
                p.fillRect(sx, y, kCellWidth, kRowHeight, selectionHl);
            }
        }
        if (has_discrete_selection()) {
            for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
                if (is_discrete_selected(ch, row)) {
                    p.fillRect(channel_x(ch), y, kCellWidth, kRowHeight, selectionHl);
                }
            }
        }

        // Row number — hex, color by beat
        if ((row % 16) == 0)
            p.setPen(rowNum16Color);
        else if ((row % 4) == 0)
            p.setPen(rowNumBeatColor);
        else
            p.setPen(rowNumColor);
        p.drawText(QRect(0, y, kRowNumWidth - 4, kRowHeight), Qt::AlignVCenter | Qt::AlignRight,
                    QStringLiteral("%1").arg(row, 2, 16, QLatin1Char('0')).toUpper());

        // Cells
        for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
            int x = channel_x(ch);
            const auto& cell = doc_->cell(ch, row);

            // Cursor highlight
            if (row == cursor_row_ && ch == cursor_ch_) {
                int sx = x, sw = kNoteWidth;
                if (cursor_sub_ == SubInst) { sx = x + kNoteWidth; sw = kInstWidth; }
                else if (cursor_sub_ == SubAttn) { sx = x + kNoteWidth + kInstWidth; sw = kAttnWidth; }
                else if (cursor_sub_ == SubFx || cursor_sub_ == SubFxP) { sx = x + kNoteWidth + kInstWidth + kAttnWidth; sw = kFxWidth; }
                p.fillRect(sx, y, sw, kRowHeight, record_mode_ ? cursorRecordHl : cursorHl);
            }

            // Note — colored by instrument, noise channel shows Nxx
            QString noteStr;
            QColor noteCol;
            if (cell.is_empty()) {
                noteStr = QStringLiteral("---");
                noteCol = emptyColor;
            } else if (cell.is_note_off()) {
                noteStr = QStringLiteral("OFF");
                noteCol = noteOffColor;
            } else {
                noteStr = (ch == 3) ? noise_note_name(cell.note) : note_name(cell.note);
                noteCol = (ch == 3) ? QColor(180, 200, 220) : instrument_color(cell.instrument);
            }
            p.setPen(noteCol);
            p.drawText(QRect(x + 2, y, kNoteWidth - 2, kRowHeight), Qt::AlignVCenter, noteStr);

            // Instrument
            if (cell.is_note_on()) {
                p.setPen(instrument_color(cell.instrument).darker(130));
                p.drawText(QRect(x + kNoteWidth, y, kInstWidth, kRowHeight), Qt::AlignVCenter,
                            QStringLiteral("%1").arg(cell.instrument, 2, 16, QLatin1Char('0')).toUpper());
            } else {
                p.setPen(emptyColor);
                p.drawText(QRect(x + kNoteWidth, y, kInstWidth, kRowHeight), Qt::AlignVCenter,
                            QStringLiteral("--"));
            }

            // Attn value + mini volume bar
            int attnX = x + kNoteWidth + kInstWidth;
            if (cell.is_note_on() && cell.attn != 0xFF) {
                // Volume bar background
                int barH = kRowHeight - 6;
                int barY = y + 3;
                int barMaxW = kAttnWidth - 3;
                p.fillRect(attnX + 1, barY, barMaxW, barH, volBarBg);

                // Volume bar fill (attn 0 = full, 15 = silent)
                float vol = 1.0f - (static_cast<float>(cell.attn) / 15.0f);
                int barW = static_cast<int>(vol * barMaxW);
                if (barW > 0) {
                    // Color gradient: green -> yellow -> red
                    int r = static_cast<int>(std::min(1.0f, (1.0f - vol) * 2.0f) * 200);
                    int g = static_cast<int>(std::min(1.0f, vol * 2.0f) * 200);
                    p.fillRect(attnX + 1, barY, barW, barH, QColor(r, g, 60, 160));
                }

                // Attn text on top
                p.setPen(attnColor);
                p.drawText(QRect(attnX, y, kAttnWidth - 2, kRowHeight),
                           Qt::AlignVCenter | Qt::AlignCenter,
                           QStringLiteral("%1").arg(cell.attn, 1, 16).toUpper());
            } else {
                p.setPen(emptyColor);
                p.drawText(QRect(attnX, y, kAttnWidth, kRowHeight),
                           Qt::AlignVCenter | Qt::AlignCenter, QStringLiteral("-"));
            }

            // FX column
            int fxX = x + kNoteWidth + kInstWidth + kAttnWidth;
            if (cell.has_fx()) {
                p.setPen(QColor(100, 200, 200));
                p.drawText(QRect(fxX + 2, y, kFxWidth - 2, kRowHeight),
                           Qt::AlignVCenter, fx_display(cell.fx, cell.fx_param));
            } else {
                p.setPen(emptyColor);
                p.drawText(QRect(fxX + 2, y, kFxWidth - 2, kRowHeight),
                           Qt::AlignVCenter, QStringLiteral("---"));
            }
        }

        // Horizontal grid lines
        if ((row % 16) == 0)
            p.setPen(beat16Line);
        else if ((row % 4) == 0)
            p.setPen(beatLine);
        else
            p.setPen(gridLine);
        p.drawLine(kRowNumWidth, y, trackerRight, y);
    }

    // Vertical separators
    p.setPen(gridLine);
    for (int ch = 0; ch <= TrackerDocument::kChannelCount; ++ch) {
        int x = (ch < TrackerDocument::kChannelCount) ? channel_x(ch) : channel_x(ch - 1) + kCellWidth;
        p.drawLine(x, 0, x, h);
    }
    QColor subLine(38, 38, 43);
    p.setPen(subLine);
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        int x = channel_x(ch);
        p.drawLine(x + kNoteWidth, kHeaderHeight, x + kNoteWidth, h);
        p.drawLine(x + kNoteWidth + kInstWidth, kHeaderHeight, x + kNoteWidth + kInstWidth, h);
        p.drawLine(x + kNoteWidth + kInstWidth + kAttnWidth, kHeaderHeight, x + kNoteWidth + kInstWidth + kAttnWidth, h);
    }

    // Muted overlays
    for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
        if (channel_muted_[ch]) {
            p.fillRect(channel_x(ch), kHeaderHeight, kCellWidth, h - kHeaderHeight, mutedOverlay);
        }
    }

    // Scrollbar (right edge, 8px wide)
    if (doc_->length() > visRows && visRows > 0) {
        int sbw = 8;
        int sbx = trackerRight - sbw;
        int sbh = h - kHeaderHeight;
        p.fillRect(sbx, kHeaderHeight, sbw, sbh, scrollTrack);

        float ratio = static_cast<float>(visRows) / doc_->length();
        int thumbH = std::max(12, static_cast<int>(sbh * ratio));
        float pos = static_cast<float>(scroll_offset_) / std::max(1, doc_->length() - visRows);
        int thumbY = kHeaderHeight + static_cast<int>(pos * (sbh - thumbH));
        p.fillRect(sbx + 1, thumbY, sbw - 2, thumbH, scrollThumb);

        // Playback position indicator on scrollbar
        if (playback_row_ >= 0) {
            float ppos = static_cast<float>(playback_row_) / std::max(1, doc_->length() - 1);
            int py = kHeaderHeight + static_cast<int>(ppos * (sbh - 2));
            p.fillRect(sbx, py, sbw, 2, QColor(80, 120, 200, 200));
        }
    }
}

// ============================================================
// Keyboard
// ============================================================

void TrackerGridWidget::keyPressEvent(QKeyEvent* event) {
    const int key = event->key();
    const auto mod = event->modifiers();
    const bool ctrl = mod & Qt::ControlModifier;
    const bool shift = mod & Qt::ShiftModifier;

    // --- Ctrl shortcuts ---
    if (ctrl) {
        switch (key) {
        case Qt::Key_Z:
            if (shift) emit redo_requested(); else emit undo_requested();
            return;
        case Qt::Key_Y:
            emit redo_requested();
            return;
        case Qt::Key_C:
            if (shift) { emit copy_text_requested(); }
            else { emit copy_requested(); }
            return;
        case Qt::Key_X:
            emit cut_requested();
            return;
        case Qt::Key_V:
            emit paste_requested();
            return;
        case Qt::Key_A:
            if (shift) select_all_channels();
            else emit select_all_requested();
            return;
        case Qt::Key_D:
            emit duplicate_row_requested();
            return;
        case Qt::Key_I:
            emit interpolate_requested();
            return;
        case Qt::Key_H:
            emit humanize_requested();
            return;
        case Qt::Key_B:
            emit batch_apply_requested();
            return;
        case Qt::Key_S:
            emit save_requested();
            return;
        case Qt::Key_O:
            emit load_requested();
            return;
        case Qt::Key_Up:
            emit transpose_requested(shift ? 12 : 1);
            return;
        case Qt::Key_Down:
            emit transpose_requested(shift ? -12 : -1);
            return;
        case Qt::Key_Delete:
            emit clear_pattern_requested();
            return;
        default:
            break;
        }
    }

    // --- Shift+arrows for selection ---
    if (shift && !ctrl) {
        selected_cells_.clear();
        if (sel_anchor_ < 0) sel_anchor_ = cursor_row_;
        if (sel_anchor_ch_ < 0) sel_anchor_ch_ = cursor_ch_;
        switch (key) {
        case Qt::Key_Up:
            move_cursor(-1, 0, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_Down:
            move_cursor(1, 0, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_Left:
            move_cursor(0, -1, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_Right:
            move_cursor(0, 1, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_PageUp:
            move_cursor(-16, 0, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_PageDown:
            move_cursor(16, 0, 0);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_Home:
            set_cursor(cursor_ch_, 0, cursor_sub_);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        case Qt::Key_End:
            set_cursor(cursor_ch_, doc_->length() - 1, cursor_sub_);
            sel_ch_start_ = sel_anchor_ch_;
            sel_ch_end_ = cursor_ch_;
            emit selection_changed();
            return;
        default:
            break;
        }
    }

    // --- Navigation (clears selection) ---
    switch (key) {
    case Qt::Key_Up:
        clear_selection(); move_cursor(-1, 0, 0); return;
    case Qt::Key_Down:
        clear_selection(); move_cursor(1, 0, 0); return;
    case Qt::Key_Left:
        clear_selection(); move_cursor(0, 0, -1); return;
    case Qt::Key_Right:
        clear_selection(); move_cursor(0, 0, 1); return;
    case Qt::Key_Tab:
        clear_selection();
        if (shift) move_cursor(0, -1, 0); else move_cursor(0, 1, 0);
        return;
    case Qt::Key_PageUp:
        clear_selection(); move_cursor(-16, 0, 0); return;
    case Qt::Key_PageDown:
        clear_selection(); move_cursor(16, 0, 0); return;
    case Qt::Key_Home:
        clear_selection(); set_cursor(cursor_ch_, 0, cursor_sub_); return;
    case Qt::Key_End:
        clear_selection(); set_cursor(cursor_ch_, doc_->length() - 1, cursor_sub_); return;
    case Qt::Key_Space:
        emit play_stop_toggled(); return;
    case Qt::Key_F5:
        emit play_from_start_requested(); return;
    case Qt::Key_F8:
        emit stop_requested(); return;
    case Qt::Key_Insert:
        emit insert_row_requested(); return;
    case Qt::Key_Delete:
        if (shift) {
            emit delete_row_requested();
        } else {
            emit cell_cleared(cursor_ch_, cursor_row_);
            move_cursor(edit_step_, 0, 0);
        }
        return;
    case Qt::Key_Backspace:
        emit note_off_entered(cursor_ch_, cursor_row_);
        move_cursor(edit_step_, 0, 0); return;
    case Qt::Key_Escape:
        clear_selection(); return;
    // F1-F4: toggle mute
    case Qt::Key_F1: emit channel_header_clicked(0); return;
    case Qt::Key_F2: emit channel_header_clicked(1); return;
    case Qt::Key_F3: emit channel_header_clicked(2); return;
    case Qt::Key_F4: emit channel_header_clicked(3); return;
    // Numpad +/- for octave change
    case Qt::Key_Plus:
        emit octave_change_requested(1); return;
    case Qt::Key_Minus:
        emit octave_change_requested(-1); return;
    // Numpad * and / for step change
    case Qt::Key_Asterisk:
        emit step_change_requested(1); return;
    case Qt::Key_Slash:
        emit step_change_requested(-1); return;
    default:
        break;
    }

    // --- Enter key: open dialog for current sub-column ---
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        switch (cursor_sub_) {
        case SubNote:
            emit note_dialog_requested(cursor_ch_, cursor_row_);
            return;
        case SubInst:
            emit instrument_dialog_requested(cursor_ch_, cursor_row_);
            return;
        case SubAttn:
            emit attn_dialog_requested(cursor_ch_, cursor_row_);
            return;
        case SubFx:
        case SubFxP:
            emit fx_dialog_requested(cursor_ch_, cursor_row_);
            return;
        }
    }

    // --- Sub-column input (only in record mode) ---
    if (record_mode_) {
        if (cursor_sub_ == SubNote) {
            int note = -1;
            if (cursor_ch_ == 3) {
                note = key_to_noise(key);
            } else {
                note = key_to_note(key, octave_);
            }
            if (note > 0 && note <= 127) {
                clear_selection();
                emit note_entered(cursor_ch_, cursor_row_, static_cast<uint8_t>(note));
                move_cursor(edit_step_, 0, 0);
                return;
            }
        } else if (cursor_sub_ == SubInst) {
            int hex = -1;
            if (key >= Qt::Key_0 && key <= Qt::Key_9) hex = key - Qt::Key_0;
            else if (key >= Qt::Key_A && key <= Qt::Key_F) hex = 10 + (key - Qt::Key_A);
            if (hex >= 0) { emit instrument_digit(cursor_ch_, cursor_row_, hex); return; }
        } else if (cursor_sub_ == SubAttn) {
            int hex = -1;
            if (key >= Qt::Key_0 && key <= Qt::Key_9) hex = key - Qt::Key_0;
            else if (key >= Qt::Key_A && key <= Qt::Key_F) hex = 10 + (key - Qt::Key_A);
            if (hex >= 0) { emit attn_digit(cursor_ch_, cursor_row_, hex); return; }
        } else if (cursor_sub_ == SubFx) {
            // FX command digit (single hex digit 0-F)
            int hex = -1;
            if (key >= Qt::Key_0 && key <= Qt::Key_9) hex = key - Qt::Key_0;
            else if (key >= Qt::Key_A && key <= Qt::Key_F) hex = 10 + (key - Qt::Key_A);
            if (hex >= 0) {
                emit fx_digit(cursor_ch_, cursor_row_, 0, hex);
                cursor_sub_ = SubFxP;  // advance to param
                update();
                return;
            }
        } else if (cursor_sub_ == SubFxP) {
            // FX parameter digits (2 hex digits, shift left like instrument)
            int hex = -1;
            if (key >= Qt::Key_0 && key <= Qt::Key_9) hex = key - Qt::Key_0;
            else if (key >= Qt::Key_A && key <= Qt::Key_F) hex = 10 + (key - Qt::Key_A);
            if (hex >= 0) {
                emit fx_digit(cursor_ch_, cursor_row_, 1, hex);
                return;
            }
        }
    }

    // --- Live preview: play note sound even when REC is off ---
    if (!record_mode_ && cursor_sub_ == SubNote) {
        int note = key_to_note(key, octave_);
        if (note > 0 && note <= 127) {
            emit note_preview_requested(cursor_ch_, static_cast<uint8_t>(note));
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

// ============================================================
// Wheel / Mouse
// ============================================================

void TrackerGridWidget::wheelEvent(QWheelEvent* event) {
    int rows = (event->angleDelta().y() > 0) ? -3 : 3;
    scroll_offset_ = std::clamp(scroll_offset_ + rows, 0,
                                 std::max(0, doc_->length() - visible_rows()));
    update();
}

void TrackerGridWidget::mousePressEvent(QMouseEvent* event) {
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());

    // Scrollbar click — jump to position
    int sbw = 8;
    int sbx = std::max(kRowNumWidth, std::min(width(), total_width())) - sbw;
    if (mx >= sbx && doc_->length() > visible_rows()) {
        int sbh = height() - kHeaderHeight;
        float pos = static_cast<float>(my - kHeaderHeight) / sbh;
        pos = std::clamp(pos, 0.0f, 1.0f);
        scroll_offset_ = static_cast<int>(pos * (doc_->length() - visible_rows()));
        scroll_offset_ = std::clamp(scroll_offset_, 0, std::max(0, doc_->length() - visible_rows()));
        update();
        return;
    }

    // Header click = toggle mute
    if (my < kHeaderHeight && event->button() == Qt::LeftButton) {
        int ch = hit_test_channel(mx);
        if (ch >= 0) {
            emit channel_header_clicked(ch);
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    int row = hit_test_row(my);
    int ch = hit_test_channel(mx);
    if (row < 0 || ch < 0) return;

    SubCol sub = SubNote;
    int lx = mx - channel_x(ch);
    if (lx >= kNoteWidth + kInstWidth + kAttnWidth) sub = SubFx;
    else if (lx >= kNoteWidth + kInstWidth) sub = SubAttn;
    else if (lx >= kNoteWidth) sub = SubInst;

    const bool shift = (event->modifiers() & Qt::ShiftModifier);
    const bool ctrl = (event->modifiers() & Qt::ControlModifier);

    if (shift) {
        selected_cells_.clear();
        if (sel_anchor_ < 0) sel_anchor_ = cursor_row_;
        if (sel_anchor_ch_ < 0) sel_anchor_ch_ = cursor_ch_;
        set_cursor(ch, row, sub);
        sel_ch_start_ = sel_anchor_ch_;
        sel_ch_end_ = ch;
        emit selection_changed();
    } else if (ctrl) {
        // Ctrl-click toggles a single cell in a dispersed selection set.
        sel_anchor_ = -1;
        sel_anchor_ch_ = -1;
        sel_ch_start_ = -1;
        sel_ch_end_ = -1;
        const int id = row * TrackerDocument::kChannelCount + ch;
        auto it = selected_cells_.find(id);
        if (it == selected_cells_.end()) selected_cells_.insert(id);
        else selected_cells_.erase(it);
        set_cursor(ch, row, sub);
        emit selection_changed();
    } else {
        clear_selection();
        sel_anchor_ = row;
        sel_anchor_ch_ = ch;
        sel_ch_start_ = ch;
        sel_ch_end_ = ch;
        dragging_ = true;
        set_cursor(ch, row, sub);
    }
    setFocus();
}

void TrackerGridWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;

    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());
    int row = hit_test_row(my);
    int ch = hit_test_channel(mx);
    if (row < 0) {
        if (my < kHeaderHeight) {
            scroll_offset_ = std::max(0, scroll_offset_ - 1);
            row = scroll_offset_;
        } else {
            scroll_offset_ = std::min(std::max(0, doc_->length() - visible_rows()),
                                       scroll_offset_ + 1);
            row = std::min(doc_->length() - 1, scroll_offset_ + visible_rows() - 1);
        }
    }
    if (ch < 0) ch = cursor_ch_;

    if (row != cursor_row_ || ch != cursor_ch_) {
        selected_cells_.clear();
        set_cursor(ch, row, cursor_sub_);
        sel_ch_start_ = sel_anchor_ch_;
        sel_ch_end_ = ch;
        emit selection_changed();
    }
}

void TrackerGridWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (dragging_) {
            dragging_ = false;
            if (sel_anchor_ == cursor_row_ && sel_anchor_ch_ == cursor_ch_) {
                sel_anchor_ = -1;
                sel_anchor_ch_ = -1;
                sel_ch_start_ = -1;
                sel_ch_end_ = -1;
            }
        }
    }
}

void TrackerGridWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());
    if (my < kHeaderHeight) return;

    int ch = hit_test_channel(mx);
    if (ch < 0) return;

    int row = hit_test_row(my);
    if (row < 0) return;

    // Determine sub-column and open corresponding dialog
    int lx = mx - channel_x(ch);
    if (lx >= kNoteWidth + kInstWidth + kAttnWidth) {
        set_cursor(ch, row, SubFx);
        emit fx_dialog_requested(ch, row);
        return;
    }
    if (lx >= kNoteWidth + kInstWidth) {
        set_cursor(ch, row, SubAttn);
        emit attn_dialog_requested(ch, row);
        return;
    }
    if (lx >= kNoteWidth) {
        set_cursor(ch, row, SubInst);
        emit instrument_dialog_requested(ch, row);
        return;
    }
    // Note column
    set_cursor(ch, row, SubNote);
    emit note_dialog_requested(ch, row);
}

void TrackerGridWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ensure_row_visible(cursor_row_);
}

// ============================================================
// Context menu
// ============================================================

void TrackerGridWidget::contextMenuEvent(QContextMenuEvent* event) {
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };

    QMenu menu(this);

    bool hasSel = has_selection();
    const QString selText = hasSel
        ? QString(" (%1 %2)")
              .arg(sel_end_row() - sel_start_row() + 1)
              .arg(ui("lignes", "rows"))
        : QString();

    auto* cutAct = menu.addAction(QString("%1%2\tCtrl+X").arg(ui("Couper", "Cut"), selText));
    auto* copyAct = menu.addAction(QString("%1%2\tCtrl+C").arg(ui("Copier", "Copy"), selText));
    auto* pasteAct = menu.addAction(QString("%1\tCtrl+V").arg(ui("Coller", "Paste")));
    menu.addSeparator();

    auto* copyTextAct = menu.addAction(QString("%1\tCtrl+Shift+C").arg(ui("Copier en texte", "Copy as text")));
    menu.addSeparator();

    auto* selAllAct = menu.addAction(QString("%1\tCtrl+A").arg(ui("Tout selectionner (canal)", "Select all (channel)")));
    auto* selAllChAct = menu.addAction(QString("%1\tCtrl+Shift+A").arg(ui("Tout selectionner (tous canaux)", "Select all (all channels)")));
    menu.addSeparator();

    auto* insertAct = menu.addAction(QString("%1\tIns").arg(ui("Inserer ligne", "Insert row")));
    auto* deleteRowAct = menu.addAction(QString("%1\tShift+Del").arg(ui("Supprimer ligne", "Delete row")));
    auto* dupAct = menu.addAction(QString("%1\tCtrl+D").arg(ui("Dupliquer ligne", "Duplicate row")));
    menu.addSeparator();

    auto* transpUpAct = menu.addAction(QString("%1\tCtrl+Up").arg(ui("Transpose +1", "Transpose +1")));
    auto* transpDownAct = menu.addAction(QString("%1\tCtrl+Down").arg(ui("Transpose -1", "Transpose -1")));
    auto* transpOctUpAct = menu.addAction(QString("%1\tCtrl+Shift+Up").arg(ui("Transpose +12", "Transpose +12")));
    auto* transpOctDownAct = menu.addAction(QString("%1\tCtrl+Shift+Down").arg(ui("Transpose -12", "Transpose -12")));
    menu.addSeparator();

    auto* interpAct = menu.addAction(QString("%1\tCtrl+I").arg(ui("Interpoler colonne", "Interpolate field")));
    interpAct->setEnabled(hasSel || has_discrete_selection());
    auto* humanizeAct = menu.addAction(QString("%1\tCtrl+H").arg(ui("Humanize attenuation", "Humanize attenuation")));
    auto* batchApplyAct = menu.addAction(QString("%1\tCtrl+B").arg(ui("Batch apply colonne", "Batch apply field")));
    menu.addSeparator();

    auto* saveAct = menu.addAction(QString("%1\tCtrl+S").arg(ui("Sauver pattern", "Save pattern")));
    auto* loadAct = menu.addAction(QString("%1\tCtrl+O").arg(ui("Charger pattern", "Load pattern")));
    menu.addSeparator();

    auto* clearAct = menu.addAction(QString("%1\tCtrl+Del").arg(ui("Effacer pattern", "Clear pattern")));

    auto* chosen = menu.exec(event->globalPos());
    if (!chosen) return;

    if (chosen == cutAct)          emit cut_requested();
    else if (chosen == copyAct)    emit copy_requested();
    else if (chosen == pasteAct)   emit paste_requested();
    else if (chosen == copyTextAct) emit copy_text_requested();
    else if (chosen == selAllAct)  emit select_all_requested();
    else if (chosen == selAllChAct) select_all_channels();
    else if (chosen == insertAct)  emit insert_row_requested();
    else if (chosen == deleteRowAct) emit delete_row_requested();
    else if (chosen == dupAct)     emit duplicate_row_requested();
    else if (chosen == transpUpAct) emit transpose_requested(1);
    else if (chosen == transpDownAct) emit transpose_requested(-1);
    else if (chosen == transpOctUpAct) emit transpose_requested(12);
    else if (chosen == transpOctDownAct) emit transpose_requested(-12);
    else if (chosen == interpAct)  emit interpolate_requested();
    else if (chosen == humanizeAct) emit humanize_requested();
    else if (chosen == batchApplyAct) emit batch_apply_requested();
    else if (chosen == saveAct)    emit save_requested();
    else if (chosen == loadAct)    emit load_requested();
    else if (chosen == clearAct)   emit clear_pattern_requested();
}

// ============================================================
// Helpers
// ============================================================

QString TrackerGridWidget::note_name(uint8_t midi_note) {
    static const char* kNames[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    if (midi_note < 1 || midi_note > 127) return QStringLiteral("???");
    int n = midi_note - 1;
    return QStringLiteral("%1%2").arg(QString::fromLatin1(kNames[n % 12])).arg(n / 12);
}

QString TrackerGridWidget::noise_note_name(uint8_t midi_note) {
    if (midi_note < 1 || midi_note > 127) return QStringLiteral("???");
    uint8_t nv = static_cast<uint8_t>((midi_note - 1) & 0x07);
    return TrackerPlaybackEngine::noise_display_name(nv);
}

QString TrackerGridWidget::cell_display_note(int ch, const TrackerCell& cell) const {
    if (cell.is_empty() && !cell.has_fx()) return QStringLiteral("---");
    if (cell.is_note_off()) return QStringLiteral("OFF");
    if (cell.note == 0) return QStringLiteral("---");
    return (ch == 3) ? noise_note_name(cell.note) : note_name(cell.note);
}

QString TrackerGridWidget::fx_display(uint8_t fx, uint8_t fx_param) {
    if (fx == 0 && fx_param == 0) return QStringLiteral("---");
    return QStringLiteral("%1%2")
        .arg(fx, 1, 16, QLatin1Char('0'))
        .arg(fx_param, 2, 16, QLatin1Char('0'))
        .toUpper();
}

// ============================================================
// Text export (selection or full pattern to formatted text)
// ============================================================

QString TrackerGridWidget::selection_to_text() const {
    int r_start = 0, r_end = doc_->length() - 1;
    int ch_start = 0, ch_end = TrackerDocument::kChannelCount - 1;

    if (has_selection()) {
        r_start = sel_start_row();
        r_end = sel_end_row();
        if (has_multi_ch_selection()) {
            ch_start = sel_start_ch();
            ch_end = sel_end_ch();
        } else {
            ch_start = cursor_ch_;
            ch_end = cursor_ch_;
        }
    }

    QString text;
    // Header
    text += QStringLiteral("Row ");
    for (int ch = ch_start; ch <= ch_end; ++ch) {
        text += QStringLiteral("| %1        ").arg(QString::fromLatin1(kChannelNames[ch]), -8);
    }
    text += QStringLiteral("\n");
    text += QStringLiteral("----");
    for (int ch = ch_start; ch <= ch_end; ++ch) {
        (void)ch;
        text += QStringLiteral("+--------------");
    }
    text += QStringLiteral("\n");

    // Rows
    for (int r = r_start; r <= r_end; ++r) {
        text += QStringLiteral("%1  ").arg(r, 2, 16, QLatin1Char('0')).toUpper();
        for (int ch = ch_start; ch <= ch_end; ++ch) {
            const auto& c = doc_->cell(ch, r);
            QString n = cell_display_note(ch, c);
            QString inst = c.is_note_on()
                ? QStringLiteral("%1").arg(c.instrument, 2, 16, QLatin1Char('0')).toUpper()
                : QStringLiteral("--");
            QString attn = (c.is_note_on() && c.attn != 0xFF)
                ? QStringLiteral("%1").arg(c.attn, 1, 16).toUpper()
                : QStringLiteral("-");
            QString fx = c.has_fx()
                ? fx_display(c.fx, c.fx_param)
                : QStringLiteral("---");
            text += QStringLiteral("| %1 %2 %3 %4 ").arg(n, -3).arg(inst).arg(attn).arg(fx);
        }
        text += QStringLiteral("\n");
    }

    return text;
}

int TrackerGridWidget::key_to_note(int qt_key, int octave) const {
    if (key_layout_ == LayoutAZERTY)
        return key_to_note_azerty(qt_key, octave);
    return key_to_note_qwerty(qt_key, octave);
}

static int semi_to_midi(int semi, int octave, int oct_offset) {
    int midi = 1 + (octave + oct_offset) * 12 + semi;
    return std::clamp(midi, 1, 127);
}

int TrackerGridWidget::key_to_note_qwerty(int qt_key, int octave) {
    int semi = -1, oct_offset = 0;
    switch (qt_key) {
    case Qt::Key_Z: semi = 0; break;  case Qt::Key_S: semi = 1; break;
    case Qt::Key_X: semi = 2; break;  case Qt::Key_D: semi = 3; break;
    case Qt::Key_C: semi = 4; break;  case Qt::Key_V: semi = 5; break;
    case Qt::Key_G: semi = 6; break;  case Qt::Key_B: semi = 7; break;
    case Qt::Key_H: semi = 8; break;  case Qt::Key_N: semi = 9; break;
    case Qt::Key_J: semi = 10; break; case Qt::Key_M: semi = 11; break;
    case Qt::Key_Q: semi = 0; oct_offset = 1; break;
    case Qt::Key_2: semi = 1; oct_offset = 1; break;
    case Qt::Key_W: semi = 2; oct_offset = 1; break;
    case Qt::Key_3: semi = 3; oct_offset = 1; break;
    case Qt::Key_E: semi = 4; oct_offset = 1; break;
    case Qt::Key_R: semi = 5; oct_offset = 1; break;
    case Qt::Key_5: semi = 6; oct_offset = 1; break;
    case Qt::Key_T: semi = 7; oct_offset = 1; break;
    case Qt::Key_6: semi = 8; oct_offset = 1; break;
    case Qt::Key_Y: semi = 9; oct_offset = 1; break;
    case Qt::Key_7: semi = 10; oct_offset = 1; break;
    case Qt::Key_U: semi = 11; oct_offset = 1; break;
    default: return -1;
    }
    return semi_to_midi(semi, octave, oct_offset);
}

int TrackerGridWidget::key_to_note_azerty(int qt_key, int octave) {
    int semi = -1, oct_offset = 0;
    switch (qt_key) {
    case Qt::Key_W:     semi = 0; break;
    case Qt::Key_S:     semi = 1; break;
    case Qt::Key_X:     semi = 2; break;
    case Qt::Key_D:     semi = 3; break;
    case Qt::Key_C:     semi = 4; break;
    case Qt::Key_V:     semi = 5; break;
    case Qt::Key_G:     semi = 6; break;
    case Qt::Key_B:     semi = 7; break;
    case Qt::Key_H:     semi = 8; break;
    case Qt::Key_N:     semi = 9; break;
    case Qt::Key_J:     semi = 10; break;
    case Qt::Key_Comma: semi = 11; break;
    case Qt::Key_A:     semi = 0; oct_offset = 1; break;
    case Qt::Key_2:     semi = 1; oct_offset = 1; break;
    case Qt::Key_Z:     semi = 2; oct_offset = 1; break;
    case Qt::Key_3:     semi = 3; oct_offset = 1; break;
    case Qt::Key_E:     semi = 4; oct_offset = 1; break;
    case Qt::Key_R:     semi = 5; oct_offset = 1; break;
    case Qt::Key_5:     semi = 6; oct_offset = 1; break;
    case Qt::Key_T:     semi = 7; oct_offset = 1; break;
    case Qt::Key_6:     semi = 8; oct_offset = 1; break;
    case Qt::Key_Y:     semi = 9; oct_offset = 1; break;
    case Qt::Key_7:     semi = 10; oct_offset = 1; break;
    case Qt::Key_U:     semi = 11; oct_offset = 1; break;
    default: return -1;
    }
    return semi_to_midi(semi, octave, oct_offset);
}

// ============================================================
// Noise channel input: 8 keys -> noise configs 1-8
// Row 1 = Periodic (P.H P.M P.L P.T), Row 2 = White (W.H W.M W.L W.T)
// ============================================================

int TrackerGridWidget::key_to_noise(int qt_key) const {
    if (key_layout_ == LayoutAZERTY)
        return key_to_noise_azerty(qt_key);
    return key_to_noise_qwerty(qt_key);
}

int TrackerGridWidget::key_to_noise_qwerty(int qt_key) {
    switch (qt_key) {
    // Bottom row: Periodic
    case Qt::Key_Z: return 1;  // P.H
    case Qt::Key_S: return 2;  // P.M
    case Qt::Key_X: return 3;  // P.L
    case Qt::Key_D: return 4;  // P.T
    // Top row: White
    case Qt::Key_Q: return 5;  // W.H
    case Qt::Key_2: return 6;  // W.M
    case Qt::Key_W: return 7;  // W.L
    case Qt::Key_3: return 8;  // W.T
    default: return -1;
    }
}

int TrackerGridWidget::key_to_noise_azerty(int qt_key) {
    switch (qt_key) {
    // Bottom row: Periodic
    case Qt::Key_W: return 1;  // P.H
    case Qt::Key_S: return 2;  // P.M
    case Qt::Key_X: return 3;  // P.L
    case Qt::Key_D: return 4;  // P.T
    // Top row: White
    case Qt::Key_A: return 5;  // W.H
    case Qt::Key_Z: return 6;  // W.M
    case Qt::Key_E: return 7;  // W.L
    case Qt::Key_R: return 8;  // W.T
    default: return -1;
    }
}
