#include "tabs/TrackerTab.h"

#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QKeySequence>
#include <QSaveFile>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <random>
#include <sstream>

#include "audio/EngineHub.h"
#include "audio/InstrumentPlayer.h"
#include "audio/PsgHelpers.h"
#include "audio/TrackerPlaybackEngine.h"
#include "audio/MidiImporter.h"
#include "audio/WavExporter.h"
#include "i18n/AppLanguage.h"
#include "models/InstrumentStore.h"
#include "models/SongDocument.h"
#include "widgets/TrackerGridWidget.h"
#include <QListWidget>
#include "widgets/FxInputDialog.h"
#include "widgets/NoteInputDialog.h"
#include "widgets/InstrumentInputDialog.h"
#include "widgets/AttnInputDialog.h"
#include "ngpc/instrument.h"

namespace {
constexpr int kMaxExportWarnings = 20;

void append_export_warning(QStringList& warnings, int& hidden_count, const QString& message) {
    if (warnings.size() < kMaxExportWarnings) {
        warnings.push_back(message);
    } else {
        hidden_count++;
    }
}

QString tracker_note_to_text(uint8_t note) {
    if (note == 0) return "---";
    if (note == 0xFF) return "OFF";
    if (note < 1 || note > 127) return "INV";

    static const char* names[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
    const int n = static_cast<int>(note) - 1;
    return QString("%1%2").arg(names[n % 12]).arg(n / 12);
}

int frames_to_ms(int frames) {
    if (frames <= 0) return 0;
    return (frames * 1000 + 59) / 60;
}

int estimate_preview_gate_ms(const ngpc::BgmInstrumentDef& d) {
    int gate_frames = (d.mode == 1) ? 12 : 22;

    if (d.adsr_on) {
        const int base_attn = std::clamp<int>(d.attn, 0, 15);
        const int sustain_attn = std::max(base_attn, std::clamp<int>(d.adsr_sustain, 0, 15));
        const int attack_steps = std::max(0, 15 - base_attn);
        const int decay_steps = std::max(0, sustain_attn - base_attn);
        const int atk_frames = (d.adsr_attack == 0) ? 0 : attack_steps * (static_cast<int>(d.adsr_attack) + 1);
        const int dec_frames = (d.adsr_decay == 0) ? 0 : decay_steps * (static_cast<int>(d.adsr_decay) + 1);
        gate_frames = std::max(gate_frames, atk_frames + dec_frames + 12);
    }

    if (d.mode == 0 && d.vib_on && d.vib_depth > 0) {
        const int vib_frames = static_cast<int>(d.vib_delay) + (static_cast<int>(d.vib_speed) + 1) * 2 + 8;
        gate_frames = std::max(gate_frames, vib_frames);
    }

    if (d.mode == 0 && d.lfo_on && d.lfo_depth > 0) {
        int lfo_frames = 0;
        if (d.lfo_wave == 0) {
            const int audible_steps = std::max(6, static_cast<int>(d.lfo_depth) / 2);
            lfo_frames = audible_steps * (static_cast<int>(d.lfo_rate) + 1) + 10;
        } else {
            lfo_frames = (static_cast<int>(d.lfo_rate) + 1) * 4 + 10;
        }
        gate_frames = std::max(gate_frames, lfo_frames);
    }

    gate_frames = std::clamp(gate_frames, (d.mode == 1) ? 10 : 18, 150);
    return frames_to_ms(gate_frames);
}

int estimate_preview_hard_stop_ms(const ngpc::BgmInstrumentDef& d, int gate_ms) {
    int tail_ms = 1200;
    if (d.adsr_on && d.adsr_release > 0) {
        const int rel_frames = 10 + 15 * (static_cast<int>(d.adsr_release) + 1);
        tail_ms = std::clamp(frames_to_ms(rel_frames), 400, 5000);
    }
    return gate_ms + tail_ms;
}

struct EditCell {
    int ch = 0;
    int row = 0;
};

std::vector<EditCell> current_edit_cells(const TrackerGridWidget* grid, int ch, int row) {
    std::vector<EditCell> cells;
    cells.push_back(EditCell{ch, row});
    if (!grid) {
        return cells;
    }
    if (grid->has_discrete_selection()) {
        cells.clear();
        for (const auto& p : grid->selected_cells()) {
            cells.push_back(EditCell{p.first, p.second});
        }
        if (cells.empty()) {
            cells.push_back(EditCell{ch, row});
        }
        return cells;
    }
    if (!grid || !grid->has_selection()) {
        return cells;
    }
    cells.clear();
    const int row_start = grid->sel_start_row();
    const int row_end = grid->sel_end_row();
    if (grid->has_multi_ch_selection()) {
        const int ch_start = grid->sel_start_ch();
        const int ch_end = grid->sel_end_ch();
        for (int cc = ch_start; cc <= ch_end; ++cc) {
            for (int rr = row_start; rr <= row_end; ++rr) {
                cells.push_back(EditCell{cc, rr});
            }
        }
    } else {
        const int cc = grid->cursor_ch();
        for (int rr = row_start; rr <= row_end; ++rr) {
            cells.push_back(EditCell{cc, rr});
        }
    }
    return cells;
}

QStringList audit_song_for_export(const SongDocument* song,
                                  const InstrumentStore* store,
                                  bool hybrid_mode) {
    QStringList warnings;
    int hidden_count = 0;

    if (!song) {
        warnings.push_back("No song loaded.");
        return warnings;
    }

    const auto& order = song->order();
    if (order.empty()) {
        warnings.push_back("Order list is empty; export will contain no music.");
        return warnings;
    }

    const int store_count = store ? store->count() : 0;
    std::array<bool, 128> warned_missing_instrument{};
    warned_missing_instrument.fill(false);
    std::array<bool, 16> warned_unsupported_fx{};
    warned_unsupported_fx.fill(false);
    std::array<bool, 1024> divider_seen{};
    divider_seen.fill(false);

    bool warned_invalid_note = false;
    bool warned_invalid_attn = false;
    bool warned_b00 = false;
    int hybrid_bxx_off_ch0 = 0;
    int hybrid_exx_off_ch0 = 0;
    QString first_bxx_off_ch0;
    QString first_exx_off_ch0;

    for (int ord_pos = 0; ord_pos < static_cast<int>(order.size()); ++ord_pos) {
        const int pat_idx = order[static_cast<size_t>(ord_pos)];
        const TrackerDocument* pat = song->pattern(pat_idx);
        if (!pat) {
            append_export_warning(
                warnings, hidden_count,
                QString("Order %1 references missing pattern %2.").arg(ord_pos).arg(pat_idx));
            continue;
        }

        for (int row = 0; row < pat->length(); ++row) {
            for (int ch = 0; ch < 4; ++ch) {
                const TrackerCell& c = pat->cell(ch, row);
                const QString loc = QString("ord %1 pat %2 row %3 ch%4")
                                        .arg(ord_pos).arg(pat_idx).arg(row).arg(ch);

                if (c.note != 0 && c.note != 0xFF && !c.is_note_on() && !warned_invalid_note) {
                    append_export_warning(
                        warnings, hidden_count,
                        QString("Invalid note value %1 at %2 (expected 1..127, OFF or empty).")
                            .arg(c.note).arg(loc));
                    warned_invalid_note = true;
                }

                if (c.attn != 0xFF && c.attn > 15 && !warned_invalid_attn) {
                    append_export_warning(
                        warnings, hidden_count,
                        QString("Invalid attenuation %1 at %2 (expected 0..15 or AUTO).")
                            .arg(c.attn).arg(loc));
                    warned_invalid_attn = true;
                }

                const uint8_t fx_nibble = static_cast<uint8_t>(c.fx & 0x0F);
                if (c.has_fx()) {
                    switch (fx_nibble) {
                    case 0x0:
                    case 0x1:
                    case 0x2:
                    case 0x3:
                    case 0x4:
                    case 0xA:
                    case 0xB:
                    case 0xC:
                    case 0xD:
                    case 0xE:
                    case 0xF:
                        break;
                    default:
                        if (!warned_unsupported_fx[fx_nibble]) {
                            append_export_warning(
                                warnings, hidden_count,
                                QString("FX %1 is not supported at runtime (first seen at %2).")
                                    .arg(fx_nibble, 1, 16, QChar('0')).toUpper()
                                    .arg(loc));
                            warned_unsupported_fx[fx_nibble] = true;
                        }
                        break;
                    }

                    if (fx_nibble == 0xB && c.fx_param == 0 && !warned_b00) {
                        append_export_warning(
                            warnings, hidden_count,
                            QString("B00 has no effect (first seen at %1).").arg(loc));
                        warned_b00 = true;
                    }
                    if (hybrid_mode && fx_nibble == 0xB && ch != 0) {
                        hybrid_bxx_off_ch0++;
                        if (first_bxx_off_ch0.isEmpty()) first_bxx_off_ch0 = loc;
                    }
                    if (hybrid_mode && fx_nibble == 0xE && ch != 0) {
                        hybrid_exx_off_ch0++;
                        if (first_exx_off_ch0.isEmpty()) first_exx_off_ch0 = loc;
                    }
                }

                if (!c.is_note_on()) continue;

                if (store_count <= 0) {
                    if (c.instrument != 0 && !warned_missing_instrument[c.instrument]) {
                        append_export_warning(
                            warnings, hidden_count,
                            QString("Instrument %1 used at %2 but instrument bank is empty.")
                                .arg(c.instrument).arg(loc));
                        warned_missing_instrument[c.instrument] = true;
                    }
                } else if (c.instrument >= store_count && !warned_missing_instrument[c.instrument]) {
                    append_export_warning(
                        warnings, hidden_count,
                        QString("Instrument %1 used at %2 but only 0..%3 exist. Fallback to default.")
                            .arg(c.instrument).arg(loc).arg(store_count - 1));
                    warned_missing_instrument[c.instrument] = true;
                }

                if (ch < 3) {
                    const uint16_t div = TrackerPlaybackEngine::midi_to_divider(c.note);
                    divider_seen[std::clamp<int>(div, 1, 1023)] = true;
                }
            }
        }
    }

    int unique_dividers = 0;
    for (int div = 1; div <= 1023; ++div) {
        if (divider_seen[static_cast<size_t>(div)]) unique_dividers++;
    }
    if (unique_dividers > 51) {
        append_export_warning(
            warnings, hidden_count,
            QString("Tone note table uses %1 unique dividers; driver limit is 51 (closest match fallback).")
                .arg(unique_dividers));
    }

    if (hybrid_mode && hybrid_bxx_off_ch0 > 0) {
        append_export_warning(
            warnings, hidden_count,
            QString("Hybrid export found %1 Bxx command(s) outside CH0 (first: %2). "
                    "Prefer putting global speed changes on CH0 for deterministic timing.")
                .arg(hybrid_bxx_off_ch0).arg(first_bxx_off_ch0));
    }

    if (hybrid_mode && hybrid_exx_off_ch0 > 0) {
        append_export_warning(
            warnings, hidden_count,
            QString("Hybrid export found %1 Exx host command(s) outside CH0 (first: %2). "
                    "Prefer CH0 for global host commands.")
                .arg(hybrid_exx_off_ch0).arg(first_exx_off_ch0));
    }

    if (hidden_count > 0) {
        warnings.push_back(QString("%1 additional warning(s) hidden.").arg(hidden_count));
    }

    return warnings;
}

ngpc::BgmInstrumentDef resolve_preview_instrument(const InstrumentStore* store, uint8_t inst_id) {
    if (store && inst_id < static_cast<uint8_t>(store->count())) {
        return store->at(inst_id).def;
    }
    const auto& presets = ngpc::FactoryInstrumentPresets();
    if (inst_id < static_cast<uint8_t>(presets.size())) {
        return presets[inst_id].def;
    }
    return ngpc::BgmInstrumentDef{};
}
} // namespace

// ============================================================
// Construction
// ============================================================

TrackerTab::TrackerTab(EngineHub* hub, InstrumentStore* store, QWidget* parent)
    : QWidget(parent), hub_(hub), store_(store)
{
    const AppLanguage lang = load_app_language();
    const auto ui = [lang](const char* fr, const char* en) {
        return app_lang_pick(lang, fr, en);
    };

    song_ = new SongDocument(this);
    doc_ = song_->active_pattern();
    grid_ = new TrackerGridWidget(doc_, this);

    // Playback engine
    engine_ = new TrackerPlaybackEngine(this);
    engine_->set_document(doc_);
    engine_->set_instrument_store(store_);
    preview_player_ = new InstrumentPlayer(hub_, this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(2);

    // --- Top toolbar row 1: transport + timing ---
    auto* transport_row = new QHBoxLayout();
    transport_row->setSpacing(4);
    transport_row->addWidget(new QLabel("Transport:", this));

    play_btn_ = new QPushButton("Play [Space]", this);
    play_song_btn_ = new QPushButton("Song", this);
    play_song_btn_->setToolTip(ui(
        "Jouer le morceau complet (patterns dans l'ordre)",
        "Play entire song (all patterns in order)"));
    loop_sel_btn_ = new QPushButton("Loop Sel", this);
    loop_sel_btn_->setToolTip(ui(
        "Boucler la lecture sur les lignes selectionnees (selectionner d'abord avec Shift+fleches)",
        "Loop playback over selected rows (select rows first with Shift+arrows)"));
    stop_btn_ = new QPushButton("Stop [F8]", this);
    transport_row->addWidget(play_btn_);
    transport_row->addWidget(play_song_btn_);
    transport_row->addWidget(loop_sel_btn_);
    transport_row->addWidget(stop_btn_);

    transport_row->addSpacing(8);
    transport_row->addWidget(new QLabel("Edit:", this));

    record_btn_ = new QPushButton("REC", this);
    record_btn_->setCheckable(true);
    record_btn_->setChecked(true);
    record_btn_->setFixedWidth(40);
    record_btn_->setToolTip(ui(
        "Mode enregistrement: quand actif, le clavier ecrit les notes dans la grille",
        "Record mode: when ON, keyboard keys write notes into the grid"));
    record_btn_->setStyleSheet(
        "QPushButton:checked { background: #c03030; color: white; font-weight: bold; }");
    transport_row->addWidget(record_btn_);

    follow_btn_ = new QPushButton("Follow", this);
    follow_btn_->setCheckable(true);
    follow_btn_->setChecked(true);
    follow_btn_->setToolTip(ui(
        "Suivre la lecture (le curseur suit la ligne jouee)",
        "Follow playback position (cursor follows play row)"));
    transport_row->addWidget(follow_btn_);

    clear_btn_ = new QPushButton("Clear", this);
    clear_btn_->setToolTip(ui(
        "Effacer toutes les donnees du pattern (Ctrl+Suppr)",
        "Clear all pattern data (Ctrl+Del)"));
    transport_row->addWidget(clear_btn_);

    transport_row->addSpacing(10);
    transport_row->addWidget(new QLabel("Timing:", this));

    transport_row->addWidget(new QLabel("TPR:", this));
    tpr_spin_ = new QSpinBox(this);
    tpr_spin_->setRange(1, 32);
    tpr_spin_->setValue(8);
    tpr_spin_->setToolTip(ui("Ticks par ligne (vitesse)", "Ticks per row (speed)"));
    transport_row->addWidget(tpr_spin_);

    bpm_label_ = new QLabel(this);
    bpm_label_->setToolTip(ui(
        "BPM estime (base sur TPR et 60fps)",
        "Estimated BPM (based on TPR and 60fps)"));
    bpm_label_->setFixedWidth(80);
    transport_row->addWidget(bpm_label_);
    transport_row->addStretch(1);
    root->addLayout(transport_row);

    // --- Top toolbar row 2: edit tools ---
    auto* edit_row = new QHBoxLayout();
    edit_row->setSpacing(4);
    edit_row->addWidget(new QLabel("Grid:", this));

    edit_row->addWidget(new QLabel("Oct:", this));
    octave_spin_ = new QSpinBox(this);
    octave_spin_->setRange(0, 8);
    octave_spin_->setValue(4);
    octave_spin_->setToolTip(ui(
        "Octave de base pour le clavier (+/- pave numerique)",
        "Base octave for keyboard (+/- numpad)"));
    edit_row->addWidget(octave_spin_);

    edit_row->addWidget(new QLabel("Step:", this));
    step_spin_ = new QSpinBox(this);
    step_spin_->setRange(0, 16);
    step_spin_->setValue(1);
    step_spin_->setToolTip(ui(
        "Pas d'edition (lignes avancees apres une note)",
        "Edit step (rows to advance after note)"));
    edit_row->addWidget(step_spin_);

    edit_row->addWidget(new QLabel("Len:", this));
    length_spin_ = new QSpinBox(this);
    length_spin_->setRange(TrackerDocument::kMinLength, TrackerDocument::kMaxLength);
    length_spin_->setValue(TrackerDocument::kDefaultLength);
    length_spin_->setToolTip(ui("Longueur du pattern (lignes)", "Pattern length (rows)"));
    edit_row->addWidget(length_spin_);

    edit_row->addWidget(new QLabel("KB:", this));
    kb_layout_combo_ = new QComboBox(this);
    kb_layout_combo_->addItem("QWERTY");
    kb_layout_combo_->addItem("AZERTY");
    kb_layout_combo_->setCurrentIndex(1);  // AZERTY default for French users
    kb_layout_combo_->setToolTip(ui(
        "Disposition clavier pour la saisie des notes",
        "Keyboard layout for note input"));
    edit_row->addWidget(kb_layout_combo_);
    grid_->set_key_layout(TrackerGridWidget::LayoutAZERTY);

    edit_row->addSpacing(8);
    auto* template_combo = new QComboBox(this);
    template_combo->addItem("Tpl: Kick 4/4 (Noise)");
    template_combo->addItem("Tpl: Snare Backbeat (Noise)");
    template_combo->addItem("Tpl: Hi-Hat 8ths (Noise)");
    template_combo->addItem("Tpl: Bass Pulse (Tone)");
    template_combo->addItem("Tpl: Arp Triad 8ths (Tone)");
    template_combo->addItem("Tpl: Chiptune Starter (All)");
    template_combo->addItem("Tpl: Kick + Hat Groove (Noise)");
    template_combo->addItem("Tpl: Snare Fill 16ths (Noise)");
    template_combo->addItem("Tpl: Bass + Arp Duo (Tone)");
    template_combo->addItem("Tpl: Full Groove Loop (All)");
    template_combo->setToolTip(ui(
        "Template de pattern. S'applique sur la selection (si active), sinon sur tout le pattern.",
        "Pattern template. Applies on selection (if any), otherwise on full pattern."));
    template_combo->setMinimumContentsLength(18);
    template_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    edit_row->addWidget(template_combo);
    auto* template_apply_btn = new QPushButton("Apply Tpl", this);
    template_apply_btn->setToolTip(ui(
        "Appliquer le template selectionne (raccourci Ctrl+T)",
        "Apply selected template (shortcut Ctrl+T)"));
    edit_row->addWidget(template_apply_btn);
    edit_row->addStretch(1);
    root->addLayout(edit_row);

    // --- Pattern / Order row ---
    auto* pat_order_row = new QHBoxLayout();
    pat_order_row->setSpacing(4);

    pat_order_row->addWidget(new QLabel("Pat:", this));
    pattern_spin_ = new QSpinBox(this);
    pattern_spin_->setRange(0, 0);  // updated by refresh_pattern_ui
    pattern_spin_->setValue(0);
    pattern_spin_->setToolTip(ui("Index du pattern courant", "Current pattern index"));
    pattern_spin_->setFixedWidth(50);
    pat_order_row->addWidget(pattern_spin_);

    pattern_count_label_ = new QLabel("/1", this);
    pattern_count_label_->setFixedWidth(24);
    pat_order_row->addWidget(pattern_count_label_);

    pat_add_btn_ = new QPushButton("+", this);
    pat_add_btn_->setFixedWidth(28);
    pat_add_btn_->setToolTip(ui("Ajouter un pattern vide", "Add new empty pattern"));
    pat_order_row->addWidget(pat_add_btn_);

    pat_clone_btn_ = new QPushButton("Cln", this);
    pat_clone_btn_->setFixedWidth(32);
    pat_clone_btn_->setToolTip(ui("Cloner le pattern courant", "Clone current pattern"));
    pat_order_row->addWidget(pat_clone_btn_);

    pat_del_btn_ = new QPushButton("-", this);
    pat_del_btn_->setFixedWidth(28);
    pat_del_btn_->setToolTip(ui("Supprimer le pattern courant", "Delete current pattern"));
    pat_order_row->addWidget(pat_del_btn_);

    pat_order_row->addSpacing(12);

    pat_order_row->addWidget(new QLabel("Order:", this));
    order_list_ = new QListWidget(this);
    order_list_->setFlow(QListView::LeftToRight);
    order_list_->setMaximumHeight(28);
    order_list_->setMinimumWidth(200);
    order_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    order_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    order_list_->setStyleSheet(
        "QListWidget { background: #1e1e2a; color: #ccccdd; font-family: 'Consolas', monospace;"
        " font-size: 12px; border: 1px solid #333; }"
        "QListWidget::item { padding: 2px 6px; }"
        "QListWidget::item:selected { background: #4060a0; }");
    pat_order_row->addWidget(order_list_);

    ord_add_btn_ = new QPushButton("+O", this);
    ord_add_btn_->setFixedWidth(28);
    ord_add_btn_->setToolTip(ui(
        "Ajouter le pattern courant a la liste d'ordre",
        "Add current pattern to order list"));
    pat_order_row->addWidget(ord_add_btn_);

    ord_del_btn_ = new QPushButton("-O", this);
    ord_del_btn_->setFixedWidth(28);
    ord_del_btn_->setToolTip(ui(
        "Retirer l'entree selectionnee de l'ordre",
        "Remove selected entry from order"));
    pat_order_row->addWidget(ord_del_btn_);

    ord_up_btn_ = new QPushButton("<", this);
    ord_up_btn_->setFixedWidth(24);
    ord_up_btn_->setToolTip(ui("Monter l'entree d'ordre", "Move order entry up"));
    pat_order_row->addWidget(ord_up_btn_);

    ord_down_btn_ = new QPushButton(">", this);
    ord_down_btn_->setFixedWidth(24);
    ord_down_btn_->setToolTip(ui("Descendre l'entree d'ordre", "Move order entry down"));
    pat_order_row->addWidget(ord_down_btn_);

    loop_btn_ = new QPushButton("Loop", this);
    loop_btn_->setFixedWidth(40);
    loop_btn_->setToolTip(ui(
        "Definir le point de boucle sur la position d'ordre selectionnee",
        "Set loop point at selected order position"));
    pat_order_row->addWidget(loop_btn_);

    pat_order_row->addStretch(1);
    root->addLayout(pat_order_row);

    // --- Main area: tracker + right control panel ---
    auto* grid_row = new QHBoxLayout();
    grid_row->setSpacing(8);
    grid_row->addWidget(grid_, 1);

    auto* side_panel = new QWidget(this);
    side_panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    side_panel->setMinimumWidth(300);
    side_panel->setMaximumWidth(380);
    auto* side_layout = new QVBoxLayout(side_panel);
    side_layout->setContentsMargins(0, 0, 0, 0);
    side_layout->setSpacing(6);

    auto* io_title = new QLabel("Import / Export", side_panel);
    io_title->setStyleSheet("QLabel { color: #c8c8da; font-weight: bold; }");
    side_layout->addWidget(io_title);

    auto* file_row = new QHBoxLayout();
    save_btn_ = new QPushButton("Save", side_panel);
    save_btn_->setToolTip(ui(
        "Sauver le morceau (.ngps) ou le pattern (.ngpat) (Ctrl+S)",
        "Save song (.ngps) or pattern (.ngpat) (Ctrl+S)"));
    load_btn_ = new QPushButton("Load", side_panel);
    load_btn_->setToolTip(ui(
        "Charger un morceau (.ngps) ou un pattern (.ngpat) (Ctrl+O)",
        "Load song (.ngps) or pattern (.ngpat) (Ctrl+O)"));
    file_row->addWidget(save_btn_);
    file_row->addWidget(load_btn_);
    side_layout->addLayout(file_row);

    auto* midi_btn = new QPushButton("Import MIDI", side_panel);
    midi_btn->setToolTip(ui(
        "Importer un fichier MIDI dans le tracker",
        "Import a MIDI file into the tracker"));
    side_layout->addWidget(midi_btn);

    auto* mode_row = new QHBoxLayout();
    mode_row->addWidget(new QLabel("Mode:", side_panel));
    export_mode_combo_ = new QComboBox(side_panel);
    export_mode_combo_->addItem("Pre-baked");
    export_mode_combo_->addItem("Hybride");
    export_mode_combo_->setToolTip(ui(
        "Pre-baked: fidelite parfaite (tick-by-tick)\nHybride: instruments driver, streams compacts",
        "Pre-baked: perfect fidelity (tick-by-tick)\nHybrid: driver instruments, compact streams"));
    mode_row->addWidget(export_mode_combo_, 1);
    side_layout->addLayout(mode_row);

    auto* export_row = new QHBoxLayout();
    export_btn_ = new QPushButton("Export C", side_panel);
    export_btn_->setToolTip(ui(
        "Exporter le morceau en fichier source C",
        "Export song as C source file"));
    export_asm_btn_ = new QPushButton("Export ASM", side_panel);
    export_asm_btn_->setToolTip(ui(
        "Exporter le morceau en fichier ASM .inc",
        "Export song as ASM .inc file"));
    auto* wav_btn = new QPushButton("Export WAV", side_panel);
    wav_btn->setToolTip(ui(
        "Exporter le morceau/pattern en WAV",
        "Export song/pattern as WAV file"));
    export_row->addWidget(export_btn_);
    export_row->addWidget(export_asm_btn_);
    export_row->addWidget(wav_btn);
    side_layout->addLayout(export_row);

    auto* mix_title = new QLabel("Mix", side_panel);
    mix_title->setStyleSheet("QLabel { color: #c8c8da; font-weight: bold; }");
    side_layout->addWidget(mix_title);

    static const char* kChNames[4] = {"T0", "T1", "T2", "N"};
    auto* mute_row = new QHBoxLayout();
    mute_row->setSpacing(2);
    mute_row->addWidget(new QLabel("Mute:", side_panel));
    for (int ch = 0; ch < 4; ++ch) {
        mute_btns_[ch] = new QPushButton(QString::fromLatin1(kChNames[ch]), side_panel);
        mute_btns_[ch]->setCheckable(true);
        mute_btns_[ch]->setFixedWidth(34);
        mute_btns_[ch]->setToolTip((lang == AppLanguage::English)
            ? QString("Mute channel %1 [F%2]").arg(ch).arg(ch + 1)
            : QString("Couper canal %1 [F%2]").arg(ch).arg(ch + 1));
        mute_row->addWidget(mute_btns_[ch]);
    }
    side_layout->addLayout(mute_row);

    auto* solo_row = new QHBoxLayout();
    solo_row->setSpacing(2);
    solo_row->addWidget(new QLabel("Solo:", side_panel));
    for (int ch = 0; ch < 4; ++ch) {
        solo_btns_[ch] = new QPushButton(QString::fromLatin1(kChNames[ch]), side_panel);
        solo_btns_[ch]->setCheckable(true);
        solo_btns_[ch]->setFixedWidth(34);
        solo_btns_[ch]->setToolTip((lang == AppLanguage::English)
            ? QString("Solo channel %1").arg(ch)
            : QString("Solo canal %1").arg(ch));
        solo_row->addWidget(solo_btns_[ch]);
    }
    side_layout->addLayout(solo_row);

    runtime_dbg_btn_ = new QPushButton("Dbg RT", side_panel);
    runtime_dbg_btn_->setCheckable(true);
    runtime_dbg_btn_->setToolTip(ui(
        "Debug runtime par ligne (note/divider/attn/fx par canal) dans le log tracker",
        "Runtime debug per row (note/divider/attn/fx per channel) in tracker log"));
    side_layout->addWidget(runtime_dbg_btn_);

    auto* help_title = new QLabel("Keyboard / Shortcuts", side_panel);
    help_title->setStyleSheet("QLabel { color: #c8c8da; font-weight: bold; }");
    side_layout->addWidget(help_title);

    kb_ref_label_ = new QLabel(side_panel);
    kb_ref_label_->setStyleSheet(
        "QLabel { background: #1e1e2a; color: #bbbbcc; padding: 6px;"
        " font-family: 'Consolas', 'Courier New', monospace; font-size: 11px;"
        " border: 1px solid #333; border-radius: 3px; }");
    kb_ref_label_->setWordWrap(true);
    kb_ref_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    update_kb_ref_label();
    side_layout->addWidget(kb_ref_label_, 1);

    grid_row->addWidget(side_panel, 0);
    root->addLayout(grid_row, 1);

    // --- Status bar ---
    status_label_ = new QLabel(this);
    status_label_->setStyleSheet(
        "QLabel { background: #1a1a24; color: #aaaaaa; padding: 2px 8px;"
        " font-family: 'Consolas', 'Courier New', monospace; font-size: 11px;"
        " border-top: 1px solid #333; }");
    status_label_->setText("Row 00  Ch 0  --  |  Instrument: ---");
    root->addWidget(status_label_);

    // --- Log ---
    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumHeight(50);
    log_->setPlaceholderText("Tracker log...");
    root->addWidget(log_);

    // --- Play timer (60 fps) ---
    play_timer_ = new QTimer(this);
    play_timer_->setInterval(1000 / 60);

    // Initial BPM label
    update_bpm_label();

    // === Connections ===

    // Transport
    connect(play_btn_, &QPushButton::clicked, this, [this]() {
        if (playing_) stop_playback(); else start_playback();
    });
    connect(stop_btn_, &QPushButton::clicked, this, &TrackerTab::stop_playback);
    connect(play_song_btn_, &QPushButton::clicked, this, &TrackerTab::start_song_playback);
    connect(loop_sel_btn_, &QPushButton::clicked, this, &TrackerTab::start_loop_selection);

    // Record mode
    connect(record_btn_, &QPushButton::toggled, this, [this](bool checked) {
        grid_->set_record_mode(checked);
    });

    // Follow mode
    connect(follow_btn_, &QPushButton::toggled, this, [this](bool checked) {
        follow_mode_ = checked;
    });

    // Clear pattern
    connect(clear_btn_, &QPushButton::clicked, this, [this]() {
        doc_->push_undo();
        doc_->clear_all();
        append_log("Pattern cleared.");
    });

    // Pattern length
    connect(length_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        doc_->set_length(val);
        append_log(QString("Pattern length: %1 rows").arg(val));
    });

    // Octave / step sync to grid
    connect(octave_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        grid_->set_octave(val);
        update_kb_ref_label();
    });
    connect(step_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        grid_->set_edit_step(val);
    });

    // TPR -> engine + BPM update
    connect(tpr_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        engine_->set_ticks_per_row(val);
        update_bpm_label();
    });

    // Engine speed_changed (from Bxx effect) -> sync TPR spin
    connect(engine_, &TrackerPlaybackEngine::speed_changed, this, [this](int tpr) {
        tpr_spin_->blockSignals(true);
        tpr_spin_->setValue(tpr);
        tpr_spin_->blockSignals(false);
        update_bpm_label();
    });

    // Engine row_changed -> update grid playback row + follow mode
    connect(engine_, &TrackerPlaybackEngine::row_changed, this, [this](int row) {
        grid_->set_playback_row(row);
        if (follow_mode_) {
            grid_->set_cursor(grid_->cursor_ch(), row, grid_->cursor_sub());
            grid_->ensure_row_visible(row);
        }
        if (runtime_debug_enabled_) {
            append_runtime_debug_row(row);
        }
    });

    // Engine pattern_finished -> song mode or loop
    connect(engine_, &TrackerPlaybackEngine::pattern_finished, this,
            &TrackerTab::on_pattern_finished);

    connect(runtime_dbg_btn_, &QPushButton::toggled, this, [this](bool enabled) {
        runtime_debug_enabled_ = enabled;
        append_log(enabled
                       ? "Runtime debug enabled (per-row channel dump)."
                       : "Runtime debug disabled.");
    });

    // --- Pattern / Order UI connections ---

    // Pattern selector
    connect(pattern_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        switch_to_pattern(val);
    });

    // Pattern bank buttons
    connect(pat_add_btn_, &QPushButton::clicked, this, [this]() {
        int idx = song_->add_pattern();
        if (idx >= 0) {
            refresh_pattern_ui();
            switch_to_pattern(idx);
            append_log(QString("Added pattern %1").arg(idx));
        }
    });
    connect(pat_clone_btn_, &QPushButton::clicked, this, [this]() {
        int idx = song_->clone_pattern(song_->active_pattern_index());
        if (idx >= 0) {
            refresh_pattern_ui();
            switch_to_pattern(idx);
            append_log(QString("Cloned pattern to %1").arg(idx));
        }
    });
    connect(pat_del_btn_, &QPushButton::clicked, this, [this]() {
        if (song_->pattern_count() <= 1) {
            append_log("Cannot delete the last pattern.");
            return;
        }
        const int old = song_->active_pattern_index();
        const auto answer = QMessageBox::warning(
            this,
            "Delete pattern",
            QString("Delete pattern %1?\nThis action cannot be undone.")
                .arg(old),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
        song_->remove_pattern(old);
        refresh_pattern_ui();
        switch_to_pattern(song_->active_pattern_index());
        refresh_order_list();
        append_log(QString("Deleted pattern %1").arg(old));
    });

    // Order list: double-click to jump to that pattern
    connect(order_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        int row = order_list_->currentRow();
        if (row < 0 || row >= song_->order_length()) return;
        const auto& ord = song_->order();
        switch_to_pattern(ord[static_cast<size_t>(row)]);
    });

    // Order buttons
    connect(ord_add_btn_, &QPushButton::clicked, this, [this]() {
        int pos = order_list_->currentRow();
        if (pos < 0) pos = song_->order_length();
        else pos++;
        song_->order_insert(pos, song_->active_pattern_index());
        refresh_order_list();
        order_list_->setCurrentRow(pos);
        append_log(QString("Order: inserted Pat %1 at pos %2")
            .arg(song_->active_pattern_index()).arg(pos));
    });
    connect(ord_del_btn_, &QPushButton::clicked, this, [this]() {
        int pos = order_list_->currentRow();
        if (pos < 0) return;
        song_->order_remove(pos);
        refresh_order_list();
    });
    connect(ord_up_btn_, &QPushButton::clicked, this, [this]() {
        int pos = order_list_->currentRow();
        if (pos <= 0) return;
        song_->order_move_up(pos);
        refresh_order_list();
        order_list_->setCurrentRow(pos - 1);
    });
    connect(ord_down_btn_, &QPushButton::clicked, this, [this]() {
        int pos = order_list_->currentRow();
        if (pos < 0 || pos >= song_->order_length() - 1) return;
        song_->order_move_down(pos);
        refresh_order_list();
        order_list_->setCurrentRow(pos + 1);
    });
    connect(loop_btn_, &QPushButton::clicked, this, [this]() {
        int pos = order_list_->currentRow();
        if (pos < 0) {
            append_log("Loop point unchanged: select an order entry first.");
            return;
        }
        song_->set_loop_point(pos);
        refresh_order_list();
        append_log(QString("Loop point set at order position %1").arg(pos));
    });

    // SongDocument signals
    connect(song_, &SongDocument::pattern_list_changed, this, [this]() {
        refresh_pattern_ui();
    });
    connect(song_, &SongDocument::order_changed, this, [this]() {
        refresh_order_list();
    });

    // Initial UI state
    refresh_pattern_ui();
    refresh_order_list();

    // Keyboard layout
    connect(kb_layout_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto layout = (idx == 1) ? TrackerGridWidget::LayoutAZERTY
                                 : TrackerGridWidget::LayoutQWERTY;
        grid_->set_key_layout(layout);
        update_kb_ref_label();
    });

    // Grid signals -> edit handlers
    connect(grid_, &TrackerGridWidget::note_entered, this, &TrackerTab::on_note_entered);
    connect(grid_, &TrackerGridWidget::note_preview_requested, this, [this](int ch, uint8_t note) {
        preview_note(note, ch);
    });
    connect(grid_, &TrackerGridWidget::note_off_entered, this, &TrackerTab::on_note_off_entered);
    connect(grid_, &TrackerGridWidget::cell_cleared, this, &TrackerTab::on_cell_cleared);
    connect(grid_, &TrackerGridWidget::instrument_digit, this, &TrackerTab::on_instrument_digit);
    connect(grid_, &TrackerGridWidget::attn_digit, this, &TrackerTab::on_attn_digit);
    connect(grid_, &TrackerGridWidget::fx_digit, this, &TrackerTab::on_fx_digit);
    connect(grid_, &TrackerGridWidget::fx_dialog_requested, this, [this](int ch, int row) {
        const auto& c = doc_->cell(ch, row);
        FxInputDialog dlg(c.fx, c.fx_param, this);
        if (dlg.exec() == QDialog::Accepted) {
            const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
            int changed = 0;
            bool undo_pushed = false;
            for (const EditCell& ec : edit_cells) {
                const auto& cell = doc_->cell(ec.ch, ec.row);
                if (cell.fx != dlg.fx() || cell.fx_param != dlg.fx_param()) {
                    if (!undo_pushed) {
                        doc_->push_undo();
                        undo_pushed = true;
                    }
                    doc_->set_fx(ec.ch, ec.row, dlg.fx());
                    doc_->set_fx_param(ec.ch, ec.row, dlg.fx_param());
                    changed++;
                }
            }
            grid_->update();
            update_status_label();
            uint8_t f = dlg.fx();
            uint8_t p = dlg.fx_param();
            if (changed == 0) {
                append_log("FX unchanged.");
            } else if (f == 0 && p == 0) {
                append_log(QString("FX cleared on %1 cell(s).").arg(changed));
            } else {
                append_log(QString("FX set on %1 cell(s): %2%3")
                    .arg(changed)
                    .arg(f, 1, 16, QChar('0'))
                    .arg(p, 2, 16, QChar('0'))
                    .toUpper());
            }
        }
    });
    // Note dialog (Enter / double-click on Note column)
    connect(grid_, &TrackerGridWidget::note_dialog_requested, this, [this](int ch, int row) {
        const auto& c = doc_->cell(ch, row);
        bool is_noise = (ch == 3);
        NoteInputDialog dlg(c.note, is_noise, this);
        if (dlg.exec() == QDialog::Accepted) {
            uint8_t n = dlg.note();
            doc_->push_undo();
            if (n == 0) {
                doc_->clear_cell(ch, row);
                append_log(QString("Note cleared at Ch%1 Row %2").arg(ch).arg(row));
            } else {
                doc_->set_note(ch, row, n);
                append_log(QString("Note set at Ch%1 Row %2").arg(ch).arg(row));
            }
            grid_->update();
            update_status_label();
        }
    });

    // Instrument dialog (Enter / double-click on Inst column)
    connect(grid_, &TrackerGridWidget::instrument_dialog_requested, this, [this](int ch, int row) {
        const auto& c = doc_->cell(ch, row);
        QStringList names;
        for (int i = 0; i <= TrackerDocument::kMaxInstrument; ++i) {
            if (store_ && i < store_->count()) {
                names << QString::fromStdString(store_->at(i).name);
            } else {
                names << "";
            }
        }
        InstrumentInputDialog dlg(c.instrument, names, this);
        if (dlg.exec() == QDialog::Accepted) {
            const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
            int changed = 0;
            bool undo_pushed = false;
            for (const EditCell& ec : edit_cells) {
                const auto& cell = doc_->cell(ec.ch, ec.row);
                if (!cell.is_note_on()) {
                    continue;
                }
                if (cell.instrument != dlg.instrument()) {
                    if (!undo_pushed) {
                        doc_->push_undo();
                        undo_pushed = true;
                    }
                    doc_->set_instrument(ec.ch, ec.row, dlg.instrument());
                    changed++;
                }
            }
            grid_->update();
            update_status_label();
            append_log(QString("Instrument set to %1 on %2 note(s)")
                .arg(dlg.instrument(), 2, 16, QChar('0')).toUpper()
                .arg(changed));
        }
    });

    // Attenuation dialog (Enter / double-click on Attn column)
    connect(grid_, &TrackerGridWidget::attn_dialog_requested, this, [this](int ch, int row) {
        const auto& c = doc_->cell(ch, row);
        AttnInputDialog dlg(c.attn, this);
        if (dlg.exec() == QDialog::Accepted) {
            const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
            int changed = 0;
            bool undo_pushed = false;
            for (const EditCell& ec : edit_cells) {
                const auto& cell = doc_->cell(ec.ch, ec.row);
                if (!cell.is_note_on()) {
                    continue;
                }
                if (cell.attn != dlg.attn()) {
                    if (!undo_pushed) {
                        doc_->push_undo();
                        undo_pushed = true;
                    }
                    doc_->set_attn(ec.ch, ec.row, dlg.attn());
                    changed++;
                }
            }
            grid_->update();
            update_status_label();
            uint8_t a = dlg.attn();
            if (a == 0xFF) {
                append_log(QString("Attn set to AUTO on %1 note(s)").arg(changed));
            } else {
                append_log(QString("Attn set to %1 on %2 note(s)").arg(a).arg(changed));
            }
        }
    });

    connect(grid_, &TrackerGridWidget::play_stop_toggled, this, [this]() {
        if (playing_) stop_playback(); else start_playback();
    });

    // Undo/Redo
    connect(grid_, &TrackerGridWidget::undo_requested, this, [this]() {
        doc_->undo();
        grid_->update();
    });
    connect(grid_, &TrackerGridWidget::redo_requested, this, [this]() {
        doc_->redo();
        grid_->update();
    });

    // Global undo/redo shortcuts for TrackerTab (also work when focus is not on grid)
    auto* undo_shortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undo_shortcut, &QShortcut::activated, this, [this]() {
        doc_->undo();
        grid_->update();
    });
    auto* redo_shortcut = new QShortcut(QKeySequence::Redo, this);
    connect(redo_shortcut, &QShortcut::activated, this, [this]() {
        doc_->redo();
        grid_->update();
    });

    // Copy/Cut/Paste
    connect(grid_, &TrackerGridWidget::copy_requested, this, [this]() {
        int start = grid_->cursor_row();
        int end = start;
        if (grid_->has_selection()) {
            start = grid_->sel_start_row();
            end = grid_->sel_end_row();
        }
        doc_->copy(grid_->cursor_ch(), start, end, clipboard_);
        int count = end - start + 1;
        append_log(QString("Copied %1 row(s) from Ch%2").arg(count).arg(grid_->cursor_ch()));
    });
    connect(grid_, &TrackerGridWidget::cut_requested, this, [this]() {
        int start = grid_->cursor_row();
        int end = start;
        if (grid_->has_selection()) {
            start = grid_->sel_start_row();
            end = grid_->sel_end_row();
        }
        doc_->cut(grid_->cursor_ch(), start, end, clipboard_);
        grid_->clear_selection();
        grid_->update();
        int count = end - start + 1;
        append_log(QString("Cut %1 row(s) from Ch%2").arg(count).arg(grid_->cursor_ch()));
    });
    connect(grid_, &TrackerGridWidget::paste_requested, this, [this]() {
        if (clipboard_.row_count() == 0) return;
        doc_->paste(grid_->cursor_ch(), grid_->cursor_row(), clipboard_);
        grid_->update();
        append_log(QString("Pasted %1 row(s)").arg(clipboard_.row_count()));
    });

    // Select all
    connect(grid_, &TrackerGridWidget::select_all_requested, this, [this]() {
        grid_->select_all();
    });

    // Transpose
    connect(grid_, &TrackerGridWidget::transpose_requested, this, [this](int semitones) {
        int start = grid_->cursor_row();
        int end = start;
        if (grid_->has_selection()) {
            start = grid_->sel_start_row();
            end = grid_->sel_end_row();
        }
        doc_->transpose(grid_->cursor_ch(), start, end, semitones);
        grid_->update();
        QString dir = (semitones > 0) ? "+" : "";
        append_log(QString("Transpose %1%2 semitones").arg(dir).arg(semitones));
    });

    // Transport shortcuts from grid
    connect(grid_, &TrackerGridWidget::play_from_start_requested, this, [this]() {
        start_playback_from_start();
    });
    connect(grid_, &TrackerGridWidget::stop_requested, this, [this]() {
        stop_playback();
    });
    connect(grid_, &TrackerGridWidget::clear_pattern_requested, this, [this]() {
        doc_->push_undo();
        doc_->clear_all();
        grid_->update();
        append_log("Pattern cleared.");
    });

    // Row operations
    connect(grid_, &TrackerGridWidget::insert_row_requested, this, [this]() {
        doc_->insert_row_all(grid_->cursor_row());
        grid_->update();
        append_log(QString("Inserted row at %1").arg(grid_->cursor_row()));
    });
    connect(grid_, &TrackerGridWidget::delete_row_requested, this, [this]() {
        doc_->delete_row_all(grid_->cursor_row());
        grid_->update();
        append_log(QString("Deleted row %1").arg(grid_->cursor_row()));
    });
    connect(grid_, &TrackerGridWidget::duplicate_row_requested, this, [this]() {
        doc_->duplicate_row(grid_->cursor_ch(), grid_->cursor_row());
        grid_->move_cursor(1, 0, 0);
        grid_->update();
        append_log("Row duplicated.");
    });

    // Interpolation
    connect(grid_, &TrackerGridWidget::interpolate_requested, this, [this]() {
        if (!grid_->has_selection() && !grid_->has_discrete_selection()) {
            append_log("Select a range first (Shift+arrows) or discrete cells (Ctrl+click).");
            return;
        }
        enum class InterpMode { Inst, Attn, FxParam };
        InterpMode mode = InterpMode::Attn;
        QString mode_name = "attn";
        switch (grid_->cursor_sub()) {
        case TrackerGridWidget::SubInst:
            mode = InterpMode::Inst;
            mode_name = "instrument";
            break;
        case TrackerGridWidget::SubAttn:
            mode = InterpMode::Attn;
            mode_name = "attn";
            break;
        case TrackerGridWidget::SubFxP:
            mode = InterpMode::FxParam;
            mode_name = "fx param";
            break;
        default:
            append_log("Interpolation works on Inst / Attn / FX param columns (place cursor there).");
            return;
        }

        const std::vector<EditCell> edit_cells = current_edit_cells(
            grid_, grid_->cursor_ch(), grid_->cursor_row());
        std::array<std::vector<int>, 4> rows_by_ch{};
        for (const EditCell& ec : edit_cells) {
            if (ec.ch < 0 || ec.ch >= 4 || ec.row < 0 || ec.row >= doc_->length()) {
                continue;
            }
            rows_by_ch[static_cast<size_t>(ec.ch)].push_back(ec.row);
        }
        for (int ch = 0; ch < 4; ++ch) {
            auto& rows = rows_by_ch[static_cast<size_t>(ch)];
            std::sort(rows.begin(), rows.end());
            rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
        }

        bool undo_pushed = false;
        int changed = 0;
        int touched = 0;
        for (int ch = 0; ch < 4; ++ch) {
            const auto& rows = rows_by_ch[static_cast<size_t>(ch)];
            if (rows.size() < 2) continue;

            const int first_row = rows.front();
            const int last_row = rows.back();
            const int span = std::max(1, last_row - first_row);

            const TrackerCell c0 = doc_->cell(ch, first_row);
            const TrackerCell c1 = doc_->cell(ch, last_row);
            int v0 = 0;
            int v1 = 0;
            switch (mode) {
            case InterpMode::Inst:
                v0 = c0.instrument;
                v1 = c1.instrument;
                break;
            case InterpMode::Attn:
                v0 = (c0.attn == 0xFF) ? 0 : c0.attn;
                v1 = (c1.attn == 0xFF) ? 15 : c1.attn;
                break;
            case InterpMode::FxParam:
                v0 = c0.fx_param;
                v1 = c1.fx_param;
                break;
            }

            for (int row : rows) {
                const TrackerCell cur = doc_->cell(ch, row);
                const double t = static_cast<double>(row - first_row) / static_cast<double>(span);
                int iv = static_cast<int>(std::lround(v0 + (v1 - v0) * t));
                if (mode == InterpMode::Inst) {
                    iv = std::clamp(iv, 0, static_cast<int>(TrackerDocument::kMaxInstrument));
                    if (!cur.is_note_on()) continue;
                    touched++;
                    if (cur.instrument != static_cast<uint8_t>(iv)) {
                        if (!undo_pushed) {
                            doc_->push_undo();
                            undo_pushed = true;
                        }
                        doc_->set_instrument(ch, row, static_cast<uint8_t>(iv));
                        changed++;
                    }
                } else if (mode == InterpMode::Attn) {
                    iv = std::clamp(iv, 0, 15);
                    if (!cur.is_note_on()) continue;
                    touched++;
                    if (cur.attn != static_cast<uint8_t>(iv)) {
                        if (!undo_pushed) {
                            doc_->push_undo();
                            undo_pushed = true;
                        }
                        doc_->set_attn(ch, row, static_cast<uint8_t>(iv));
                        changed++;
                    }
                } else {
                    iv = std::clamp(iv, 0, 255);
                    if (!cur.has_fx()) continue;
                    touched++;
                    if (cur.fx_param != static_cast<uint8_t>(iv)) {
                        if (!undo_pushed) {
                            doc_->push_undo();
                            undo_pushed = true;
                        }
                        doc_->set_fx_param(ch, row, static_cast<uint8_t>(iv));
                        changed++;
                    }
                }
            }
        }

        grid_->update();
        if (touched == 0) {
            append_log(QString("Interpolation skipped: no compatible cells for %1.").arg(mode_name));
        } else {
            append_log(QString("Interpolated %1 on %2 cell(s). Changed: %3.")
                           .arg(mode_name).arg(touched).arg(changed));
        }
    });

    // Humanize (light random variation on attenuation)
    connect(grid_, &TrackerGridWidget::humanize_requested, this, [this]() {
        bool ok = false;
        const int depth = QInputDialog::getInt(
            this,
            "Humanize Attn",
            "Random range (+/- steps):",
            1, 1, 4, 1, &ok);
        if (!ok) return;

        const int probability = QInputDialog::getInt(
            this,
            "Humanize Attn",
            "Apply probability (%):",
            100, 1, 100, 1, &ok);
        if (!ok) return;

        const std::vector<EditCell> edit_cells = current_edit_cells(
            grid_, grid_->cursor_ch(), grid_->cursor_row());
        if (edit_cells.empty()) {
            append_log("Humanize skipped: no target cells.");
            return;
        }

        std::array<bool, TrackerDocument::kChannelCount * TrackerDocument::kMaxLength> seen{};
        seen.fill(false);
        std::vector<EditCell> unique_cells;
        unique_cells.reserve(edit_cells.size());
        for (const EditCell& ec : edit_cells) {
            if (ec.ch < 0 || ec.ch >= TrackerDocument::kChannelCount ||
                ec.row < 0 || ec.row >= doc_->length()) {
                continue;
            }
            const int id = ec.row * TrackerDocument::kChannelCount + ec.ch;
            if (id < 0 || id >= static_cast<int>(seen.size())) continue;
            if (seen[static_cast<size_t>(id)]) continue;
            seen[static_cast<size_t>(id)] = true;
            unique_cells.push_back(ec);
        }

        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> delta_dist(-depth, depth);
        std::uniform_int_distribution<int> prob_dist(1, 100);

        bool undo_pushed = false;
        int touched = 0;
        int changed = 0;
        int auto_materialized = 0;

        for (const EditCell& ec : unique_cells) {
            const TrackerCell cur = doc_->cell(ec.ch, ec.row);
            if (!cur.is_note_on()) continue;
            touched++;
            if (prob_dist(rng) > probability) continue;

            const ngpc::BgmInstrumentDef def = resolve_preview_instrument(
                store_, static_cast<uint8_t>(cur.instrument));
            const int base_attn = (cur.attn == 0xFF) ? static_cast<int>(def.attn) : static_cast<int>(cur.attn);
            const int next_attn = std::clamp(base_attn + delta_dist(rng), 0, 15);

            const bool would_change = (cur.attn == 0xFF)
                ? (next_attn != base_attn)
                : (next_attn != static_cast<int>(cur.attn));
            if (!would_change) continue;

            if (!undo_pushed) {
                doc_->push_undo();
                undo_pushed = true;
            }
            doc_->set_attn(ec.ch, ec.row, static_cast<uint8_t>(next_attn));
            if (cur.attn == 0xFF) auto_materialized++;
            changed++;
        }

        grid_->update();
        update_status_label();
        append_log(QString("Humanize attn: touched %1 note(s), changed %2 (depth +/-%3, prob %4%%, auto->explicit %5).")
                       .arg(touched).arg(changed).arg(depth).arg(probability).arg(auto_materialized));
    });

    // Batch apply (set current field to a fixed value on selection)
    connect(grid_, &TrackerGridWidget::batch_apply_requested, this, [this]() {
        enum class BatchMode { Inst, Attn, FxCmd, FxParam };
        BatchMode mode = BatchMode::Attn;
        QString field_name = "attn";
        int value = 0;
        bool ok = false;

        switch (grid_->cursor_sub()) {
        case TrackerGridWidget::SubInst:
            mode = BatchMode::Inst;
            field_name = "instrument";
            value = QInputDialog::getInt(
                this, "Batch Apply", "Instrument value (0..127):",
                0, 0, static_cast<int>(TrackerDocument::kMaxInstrument), 1, &ok);
            break;
        case TrackerGridWidget::SubAttn:
            mode = BatchMode::Attn;
            field_name = "attn";
            value = QInputDialog::getInt(
                this, "Batch Apply", "Attn value (-1=AUTO, 0..15):",
                -1, -1, 15, 1, &ok);
            break;
        case TrackerGridWidget::SubFx:
            mode = BatchMode::FxCmd;
            field_name = "fx";
            value = QInputDialog::getInt(
                this, "Batch Apply", "FX command (0..15 hex nibble):",
                0, 0, 15, 1, &ok);
            break;
        case TrackerGridWidget::SubFxP:
            mode = BatchMode::FxParam;
            field_name = "fx param";
            value = QInputDialog::getInt(
                this, "Batch Apply", "FX param (0..255):",
                0, 0, 255, 1, &ok);
            break;
        default:
            append_log("Batch apply works on Inst / Attn / FX / FX param columns.");
            return;
        }
        if (!ok) return;

        const std::vector<EditCell> edit_cells = current_edit_cells(
            grid_, grid_->cursor_ch(), grid_->cursor_row());
        if (edit_cells.empty()) {
            append_log("Batch apply skipped: no target cells.");
            return;
        }

        bool undo_pushed = false;
        int touched = 0;
        int changed = 0;
        for (const EditCell& ec : edit_cells) {
            const TrackerCell cur = doc_->cell(ec.ch, ec.row);

            if (mode == BatchMode::Inst) {
                if (!cur.is_note_on()) continue;
                touched++;
                const uint8_t next = static_cast<uint8_t>(value);
                if (cur.instrument != next) {
                    if (!undo_pushed) { doc_->push_undo(); undo_pushed = true; }
                    doc_->set_instrument(ec.ch, ec.row, next);
                    changed++;
                }
            } else if (mode == BatchMode::Attn) {
                if (!cur.is_note_on()) continue;
                touched++;
                const uint8_t next = (value < 0) ? static_cast<uint8_t>(0xFF)
                                                 : static_cast<uint8_t>(value & 0x0F);
                if (cur.attn != next) {
                    if (!undo_pushed) { doc_->push_undo(); undo_pushed = true; }
                    doc_->set_attn(ec.ch, ec.row, next);
                    changed++;
                }
            } else if (mode == BatchMode::FxCmd) {
                touched++;
                const uint8_t next = static_cast<uint8_t>(value & 0x0F);
                if (cur.fx != next) {
                    if (!undo_pushed) { doc_->push_undo(); undo_pushed = true; }
                    doc_->set_fx(ec.ch, ec.row, next);
                    changed++;
                }
            } else {
                touched++;
                const uint8_t next = static_cast<uint8_t>(value & 0xFF);
                if (cur.fx_param != next) {
                    if (!undo_pushed) { doc_->push_undo(); undo_pushed = true; }
                    doc_->set_fx_param(ec.ch, ec.row, next);
                    changed++;
                }
            }
        }

        grid_->update();
        update_status_label();
        append_log(QString("Batch apply %1: touched %2 cell(s), changed %3.")
                       .arg(field_name).arg(touched).arg(changed));
    });

    // Channel header click -> toggle mute
    connect(grid_, &TrackerGridWidget::channel_header_clicked, this, [this](int ch) {
        if (ch >= 0 && ch < 4) {
            mute_btns_[ch]->toggle();
        }
    });

    // Cursor moved -> update status bar
    connect(grid_, &TrackerGridWidget::cursor_moved, this, [this](int, int) {
        update_status_label();
    });

    // Mute buttons
    for (int ch = 0; ch < 4; ++ch) {
        connect(mute_btns_[ch], &QPushButton::toggled, this, [this, ch](bool checked) {
            (void)checked;
            if (solo_channel_ == ch) {
                solo_channel_ = -1;
                solo_btns_[ch]->setChecked(false);
            }
            update_mute_state();
        });
    }

    // Solo buttons (exclusive)
    for (int ch = 0; ch < 4; ++ch) {
        connect(solo_btns_[ch], &QPushButton::toggled, this, [this, ch](bool checked) {
            if (checked) {
                solo_channel_ = ch;
                for (int i = 0; i < 4; ++i) {
                    if (i != ch) solo_btns_[i]->setChecked(false);
                }
            } else {
                if (solo_channel_ == ch) solo_channel_ = -1;
            }
            update_mute_state();
        });
    }

    // Playback timer
    connect(play_timer_, &QTimer::timeout, this, &TrackerTab::on_tick);

    // File operations (buttons)
    connect(save_btn_, &QPushButton::clicked, this, &TrackerTab::on_save);
    connect(load_btn_, &QPushButton::clicked, this, &TrackerTab::on_load);
    connect(export_btn_, &QPushButton::clicked, this, &TrackerTab::on_export);
    connect(export_asm_btn_, &QPushButton::clicked, this, &TrackerTab::on_export_asm);

    // WAV export
    connect(wav_btn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export WAV",
                                                     QString(), "WAV Audio (*.wav)");
        if (path.isEmpty()) return;

        WavExporter::Settings ws;
        ws.sample_rate = 44100;
        ws.ticks_per_row = tpr_spin_->value();
        ws.song_mode = (song_->order_length() > 1 || song_->pattern_count() > 1);
        ws.max_loops = 1;

        append_log("Exporting WAV...");
        QString err;
        if (WavExporter::render_to_file(path, song_, store_, ws, &err)) {
            append_log(QString("WAV exported to %1").arg(path));
        } else {
            append_log(QString("ERROR WAV export: %1").arg(err));
        }
    });

    // MIDI import
    connect(midi_btn, &QPushButton::clicked, this, &TrackerTab::on_import_midi);

    auto apply_selected_template = [this, template_combo]() {
        if (!doc_ || !grid_ || !template_combo) return;

        int row_start = 0;
        int row_end = doc_->length() - 1;
        if (grid_->has_selection()) {
            row_start = grid_->sel_start_row();
            row_end = grid_->sel_end_row();
        } else if (grid_->has_discrete_selection()) {
            const auto cells = grid_->selected_cells();
            if (!cells.empty()) {
                row_start = doc_->length() - 1;
                row_end = 0;
                for (const auto& p : cells) {
                    row_start = std::min(row_start, p.second);
                    row_end = std::max(row_end, p.second);
                }
            }
        }
        row_start = std::clamp(row_start, 0, doc_->length() - 1);
        row_end = std::clamp(row_end, 0, doc_->length() - 1);
        if (row_start > row_end) std::swap(row_start, row_end);

        const int tpl = template_combo->currentIndex();
        const int tone_ch = (grid_->cursor_ch() < 3) ? grid_->cursor_ch() : 0;
        const int tone_ch_b = (tone_ch + 1) % 3;
        const int rows_span = (row_end - row_start + 1);

        std::array<bool, 4> touched_ch{};
        touched_ch.fill(false);
        switch (tpl) {
        case 0:
        case 1:
        case 2:
        case 6:
        case 7:
            touched_ch[3] = true;
            break;
        case 3:
        case 4:
            touched_ch[static_cast<size_t>(tone_ch)] = true;
            break;
        case 8:
            touched_ch[static_cast<size_t>(tone_ch)] = true;
            touched_ch[static_cast<size_t>(tone_ch_b)] = true;
            break;
        default:
            touched_ch = {true, true, true, true};
            break;
        }

        const auto answer = QMessageBox::question(
            this,
            "Apply Template",
            "Clear target channel rows before applying template?\n"
            "Yes = clean apply, No = merge with existing data.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (answer == QMessageBox::Cancel) return;
        const bool clear_first = (answer == QMessageBox::Yes);

        auto set_note = [this](int ch, int row, uint8_t note) {
            if (ch < 0 || ch >= 4 || row < 0 || row >= doc_->length()) return;
            doc_->set_note(ch, row, note);
        };
        auto set_inst = [this](int ch, int row, uint8_t inst) {
            if (ch < 0 || ch >= 4 || row < 0 || row >= doc_->length()) return;
            doc_->set_instrument(ch, row, inst);
        };
        auto set_attn = [this](int ch, int row, uint8_t attn) {
            if (ch < 0 || ch >= 4 || row < 0 || row >= doc_->length()) return;
            doc_->set_attn(ch, row, attn);
        };
        auto set_fx = [this](int ch, int row, uint8_t fx, uint8_t fx_param) {
            if (ch < 0 || ch >= 4 || row < 0 || row >= doc_->length()) return;
            doc_->set_fx(ch, row, fx);
            doc_->set_fx_param(ch, row, fx_param);
        };
        auto write_cell = [&](int ch, int row, uint8_t note, uint8_t inst, uint8_t attn, uint8_t fx = 0, uint8_t fx_param = 0) {
            set_note(ch, row, note);
            // In merge mode, keep existing instruments to avoid destructive overwrite.
            if (clear_first) {
                set_inst(ch, row, inst);
                set_attn(ch, row, attn);
                set_fx(ch, row, fx, fx_param);
            }
        };

        doc_->push_undo();
        if (clear_first) {
            for (int ch = 0; ch < 4; ++ch) {
                if (!touched_ch[static_cast<size_t>(ch)]) continue;
                for (int row = row_start; row <= row_end; ++row) {
                    doc_->clear_cell(ch, row);
                }
            }
        }

        int notes_written = 0;
        const uint8_t kKick = 3;
        const uint8_t kSnare = 6;
        const uint8_t kHat = 8;
        const uint8_t kAttnAuto = 0xFF;
        const uint8_t kAttnKick = 1;
        const uint8_t kAttnSnare = 2;
        const uint8_t kAttnHat = 5;
        auto pick_inst = [this](uint8_t wanted) -> uint8_t {
            if (!store_ || store_->count() <= 0) return 0;
            const int max_idx = store_->count() - 1;
            return static_cast<uint8_t>(std::clamp<int>(wanted, 0, max_idx));
        };
        const uint8_t kInstKick = pick_inst(1);
        const uint8_t kInstHat = pick_inst(2);
        const uint8_t kInstSnare = pick_inst(3);
        const uint8_t kInstLead = pick_inst(4);
        const uint8_t kInstPluck = pick_inst(6);
        const uint8_t kInstBass = pick_inst(7);
        const uint8_t kInstWide = pick_inst(11);

        for (int row = row_start; row <= row_end; ++row) {
            const int rel = row - row_start;
            switch (tpl) {
            case 0: // Kick 4/4
                if ((rel % 4) == 0) {
                    write_cell(3, row, kKick, kInstKick, kAttnKick);
                    notes_written++;
                }
                break;
            case 1: // Snare backbeat
                if ((rel % 8) == 4) {
                    write_cell(3, row, kSnare, kInstSnare, kAttnSnare);
                    notes_written++;
                }
                break;
            case 2: // Hi-hat 8ths
                if ((rel % 2) == 0) {
                    write_cell(3, row, kHat, kInstHat, kAttnHat, 0xC, 0x02);
                    notes_written++;
                }
                break;
            case 3: { // Bass pulse
                if ((rel % 4) == 0) {
                    static const uint8_t kSeq[] = {
                        /* C2 C2 G1 G1 */
                        25, 25, 20, 20
                    };
                    const int idx = (rel / 4) % 4;
                    write_cell(tone_ch, row, kSeq[idx], kInstBass, kAttnAuto);
                    notes_written++;
                }
                break;
            }
            case 4: { // Arp triad 8ths
                if ((rel % 2) == 0) {
                    static const uint8_t kSeq[] = {
                        /* C4 E4 G4 E4 */
                        49, 53, 56, 53
                    };
                    const int idx = (rel / 2) % 4;
                    write_cell(tone_ch, row, kSeq[idx], kInstPluck, kAttnAuto);
                    notes_written++;
                }
                break;
            }
            case 6: { // Kick + hat groove
                if ((rel % 8) == 0 || (rel % 8) == 5) {
                    write_cell(3, row, kKick, kInstKick, kAttnKick);
                    notes_written++;
                } else if ((rel % 2) == 0) {
                    const uint8_t hat_attn = ((rel % 8) == 2 || (rel % 8) == 6) ? 4 : 6;
                    write_cell(3, row, kHat, kInstHat, hat_attn, 0xC, 0x02);
                    notes_written++;
                }
                break;
            }
            case 7: { // Snare fill 16ths
                const bool fill_zone = (rows_span >= 8) && (rel >= rows_span - 8);
                if (fill_zone) {
                    if ((rel % 2) == 0) {
                        write_cell(3, row, kSnare, kInstSnare, 3, 0xC, 0x01);
                        notes_written++;
                    }
                } else {
                    if ((rel % 8) == 4) {
                        write_cell(3, row, kSnare, kInstSnare, kAttnSnare);
                        notes_written++;
                    } else if ((rel % 2) == 0) {
                        write_cell(3, row, kHat, kInstHat, 6, 0xC, 0x02);
                        notes_written++;
                    }
                }
                break;
            }
            case 8: { // Bass + arp duo
                if ((rel % 4) == 0) {
                    static const uint8_t kBassSeq[] = {25, 25, 20, 20, 22, 22, 20, 20};
                    const int idx = (rel / 4) % 8;
                    write_cell(tone_ch, row, kBassSeq[idx], kInstBass, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 2) == 0) {
                    static const uint8_t kArpSeq[] = {49, 53, 56, 53, 51, 55, 58, 55};
                    const int idx = (rel / 2) % 8;
                    write_cell(tone_ch_b, row, kArpSeq[idx], kInstPluck, kAttnAuto);
                    notes_written++;
                }
                break;
            }
            case 9: { // Full groove loop
                if ((rel % 2) == 0) {
                    static const uint8_t kMel[] = {49, 51, 53, 56, 58, 56, 53, 51, 49, 53, 56, 58, 56, 53, 51, 49};
                    const int mi = (rel / 2) % 16;
                    write_cell(0, row, kMel[mi], kInstLead, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 4) == 0) {
                    static const uint8_t kBass[] = {25, 25, 20, 20, 22, 22, 20, 20};
                    const int bi = (rel / 4) % 8;
                    write_cell(1, row, kBass[bi], kInstBass, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 4) == 2) {
                    static const uint8_t kHarm[] = {41, 44, 39, 44, 43, 46, 41, 46};
                    const int hi = ((rel - 2) / 4) % 8;
                    write_cell(2, row, kHarm[hi], kInstWide, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 8) == 4) {
                    write_cell(3, row, kSnare, kInstSnare, kAttnSnare);
                    notes_written++;
                } else if ((rel % 8) == 0 || (rel % 8) == 5) {
                    write_cell(3, row, kKick, kInstKick, kAttnKick);
                    notes_written++;
                } else if ((rel % 2) == 0) {
                    write_cell(3, row, kHat, kInstHat, kAttnHat, 0xC, 0x02);
                    notes_written++;
                }
                break;
            }
            default: // Chiptune starter (all channels)
                if ((rel % 2) == 0) {
                    static const uint8_t kMel[] = {49, 53, 56, 58, 56, 53, 51, 53};
                    const int mi = (rel / 2) % 8;
                    write_cell(0, row, kMel[mi], kInstLead, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 4) == 0) {
                    static const uint8_t kBass[] = {25, 25, 20, 20};
                    const int bi = (rel / 4) % 4;
                    write_cell(1, row, kBass[bi], kInstBass, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 4) == 2) {
                    static const uint8_t kHarm[] = {41, 44, 39, 44};
                    const int hi = ((rel - 2) / 4) % 4;
                    write_cell(2, row, kHarm[hi], kInstWide, kAttnAuto);
                    notes_written++;
                }
                if ((rel % 8) == 4) {
                    write_cell(3, row, kSnare, kInstSnare, kAttnSnare);
                    notes_written++;
                } else if ((rel % 4) == 0) {
                    write_cell(3, row, kKick, kInstKick, kAttnKick);
                    notes_written++;
                } else if ((rel % 2) == 0) {
                    write_cell(3, row, kHat, kInstHat, kAttnHat, 0xC, 0x02);
                    notes_written++;
                }
                break;
            }
        }

        grid_->update();
        update_status_label();
        append_log(QString("Template applied: %1 | rows %2-%3 | notes written: %4%5")
                       .arg(template_combo->currentText())
                       .arg(row_start)
                       .arg(row_end)
                       .arg(notes_written)
                       .arg(clear_first ? " | clear=yes" : " | clear=no"));
    };
    connect(template_apply_btn, &QPushButton::clicked, this, apply_selected_template);
    auto* template_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this);
    connect(template_shortcut, &QShortcut::activated, this, apply_selected_template);

    // File operations (keyboard shortcuts from grid)
    connect(grid_, &TrackerGridWidget::save_requested, this, &TrackerTab::on_save);
    connect(grid_, &TrackerGridWidget::load_requested, this, &TrackerTab::on_load);

    // Octave / step change from grid (numpad +/- and */)
    connect(grid_, &TrackerGridWidget::octave_change_requested, this, [this](int delta) {
        octave_spin_->setValue(octave_spin_->value() + delta);
    });
    connect(grid_, &TrackerGridWidget::step_change_requested, this, [this](int delta) {
        step_spin_->setValue(step_spin_->value() + delta);
    });

    // Copy pattern as text to system clipboard
    connect(grid_, &TrackerGridWidget::copy_text_requested, this, [this]() {
        QString text = grid_->selection_to_text();
        QGuiApplication::clipboard()->setText(text);
        append_log(QString("Copied pattern text to clipboard (%1 chars).").arg(text.size()));
    });

    append_log("Tracker ready. Multi-pattern + Song mode. [Song]=play order, [WAV]=export audio. F1-F4=mute.");
}

