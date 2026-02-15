#include "models/ProjectDocument.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace {
int clamp_autosave_interval(int seconds) {
    if (seconds <= 0) return 0;
    if (seconds <= 30) return 30;
    if (seconds <= 60) return 60;
    if (seconds <= 120) return 120;
    return 300;
}
} // namespace

void ProjectDocument::set_defaults(const QString& project_name) {
    name = project_name.trimmed();
    songs.clear();
    sfx.clear();
    active_song_id.clear();
    autosave = ProjectAutosaveSettings{};
}

int ProjectDocument::song_index_by_id(const QString& id) const {
    for (int i = 0; i < songs.size(); ++i) {
        if (songs[i].id == id) return i;
    }
    return -1;
}

int ProjectDocument::sfx_index_by_id(const QString& id) const {
    for (int i = 0; i < sfx.size(); ++i) {
        if (sfx[i].id == id) return i;
    }
    return -1;
}

bool ProjectDocument::load_from_file(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QString("Cannot open project file: %1").arg(path);
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) *error = "Invalid project JSON";
        return false;
    }

    const QJsonObject root = doc.object();
    const int version = root.value("version").toInt(0);
    if (version != kVersion) {
        if (error) *error = QString("Unsupported project version: %1").arg(version);
        return false;
    }

    const QString loaded_name = root.value("name").toString().trimmed();
    if (loaded_name.isEmpty()) {
        if (error) *error = "Project name is missing";
        return false;
    }

    const QJsonArray song_array = root.value("songs").toArray();
    if (song_array.isEmpty()) {
        if (error) *error = "Project has no songs";
        return false;
    }

    QVector<ProjectSongEntry> loaded_songs;
    loaded_songs.reserve(song_array.size());
    for (const QJsonValue& val : song_array) {
        if (!val.isObject()) continue;
        const QJsonObject s = val.toObject();
        const QString id = s.value("id").toString().trimmed();
        const QString title = s.value("name").toString().trimmed();
        const QString file_rel = s.value("file").toString().trimmed();
        if (id.isEmpty() || title.isEmpty() || file_rel.isEmpty()) {
            continue;
        }
        loaded_songs.push_back(ProjectSongEntry{id, title, file_rel});
    }
    if (loaded_songs.isEmpty()) {
        if (error) *error = "Project has no valid songs";
        return false;
    }

    QVector<ProjectSfxEntry> loaded_sfx;
    const QJsonArray sfx_array = root.value("sfx").toArray();
    loaded_sfx.reserve(sfx_array.size());
    for (const QJsonValue& val : sfx_array) {
        if (!val.isObject()) continue;
        const QJsonObject o = val.toObject();
        ProjectSfxEntry e;
        e.id = o.value("id").toString().trimmed();
        e.name = o.value("name").toString().trimmed();
        if (e.id.isEmpty() || e.name.isEmpty()) continue;
        e.tone_on = std::clamp(o.value("tone_on").toInt(e.tone_on), 0, 1);
        e.tone_ch = std::clamp(o.value("tone_ch").toInt(e.tone_ch), 0, 2);
        e.tone_div = std::clamp(o.value("tone_div").toInt(e.tone_div), 1, 1023);
        e.tone_attn = std::clamp(o.value("tone_attn").toInt(e.tone_attn), 0, 15);
        e.tone_frames = std::clamp(o.value("tone_frames").toInt(e.tone_frames), 0, 255);
        e.tone_sw_on = std::clamp(o.value("tone_sw_on").toInt(e.tone_sw_on), 0, 1);
        e.tone_sw_end = std::clamp(o.value("tone_sw_end").toInt(e.tone_sw_end), 1, 1023);
        e.tone_sw_step = std::clamp(o.value("tone_sw_step").toInt(e.tone_sw_step), -32768, 32767);
        e.tone_sw_speed = std::clamp(o.value("tone_sw_speed").toInt(e.tone_sw_speed), 1, 30);
        e.tone_sw_ping = std::clamp(o.value("tone_sw_ping").toInt(e.tone_sw_ping), 0, 1);
        e.tone_env_on = std::clamp(o.value("tone_env_on").toInt(e.tone_env_on), 0, 1);
        e.tone_env_step = std::clamp(o.value("tone_env_step").toInt(e.tone_env_step), 1, 4);
        e.tone_env_spd = std::clamp(o.value("tone_env_spd").toInt(e.tone_env_spd), 1, 10);
        e.noise_on = std::clamp(o.value("noise_on").toInt(e.noise_on), 0, 1);
        e.noise_rate = std::clamp(o.value("noise_rate").toInt(e.noise_rate), 0, 3);
        e.noise_type = std::clamp(o.value("noise_type").toInt(e.noise_type), 0, 1);
        e.noise_attn = std::clamp(o.value("noise_attn").toInt(e.noise_attn), 0, 15);
        e.noise_frames = std::clamp(o.value("noise_frames").toInt(e.noise_frames), 0, 255);
        e.noise_burst = std::clamp(o.value("noise_burst").toInt(e.noise_burst), 0, 1);
        e.noise_burst_dur = std::clamp(o.value("noise_burst_dur").toInt(e.noise_burst_dur), 1, 30);
        e.noise_env_on = std::clamp(o.value("noise_env_on").toInt(e.noise_env_on), 0, 1);
        e.noise_env_step = std::clamp(o.value("noise_env_step").toInt(e.noise_env_step), 1, 4);
        e.noise_env_spd = std::clamp(o.value("noise_env_spd").toInt(e.noise_env_spd), 1, 10);
        e.tone_adsr_on = std::clamp(o.value("tone_adsr_on").toInt(e.tone_adsr_on), 0, 1);
        e.tone_adsr_ar = std::clamp(o.value("tone_adsr_ar").toInt(e.tone_adsr_ar), 0, 31);
        e.tone_adsr_dr = std::clamp(o.value("tone_adsr_dr").toInt(e.tone_adsr_dr), 0, 31);
        e.tone_adsr_sl = std::clamp(o.value("tone_adsr_sl").toInt(e.tone_adsr_sl), 0, 15);
        e.tone_adsr_sr = std::clamp(o.value("tone_adsr_sr").toInt(e.tone_adsr_sr), 0, 31);
        e.tone_adsr_rr = std::clamp(o.value("tone_adsr_rr").toInt(e.tone_adsr_rr), 0, 31);
        e.tone_lfo1_on = std::clamp(o.value("tone_lfo1_on").toInt(e.tone_lfo1_on), 0, 1);
        e.tone_lfo1_wave = std::clamp(o.value("tone_lfo1_wave").toInt(e.tone_lfo1_wave), 0, 4);
        e.tone_lfo1_hold = std::clamp(o.value("tone_lfo1_hold").toInt(e.tone_lfo1_hold), 0, 255);
        e.tone_lfo1_rate = std::clamp(o.value("tone_lfo1_rate").toInt(e.tone_lfo1_rate), 0, 255);
        e.tone_lfo1_depth = std::clamp(o.value("tone_lfo1_depth").toInt(e.tone_lfo1_depth), 0, 255);
        e.tone_lfo2_on = std::clamp(o.value("tone_lfo2_on").toInt(e.tone_lfo2_on), 0, 1);
        e.tone_lfo2_wave = std::clamp(o.value("tone_lfo2_wave").toInt(e.tone_lfo2_wave), 0, 4);
        e.tone_lfo2_hold = std::clamp(o.value("tone_lfo2_hold").toInt(e.tone_lfo2_hold), 0, 255);
        e.tone_lfo2_rate = std::clamp(o.value("tone_lfo2_rate").toInt(e.tone_lfo2_rate), 0, 255);
        e.tone_lfo2_depth = std::clamp(o.value("tone_lfo2_depth").toInt(e.tone_lfo2_depth), 0, 255);
        e.tone_lfo_algo = std::clamp(o.value("tone_lfo_algo").toInt(e.tone_lfo_algo), 0, 7);
        loaded_sfx.push_back(e);
    }

    const QJsonObject autosave_obj = root.value("autosave").toObject();
    ProjectAutosaveSettings loaded_autosave;
    loaded_autosave.interval_sec = clamp_autosave_interval(
        autosave_obj.value("interval_sec").toInt(ProjectAutosaveSettings{}.interval_sec));
    loaded_autosave.on_tab_change =
        autosave_obj.value("on_tab_change").toBool(ProjectAutosaveSettings{}.on_tab_change);
    loaded_autosave.on_close =
        autosave_obj.value("on_close").toBool(ProjectAutosaveSettings{}.on_close);

    QString loaded_active = root.value("active_song_id").toString();
    bool active_found = false;
    for (const auto& s : loaded_songs) {
        if (s.id == loaded_active) {
            active_found = true;
            break;
        }
    }
    if (!active_found) {
        loaded_active = loaded_songs.first().id;
    }

    name = loaded_name;
    songs = std::move(loaded_songs);
    sfx = std::move(loaded_sfx);
    active_song_id = loaded_active;
    autosave = loaded_autosave;
    return true;
}

