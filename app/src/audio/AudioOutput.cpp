#include "AudioOutput.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <cmath>
#include <vector>

#include "ngpc/sound_engine.h"

namespace {
static constexpr int kZ80ClockHz = 3072000;
static constexpr double kIrqHz = 7800.0;
static constexpr double kCyclesPerIrq = static_cast<double>(kZ80ClockHz) / kIrqHz;
}

AudioOutput::AudioOutput(QObject* parent)
    : QObject(parent) {}

AudioOutput::~AudioOutput() {
    stop();
}

bool AudioOutput::start(ngpc::SoundEngine* engine, int sample_rate) {
    if (!engine) {
        return false;
    }

    stop();
    stopping_ = false;
    last_error_.clear();
    engine_ = engine;

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        last_error_ = "No default audio output device";
        stop();
        return false;
    }
    device_desc_ = device.description();

    QAudioFormat format;
    format.setSampleRate(sample_rate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
    }

    if (format.sampleFormat() != QAudioFormat::Int16 &&
        format.sampleFormat() != QAudioFormat::Float) {
        last_error_ = "Audio device does not support Int16 or Float format";
        stop();
        return false;
    }

    sink_ = new QAudioSink(device, format, this);
    sink_->setBufferSize(8192);
    sink_->setVolume(1.0f);
    format_sample_rate_ = format.sampleRate();
    format_channels_ = format.channelCount();
    format_is_float_ = (format.sampleFormat() == QAudioFormat::Float) ? 1 : 0;
    bytes_per_sample_ = (format.sampleFormat() == QAudioFormat::Float)
                            ? static_cast<int>(sizeof(float))
                            : static_cast<int>(sizeof(int16_t));
    cycles_per_sample_ = static_cast<double>(kZ80ClockHz) / format_sample_rate_;
    cycles_accum_ = 0.0;
    irq_cycle_pos_ = 0.0;

    device_ = sink_->start();
    if (!device_) {
        last_error_ = "Audio start returned null device";
        stop();
        return false;
    }
    if (!device_->isOpen()) {
        last_error_ = "Audio device failed to open";
        stop();
        return false;
    }
    if (timer_) {
        timer_->stop();
        timer_->deleteLater();
    }
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &AudioOutput::on_audio_tick);
    timer_->start(10);
    return true;
}

void AudioOutput::stop() {
    if (stopping_) {
        return;
    }
    stopping_ = true;
    finalize_stop();
}

bool AudioOutput::is_running() const {
    return sink_ != nullptr;
}

QString AudioOutput::last_error() const {
    return last_error_;
}

QString AudioOutput::debug_info() const {
    if (format_sample_rate_ <= 0) {
        return QString();
    }
    const QString fmt = format_is_float_ ? "float" : "int16";
    QString state = "Unknown";
    QString err = "Unknown";
    if (sink_) {
        switch (sink_->state()) {
        case QAudio::ActiveState:
            state = "Active";
            break;
        case QAudio::SuspendedState:
            state = "Suspended";
            break;
        case QAudio::StoppedState:
            state = "Stopped";
            break;
        case QAudio::IdleState:
            state = "Idle";
            break;
        }
        switch (sink_->error()) {
        case QAudio::NoError:
            err = "None";
            break;
        case QAudio::OpenError:
            err = "OpenError";
            break;
        case QAudio::IOError:
            err = "IOError";
            break;
        case QAudio::UnderrunError:
            err = "Underrun";
            break;
        case QAudio::FatalError:
            err = "FatalError";
            break;
        }
    }
    return QString("%1 Hz, %2 ch, %3 (%4), state=%5, err=%6")
        .arg(format_sample_rate_)
        .arg(format_channels_)
        .arg(fmt)
        .arg(device_desc_)
        .arg(state)
        .arg(err);
}

void AudioOutput::set_step_z80(bool enabled) {
    step_z80_ = enabled;
}

int AudioOutput::peak_percent() const {
    int p = static_cast<int>(std::lround(peak_level_ * 100.0f));
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    return p;
}

bool AudioOutput::clip_recent() const {
    return clip_hold_ticks_ > 0;
}