// ============================================================
// Keyboard reference
// ============================================================

void TrackerTab::update_kb_ref_label() {
    int oct = octave_spin_ ? octave_spin_->value() : 4;
    bool azerty = (kb_layout_combo_ && kb_layout_combo_->currentIndex() == 1);

    QString lo_keys, hi_keys;
    if (azerty) {
        lo_keys = QString(
            "W=C%1  X=C#  C=D%1  D=D#  V=E%1  B=F%1  G=F#  N=G%1  H=G#  ,=A%1  J=A#  ;=B%1"
        ).arg(oct);
        hi_keys = QString(
            "A=C%1  2=C#  Z=D%1  3=D#  E=E%1  R=F%1  5=F#  T=G%1  6=G#  Y=A%1  7=A#  U=B%1"
        ).arg(oct + 1);
    } else {
        lo_keys = QString(
            "Z=C%1  S=C#  X=D%1  D=D#  C=E%1  V=F%1  G=F#  B=G%1  H=G#  N=A%1  J=A#  M=B%1"
        ).arg(oct);
        hi_keys = QString(
            "Q=C%1  2=C#  W=D%1  3=D#  E=E%1  R=F%1  5=F#  T=G%1  6=G#  Y=A%1  7=A#  U=B%1"
        ).arg(oct + 1);
    }

    kb_ref_label_->setText(
        QString("Correspondance notes/clavier\n"
                "Oct %1: %2\n"
                "Oct %3: %4\n\n"
                "Raccourcis\n"
                "Ins=insert  Shift+Del=delete  Ctrl+D=dup  Ctrl+X=cut\n"
                "Ctrl+I=interpolate  Ctrl+H=humanize  Ctrl+B=batch  Ctrl+T=template")
            .arg(oct).arg(lo_keys).arg(oct + 1).arg(hi_keys));
}

