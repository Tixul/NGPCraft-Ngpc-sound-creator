#include "audio/MidiImporter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <QFile>

#include "audio/TrackerPlaybackEngine.h"
#include "models/SongDocument.h"
#include "models/TrackerDocument.h"

// ============================================================
// MIDI binary helpers (local)
// ============================================================

namespace {

bool ReadU16BE(const uint8_t* d, size_t sz, size_t pos, uint16_t* out) {
    if (pos + 2 > sz) return false;
    *out = static_cast<uint16_t>((d[pos] << 8) | d[pos + 1]);
    return true;
}

bool ReadU32BE(const uint8_t* d, size_t sz, size_t pos, uint32_t* out) {
    if (pos + 4 > sz) return false;
    *out = (uint32_t(d[pos]) << 24) | (uint32_t(d[pos+1]) << 16) |
           (uint32_t(d[pos+2]) << 8) | uint32_t(d[pos+3]);
    return true;
}

bool ReadVLQ(const uint8_t* d, size_t sz, size_t* pos, uint32_t* out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        if (*pos >= sz) return false;
        uint8_t b = d[*pos]; (*pos)++;
        v = (v << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) { *out = v; return true; }
    }
    return false;
}

// ============================================================
// MIDI event types for intermediate representation
// ============================================================

struct MidiNoteEvent {
    int32_t tick;        // absolute tick position
    uint8_t channel;     // MIDI channel (0-15)
    uint8_t note;        // MIDI note number
    uint8_t velocity;    // 0 = note off
};

struct MidiTempoEvent {
    int32_t tick;
    uint32_t us_per_beat; // microseconds per beat
};

// velocity (1-127) to PSG attenuation (0-15), 0=loud 15=silent
uint8_t velocity_to_attn(uint8_t vel) {
    if (vel == 0) return 15;
    // Linear map: vel 127 -> attn 0, vel 1 -> attn 14
    int a = 14 - (vel - 1) * 14 / 126;
    return static_cast<uint8_t>(std::clamp(a, 0, 14));
}

// Convert standard MIDI note (C0=12, C4=60) to tracker 1-based note (C0=1, C4=49).
// midi_to_divider() does tracker_note + 11 to get back to MIDI numbering.
uint8_t midi_to_tracker_note(uint8_t midi_note) {
    int t = static_cast<int>(midi_note) - 11;
    return static_cast<uint8_t>(std::clamp(t, 1, 127));
}

// Map GM drum MIDI note to tracker noise config (1-8).
// noise_config values: 1=P.H 2=P.M 3=P.L 4=P.T 5=W.H 6=W.M 7=W.L 8=W.T
uint8_t gm_drum_to_noise(uint8_t midi_note) {
    switch (midi_note) {
    case 35: case 36:               return 1;  // Kick        -> P.H (periodic high)
    case 41: case 43: case 45:      return 2;  // Low toms    -> P.M (periodic medium)
    case 47: case 48: case 50:      return 3;  // High toms   -> P.L (periodic low)
    case 38: case 39: case 40:      return 6;  // Snare/clap  -> W.M (white medium)
    case 42: case 44:               return 5;  // Closed HH   -> W.H (white high)
    case 46:                        return 5;  // Open HH     -> W.H
    case 49: case 52: case 55: case 57: return 7; // Crash/China -> W.L (white low)
    case 51: case 53: case 59:      return 5;  // Ride/Bell   -> W.H
    case 37: case 56: case 54:      return 4;  // Rimshot/cowbell -> P.T
    default:                        return 5;  // Fallback    -> W.H
    }
}

} // namespace

// ============================================================
// ImportMidi
// ============================================================

