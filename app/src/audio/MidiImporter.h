#pragma once

#include <QString>
#include <cstdint>

class SongDocument;

struct MidiImportSettings {
    int rows_per_beat = 4;       // quantization (4 = 16th note grid)
    int pattern_length = 64;     // rows per pattern
    bool import_velocity = true; // map velocity to attenuation
};

struct MidiImportResult {
    bool success = false;
    QString error;
    int patterns_created = 0;
    int notes_imported = 0;
    int notes_dropped = 0;  // notes dropped due to polyphony limits
    int suggested_tpr = 8;  // suggested ticks-per-row for correct tempo
};

MidiImportResult ImportMidi(const QString& path, SongDocument* song,
                            const MidiImportSettings& settings);
