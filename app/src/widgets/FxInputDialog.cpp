#include "widgets/FxInputDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ============================================================
// FX combo mapping: index -> fx code
// ============================================================

// 0=Aucun, 1=Arpege(0), 2=PitchUp(1), 3=PitchDown(2),
// 4=Porta(3), 5=PitchBend(4), 6=VolSlide(A), 7=Speed(B),
// 8=NoteCut(C), 9=NoteDelay(D), 10=HostCmd(E), 11=Expression(F)

uint8_t FxInputDialog::combo_to_fx(int index) {
    switch (index) {
    case 0: return 0x0;  // Aucun
    case 1: return 0x0;  // Arpege (param != 0)
    case 2: return 0x1;  // Pitch slide up
    case 3: return 0x2;  // Pitch slide down
    case 4: return 0x3;  // Portamento
    case 5: return 0x4;  // Pitch bend
    case 6: return 0xA;  // Volume slide
    case 7: return 0xB;  // Set speed
    case 8: return 0xC;  // Note cut
    case 9: return 0xD;  // Note delay
    case 10: return 0xE; // Host command
    case 11: return 0xF; // Expression
    default: return 0x0;
    }
}

int FxInputDialog::fx_to_combo(uint8_t fx) {
    switch (fx) {
    case 0x0: return 1;  // Could be arpege or aucun — caller distinguishes by param
    case 0x1: return 2;
    case 0x2: return 3;
    case 0x3: return 4;
    case 0x4: return 5;
    case 0xA: return 6;
    case 0xB: return 7;
    case 0xC: return 8;
    case 0xD: return 9;
    case 0xE: return 10;
    case 0xF: return 11;
    default:  return 0;
    }
}

// ============================================================
// Construction
// ============================================================

FxInputDialog::FxInputDialog(uint8_t current_fx, uint8_t current_param,
                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("Choisir un effet"));
    setMinimumWidth(380);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    // --- Effect combo ---
    auto* combo_row = new QHBoxLayout();
    combo_row->addWidget(new QLabel(QString::fromUtf8("Effet :"), this));
    fx_combo_ = new QComboBox(this);
    fx_combo_->addItem(QString::fromUtf8("Aucun (supprimer l'effet)"));
    fx_combo_->addItem(QString::fromUtf8("Arp\u00e8ge (0xy) \u2014 accord rapide"));
    fx_combo_->addItem(QString::fromUtf8("Pitch slide haut (1xx) \u2014 monte la fr\u00e9quence"));
    fx_combo_->addItem(QString::fromUtf8("Pitch slide bas (2xx) \u2014 baisse la fr\u00e9quence"));
    fx_combo_->addItem(QString::fromUtf8("Portamento (3xx) \u2014 glisse vers la note"));
    fx_combo_->addItem(QString::fromUtf8("Pitch bend (4xx) \u2014 d\u00e9cale la fr\u00e9quence"));
    fx_combo_->addItem(QString::fromUtf8("Volume slide (Axy) \u2014 monte/baisse le volume"));
    fx_combo_->addItem(QString::fromUtf8("Changer vitesse (Bxx) \u2014 modifie le TPR"));
    fx_combo_->addItem(QString::fromUtf8("Couper note (Cxx) \u2014 coupe apr\u00e8s x ticks"));
    fx_combo_->addItem(QString::fromUtf8("Retarder note (Dxx) \u2014 d\u00e9clenche apr\u00e8s x ticks"));
    fx_combo_->addItem(QString::fromUtf8("Host command (Exy) \u2014 fade out / tempo"));
    fx_combo_->addItem(QString::fromUtf8("Expression (Fxx) \u2014 offset volume persistant"));
    fx_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    combo_row->addWidget(fx_combo_, 1);
    root->addLayout(combo_row);

    // --- Description ---
    desc_label_ = new QLabel(this);
    desc_label_->setWordWrap(true);
    desc_label_->setStyleSheet(
        "QLabel { background: #1e1e2a; color: #aabbcc; padding: 6px 10px;"
        " border: 1px solid #333; border-radius: 4px; font-size: 11px; }");
    root->addWidget(desc_label_);

    // --- Parameters ---
    auto* param_row1 = new QHBoxLayout();
    param1_label_ = new QLabel(this);
    param1_spin_ = new QSpinBox(this);
    param1_spin_->setMinimumWidth(80);
    param_row1->addWidget(param1_label_);
    param_row1->addWidget(param1_spin_);
    param_row1->addStretch(1);
    root->addLayout(param_row1);

    auto* param_row2 = new QHBoxLayout();
    param2_label_ = new QLabel(this);
    param2_spin_ = new QSpinBox(this);
    param2_spin_->setMinimumWidth(80);
    param_row2->addWidget(param2_label_);
    param_row2->addWidget(param2_spin_);
    param_row2->addStretch(1);
    root->addLayout(param_row2);

    // --- Result preview ---
    result_label_ = new QLabel(this);
    result_label_->setStyleSheet(
        "QLabel { color: #66cccc; font-family: 'Consolas', monospace;"
        " font-size: 14px; font-weight: bold; padding: 4px; }");
    result_label_->setAlignment(Qt::AlignCenter);
    root->addWidget(result_label_);

    // --- Buttons ---
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText("Annuler");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // --- Wiring ---
    connect(fx_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FxInputDialog::on_fx_changed);
    connect(param1_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { update_result_preview(); });
    connect(param2_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { update_result_preview(); });

    // --- Initialize from current values ---
    if (current_fx == 0 && current_param == 0) {
        fx_combo_->setCurrentIndex(0);  // Aucun
    } else {
        int combo_idx = fx_to_combo(current_fx);
        // Special case: fx=0 with param!=0 is arpeggio
        if (current_fx == 0 && current_param != 0) combo_idx = 1;
        fx_combo_->setCurrentIndex(combo_idx);
    }

    // Set parameter values after combo is set (on_fx_changed will configure ranges)
    on_fx_changed(fx_combo_->currentIndex());

    // Now apply the actual parameter values
    uint8_t hi = (current_param >> 4) & 0x0F;
    uint8_t lo = current_param & 0x0F;
    int idx = fx_combo_->currentIndex();
    if (idx == 1 || idx == 6 || idx == 10) {
        // Arpege, VolSlide, or HostCmd: two nibbles
        param1_spin_->setValue(hi);
        param2_spin_->setValue(lo);
    } else if (idx >= 2 && idx <= 5) {
        // Pitch slide, portamento, pitch bend: full byte
        param1_spin_->setValue(current_param);
    } else if (idx == 7) {
        // Set speed
        param1_spin_->setValue(current_param);
    } else if (idx == 8 || idx == 9) {
        // Note cut / delay: tick count
        param1_spin_->setValue(current_param);
    } else if (idx == 11) {
        // Expression
        param1_spin_->setValue(current_param);
    }

    update_result_preview();
}

