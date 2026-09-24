#pragma once

#include <QWidget>

#include <array>
#include <cstdint>
#include <set>
#include <vector>

#include "models/NoteSpans.h"

class QScrollBar;
class TrackerDocument;

// ============================================================
// PianoRollWidget — alternative view of a pattern: time runs left to
// right (one column per tracker row), pitch bottom to top.
//
// It shows one channel at a time, the three other voices drawn as ghosts.
// The noise channel has no pitch: it gets one lane per noise timbre.
// A volume lane under the notes edits each note's attenuation.
// The view mirrors the tracker grid (document, cursor row, playback row,
// mutes); the grid stays the source of truth.
// ============================================================

class PianoRollWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kKeyboardWidth = 64;
    static constexpr int kRulerHeight   = 22;
    static constexpr int kVolumeHeight  = 56;
    static constexpr int kVolumeGap     = 4;
    static constexpr int kToneKeyHeight = 12;
    static constexpr int kNoiseLaneHeight = 30;
    static constexpr int kNoiseLanes    = 8;
    static constexpr int kMinColWidth   = 6;
    static constexpr int kMinFitColWidth = 10;
    static constexpr int kMaxColWidth   = 48;
    static constexpr int kDefaultColWidth = 16;
    static constexpr uint8_t kLowestNote  = 1;    // C-0
    static constexpr uint8_t kHighestNote = 127;  // F#10, top of the tracker range

    explicit PianoRollWidget(TrackerDocument* doc, QWidget* parent = nullptr);

    void set_document(TrackerDocument* doc);
    TrackerDocument* document() const { return doc_; }

    int channel() const { return channel_; }
    void set_channel(int ch);

    void set_cursor_row(int row);
    int cursor_row() const { return cursor_row_; }

    void set_playback_row(int row);
    void set_channel_muted(int ch, bool muted);

    // Scroll so that the notes of the current channel are in view.
    void center_on_notes();

signals:
    void cursor_row_clicked(int row);
    void channel_requested(int ch);
    void note_preview_requested(int ch, uint8_t note);
    void instrument_dialog_requested(int ch, int row);
    void play_stop_toggled();
    void play_from_start_requested();
    void stop_requested();
    void mute_toggle_requested(int ch);
    void save_requested();
    void load_requested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    QSize sizeHint() const override;

private:
    TrackerDocument* doc_ = nullptr;
    QScrollBar* hscroll_ = nullptr;
    QScrollBar* vscroll_ = nullptr;

    int channel_ = 0;
    int cursor_row_ = 0;
    int playback_row_ = -1;
    int col_width_ = kDefaultColWidth;
    bool muted_[4] = {};
    int hover_lane_ = -1;   // lane under the mouse on the keyboard, -1 = none

    // --- Mouse editing ---
    enum class Drag { None, Create, Move, Resize, Erase, Select, Volume, VolumeReset };
    Drag drag_ = Drag::None;
    NoteSpan drag_cur_;                 // Create / Resize: bar written on release
    NoteSpan drag_orig_;                // Resize: bar as it was
    std::vector<NoteSpan> drag_group_;  // Move: bars as they were when grabbed
    NoteSpan drag_grabbed_;             // Move: the bar under the mouse
    int drag_row_ = 0;                  // Move: row / lane of the grab point
    int drag_lane_ = 0;
    int drag_d_row_ = 0;                // Move: current offset
    int drag_d_lane_ = 0;
    bool drag_left_start_row_ = false;  // Create: the mouse left the first row, length follows it
    bool gesture_undo_pushed_ = false;  // Erase / Volume: one snapshot per gesture
    QPoint last_pos_;                   // Erase / Volume: previous mouse position
    QPoint select_from_;                // Select: rubber band corners
    QPoint select_to_;

    std::set<int> selection_;           // start rows of the selected bars
    std::vector<NoteSpan> clipboard_;   // bars copied, starts relative to the first one
    int new_note_length_ = 4;           // rows; follows the last bar drawn or resized
    // Instrument/attn given to new bars, per channel (-1 = not chosen yet)
    std::array<int, 4> brush_inst_{{-1, -1, -1, -1}};
    std::array<int, 4> brush_attn_{{-1, -1, -1, -1}};

    void connect_document();
    bool is_noise() const { return channel_ == 3; }

    // Vertical lanes: 0 = top. Tone: one lane per note, highest first.
    int lane_count() const;
    int lane_height() const;
    int lane_of_note(uint8_t note) const;   // -1 if out of range
    uint8_t note_of_lane(int lane) const;   // tracker note id

    QRect grid_rect() const;
    QRect volume_rect() const;
    int row_x(int row) const;               // widget x of a row's left edge
    int lane_y(int lane) const;             // widget y of a lane's top edge
    int hit_row(int x) const;               // -1 outside the pattern
    int hit_lane(int y) const;              // -1 outside the lanes
    int row_at_clamped(int x) const;
    int lane_at_clamped(int y) const;

    void update_scrollbars();
    void ensure_row_visible(int row);
    void set_zoom(int col_width, int anchor_x);
    void fit_width();                       // columns fill the view (within zoom limits)
    QString lane_label(int lane) const;

    QRect bar_rect(const NoteSpan& s) const;
    int hit_span(const QPoint& pos, const std::vector<NoteSpan>& spans) const; // index or -1
    bool on_resize_edge(const QPoint& pos, const NoteSpan& s) const;
    void pick_brush(const NoteSpan& s);
    NoteSpan new_span(int row, int lane) const;

    std::vector<NoteSpan> selected_spans() const;
    std::vector<NoteSpan> shifted(const std::vector<NoteSpan>& group, int d_row, int d_lane) const;
    void clamp_shift(const std::vector<NoteSpan>& group, int& d_row, int& d_lane) const;
    void apply_group(const std::vector<NoteSpan>& from, const std::vector<NoteSpan>& to);
    void transpose_selection(int d_lane);
    void copy_selection();
    void cut_selection();
    void paste_at_cursor();
    void delete_selection();

    void commit_drag(const QPoint& pos);
    void cancel_drag();
    void erase_at(const QPoint& pos);
    void set_volume_at(const QPoint& pos, bool reset);
    void update_hover_cursor(const QPoint& pos);
    void paint_volume_lane(QPainter& p, const std::vector<NoteSpan>& spans);
};
