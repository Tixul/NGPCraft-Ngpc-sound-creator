#pragma once

#include <QObject>
#include <QString>

#include "ngpc/polling_driver.h"
#include "ngpc/sound_engine.h"

class AudioOutput;

class EngineHub : public QObject
{
    Q_OBJECT

public:
    explicit EngineHub(QObject* parent = nullptr);
    ~EngineHub() override;

    bool ensure_engine(int sample_rate = 44100);
    bool start_audio(int sample_rate = 44100);
    bool ensure_audio_running(int sample_rate = 44100);
    void stop_audio();

    bool load_driver(const QString& path, QString* error = nullptr);
    bool load_builtin_polling();

    bool engine_ready() const;
    bool driver_loaded() const;
    bool driver_is_polling() const;
    bool audio_running() const;
    QString last_audio_error() const;
    QString audio_debug_info() const;
    int audio_peak_percent() const;
    bool audio_clip_recent() const;
    void set_step_z80(bool enabled);

    ngpc::SoundEngine& engine();
    ngpc::PollingDriverHost& polling();

private:
    AudioOutput* audio_ = nullptr;
    ngpc::SoundEngine engine_;
    ngpc::PollingDriverHost polling_;
    bool engine_ready_ = false;
    bool driver_loaded_ = false;
    bool driver_is_polling_ = false;
};