// ============================================================
// Effect changed — update UI
// ============================================================

void FxInputDialog::on_fx_changed(int index) {
    // Reset visibility
    param1_label_->hide();
    param1_spin_->hide();
    param2_label_->hide();
    param2_spin_->hide();

    switch (index) {
    case 0:  // Aucun
        desc_label_->setText(QString::fromUtf8(
            "Supprime l'effet de cette cellule."));
        break;

    case 1:  // Arpege
        desc_label_->setText(QString::fromUtf8(
            "Alterne rapidement entre la note de base, note+X demi-tons et note+Y demi-tons "
            "a chaque tick.\n"
            "Exemple : X=3, Y=7 sur un Do donne Do-Mi-Sol (accord majeur)."));
        param1_label_->setText(QString::fromUtf8("X (demi-tons 1) :"));
        param1_spin_->setRange(0, 15);
        param1_spin_->setValue(3);
        param1_label_->show();
        param1_spin_->show();
        param2_label_->setText(QString::fromUtf8("Y (demi-tons 2) :"));
        param2_spin_->setRange(0, 15);
        param2_spin_->setValue(7);
        param2_label_->show();
        param2_spin_->show();
        break;

    case 2:  // Pitch slide up
        desc_label_->setText(QString::fromUtf8(
            "Monte la frequence progressivement a chaque tick.\n"
            "Plus la vitesse est elevee, plus ca monte vite.\n"
            "Valeurs typiques : 1-4 (lent), 8-16 (moyen), 32+ (rapide)."));
        param1_label_->setText(QString::fromUtf8("Vitesse :"));
        param1_spin_->setRange(1, 255);
        param1_spin_->setValue(4);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 3:  // Pitch slide down
        desc_label_->setText(QString::fromUtf8(
            "Baisse la frequence progressivement a chaque tick.\n"
            "Plus la vitesse est elevee, plus ca descend vite.\n"
            "Valeurs typiques : 1-4 (lent), 8-16 (moyen), 32+ (rapide)."));
        param1_label_->setText(QString::fromUtf8("Vitesse :"));
        param1_spin_->setRange(1, 255);
        param1_spin_->setValue(4);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 4:  // Portamento
        desc_label_->setText(QString::fromUtf8(
            "Glisse depuis la note precedente vers cette note.\n"
            "La vitesse controle a quelle rapidite le glissement s'effectue.\n"
            "Il faut avoir une note deja en cours de lecture sur ce canal."));
        param1_label_->setText(QString::fromUtf8("Vitesse :"));
        param1_spin_->setRange(1, 255);
        param1_spin_->setValue(8);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 5:  // Pitch bend
        desc_label_->setText(QString::fromUtf8(
            "Decale la frequence de la note d'un offset fixe.\n"
            "Valeur 01-7F : baisse la frequence (son plus grave).\n"
            "Valeur 80-FF : monte la frequence (son plus aigu).\n"
            "00 = pas de bend. Effet persistant (reste actif jusqu'au prochain 4xx)."));
        param1_label_->setText(QString::fromUtf8("Offset (0-255) :"));
        param1_spin_->setRange(0, 255);
        param1_spin_->setValue(0);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 6:  // Volume slide
        desc_label_->setText(QString::fromUtf8(
            "Modifie le volume progressivement a chaque tick.\n"
            "Montee : augmente le volume (diminue l'attenuation).\n"
            "Descente : baisse le volume (augmente l'attenuation).\n"
            "Mettez un seul des deux a une valeur non-zero."));
        param1_label_->setText(QString::fromUtf8("Montee (0-15) :"));
        param1_spin_->setRange(0, 15);
        param1_spin_->setValue(1);
        param1_label_->show();
        param1_spin_->show();
        param2_label_->setText(QString::fromUtf8("Descente (0-15) :"));
        param2_spin_->setRange(0, 15);
        param2_spin_->setValue(0);
        param2_label_->show();
        param2_spin_->show();
        break;

    case 7:  // Set speed
        desc_label_->setText(QString::fromUtf8(
            "Change la vitesse du morceau immediatement.\n"
            "TPR = Ticks Per Row.\n"
            "4 = rapide (~225 BPM), 8 = moyen (~112 BPM), 12 = lent (~75 BPM)."));
        param1_label_->setText(QString::fromUtf8("TPR (1-32) :"));
        param1_spin_->setRange(1, 32);
        param1_spin_->setValue(8);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 8:  // Note cut
        desc_label_->setText(QString::fromUtf8(
            "Coupe le son apres le nombre de ticks indique.\n"
            "Utile pour des notes courtes type hi-hat ou staccato.\n"
            "Exemple : 2 ticks = son tres court, 4 = moyen."));
        param1_label_->setText(QString::fromUtf8("Ticks avant coupure :"));
        param1_spin_->setRange(0, 255);
        param1_spin_->setValue(4);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 9:  // Note delay
        desc_label_->setText(QString::fromUtf8(
            "Retarde le declenchement de la note du nombre de ticks indique.\n"
            "Utile pour creer un effet de swing ou de groove.\n"
            "Exemple : 2-3 ticks de retard donne un feeling shuffle."));
        param1_label_->setText(QString::fromUtf8("Ticks de retard :"));
        param1_spin_->setRange(0, 255);
        param1_spin_->setValue(3);
        param1_label_->show();
        param1_spin_->show();
        break;

    case 10:  // Host command (Exy)
        desc_label_->setText(QString::fromUtf8(
            "Commandes globales (affectent tout le morceau).\n"
            "Type 0 = Fade out : le volume baisse progressivement (valeur = vitesse, 0 = annuler).\n"
            "Type 1 = Tempo : change la vitesse (valeur = nouveau TPR, 0 = 16)."));
        param1_label_->setText(QString::fromUtf8("Type (0=fade, 1=tempo) :"));
        param1_spin_->setRange(0, 1);
        param1_spin_->setValue(0);
        param1_label_->show();
        param1_spin_->show();
        param2_label_->setText(QString::fromUtf8("Valeur (0-15) :"));
        param2_spin_->setRange(0, 15);
        param2_spin_->setValue(3);
        param2_label_->show();
        param2_spin_->show();
        break;

    case 11:  // Expression (Fxx)
        desc_label_->setText(QString::fromUtf8(
            "Ajoute un offset d'attenuation persistant au canal.\n"
            "0 = volume normal, 1-F = volume reduit.\n"
            "L'effet reste actif jusqu'au prochain Fxx.\n"
            "Utile pour baisser un canal d'accompagnement."));
        param1_label_->setText(QString::fromUtf8("Attenuation (0-15) :"));
        param1_spin_->setRange(0, 15);
        param1_spin_->setValue(0);
        param1_label_->show();
        param1_spin_->show();
        break;
    }

    update_result_preview();
}

