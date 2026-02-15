#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

class NoteInputDialog : public QDialog
{
    Q_OBJECT

public:
    // is_noise: true for noise channel (shows noise rate instead of note picker)
    explicit NoteInputDialog(uint8_t current_note, bool is_noise,
                             QWidget* parent = nullptr);

    // Result: 0=clear, 0xFF=note-off, 1-127=MIDI note
    uint8_t note() const { return result_note_; }

private:
    uint8_t result_note_ = 0;
    bool is_noise_ = false;

    // Tone mode widgets
    QComboBox* note_combo_ = nullptr;   // C, C#, D, D#, E, F, F#, G, G#, A, A#, B
    QSpinBox* octave_spin_ = nullptr;   // 0-9
    QLabel* result_label_ = nullptr;

    // Noise mode widgets
    QComboBox* noise_combo_ = nullptr;  // 8 configs (P.H..W.T)

    QPushButton* noteoff_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;

    void update_preview();
};