void AudioOutput::on_audio_tick() {
    if (stopping_ || !engine_ || !sink_ || !device_) {
        return;
    }
    if (sink_->state() == QAudio::StoppedState) {
        return;
    }
    const int bytes_free = sink_->bytesFree();
    if (bytes_free <= 0 || bytes_per_sample_ <= 0 || format_channels_ <= 0) {
        return;
    }
    const int bytes_per_frame = bytes_per_sample_ * format_channels_;
    if (bytes_free < bytes_per_frame * 128) {
        return;
    }
    int frames = bytes_free / bytes_per_frame;
    if (frames <= 0) {
        return;
    }

    if (step_z80_) {
        double total_cycles = cycles_accum_ + (cycles_per_sample_ * frames);
        const int total_cycles_i = static_cast<int>(total_cycles);
        cycles_accum_ = total_cycles - total_cycles_i;

        int remaining = total_cycles_i;
        while (remaining > 0) {
            const int to_irq = static_cast<int>(kCyclesPerIrq - irq_cycle_pos_);
            const int step = (remaining < to_irq) ? remaining : to_irq;
            if (step > 0) {
                engine_->step_cycles(step);
                remaining -= step;
                irq_cycle_pos_ += step;
            }
            if (irq_cycle_pos_ >= kCyclesPerIrq) {
                engine_->request_irq();
                irq_cycle_pos_ -= kCyclesPerIrq;
            }
        }
    }

    mono_.resize(static_cast<size_t>(frames));
    engine_->render(mono_.data(), frames);
    if (stopping_ || !device_ || !sink_) {
        return;
    }

    {
        int peak_abs = 0;
        bool clipped = false;
        for (int i = 0; i < frames; ++i) {
            const int v = static_cast<int>(mono_[static_cast<size_t>(i)]);
            const int av = std::abs(v);
            if (av > peak_abs) {
                peak_abs = av;
            }
            if (av >= 32767) {
                clipped = true;
            }
        }
        const float instant = static_cast<float>(peak_abs) / 32767.0f;
        if (instant > peak_level_) {
            peak_level_ = instant;
        } else {
            peak_level_ *= 0.92f;
            if (instant > peak_level_) {
                peak_level_ = instant;
            }
        }
        if (clipped) {
            clip_hold_ticks_ = 20;  // ~400 ms at 20 ms tick
        } else if (clip_hold_ticks_ > 0) {
            --clip_hold_ticks_;
        }
    }

    if (format_is_float_) {
        const int total_samples = frames * format_channels_;
        std::vector<float> out(static_cast<size_t>(total_samples));
        for (int i = 0; i < frames; ++i) {
            const float sample = static_cast<float>(mono_[static_cast<size_t>(i)]) / 32768.0f;
            for (int ch = 0; ch < format_channels_; ++ch) {
                out[static_cast<size_t>(i * format_channels_ + ch)] = sample;
            }
        }
        device_->write(reinterpret_cast<const char*>(out.data()),
                       total_samples * static_cast<int>(sizeof(float)));
    } else {
        const int total_samples = frames * format_channels_;
        std::vector<int16_t> out(static_cast<size_t>(total_samples));
        for (int i = 0; i < frames; ++i) {
            const int16_t sample = mono_[static_cast<size_t>(i)];
            for (int ch = 0; ch < format_channels_; ++ch) {
                out[static_cast<size_t>(i * format_channels_ + ch)] = sample;
            }
        }
        device_->write(reinterpret_cast<const char*>(out.data()),
                       total_samples * static_cast<int>(sizeof(int16_t)));
    }
}

void AudioOutput::finalize_stop() {
    if (timer_) {
        timer_->stop();
        timer_->deleteLater();
        timer_ = nullptr;
    }
    if (sink_) {
        sink_->stop();
        sink_->deleteLater();
        sink_ = nullptr;
    }
    // device_ is owned by QAudioSink – do not close or delete it separately.
    device_ = nullptr;
    engine_ = nullptr;
    bytes_per_sample_ = 0;
    format_sample_rate_ = 0;
    format_channels_ = 0;
    format_is_float_ = 0;
    cycles_per_sample_ = 0.0;
    cycles_accum_ = 0.0;
    irq_cycle_pos_ = 0.0;
    peak_level_ = 0.0f;
    clip_hold_ticks_ = 0;
    stopping_ = false;
    cleanup_pending_ = false;
}
