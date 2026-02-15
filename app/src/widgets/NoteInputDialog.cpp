#include "widgets/NoteInputDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "audio/TrackerPlaybackEngine.h"

// Note names: English (French)
static const char* kNoteNames[12] = {
    "C  (Do)",  "C# (Do#)", "D  (Re)",  "D# (Re#)",
    "E  (Mi)",  "F  (Fa)",  "F# (Fa#)", "G  (Sol)",
    "G# (Sol#)","A  (La)",  "A# (La#)", "B  (Si)"
};

// Short display names
static const char* kNoteShort[12] = {
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

NoteInputDialog::NoteInputDialog(uint8_t current_note, bool is_noise,
                                 QWidget* parent)
    : QDialog(parent), result_note_(current_note), is_noise_(is_noise)
{
    setWindowTitle(is_noise ? QString::fromUtf8("Choisir un bruit (Noise)")
                           : QString::fromUtf8("Choisir une note"));
    setMinimumWidth(360);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    if (is_noise_) {
        // --- Noise mode: 8 configurations ---
        auto* desc = new QLabel(QString::fromUtf8(
            "Le canal Noise du T6W28 a 8 configurations :\n"
            "- Type : Periodic (P) ou White/Blanc (W)\n"
            "- Rate : High (rapide), Medium, Low (lent), Tone2 (suit le canal Tone 2)"), this);
        desc->setWordWrap(true);
        desc->setStyleSheet(
            "QLabel { background: #1e1e2a; color: #aabbcc; padding: 6px 10px;"
            " border: 1px solid #333; border-radius: 4px; font-size: 11px; }");
        root->addWidget(desc);

        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(QString::fromUtf8("Configuration :"), this));
        noise_combo_ = new QComboBox(this);
        static const char* kNoiseLabels[8] = {
            "P.H \xe2\x80\x93 Periodique, rapide (kick, tom)",
            "P.M \xe2\x80\x93 Periodique, moyen",
            "P.L \xe2\x80\x93 Periodique, lent",
            "P.T \xe2\x80\x93 Periodique, freq. Tone 2",
            "W.H \xe2\x80\x93 Bruit blanc, rapide (hihat, crash)",
            "W.M \xe2\x80\x93 Bruit blanc, moyen (snare)",
            "W.L \xe2\x80\x93 Bruit blanc, lent",
            "W.T \xe2\x80\x93 Bruit blanc, freq. Tone 2"
        };
        for (int i = 0; i < 8; ++i) {
            noise_combo_->addItem(QString::fromUtf8(kNoiseLabels[i]));
        }
        noise_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // Set current from note value
        int cur_idx = 0;
        if (current_note >= 1 && current_note <= 127) {
            cur_idx = (current_note - 1) & 0x07;
        }
        noise_combo_->setCurrentIndex(cur_idx);
        row->addWidget(noise_combo_, 1);
        root->addLayout(row);

        connect(noise_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { update_preview(); });

    } else {
        // --- Tone mode ---
        auto* desc = new QLabel(QString::fromUtf8(
            "Choisissez la note et l'octave.\n"
            "Octave 2 = grave (basse), 4 = medium (melodie), 6 = aigu."), this);
        desc->setWordWrap(true);
        desc->setStyleSheet(
            "QLabel { background: #1e1e2a; color: #aabbcc; padding: 6px 10px;"
            " border: 1px solid #333; border-radius: 4px; font-size: 11px; }");
        root->addWidget(desc);

        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(QString::fromUtf8("Note :"), this));
        note_combo_ = new QComboBox(this);
        for (int i = 0; i < 12; ++i) {
            note_combo_->addItem(QString::fromUtf8(kNoteNames[i]));
        }
        note_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(note_combo_, 1);
        root->addLayout(row);

        auto* row2 = new QHBoxLayout();
        row2->addWidget(new QLabel(QString::fromUtf8("Octave :"), this));
        octave_spin_ = new QSpinBox(this);
        octave_spin_->setRange(0, 9);
        octave_spin_->setValue(4);
        octave_spin_->setToolTip(QString::fromUtf8(
            "0-1 = tres grave, 2-3 = grave, 4-5 = medium, 6-7 = aigu, 8-9 = tres aigu"));
        row2->addWidget(octave_spin_);
        row2->addStretch(1);
        root->addLayout(row2);

        // Initialize from current note
        if (current_note >= 1 && current_note <= 127) {
            int n = current_note - 1;
            note_combo_->setCurrentIndex(n % 12);
            octave_spin_->setValue(n / 12);
        }

        connect(note_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { update_preview(); });
        connect(octave_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { update_preview(); });
    }

    // --- Result preview ---
    result_label_ = new QLabel(this);
    result_label_->setStyleSheet(
        "QLabel { color: #66cccc; font-family: 'Consolas', monospace;"
        " font-size: 16px; font-weight: bold; padding: 6px; }");
    result_label_->setAlignment(Qt::AlignCenter);
    root->addWidget(result_label_);

    // --- Special buttons ---
    auto* special_row = new QHBoxLayout();
    noteoff_btn_ = new QPushButton(QString::fromUtf8("Note OFF (couper le son)"), this);
    noteoff_btn_->setToolTip(QString::fromUtf8(
        "Coupe le son sur ce canal a cette ligne"));
    special_row->addWidget(noteoff_btn_);

    clear_btn_ = new QPushButton(QString::fromUtf8("Effacer"), this);
    clear_btn_->setToolTip(QString::fromUtf8(
        "Supprime la note de cette cellule"));
    special_row->addWidget(clear_btn_);
    special_row->addStretch(1);
    root->addLayout(special_row);

    // --- OK / Cancel ---
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        // Compute result note from UI state
        if (is_noise_) {
            result_note_ = static_cast<uint8_t>(noise_combo_->currentIndex() + 1);
        } else {
            int semi = note_combo_->currentIndex();
            int oct = octave_spin_->value();
            int midi = 1 + oct * 12 + semi;
            result_note_ = static_cast<uint8_t>(std::clamp(midi, 1, 127));
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(noteoff_btn_, &QPushButton::clicked, this, [this]() {
        result_note_ = 0xFF;
        accept();
    });
    connect(clear_btn_, &QPushButton::clicked, this, [this]() {
        result_note_ = 0;
        accept();
    });

    update_preview();
}

void NoteInputDialog::update_preview() {
    if (is_noise_) {
        int idx = noise_combo_->currentIndex();
        uint8_t nv = static_cast<uint8_t>(idx & 0x07);
        result_label_->setText(
            QString::fromUtf8("Apercu : %1  (valeur %2)")
                .arg(TrackerPlaybackEngine::noise_display_name(nv))
                .arg(idx + 1));
    } else {
        int semi = note_combo_->currentIndex();
        int oct = octave_spin_->value();
        int midi = 1 + oct * 12 + semi;
        midi = std::clamp(midi, 1, 127);
        result_label_->setText(
            QString::fromUtf8("Apercu : %1%2  (MIDI %3)")
                .arg(QString::fromLatin1(kNoteShort[semi]))
                .arg(oct)
                .arg(midi));
    }
}
