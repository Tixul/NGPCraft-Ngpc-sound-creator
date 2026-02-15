#pragma once

#include <QWidget>

#include <set>
#include <utility>
#include <vector>

class TrackerDocument;
struct TrackerCell;

class TrackerGridWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kRowHeight      = 20;
    static constexpr int kRowNumWidth    = 36;
    static constexpr int kNoteWidth      = 42;
    static constexpr int kInstWidth      = 24;
    static constexpr int kAttnWidth      = 24;
    static constexpr int kFxWidth        = 36;
    static constexpr int kCellWidth      = kNoteWidth + kInstWidth + kAttnWidth + kFxWidth;
    static constexpr int kChannelGap     = 2;
    static constexpr int kHeaderHeight   = 42;

    enum SubCol { SubNote = 0, SubInst = 1, SubAttn = 2, SubFx = 3, SubFxP = 4 };
    enum KeyLayout { LayoutQWERTY = 0, LayoutAZERTY = 1 };

    explicit TrackerGridWidget(TrackerDocument* doc, QWidget* parent = nullptr);

    // Switch the underlying document (pattern switching)
    void set_document(TrackerDocument* doc);
    TrackerDocument* document() const { return doc_; }

    // Cursor
    int cursor_row() const { return cursor_row_; }
    int cursor_ch() const  { return cursor_ch_; }
    SubCol cursor_sub() const { return cursor_sub_; }
    void set_cursor(int ch, int row, SubCol sub);
    void move_cursor(int d_row, int d_ch, int d_sub);

    // Selection
    bool has_selection() const { return sel_anchor_ >= 0; }
    bool has_multi_ch_selection() const { return sel_ch_start_ >= 0; }
    bool has_discrete_selection() const { return !selected_cells_.empty(); }
    std::vector<std::pair<int, int>> selected_cells() const;
    int sel_start_row() const;
    int sel_end_row() const;
    int sel_start_ch() const;
    int sel_end_ch() const;
    void clear_selection();
    void select_all();          // current channel
    void select_all_channels(); // all channels

    // Text export
    QString selection_to_text() const;

    // Playback
    void set_playback_row(int row);
    int playback_row() const { return playback_row_; }

    // Settings
    void set_edit_step(int step) { edit_step_ = step; }
    int edit_step() const { return edit_step_; }
    void set_octave(int oct) { octave_ = oct; }
    int octave() const { return octave_; }
    void set_key_layout(KeyLayout layout) { key_layout_ = layout; }
    KeyLayout key_layout() const { return key_layout_; }
    void set_cursor_wrap(bool on) { cursor_wrap_ = on; }
    bool cursor_wrap() const { return cursor_wrap_; }

    // Record mode
    void set_record_mode(bool on) { record_mode_ = on; update(); }
    bool record_mode() const { return record_mode_; }

    // Mute
    void set_channel_muted(int ch, bool muted);
    bool is_channel_muted(int ch) const;

    void ensure_row_visible(int row);

signals:
    void cursor_moved(int ch, int row);
    void note_entered(int ch, int row, uint8_t note);
    void note_preview_requested(int ch, uint8_t note);
    void note_off_entered(int ch, int row);
    void cell_cleared(int ch, int row);
    void instrument_digit(int ch, int row, int hex_digit);
    void attn_digit(int ch, int row, int hex_digit);
    void fx_digit(int ch, int row, int col_index, int hex_digit); // col_index: 0=cmd, 1=param_hi, 2=param_lo
    void fx_dialog_requested(int ch, int row);
    void note_dialog_requested(int ch, int row);
    void instrument_dialog_requested(int ch, int row);
    void attn_dialog_requested(int ch, int row);
    void play_stop_toggled();
    // Editing signals
    void undo_requested();
    void redo_requested();
    void copy_requested();
    void cut_requested();
    void paste_requested();
    void select_all_requested();
    void transpose_requested(int semitones);
    void play_from_start_requested();
    void stop_requested();
    void clear_pattern_requested();
    void selection_changed();
    // Row operations
    void insert_row_requested();
    void delete_row_requested();
    void duplicate_row_requested();
    void interpolate_requested();
    void humanize_requested();
    void batch_apply_requested();
    // Header click
    void channel_header_clicked(int ch);
    // File / settings signals
    void save_requested();
    void load_requested();
    void octave_change_requested(int delta);
    void step_change_requested(int delta);
    void copy_text_requested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    QSize sizeHint() const override;

private:
    TrackerDocument* doc_ = nullptr;

    int cursor_row_  = 0;
    int cursor_ch_   = 0;
    SubCol cursor_sub_ = SubNote;
    int playback_row_ = -1;
    int scroll_offset_ = 0;
    int edit_step_ = 1;
    int octave_ = 4;
    KeyLayout key_layout_ = LayoutQWERTY;
    bool channel_muted_[4] = {};
    bool record_mode_ = true;
    bool cursor_wrap_ = true;

    // Selection
    int sel_anchor_ = -1;   // -1 = no selection
    int sel_anchor_ch_ = -1;
    int sel_ch_start_ = -1; // -1 = single channel, else start ch
    int sel_ch_end_ = -1;   // -1 = single channel, else end ch
    std::set<int> selected_cells_; // discrete selection (row*kChannelCount + ch)

    // Mouse drag
    bool dragging_ = false;

    bool is_discrete_selected(int ch, int row) const;

    // Instrument color palette (16 hues, cycled by instrument id)
    static QColor instrument_color(uint8_t inst);

    int visible_rows() const;
    int total_width() const;
    int channel_x(int ch) const;
    int hit_test_channel(int mx) const;
    int hit_test_row(int my) const;

    static QString note_name(uint8_t midi_note);
    static QString noise_note_name(uint8_t midi_note);
    QString cell_display_note(int ch, const TrackerCell& cell) const;
    static QString fx_display(uint8_t fx, uint8_t fx_param);
    static int key_to_note_qwerty(int qt_key, int octave);
    static int key_to_note_azerty(int qt_key, int octave);
    int key_to_note(int qt_key, int octave) const;
    int key_to_noise(int qt_key) const;
    static int key_to_noise_qwerty(int qt_key);
    static int key_to_noise_azerty(int qt_key);

    static const char* kChannelNames[4];
};