// ============================================================
// Result preview
// ============================================================

void FxInputDialog::update_result_preview() {
    uint8_t f = fx();
    uint8_t p = fx_param();
    if (f == 0 && p == 0) {
        result_label_->setText(QString::fromUtf8("Resultat : --- (aucun effet)"));
    } else {
        result_label_->setText(
            QString::fromUtf8("Resultat : %1%2")
                .arg(f, 1, 16, QLatin1Char('0'))
                .arg(p, 2, 16, QLatin1Char('0'))
                .toUpper());
    }
}

// ============================================================
// Getters
// ============================================================

uint8_t FxInputDialog::fx() const {
    int idx = fx_combo_->currentIndex();
    if (idx == 0) return 0;  // Aucun
    return combo_to_fx(idx);
}

uint8_t FxInputDialog::fx_param() const {
    int idx = fx_combo_->currentIndex();
    if (idx == 0) return 0;  // Aucun

    switch (idx) {
    case 1:  // Arpege: x=hi nibble, y=lo nibble
    case 6:  // VolSlide: x=hi nibble, y=lo nibble
    case 10: // HostCmd: x=type, y=value
        return static_cast<uint8_t>((param1_spin_->value() << 4) | param2_spin_->value());
    case 2:  // Pitch slide up
    case 3:  // Pitch slide down
    case 4:  // Portamento
    case 5:  // Pitch bend
    case 7:  // Set speed
    case 8:  // Note cut
    case 9:  // Note delay
    case 11: // Expression
        return static_cast<uint8_t>(param1_spin_->value());
    default:
        return 0;
    }
}
