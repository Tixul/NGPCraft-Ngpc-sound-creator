#include "audio/WavExporter.h"

#include <QFile>

#include <cstring>

#include "audio/PsgHelpers.h"
#include "audio/TrackerPlaybackEngine.h"
#include "models/InstrumentStore.h"
#include "models/SongDocument.h"
#include "models/TrackerDocument.h"
#include "ngpc/sound_engine.h"

// ============================================================
// Offline render
// ============================================================

std::vector<int16_t> WavExporter::render_to_pcm(SongDocument* song,
                                                  InstrumentStore* store,
                                                  const Settings& settings)
{
    if (!song || song->pattern_count() == 0) return {};

    // Create dedicated engine and playback
    ngpc::SoundEngine snd;
    snd.init(settings.sample_rate);

    TrackerPlaybackEngine playback;
    playback.set_instrument_store(store);
    playback.set_ticks_per_row(settings.ticks_per_row);

    // Samples per tick at 60fps
    const int samples_per_tick = settings.sample_rate / 60;

    std::vector<int16_t> pcm;
    // Reserve a rough estimate (avoid too many reallocs)
    pcm.reserve(static_cast<size_t>(settings.sample_rate * 60)); // ~60 sec

    std::vector<int16_t> tick_buf(static_cast<size_t>(samples_per_tick));

    auto write_outputs_to_psg = [&](TrackerPlaybackEngine& eng, ngpc::SoundEngine& snd_engine) {
        for (int ch = 0; ch < 4; ++ch) {
            auto out = eng.channel_output(ch);
            if (!out.active) {
                if (ch < 3) {
                    psg_helpers::DirectSilenceTone(snd_engine, ch);
                } else {
                    psg_helpers::DirectSilenceNoise(snd_engine);
                }
            } else {
                if (ch < 3) {
                    psg_helpers::DirectToneCh(snd_engine, ch, out.divider, out.attn);
                } else {
                    auto nc = TrackerPlaybackEngine::decode_noise_val(out.noise_val);
                    psg_helpers::DirectNoise(snd_engine, nc.rate, nc.type, out.attn);
                }
            }
        }
    };

    if (settings.song_mode && song->order_length() > 0) {
        // Song mode: iterate through order list
        const auto& order = song->order();
        int loop_count = 0;
        int order_pos = 0;

        while (loop_count < settings.max_loops) {
            int pat_idx = order[static_cast<size_t>(order_pos)];
            TrackerDocument* pat = song->pattern(pat_idx);
            if (!pat) break;

            playback.set_document(pat);
            playback.start(0);

            // Play through entire pattern.
            // Use had_ticks flag like build_export_streams to handle
            // edge case of 1-row patterns at tpr=1 correctly.
            bool pattern_done = false;
            bool had_ticks = false;
            while (!pattern_done) {
                playback.tick();
                had_ticks = true;
                write_outputs_to_psg(playback, snd);
                snd.render(tick_buf.data(), samples_per_tick);
                pcm.insert(pcm.end(), tick_buf.begin(), tick_buf.end());

                if (playback.current_row() == 0 && playback.tick_counter() == 0
                    && had_ticks) {
                    pattern_done = true;
                }
            }

            playback.stop();

            // Advance to next order entry
            order_pos++;
            if (order_pos >= static_cast<int>(order.size())) {
                loop_count++;
                if (loop_count < settings.max_loops) {
                    order_pos = song->loop_point();
                }
            }
        }
    } else {
        // Single pattern mode: play active pattern once
        TrackerDocument* pat = song->active_pattern();
        if (!pat) return {};

        playback.set_document(pat);
        playback.start(0);

        int total_ticks = pat->length() * settings.ticks_per_row;
        for (int t = 0; t < total_ticks; ++t) {
            playback.tick();
            write_outputs_to_psg(playback, snd);
            snd.render(tick_buf.data(), samples_per_tick);
            pcm.insert(pcm.end(), tick_buf.begin(), tick_buf.end());
        }

        playback.stop();
    }

    // Silence tail (short fade to avoid click)
    // Silence the PSG and render a short tail
    for (int ch = 0; ch < 3; ++ch) {
        psg_helpers::DirectSilenceTone(snd, ch);
    }
    psg_helpers::DirectSilenceNoise(snd);

    int tail_samples = settings.sample_rate / 10; // 100ms silence
    std::vector<int16_t> tail(static_cast<size_t>(tail_samples), 0);
    snd.render(tail.data(), tail_samples);
    pcm.insert(pcm.end(), tail.begin(), tail.end());

    return pcm;
}

// ============================================================
// WAV file writing
// ============================================================

QByteArray WavExporter::build_wav_header(int sample_rate, int num_samples) {
    // WAV header: 44 bytes, PCM mono 16-bit
    QByteArray header(44, '\0');
    auto* h = reinterpret_cast<uint8_t*>(header.data());

    int data_size = num_samples * 2;  // 16-bit = 2 bytes per sample
    int file_size = 36 + data_size;

    // RIFF chunk
    std::memcpy(h + 0, "RIFF", 4);
    h[4] = static_cast<uint8_t>(file_size & 0xFF);
    h[5] = static_cast<uint8_t>((file_size >> 8) & 0xFF);
    h[6] = static_cast<uint8_t>((file_size >> 16) & 0xFF);
    h[7] = static_cast<uint8_t>((file_size >> 24) & 0xFF);
    std::memcpy(h + 8, "WAVE", 4);

    // fmt sub-chunk
    std::memcpy(h + 12, "fmt ", 4);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0; // chunk size = 16
    h[20] = 1; h[21] = 0; // PCM format
    h[22] = 1; h[23] = 0; // mono

    // sample rate
    h[24] = static_cast<uint8_t>(sample_rate & 0xFF);
    h[25] = static_cast<uint8_t>((sample_rate >> 8) & 0xFF);
    h[26] = static_cast<uint8_t>((sample_rate >> 16) & 0xFF);
    h[27] = static_cast<uint8_t>((sample_rate >> 24) & 0xFF);

    // byte rate = sample_rate * 2
    int byte_rate = sample_rate * 2;
    h[28] = static_cast<uint8_t>(byte_rate & 0xFF);
    h[29] = static_cast<uint8_t>((byte_rate >> 8) & 0xFF);
    h[30] = static_cast<uint8_t>((byte_rate >> 16) & 0xFF);
    h[31] = static_cast<uint8_t>((byte_rate >> 24) & 0xFF);

    h[32] = 2; h[33] = 0; // block align = 2
    h[34] = 16; h[35] = 0; // bits per sample = 16

    // data sub-chunk
    std::memcpy(h + 36, "data", 4);
    h[40] = static_cast<uint8_t>(data_size & 0xFF);
    h[41] = static_cast<uint8_t>((data_size >> 8) & 0xFF);
    h[42] = static_cast<uint8_t>((data_size >> 16) & 0xFF);
    h[43] = static_cast<uint8_t>((data_size >> 24) & 0xFF);

    return header;
}

bool WavExporter::render_to_file(const QString& path,
                                  SongDocument* song,
                                  InstrumentStore* store,
                                  const Settings& settings,
                                  QString* error)
{
    auto pcm = render_to_pcm(song, store, settings);
    if (pcm.empty()) {
        if (error) *error = "No audio data generated.";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QString("Could not open file: %1").arg(path);
        return false;
    }

    QByteArray header = build_wav_header(settings.sample_rate, static_cast<int>(pcm.size()));
    f.write(header);
    f.write(reinterpret_cast<const char*>(pcm.data()),
            static_cast<qint64>(pcm.size() * sizeof(int16_t)));
    f.close();

    return true;
}
