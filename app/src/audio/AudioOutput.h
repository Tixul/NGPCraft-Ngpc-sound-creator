#pragma once

#include <QObject>
#include <vector>

class QAudioSink;
class QIODevice;
class QTimer;

namespace ngpc {
class SoundEngine;
}

class AudioOutput : public QObject
{
    Q_OBJECT

public:
    explicit AudioOutput(QObject* parent = nullptr);
    ~AudioOutput();

    bool start(ngpc::SoundEngine* engine, int sample_rate = 44100);
    void stop();
    bool is_running() const;
    QString last_error() const;
    QString debug_info() const;
    void set_step_z80(bool enabled);
    int peak_percent() const;
    bool clip_recent() const;

private:
    void on_audio_tick();
    void finalize_stop();

    QAudioSink* sink_ = nullptr;
    QIODevice* device_ = nullptr;
    QTimer* timer_ = nullptr;
    ngpc::SoundEngine* engine_ = nullptr;
    QString last_error_;
    QString device_desc_;
    int format_sample_rate_ = 0;
    int format_channels_ = 0;
    int format_is_float_ = 0;
    int bytes_per_sample_ = 0;
    double cycles_per_sample_ = 0.0;
    double cycles_accum_ = 0.0;
    double irq_cycle_pos_ = 0.0;
    std::vector<int16_t> mono_;
    bool step_z80_ = true;
    bool stopping_ = false;
    bool cleanup_pending_ = false;
    float peak_level_ = 0.0f;
    int clip_hold_ticks_ = 0;
};
