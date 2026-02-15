#pragma once

#include <QObject>
#include <QByteArray>
#include <vector>

class TrackerDocument;

class SongDocument : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxPatterns    = 64;
    static constexpr int kMaxOrderLength = 256;

    explicit SongDocument(QObject* parent = nullptr);

    // --- Pattern bank ---
    int pattern_count() const;
    TrackerDocument* pattern(int index);
    const TrackerDocument* pattern(int index) const;
    int add_pattern();                       // returns new pattern index
    int clone_pattern(int source_index);     // returns new pattern index
    void remove_pattern(int index);          // refuses if last pattern

    // --- Active pattern (for editing) ---
    int active_pattern_index() const { return active_index_; }
    void set_active_pattern(int index);
    TrackerDocument* active_pattern();

    // --- Order list ---
    const std::vector<int>& order() const { return order_; }
    int order_length() const { return static_cast<int>(order_.size()); }
    void order_insert(int position, int pattern_index);
    void order_remove(int position);
    void order_move_up(int position);
    void order_move_down(int position);
    void order_set_entry(int position, int pattern_index);

    // --- Loop point ---
    int loop_point() const { return loop_point_; }
    void set_loop_point(int order_position);

    // --- Serialization ---
    QByteArray to_json() const;
    bool from_json(const QByteArray& data);
    bool import_ngpat(const QByteArray& data);

signals:
    void active_pattern_changed(int index);
    void order_changed();
    void pattern_list_changed();

private:
    std::vector<TrackerDocument*> patterns_;
    std::vector<int> order_;
    int active_index_ = 0;
    int loop_point_   = 0;

    void sanitize_order();
};
