#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

class SongDocument;
class InstrumentStore;

// ============================================================
// WavExporter — Offline render of song/pattern to WAV file
// ============================================================

class WavExporter
{
public:
    struct Settings {
        int sample_rate = 44100;
        int ticks_per_row = 8;
        bool song_mode = true;   // true: play entire order list, false: active pattern only
        int max_loops = 1;       // how many times to play through order before stopping
    };

    // Render to WAV file. Returns true on success.
    static bool render_to_file(const QString& path,
                               SongDocument* song,
                               InstrumentStore* store,
                               const Settings& settings,
                               QString* error = nullptr);

    // Render to raw PCM (mono int16). Returns sample count.
    static std::vector<int16_t> render_to_pcm(SongDocument* song,
                                               InstrumentStore* store,
                                               const Settings& settings);

private:
    static QByteArray build_wav_header(int sample_rate, int num_samples);
};