// ============================================================
// Status bar
// ============================================================

void TrackerTab::update_status_label() {
    if (!status_label_ || !grid_ || !doc_) return;

    int row = grid_->cursor_row();
    int ch = grid_->cursor_ch();
    const auto& c = doc_->cell(ch, row);

    // Note name
    QString note_str = "---";
    if (c.is_note_off()) {
        note_str = "OFF";
    } else if (c.is_note_on()) {
        static const char* names[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
        const int n = c.note - 1; // tracker note ids are 1-based
        int oct_val = n / 12;
        int semi = n % 12;
        note_str = QString("%1%2").arg(names[semi]).arg(oct_val);
    }

    // Instrument name
    QString inst_str = "---";
    if (c.is_note_on() || c.instrument != 0) {
        inst_str = QString("%1").arg(c.instrument, 2, 16, QChar('0')).toUpper();
        if (store_ && c.instrument < store_->count()) {
            inst_str += QString(" (%1)").arg(
                QString::fromStdString(store_->at(c.instrument).name));
        }
    }

    // Attn
    QString attn_str = "-";
    if (c.attn != 0xFF) {
        attn_str = QString("%1").arg(c.attn, 1, 16).toUpper();
    }

    // FX
    QString fx_str = "---";
    if (c.has_fx()) {
        static const char* kFxNames[] = {
            "Arp", "PitchUp", "PitchDn", "Porta",
            "PitchBend", "Rsv5", "Rsv6", "Rsv7",
            "Rsv8", "Rsv9", "VolSlide", "SetSpeed",
            "NoteCut", "NoteDelay", "HostCmd", "Expr"
        };
        fx_str = QString("%1%2 (%3)")
            .arg(c.fx, 1, 16, QChar('0')).toUpper()
            .arg(c.fx_param, 2, 16, QChar('0')).toUpper()
            .arg(QString::fromLatin1(kFxNames[c.fx & 0x0F]));
    }

    // Selection info
    QString sel_str;
    if (grid_->has_selection()) {
        int count = grid_->sel_end_row() - grid_->sel_start_row() + 1;
        sel_str = QString("  Sel: %1 rows").arg(count);
    }

    QString pat_str = QString("Pat %1/%2")
        .arg(song_ ? song_->active_pattern_index() : 0)
        .arg(song_ ? song_->pattern_count() : 1);

    status_label_->setText(
        QString("%1  Row %2  Ch %3  Note: %4  Inst: %5  Attn: %6  FX: %7%8")
            .arg(pat_str)
            .arg(row, 2, 16, QChar('0')).toUpper()
            .arg(ch)
            .arg(note_str)
            .arg(inst_str)
            .arg(attn_str)
            .arg(fx_str)
            .arg(sel_str));
}

// ============================================================
// BPM display
// ============================================================

void TrackerTab::update_bpm_label() {
    if (!bpm_label_ || !tpr_spin_) return;
    int tpr = tpr_spin_->value();
    double rows_per_sec = 60.0 / tpr;
    double bpm = rows_per_sec / 4.0 * 60.0;
    bpm_label_->setText(QString("~%1 BPM").arg(static_cast<int>(bpm + 0.5)));
}

int TrackerTab::analyze_song_peak_percent(int ticks_per_row) {
    if (!song_) {
        return 0;
    }
    WavExporter::Settings ws;
    ws.sample_rate = 44100;
    ws.ticks_per_row = (ticks_per_row > 0)
        ? ticks_per_row
        : (tpr_spin_ ? tpr_spin_->value() : 8);
    ws.song_mode = true;
    ws.max_loops = 1;

    const std::vector<int16_t> pcm = WavExporter::render_to_pcm(song_, store_, ws);
    if (pcm.empty()) {
        return 0;
    }

    int peak_abs = 0;
    for (int i = 0; i < static_cast<int>(pcm.size()); ++i) {
        int v = static_cast<int>(pcm[static_cast<size_t>(i)]);
        if (v < 0) v = -v;
        if (v > peak_abs) {
            peak_abs = v;
        }
    }
    int peak_percent = static_cast<int>(std::lround((peak_abs * 100.0) / 32767.0));
    peak_percent = std::clamp(peak_percent, 0, 100);
    return peak_percent;
}

int TrackerTab::suggest_song_attn_offset_for_target_peak(int target_peak_percent,
                                                         int ticks_per_row,
                                                         int current_peak_percent) {
    target_peak_percent = std::clamp(target_peak_percent, 1, 100);
    const int peak = (current_peak_percent > 0)
        ? current_peak_percent
        : analyze_song_peak_percent(ticks_per_row);
    if (peak <= 0) {
        return 0;
    }

    const double ratio = static_cast<double>(peak) / static_cast<double>(target_peak_percent);
    if (ratio <= 0.0) {
        return 0;
    }

    // PSG attenuation steps are roughly logarithmic; approximate 2 dB/step.
    const double db = 20.0 * std::log10(ratio);
    int delta = static_cast<int>(std::lround(db / 2.0));
    delta = std::clamp(delta, -8, 8);
    return delta;
}

int TrackerTab::apply_song_attn_offset(int delta) {
    if (!song_ || delta == 0) {
        return 0;
    }
    int changed = 0;
    const int pat_count = song_->pattern_count();
    for (int pi = 0; pi < pat_count; ++pi) {
        TrackerDocument* pat = song_->pattern(pi);
        if (!pat) continue;

        struct Change {
            int ch = 0;
            int row = 0;
            TrackerCell cell;
        };
        std::vector<Change> changes;
        const int len = pat->length();
        for (int ch = 0; ch < TrackerDocument::kChannelCount; ++ch) {
            for (int row = 0; row < len; ++row) {
                const TrackerCell& c = pat->cell(ch, row);
                if (c.attn == 0xFF) continue;
                const int next = std::clamp(static_cast<int>(c.attn) + delta, 0, 15);
                if (next == static_cast<int>(c.attn)) continue;
                TrackerCell n = c;
                n.attn = static_cast<uint8_t>(next);
                changes.push_back(Change{ch, row, n});
            }
        }

        if (!changes.empty()) {
            pat->push_undo();
            for (const Change& c : changes) {
                pat->set_cell(c.ch, c.row, c.cell);
                ++changed;
            }
        }
    }
    return changed;
}

// ============================================================
// Playback (delegates to TrackerPlaybackEngine)
// ============================================================

void TrackerTab::start_playback() {
    if (playing_) return;

    if (hub_) hub_->set_step_z80(false);

    if (!hub_ || !hub_->ensure_audio_running(44100)) {
        append_log("ERROR: Audio engine not ready.");
        return;
    }

    song_mode_ = false;
    playing_ = true;
    engine_->set_document(doc_);
    engine_->set_ticks_per_row(tpr_spin_->value());
    engine_->start(grid_->cursor_row());
    grid_->set_playback_row(engine_->current_row());

    play_btn_->setText("Pause [Space]");
    play_timer_->start();
}

void TrackerTab::start_playback_from_start() {
    stop_playback();

    if (hub_) hub_->set_step_z80(false);

    if (!hub_ || !hub_->ensure_audio_running(44100)) {
        append_log("ERROR: Audio engine not ready.");
        return;
    }

    song_mode_ = false;
    playing_ = true;
    engine_->set_document(doc_);
    engine_->set_ticks_per_row(tpr_spin_->value());
    engine_->start(0);
    grid_->set_playback_row(0);

    play_btn_->setText("Pause [Space]");
    play_timer_->start();
}

void TrackerTab::start_loop_selection() {
    if (!grid_->has_selection()) {
        append_log("Select rows first (Shift+Up/Down) to loop.");
        return;
    }

    stop_playback();

    if (hub_) hub_->set_step_z80(false);
    if (!hub_ || !hub_->ensure_audio_running(44100)) {
        append_log("ERROR: Audio engine not ready.");
        return;
    }

    int sel_start = grid_->sel_start_row();
    int sel_end = grid_->sel_end_row();

    song_mode_ = false;
    playing_ = true;
    engine_->set_document(doc_);
    engine_->set_ticks_per_row(tpr_spin_->value());
    engine_->set_loop_range(sel_start, sel_end);
    engine_->start(sel_start);
    grid_->set_playback_row(sel_start);

    play_btn_->setText("Pause [Space]");
    play_timer_->start();
    append_log(QString("Looping rows %1-%2").arg(sel_start).arg(sel_end));
}

void TrackerTab::stop_playback() {
    if (!playing_) return;
    play_timer_->stop();
    playing_ = false;
    song_mode_ = false;
    engine_->clear_loop_range();
    engine_->stop();
    silence_all();
    grid_->set_playback_row(-1);
    play_btn_->setText("Play [Space]");

    // Restore engine to current editing pattern
    engine_->set_document(doc_);
    if (preview_player_) {
        preview_player_->stop();
    }
}

void TrackerTab::on_tick() {
    if (!playing_ || !hub_ || !hub_->engine_ready()) {
        stop_playback();
        return;
    }

    engine_->tick();
    write_voices_to_psg();
}

void TrackerTab::write_voices_to_psg() {
    if (!hub_ || !hub_->engine_ready()) return;

    for (int ch = 0; ch < 4; ++ch) {
        bool muted = false;
        if (solo_channel_ >= 0) {
            muted = (ch != solo_channel_);
        } else {
            muted = mute_btns_[ch]->isChecked() || grid_->is_channel_muted(ch);
        }

        auto out = engine_->channel_output(ch);

        if (muted || !out.active) {
            if (ch < 3) {
                psg_helpers::DirectSilenceTone(hub_->engine(), ch);
            } else {
                psg_helpers::DirectSilenceNoise(hub_->engine());
            }
            continue;
        }

        if (ch < 3) {
            psg_helpers::DirectToneCh(hub_->engine(), ch, out.divider, out.attn);
        } else {
            auto nc = TrackerPlaybackEngine::decode_noise_val(out.noise_val);
            psg_helpers::DirectNoise(hub_->engine(), nc.rate, nc.type, out.attn);
        }
    }
}

void TrackerTab::silence_all() {
    if (!hub_ || !hub_->engine_ready()) return;
    for (int ch = 0; ch < 3; ++ch) {
        psg_helpers::DirectSilenceTone(hub_->engine(), ch);
    }
    psg_helpers::DirectSilenceNoise(hub_->engine());
}

void TrackerTab::update_mute_state() {
    bool channel_muted[4] = {};

    if (solo_channel_ >= 0) {
        for (int i = 0; i < 4; ++i) {
            channel_muted[i] = (i != solo_channel_);
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            channel_muted[i] = mute_btns_[i]->isChecked();
        }
    }

    for (int i = 0; i < 4; ++i) {
        grid_->set_channel_muted(i, channel_muted[i]);
    }
}

// ============================================================
// Edit signal handlers
// ============================================================

void TrackerTab::on_note_entered(int ch, int row, uint8_t note) {
    doc_->push_undo();
    TrackerCell c = doc_->cell(ch, row);
    c.note = note;
    doc_->set_cell(ch, row, c);
    update_status_label();

    // Preview note
    preview_note(note, ch);
}

void TrackerTab::on_note_off_entered(int ch, int row) {
    doc_->push_undo();
    doc_->set_note(ch, row, 0xFF);
    update_status_label();
}

void TrackerTab::on_cell_cleared(int ch, int row) {
    doc_->push_undo();
    doc_->clear_cell(ch, row);
    update_status_label();
}

void TrackerTab::on_instrument_digit(int ch, int row, int hex_digit) {
    const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
    int changed = 0;
    int touched = 0;
    bool undo_pushed = false;
    for (const EditCell& ec : edit_cells) {
        const auto& c = doc_->cell(ec.ch, ec.row);
        if (!c.is_note_on()) {
            continue;
        }
        touched++;
        // Shift previous low nibble, append new nibble (works for single and multi-selection).
        uint8_t inst = static_cast<uint8_t>(((c.instrument & 0x0F) << 4) | (hex_digit & 0x0F));
        inst = std::min<uint8_t>(inst, TrackerDocument::kMaxInstrument);
        if (c.instrument != inst) {
            if (!undo_pushed) {
                doc_->push_undo();
                undo_pushed = true;
            }
            doc_->set_instrument(ec.ch, ec.row, inst);
            changed++;
        }
    }
    if (touched == 0) {
        return;
    }
    if (changed > 1) {
        append_log(QString("Instrument nibble applied on %1 note(s).").arg(changed));
    }
    update_status_label();
}

void TrackerTab::on_attn_digit(int ch, int row, int hex_digit) {
    const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
    int changed = 0;
    int touched = 0;
    bool undo_pushed = false;
    const uint8_t attn = static_cast<uint8_t>(hex_digit & 0x0F);
    for (const EditCell& ec : edit_cells) {
        const auto& c = doc_->cell(ec.ch, ec.row);
        if (!c.is_note_on()) {
            continue;
        }
        touched++;
        if (c.attn != attn) {
            if (!undo_pushed) {
                doc_->push_undo();
                undo_pushed = true;
            }
            doc_->set_attn(ec.ch, ec.row, attn);
            changed++;
        }
    }
    if (touched == 0) {
        return;
    }
    if (changed > 1) {
        append_log(QString("Attn applied on %1 note(s).").arg(changed));
    }
    update_status_label();
}

void TrackerTab::on_fx_digit(int ch, int row, int col_index, int hex_digit) {
    const std::vector<EditCell> edit_cells = current_edit_cells(grid_, ch, row);
    int changed = 0;
    bool undo_pushed = false;
    for (const EditCell& ec : edit_cells) {
        const auto& c = doc_->cell(ec.ch, ec.row);
        if (col_index == 0) {
            const uint8_t fx = static_cast<uint8_t>(hex_digit & 0x0F);
            if (c.fx != fx) {
                if (!undo_pushed) {
                    doc_->push_undo();
                    undo_pushed = true;
                }
                doc_->set_fx(ec.ch, ec.row, fx);
                changed++;
            }
        } else {
            uint8_t param = static_cast<uint8_t>(((c.fx_param & 0x0F) << 4) | (hex_digit & 0x0F));
            if (c.fx_param != param) {
                if (!undo_pushed) {
                    doc_->push_undo();
                    undo_pushed = true;
                }
                doc_->set_fx_param(ec.ch, ec.row, param);
                changed++;
            }
        }
    }
    if (changed > 1) {
        append_log(QString("FX field applied on %1 cell(s).").arg(changed));
    }
    update_status_label();
}

// ============================================================
// Note preview
// ============================================================

void TrackerTab::preview_note(uint8_t midi_note, int ch) {
    if (!hub_ || playing_) return;

    hub_->set_step_z80(false);
    if (!hub_->ensure_audio_running(44100) || !preview_player_) return;

    const auto& cell = doc_->cell(ch, grid_->cursor_row());
    ngpc::BgmInstrumentDef def = resolve_preview_instrument(
        store_, static_cast<uint8_t>(cell.instrument));
    if (cell.attn != 0xFF) {
        def.attn = static_cast<uint8_t>(cell.attn & 0x0F);
    }

    uint16_t divider = TrackerPlaybackEngine::midi_to_divider(midi_note);
    uint8_t tone_ch = static_cast<uint8_t>(std::clamp(ch, 0, 2));
    if (ch == 3) {
        def.mode = 1;
        def.noise_config = TrackerPlaybackEngine::midi_note_to_noise_val(midi_note);
        divider = 1;
        tone_ch = 0;
    } else {
        def.mode = 0;
    }

    preview_player_->play(def, divider, tone_ch);
    const int token = ++preview_note_token_;
    const int gate_ms = estimate_preview_gate_ms(def);
    const int hard_stop_ms = estimate_preview_hard_stop_ms(def, gate_ms);
    QTimer::singleShot(gate_ms, this, [this, token]() {
        if (playing_) return;
        if (!preview_player_) return;
        if (token != preview_note_token_) return;
        if (preview_player_->is_playing()) preview_player_->note_off();
    });
    // Safety fallback: hard stop in case a custom instrument never reaches silence.
    QTimer::singleShot(hard_stop_ms, this, [this, token]() {
        if (playing_) return;
        if (!preview_player_) return;
        if (token != preview_note_token_) return;
        if (preview_player_->is_playing()) preview_player_->stop();
    });
}

// ============================================================
// Save / Load
// ============================================================

void TrackerTab::on_save() {
    QString path = QFileDialog::getSaveFileName(this, "Save Song / Pattern",
                                                 QString(),
                                                 "NGPC Song (*.ngps);;NGPC Pattern (*.ngpat);;JSON (*.json)");
    if (path.isEmpty()) return;

    QString error;
    if (save_song_to_path(path, &error)) {
        append_log(QString("Saved to %1").arg(path));
    } else {
        append_log(QString("ERROR: %1").arg(error));
    }
}

void TrackerTab::on_load() {
    QString path = QFileDialog::getOpenFileName(this, "Load Song / Pattern",
                                                 QString(),
                                                 "All supported (*.ngps *.ngpat *.json);;NGPC Song (*.ngps);;NGPC Pattern (*.ngpat);;All (*)");
    if (path.isEmpty()) return;

    QString error;
    if (load_song_from_path(path, &error)) {
        append_log(QString("Loaded from %1 (%2 patterns, order length %3)")
            .arg(path).arg(song_->pattern_count()).arg(song_->order_length()));
    } else {
        append_log(QString("ERROR: %1").arg(error));
    }
}

bool TrackerTab::save_song_to_path(const QString& path, QString* error) {
    if (path.isEmpty()) {
        if (error) *error = "Empty save path";
        return false;
    }

    QByteArray data;
    if (path.endsWith(".ngpat", Qt::CaseInsensitive) || path.endsWith(".json", Qt::CaseInsensitive)) {
        data = doc_->to_json(); // pattern only
    } else {
        data = song_->to_json(); // full song
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QString("Could not write %1").arg(path);
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        if (error) *error = QString("Could not commit %1").arg(path);
        return false;
    }
    return true;
}

bool TrackerTab::load_song_from_path(const QString& path, QString* error) {
    if (path.isEmpty()) {
        if (error) *error = "Empty load path";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("Could not open %1").arg(path);
        return false;
    }

    const QByteArray data = f.readAll();
    f.close();

    bool loaded = false;
    if (path.endsWith(".ngps", Qt::CaseInsensitive)) {
        loaded = song_->from_json(data);
    } else {
        loaded = song_->from_json(data);
        if (!loaded) {
            loaded = song_->import_ngpat(data);
        }
    }
    if (!loaded) {
        if (error) *error = QString("Invalid file %1").arg(path);
        return false;
    }

    doc_ = song_->active_pattern();
    grid_->set_document(doc_);
    engine_->set_document(doc_);
    length_spin_->setValue(doc_->length());
    refresh_pattern_ui();
    refresh_order_list();
    grid_->set_cursor(0, 0, TrackerGridWidget::SubNote);
    grid_->update();
    return true;
}

std::array<bool, 128> TrackerTab::collect_used_instruments() const {
    std::array<bool, 128> used{};
    used.fill(false);
    if (!song_) {
        return used;
    }

    auto collect_from_pattern = [&](const TrackerDocument* pat) {
        if (!pat) return;
        for (int row = 0; row < pat->length(); ++row) {
            for (int ch = 0; ch < 4; ++ch) {
                const TrackerCell& c = pat->cell(ch, row);
                if (c.is_note_on()) {
                    used[static_cast<uint8_t>(c.instrument)] = true;
                }
            }
        }
    };

    const auto& order = song_->order();
    if (!order.empty()) {
        for (int ord_pos = 0; ord_pos < order.size(); ++ord_pos) {
            collect_from_pattern(song_->pattern(order[ord_pos]));
        }
        return used;
    }

    for (int i = 0; i < song_->pattern_count(); ++i) {
        collect_from_pattern(song_->pattern(i));
    }
    return used;
}

// ============================================================
// Export — pre-baked (tick-by-tick with effects + instruments)
// ============================================================

TrackerTab::ExportStreams TrackerTab::build_export_streams() const
{
    ExportStreams result;
    result.loop_offsets.fill(0);

    if (!song_ || song_->pattern_count() == 0) return result;

    static constexpr int kMaxDriverNotes = 51;

    // --- Phase 1: Tick-by-tick simulation (like WavExporter) ---

    struct TickSnapshot {
        bool active;
        uint16_t divider;
        uint8_t attn;
        uint8_t noise_val;
    };
    // snapshots[tick_index][channel]
    std::vector<std::array<TickSnapshot, 4>> snapshots;
    int loop_tick = -1; // tick index where loop point starts

    TrackerPlaybackEngine engine;
    engine.set_instrument_store(const_cast<InstrumentStore*>(store_));
    engine.set_ticks_per_row(tpr_spin_->value());

    const auto& order = song_->order();
    if (order.empty()) return result;

    for (int ord_pos = 0; ord_pos < static_cast<int>(order.size()); ++ord_pos) {
        // Record loop point
        if (ord_pos == song_->loop_point()) {
            loop_tick = static_cast<int>(snapshots.size());
        }

        int pat_idx = order[static_cast<size_t>(ord_pos)];
        TrackerDocument* pat = song_->pattern(pat_idx);
        if (!pat) continue;

        engine.set_document(pat);
        engine.start(0);

        bool pattern_done = false;
        bool had_ticks = false;
        while (!pattern_done) {
            engine.tick();
            had_ticks = true;

            std::array<TickSnapshot, 4> snap;
            for (int ch = 0; ch < 4; ++ch) {
                auto out = engine.channel_output(ch);
                snap[static_cast<size_t>(ch)] = {out.active, out.divider, out.attn, out.noise_val};
            }
            snapshots.push_back(snap);

            // Pattern finished detection (same as WavExporter)
            if (engine.current_row() == 0 && engine.tick_counter() == 0 && had_ticks) {
                pattern_done = true;
            }
        }
        engine.stop();
    }

    if (snapshots.empty()) return result;

    // --- Phase 2: Build NOTE_TABLE from unique dividers ---

    std::vector<uint16_t>& note_table = result.note_table;
    bool note_table_capped = false;

    auto find_or_add_divider = [&](uint16_t div) -> int {
        for (size_t i = 0; i < note_table.size(); ++i) {
            if (note_table[i] == div) return static_cast<int>(i);
        }
        if (static_cast<int>(note_table.size()) < kMaxDriverNotes) {
            note_table.push_back(div);
            return static_cast<int>(note_table.size() - 1);
        }
        // Table full — find closest divider
        note_table_capped = true;
        int best_idx = 0;
        int best_diff = std::abs(static_cast<int>(note_table[0]) - static_cast<int>(div));
        for (size_t i = 1; i < note_table.size(); ++i) {
            int diff = std::abs(static_cast<int>(note_table[i]) - static_cast<int>(div));
            if (diff < best_diff) { best_diff = diff; best_idx = static_cast<int>(i); }
        }
        return best_idx;
    };

    // Pre-collect all unique dividers from tone channels
    for (const auto& snap : snapshots) {
        for (int ch = 0; ch < 3; ++ch) {
            if (snap[static_cast<size_t>(ch)].active && snap[static_cast<size_t>(ch)].divider > 0) {
                find_or_add_divider(snap[static_cast<size_t>(ch)].divider);
            }
        }
    }
    if (note_table.empty()) note_table.push_back(1);

    // --- Phase 3: Build streams from snapshots ---

    auto append_event = [](std::vector<uint8_t>& dst, uint8_t opcode, int duration) {
        int remaining = std::max(duration, 1);
        while (remaining > 0) {
            int chunk = std::min(remaining, 255);
            dst.push_back(opcode);
            dst.push_back(static_cast<uint8_t>(chunk));
            remaining -= chunk;
        }
    };

    const int total_ticks = static_cast<int>(snapshots.size());

    for (int ch = 0; ch < 4; ++ch) {
        auto& stream = result.streams[static_cast<size_t>(ch)];
        const bool is_noise = (ch == 3);

        // Track current state
        bool cur_active = false;
        uint16_t cur_divider = 0;
        uint8_t cur_attn = 15;
        uint8_t cur_noise = 0;
        uint8_t cur_note_idx = 0; // 0 = no note playing
        int pending_dur = 0;

        for (int t = 0; t < total_ticks; ++t) {
            // Record loop offset
            if (t == loop_tick) {
                result.loop_offsets[static_cast<size_t>(ch)] = static_cast<uint16_t>(stream.size());
            }

            const auto& s = snapshots[static_cast<size_t>(t)][static_cast<size_t>(ch)];

            if (!s.active) {
                // Channel silent
                if (cur_active && cur_note_idx != 0) {
                    // Was playing a note → flush it, then start rest
                    append_event(stream, cur_note_idx, pending_dur);
                    pending_dur = 0;
                    cur_note_idx = 0;
                }
                cur_active = false;
                // Accumulate rest duration (0xFF)
                if (cur_note_idx == 0) {
                    pending_dur++;
                }
                continue;
            }

            // Channel is active
            uint8_t new_note_idx;
            if (is_noise) {
                new_note_idx = static_cast<uint8_t>((s.noise_val & 0x07) + 1);
            } else {
                new_note_idx = static_cast<uint8_t>(find_or_add_divider(s.divider) + 1);
            }

            bool note_changed = (new_note_idx != cur_note_idx) || !cur_active;
            bool attn_changed = (s.attn != cur_attn);

            if (note_changed || attn_changed) {
                // Flush pending duration
                if (pending_dur > 0) {
                    if (cur_note_idx == 0) {
                        // Was a rest
                        append_event(stream, 0xFF, pending_dur);
                    } else {
                        // Was a note
                        append_event(stream, cur_note_idx, pending_dur);
                    }
                    pending_dur = 0;
                }

                // Emit attenuation change if needed
                if (attn_changed) {
                    stream.push_back(0xF0);
                    stream.push_back(static_cast<uint8_t>(s.attn & 0x0F));
                    cur_attn = s.attn;
                }

                cur_note_idx = new_note_idx;
                cur_active = true;
                cur_divider = s.divider;
                cur_noise = s.noise_val;
            }

            pending_dur++;
        }

        // Flush remaining
        if (pending_dur > 0) {
            if (cur_note_idx == 0) {
                append_event(stream, 0xFF, pending_dur);
            } else {
                append_event(stream, cur_note_idx, pending_dur);
            }
        }

        // End marker
        stream.push_back(0x00);
    }

    return result;
}

// ============================================================
// Export — hybrid (row-based with instrument opcodes)
// ============================================================

TrackerTab::ExportStreams TrackerTab::build_export_streams_hybrid(
    const std::array<uint8_t, 128>* instrument_remap) const
{
    ExportStreams result;
    result.loop_offsets.fill(0);

    if (!song_ || song_->pattern_count() == 0) return result;

    static constexpr int kMaxDriverNotes = 51;

    // --- Collect all unique MIDI notes to build NOTE_TABLE ---

    std::vector<uint16_t>& note_table = result.note_table;

    auto find_or_add_divider = [&](uint16_t div) -> int {
        for (size_t i = 0; i < note_table.size(); ++i) {
            if (note_table[i] == div) return static_cast<int>(i);
        }
        if (static_cast<int>(note_table.size()) < kMaxDriverNotes) {
            note_table.push_back(div);
            return static_cast<int>(note_table.size() - 1);
        }
        // Table full — find closest divider
        int best_idx = 0;
        int best_diff = std::abs(static_cast<int>(note_table[0]) - static_cast<int>(div));
        for (size_t i = 1; i < note_table.size(); ++i) {
            int diff = std::abs(static_cast<int>(note_table[i]) - static_cast<int>(div));
            if (diff < best_diff) { best_diff = diff; best_idx = static_cast<int>(i); }
        }
        return best_idx;
    };

    auto append_event = [](std::vector<uint8_t>& dst, uint8_t opcode, int duration) {
        int remaining = std::max(duration, 1);
        while (remaining > 0) {
            int chunk = std::min(remaining, 255);
            dst.push_back(opcode);
            dst.push_back(static_cast<uint8_t>(chunk));
            remaining -= chunk;
        }
    };

    // Helper: emit instrument inline opcodes (0xF4 + 0xF0-0xF3)
    auto emit_instrument = [&](std::vector<uint8_t>& stream, int inst_idx) {
        const uint8_t src_inst = static_cast<uint8_t>(inst_idx & 0x7F);
        const uint8_t driver_inst = instrument_remap ? (*instrument_remap)[src_inst] : src_inst;
        ngpc::BgmInstrumentDef def;
        if (store_ && src_inst < store_->count()) {
            def = store_->at(src_inst).def;
        }
        // 0xF4 SET_INST (for real driver)
        stream.push_back(0xF4);
        stream.push_back(driver_inst);
        // 0xF0 SET_ATTN (for PlayerTab preview)
        stream.push_back(0xF0);
        stream.push_back(static_cast<uint8_t>(def.attn & 0x0F));
        // 0xF1 SET_ENV
        if (def.env_on) {
            stream.push_back(0xF1);
            stream.push_back(def.env_step);
            stream.push_back(def.env_speed);
        } else {
            stream.push_back(0xF1);
            stream.push_back(0); // step=0 → env off
            stream.push_back(1);
        }
        // 0xF2 SET_VIB
        stream.push_back(0xF2);
        stream.push_back(def.vib_depth);
        stream.push_back(def.vib_speed > 0 ? def.vib_speed : static_cast<uint8_t>(1));
        stream.push_back(def.vib_delay);
        // 0xF3 SET_SWEEP
        if (def.sweep_on) {
            stream.push_back(0xF3);
            stream.push_back(static_cast<uint8_t>(def.sweep_end & 0xFF));
            stream.push_back(static_cast<uint8_t>((def.sweep_end >> 8) & 0xFF));
            stream.push_back(static_cast<uint8_t>(def.sweep_step & 0xFF));
            stream.push_back(def.sweep_speed > 0 ? def.sweep_speed : static_cast<uint8_t>(1));
        }
        // 0xF9 SET_ADSR (legacy 4 params, kept for fallback)
        if (def.adsr_on) {
            stream.push_back(0xF9);
            stream.push_back(def.adsr_attack);
            stream.push_back(def.adsr_decay);
            stream.push_back(def.adsr_sustain);
            stream.push_back(def.adsr_release);
        }
        // 0xFA SET_LFO (legacy LFO1 shorthand, kept for fallback)
        stream.push_back(0xFA);
        stream.push_back(static_cast<uint8_t>(std::clamp<int>(def.lfo_wave, 0, 4)));
        stream.push_back(def.lfo_rate);
        stream.push_back(def.lfo_on ? def.lfo_depth : static_cast<uint8_t>(0));
        // 0xFE EXT 0x01: ADSR5 (A,D,SL,SR,RR)
        if (def.adsr_on) {
            stream.push_back(0xFE);
            stream.push_back(0x01);
            stream.push_back(def.adsr_attack);
            stream.push_back(def.adsr_decay);
            stream.push_back(def.adsr_sustain);
            stream.push_back(def.adsr_sustain_rate);
            stream.push_back(def.adsr_release);
        }
        // 0xFE EXT 0x02: MOD2 (algo + LFO1 + LFO2)
        stream.push_back(0xFE);
        stream.push_back(0x02);
        stream.push_back(static_cast<uint8_t>(std::clamp<int>(def.lfo_algo, 0, 7)));
        stream.push_back(def.lfo_on ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0));
        stream.push_back(static_cast<uint8_t>(std::clamp<int>(def.lfo_wave, 0, 4)));
        stream.push_back(def.lfo_hold);
        stream.push_back(def.lfo_rate);
        stream.push_back(def.lfo_depth);
        stream.push_back(def.lfo2_on ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0));
        stream.push_back(static_cast<uint8_t>(std::clamp<int>(def.lfo2_wave, 0, 4)));
        stream.push_back(def.lfo2_hold);
        stream.push_back(def.lfo2_rate);
        stream.push_back(def.lfo2_depth);
        // 0xFB SET_ENV_CURVE (curve_id)
        stream.push_back(0xFB);
        stream.push_back(def.env_curve_id);
        // 0xFC SET_PITCH_CURVE (curve_id)
        stream.push_back(0xFC);
        stream.push_back(def.pitch_curve_id);
        // 0xFD SET_MACRO (macro_id)
        stream.push_back(0xFD);
        stream.push_back(def.macro_id);
    };

    const auto& order = song_->order();
    if (order.empty()) return result;

    int ticks_per_row = tpr_spin_->value();

    // --- Build streams row by row ---

    for (int ch = 0; ch < 4; ++ch) {
        auto& stream = result.streams[static_cast<size_t>(ch)];
        const bool is_noise = (ch == 3);

        int cur_instrument = -1;
        uint8_t cur_attn = 0xFF; // no override yet

        // Pending state: accumulate durations for same note/silence
        enum PendingType { PEND_NONE, PEND_NOTE, PEND_SILENCE };
        PendingType pending = PEND_NONE;
        uint8_t pending_note_idx = 0;
        int pending_dur = 0;

        auto flush_pending = [&]() {
            if (pending_dur <= 0) return;
            if (pending == PEND_NOTE && pending_note_idx > 0) {
                append_event(stream, pending_note_idx, pending_dur);
            } else if (pending == PEND_SILENCE) {
                append_event(stream, 0xFF, pending_dur);
            }
            pending = PEND_NONE;
            pending_dur = 0;
        };

        int local_tpr = ticks_per_row;

        for (int ord_pos = 0; ord_pos < static_cast<int>(order.size()); ++ord_pos) {
            // Record loop point
            if (ch == 0 || true) { // all channels
                if (ord_pos == song_->loop_point()) {
                    flush_pending();
                    result.loop_offsets[static_cast<size_t>(ch)] = static_cast<uint16_t>(stream.size());
                }
            }

            int pat_idx = order[static_cast<size_t>(ord_pos)];
            TrackerDocument* pat = song_->pattern(pat_idx);
            if (!pat) continue;

            for (int row = 0; row < pat->length(); ++row) {
                const TrackerCell& c = pat->cell(ch, row);
                int dur = local_tpr;
                PendingType resume_pending = PEND_NONE;
                uint8_t resume_note_idx = 0;
                bool resume_after_inline = false;
                auto flush_pending_inline = [&]() {
                    if (pending_dur <= 0) {
                        return;
                    }
                    resume_pending = pending;
                    resume_note_idx = pending_note_idx;
                    resume_after_inline = true;
                    flush_pending();
                };

                // Bxx: speed change
                if (c.fx == 0xB && c.fx_param > 0) {
                    local_tpr = c.fx_param;
                    dur = local_tpr;
                }

                // Exx: host commands (emit on channel 0 only)
                if (c.fx == 0xE && ch == 0) {
                    flush_pending_inline();
                    uint8_t sub = (c.fx_param >> 4) & 0x0F;
                    uint8_t val = c.fx_param & 0x0F;
                    stream.push_back(0xF6); // HOST_CMD
                    stream.push_back(sub);  // type: 0=fade, 1=tempo
                    stream.push_back(val);  // data
                }

                // Fxx: expression (per-channel)
                if (c.fx == 0xF) {
                    flush_pending_inline();
                    uint8_t expr = c.fx_param;
                    if (expr > 15) expr = 15;
                    stream.push_back(0xF7); // SET_EXPR
                    stream.push_back(expr);
                }

                // 4xx: pitch bend (per-channel, signed byte -> s16 LE)
                if (c.fx == 0x4) {
                    flush_pending_inline();
                    int8_t sb = static_cast<int8_t>(c.fx_param);
                    int16_t bend = static_cast<int16_t>(sb);
                    stream.push_back(0xF8); // PITCH_BEND
                    stream.push_back(static_cast<uint8_t>(bend & 0xFF));
                    stream.push_back(static_cast<uint8_t>((bend >> 8) & 0xFF));
                }

                if (c.is_note_on()) {
                    // Flush any pending note/silence
                    flush_pending();

                    // Instrument change?
                    if (static_cast<int>(c.instrument) != cur_instrument) {
                        cur_instrument = c.instrument;
                        emit_instrument(stream, cur_instrument);
                    }

                    // Attn override?
                    if (c.attn != 0xFF) {
                        stream.push_back(0xF0);
                        stream.push_back(static_cast<uint8_t>(c.attn & 0x0F));
                        cur_attn = c.attn;
                    }

                    // Compute note index
                    uint8_t note_idx;
                    if (is_noise) {
                        note_idx = static_cast<uint8_t>((TrackerPlaybackEngine::midi_note_to_noise_val(c.note) & 0x07) + 1);
                    } else {
                        uint16_t div = TrackerPlaybackEngine::midi_to_divider(c.note);
                        note_idx = static_cast<uint8_t>(find_or_add_divider(div) + 1);
                    }

                    // Handle Cxx (note cut)
                    if (c.fx == 0xC) {
                        int cut = std::min(static_cast<int>(c.fx_param), dur);
                        if (cut > 0) {
                            append_event(stream, note_idx, cut);
                        }
                        int rest = dur - cut;
                        if (rest > 0) {
                            pending = PEND_SILENCE;
                            pending_dur = rest;
                        }
                    }
                    // Handle Dxx (note delay)
                    else if (c.fx == 0xD) {
                        int delay = std::min(static_cast<int>(c.fx_param), dur);
                        if (delay > 0) {
                            append_event(stream, 0xFF, delay);
                        }
                        int rest = dur - delay;
                        if (rest > 0) {
                            pending = PEND_NOTE;
                            pending_note_idx = note_idx;
                            pending_dur = rest;
                        }
                    }
                    else {
                        pending = PEND_NOTE;
                        pending_note_idx = note_idx;
                        pending_dur = dur;
                    }
                }
                else if (c.is_note_off()) {
                    flush_pending();
                    pending = PEND_SILENCE;
                    pending_dur = dur;
                }
                else {
                    // Empty row or effect-only row
                    // Attn change on empty row: emit inline
                    if (c.attn != 0xFF && c.attn != cur_attn) {
                        flush_pending_inline();
                        stream.push_back(0xF0);
                        stream.push_back(static_cast<uint8_t>(c.attn & 0x0F));
                        cur_attn = c.attn;
                        // Restart same state (the note/silence continues)
                    }
                    if (resume_after_inline && pending == PEND_NONE) {
                        pending = resume_pending;
                        pending_note_idx = resume_note_idx;
                    }
                    // Accumulate duration
                    if (pending == PEND_NONE) {
                        // No note was playing → silence
                        pending = PEND_SILENCE;
                        pending_dur = dur;
                    } else {
                        pending_dur += dur;
                    }
                }
            }
        }

        // Flush remaining
        flush_pending();

        // End marker
        stream.push_back(0x00);
    }

    if (note_table.empty()) note_table.push_back(1);

    return result;
}

