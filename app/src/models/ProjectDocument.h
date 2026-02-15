#pragma once

#include <QString>
#include <QVector>

struct ProjectSongEntry {
    QString id;
    QString name;
    QString file; // relative path from project root, e.g. songs/intro.ngps
};

struct ProjectSfxEntry {
    QString id;
    QString name;
    int tone_on = 1;
    int tone_ch = 0;
    int tone_div = 218;
    int tone_attn = 2;
    int tone_frames = 6;
    int tone_sw_on = 0;
    int tone_sw_end = 218;
    int tone_sw_step = 1;
    int tone_sw_speed = 1;
    int tone_sw_ping = 0;
    int tone_env_on = 0;
    int tone_env_step = 1;
    int tone_env_spd = 1;
    int noise_on = 1;
    int noise_rate = 1;
    int noise_type = 1;
    int noise_attn = 2;
    int noise_frames = 4;
    int noise_burst = 0;
    int noise_burst_dur = 1;
    int noise_env_on = 0;
    int noise_env_step = 1;
    int noise_env_spd = 1;
    int tone_adsr_on = 0;
    int tone_adsr_ar = 0;
    int tone_adsr_dr = 2;
    int tone_adsr_sl = 8;
    int tone_adsr_sr = 0;
    int tone_adsr_rr = 2;
    int tone_lfo1_on = 0;
    int tone_lfo1_wave = 0;
    int tone_lfo1_hold = 0;
    int tone_lfo1_rate = 1;
    int tone_lfo1_depth = 0;
    int tone_lfo2_on = 0;
    int tone_lfo2_wave = 0;
    int tone_lfo2_hold = 0;
    int tone_lfo2_rate = 1;
    int tone_lfo2_depth = 0;
    int tone_lfo_algo = 1;
};

struct ProjectAutosaveSettings {
    int interval_sec = 60;   // 0 = disabled
    bool on_tab_change = true;
    bool on_close = true;
};

class ProjectDocument
{
public:
    static constexpr int kVersion = 1;

    QString name;
    QVector<ProjectSongEntry> songs;
    QVector<ProjectSfxEntry> sfx;
    QString active_song_id;
    ProjectAutosaveSettings autosave;

    void set_defaults(const QString& project_name);

    int song_index_by_id(const QString& id) const;
    int sfx_index_by_id(const QString& id) const;

    bool load_from_file(const QString& path, QString* error = nullptr);
    bool save_to_file(const QString& path, QString* error = nullptr) const;
};