bool ProjectDocument::save_to_file(const QString& path, QString* error) const {
    if (name.trimmed().isEmpty()) {
        if (error) *error = "Cannot save project with empty name";
        return false;
    }
    if (songs.isEmpty()) {
        if (error) *error = "Cannot save project with no songs";
        return false;
    }

    QJsonObject root;
    root["version"] = kVersion;
    root["name"] = name;
    root["active_song_id"] = active_song_id;

    QJsonArray song_array;
    for (const auto& s : songs) {
        QJsonObject item;
        item["id"] = s.id;
        item["name"] = s.name;
        item["file"] = s.file;
        song_array.append(item);
    }
    root["songs"] = song_array;

    QJsonArray sfx_array;
    for (const auto& s : sfx) {
        QJsonObject item;
        item["id"] = s.id;
        item["name"] = s.name;
        item["tone_on"] = s.tone_on;
        item["tone_ch"] = s.tone_ch;
        item["tone_div"] = s.tone_div;
        item["tone_attn"] = s.tone_attn;
        item["tone_frames"] = s.tone_frames;
        item["tone_sw_on"] = s.tone_sw_on;
        item["tone_sw_end"] = s.tone_sw_end;
        item["tone_sw_step"] = s.tone_sw_step;
        item["tone_sw_speed"] = s.tone_sw_speed;
        item["tone_sw_ping"] = s.tone_sw_ping;
        item["tone_env_on"] = s.tone_env_on;
        item["tone_env_step"] = s.tone_env_step;
        item["tone_env_spd"] = s.tone_env_spd;
        item["noise_on"] = s.noise_on;
        item["noise_rate"] = s.noise_rate;
        item["noise_type"] = s.noise_type;
        item["noise_attn"] = s.noise_attn;
        item["noise_frames"] = s.noise_frames;
        item["noise_burst"] = s.noise_burst;
        item["noise_burst_dur"] = s.noise_burst_dur;
        item["noise_env_on"] = s.noise_env_on;
        item["noise_env_step"] = s.noise_env_step;
        item["noise_env_spd"] = s.noise_env_spd;
        item["tone_adsr_on"] = s.tone_adsr_on;
        item["tone_adsr_ar"] = s.tone_adsr_ar;
        item["tone_adsr_dr"] = s.tone_adsr_dr;
        item["tone_adsr_sl"] = s.tone_adsr_sl;
        item["tone_adsr_sr"] = s.tone_adsr_sr;
        item["tone_adsr_rr"] = s.tone_adsr_rr;
        item["tone_lfo1_on"] = s.tone_lfo1_on;
        item["tone_lfo1_wave"] = s.tone_lfo1_wave;
        item["tone_lfo1_hold"] = s.tone_lfo1_hold;
        item["tone_lfo1_rate"] = s.tone_lfo1_rate;
        item["tone_lfo1_depth"] = s.tone_lfo1_depth;
        item["tone_lfo2_on"] = s.tone_lfo2_on;
        item["tone_lfo2_wave"] = s.tone_lfo2_wave;
        item["tone_lfo2_hold"] = s.tone_lfo2_hold;
        item["tone_lfo2_rate"] = s.tone_lfo2_rate;
        item["tone_lfo2_depth"] = s.tone_lfo2_depth;
        item["tone_lfo_algo"] = s.tone_lfo_algo;
        sfx_array.append(item);
    }
    root["sfx"] = sfx_array;

    QJsonObject autosave_obj;
    autosave_obj["interval_sec"] = clamp_autosave_interval(autosave.interval_sec);
    autosave_obj["on_tab_change"] = autosave.on_tab_change;
    autosave_obj["on_close"] = autosave.on_close;
    root["autosave"] = autosave_obj;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QString("Cannot write project file: %1").arg(path);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = QString("Cannot commit project file: %1").arg(path);
        return false;
    }
    return true;
}