bool TrackerTab::export_song_to_path(const QString& path,
                                     bool asm_export,
                                     bool include_instrument_export,
                                     QString* error,
                                     QString* instrument_export_path,
                                     int forced_export_mode,
                                     const std::array<uint8_t, 128>* instrument_remap) {
    if (instrument_export_path) {
        instrument_export_path->clear();
    }
    if (path.isEmpty()) {
        if (error) *error = "Empty export path";
        return false;
    }

    const int mode_index = (forced_export_mode >= 0)
                               ? forced_export_mode
                               : (export_mode_combo_ ? export_mode_combo_->currentIndex() : 0);
    const bool hybrid = (mode_index == 1);
    const auto es = hybrid ? build_export_streams_hybrid(instrument_remap) : build_export_streams();
    if (es.note_table.empty()) {
        if (error) *error = "Nothing to export";
        return false;
    }

    const QStringList warnings = audit_song_for_export(song_, store_, hybrid);
    const char* mode_label = hybrid ? "Hybrid" : "Pre-baked";
    std::ostringstream out;

    if (!asm_export) {
        out << "/* Generated by NGPC Sound Creator - " << mode_label << " Export */\n";
        out << "/* BGM_CHN noise format: val = stream_byte - 1 (0-7)        */\n";
        out << "/*   rate = val & 0x03 (0=H,1=M,2=L,3=Tone2)               */\n";
        out << "/*   type = (val >> 2) & 0x01 (0=Periodic,1=White)          */\n";
        for (const QString& w : warnings) {
            const QByteArray wb = w.toUtf8();
            out << "/* WARN export: " << wb.constData() << " */\n";
        }
        out << "\n";

        auto write_u8_array = [&](const char* name, const std::vector<uint8_t>& values) {
            out << "const unsigned char " << name << "[] = {\n";
            if (values.empty()) {
                out << "    0x00\n";
            } else {
                for (size_t i = 0; i < values.size(); ++i) {
                    if ((i % 12) == 0) out << "    ";
                    out << static_cast<int>(values[i]);
                    if (i + 1 < values.size()) out << ", ";
                    if ((i % 12) == 11 || i + 1 == values.size()) out << "\n";
                }
            }
            out << "};\n\n";
        };

        std::vector<uint8_t> note_table_bytes;
        note_table_bytes.reserve(es.note_table.size() * 2);
        for (uint16_t d : es.note_table) {
            note_table_bytes.push_back(static_cast<uint8_t>(d & 0x0F));
            note_table_bytes.push_back(static_cast<uint8_t>((d >> 4) & 0x3F));
        }
        write_u8_array("NOTE_TABLE", note_table_bytes);
        out << "const unsigned short BGM_CH0_LOOP = " << es.loop_offsets[0] << ";\n";
        out << "const unsigned short BGM_CH1_LOOP = " << es.loop_offsets[1] << ";\n";
        out << "const unsigned short BGM_CH2_LOOP = " << es.loop_offsets[2] << ";\n";
        out << "const unsigned short BGM_CHN_LOOP = " << es.loop_offsets[3] << ";\n\n";
        write_u8_array("BGM_CH0", es.streams[0]);
        write_u8_array("BGM_CH1", es.streams[1]);
        write_u8_array("BGM_CH2", es.streams[2]);
        write_u8_array("BGM_CHN", es.streams[3]);
        out << "const unsigned char BGM_MONO[] = { 0x00 };\n";
    } else {
        out << "; Generated by NGPC Sound Creator - " << mode_label << " ASM Export\n";
        out << "; Format: TLCS-900H / SNK NGPC toolchain (.inc)\n";
        out << "; BGM_CHN noise: val = byte - 1 (0-7)\n";
        out << ";   rate = val & 0x03 (0=H,1=M,2=L,3=Tone2)\n";
        out << ";   type = (val >> 2) & 0x01 (0=Periodic,1=White)\n";
        for (const QString& w : warnings) {
            const QByteArray wb = w.toUtf8();
            out << "; WARN export: " << wb.constData() << "\n";
        }
        out << "\n";

        auto write_db_array = [&](const char* label, const std::vector<uint8_t>& values) {
            out << label << ":\n";
            for (size_t i = 0; i < values.size(); ++i) {
                if ((i % 12) == 0) out << "        .db     ";
                char hex[8];
                std::snprintf(hex, sizeof(hex), "0x%02X", values[i]);
                out << hex;
                if ((i % 12) == 11 || i + 1 == values.size()) {
                    out << "\n";
                } else {
                    out << ", ";
                }
            }
            out << "\n";
        };

        auto write_dw = [&](const char* label, uint16_t value) {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "0x%04X", value);
            out << label << ":\n        .dw     " << hex << "\n\n";
        };

        std::vector<uint8_t> note_table_bytes;
        note_table_bytes.reserve(es.note_table.size() * 2);
        for (uint16_t d : es.note_table) {
            note_table_bytes.push_back(static_cast<uint8_t>(d & 0x0F));
            note_table_bytes.push_back(static_cast<uint8_t>((d >> 4) & 0x3F));
        }
        write_db_array("NOTE_TABLE", note_table_bytes);
        write_dw("BGM_CH0_LOOP", es.loop_offsets[0]);
        write_dw("BGM_CH1_LOOP", es.loop_offsets[1]);
        write_dw("BGM_CH2_LOOP", es.loop_offsets[2]);
        write_dw("BGM_CHN_LOOP", es.loop_offsets[3]);
        write_db_array("BGM_CH0", es.streams[0]);
        write_db_array("BGM_CH1", es.streams[1]);
        write_db_array("BGM_CH2", es.streams[2]);
        write_db_array("BGM_CHN", es.streams[3]);
        out << "BGM_MONO:\n        .db     0x00\n";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QString("Could not write %1").arg(path);
        return false;
    }
    file.write(QString::fromStdString(out.str()).toUtf8());
    file.close();

    QString inst_path;
    if (include_instrument_export && store_) {
        const QFileInfo fi(path);
        inst_path = fi.path() + "/" + fi.completeBaseName() + "_instruments.c";
        QFile ifile(inst_path);
        if (ifile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString inst_code;
            inst_code += "/* Generated by NGPC Sound Creator - Instrument Presets */\n";
            inst_code += "/* Keep this table in sync with driver_custom_latest/sounds.c */\n\n";
            inst_code += store_->export_c_array();
            ifile.write(inst_code.toUtf8());
            ifile.close();
        } else {
            append_log("WARNING: could not write instrument export to " + inst_path);
            inst_path.clear();
        }
    }
    if (instrument_export_path) {
        *instrument_export_path = inst_path;
    }

    int total_bytes = 0;
    for (int ch = 0; ch < 4; ++ch) {
        total_bytes += static_cast<int>(es.streams[static_cast<size_t>(ch)].size());
    }
    append_log(QString("%1 %2 export: %3 notes, %4 stream bytes, saved to %5.")
                   .arg(mode_label)
                   .arg(asm_export ? "ASM" : "C")
                   .arg(es.note_table.size())
                   .arg(total_bytes)
                   .arg(QFileInfo(path).fileName()));
    for (const QString& w : warnings) {
        append_log(QString("WARN export: %1").arg(w));
    }
    if (!inst_path.isEmpty()) {
        append_log(QString("Instrument presets exported to %1.")
                       .arg(QFileInfo(inst_path).fileName()));
    }

    return true;
}

