#include "models/InstrumentStore.h"

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

InstrumentStore::InstrumentStore(QObject* parent)
    : QObject(parent)
{
    load_factory_presets();
}

int InstrumentStore::count() const {
    return static_cast<int>(presets_.size());
}

const ngpc::InstrumentPreset& InstrumentStore::at(int index) const {
    return presets_.at(static_cast<size_t>(index));
}

void InstrumentStore::set(int index, const ngpc::InstrumentPreset& preset) {
    if (index < 0 || index >= count()) {
        return;
    }
    presets_[static_cast<size_t>(index)] = preset;
    emit preset_changed(index);
}

void InstrumentStore::add(const ngpc::InstrumentPreset& preset) {
    if (count() >= kMaxPresets) {
        return;
    }
    presets_.push_back(preset);
    emit list_changed();
}

void InstrumentStore::remove(int index) {
    if (index < 0 || index >= count()) {
        return;
    }
    presets_.erase(presets_.begin() + index);
    emit list_changed();
}

void InstrumentStore::duplicate(int index) {
    if (index < 0 || index >= count()) {
        return;
    }
    if (count() >= kMaxPresets) {
        return;
    }
    ngpc::InstrumentPreset copy = presets_[static_cast<size_t>(index)];
    copy.name += " (copy)";
    presets_.insert(presets_.begin() + index + 1, copy);
    emit list_changed();
}

void InstrumentStore::move_up(int index) {
    if (index <= 0 || index >= count()) {
        return;
    }
    std::swap(presets_[static_cast<size_t>(index)],
              presets_[static_cast<size_t>(index - 1)]);
    emit list_changed();
}

void InstrumentStore::move_down(int index) {
    if (index < 0 || index >= count() - 1) {
        return;
    }
    std::swap(presets_[static_cast<size_t>(index)],
              presets_[static_cast<size_t>(index + 1)]);
    emit list_changed();
}

void InstrumentStore::load_factory_presets() {
    presets_ = ngpc::FactoryInstrumentPresets();
    if (count() > kMaxPresets) {
        presets_.resize(static_cast<size_t>(kMaxPresets));
    }
    emit list_changed();
}

static QJsonObject DefToJson(const ngpc::BgmInstrumentDef& d) {
    QJsonObject o;
    o["attn"]           = d.attn;
    o["env_on"]         = d.env_on;
    o["env_step"]       = d.env_step;
    o["env_speed"]      = d.env_speed;
    o["env_curve_id"]   = d.env_curve_id;
    o["pitch_curve_id"] = d.pitch_curve_id;
    o["vib_on"]         = d.vib_on;
    o["vib_depth"]      = d.vib_depth;
    o["vib_speed"]      = d.vib_speed;
    o["vib_delay"]      = d.vib_delay;
    o["sweep_on"]       = d.sweep_on;
    o["sweep_end"]      = d.sweep_end;
    o["sweep_step"]     = d.sweep_step;
    o["sweep_speed"]    = d.sweep_speed;
    o["mode"]           = d.mode;
    o["noise_config"]   = d.noise_config;
    o["macro_id"]       = d.macro_id;
    o["adsr_on"]        = d.adsr_on;
    o["adsr_attack"]    = d.adsr_attack;
    o["adsr_decay"]     = d.adsr_decay;
    o["adsr_sustain"]   = d.adsr_sustain;
    o["adsr_sustain_rate"] = d.adsr_sustain_rate;
    o["adsr_release"]   = d.adsr_release;
    o["lfo_on"]         = d.lfo_on;
    o["lfo_wave"]       = d.lfo_wave;
    o["lfo_hold"]       = d.lfo_hold;
    o["lfo_rate"]       = d.lfo_rate;
    o["lfo_depth"]      = d.lfo_depth;
    o["lfo2_on"]        = d.lfo2_on;
    o["lfo2_wave"]      = d.lfo2_wave;
    o["lfo2_hold"]      = d.lfo2_hold;
    o["lfo2_rate"]      = d.lfo2_rate;
    o["lfo2_depth"]     = d.lfo2_depth;
    o["lfo_algo"]       = d.lfo_algo;
    return o;
}