MidiImportResult ImportMidi(const QString& path, SongDocument* song,
                            const MidiImportSettings& settings)
{
    MidiImportResult result;

    // --- Read file ---
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = "Cannot open file";
        return result;
    }
    QByteArray raw = file.readAll();
    file.close();

    const auto* d = reinterpret_cast<const uint8_t*>(raw.constData());
    const size_t sz = static_cast<size_t>(raw.size());

    // --- Parse MThd ---
    if (sz < 14 || std::memcmp(d, "MThd", 4) != 0) {
        result.error = "Not a valid MIDI file (missing MThd)";
        return result;
    }

    uint32_t header_len = 0;
    if (!ReadU32BE(d, sz, 4, &header_len) || header_len < 6) {
        result.error = "Invalid MIDI header";
        return result;
    }

    uint16_t format = 0, num_tracks = 0, division = 0;
    ReadU16BE(d, sz, 8, &format);
    ReadU16BE(d, sz, 10, &num_tracks);
    ReadU16BE(d, sz, 12, &division);

    if (format > 1) {
        result.error = "Only MIDI type 0 and 1 supported";
        return result;
    }
    if (division & 0x8000) {
        result.error = "SMPTE time division not supported";
        return result;
    }
    if (division == 0) {
        result.error = "Invalid MIDI division (0)";
        return result;
    }

    int ticks_per_beat = division;

    // --- Parse all tracks: extract note events + tempo ---
    std::vector<MidiNoteEvent> notes;
    std::vector<MidiTempoEvent> tempos;

    size_t pos = 8 + header_len;

    for (uint16_t trk = 0; trk < num_tracks; ++trk) {
        if (pos + 8 > sz || std::memcmp(d + pos, "MTrk", 4) != 0) {
            result.error = QString("Missing MTrk for track %1").arg(trk);
            return result;
        }

        uint32_t track_len = 0;
        ReadU32BE(d, sz, pos + 4, &track_len);
        pos += 8;
        size_t track_end = pos + track_len;
        if (track_end > sz) track_end = sz;

        int32_t abs_tick = 0;
        uint8_t running = 0;

        while (pos < track_end) {
            uint32_t delta = 0;
            if (!ReadVLQ(d, sz, &pos, &delta)) break;
            abs_tick += static_cast<int32_t>(delta);

            if (pos >= track_end) break;

            uint8_t status = d[pos];
            if (status < 0x80) {
                if (running == 0) break;
                status = running;
            } else {
                pos++;
                if (status < 0xF0) running = status;
            }

            // Meta event
            if (status == 0xFF) {
                if (pos >= track_end) break;
                uint8_t meta = d[pos++];
                uint32_t len = 0;
                if (!ReadVLQ(d, sz, &pos, &len)) break;
                if (pos + len > track_end) break;

                // Set Tempo
                if (meta == 0x51 && len == 3) {
                    uint32_t us = (uint32_t(d[pos]) << 16) |
                                  (uint32_t(d[pos+1]) << 8) |
                                  uint32_t(d[pos+2]);
                    tempos.push_back({abs_tick, us});
                }
                pos += len;
                continue;
            }

            // SysEx
            if (status == 0xF0 || status == 0xF7) {
                uint32_t len = 0;
                if (!ReadVLQ(d, sz, &pos, &len)) break;
                if (pos + len > track_end) break;
                pos += len;
                continue;
            }

            uint8_t hi = status & 0xF0;
            uint8_t ch = status & 0x0F;
            int data_bytes = 0;
            switch (hi) {
            case 0x80: case 0x90: case 0xA0:
            case 0xB0: case 0xE0: data_bytes = 2; break;
            case 0xC0: case 0xD0: data_bytes = 1; break;
            default: data_bytes = 0; break;
            }

            if (pos + data_bytes > track_end) break;

            if (hi == 0x90 && data_bytes == 2) {
                uint8_t note = d[pos];
                uint8_t vel = d[pos + 1];
                notes.push_back({abs_tick, ch, note, vel});
            } else if (hi == 0x80 && data_bytes == 2) {
                uint8_t note = d[pos];
                notes.push_back({abs_tick, ch, note, 0}); // note off
            }

            pos += data_bytes;
        }

        pos = track_end; // skip to next track
    }

    if (notes.empty()) {
        result.error = "No note events found in MIDI file";
        return result;
    }

    // Sort by tick, then by velocity descending (note-offs before note-ons at same tick)
    std::sort(notes.begin(), notes.end(), [](const MidiNoteEvent& a, const MidiNoteEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        // Note-offs (vel=0) should come before note-ons at same tick
        if (a.velocity == 0 && b.velocity != 0) return true;
        if (a.velocity != 0 && b.velocity == 0) return false;
        return false;
    });

    // --- Compute tempo for TPR suggestion ---
    uint32_t first_tempo = 500000; // default 120 BPM
    if (!tempos.empty()) {
        first_tempo = tempos[0].us_per_beat;
    }
    // BPM = 60,000,000 / us_per_beat
    double bpm = 60000000.0 / first_tempo;
    // We want: bpm = 60.0 / tpr * rows_per_beat / 4.0 * 60.0
    //   => bpm = 60.0 * 60.0 * rows_per_beat / (4 * tpr)
    //   => tpr = 900 * rows_per_beat / (bpm * 4)
    // Actually: at 60fps, rows_per_second = 60/tpr
    //   beats_per_second = rows_per_second / rows_per_beat
    //   bpm = beats_per_second * 60
    //   => bpm = (60 / tpr / rows_per_beat) * 60
    //   => tpr = 3600 / (bpm * rows_per_beat)
    double ideal_tpr = 3600.0 / (bpm * settings.rows_per_beat);
    result.suggested_tpr = std::clamp(static_cast<int>(std::round(ideal_tpr)), 1, 32);

    // --- Convert ticks to rows ---
    // row = tick * rows_per_beat / ticks_per_beat
    auto tick_to_row = [&](int32_t tick) -> int {
        int64_t r = static_cast<int64_t>(tick) * settings.rows_per_beat / ticks_per_beat;
        return static_cast<int>(r);
    };

    // Find total rows needed
    int32_t max_tick = notes.back().tick;
    int total_rows = tick_to_row(max_tick) + settings.rows_per_beat; // add one beat of margin

    // --- Voice allocation ---
    // 3 tone channels (tracker ch 0-2), 1 noise channel (tracker ch 3)
    // MIDI channel 9 (0-indexed) = drums -> noise

    struct VoiceSlot {
        bool active = false;
        uint8_t note = 0;
        uint8_t midi_ch = 0;
        int32_t start_tick = 0;
    };

    VoiceSlot tone_slots[3] = {};
    VoiceSlot noise_slot = {};

    // Output: flat array of cells indexed by [row * 4 + tracker_ch]
    int num_patterns = (total_rows + settings.pattern_length - 1) / settings.pattern_length;
    num_patterns = std::clamp(num_patterns, 1, 64);
    int actual_total_rows = num_patterns * settings.pattern_length;

    struct CellEntry {
        bool has_note = false;
        bool is_note_off = false;
        uint8_t note = 0;
        uint8_t attn = 0xFF;
        uint8_t instrument = 0;
    };

    std::vector<CellEntry> cells(static_cast<size_t>(actual_total_rows * 4));

    auto set_cell = [&](int row, int ch, const CellEntry& e) {
        if (row < 0 || row >= actual_total_rows || ch < 0 || ch >= 4) return;
        cells[static_cast<size_t>(row * 4 + ch)] = e;
    };

    auto get_cell = [&](int row, int ch) -> CellEntry& {
        return cells[static_cast<size_t>(row * 4 + ch)];
    };

    int notes_imported = 0;
    int notes_dropped = 0;

    for (const auto& ev : notes) {
        int row = tick_to_row(ev.tick);
        if (row >= actual_total_rows) continue;

        bool is_drum = (ev.channel == 9);

        if (ev.velocity == 0) {
            // Note off
            if (is_drum) {
                if (noise_slot.active && noise_slot.note == ev.note) {
                    // Write note-off at this row if cell is empty
                    auto& c = get_cell(row, 3);
                    if (!c.has_note) {
                        c.has_note = true;
                        c.is_note_off = true;
                    }
                    noise_slot.active = false;
                }
            } else {
                for (int i = 0; i < 3; ++i) {
                    if (tone_slots[i].active && tone_slots[i].note == ev.note &&
                        tone_slots[i].midi_ch == ev.channel) {
                        auto& c = get_cell(row, i);
                        if (!c.has_note) {
                            c.has_note = true;
                            c.is_note_off = true;
                        }
                        tone_slots[i].active = false;
                        break;
                    }
                }
            }
            continue;
        }

        // Note on
        uint8_t attn = settings.import_velocity ? velocity_to_attn(ev.velocity) : 0;

        if (is_drum) {
            // Noise channel
            if (noise_slot.active) {
                // Insert note-off for previous note
                auto& off_c = get_cell(row, 3);
                if (!off_c.has_note) {
                    // Will be overwritten by note-on below
                }
            }
            CellEntry ce;
            ce.has_note = true;
            ce.note = gm_drum_to_noise(ev.note);
            ce.attn = attn;
            set_cell(row, 3, ce);
            noise_slot = {true, ev.note, ev.channel, ev.tick};
            notes_imported++;
        } else {
            // Find free tone slot
            int slot = -1;
            for (int i = 0; i < 3; ++i) {
                if (!tone_slots[i].active) { slot = i; break; }
            }

            if (slot < 0) {
                // Voice stealing: find oldest note
                int oldest = 0;
                for (int i = 1; i < 3; ++i) {
                    if (tone_slots[i].start_tick < tone_slots[oldest].start_tick)
                        oldest = i;
                }
                // Write note-off for stolen voice
                auto& off_c = get_cell(row, oldest);
                if (!off_c.has_note) {
                    off_c.has_note = true;
                    off_c.is_note_off = true;
                }
                tone_slots[oldest].active = false;
                slot = oldest;
                notes_dropped++;
            }

            // Convert MIDI note to tracker 1-based note
            uint8_t tracker_note = midi_to_tracker_note(ev.note);

            CellEntry ce;
            ce.has_note = true;
            ce.note = tracker_note;
            ce.attn = attn;
            set_cell(row, slot, ce);

            tone_slots[slot] = {true, ev.note, ev.channel, ev.tick};
            notes_imported++;
        }
    }

    // --- Write cells into SongDocument patterns ---
    // Clear existing song data
    while (song->pattern_count() > 1) {
        song->remove_pattern(song->pattern_count() - 1);
    }

    // Setup first pattern
    TrackerDocument* pat = song->pattern(0);
    pat->set_length(settings.pattern_length);
    pat->clear_all();

    // Create additional patterns
    for (int p = 1; p < num_patterns; ++p) {
        song->add_pattern();
        TrackerDocument* np = song->pattern(p);
        np->set_length(settings.pattern_length);
    }

    // Write cells
    for (int row = 0; row < actual_total_rows; ++row) {
        int pat_idx = row / settings.pattern_length;
        int pat_row = row % settings.pattern_length;
        TrackerDocument* p = song->pattern(pat_idx);
        if (!p) continue;

        for (int ch = 0; ch < 4; ++ch) {
            const auto& ce = get_cell(row, ch);
            if (!ce.has_note) continue;

            if (ce.is_note_off) {
                p->set_note(ch, pat_row, 0xFF); // note off
            } else {
                p->set_note(ch, pat_row, ce.note);
                if (ce.attn != 0xFF) {
                    p->set_attn(ch, pat_row, ce.attn);
                }
                p->set_instrument(ch, pat_row, ce.instrument);
            }
        }
    }

    // Setup order list: one entry per pattern
    // Clear old order and build new
    while (song->order_length() > 1) {
        song->order_remove(song->order_length() - 1);
    }
    song->order_set_entry(0, 0);
    for (int p = 1; p < num_patterns; ++p) {
        song->order_insert(p, p);
    }
    song->set_loop_point(0);

    result.success = true;
    result.patterns_created = num_patterns;
    result.notes_imported = notes_imported;
    result.notes_dropped = notes_dropped;
    return result;
}