void TrackerTab::on_export() {
    const QString path =
        QFileDialog::getSaveFileName(this, "Export C", QString(), "C Source (*.c *.h);;All Files (*)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!export_song_to_path(path, false, true, &error, nullptr)) {
        append_log(QString("ERROR export C: %1").arg(error));
    }
}

void TrackerTab::on_export_asm() {
    const QString path =
        QFileDialog::getSaveFileName(this, "Export ASM", QString(), "ASM Include (*.inc);;All Files (*)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!export_song_to_path(path, true, true, &error, nullptr)) {
        append_log(QString("ERROR export ASM: %1").arg(error));
    }
}

// ============================================================
// Pattern / Order management
// ============================================================

void TrackerTab::switch_to_pattern(int index) {
    if (index < 0 || index >= song_->pattern_count()) return;
    song_->set_active_pattern(index);
    doc_ = song_->active_pattern();
    grid_->set_document(doc_);
    engine_->set_document(doc_);
    length_spin_->blockSignals(true);
    length_spin_->setValue(doc_->length());
    length_spin_->blockSignals(false);
    pattern_spin_->blockSignals(true);
    pattern_spin_->setValue(index);
    pattern_spin_->blockSignals(false);
    grid_->update();
    update_status_label();
}

void TrackerTab::refresh_pattern_ui() {
    int count = song_->pattern_count();
    pattern_spin_->blockSignals(true);
    pattern_spin_->setRange(0, std::max(0, count - 1));
    pattern_spin_->setValue(song_->active_pattern_index());
    pattern_spin_->blockSignals(false);
    pattern_count_label_->setText(QString("/%1").arg(count));
    pat_del_btn_->setEnabled(count > 1);
}

