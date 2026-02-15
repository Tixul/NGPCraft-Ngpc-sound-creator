#pragma once

#include <QObject>
#include <QString>
#include <vector>

#include "ngpc/instrument.h"

class QJsonDocument;

class InstrumentStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxPresets = 128;

    explicit InstrumentStore(QObject* parent = nullptr);

    int count() const;
    const ngpc::InstrumentPreset& at(int index) const;
    void set(int index, const ngpc::InstrumentPreset& preset);
    void add(const ngpc::InstrumentPreset& preset);
    void remove(int index);
    void duplicate(int index);
    void move_up(int index);
    void move_down(int index);

    bool save_json(const QString& path) const;
    bool load_json(const QString& path);
    QString export_c_array() const;

    void load_factory_presets();

signals:
    void list_changed();
    void preset_changed(int index);

private:
    std::vector<ngpc::InstrumentPreset> presets_;
};