static ngpc::BgmInstrumentDef DefFromJson(const QJsonObject& o) {
    ngpc::BgmInstrumentDef d;
    d.attn           = static_cast<uint8_t>(o["attn"].toInt(2));
    d.env_on         = static_cast<uint8_t>(o["env_on"].toInt(0));
    d.env_step       = static_cast<uint8_t>(o["env_step"].toInt(1));
    d.env_speed      = static_cast<uint8_t>(o["env_speed"].toInt(1));
    d.env_curve_id   = static_cast<uint8_t>(o["env_curve_id"].toInt(0));
    d.pitch_curve_id = static_cast<uint8_t>(o["pitch_curve_id"].toInt(0));
    d.vib_on         = static_cast<uint8_t>(o["vib_on"].toInt(0));
    d.vib_depth      = static_cast<uint8_t>(o["vib_depth"].toInt(0));
    d.vib_speed      = static_cast<uint8_t>(o["vib_speed"].toInt(1));
    d.vib_delay      = static_cast<uint8_t>(o["vib_delay"].toInt(0));
    d.sweep_on       = static_cast<uint8_t>(o["sweep_on"].toInt(0));
    d.sweep_end      = static_cast<uint16_t>(o["sweep_end"].toInt(1));
    d.sweep_step     = static_cast<int16_t>(o["sweep_step"].toInt(0));
    d.sweep_speed    = static_cast<uint8_t>(o["sweep_speed"].toInt(1));
    d.mode           = static_cast<uint8_t>(o["mode"].toInt(0));
    d.noise_config   = static_cast<uint8_t>(o["noise_config"].toInt(0));
    d.macro_id       = static_cast<uint8_t>(o["macro_id"].toInt(0));
    d.adsr_on        = static_cast<uint8_t>(o["adsr_on"].toInt(0));
    d.adsr_attack    = static_cast<uint8_t>(o["adsr_attack"].toInt(0));
    d.adsr_decay     = static_cast<uint8_t>(o["adsr_decay"].toInt(0));
    d.adsr_sustain   = static_cast<uint8_t>(o["adsr_sustain"].toInt(0));
    d.adsr_sustain_rate = static_cast<uint8_t>(o["adsr_sustain_rate"].toInt(0));
    d.adsr_release   = static_cast<uint8_t>(o["adsr_release"].toInt(0));
    d.lfo_on         = static_cast<uint8_t>(o["lfo_on"].toInt(0) ? 1 : 0);
    d.lfo_wave       = static_cast<uint8_t>(std::clamp(o["lfo_wave"].toInt(0), 0, 4));
    d.lfo_hold       = static_cast<uint8_t>(std::clamp(o["lfo_hold"].toInt(0), 0, 255));
    d.lfo_rate       = static_cast<uint8_t>(std::clamp(o["lfo_rate"].toInt(1), 0, 255));
    d.lfo_depth      = static_cast<uint8_t>(std::clamp(o["lfo_depth"].toInt(0), 0, 255));
    d.lfo2_on        = static_cast<uint8_t>(o["lfo2_on"].toInt(0) ? 1 : 0);
    d.lfo2_wave      = static_cast<uint8_t>(std::clamp(o["lfo2_wave"].toInt(0), 0, 4));
    d.lfo2_hold      = static_cast<uint8_t>(std::clamp(o["lfo2_hold"].toInt(0), 0, 255));
    d.lfo2_rate      = static_cast<uint8_t>(std::clamp(o["lfo2_rate"].toInt(1), 0, 255));
    d.lfo2_depth     = static_cast<uint8_t>(std::clamp(o["lfo2_depth"].toInt(0), 0, 255));
    d.lfo_algo       = static_cast<uint8_t>(std::clamp(o["lfo_algo"].toInt(1), 0, 7));
    return d;
}

bool InstrumentStore::save_json(const QString& path) const {
    QJsonArray arr;
    for (const auto& p : presets_) {
        QJsonObject obj;
        obj["name"] = QString::fromStdString(p.name);
        obj["def"]  = DefToJson(p.def);
        arr.append(obj);
    }
    QJsonObject root;
    root["version"]     = 2;
    root["instruments"] = arr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool InstrumentStore::load_json(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonArray arr = root["instruments"].toArray();
    if (arr.isEmpty()) {
        return false;
    }
    std::vector<ngpc::InstrumentPreset> loaded;
    for (const QJsonValue& val : arr) {
        if (loaded.size() >= static_cast<size_t>(kMaxPresets)) {
            break;
        }
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        ngpc::InstrumentPreset p;
        p.name = obj["name"].toString("Untitled").toStdString();
        p.def  = DefFromJson(obj["def"].toObject());
        loaded.push_back(std::move(p));
    }
    if (loaded.empty()) {
        return false;
    }
    presets_ = std::move(loaded);
    emit list_changed();
    return true;
}

QString InstrumentStore::export_c_array() const {
    return QString::fromStdString(ngpc::InstrumentPresetsToCArray(presets_));
}