void TrackerTab::refresh_order_list() {
    order_list_->blockSignals(true);
    int prev_row = order_list_->currentRow();
    order_list_->clear();
    const auto& ord = song_->order();
    int loop_pt = song_->loop_point();
    for (int i = 0; i < static_cast<int>(ord.size()); ++i) {
        QString text = QString("%1").arg(ord[static_cast<size_t>(i)], 2, 10, QChar('0'));
        if (i == loop_pt) text = "L>" + text;
        order_list_->addItem(text);
    }
    if (prev_row >= 0 && prev_row < order_list_->count())
        order_list_->setCurrentRow(prev_row);
    else if (order_list_->count() > 0)
        order_list_->setCurrentRow(0);
    order_list_->blockSignals(false);
}

void TrackerTab::on_pattern_finished() {
    if (!song_mode_) {
        // Single-pattern mode: engine already wrapped to row 0
        return;
    }

    // Song mode: advance to next order entry
    song_order_pos_++;
    if (song_order_pos_ >= song_->order_length()) {
        // Reached end of order list: loop back to loop_point
        song_order_pos_ = song_->loop_point();
    }

    const auto& ord = song_->order();
    int pat_idx = ord[static_cast<size_t>(song_order_pos_)];

    // Switch the engine to the new pattern
    TrackerDocument* new_doc = song_->pattern(pat_idx);
    if (!new_doc) {
        stop_playback();
        return;
    }

    engine_->set_document(new_doc);
    engine_->start(0);  // start from row 0 of new pattern

    // Update grid to show current playback pattern
    if (follow_mode_) {
        switch_to_pattern(pat_idx);
    }

    // Highlight current order position
    order_list_->setCurrentRow(song_order_pos_);
    append_log(QString("Song: order %1 -> Pat %2").arg(song_order_pos_).arg(pat_idx));
}

