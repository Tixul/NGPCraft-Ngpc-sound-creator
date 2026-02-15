#include "audio/EngineHub.h"

#include "audio/AudioOutput.h"

EngineHub::EngineHub(QObject* parent)
    : QObject(parent) {
    audio_ = new AudioOutput(this);
    polling_.set_z80(&engine_.z80());
}

EngineHub::~EngineHub() {
    stop_audio();
}

bool EngineHub::ensure_engine(int sample_rate) {
    if (engine_ready_) {
        return true;
    }
    if (!engine_.init(sample_rate)) {
        return false;
    }
    engine_ready_ = true;
    polling_.set_z80(&engine_.z80());
    return true;
}

bool EngineHub::start_audio(int sample_rate) {
    if (!ensure_engine(sample_rate)) {
        return false;
    }
    if (audio_->is_running()) {
        return true;
    }
    return audio_->start(&engine_, sample_rate);
}

bool EngineHub::ensure_audio_running(int sample_rate) {
    if (audio_->is_running()) {
        return true;
    }
    return start_audio(sample_rate);
}

void EngineHub::stop_audio() {
    if (audio_) {
        audio_->stop();
    }
}

bool EngineHub::load_driver(const QString& path, QString* error) {
    stop_audio();

    if (!ensure_engine(44100)) {
        if (error) {
            *error = "Engine init failed";
        }
        return false;
    }
    engine_.reset();
    polling_.set_z80(&engine_.z80());

    const std::string local_path = path.toStdString();
    if (local_path.empty()) {
        if (error) {
            *error = "Driver path empty";
        }
        return false;
    }

    driver_loaded_ = false;
    driver_is_polling_ = false;
    std::string load_error;
    if (!engine_.load_z80_driver(local_path, &load_error)) {
        if (error) {
            *error = load_error.empty() ? QString("Driver load failed")
                                        : QString::fromStdString(load_error);
        }
        return false;
    }

    driver_loaded_ = true;
    return true;
}

bool EngineHub::load_builtin_polling() {
    stop_audio();

    if (!ensure_engine(44100)) {
        return false;
    }
    engine_.reset();
    polling_.set_z80(&engine_.z80());

    driver_loaded_ = false;
    driver_is_polling_ = false;

    if (!polling_.load_builtin_driver()) {
        return false;
    }

    driver_loaded_ = true;
    driver_is_polling_ = true;
    return true;
}

bool EngineHub::engine_ready() const {
    return engine_ready_;
}

bool EngineHub::driver_loaded() const {
    return driver_loaded_;
}

bool EngineHub::driver_is_polling() const {
    return driver_is_polling_;
}

bool EngineHub::audio_running() const {
    return audio_ && audio_->is_running();
}

QString EngineHub::last_audio_error() const {
    return audio_ ? audio_->last_error() : QString();
}

QString EngineHub::audio_debug_info() const {
    return audio_ ? audio_->debug_info() : QString();
}

int EngineHub::audio_peak_percent() const {
    return audio_ ? audio_->peak_percent() : 0;
}

bool EngineHub::audio_clip_recent() const {
    return audio_ ? audio_->clip_recent() : false;
}

void EngineHub::set_step_z80(bool enabled) {
    if (audio_) {
        audio_->set_step_z80(enabled);
    }
}

ngpc::SoundEngine& EngineHub::engine() {
    return engine_;
}

ngpc::PollingDriverHost& EngineHub::polling() {
    return polling_;
}