void TrackerTab::start_song_playback() {
    stop_playback();

    if (hub_) hub_->set_step_z80(false);

    if (!hub_ || !hub_->ensure_audio_running(44100)) {
        append_log("ERROR: Audio engine not ready.");
        return;
    }

    if (song_->order_length() == 0) {
        append_log("ERROR: Order list is empty.");
        return;
    }

    song_mode_ = true;
    song_order_pos_ = 0;
    playing_ = true;

    // Start with the first pattern in the order
    const auto& ord = song_->order();
    int pat_idx = ord[0];
    TrackerDocument* start_doc = song_->pattern(pat_idx);
    if (!start_doc) {
        stop_playback();
        return;
    }

    engine_->set_document(start_doc);
    engine_->set_ticks_per_row(tpr_spin_->value());
    engine_->start(0);

    if (follow_mode_) {
        switch_to_pattern(pat_idx);
    }
    grid_->set_playback_row(0);
    order_list_->setCurrentRow(0);

    play_btn_->setText("Pause [Space]");
    play_timer_->start();
    append_log(QString("Song playback started (%1 entries in order)").arg(song_->order_length()));
}

// ============================================================
// Helpers
// ============================================================

void TrackerTab::append_runtime_debug_row(int row) {
    if (!engine_ || !doc_) return;
    if (row < 0 || row >= doc_->length()) return;

    QStringList ch_dump;
    ch_dump.reserve(4);

    for (int ch = 0; ch < 4; ++ch) {
        const TrackerCell& cell = doc_->cell(ch, row);
        const auto out = engine_->channel_output(ch);
        const auto& fs = engine_->fx_state(ch);

        const QString inst_txt = QString("%1").arg(cell.instrument, 2, 16, QChar('0')).toUpper();
        const QString cell_attn_txt = (cell.attn == 0xFF)
                                          ? "A-"
                                          : QString("A%1").arg(cell.attn, 1, 16).toUpper();
        const QString cell_fx_txt = cell.has_fx()
                                        ? QString("%1%2")
                                              .arg(cell.fx & 0x0F, 1, 16, QChar('0'))
                                              .arg(cell.fx_param, 2, 16, QChar('0'))
                                              .toUpper()
                                        : "--";

        QString runtime_out_txt = "OFF";
        if (out.active) {
            if (ch < 3) {
                runtime_out_txt = QString("D%1").arg(out.divider);
            } else {
                runtime_out_txt = QString("N%1")
                                      .arg(TrackerPlaybackEngine::noise_display_name(out.noise_val));
            }
        }

        const QString runtime_fx_txt = (fs.fx == 0 && fs.param == 0)
                                           ? "--"
                                           : QString("%1%2")
                                                 .arg(fs.fx & 0x0F, 1, 16, QChar('0'))
                                                 .arg(fs.param, 2, 16, QChar('0'))
                                                 .toUpper();

        ch_dump.push_back(
            QString("C%1 %2 I%3 %4 FX%5 OUT:%6 A%7 RTX:%8 E%9 PB%10")
                .arg(ch)
                .arg(tracker_note_to_text(cell.note))
                .arg(inst_txt)
                .arg(cell_attn_txt)
                .arg(cell_fx_txt)
                .arg(runtime_out_txt)
                .arg(out.attn, 1, 16, QChar('0'))
                .arg(runtime_fx_txt)
                .arg(fs.expression, 1, 16, QChar('0'))
                .arg(fs.pitch_bend));
    }

    const QString msg = QString("DBG row %1 | %2")
                            .arg(row, 2, 16, QChar('0'))
                            .toUpper()
                            .arg(ch_dump.join(" | "));
    append_log(msg);
}

void TrackerTab::append_log(const QString& text) {
    log_->appendPlainText(text);
}

// ============================================================
// MIDI import
// ============================================================

void TrackerTab::on_import_midi() {
    QString path = QFileDialog::getOpenFileName(this, "Import MIDI",
                                                 QString(), "MIDI files (*.mid *.midi);;All (*)");
    if (path.isEmpty()) return;

    QString error;
    if (import_midi_from_path(path, &error)) {
        append_log(QString("MIDI imported from %1").arg(path));
    } else {
        append_log(QString("ERROR MIDI import: %1").arg(error));
    }
}

bool TrackerTab::import_midi_from_path(const QString& path, QString* error) {
    if (path.isEmpty()) {
        if (error) *error = "Empty MIDI path";
        return false;
    }

    stop_playback();

    MidiImportSettings settings;
    settings.rows_per_beat = 4;
    settings.pattern_length = length_spin_->value();
    settings.import_velocity = true;

    append_log(QString("Importing MIDI: %1 ...").arg(path));

    MidiImportResult res = ImportMidi(path, song_, settings);

    if (!res.success) {
        if (error) *error = res.error;
        return false;
    }

    // Refresh UI with imported data
    doc_ = song_->active_pattern();
    grid_->set_document(doc_);
    engine_->set_document(doc_);
    length_spin_->blockSignals(true);
    length_spin_->setValue(doc_->length());
    length_spin_->blockSignals(false);
    refresh_pattern_ui();
    refresh_order_list();
    grid_->set_cursor(0, 0, TrackerGridWidget::SubNote);
    grid_->update();

    // Apply suggested TPR
    if (res.suggested_tpr != tpr_spin_->value()) {
        tpr_spin_->setValue(res.suggested_tpr);
        append_log(QString("TPR set to %1 (to match MIDI tempo)").arg(res.suggested_tpr));
    }

    append_log(QString("MIDI imported: %1 patterns, %2 notes (%3 dropped due to polyphony)")
        .arg(res.patterns_created).arg(res.notes_imported).arg(res.notes_dropped));
    return true;
}
